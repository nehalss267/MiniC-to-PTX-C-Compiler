#include "symtab.h"

namespace minic {

void SymbolTable::enterScope() {
    scopes_.push_back({});
}

void SymbolTable::exitScope() {
    if (scopes_.size() > 1) scopes_.pop_back();
}

bool SymbolTable::declare(const Symbol& sym) {
    auto& top = scopes_.back();
    if (top.find(sym.name) != top.end()) return false;
    top[sym.name] = sym;
    return true;
}

const Symbol* SymbolTable::lookup(const std::string& name) const {
    for (size_t i = scopes_.size(); i-- > 0;) {
        auto it = scopes_[i].find(name);
        if (it != scopes_[i].end()) return &it->second;
    }
    return nullptr;
}

const Symbol* SymbolTable::lookupCurrent(const std::string& name) const {
    const auto& top = scopes_.back();
    auto it = top.find(name);
    return it == top.end() ? nullptr : &it->second;
}

} // namespace minic
