#ifndef MINIC_IR_H
#define MINIC_IR_H

#include <string>
#include <vector>
#include <cstdint>
#include "token.h"

namespace minic {

// ---- Operands ----
enum class OpKind { Const, Temp, Var };

struct Operand {
    OpKind kind = OpKind::Const;
    long value = 0;      // Const
    int index = 0;       // Temp index or Var index
    std::string name;    // for readability / Var

    static Operand constant(long v) { Operand o; o.kind = OpKind::Const; o.value = v; return o; }
    static Operand temp(int i)      { Operand o; o.kind = OpKind::Temp;  o.index = i; return o; }
    static Operand var(int i, const std::string& n) { Operand o; o.kind = OpKind::Var; o.index = i; o.name = n; return o; }
};

// ---- Instructions (three-address code) ----
enum class IrOp {
    Assign,     // dst = src (copy of operand)
    Binary,     // dst = lhs <op> rhs
    Unary,      // dst = <op> src
    Label,      // target:
    Jump,       // goto label
    JumpIfZero, // if src == 0 goto label
    Call,       // dst = call callee(args...)
    Return,     // return src (src ignored if void)
    Nop
};

struct Instr {
    IrOp op = IrOp::Nop;
    Operand dst;
    Operand lhs;
    Operand rhs;
    Tok binOp = Tok::Plus;   // for Binary/Unary
    std::string label;       // for Label/Jump/JumpIfZero
    std::string callee;      // for Call
    std::vector<Operand> args; // for Call
    int line = 0;
    // optimization bookkeeping
    bool dead = false;
};

struct IRFunction {
    std::string name;
    bool isVoid = false;
    std::vector<std::string> params; // variable names
    int tempCount = 0;
    int varCount = 0;
    std::vector<std::string> varNames; // index -> name
    std::vector<Instr> code;
};

struct IRProgram {
    std::vector<IRFunction> functions;
};

const char* irOpName(IrOp op);
std::string operandToString(const Operand& o);

} // namespace minic

#endif
