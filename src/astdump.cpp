#include "astdump.h"
#include <sstream>

namespace minic {

namespace {

void indent(std::ostringstream& os, int d) {
    for (int i = 0; i < d; ++i) os << "  ";
}

const char* opSymbol(Tok k) {
    switch (k) {
        case Tok::Plus: return "+"; case Tok::Minus: return "-";
        case Tok::Star: return "*"; case Tok::Slash: return "/";
        case Tok::Percent: return "%";
        case Tok::Eq: return "=="; case Tok::Ne: return "!=";
        case Tok::Lt: return "<"; case Tok::Le: return "<=";
        case Tok::Gt: return ">"; case Tok::Ge: return ">=";
        case Tok::AndAnd: return "&&"; case Tok::OrOr: return "||";
        case Tok::Not: return "!"; default: return "?";
    }
}

void dumpExpr(std::ostringstream& os, const Expr& e, int d);
void dumpStmt(std::ostringstream& os, const Stmt& s, int d);

void dumpExpr(std::ostringstream& os, const Expr& e, int d) {
    indent(os, d);
    if (auto n = dynamic_cast<const NumExpr*>(&e)) {
        os << "Num " << n->value << "\n";
    } else if (auto v = dynamic_cast<const VarExpr*>(&e)) {
        os << "Var " << v->name << "\n";
    } else if (auto a = dynamic_cast<const AssignExpr*>(&e)) {
        os << "Assign " << a->name << "\n";
        dumpExpr(os, *a->value, d + 1);
    } else if (auto u = dynamic_cast<const UnaryExpr*>(&e)) {
        os << "Unary " << opSymbol(u->op) << "\n";
        dumpExpr(os, *u->operand, d + 1);
    } else if (auto b = dynamic_cast<const BinaryExpr*>(&e)) {
        os << "Binary " << opSymbol(b->op) << "\n";
        dumpExpr(os, *b->lhs, d + 1);
        dumpExpr(os, *b->rhs, d + 1);
    } else if (auto c = dynamic_cast<const CallExpr*>(&e)) {
        os << "Call " << c->callee << "\n";
        for (const auto& arg : c->args) dumpExpr(os, *arg, d + 1);
    }
}

void dumpStmt(std::ostringstream& os, const Stmt& s, int d) {
    indent(os, d);
    if (auto b = dynamic_cast<const BlockStmt*>(&s)) {
        os << "Block\n";
        for (const auto& st : b->stmts) dumpStmt(os, *st, d + 1);
    } else if (auto decl = dynamic_cast<const DeclStmt*>(&s)) {
        os << "Decl " << decl->name << "\n";
        if (decl->init) dumpExpr(os, *decl->init, d + 1);
    } else if (auto e = dynamic_cast<const ExprStmt*>(&s)) {
        os << "ExprStmt\n";
        dumpExpr(os, *e->expr, d + 1);
    } else if (auto i = dynamic_cast<const IfStmt*>(&s)) {
        os << "If\n";
        indent(os, d + 1); os << "cond:\n";
        dumpExpr(os, *i->cond, d + 2);
        indent(os, d + 1); os << "then:\n";
        dumpStmt(os, *i->then, d + 2);
        if (i->els) {
            indent(os, d + 1); os << "else:\n";
            dumpStmt(os, *i->els, d + 2);
        }
    } else if (auto w = dynamic_cast<const WhileStmt*>(&s)) {
        os << "While\n";
        indent(os, d + 1); os << "cond:\n";
        dumpExpr(os, *w->cond, d + 2);
        indent(os, d + 1); os << "body:\n";
        dumpStmt(os, *w->body, d + 2);
    } else if (auto r = dynamic_cast<const ReturnStmt*>(&s)) {
        os << "Return\n";
        if (r->value) dumpExpr(os, *r->value, d + 1);
    }
}

} // namespace

std::string dumpAst(const Program& prog) {
    std::ostringstream os;
    for (const auto& fn : prog.functions) {
        os << "Function " << fn->name << "(";
        for (size_t i = 0; i < fn->params.size(); ++i) {
            if (i) os << ", ";
            os << fn->params[i].name;
        }
        os << ") -> " << (fn->isVoid ? "void" : "int") << "\n";
        dumpStmt(os, *fn->body, 1);
    }
    return os.str();
}

} // namespace minic
