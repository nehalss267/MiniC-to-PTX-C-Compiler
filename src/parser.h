#ifndef MINIC_PARSER_H
#define MINIC_PARSER_H

#include <vector>
#include <memory>
#include "token.h"
#include "ast.h"

namespace minic {

// Recursive-descent parser for MiniC. Consumes the token stream produced by
// the lexer and builds a Program AST. Throws CompileError on syntax errors.
class Parser {
public:
    explicit Parser(std::vector<Token> toks) : toks_(std::move(toks)) {}

    std::unique_ptr<Program> parseProgram();

private:
    std::vector<Token> toks_;
    size_t pos_ = 0;

    const Token& peek(int ahead = 0) const;
    const Token& cur() const;
    Token advance();
    bool check(Tok k) const;
    bool match(Tok k);
    Token expect(Tok k, const std::string& what);

    std::unique_ptr<Function> parseFunction();
    std::unique_ptr<BlockStmt> parseBlock();
    std::unique_ptr<Stmt> parseStatement();
    std::unique_ptr<Stmt> parseDeclOrExpr();
    std::unique_ptr<Stmt> parseIf();
    std::unique_ptr<Stmt> parseWhile();
    std::unique_ptr<Stmt> parseReturn();

    std::unique_ptr<Expr> parseExpr();
    std::unique_ptr<Expr> parseAssign();
    std::unique_ptr<Expr> parseLor();
    std::unique_ptr<Expr> parseLand();
    std::unique_ptr<Expr> parseEquality();
    std::unique_ptr<Expr> parseRelational();
    std::unique_ptr<Expr> parseAdditive();
    std::unique_ptr<Expr> parseMultiplicative();
    std::unique_ptr<Expr> parseUnary();
    std::unique_ptr<Expr> parsePrimary();
};

} // namespace minic

#endif
