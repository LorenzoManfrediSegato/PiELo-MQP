#include "codegen.h"
#include <iostream>
#include <fstream>
#include <map>

namespace PiELo {
    // The file stream which contains the final code
    std::fstream codeFile;
    // Pointer to the file that assembly is currently being written to
    std::fstream* file;
    // Each function definition gets a temporary file
    // Each function is stored in the functionDefinitions list
    // The current function being written to is pointed to by the index
    // If a new function needs to be defined, the code must change the index to be the end of the list and create a new file for itself
    // Then, the code must set the index back to what it was before when it is finished defining the function
    // That way, if functions are defined inside of each other, the original code will continue to define the higher-level function
    std::vector<std::fstream*> functionDefinitions;
    // Index of -1 indicates codeFile
    long functionDefinitionsIndex = -1;
    enum FunctionType {
        NIL,
        C_CLOSURE,
        PIELO_CLOSURE
    };
    
    void codegen(Expression e); 
    struct VariableInfo {
        std::string scope;
        std::string reactivity;
        std::vector<std::string> dependencies;
        FunctionType functionType = NIL;
    };
    // Name, info of variables. 
    // Scope is local, global, shared, or map
    // Reactivity is reactive or inert
    // Dependencies is only used for functions
    std::map<std::string, VariableInfo> env;

    // Check if an expression is valid as a scope argument
    bool isValidScope(Expression e) {
        
        if (e.type != Expression::SYMBOL || (e.symbolValue != "local" && e.symbolValue != "global" && e.symbolValue != "tagged" && e.symbolValue != "map")) {
            return false;
        }
        return true;
    }

    bool isValidReactivity(Expression e) {
        if (e.type != Expression::SYMBOL || (e.symbolValue != "reactive" && e.symbolValue != "inert")) {
            return false;
        }
        return true;
    }

    // Top-level codegen function. Performs initialization and will write to the filename given
    void codegenProgram(Expression e, std::string filename) {
        // Open the file for input/output, and discard all current contents
        codeFile.open(filename, std::fstream::in | std::fstream::out | std::fstream::trunc);
        if(codeFile.fail()) throw std::runtime_error("Failed to open " + filename);
        file = &codeFile;
        codegen(e);
        *file << "end\n" << std::endl;
        std::cout << "here2" << std::endl;
        // Append all of the function definitions
        for (std::vector<std::fstream*>::iterator funcFile = functionDefinitions.begin(); funcFile != functionDefinitions.end(); funcFile++) {
            (*funcFile)->seekg(0, std::ios_base::beg);
            *file << (*funcFile)->rdbuf();
            (*funcFile)->close();
            delete *funcFile;
        }
        // Delete the temporary files
        for (size_t i = 0; i < functionDefinitions.size(); i++) {
            std::string tmpFilename = "tmp_" + std::to_string(i);
            int result = remove(tmpFilename.c_str());
            if (result != 0) perror(("remove " + tmpFilename).c_str());
        }
        std::cout << "here1" << std::endl;
        file->close();
    }

    // Handles all regular procedures. Expects to be handed a LIST.
    // Similar to codegenList except only handles normal procedures and closure calls
    void codegenProcedure(Expression e) {
        // e.listValue[0] was confirmed to have type SYMBOL by codegenList().
        std::string procedureName = e.listValue[0].symbolValue;
        std::string assemblyInstruction = "";
        int expectedArguments;
        if (procedureName == "+") {
            expectedArguments = 2;
            assemblyInstruction = "add";
        } else if (procedureName == "-") {
            expectedArguments = 2;
            assemblyInstruction = "sub";
        }
        else if (procedureName == "*") {
            expectedArguments = 2;
            assemblyInstruction = "mul";}
        else if (procedureName == "/") {
            expectedArguments = 2;
            assemblyInstruction = "div";
        }
        else if (procedureName == "%") {
            expectedArguments = 1;
            assemblyInstruction = "mod";
        }
        else if (procedureName == "&&") {
            expectedArguments = 2;
            assemblyInstruction = "land";
        }
        else if (procedureName == "||") {
            expectedArguments = 2;
            assemblyInstruction = "lor";
        }
        else if (procedureName == "!") {
            expectedArguments = 1;
            assemblyInstruction = "lnot";
        }
        else if (procedureName == "==") {
            expectedArguments = 2;
            assemblyInstruction = "eql";
        }
        else if (procedureName == "!=") {
            expectedArguments = 2;
            assemblyInstruction = "neql";
        }
        // These are backwards because the arguments are pushed to the stack in opposite order
        else if (procedureName == ">") {
            expectedArguments = 2;
            assemblyInstruction = "lt";
        }
        else if (procedureName == "<") {
            expectedArguments = 2;
            assemblyInstruction = "gt";
        }
        else if (procedureName == ">=") {
            expectedArguments = 2;
            assemblyInstruction = "lte";
        }
        else if (procedureName == "<=") {
            expectedArguments = 2;
            assemblyInstruction = "gte";
        }
        else if (procedureName == "print") {
            expectedArguments = 1;
            assemblyInstruction = "print";
        } else if (procedureName == "spin") {
            expectedArguments = 0;
            assemblyInstruction = "spin";
        } else if (procedureName == "end") {
            expectedArguments = 0;
            assemblyInstruction = "end";
        }

        // Codegen each arg in the list one by one (skipping the procedure name)
        for (std::vector<Expression>::iterator it = e.listValue.begin() + 1; it != e.listValue.end(); it++) {
            codegen(*it);
        }

        if (assemblyInstruction != "") {
            // Plus one for the procedure name itself
            if (e.listValue.size() != expectedArguments + 1) {
                throw std::invalid_argument("Expected " + std::to_string(expectedArguments) + " for operation '" + procedureName + 
                                            "'. Instead, got " + std::to_string(e.listValue.size() - 1) + " arguments. Expression: " + e.toString()); 
            }
            // Built in procedure
            *file << assemblyInstruction << std::endl;
        } else {
            if(env.count(procedureName)) {
                if (env[procedureName].functionType == PIELO_CLOSURE) {
                    // Closure call
                    *file << "push i " << e.listValue.size() - 1 << std::endl; // Number of arguments = number of expressions - 1 (for the procedure itself)
                    *file << "load " << procedureName << std::endl;
                    *file << "call_closure" << std::endl;
                } else if (env[procedureName].functionType == C_CLOSURE) {
                    *file << "call_c_closure " << procedureName << std::endl;
                }   
            } else {
                throw std::invalid_argument("Function " + procedureName + " must be defined before it can be used.");
            }
        }
    }

    int iterCounter = 0;
    // Codegen the foreach loop body
    void codegenIterBlock(std::string mapVar, std::string inVar, Expression body) {
        int localIterCounter = iterCounter++;
        *file << "label iter_start_" << localIterCounter << std::endl;
        *file << "is_iter_at_end " << mapVar << std::endl;
        *file << "jmp_if_not_zero iter_end_" << localIterCounter << std::endl;
        *file << "push_next_in_stig " << mapVar << std::endl;
        *file << "store local " << inVar << std::endl;

        // Codegen the body
        codegen(body);

        *file << "pop\n";
        *file << "jmp iter_start_" << localIterCounter << std::endl;
        *file << "label iter_end_" << localIterCounter << std::endl;
        *file << "push_nil" << std::endl;
    }


    int ifCounter = 0;
    int lambdaCounter = 0;
    int whileCounter = 0;

    // Used to find dependencies of an expression
    // Finds all variable names in the expression, including dependencies of any closures called
    std::vector<std::string> findVariables(Expression e) {
        std::vector<std::string> variables;
        if (e.type == Expression::SYMBOL) {
            // Make sure variable was defined as reactive if it has an apostrophe
            if(e.symbolValue.back() == '\''){
                // Save variable name and delete
                std::string key = e.symbolValue.substr(0, e.symbolValue.size() - 1);
                if (env.count(key) != 0) {
                    // Variable was defined
                    VariableInfo value = env.at(key);
            
                    if (value.scope == "local") {
                        throw std::runtime_error("Error: Reactive variable '" + key + "' cannot be defined in the local scope.");
                    } else {
                        // Record the bare name (env and the VM key on it, not on "name'").
                        variables.push_back(key);
                    }
                } else {
                    throw std::runtime_error("Error: Variable '" + key + "' is not defined.");
                }
            }
        
        } else if (e.type == Expression::LIST) {
            if (e.listValue.size() > 1) {
                if (e.listValue[0].type != Expression::SYMBOL) throw std::invalid_argument("Expected a SYMBOL at the start of a list. Expression: " + e.toString());
                
                // Definitions should not be counted as a dependency
                if (e.listValue[0].symbolValue != "var" && e.listValue[0].symbolValue != "fun") {
                    
                    // Check if the procedure is a defined closure call
                    // Add all dependencies if so
                    if (env.count(e.listValue[0].symbolValue) != 0) {
                        std::vector<std::string> procDeps = env.at(e.listValue[0].symbolValue).dependencies;
                        // Erase all duplicates
                        for (std::string variable : variables) {
                            for (auto depIt = procDeps.begin(); depIt != procDeps.end(); depIt++) {
                                if (*depIt == variable) {
                                    depIt = procDeps.erase(depIt) - 1;
                                }
                            }
                        }
                        variables.insert(variables.end(), procDeps.begin(), procDeps.end());
                    }
                    
                    // Add all variables other than the procedure
                    int i = 1;

                    for (; i < e.listValue.size(); i++) {
                        std::vector<std::string> subVariables = findVariables(e.listValue[i]);
                        // Erase all duplicates
                        for (std::string variable : variables) {
                            for (auto subVarIt = subVariables.begin(); subVarIt != subVariables.end(); subVarIt++) {
                                if (*subVarIt == variable) {
                                    subVarIt = subVariables.erase(subVarIt) - 1;
                                }
                            }
                        }
                        variables.insert(variables.end(), subVariables.begin(), subVariables.end());
                    }
                }
            }
        }
        return variables;
    }

    void defineFunction(std::string name, std::vector<std::string> arguments, std::vector<std::string> dependencies, Expression body) {
        env[name].dependencies = dependencies;
        env[name].functionType = PIELO_CLOSURE;
        // Create new file and adjust file pointer as necessary
        long oldIndex = functionDefinitionsIndex;
        functionDefinitionsIndex = functionDefinitions.size();
        std::cout << "Function " << name << ": oldIndex " << oldIndex << " functionDefinitionsIndex " << functionDefinitionsIndex << std::endl;
        std::fstream* funcFile = new std::fstream();
        funcFile->open("tmp_" + std::to_string(functionDefinitionsIndex), std::ios_base::trunc | std::ios_base::out | std::ios_base::in);
        if(funcFile->fail()) throw std::runtime_error("Failed to open file tmp_" + std::to_string(functionDefinitionsIndex));
        functionDefinitions.push_back(funcFile);
        file = funcFile;
        
        // Add arguments
        *file << "define_closure " << name << " " << arguments.size() << " ";
        for (std::string argument : arguments) *file << argument << " ";
        
        // Add dependencies
        *file << dependencies.size() << " ";
        for (std::string dependency : dependencies) {
            VariableInfo info;
            try {
                info = env.at(dependency);
            } catch (std::out_of_range e) {
                throw std::out_of_range("Could not find info for dependency " + dependency + " of function " + name);
            }
            if (info.scope == "local") throw std::invalid_argument("Dependency " + dependency + " of function " + name + " has local scope. This is not allowed.");
            *file << dependency << " ";
        }

        *file << std::endl;

        // Codegen the body
        codegen(body);

        // Return from the closure and store the result (closure index) into the given variable name
        *file << "push i 1\n" << "ret_from_closure" << std::endl;

        // Set the file back to how it was before
        functionDefinitionsIndex = oldIndex;
        std::cout << "Function " << name << " set functionDefinitionsIndex back to " << functionDefinitionsIndex << std::endl;
        if (functionDefinitionsIndex == -1) file = &codeFile;
        else file = functionDefinitions[functionDefinitionsIndex];
    }

    // For use when the expression is known to be of type LIST. Can handle special cases.
    void codegenList(Expression e) {
        if (e.listValue[0].type != Expression::SYMBOL) {
            throw std::runtime_error("codegenList got LIST with a first element of type " + e.typeToString() + " expression string " + e.toString());
        }

        // Conditionals
        if (e.listValue[0].symbolValue == "if") {
            // if special case would go here
            if (e.listValue.size() != 4) {
                throw std::invalid_argument("Incorrect number of arguments to 'if'. Expected 4, got " + std::to_string(e.listValue.size()) + ". from " + e.toString());
            }
            int localIfCounter = ifCounter++;
            // Conditional
            codegen(e.listValue[1]);
            *file << "jmp_if_zero else_" << localIfCounter << std::endl;
            // Then
            codegen(e.listValue[2]);
            *file << "jmp endif_" << localIfCounter << std::endl;
            // Else
            *file << "label else_" << localIfCounter << std::endl;
            codegen(e.listValue[3]);
            // End
            *file << "label endif_" << localIfCounter << std::endl;
        } 
        
        // Variable declarations
        else if (e.listValue[0].symbolValue == "var") {
            if (e.listValue.size() != 4) {
                throw std::invalid_argument("Incorrect number of arguments to 'vardef'. Expected 4, got " + std::to_string(e.listValue.size()) + ". from " + e.toString());
            }
            Expression scope = e.listValue[1];
            Expression reactivity = e.listValue[2];
            Expression name = e.listValue[3];
            if (!isValidScope(scope)) {
                throw std::invalid_argument("The scope argument must be one of 'local', 'global', 'map', or 'tagged'. Expression was: " + e.toString());
            }
            if (!isValidReactivity(reactivity)) {
                throw std::invalid_argument("The reactivity argument must be one of 'reactive' or 'inert'. Expression was: " + e.toString());
            }
            if (name.type != Expression::SYMBOL) {
                throw std::invalid_argument("The name argument must be a symbol. Expression was: " + e.toString());
            }
            env[name.symbolValue].reactivity = reactivity.symbolValue;
            env[name.symbolValue].scope = scope.symbolValue;

            *file << "push_nil" << std::endl;
            std::cout << "Defined variable " << name.symbolValue << std::endl;
        } 
        
        // Variable Definitions
        else if (e.listValue[0].symbolValue == "set") {
            int localLambdaCounter = lambdaCounter++;
            if (e.listValue.size() != 3) {
                throw std::invalid_argument("Incorrect number of arguments to 'set'. Expected 3, got " + std::to_string(e.listValue.size()) + ". from " + e.toString());
            }
            if (e.listValue[1].type != Expression::SYMBOL) {
                throw std::invalid_argument("Attempted to set a non-symbol. " + e.toString());
            }


            VariableInfo var = env.at(e.listValue[1].symbolValue);
            if (var.scope == "map") var.scope = "stig";
            if (var.reactivity == "reactive") {
                // Define an anonymous function and then immediately call it and store the result in this variable
                std::string name = "__lambda_" + std::to_string(localLambdaCounter);
                std::vector<std::string> dependencies = findVariables(e.listValue[2]);
                defineFunction(name, std::vector<std::string>(), dependencies, e.listValue[2]);

                *file << "push i 0\n" << "load " << name << "\n" << "call_closure\n";
                *file << "store " << var.scope << " " << e.listValue[1].symbolValue << std::endl;
            } else {
                // This needs to be a lambda to make sure that dependencies are ignored.
                // If it is not, if it is set to a reactive function's return value, that reactive function will update itself
                // So this variable is stored as a lambda which will not update when that function does.
                codegen(e.listValue[2]);
                // Uncache the variable if it's cached
                *file << "uncache\n";
                *file << "store " << var.scope << " " << e.listValue[1].symbolValue << std::endl;
            }
            *file << "push_nil" << std::endl;
        } 
        
        // Code blocks
        else if (e.listValue[0].symbolValue == "begin") {
            // Code block
            for (int i = 1; i < e.listValue.size(); i++) {
                codegen(e.listValue[i]);
                // Discard the result of all expressions other than the last
                if (i < e.listValue.size() - 1) *file << "pop" << std::endl;
            }
        } 

        // Function Definitions
        else if (e.listValue[0].symbolValue == "fun") {
            // TODO: Update to make the programmer explicitly define arguments
            if (e.listValue.size() != 5) throw std::invalid_argument("fun expects 5 expressions, got " + std::to_string(e.listValue.size()) + ". Expression: " + e.toString());
            Expression reactivity = e.listValue[1];
            Expression name = e.listValue[2];
            Expression args = e.listValue[3];
            Expression body = e.listValue[4];
            
            if (name.type != Expression::SYMBOL) 
                throw std::invalid_argument("Expected a SYMBOL as second argument to fun. Got a " + e.typeToString() + ". Expression: " + e.toString());
            if (reactivity.type != Expression::SYMBOL) 
                throw std::invalid_argument("Expected a SYMBOL as first argument to fun for function " + name.symbolValue + ". Got a " + e.typeToString() + ". Expression: " + e.toString());
            if (reactivity.symbolValue != "reactive" && reactivity.symbolValue != "inert")
                throw std::invalid_argument("Expected reactivity of function " + name.symbolValue + " to be either 'reactive' or 'inert'. Instead, got " + reactivity.symbolValue);
            if (args.type != Expression::LIST) 
                throw std::invalid_argument("Expected a LIST as third argument to fun for function " + name.symbolValue + ". Got a " + e.typeToString() + ". Expression: " + e.toString());

            // Collect argument names
            std::vector<std::string> argumentNames;
            for (Expression argExp : args.listValue) {
                if (argExp.type != Expression::SYMBOL) throw std::invalid_argument("Expected a SYMBOL as a parameter to function " + name.symbolValue + ". Got a " + e.typeToString() + ". Expression: " + e.toString() + ", argument: " + argExp.toString());
                argumentNames.push_back(argExp.symbolValue);
            }
            

            // Find all dependencies, but ignore arguments
            // If the function is not reactive, it has no dependencies.
            std::vector<std::string> dependencies;
            if (reactivity.symbolValue == "reactive") {
                dependencies = findVariables(body);
                // Loop over all dependencies
                for (std::vector<std::string>::iterator dependency = dependencies.begin(); dependency != dependencies.end(); dependency++) {
                    // For each dependency, check against all arguments
                    for (std::string arg : argumentNames) {
                        if (arg == *dependency) {
                            // If this dependency matches, erase it from the list
                            // Also set the iterator to the next element and end the arg loop
                            dependency = dependencies.erase(dependency);
                            break;
                        }
                    }
                }
            }
            defineFunction(name.symbolValue, argumentNames, dependencies, body); 
            // Everything has to push something!
            *file << "push_nil" << std::endl;
        } 
        else if (e.listValue[0].symbolValue == "print") {
            if (e.listValue.size() < 2) throw std::invalid_argument("Expected at least 1 argument to print. Expression: " + e.toString());

            if (e.listValue[1].type == Expression::SYMBOL && e.listValue[1].symbolValue.at(0) == '"') {
                *file << "debug_print ";
                // Remove the trailing quote
                Expression lastExp = e.listValue[e.listValue.size() - 1];
                if (lastExp.symbolValue.at(lastExp.symbolValue.length() - 1) == '"') {
                    lastExp.symbolValue.erase(lastExp.symbolValue.length() - 1);
                } else {
                    throw std::invalid_argument("Expected ending quote of print statement. Expression: " + e.toString());
                }
                for (int i = 1; i < e.listValue.size(); i++) {
                    *file << e.listValue[i].toString();
                    if (i == e.listValue.size() - 1) *file << " ";
                }
                *file << std::endl;
                *file << "push_nil" << std::endl;
            } else {
                // Just a regular print statement
                codegenProcedure(e);
            }
        }
        else if (e.listValue[0].symbolValue == "foreach") {
            if (e.listValue.size() != 4) 
                throw std::invalid_argument("Expected 3 arguments to foreach. Got " + std::to_string(e.listValue.size() - 1) + " from expression " + e.toString());

            if (e.listValue[1].type != Expression::SYMBOL || (e.listValue[1].symbolValue != "reactive" && e.listValue[1].symbolValue != "inert"))
                throw std::invalid_argument("Expected one of 'reactive' or 'inert' for foreach reactivity. Expression: " + e.toString());
            std::string reactivity = e.listValue[1].symbolValue; 

            if (e.listValue[2].type != Expression::LIST) {
                throw std::invalid_argument("Expected an (in) block in foreach: " + e.toString());
            } 
            Expression inBlock = e.listValue[2];
            if (inBlock.listValue[0].type != Expression::SYMBOL || inBlock.listValue[0].symbolValue != "in") {
                throw std::invalid_argument("Expected in block in foreach: " + e.toString());
            }

            Expression inVar = inBlock.listValue[1]; // The local variable used for the foreach loop
            Expression stigVar = inBlock.listValue[2];
            if (inVar.type != Expression::SYMBOL) 
                throw std::invalid_argument("Expected a variable name as first argument to 'in'. Expression: " + e.toString());
            if (stigVar.type != Expression::SYMBOL || env.count(stigVar.symbolValue) == 0 || env.at(stigVar.symbolValue).scope != "map") 
                throw std::invalid_argument("Expected a map variable as first argument to 'in'. Expression: " + e.toString());
            env[inVar.symbolValue].scope = "global";
            env[inVar.symbolValue].reactivity = "inert";

            Expression body = e.listValue[3];
            // Create a function with the relevant dependencies
            if (reactivity == "reactive") {
                std::string functionBody;

                int localLambdaCounter = lambdaCounter++;
                std::string funcName = "__lambda_" + std::to_string(localLambdaCounter);
                std::vector<std::string> dependencies = findVariables(body);
                dependencies.push_back(stigVar.symbolValue);

                // Delete the in variable from the dependencies - it is local
                for (std::vector<std::string>::iterator dependency = dependencies.begin(); dependency != dependencies.end(); dependency++) {
                    if (*dependency == inVar.symbolValue) {
                        dependencies.erase(dependency);
                        break;
                    }
                }
                *file << "push i 0" << std::endl;
                *file << "load " << funcName << std::endl;
                *file << "call_closure" << std::endl;
                
                env[funcName].dependencies = dependencies;
                env[funcName].functionType = PIELO_CLOSURE;
                std::cout << "foreach before. functionDefinitionsIndex: " << functionDefinitionsIndex << std::endl;
                // Create new file and adjust file pointer as necessary
                long oldIndex = functionDefinitionsIndex;
                functionDefinitionsIndex = functionDefinitions.size();
                std::cout << "foreach" << ": oldIndex " << oldIndex << " functionDefinitionsIndex " << functionDefinitionsIndex << std::endl;
                std::fstream* funcFile = new std::fstream();
                funcFile->open("tmp_" + std::to_string(functionDefinitionsIndex), std::ios_base::trunc | std::ios_base::out | std::ios_base::in);
                if(funcFile->fail()) throw std::runtime_error("Failed to open file tmp_" + std::to_string(functionDefinitionsIndex));
                functionDefinitions.push_back(funcFile);
                file = funcFile;
                
                // Add arguments
                *file << "define_closure " << funcName << " 0 ";
                
                // Add dependencies
                *file << dependencies.size() << " ";
                for (std::string dependency : dependencies) {
                    VariableInfo info;
                    try {
                        info = env.at(dependency);
                    } catch (std::out_of_range e) {
                        throw std::out_of_range("Could not find info for dependency " + dependency + " of function " + funcName);
                    }
                    if (info.scope == "local") throw std::invalid_argument("Dependency " + dependency + " of function " + funcName + " has local scope. This is not allowed.");
                    *file << dependency << " ";
                }

                *file << std::endl;

                codegenIterBlock(stigVar.symbolValue, inVar.symbolValue, body);

                // Return from the closure and store the result (closure index) into the given variable name
                *file << "push i 1\n" << "ret_from_closure" << std::endl;

                // Set the file back to how it was before
                functionDefinitionsIndex = oldIndex;
                if (functionDefinitionsIndex == -1) file = &codeFile;
                else file = functionDefinitions[functionDefinitionsIndex];
            }

            // Just need to generate the loop block itself. No need to worry abotu dependencies.
            else {
                codegenIterBlock(stigVar.symbolValue, inVar.symbolValue, body);
            }
        }
        else if (e.listValue[0].symbolValue == "while") {
            if (e.listValue.size() != 3)
                throw std::invalid_argument("Expected 2 arguments to while. Instead, got " + std::to_string(e.listValue.size() - 1) + " from expression " + e.toString());
            int localWhileCounter = whileCounter++;
            Expression conditional = e.listValue[1];
            Expression body = e.listValue[2];
            *file << "label while_" << localWhileCounter << std::endl;
            codegen(conditional);
            *file << "jmp_if_zero while_end_" << localWhileCounter << std::endl;
            codegen(body);
            *file << "pop" << std::endl;
            *file << "jmp while_" << localWhileCounter << std::endl;
            *file << "label while_end_" << localWhileCounter << std::endl;
        }
        else if (e.listValue[0].symbolValue == "include") {
            if (e.listValue.size() != 2) 
                throw std::invalid_argument("Expected one argument to 'include'. Got " + std::to_string(e.listValue.size() - 1) + ". Expression: " + e.toString());
            
            if (e.listValue[1].type != Expression::SYMBOL)
                throw std::invalid_argument("Expected a symbol as the argument to 'include'. Instead, got a " + e.listValue[1].typeToString() + ". Expression: "  + e.toString());
            
            std::ifstream includeFile;
            includeFile.open(e.listValue[1].symbolValue);
            if (includeFile.fail()) throw std::runtime_error("Failed to open file " + e.listValue[1].symbolValue);
            std::string closureName;
            includeFile >> closureName;
            env[closureName].reactivity = "inert";
            env[closureName].functionType = C_CLOSURE;
            *file << "push_nil" << std::endl; // Must push something!
        }
        else {
            // All builtin procedures and closure calls
            codegenProcedure(e);
        }
    }

    void codegen(Expression expression) {
        std::cout << "Codegen-ing " << expression.toString() << std::endl;
        switch (expression.type) {
            case Expression::LIST:
            codegenList(expression);
            break;
            case Expression::INT:
            *file << "push i " << std::to_string(expression.intValue) << std::endl;
            break;
            case Expression::FLOAT:
            *file << "push f " << std::to_string(expression.floatValue) << std::endl;
            break;
            case Expression::SYMBOL: {
            // Assuming this is not a procedure call as that would be in codegenProcedure.
            // A trailing apostrophe (e.g. a') marks a reactive reference; the dependency is
            // recorded separately by findVariables. The emitted load must use the bare name,
            // since the VM resolves it against tables keyed by the undecorated variable name.
            std::string name = expression.symbolValue;
            if (!name.empty() && name.back() == '\'') name.pop_back();
            *file << "load " << name << std::endl;
            break;
            }
            default:
            std::cout << "nil!" << std::endl;
            break;
        }
    }
}