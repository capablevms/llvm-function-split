#include <algorithm>
#include <execution>
#include <fstream>
#include <iostream>
#include <memory>
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
#include <llvm/IRReader/IRReader.h>
#include <llvm/Pass.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Transforms/Utils/ValueMapper.h>

#include <assert.h>
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

std::set<llvm::GlobalValue *> toSplit;

struct MyPass : public llvm::InstVisitor<MyPass> {
  std::unordered_set<llvm::GlobalVariable *> globals;

  void visitInstruction(llvm::Instruction &instruction) {
    std::stack<llvm::Value *> operands;

    for (uint32_t i = 0; i < instruction.getNumOperands(); i++) {
      operands.push(instruction.getOperand(i));
    }

    while (operands.size() > 0) {
      auto currentOperand = operands.top();
      operands.pop();

      if (currentOperand == nullptr) {
        // TODO: ignore null's for now.
      } else if (auto globalVariable =
                     llvm::dyn_cast<llvm::GlobalVariable>(currentOperand)) {
        if (globalVariable->isConstant()) {
          globals.insert(globalVariable);
        } else {
          toSplit.insert(globalVariable);
        }
      } else if (auto user = llvm::dyn_cast<llvm::User>(currentOperand)) {
        for (uint32_t i = 0; i < user->getNumOperands(); i++) {
          auto nextOperand = user->getOperand(i);
          if (auto instruction =
                  llvm::dyn_cast<llvm::Instruction>(nextOperand)) {
            continue;
          } else {
            operands.push(nextOperand);
          }
        }
      }
    }
  }
};

int main(int argc, char **argv) {
  llvm::cl::ParseCommandLineOptions(argc, argv);

  std::string file;
  std::unique_ptr<llvm::Module> loadedModule =
      llvm::parseIRFile(inputFilename, err, context);

  std::unordered_map<const llvm::Function *, std::set<llvm::GlobalVariable *>>
      users;

  for (auto &function : loadedModule->functions()) {
    if (function.isDeclaration()) {
      continue;
    }
    MyPass pass;
    pass.visit(function);

    llvm::outs() << "# " << function.getName() << "\n";
    std::stringstream command;
    command << "llvm-extract " << loadedModule->getName().str()
            << " --func=" << function.getName().str() << " ";

    for (auto const &use : pass.globals) {
      command << "--glob=" << use->getName().str() << " ";
    }
    command << "-o " << outputDirectory << "/" << function.getName().str()
            << ".bc &\n\n";
    std::cout << command.str();

    if (!dry) {
      system(command.str().c_str());
    }
  }

  for (auto &globalVariable : toSplit) {
    std::stringstream command;
    command << "llvm-extract " << loadedModule->getName().str()
            << " --glob=" << globalVariable->getName().str() << " "
            << "-o " << outputDirectory << "/"
            << globalVariable->getName().str() << ".bc &\n\n";
    std::cout << command.str();

    if (!dry) {
      system(command.str().c_str());
    }
  }
}
