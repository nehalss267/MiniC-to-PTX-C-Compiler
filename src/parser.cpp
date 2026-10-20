#include "parser.h"
#include "diag.h"

namespace minic {

const Token& Parser::peek(int ahead) const {
    size_t p = pos_ + (size_t)ahead;
    if (p >= toks_.size()) p = toks_.size() - 1;
    return toks_[p];
}

const Token& Parser::cur() const { return peek(0); }

Token Parser::advance() {
    Token t = toks_[pos_];
    if (pos_ + 1 < toks_.size()) pos_++;
    return t;
}

bool Parser::check(Tok k) const { return cur().kind == k; }

bool Parser::match(Tok k) {
    if (check(k)) { advance(); return true; }
    return false;
}

Token Parser::expect(Tok k, const std::string& what) {
    if (!check(k)) {
        std::string got = std::string("'") + tokName(cur().kind) + "'";
        errorAt(cur().line, cur().col, "expected " + what + " but found " + got);
    }
    return advance();
}

// ---------------- Program / Functions ----------------

std::unique_ptr<Program> Parser::parseProgram() {
    auto prog = std::unique_ptr<Program>(new Program());
    while (!check(Tok::End)) {
        prog->functions.push_back(parseFunction());
    }
    return prog;
}

std::unique_ptr<Function> Parser::parseFunction() {
    auto fn = std::unique_ptr<Function>(new Function());
    const Token& start = cur();
    fn->line = start.line;
    fn->col = start.col;

    if (match(Tok::KwInt)) fn->isVoid = false;
    else if (match(Tok::KwVoid)) fn->isVoid = true;
    else errorAt(cur().line, cur().col, "expected return type ('int' or 'void')");

    Token name = expect(Tok::Ident, "function name");
    fn->name = name.text;

    expect(Tok::LParen, "'('");
    if (!check(Tok::RParen)) {
        for (;;) {
            Param p;
            // Parameters are declared as `int name` (optionally `void` for none).
            if (!match(Tok::KwInt)) {
                errorAt(cur().line, cur().col, "expected 'int' before parameter name");
            }
            Token pt = expect(Tok::Ident, "parameter name");
            p.name = pt.text;
            p.line = pt.line;
            p.col = pt.col;
            fn->params.push_back(p);
            if (!match(Tok::Comma)) break;
        }
    }
    expect(Tok::RParen, "')'");
    fn->body = parseBlock();
    return fn;
}

std::unique_ptr<BlockStmt> Parser::parseBlock() {
    auto blk = std::unique_ptr<BlockStmt>(new BlockStmt());
    const Token& start = cur();
    blk->line = start.line;
    blk->col = start.col;
    expect(Tok::LBrace, "'{'");
    while (!check(Tok::RBrace) && !check(Tok::End)) {
        blk->stmts.push_back(parseStatement());
    }
    expect(Tok::RBrace, "'}'");
    return blk;
}

// ---------------- Statements ----------------

std::unique_ptr<Stmt> Parser::parseStatement() {
    if (check(Tok::LBrace)) return parseBlock();
    if (check(Tok::KwIf)) return parseIf();
    if (check(Tok::KwWhile)) return parseWhile();
    if (check(Tok::KwReturn)) return parseReturn();
    return parseDeclOrExpr();
}

std::unique_ptr<Stmt> Parser::parseDeclOrExpr() {
    if (check(Tok::KwInt)) {
        auto d = std::unique_ptr<DeclStmt>(new DeclStmt());
        const Token& start = cur();
        d->line = start.line;
        d->col = start.col;
        advance(); // int
        Token name = expect(Tok::Ident, "variable name");
        d->name = name.text;
        if (match(Tok::Assign)) {
            d->init = parseExpr();
        }
        expect(Tok::Semi, "';'");
        return std::unique_ptr<Stmt>(std::move(d));
    }
    // expression or assignment statement
    auto e = std::unique_ptr<ExprStmt>(new ExprStmt());
    const Token& start = cur();
    e->line = start.line;
    e->col = start.col;
    e->expr = parseExpr();
    expect(Tok::Semi, "';'");
    return std::unique_ptr<Stmt>(std::move(e));
}

std::unique_ptr<Stmt> Parser::parseIf() {
    auto s = std::unique_ptr<IfStmt>(new IfStmt());
    const Token& start = cur();
    s->line = start.line;
    s->col = start.col;
    advance(); // if
    expect(Tok::LParen, "'('");
    s->cond = parseExpr();
    expect(Tok::RParen, "')'");
    s->then = parseStatement();
    if (match(Tok::KwElse)) {
        s->els = parseStatement();
    }
    return std::unique_ptr<Stmt>(std::move(s));
}

std::unique_ptr<Stmt> Parser::parseWhile() {
    auto s = std::unique_ptr<WhileStmt>(new WhileStmt());
    const Token& start = cur();
    s->line = start.line;
    s->col = start.col;
    advance(); // while
    expect(Tok::LParen, "'('");
    s->cond = parseExpr();
    expect(Tok::RParen, "')'");
    s->body = parseStatement();
    return std::unique_ptr<Stmt>(std::move(s));
}

std::unique_ptr<Stmt> Parser::parseReturn() {
    auto s = std::unique_ptr<ReturnStmt>(new ReturnStmt());
    const Token& start = cur();
    s->line = start.line;
    s->col = start.col;
    advance(); // return
    if (!check(Tok::Semi)) {
        s->value = parseExpr();
    }
    expect(Tok::Semi, "';'");
    return std::unique_ptr<Stmt>(std::move(s));
}

// ---------------- Expressions ----------------

std::unique_ptr<Expr> Parser::parseExpr() { return parseAssign(); }

std::unique_ptr<Expr> Parser::parseAssign() {
    // Only IDENT '=' ... is a valid assignment target in MiniC.
    if (check(Tok::Ident) && peek(1).kind == Tok::Assign) {
        auto a = std::unique_ptr<AssignExpr>(new AssignExpr());
        const Token& start = cur();
        a->line = start.line;
        a->col = start.col;
        Token name = advance();
        a->name = name.text;
        advance(); // '='
        a->value = parseAssign();
        return std::unique_ptr<Expr>(std::move(a));
    }
    return parseLor();
}

std::unique_ptr<Expr> Parser::parseLor() {
    auto lhs = parseLand();
    while (check(Tok::OrOr)) {
        auto b = std::unique_ptr<BinaryExpr>(new BinaryExpr());
        b->line = lhs->line; b->col = lhs->col;
        b->op = advance().kind;
        b->lhs = std::move(lhs);
        b->rhs = parseLand();
        lhs = std::move(b);
    }
    return lhs;
}

std::unique_ptr<Expr> Parser::parseLand() {
    auto lhs = parseEquality();
    while (check(Tok::AndAnd)) {
        auto b = std::unique_ptr<BinaryExpr>(new BinaryExpr());
        b->line = lhs->line; b->col = lhs->col;
        b->op = advance().kind;
        b->lhs = std::move(lhs);
        b->rhs = parseEquality();
        lhs = std::move(b);
    }
    return lhs;
}

std::unique_ptr<Expr> Parser::parseEquality() {
    auto lhs = parseRelational();
    while (check(Tok::Eq) || check(Tok::Ne)) {
        auto b = std::unique_ptr<BinaryExpr>(new BinaryExpr());
        b->line = lhs->line; b->col = lhs->col;
        b->op = advance().kind;
        b->lhs = std::move(lhs);
        b->rhs = parseRelational();
        lhs = std::move(b);
    }
    return lhs;
}

std::unique_ptr<Expr> Parser::parseRelational() {
    auto lhs = parseAdditive();
    while (check(Tok::Lt) || check(Tok::Le) || check(Tok::Gt) || check(Tok::Ge)) {
        auto b = std::unique_ptr<BinaryExpr>(new BinaryExpr());
        b->line = lhs->line; b->col = lhs->col;
        b->op = advance().kind;
        b->lhs = std::move(lhs);
        b->rhs = parseAdditive();
        lhs = std::move(b);
    }
    return lhs;
}

std::unique_ptr<Expr> Parser::parseAdditive() {
    auto lhs = parseMultiplicative();
    while (check(Tok::Plus) || check(Tok::Minus)) {
        auto b = std::unique_ptr<BinaryExpr>(new BinaryExpr());
        b->line = lhs->line; b->col = lhs->col;
        b->op = advance().kind;
        b->lhs = std::move(lhs);
        b->rhs = parseMultiplicative();
        lhs = std::move(b);
    }
    return lhs;
}

std::unique_ptr<Expr> Parser::parseMultiplicative() {
    auto lhs = parseUnary();
    while (check(Tok::Star) || check(Tok::Slash) || check(Tok::Percent)) {
        auto b = std::unique_ptr<BinaryExpr>(new BinaryExpr());
        b->line = lhs->line; b->col = lhs->col;
        b->op = advance().kind;
        b->lhs = std::move(lhs);
        b->rhs = parseUnary();
        lhs = std::move(b);
    }
    return lhs;
}

std::unique_ptr<Expr> Parser::parseUnary() {
    if (check(Tok::Minus) || check(Tok::Not)) {
        auto u = std::unique_ptr<UnaryExpr>(new UnaryExpr());
        const Token& start = cur();
        u->line = start.line;
        u->col = start.col;
        u->op = advance().kind;
        u->operand = parseUnary();
        return std::unique_ptr<Expr>(std::move(u));
    }
    return parsePrimary();
}

std::unique_ptr<Expr> Parser::parsePrimary() {
    if (check(Tok::Num)) {
        auto n = std::unique_ptr<NumExpr>(new NumExpr());
        const Token& t = cur();
        n->line = t.line; n->col = t.col; n->value = t.value;
        advance();
        return std::unique_ptr<Expr>(std::move(n));
    }
    if (check(Tok::Ident)) {
        if (peek(1).kind == Tok::LParen) {
            auto c = std::unique_ptr<CallExpr>(new CallExpr());
            const Token& t = cur();
            c->line = t.line; c->col = t.col;
            c->callee = advance().text;
            advance(); // '('
            if (!check(Tok::RParen)) {
                for (;;) {
                    c->args.push_back(parseExpr());
                    if (!match(Tok::Comma)) break;
                }
            }
            expect(Tok::RParen, "')'");
            return std::unique_ptr<Expr>(std::move(c));
        }
        auto v = std::unique_ptr<VarExpr>(new VarExpr());
        const Token& t = cur();
        v->line = t.line; v->col = t.col; v->name = t.text;
        advance();
        return std::unique_ptr<Expr>(std::move(v));
    }
    if (check(Tok::LParen)) {
        advance();
        auto e = parseExpr();
        expect(Tok::RParen, "')'");
        return e;
    }
    errorAt(cur().line, cur().col,
            std::string("expected expression but found '") + tokName(cur().kind) + "'");
    return nullptr;
}

} // namespace minic
