#include <algorithm>
#include <execution>
#include <iostream>
#include <memory>
#include <fstream>
#include <string>
#include <system_error>
#include <unordered_map>

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
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Transforms/Utils/ValueMapper.h>
#include <llvm/Analysis/CallGraph.h>



static llvm::LLVMContext context;
llvm::SMDiagnostic err;

llvm::cl::opt<std::string> InputFilename(llvm::cl::Positional,
                                         llvm::cl::desc("<input file>"));

std::unordered_map<llvm::GlobalValue::LinkageTypes, std::string> linkageTypes = {
  { llvm::GlobalValue::LinkageTypes::ExternalLinkage, "(external)" },
  { llvm::GlobalValue::LinkageTypes::AvailableExternallyLinkage, "(available externally)" },
  { llvm::GlobalValue::LinkageTypes::InternalLinkage, "(internal)" },
  { llvm::GlobalValue::LinkageTypes::PrivateLinkage, "(private)" },
};

std::unordered_map<llvm::GlobalValue::VisibilityTypes, std::string> visibilityTypes = {
  { llvm::GlobalValue::VisibilityTypes::DefaultVisibility, "(default)" },
  { llvm::GlobalValue::VisibilityTypes::HiddenVisibility, "(hidden)" },
  { llvm::GlobalValue::VisibilityTypes::ProtectedVisibility, "(protected)" }
};

int main(int argc, char **argv) {
  llvm::cl::ParseCommandLineOptions(argc, argv);

  auto fileListFile = InputFilename.getValue();
  std::cout << fileListFile << std::endl;
  std::ifstream fileList(fileListFile);

  std::unordered_map<std::string, const llvm::Function*> definedFunctions;

  std::string file;
  std::vector<std::unique_ptr<llvm::Module>> loadedModules;

  while (std::getline(fileList, file))
  {
    if(std::unique_ptr<llvm::Module> module = llvm::parseIRFile(file, err, context)) {
      for(auto const &function : module->functions()) {
        if(!function.isDeclaration()) {
          definedFunctions.emplace(function.getName().str(), &function);
        }
      }
      loadedModules.push_back(std::move(module));
    } else {
      err.print(file.c_str(), llvm::outs());
    }
  }

  for(const auto& module : loadedModules) {
    for(auto const &function : module->functions()) {
      if(function.isDeclaration()) {
        definedFunctions.erase(function.getName().str());
      }
    }
  }
  
  for(const auto &[key, value] : definedFunctions) {
    auto linkage = linkageTypes[value->getLinkage()];
    if(linkage.length() == 0) {
      linkage = std::to_string(value->getLinkage());
    }
    auto visibility = visibilityTypes[value->getVisibility()];

    if(value->getLinkage() != llvm::GlobalValue::InternalLinkage) {
      std::cout << key << ": " << visibility << " " << linkage << std::endl;
    }
  }
}
