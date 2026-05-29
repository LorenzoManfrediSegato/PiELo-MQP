#include "gc.h"
#include "vm.h"
#include <iostream>
#include <algorithm>

namespace PiELo {
    // mark a variable's reachable closures
    void GarbageCollector::markVariable(Variable& var){
        if (var.getType() == PIELO_CLOSURE) markVar(var.getClosureIndex());
        // Dependants are closures that must re-run when this variable changes; while the
        // variable is reachable they must stay alive even if no longer on the stack or in
        // a symbol table (e.g. a reactive function whose call result was discarded).
        for (size_t depIndex : var.dependants) markVar(depIndex);
    }

    // mark one var
    void GarbageCollector::markVar(size_t closureIndex){
        // look for entry in gcelement and mark it. Use find (not at/[]) so a stale index
        // returns harmlessly instead of throwing or default-inserting an empty closure.
        auto it = closureList.find(closureIndex);
        if (it == closureList.end()) return;
        ClosureData* closure = &it->second;
        if (closure->marked) return; // already visited; also breaks dependency cycles
        closure->marked = true;
        // symbol table
        for (auto &pair: closure->localSymbolTable){
            markVariable(pair.second);
        }

        // Mark all dependencies. A reactive closure may be defined (and live in a
        // symbol table) before its dependency variable exists -- e.g. the variable is
        // created by a later 'set'. A not-yet-defined dependency cannot reference a
        // live closure, so skip it rather than letting findVariable throw and abort GC.
        for (auto varName : closure->dependencies) {
            Variable* var;
            try {
                var = findVariable(varName);
            } catch (...) {
                continue;
            }
            if (var->getType() == PIELO_CLOSURE) markVar(var->getClosureIndex());
        }

        // Closures that depend on this one must likewise survive to be re-run.
        for (size_t depIndex : closure->dependants) markVar(depIndex);

        if (closure->cachedValue.getType() == PIELO_CLOSURE) markVar(closure->cachedValue.asClosureIndex);
    }

    // mark all roots
    void GarbageCollector::markRoots(){
        // Mark the closure we're currently in
        markVar(currentClosureIndex);

        // global symbol table
        for (auto &pair : globalSymbolTable){
            markVariable(pair.second);
        }

        // tagged table
        for (auto &pair : taggedTable){
            markVariable(pair.second);
        }

        // call stack
        std::stack<Variable> tempStack = stack;
        while (!tempStack.empty()) {
            if (tempStack.top().getType() == PIELO_CLOSURE) 
                markVar(tempStack.top().getClosureIndex());
            tempStack.pop();
        }

        // local sym tables
        for (auto &closurePair : closureList) {
            if (closurePair.second.marked) {
                for (auto &localPair : closurePair.second.localSymbolTable) {
                    markVariable(localPair.second);
                }
            }

        }

        // Mark all closures in the return address stack
        std::stack<scopeData> tempReturnStack = returnAddrStack;
        while (!tempReturnStack.empty()) {
            markVar(tempReturnStack.top().closureIndex);
            tempReturnStack.pop();
        }
    }

    // sweep through gc heap and free unmarked
    void GarbageCollector::sweep() {

        // std::cout << "sweeping" << std::endl;
        for (std::map<size_t, ClosureData>::iterator it = closureList.begin(); it != closureList.end();) {
            // std::cout << "Looking at closure index " << it->first << std::endl;
            // Erase if not marked and move to the next item (erase does ++ for you)
            if (!it->second.marked) {
                // std::cout << "erasing closure index" << it->first << std::endl;
                it = closureList.erase(it);
            }
            else {
                it->second.marked = false;
                it++;
            }
        }
    }

    // run full gc cycle
    void GarbageCollector::collectGarbage() {
        std::cout << "GC: Starting Collection. closureList size = " << closureList.size() << std::endl;
        markRoots();
        sweep();
        std::cout << "GC  Collection complete. closureList size = " << closureList.size() << std::endl;
    }

    size_t GarbageCollector::heapSize(){
        return closureList.size();
    }

}
