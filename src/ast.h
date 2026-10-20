#ifndef MINIC_AST_H
#define MINIC_AST_H

#include <string>
#include <vector>
#include <memory>
#include "token.h"

namespace minic {

// ---- Expressions ----
struct Expr {
    int line = 0, col = 0;
    virtual ~Expr() {}
};

struct NumExpr : Expr {
    long value = 0;
};

struct VarExpr : Expr {
    std::string name;
};

struct AssignExpr : Expr {
    std::string name;
    std::unique_ptr<Expr> value;
};

struct UnaryExpr : Expr {
    Tok op;
    std::unique_ptr<Expr> operand;
};

struct BinaryExpr : Expr {
    Tok op;
    std::unique_ptr<Expr> lhs, rhs;
};

struct CallExpr : Expr {
    std::string callee;
    std::vector<std::unique_ptr<Expr>> args;
};

// ---- Statements ----
struct Stmt {
    int line = 0, col = 0;
    virtual ~Stmt() {}
};

struct BlockStmt : Stmt {
    std::vector<std::unique_ptr<Stmt>> stmts;
};

struct DeclStmt : Stmt {
    std::string name;
    std::unique_ptr<Expr> init; // may be null
};

struct ExprStmt : Stmt {
    std::unique_ptr<Expr> expr;
};

struct IfStmt : Stmt {
    std::unique_ptr<Expr> cond;
    std::unique_ptr<Stmt> then, els; // els may be null
};

struct WhileStmt : Stmt {
    std::unique_ptr<Expr> cond;
    std::unique_ptr<Stmt> body;
};

struct ReturnStmt : Stmt {
    std::unique_ptr<Expr> value; // may be null (void)
};

struct Param {
    std::string name;
    int line = 0, col = 0;
};

struct Function {
    std::string name;
    bool isVoid = false;
    std::vector<Param> params;
    std::unique_ptr<BlockStmt> body;
    int line = 0, col = 0;
};

struct Program {
    std::vector<std::unique_ptr<Function>> functions;
};

} // namespace minic

#endif
