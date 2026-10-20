#include "irbuilder.h"
#include "symtab.h"

namespace minic {

Operand IRBuilder::newTemp() {
    return Operand::temp(fn_->tempCount++);
}

Operand IRBuilder::varOperand(const std::string& name) {
    auto it = varIndex_.find(name);
    int idx;
    if (it == varIndex_.end()) {
        idx = fn_->varCount++;
        varIndex_[name] = idx;
        fn_->varNames.push_back(name);
    } else {
        idx = it->second;
    }
    return Operand::var(idx, name);
}

std::string IRBuilder::newLabel(const std::string& hint) {
    return hint + "_" + std::to_string(labelCounter_++);
}

void IRBuilder::emit(const Instr& in) {
    fn_->code.push_back(in);
}

std::unique_ptr<IRProgram> IRBuilder::build(const Program& prog) {
    auto out = std::unique_ptr<IRProgram>(new IRProgram());
    for (const auto& f : prog.functions) {
        out->functions.push_back(IRFunction());
        fn_ = &out->functions.back();
        buildFunction(*f);
    }
    return out;
}

void IRBuilder::buildFunction(const Function& f) {
    varIndex_.clear();
    labelCounter_ = 0;
    fn_->name = f.name;
    fn_->isVoid = f.isVoid;

    // Parameters become variables 0..n-1.
    for (const auto& p : f.params) {
        Operand v = varOperand(p.name);
        (void)v;
        fn_->params.push_back(p.name);
    }
    buildBlock(*f.body);
}

void IRBuilder::buildBlock(const BlockStmt& b) {
    for (const auto& s : b.stmts) buildStmt(*s);
}

void IRBuilder::buildStmt(const Stmt& s) {
    if (auto d = dynamic_cast<const DeclStmt*>(&s)) {
        Operand v = varOperand(d->name);
        if (d->init) {
            Operand r = buildExpr(*d->init);
            Instr in; in.op = IrOp::Assign; in.dst = v; in.lhs = r; in.line = d->line;
            emit(in);
        } else {
            Instr in; in.op = IrOp::Assign; in.dst = v;
            in.lhs = Operand::constant(0); in.line = d->line;
            emit(in);
        }
        return;
    }
    if (auto b = dynamic_cast<const BlockStmt*>(&s)) { buildBlock(*b); return; }
    if (auto e = dynamic_cast<const ExprStmt*>(&s)) { buildExpr(*e->expr); return; }
    if (auto i = dynamic_cast<const IfStmt*>(&s)) {
        Operand c = buildExpr(*i->cond);
        std::string lElse = newLabel("else");
        std::string lEnd = newLabel("endif");
        Instr jz; jz.op = IrOp::JumpIfZero; jz.lhs = c;
        jz.label = i->els ? lElse : lEnd; jz.line = i->line;
        emit(jz);
        buildStmt(*i->then);
        if (i->els) {
            Instr j; j.op = IrOp::Jump; j.label = lEnd; j.line = i->line;
            emit(j);
            Instr le; le.op = IrOp::Label; le.label = lElse; le.line = i->line;
            emit(le);
            buildStmt(*i->els);
        }
        Instr end; end.op = IrOp::Label; end.label = lEnd; end.line = i->line;
        emit(end);
        return;
    }
    if (auto w = dynamic_cast<const WhileStmt*>(&s)) {
        std::string lTop = newLabel("while_top");
        std::string lEnd = newLabel("while_end");
        Instr top; top.op = IrOp::Label; top.label = lTop; top.line = w->line;
        emit(top);
        Operand c = buildExpr(*w->cond);
        Instr jz; jz.op = IrOp::JumpIfZero; jz.lhs = c; jz.label = lEnd; jz.line = w->line;
        emit(jz);
        buildStmt(*w->body);
        Instr j; j.op = IrOp::Jump; j.label = lTop; j.line = w->line;
        emit(j);
        Instr end; end.op = IrOp::Label; end.label = lEnd; end.line = w->line;
        emit(end);
        return;
    }
    if (auto r = dynamic_cast<const ReturnStmt*>(&s)) {
        Instr in; in.op = IrOp::Return; in.line = r->line;
        if (r->value && !fn_->isVoid) in.lhs = buildExpr(*r->value);
        emit(in);
        return;
    }
}

Operand IRBuilder::buildExpr(const Expr& e) {
    if (auto n = dynamic_cast<const NumExpr*>(&e)) {
        return Operand::constant(n->value);
    }
    if (auto v = dynamic_cast<const VarExpr*>(&e)) {
        return varOperand(v->name);
    }
    if (auto a = dynamic_cast<const AssignExpr*>(&e)) {
        Operand r = buildExpr(*a->value);
        Operand dst = varOperand(a->name);
        Instr in; in.op = IrOp::Assign; in.dst = dst; in.lhs = r; in.line = a->line;
        emit(in);
        return dst;
    }
    if (auto u = dynamic_cast<const UnaryExpr*>(&e)) {
        Operand s = buildExpr(*u->operand);
        Operand t = newTemp();
        Instr in; in.op = IrOp::Unary; in.dst = t; in.lhs = s;
        in.binOp = u->op; in.line = u->line;
        emit(in);
        return t;
    }
    if (auto b = dynamic_cast<const BinaryExpr*>(&e)) {
        // Short-circuit && and || lower to control flow.
        //   a && b :  t=0; if a==0 goto end; if b==0 goto end; t=1; end:
        //   a || b :  t=1; if a!=0 goto end; if b!=0 goto end; t=0; end:
        // 'if x != 0 goto L' is synthesized as: if x==0 goto Lnext; goto L; Lnext:
        if (b->op == Tok::AndAnd || b->op == Tok::OrOr) {
            bool isAnd = (b->op == Tok::AndAnd);
            Operand t = newTemp();
            std::string lEnd = newLabel(isAnd ? "and_end" : "or_end");

            // Default result: 0 for &&, 1 for ||.
            Instr init; init.op = IrOp::Assign; init.dst = t;
            init.lhs = Operand::constant(isAnd ? 0 : 1); init.line = b->line;
            emit(init);

            Operand l = buildExpr(*b->lhs);

            // For &&: any false operand jumps to lEnd (keeping default 0).
            // For ||: any true operand jumps to lEnd (keeping default 1).
            auto emitShortCircuitTest = [&](const Operand& v) {
                if (isAnd) {
                    Instr jz; jz.op = IrOp::JumpIfZero;
                    jz.lhs = v; jz.label = lEnd; jz.line = b->line;
                    emit(jz);
                } else {
                    // if v == 0 fall through, else goto lEnd.
                    std::string lNext = newLabel("or_next");
                    Instr jz; jz.op = IrOp::JumpIfZero;
                    jz.lhs = v; jz.label = lNext; jz.line = b->line;
                    emit(jz);
                    Instr j; j.op = IrOp::Jump; j.label = lEnd; j.line = b->line;
                    emit(j);
                    Instr ln; ln.op = IrOp::Label; ln.label = lNext; ln.line = b->line;
                    emit(ln);
                }
            };

            emitShortCircuitTest(l);
            // rhs is lowered here so it only executes when not short-circuited.
            Operand r = buildExpr(*b->rhs);
            emitShortCircuitTest(r);

            // Neither operand short-circuited: result is the "decisive" value.
            Instr setDecisive; setDecisive.op = IrOp::Assign; setDecisive.dst = t;
            setDecisive.lhs = Operand::constant(isAnd ? 1 : 0); setDecisive.line = b->line;
            emit(setDecisive);

            Instr le; le.op = IrOp::Label; le.label = lEnd; le.line = b->line;
            emit(le);
            return t;
        }

        Operand l = buildExpr(*b->lhs);
        Operand r = buildExpr(*b->rhs);
        Operand t = newTemp();
        Instr in; in.op = IrOp::Binary; in.dst = t; in.lhs = l; in.rhs = r;
        in.binOp = b->op; in.line = b->line;
        emit(in);
        return t;
    }
    if (auto c = dynamic_cast<const CallExpr*>(&e)) {
        Instr in; in.op = IrOp::Call; in.callee = c->callee; in.line = c->line;
        for (const auto& arg : c->args) in.args.push_back(buildExpr(*arg));
        // Determine void-ness from arity: we don't have the symbol table here,
        // so the PTX backend decides whether to capture the result based on a
        // pre-pass over functions. Reserve a temp regardless.
        Operand t = newTemp();
        in.dst = t;
        emit(in);
        return t;
    }
    return Operand::constant(0);
}

} // namespace minic
