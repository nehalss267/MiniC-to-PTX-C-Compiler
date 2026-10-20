#include "opt.h"
#include <algorithm>

namespace minic {

static bool isConst(const Operand& o) { return o.kind == OpKind::Const; }

static long evalBinary(Tok op, long a, long b, bool& ok) {
    ok = true;
    switch (op) {
        case Tok::Plus:    return a + b;
        case Tok::Minus:   return a - b;
        case Tok::Star:    return a * b;
        case Tok::Slash:   if (b == 0) { ok = false; return 0; } return a / b;
        case Tok::Percent: if (b == 0) { ok = false; return 0; } return a % b;
        case Tok::Eq:      return a == b ? 1 : 0;
        case Tok::Ne:      return a != b ? 1 : 0;
        case Tok::Lt:      return a <  b ? 1 : 0;
        case Tok::Le:      return a <= b ? 1 : 0;
        case Tok::Gt:      return a >  b ? 1 : 0;
        case Tok::Ge:      return a >= b ? 1 : 0;
        default: ok = false; return 0;
    }
}

static long evalUnary(Tok op, long a, bool& ok) {
    ok = true;
    switch (op) {
        case Tok::Minus: return -a;
        case Tok::Not:   return a == 0 ? 1 : 0;
        default: ok = false; return 0;
    }
}

// Replace instructions whose operands are all constant with a constant Assign.
bool Optimizer::constantFold(IRFunction& fn) {
    bool changed = false;
    for (auto& in : fn.code) {
        if (in.dead) continue;
        if (in.op == IrOp::Binary && isConst(in.lhs) && isConst(in.rhs)) {
            bool ok = false;
            long v = evalBinary(in.binOp, in.lhs.value, in.rhs.value, ok);
            if (!ok) continue;
            in.op = IrOp::Assign;
            in.lhs = Operand::constant(v);
            in.rhs = Operand();
            changed = true;
        } else if (in.op == IrOp::Unary && isConst(in.lhs)) {
            bool ok = false;
            long v = evalUnary(in.binOp, in.lhs.value, ok);
            if (!ok) continue;
            in.op = IrOp::Assign;
            in.lhs = Operand::constant(v);
            changed = true;
        } else if (in.op == IrOp::JumpIfZero && isConst(in.lhs)) {
            // Constant condition: turn into unconditional jump or Nop.
            if (in.lhs.value == 0) {
                in.op = IrOp::Jump;
            } else {
                in.op = IrOp::Nop;
            }
            changed = true;
        }
    }
    return changed;
}

// Sparse constant propagation: a value (temp or var) that is assigned exactly
// one constant and never reassigned is replaced by that constant everywhere it
// is read. This is intra-function and does not attempt full dataflow across
// joins; a value defined in more than one place, or in a loop, is left alone.
bool Optimizer::sparseConstProp(IRFunction& fn) {
    std::vector<long> constVal(fn.tempCount + fn.varCount, 0);
    std::vector<bool> isConstVal(fn.tempCount + fn.varCount, false);
    std::vector<int> defCount(fn.tempCount + fn.varCount, 0);

    auto slotOf = [&](const Operand& o, int& slot) -> bool {
        if (o.kind == OpKind::Temp) { slot = o.index; return true; }
        if (o.kind == OpKind::Var) { slot = fn.tempCount + o.index; return true; }
        return false;
    };

    // Record definitions. A value is eligible only if it has a single def, and
    // that def is an Assign of a constant.
    for (auto& in : fn.code) {
        if (in.dead) continue;
        int slot;
        if (in.op == IrOp::Assign && slotOf(in.dst, slot)) {
            defCount[slot]++;
            if (in.lhs.kind == OpKind::Const) {
                isConstVal[slot] = true;
                constVal[slot] = in.lhs.value;
            }
        } else if ((in.op == IrOp::Binary || in.op == IrOp::Unary || in.op == IrOp::Call) &&
                   slotOf(in.dst, slot)) {
            defCount[slot]++;
        }
    }

    std::vector<bool> eligible(fn.tempCount + fn.varCount, false);
    for (size_t i = 0; i < eligible.size(); ++i) {
        eligible[i] = (defCount[i] == 1 && isConstVal[i]);
    }

    auto substConst = [&](Operand& o) -> bool {
        int slot;
        if (slotOf(o, slot) && eligible[slot]) {
            o = Operand::constant(constVal[slot]);
            return true;
        }
        return false;
    };

    bool changed = false;
    for (auto& in : fn.code) {
        if (in.dead) continue;
        // Do not rewrite the destination of a definition.
        switch (in.op) {
            case IrOp::Assign: if (substConst(in.lhs)) changed = true; break;
            case IrOp::Binary:
                if (substConst(in.lhs)) changed = true;
                if (substConst(in.rhs)) changed = true;
                break;
            case IrOp::Unary: if (substConst(in.lhs)) changed = true; break;
            case IrOp::JumpIfZero: if (substConst(in.lhs)) changed = true; break;
            case IrOp::Return: if (substConst(in.lhs)) changed = true; break;
            case IrOp::Call: for (auto& a : in.args) if (substConst(a)) changed = true; break;
            default: break;
        }
    }
    return changed;
}

// If t is only ever defined once by a copy `t = x` and x is a Const/Var, replace
// uses of t with x and drop the copy. Conservative: only folds temps whose
// single definition is a copy and which are not redefined elsewhere.
bool Optimizer::copyPropagate(IRFunction& fn) {
    // Count definitions and record the source for simple copies.
    std::vector<int> defCount(fn.tempCount, 0);
    std::vector<Operand> copyOf(fn.tempCount);
    std::vector<bool> hasCopy(fn.tempCount, false);

    auto noteDef = [&](const Operand& d, const Operand& src, bool isCopy) {
        if (d.kind == OpKind::Temp) {
            defCount[d.index]++;
            if (isCopy) { copyOf[d.index] = src; hasCopy[d.index] = true; }
        }
    };

    for (auto& in : fn.code) {
        if (in.dead) continue;
        switch (in.op) {
            case IrOp::Assign: {
                bool simple = (in.lhs.kind == OpKind::Const || in.lhs.kind == OpKind::Var);
                noteDef(in.dst, in.lhs, simple);
                break;
            }
            case IrOp::Binary: noteDef(in.dst, Operand(), false); break;
            case IrOp::Unary:  noteDef(in.dst, Operand(), false); break;
            case IrOp::Call:   noteDef(in.dst, Operand(), false); break;
            default: break;
        }
    }

    auto subst = [&](Operand& o) -> bool {
        if (o.kind == OpKind::Temp && defCount[o.index] == 1 && hasCopy[o.index]) {
            o = copyOf[o.index];
            return true;
        }
        return false;
    };

    bool changed = false;
    for (auto& in : fn.code) {
        if (in.dead) continue;
        // Never substitute into the destination position of a definition whose
        // source we're tracking against (only reads).
        switch (in.op) {
            case IrOp::Assign: if (subst(in.lhs)) changed = true; break;
            case IrOp::Binary:
                if (subst(in.lhs)) changed = true;
                if (subst(in.rhs)) changed = true;
                break;
            case IrOp::Unary:  if (subst(in.lhs)) changed = true; break;
            case IrOp::JumpIfZero: if (subst(in.lhs)) changed = true; break;
            case IrOp::Return: if (subst(in.lhs)) changed = true; break;
            case IrOp::Call: for (auto& a : in.args) if (subst(a)) changed = true; break;
            default: break;
        }
    }

    // Now drop single-def copies to temps that became unused.
    auto used = [&](int tempIdx) {
        for (auto& in : fn.code) {
            if (in.dead) continue;
            if (in.op == IrOp::Assign && in.lhs.kind == OpKind::Temp && in.lhs.index == tempIdx) return true;
            if (in.op == IrOp::Binary && ((in.lhs.kind == OpKind::Temp && in.lhs.index == tempIdx) ||
                                          (in.rhs.kind == OpKind::Temp && in.rhs.index == tempIdx))) return true;
            if (in.op == IrOp::Unary && in.lhs.kind == OpKind::Temp && in.lhs.index == tempIdx) return true;
            if (in.op == IrOp::JumpIfZero && in.lhs.kind == OpKind::Temp && in.lhs.index == tempIdx) return true;
            if (in.op == IrOp::Return && in.lhs.kind == OpKind::Temp && in.lhs.index == tempIdx) return true;
            if (in.op == IrOp::Call) {
                if (in.dst.kind == OpKind::Temp && in.dst.index == tempIdx) return true;
                for (auto& a : in.args) if (a.kind == OpKind::Temp && a.index == tempIdx) return true;
            }
        }
        return false;
    };

    for (auto& in : fn.code) {
        if (in.dead) continue;
        if (in.op == IrOp::Assign && in.dst.kind == OpKind::Temp &&
            hasCopy[in.dst.index] && defCount[in.dst.index] == 1 && !used(in.dst.index)) {
            in.dead = true;
            changed = true;
        }
    }
    return changed;
}

// Remove assignments/copies to temps that are never read, and unreachable code.
bool Optimizer::deadCodeEliminate(IRFunction& fn) {
    bool changed = false;

    // Iteratively mark dead until stable.
    bool localChange = true;
    while (localChange) {
        localChange = false;
        std::vector<bool> read(fn.tempCount, false);
        std::vector<bool> readVar(fn.varCount, false);

        auto markRead = [&](const Operand& o) {
            if (o.kind == OpKind::Temp) read[o.index] = true;
            else if (o.kind == OpKind::Var) readVar[o.index] = true;
        };

        for (auto& in : fn.code) {
            if (in.dead) continue;
            switch (in.op) {
                case IrOp::Assign: markRead(in.lhs); break;
                case IrOp::Binary: markRead(in.lhs); markRead(in.rhs); break;
                case IrOp::Unary: markRead(in.lhs); break;
                case IrOp::JumpIfZero: markRead(in.lhs); break;
                case IrOp::Return: markRead(in.lhs); break;
                case IrOp::Call: for (auto& a : in.args) markRead(a); break;
                default: break;
            }
        }

        for (auto& in : fn.code) {
            if (in.dead) continue;
            bool dstTemp = in.dst.kind == OpKind::Temp;
            bool dstVar = in.dst.kind == OpKind::Var;
            bool dstUnread =
                (dstTemp && !read[in.dst.index]) ||
                (dstVar && !readVar[in.dst.index]);
            if (in.op == IrOp::Assign && dstUnread) {
                in.dead = true;
                localChange = true;
                changed = true;
            }
            // Binary/Unary/Call results that are never read.
            if ((in.op == IrOp::Binary || in.op == IrOp::Unary) && dstUnread) {
                in.dead = true;
                localChange = true;
                changed = true;
            }
        }
    }

    removeUnreachable(fn);

    // Drop no-ops that remain after constant-condition folding.
    for (auto& in : fn.code) {
        if (!in.dead && in.op == IrOp::Nop) {
            in.dead = true;
            changed = true;
        }
    }
    return changed;
}

void Optimizer::removeUnreachable(IRFunction& fn) {
    bool reachable = true;
    for (auto& in : fn.code) {
        if (in.dead) continue;
        if (!reachable) {
            if (in.op == IrOp::Label) {
                reachable = true;
            } else {
                in.dead = true;
                continue;
            }
        }
        if (in.op == IrOp::Jump || in.op == IrOp::Return) {
            reachable = false;
        }
    }
}

void Optimizer::run(IRProgram& prog) {
    for (auto& fn : prog.functions) {
        for (int iter = 0; iter < 16; ++iter) {
            bool changed = false;
            changed |= sparseConstProp(fn);
            changed |= constantFold(fn);
            changed |= copyPropagate(fn);
            changed |= deadCodeEliminate(fn);
            if (!changed) break;
        }
    }
}

} // namespace minic
