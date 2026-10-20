#ifndef MINIC_DIAG_H
#define MINIC_DIAG_H

#include <string>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include "token.h"

namespace minic {

class CompileError : public std::runtime_error {
public:
    explicit CompileError(const std::string& msg) : std::runtime_error(msg) {}
};

inline void errorAt(int line, int col, const std::string& msg) {
    std::ostringstream os;
    os << "error:" << line << ":" << col << ": " << msg;
    throw CompileError(os.str());
}

} // namespace minic

#endif
