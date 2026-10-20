#ifndef MINIC_PTXGEN_H
#define MINIC_PTXGEN_H

#include "ir.h"
#include <string>
#include <sstream>
#include <map>
#include <vector>

namespace minic {

// Lowers IR to a PTX module. Uses a simple linear register assignment:
// every IR temp and variable gets a dedicated .s32 virtual register (the PTX
// JIT does the real physical allocation). If a function needs more than the
// documented register budget the backend records it as a warning in the output.
class PtxGen {
public:
    std::string generate(const IRProgram& prog);

private:
    std::ostringstream out_;
    const IRProgram* prog_ = nullptr;

    void emitPreamble();
    void emitFunction(const IRFunction& fn);

    std::string regFor(const Operand& o, const IRFunction& fn);
    std::string mangle(const std::string& name);
    static const char* ptxBinOp(Tok op);
    static const char* ptxCmpOp(Tok op);
};

} // namespace minic

#endif
