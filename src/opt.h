#ifndef MINIC_OPT_H
#define MINIC_OPT_H

#include "ir.h"

namespace minic {

// Intra-function optimization passes. Each returns true if it changed the IR.
class Optimizer {
public:
    // Runs fold -> propagate -> DCE repeatedly until a fixed point.
    void run(IRProgram& prog);

    bool constantFold(IRFunction& fn);
    bool copyPropagate(IRFunction& fn);
    bool deadCodeEliminate(IRFunction& fn);
    bool sparseConstProp(IRFunction& fn);

private:
    void removeUnreachable(IRFunction& fn);
};

} // namespace minic

#endif
