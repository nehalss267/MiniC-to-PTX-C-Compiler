#include "irdump.h"
#include <sstream>

namespace minic {

const char* irOpName(IrOp op) {
    switch (op) {
        case IrOp::Assign: return "assign";
        case IrOp::Binary: return "binary";
        case IrOp::Unary: return "unary";
        case IrOp::Label: return "label";
        case IrOp::Jump: return "jump";
        case IrOp::JumpIfZero: return "jump_if_zero";
        case IrOp::Call: return "call";
        case IrOp::Return: return "return";
        case IrOp::Nop: return "nop";
    }
    return "?";
}

std::string operandToString(const Operand& o) {
    switch (o.kind) {
        case OpKind::Const: return std::to_string(o.value);
        case OpKind::Temp: return "%t" + std::to_string(o.index);
        case OpKind::Var: return "%v" + std::to_string(o.index) + "(" + o.name + ")";
    }
    return "?";
}

static const char* sym(Tok k) {
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

std::string dumpIR(const IRProgram& prog) {
    std::ostringstream os;
    for (const auto& fn : prog.functions) {
        os << "func " << fn.name << "(";
        for (size_t i = 0; i < fn.params.size(); ++i) {
            if (i) os << ", ";
            os << fn.params[i];
        }
        os << ")" << (fn.isVoid ? " -> void" : " -> int") << " {\n";
        for (const auto& in : fn.code) {
            if (in.dead) continue;
            os << "  ";
            switch (in.op) {
                case IrOp::Assign:
                    os << operandToString(in.dst) << " = " << operandToString(in.lhs);
                    break;
                case IrOp::Binary:
                    os << operandToString(in.dst) << " = " << operandToString(in.lhs)
                       << " " << sym(in.binOp) << " " << operandToString(in.rhs);
                    break;
                case IrOp::Unary:
                    os << operandToString(in.dst) << " = " << sym(in.binOp)
                       << operandToString(in.lhs);
                    break;
                case IrOp::Label:
                    os << in.label << ":";
                    break;
                case IrOp::Jump:
                    os << "goto " << in.label;
                    break;
                case IrOp::JumpIfZero:
                    os << "if " << operandToString(in.lhs) << " == 0 goto " << in.label;
                    break;
                case IrOp::Call:
                    os << operandToString(in.dst) << " = call " << in.callee << "(";
                    for (size_t i = 0; i < in.args.size(); ++i) {
                        if (i) os << ", ";
                        os << operandToString(in.args[i]);
                    }
                    os << ")";
                    break;
                case IrOp::Return:
                    os << "return";
                    if (!fn.isVoid) os << " " << operandToString(in.lhs);
                    break;
                case IrOp::Nop:
                    os << "nop";
                    break;
            }
            os << "\n";
        }
        os << "}\n\n";
    }
    return os.str();
}

} // namespace minic
