#include "sema.h"
#include "diag.h"

namespace minic {

void Sema::analyze(const Program& prog) {
    // Pass 1: declare all functions (enables mutual recursion / forward calls).
    for (const auto& fn : prog.functions) {
        Symbol s;
        s.name = fn->name;
        s.kind = SymKind::Func;
        s.isVoid = fn->isVoid;
        s.arity = (int)fn->params.size();
        s.line = fn->line;
        s.col = fn->col;
        if (!syms_.declare(s)) {
            errorAt(fn->line, fn->col, "duplicate function '" + fn->name + "'");
        }
    }

    // Entry point convention.
    const Symbol* mainSym = syms_.lookup("main");
    if (!mainSym || mainSym->kind != SymKind::Func) {
        errorAt(1, 1, "no entry point 'main' function found");
    } else if (mainSym->arity != 0) {
        errorAt(1, 1, "'main' must take zero parameters");
    }

    // Pass 2: check bodies.
    for (const auto& fn : prog.functions) {
        checkFunction(*fn);
    }
}

void Sema::checkFunction(const Function& fn) {
    curFnIsVoid_ = fn.isVoid;
    curFnName_ = fn.name;
    syms_.enterScope();
    for (const auto& p : fn.params) {
        Symbol s;
        s.name = p.name;
        s.kind = SymKind::Var;
        s.line = p.line;
        s.col = p.col;
        if (!syms_.declare(s)) {
            errorAt(p.line, p.col, "duplicate parameter '" + p.name + "'");
        }
    }
    checkBlock(*fn.body);
    syms_.exitScope();
}

void Sema::checkBlock(const BlockStmt& b) {
    syms_.enterScope();
    for (const auto& s : b.stmts) checkStmt(*s);
    syms_.exitScope();
}

void Sema::checkStmt(const Stmt& s) {
    if (auto d = dynamic_cast<const DeclStmt*>(&s)) {
        if (d->init) checkExpr(*d->init);
        Symbol sym;
        sym.name = d->name;
        sym.kind = SymKind::Var;
        sym.line = d->line;
        sym.col = d->col;
        if (!syms_.declare(sym)) {
            errorAt(d->line, d->col, "duplicate declaration of '" + d->name + "'");
        }
        return;
    }
    if (auto b = dynamic_cast<const BlockStmt*>(&s)) { checkBlock(*b); return; }
    if (auto e = dynamic_cast<const ExprStmt*>(&s)) { checkExpr(*e->expr); return; }
    if (auto i = dynamic_cast<const IfStmt*>(&s)) {
        checkExpr(*i->cond);
        checkStmt(*i->then);
        if (i->els) checkStmt(*i->els);
        return;
    }
    if (auto w = dynamic_cast<const WhileStmt*>(&s)) {
        checkExpr(*w->cond);
        checkStmt(*w->body);
        return;
    }
    if (auto r = dynamic_cast<const ReturnStmt*>(&s)) {
        if (r->value) checkExpr(*r->value);
        if (curFnIsVoid_ && r->value) {
            errorAt(r->line, r->col,
                    "'return' with a value in void function '" + curFnName_ + "'");
        }
        if (!curFnIsVoid_ && !r->value) {
            errorAt(r->line, r->col,
                    "non-void function '" + curFnName_ + "' must return a value");
        }
        return;
    }
    errorAt(s.line, s.col, "internal: unknown statement kind");
}

void Sema::checkExpr(const Expr& e) {
    if (auto v = dynamic_cast<const VarExpr*>(&e)) {
        const Symbol* s = syms_.lookup(v->name);
        if (!s || s->kind != SymKind::Var) {
            errorAt(v->line, v->col, "use of undeclared variable '" + v->name + "'");
        }
        return;
    }
    if (auto a = dynamic_cast<const AssignExpr*>(&e)) {
        const Symbol* s = syms_.lookup(a->name);
        if (!s || s->kind != SymKind::Var) {
            errorAt(a->line, a->col, "assignment to undeclared variable '" + a->name + "'");
        }
        checkExpr(*a->value);
        return;
    }
    if (auto u = dynamic_cast<const UnaryExpr*>(&e)) {
        checkExpr(*u->operand);
        return;
    }
    if (auto b = dynamic_cast<const BinaryExpr*>(&e)) {
        checkExpr(*b->lhs);
        checkExpr(*b->rhs);
        return;
    }
    if (auto c = dynamic_cast<const CallExpr*>(&e)) {
        for (const auto& arg : c->args) checkExpr(*arg);
        const Symbol* s = syms_.lookup(c->callee);
        if (!s) {
            errorAt(c->line, c->col, "call to undeclared function '" + c->callee + "'");
        }
        if (s->kind != SymKind::Func) {
            errorAt(c->line, c->col, "'" + c->callee + "' is not a function");
        }
        if ((int)c->args.size() != s->arity) {
            errorAt(c->line, c->col,
                    "function '" + c->callee + "' expects " +
                    std::to_string(s->arity) + " argument(s), got " +
                    std::to_string(c->args.size()));
        }
        return;
    }
    // NumExpr: nothing to check.
}

} // namespace minic
