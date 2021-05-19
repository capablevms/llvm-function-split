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
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Use.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Transforms/Utils/ValueMapper.h>

#include <assert.h>

static llvm::LLVMContext context;
llvm::SMDiagnostic err;

llvm::cl::opt<std::string> InputFilename(llvm::cl::Positional,
                                         llvm::cl::desc("<input file>"));

llvm::cl::opt<std::string>
    ExtractFunction("e", llvm::cl::desc("Function to extract."),
                    llvm::cl::value_desc("function name"));

llvm::cl::opt<bool>
    Verbose("v", llvm::cl::desc("Enable vrbose output."));

std::unordered_map<llvm::GlobalValue::LinkageTypes, std::string> linkageTypes =
    {
        {llvm::GlobalValue::LinkageTypes::ExternalLinkage, "(external)"},
        {llvm::GlobalValue::LinkageTypes::AvailableExternallyLinkage,
         "(available externally)"},
        {llvm::GlobalValue::LinkageTypes::InternalLinkage, "(internal)"},
        {llvm::GlobalValue::LinkageTypes::PrivateLinkage, "(private)"},
};

std::unordered_map<llvm::GlobalValue::VisibilityTypes, std::string>
    visibilityTypes = {
        {llvm::GlobalValue::VisibilityTypes::DefaultVisibility, "(default)"},
        {llvm::GlobalValue::VisibilityTypes::HiddenVisibility, "(hidden)"},
        {llvm::GlobalValue::VisibilityTypes::ProtectedVisibility,
         "(protected)"}};

std::unordered_set<llvm::GlobalVariable *> getUses(const llvm::Function &function) {
  std::queue<llvm::User *> operands;
  std::unordered_set<llvm::GlobalVariable *> globals;
  for (const auto &basicBlock : function.getBasicBlockList()) {
    for (const auto &instruction : basicBlock.getInstList()) {
      if (instruction.mayReadOrWriteMemory() &&
          instruction.getOpcode() != llvm::Instruction::PHI) {
        operands.push((llvm::User *)(&instruction));
      }
    }
  }

  while (operands.size() > 0) {
    auto value = operands.front();
    operands.pop();

    if (auto globalVariable = llvm::dyn_cast<llvm::GlobalVariable>(value)) {
      if (globalVariable->isConstant()) {
        globals.insert(globalVariable);
      }
    }

    for (uint32_t i = 0; i < value->getNumOperands(); i++) {
      auto operand = value->getOperand(i);

      if (auto instruction = llvm::dyn_cast<llvm::Instruction>(operand)) {
        if (instruction->getOpcode() == llvm::Instruction::PHI) {
          continue;
        }
      }

      if (auto user = llvm::dyn_cast<llvm::User>(operand)) {
        operands.push(user);
      }
    }
  }
  return globals;
}

int main(int argc, char **argv) {
  llvm::cl::ParseCommandLineOptions(argc, argv);
  std::ifstream fileList(InputFilename);
  std::unordered_map<std::string, const llvm::Function *> definedFunctions;
  std::string file;
  std::vector<std::unique_ptr<llvm::Module>> loadedModules;

  while (std::getline(fileList, file)) {
    if (std::unique_ptr<llvm::Module> module =
            llvm::parseIRFile(file, err, context)) {
      for (auto const &function : module->functions()) {
        if (!function.isDeclaration()) {
          definedFunctions.emplace(function.getName().str(), &function);
        }
      }

      loadedModules.push_back(std::move(module));
    } else {
      err.print(file.c_str(), llvm::outs());
    }
  }

  for (const auto &module : loadedModules) {
    for (auto const &function : module->functions()) {
      if (function.isDeclaration()) {
        definedFunctions.erase(function.getName().str());
      }
    }
  }

  for (auto &[key, value] : definedFunctions) {
    auto linkage = linkageTypes[value->getLinkage()];
    if (linkage.length() == 0) {
      linkage = std::to_string(value->getLinkage());
    }
    auto visibility = visibilityTypes[value->getVisibility()];

    std::stringstream out;
    std::stringstream command;

    // TODO: Const washing is a bad thing.
    llvm::Module *module = (llvm::Module *)value->getParent();
    command << "llvm-extract " << module->getName().str()
            << " --func=" << value->getName().str() << " ";

    auto callGraph = new llvm::CallGraph(*module);
    bool viable = true;
    if (value->getName().str() == ExtractFunction) {
      getUses(*value);
    }

    for (const auto &[_, node] : *callGraph->getOrInsertFunction(value)) {
      if (node->getFunction() == nullptr) {
        continue;
      }
      const auto child = node->getFunction();
      if (child->getLinkage() == llvm::GlobalValue::InternalLinkage) {
        viable = false;
      }
      out << "c- " << child->getName().str() << ": "
          << visibilityTypes[child->getVisibility()] << " "
          << linkageTypes[child->getLinkage()] << std::endl;
    }

    for (const llvm::User *user : value->users()) {
      if (auto *instruction = llvm::dyn_cast<llvm::Instruction>(user)) {
        auto function = instruction->getParent()->getParent();
        if (function->getLinkage() == llvm::GlobalValue::InternalLinkage ||
            instruction->getOpcode() == llvm::Instruction::Store) {
          viable = false;
        }
        if (auto callInstruction =
                llvm::dyn_cast<llvm::CallBase>(instruction)) {

          // if the instruction that references this function isn't calling it
          // it is being passed as argument. And we don't want functions which
          // have their addresses taken.
          if (callInstruction->getCalledFunction() != value) {
            viable = false;
          }
        }

        out << "Inst: " << instruction->getOpcodeName() << "<"
            << instruction->getOpcode() << ">" << function->getName().str()
            << ": " << visibilityTypes[function->getVisibility()] << " "
            << linkageTypes[function->getLinkage()] << "\n";

        command << "--func=" << function->getName().str() << " ";
        if (value->getName().str() == ExtractFunction) {
          for (auto global : getUses(*function)) {
            command << "--glob=" << global->getName().str() << " ";
          }
        }

        for (const auto &[_, node] :
             *callGraph->getOrInsertFunction(function)) {
          if (node->getFunction() == nullptr) {
            continue;
          }
          const auto child = node->getFunction();
          if (child->getLinkage() == llvm::GlobalValue::InternalLinkage) {
            // viable = false;
          }
          out << "u- \t\t" << child->getName().str() << ": "
              << visibilityTypes[child->getVisibility()] << " "
              << linkageTypes[child->getLinkage()] << std::endl;
        }
      } else if (auto *globalValue = llvm::dyn_cast<llvm::GlobalValue>(user)) {
        out << "g- " << globalValue->getName().str() << "\n";
        viable = false;
      } else {
        viable = false;
        if (auto *constant = llvm::dyn_cast<llvm::Constant>(user)) {
          out << "const- " << constant->getType() << "\n";
        }
      }
    }
    command << "-o split1.bc" << std::endl;
    if (ExtractFunction.getNumOccurrences() == 0 ||
        value->getName().str() == ExtractFunction) {
      if (viable) {
        std::cout << key << ": " << visibility << " " << linkage << std::endl;
        if(Verbose) {
          std::cout << out.str();
        }
        std::cout << command.str();
      }
    }
  }
}
