#ifndef MINIC_SYMTAB_H
#define MINIC_SYMTAB_H

#include <string>
#include <vector>
#include <unordered_map>

namespace minic {

enum class SymKind { Var, Func };

struct Symbol {
    std::string name;
    SymKind kind = SymKind::Var;
    bool isVoid = false;   // for funcs: void return
    int arity = 0;         // for funcs
    int line = 0, col = 0;
};

// Scoped symbol table: a stack of scopes; lookups walk outward.
class SymbolTable {
public:
    SymbolTable() { enterScope(); }

    void enterScope();
    void exitScope();

    // Returns false if name already declared in the current scope.
    bool declare(const Symbol& sym);
    const Symbol* lookup(const std::string& name) const;
    const Symbol* lookupCurrent(const std::string& name) const;

    int depth() const { return (int)scopes_.size(); }

private:
    std::vector<std::unordered_map<std::string, Symbol>> scopes_;
};

} // namespace minic

#endif
