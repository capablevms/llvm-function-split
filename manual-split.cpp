#include <algorithm>
#include <execution>
#include <iostream>
#include <memory>
#include <pstl/glue_execution_defs.h>
#include <string>
#include <system_error>

#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/IR/Argument.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Transforms/Utils/ValueMapper.h>

static llvm::LLVMContext context;

enum FunctionAction { MakePublic, Skip, MakePrivate, MakeRefernce };

static void copyComdat(const llvm::GlobalObject *source,
                       llvm::GlobalObject *destination) {
  if (const auto *comdat = source->getComdat()) {
    auto *newComdat =
        destination->getParent()->getOrInsertComdat(comdat->getName());
    newComdat->setSelectionKind(comdat->getSelectionKind());
    destination->setComdat(newComdat);
  }
}

void moveFunctions(const llvm::StringRef outputName, const llvm::Module *module,
                   FunctionAction(CheckFunction)(const llvm::Module &,
                                                 const llvm::Function &)) {
  llvm::Module result(outputName, context);

  result.setDataLayout(module->getDataLayout());
  result.setTargetTriple(module->getTargetTriple());
  result.setModuleInlineAsm(module->getModuleInlineAsm());

  /*
    The value to value map is the central datastructure to the function
    cloning/moving. It is used to map the llvm values that reside in the
    original module to the ones that reside in the new resutling module module.
  */
  llvm::ValueToValueMapTy vMap;

  // Move the global values over, as they are used even when no gobal values are
  // present in the code, for things like string literals.
  for (const auto &global : module->globals()) {
    std::cout << global.getName().str() << std::endl;

    auto resultGlobal = new llvm::GlobalVariable(
        result, global.getValueType(), global.isConstant(), global.getLinkage(),
        nullptr, global.getName(), nullptr, global.getThreadLocalMode(),
        global.getType()->getAddressSpace());

    // TODO: Find a way to understand how CloneModule copies these things
    // without calling this functions.
    resultGlobal->setAlignment(global.getAlign());
    resultGlobal->setUnnamedAddr(global.getUnnamedAddr());

    vMap[&global] = resultGlobal;

    llvm::SmallVector<std::pair<unsigned, llvm::MDNode *>, 1> metadataList;
    global.getAllMetadata(metadataList);
    for (auto [kind, node] : metadataList) {
      resultGlobal->addMetadata(
          kind, *MapMetadata(node, vMap, llvm::RF_MoveDistinctMDs));
    }

    if (global.hasInitializer()) {
      resultGlobal->setInitializer(MapValue(global.getInitializer(), vMap));
    }

    copyComdat(&global, resultGlobal);
  }

  // Go through all the functions and create the declarations without
  // bodies so they can be linked against when copying the bodies later.
  for (const llvm::Function &function : module->functions()) {
    auto action = CheckFunction(result, function);
    if (action == FunctionAction::Skip) {
      continue;
    }

    auto resultFunction = llvm::Function::Create(
        llvm::cast<llvm::FunctionType>(function.getValueType()),
        function.getLinkage(), function.getName(), result);

    vMap[&function] = resultFunction;
  }

  // Now copy all of the functions bodies and meta-data.
  for (const llvm::Function &function : module->functions()) {
    auto action = CheckFunction(result, function);
    if (action == FunctionAction::Skip) {
      continue;
    }

    auto resultFunction = llvm::cast<llvm::Function>(vMap[&function]);

    auto resultArg = resultFunction->arg_begin();
    for (const auto &arg : function.args()) {
      resultArg->setName(arg.getName());
      vMap[&arg] = &*resultArg++;
    }

    llvm::SmallVector<llvm::ReturnInst *, 8> returns;
    if (action != FunctionAction::MakeRefernce) {
      llvm::CloneFunctionInto(resultFunction, &function, vMap, true, returns);
    }
    resultFunction->copyAttributesFrom(&function);
    copyComdat(&function, resultFunction);

    if (action == MakePrivate) {
      resultFunction->setLinkage(llvm::GlobalValue::ExternalLinkage);
      resultFunction->setVisibility(
          llvm::GlobalValue::VisibilityTypes::HiddenVisibility);
    }
  }

  std::error_code ec;
  llvm::raw_fd_ostream OS(outputName, ec);
  result.print(llvm::outs(), nullptr);

  if (llvm::verifyModule(result, &llvm::outs(), nullptr)) {
    llvm::WriteBitcodeToFile(result, OS);
  }
}

llvm::SMDiagnostic err;

llvm::cl::opt<std::string> InputFilename(llvm::cl::Positional,
                                         llvm::cl::desc("<input file>"));

int main(int argc, char **argv) {
  llvm::cl::ParseCommandLineOptions(argc, argv);

  auto file = InputFilename.getValue();
  std::cout << file << std::endl;
  std::unique_ptr<llvm::Module> um = llvm::parseIRFile(file, err, context);
  const llvm::Module *m = um.get();
  m->print(llvm::outs(), nullptr);

  // Show that the CloneModule function works fine, for all inputs.
  auto cloned = llvm::CloneModule(*m);
  cloned.get()->print(llvm::outs(), nullptr);

  if (m) {
    // TODO: These definitions would come from some file describing them
    // or would be constructed automatically.
    moveFunctions(
        "lib.bc", m,
        [](auto module, const llvm::Function &function) -> FunctionAction {
          if (function.getName().equals("a")) {
            return FunctionAction::MakePrivate;
          } else if (function.getName().equals("b")) {
            return FunctionAction::MakePublic;
          } else if (function.getName().equals("cc")) {
            return FunctionAction::MakePublic;
          } else {
            return FunctionAction::Skip;
          }
        });

    moveFunctions(
        "main.bc", m,
        [](auto module, const llvm::Function &function) -> FunctionAction {
          if (function.getName().equals("b")) {
            return FunctionAction::MakeRefernce;
          } else if (function.getName().equals("cc")) {
            return FunctionAction::MakeRefernce;
          } else if (function.getName().equals("main")) {
            return FunctionAction::MakePublic;
          } else if (function.getName().equals("printf")) {
            return FunctionAction::MakeRefernce;
          } else {
            return FunctionAction::Skip;
          }
        });
  } else {
    err.print(file.c_str(), llvm::outs());
  }
}
