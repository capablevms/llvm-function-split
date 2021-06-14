#include "llvm/IR/GlobalObject.h"
#include <algorithm>
#include <execution>
#include <fstream>
#include <iostream>
#include <memory>
#include <pstl/glue_execution_defs.h>
#include <queue>
#include <sstream>
#include <string>
#include <system_error>
#include <unordered_map>
#include <unordered_set>

#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Analysis/CallGraph.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/IR/Argument.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/DerivedUser.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/InstVisitor.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Operator.h>
#include <llvm/IR/Use.h>
#include <llvm/IR/User.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Pass.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Transforms/Utils/ValueMapper.h>

#include <assert.h>
#include <thread>
#include <typeinfo>

static llvm::LLVMContext context;
llvm::SMDiagnostic err;

llvm::cl::opt<std::string> inputFilename(llvm::cl::Positional,
                                         llvm::cl::desc("<input file>"),
                                         llvm::cl::Required);

llvm::cl::opt<std::string>
    outputDirectory("o",
                    llvm::cl::desc("Where the output files will be created."),
                    llvm::cl::Required);
llvm::cl::opt<bool> dry("d",
                        llvm::cl::desc("Run without executing the commands."));

llvm::cl::opt<bool> verbose("v", llvm::cl::desc("Enable vrbose output."));

std::unordered_set<llvm::GlobalVariable *> iterateOperands(llvm::User &user) {
  std::stack<llvm::Value *> operands;
  std::unordered_set<llvm::GlobalVariable *> globals;

  for (auto &operand : user.operands()) {
    operands.push(operand);
  }

  while (operands.size() > 0) {
    auto currentOperand = operands.top();
    operands.pop();

    if (auto globalVariable =
            llvm::dyn_cast<llvm::GlobalVariable>(currentOperand)) {
      if (globalVariable->isConstant()) {
        globals.insert(globalVariable);
      }
    } else if (auto user = llvm::dyn_cast<llvm::User>(currentOperand)) {

      for (auto &nextOperand : user->operands()) {
        if (auto instruction = llvm::dyn_cast<llvm::Instruction>(nextOperand)) {
          continue;
        } else {
          operands.push(nextOperand);
        }
      }
    }
  }
  return globals;
}

std::unordered_set<llvm::GlobalVariable *>
resolveAllDependencies(llvm::User &user) {
  std::unordered_set<llvm::GlobalVariable *> globals;
  std::queue<llvm::GlobalVariable *> queueVariablesNeedMoving;
  for (const auto item : iterateOperands(user)) {
    queueVariablesNeedMoving.push(item);
  }
  while (queueVariablesNeedMoving.size() > 0) {
    auto currentVariable = queueVariablesNeedMoving.front();
    queueVariablesNeedMoving.pop();
    globals.insert(currentVariable);

    if (currentVariable->hasInitializer()) {
      for (const auto item :
           iterateOperands(*currentVariable->getInitializer())) {
        if (globals.find(item) == globals.end()) {
          queueVariablesNeedMoving.push(item);
        }
      }
    }
  }
  return globals;
}

struct MyPass : public llvm::InstVisitor<MyPass> {
  std::unordered_set<llvm::GlobalVariable *> globals;
  // TODO: Move this to a separate function because globals
  // also need the full dependency tree moved.
  void visitInstruction(llvm::Instruction &instruction) {
    for (const auto item : resolveAllDependencies(instruction)) {
      globals.insert(item);
    }
  }
};

// Returns an empty string if no debug info can be found.
std::string getFileName(llvm::GlobalObject &value) {
  llvm::SmallVector<std::pair<unsigned, llvm::MDNode *>, 4> MDs;
  value.getAllMetadata(MDs);
  for (auto &MD : MDs) {
    if (llvm::MDNode *N = MD.second) {
      if (auto *subProgram = llvm::dyn_cast<llvm::DISubprogram>(N)) {
        return subProgram->getFilename().str();
      }
    }
  }
  return "";
}

int main(int argc, char **argv) {
  llvm::cl::ParseCommandLineOptions(argc, argv);
  std::string file;
  std::error_code ecode;
  std::unique_ptr<llvm::Module> loadedModule =
      llvm::parseIRFile(inputFilename, err, context);
  std::unordered_map<const llvm::Function *, std::set<llvm::GlobalVariable *>>
      users;
  std::string extractFilename = "temp.bc";
  std::string extractProgram = "llvm-extract";

  if (auto extractProgramOverride = std::getenv("LLVM_EXTRACT")) {
    extractProgram = std::string(extractProgramOverride);
  }

  for (auto &global : loadedModule->globals()) {
    if (global.isConstant()) {
      global.setVisibility(
          llvm::GlobalValue::VisibilityTypes::DefaultVisibility);
      global.setDSOLocal(false);
      continue;
    }
    global.setVisibility(llvm::GlobalValue::VisibilityTypes::DefaultVisibility);
    global.setLinkage(llvm::GlobalValue::LinkageTypes::ExternalLinkage);
    global.setDSOLocal(false);
  }

  for (auto &function : loadedModule->functions()) {
    if (function.isDeclaration()) {
      continue;
    }
    function.setVisibility(
        llvm::GlobalValue::VisibilityTypes::DefaultVisibility);
    function.setLinkage(llvm::GlobalValue::ExternalLinkage);
    function.setDSOLocal(false);
  }

  if (!dry) {
    llvm::raw_fd_ostream outputFile("temp.bc", ecode);
    llvm::WriteBitcodeToFile(*loadedModule, outputFile);
    outputFile.close();
  }

#pragma omp parallel
#pragma omp single
  for (llvm::GlobalVariable &globalVariable : loadedModule->globals()) {
    if (globalVariable.isConstant() ||
        globalVariable.isExternallyInitialized()) {
      continue;
    }

    std::cout << getFileName(globalVariable) << "\n";
    std::stringstream command;
    command << extractProgram << " " << extractFilename
            << " --glob=" << globalVariable.getName().str() << " ";

    if (globalVariable.hasInitializer()) {
      for (const auto dependency :
           resolveAllDependencies(*globalVariable.getInitializer())) {
        command << " --glob=" << dependency->getName().str() << " ";
      }
    }

    command << "-o " << outputDirectory << "/"
            << "_" << globalVariable.getName().str() << ".bc ";

    std::cout << command.str() << "\n\n";
    std::string materializedCommand = command.str();
#pragma omp task
    if (!dry) {
      system(materializedCommand.c_str());
    }
  }

  for (auto &global : loadedModule->globals()) {
    if (global.isConstant()) {
      continue;
    }
    global.setInitializer(nullptr);
    global.setVisibility(llvm::GlobalValue::VisibilityTypes::DefaultVisibility);
    global.setLinkage(llvm::GlobalValue::LinkageTypes::ExternalLinkage);
    global.setDSOLocal(false);
  }

  if (!dry) {
    llvm::raw_fd_ostream outputFile("temp.bc", ecode);
    llvm::WriteBitcodeToFile(*loadedModule, outputFile);
    outputFile.close();
  }

#pragma omp parallel
#pragma omp single
  for (llvm::Function &function : loadedModule->functions()) {
    if (function.isDeclaration()) {
      continue;
    }

    if (function.getName().equals("createmeta")) {
      std::cout << "fount \n";
    }

    std::cout << "filename: " << getFileName(function) << "\n";

    const auto name = function.getName().str();
    MyPass pass;
    pass.visit(function);

    std::stringstream command;
    command << extractProgram << " " << extractFilename << " --func=" << name
            << " ";

    for (auto const &use : pass.globals) {
      command << "--glob=" << use->getName().str() << " ";
    }
    command << "-o " << outputDirectory << "/"
            << "_" << name << ".bc ";

    std::cout << command.str() << "\n\n";
    const auto materializedCommand = command.str();
#pragma omp task
    if (!dry) {
      std::cout << "# " << std::this_thread::get_id() << " : " << name << "\n";
      system(materializedCommand.c_str());
    }
  }
}
