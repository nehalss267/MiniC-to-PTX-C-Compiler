#ifndef MINIC_SEMA_H
#define MINIC_SEMA_H

#include "ast.h"
#include "symtab.h"

namespace minic {

// Walks the AST, building the symbol table and checking that:
//  - every variable/function used is declared
//  - no duplicate declarations in the same scope
//  - call arity matches the declared function
//  - return statement presence/absence matches the function type
//  - an entry point `main` with zero parameters exists
// Throws CompileError with line:col on the first violation.
class Sema {
public:
    void analyze(const Program& prog);

private:
    SymbolTable syms_;
    bool curFnIsVoid_ = false;
    std::string curFnName_;

    void checkFunction(const Function& fn);
    void checkStmt(const Stmt& s);
    void checkBlock(const BlockStmt& b);
    void checkExpr(const Expr& e);
};

} // namespace minic

#endif
