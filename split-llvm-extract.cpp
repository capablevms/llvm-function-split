/*
 * A program that uses llvm-extract to split
 * LLVM bitcode `*.bc` files into modules containing only
 * a single function/global.
 *
 * The exact llvm-extract binary can be be passed as the env
 * var LLVM_EXTRACT to override the default. The reason to do so
 * is that it may differ depending on architectures/systems.
 */

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
#include <filesystem>
#include <stack>
#include <thread>
#include <typeinfo>

static llvm::LLVMContext context;
llvm::SMDiagnostic err;

llvm::cl::opt<std::string> inputFilename(llvm::cl::Positional, llvm::cl::desc("<input file>"),
										 llvm::cl::Required);

llvm::cl::opt<std::string>
	outputDirectory("o", llvm::cl::desc("Where the output files will be created."),
					llvm::cl::Required);
llvm::cl::opt<bool> dry("d", llvm::cl::desc("Run without executing the commands."));

llvm::cl::opt<bool> verbose("v", llvm::cl::desc("Enable verbose output."));

/**
 * Utilize the User api to find all of the operands
 * referenced by a specific instruction.
 *
 * It uses an in-order tree traversal to find all the
 * operands and then stores the ones of type llvm::GlobalVariable
 * and are constants in the globals set.
 *
 * @param user The instruction from which the iteration starts.
 * @return A set of the global variables referenced by user.
 */
std::unordered_set<llvm::GlobalVariable *> iterateOperands(llvm::User &user)
{
	std::stack<llvm::Value *> operands;
	std::unordered_set<llvm::GlobalVariable *> globals;

	for (auto &operand : user.operands())
	{
		operands.push(operand);
	}

	while (operands.size() > 0)
	{
		auto currentOperand = operands.top();
		operands.pop();

		if (auto globalVariable = llvm::dyn_cast<llvm::GlobalVariable>(currentOperand))
		{
			if (globalVariable->isConstant())
			{
				globals.insert(globalVariable);
			}
		}
		else if (auto user = llvm::dyn_cast<llvm::User>(currentOperand))
		{

			for (auto &nextOperand : user->operands())
			{
				if (auto instruction = llvm::dyn_cast<llvm::Instruction>(nextOperand))
				{
					continue;
				}
				else
				{
					operands.push(nextOperand);
				}
			}
		}
	}
	return globals;
}

/**
 * This function uses iterateOperands to find all the
 * globals that will be moved. Then it uses iterateOperands
 * on their initializers (initializers are llvm::Constant which
 * are like instructions but can be resolved at compile time)
 * to see which other globals will need to be moved with them.
 *
 * This algorithm uses BFS to iterate the tree and find all of
 * the needed llvm::GlobalVariable.
 *
 * @user The user from which the search starts.
 * @return All of the global constants that must be moved together.
 */
std::unordered_set<llvm::GlobalVariable *> resolveAllDependencies(llvm::User &user)
{
	std::unordered_set<llvm::GlobalVariable *> globals;
	std::queue<llvm::GlobalVariable *> queueVariablesNeedMoving;
	for (const auto item : iterateOperands(user))
	{
		queueVariablesNeedMoving.push(item);
	}
	while (queueVariablesNeedMoving.size() > 0)
	{
		auto currentVariable = queueVariablesNeedMoving.front();
		queueVariablesNeedMoving.pop();
		globals.insert(currentVariable);

		if (currentVariable->hasInitializer())
		{
			for (const auto item : iterateOperands(*currentVariable->getInitializer()))
			{
				if (globals.find(item) == globals.end())
				{
					queueVariablesNeedMoving.push(item);
				}
			}
		}
	}
	return globals;
}

/**
 * This is an LLVM instruction visitor.
 * Instead of manually going over all of the basic blocks
 * and then iterating over the instruction a visitor is used
 * to get to the instructions directly.
 */
struct MyPass : public llvm::InstVisitor<MyPass>
{
	std::unordered_set<llvm::GlobalVariable *> globals;
	void visitInstruction(llvm::Instruction &instruction)
	{
		for (const auto item : resolveAllDependencies(instruction))
		{
			globals.insert(item);
		}
	}
};

/**
 * @return The filename of the function/global/variable using the
 * debug information. Returns an empty string if no debug info
 * can be found.
 */
std::string getFileName(llvm::GlobalObject &value)
{
	llvm::SmallVector<std::pair<unsigned, llvm::MDNode *>, 4> MDs;
	value.getAllMetadata(MDs);
	for (auto &MD : MDs)
	{
		if (llvm::MDNode *N = MD.second)
		{
			if (auto *subProgram = llvm::dyn_cast<llvm::DISubprogram>(N))
			{
				return subProgram->getFilename().str();
			}
		}
	}
	return "";
}

int main(int argc, char **argv)
{
	llvm::cl::ParseCommandLineOptions(argc, argv);

	std::string file;
	std::error_code ecode;
	std::unique_ptr<llvm::Module> loadedModule = llvm::parseIRFile(inputFilename, err, context);
	std::unordered_map<const llvm::Function *, std::set<llvm::GlobalVariable *>> users;
	std::string extractFilename = "temp.bc";
	std::string extractProgram = "llvm-extract";

	if (auto extractProgramOverride = std::getenv("LLVM_EXTRACT"))
	{
		extractProgram = std::string(extractProgramOverride);
	}

	/**
	 * Moving happens on multiple stages.
	 *
	 * This is the first stage where all the properties of the
	 * globals and functions in the modules are changed and saved
	 * as a temporary module. This is done because later on
	 * llvm-extract will be ran on this new module not on the
	 * one given as an argument to the program.
	 */

	for (auto &global : loadedModule->globals())
	{
		global.setDSOLocal(false);
		global.setLinkage(llvm::GlobalValue::LinkageTypes::ExternalLinkage);
		global.setVisibility(llvm::GlobalValue::VisibilityTypes::DefaultVisibility);
	}

	for (auto &function : loadedModule->functions())
	{
		if (function.isDeclaration())
		{
			continue;
		}
		function.setDSOLocal(false);
		function.setLinkage(llvm::GlobalValue::ExternalLinkage);
		function.setVisibility(llvm::GlobalValue::VisibilityTypes::DefaultVisibility);
	}

	if (!dry)
	{
		llvm::raw_fd_ostream outputFile("temp.bc", ecode);
		llvm::WriteBitcodeToFile(*loadedModule, outputFile);
		outputFile.close();
	}

	/**
	 * This is the second stage.
	 *
	 * Here we find all of the globals that are suitable for
	 * moving, create a command to move them and then execute it,
	 */
#pragma omp parallel
#pragma omp single
	for (llvm::GlobalVariable &globalVariable : loadedModule->globals())
	{
		std::string globName;

		if (globalVariable.isConstant() && globalVariable.hasInitializer())
		{
			if (!globalVariable.getInitializer()->hasName())
			{
				continue;
			}
			globName = globalVariable.getInitializer()->getName().str();
		}
		else
		{
			globName = globalVariable.getName().str();
		}

		std::cout << getFileName(globalVariable) << "\n";
		std::stringstream command;
		command << extractProgram << " " << extractFilename << " --glob=" << globName << " ";

		if (globalVariable.hasInitializer())
		{
			for (const auto dependency : resolveAllDependencies(*globalVariable.getInitializer()))
			{
				command << " --glob=" << dependency->getName().str() << " ";
			}
		}

		// Create directory if it does not exist
		std::filesystem::create_directory(outputDirectory.c_str());
		command << "-o " << outputDirectory << "/"
				<< "_" << globName << ".bc ";

		std::cout << command.str() << "\n\n";
		std::string materializedCommand = command.str();
#pragma omp task
		if (!dry)
		{
			system(materializedCommand.c_str());
		}
	}

	/**
	 * This is the third stage.
	 *
	 * Here all of the already moved variables' definitions get changed to
	 * declarations. This is done because when moving llvm-extract will just
	 * copy the referenced globals, but that means that it will copy it as an
	 * empty definition, which will fail when compiling, when changed to a
	 * declaration it would expect the global to be defined in an external module.
	 * Changing these things again requires creating a new temp.bc file that will
	 * then be used when splitting.
	 */
	for (auto &global : loadedModule->globals())
	{
		if (global.isConstant())
		{
			continue;
		}
		global.setDSOLocal(false);
		global.setInitializer(nullptr);
	}

	if (!dry)
	{
		llvm::raw_fd_ostream outputFile("temp.bc", ecode);
		llvm::WriteBitcodeToFile(*loadedModule, outputFile);
		outputFile.close();
	}

	/**
	 * This is the forth and final stage.
	 *
	 * All of the functions are iterated and commands for splitting
	 * them are then created and executed.
	 */
#pragma omp parallel
#pragma omp single
	for (llvm::Function &function : loadedModule->functions())
	{
		if (function.isDeclaration())
		{
			continue;
		}

		std::cout << "filename: " << getFileName(function) << "\n";

		const auto name = function.getName().str();
		MyPass pass;
		pass.visit(function);

		std::stringstream command;
		command << extractProgram << " " << extractFilename << " --func=" << name << " ";

		for (auto const &use : pass.globals)
		{
			command << "--glob=" << use->getName().str() << " ";
		}

		command << "-o " << outputDirectory << "/"
				<< "_" << name << ".bc ";

		std::cout << command.str() << "\n\n";
		const auto materializedCommand = command.str();
#pragma omp task
		if (!dry)
		{
			std::cout << "# " << std::this_thread::get_id() << " : " << name << "\n";
			system(materializedCommand.c_str());
		}
	}
}
