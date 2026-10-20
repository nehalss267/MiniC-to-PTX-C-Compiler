#ifndef MINIC_ASTDUMP_H
#define MINIC_ASTDUMP_H

#include "ast.h"
#include <string>

namespace minic {

std::string dumpAst(const Program& prog);

} // namespace minic

#endif
