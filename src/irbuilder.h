#ifndef MINIC_IRBUILDER_H
#define MINIC_IRBUILDER_H

#include "ast.h"
#include "ir.h"
#include <map>

namespace minic {

// Lowers a type-checked AST into three-address-code IR.
// Because Sema has already validated names, the builder only needs to map
// variable names to slots and manage temporaries and labels.
class IRBuilder {
public:
    std::unique_ptr<IRProgram> build(const Program& prog);

private:
    IRFunction* fn_ = nullptr;
    std::map<std::string, int> varIndex_;
    int labelCounter_ = 0;

    Operand newTemp();
    Operand varOperand(const std::string& name);
    std::string newLabel(const std::string& hint);

    void buildFunction(const Function& f);
    void buildStmt(const Stmt& s);
    void buildBlock(const BlockStmt& b);

    // Returns the operand holding the value of an expression.
    Operand buildExpr(const Expr& e);

    void emit(const Instr& in);
};

} // namespace minic

#endif
