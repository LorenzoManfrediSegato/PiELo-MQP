#include "storeLoad.h"
#include "../vm.h"
#include "../instructionHandler.h"
#include "../networking.h"
#include <sys/time.h>
#include "../gc.h"
#include "../debug.h"

namespace PiELo {
 void storeLocal(std::string varName){
        if (stack.empty()){
            throw std::runtime_error("Stack underflow: storeLocal");
        }

        try {
            // This will throw an error if varName is not found
            Variable* var = &currentSymbolTable->at(varName);

            var->mutateValue(stack.top());

            handleDependants(*var);
        } catch (...) {
            (*currentSymbolTable)[varName] = stack.top();
        }

        stack.pop();

        // std::string varName = *(var.getNameValue());
        debugPrint("Stored local to " << varName << std::endl);
    }

    void storeGlobal(std::string varName){
        if (stack.empty()){
            throw std::runtime_error("Stack underflow: storeLocal");
        }

        try {
            // This will throw an error if varName is not found
            Variable* var = &globalSymbolTable.at(varName);
            

            var->mutateValue(stack.top());

            handleDependants(*var);
        } catch (...) {
            (globalSymbolTable)[varName] = stack.top();
        }

        stack.pop();

        // std::string varName = *(var.getNameValue());
        debugPrint("Stored global to " << varName << std::endl);
    }

    void loadToStack(const std::string& varName){
        debugPrint("Beginning loadToStack " << std::endl);
        Variable* var = nullptr;
        

        // search local sym table
        #ifdef __DEBUG_INSTRUCTIONS__
            debugPrint("Searching current symbol table for " << varName << std::endl);
            debugPrint(" symbol table has: " << std::endl);
            for (auto it : *currentSymbolTable) {
                debugPrint("  " << it.first << ":");
                it.second.print();
                debugPrint(std::endl);
            }
        #endif

        auto local = currentSymbolTable -> find(varName);
        if (local != currentSymbolTable -> end()) {
            debugPrint("Found it! " << std::endl);
            var = &(local -> second);
        }

        local = globalSymbolTable.find(varName);
        if (local != globalSymbolTable.end()) {
            debugPrint("Found it! " << std::endl);
            var = &(local -> second);
        }

        // search tagged table
        if (!var){
            auto tag = taggedTable.find(varName);
            if (tag != taggedTable.end()) {
                var = &(tag -> second);
            }
        }
        
        if (!var){
            throw std::runtime_error("Variable not found (loadToStack): "+ varName);
        }
        stack.push(*var);
        #ifdef __DEBUG_INSTRUCTIONS__
        debugPrint("load result: ");
        debugPrint("Stack top: ");
        stack.top().print();
        debugPrint(std::endl);
        #endif
    }

    void tagVariable(const std::string& varName, const std::string& tagName) {
        Variable* var = nullptr;
        
        debugPrint("Tagging variable " << varName << " with tag " << tagName << std::endl);
        // search local sym table
        auto local = currentSymbolTable -> find(varName);
        if (local != currentSymbolTable -> end()){
            var = &(local -> second);
        }

        // search tagged table
        if (!var){
            auto tag = taggedTable.find(varName);
            if (tag != taggedTable.end()) {
                var = &(tag -> second);
            }
        }

        if (!var){
            throw std::runtime_error("Variable not found (tagVariable): " + varName);
        }

        // add tag
        var -> tags.push_back(Tag{tagName});
    }

    void tagRobot(const std::string& tagName) {
        
        // check if tag already exists
        for (const auto& tag : robotTagList){
            if (tag.tagName == tagName){
                // no need to add if it exists
                return;
            }
        }

        robotTagList.push_back(Tag{tagName});
    }

    // Takes in a closure index and does what it says it will do
    void recursivelyAddDependantsOfClosureToReturnAddrStack(size_t closureIndex, std::stack<scopeData> &dependants) {
        // Push the closure itself to the return address stack
        ClosureData* closure = &closureList[closureIndex];
        dependants.push((scopeData) {.scopeSymbolTable = &closure->localSymbolTable, .codePointer = closure->codePointer, .closureIndex = closureIndex});
        debugPrint(closureIndex << ", ");
        // Push each of its dependants to the dependants stack, recursively
        for (size_t depIndex : closure->dependants) {
            recursivelyAddDependantsOfClosureToReturnAddrStack(depIndex, dependants);
        }
    }

    void handleDependants(Variable& var) {
        if (var.dependants.size() > 0) {
            // Store where we currently are
            debugPrint(" Variable has closure index dependants: ");
            returnAddrStack.push((scopeData){.scopeSymbolTable = currentSymbolTable, .codePointer = programCounter, .closureIndex = currentClosureIndex});
            
            std::stack<scopeData> dependants;
            for (size_t i = 0; i < var.dependants.size(); i++) {
                // Push the info of each dependant closure and all of its dependants to the dependants stack
                recursivelyAddDependantsOfClosureToReturnAddrStack(var.dependants[i], dependants);
            }

            // Reverse the order of the found dependants. This ensures that parent nodes are updated before their children
            size_t numDeps = dependants.size();
            for (size_t i = 0; i < numDeps; i++) {
                returnAddrStack.push(dependants.top());
                dependants.pop();
            }
            debugPrint(std::endl);

            // Go to the first closure in the list
            programCounter = returnAddrStack.top().codePointer;
            currentSymbolTable = returnAddrStack.top().scopeSymbolTable;
            currentClosureIndex = returnAddrStack.top().closureIndex;
            returnAddrStack.pop();
            debugPrint(" Jumped to PC " << programCounter << std::endl);
        }
    }

    void storeTagged(const std::string& varName){
        if (stack.empty()){
            throw std::runtime_error("Stack underflow: storeTagged");
        }
        debugPrint("Storing variable " << varName << std::endl);
        try {
            // This will throw an error if varName is not found
            Variable* var = &taggedTable.at(varName);
            

            var->mutateValue(stack.top());
            gettimeofday(&(var->lastUpdated), NULL);

            handleDependants(*var);
        } catch (...) {
            taggedTable[varName] = stack.top();
            gettimeofday(&taggedTable[varName].lastUpdated, NULL);
        }

        // Can assume that the variable now must exist in the tagged table
        broadcastVariable(varName, taggedTable[varName].getVariableData());
        stack.pop();
    }

    void storeStig(const std::string& varName) {
        if (stack.empty()){
            throw std::runtime_error("Stack underflow: storeTagged");
        }
        
        auto it = taggedTable.find(varName);
        if (it != taggedTable.end()) {
            // Variable was found in the taggedTable
            Variable* var = &(*it).second;
            
            var->updateStigValue(robotID, stack.top());
            var->resetIter();
            gettimeofday(&(var->lastUpdated), NULL);

            handleDependants(*var);
        } else {
            // Variable does not exist yet
            Variable empty;
            taggedTable[varName] = empty;
            taggedTable[varName].isStigmergy = true;
            taggedTable[varName].updateStigValue(robotID, stack.top());
            debugPrint("Store stig pushed type " << taggedTable[varName].stigmergyData[robotID].getTypeAsString() << " value " << taggedTable[varName].stigmergyData[robotID].asInt << std::endl)
            taggedTable[varName].resetIter();
            debugPrint("   Iter type " << taggedTable[varName].peekIterValue().getTypeAsString() << " int value " << taggedTable[varName].peekIterValue().asInt << std::endl)
            gettimeofday(&taggedTable[varName].lastUpdated, NULL);

        }

        // Can assume that the variable now must exist in the tagged table
        broadcastVariable(varName, taggedTable[varName]);
        stack.pop();
    }

    void pushNextElementOfStig(const std::string& varName) {
        auto it = taggedTable.find(varName);
        if (it == taggedTable.end()) {
            throw std::runtime_error("Attempted to push next element of non-existent stig variable " + varName);
        }
        Variable* var = &(*it).second;
        
        var->ensureStig();
        VariableData data = var->nextIterValue();
        debugPrint("pushNextElement of stig got type " << data.getTypeAsString() << " int value " << data.asInt << std::endl)
        stack.push(data);
    }

    void isIterAtEnd(const std::string& varName) {
        auto it = taggedTable.find(varName);
        if (it == taggedTable.end()) {
            throw std::runtime_error("Attempted to check iter status of non-stig variable " + varName);
        }
        Variable* var = &(*it).second;
        
        var->ensureStig();
        bool retVal = var->isIterAtEnd();
        debugPrint("isIterAtEnd returned " << retVal << std::endl);
        stack.push(retVal);
    }

    void resetIter(const std::string& varName) {
        auto it = taggedTable.find(varName);
        if (it == taggedTable.end()) {
            throw std::runtime_error("Attempted to check iter status of non-stig variable " + varName);
        }
        Variable* var = &(*it).second;
        var->ensureStig();
        var->resetIter();
    }
}