#ifndef MINIC_LEXER_H
#define MINIC_LEXER_H

#include <vector>
#include <string>
#include "token.h"

namespace minic {

// Hand-written lexer for the MiniC language.
// Turn a source string into a flat vector of tokens terminated by Tok::End.
class Lexer {
public:
    explicit Lexer(const std::string& src) : src_(src) {}

    std::vector<Token> tokenize();

private:
    std::string src_;
    size_t pos_ = 0;
    int line_ = 1;
    int col_ = 1;

    char peek(int ahead = 0) const;
    char advance();
    bool atEnd() const;
    void skipWhitespaceAndComments();
    Token make(Tok kind, const std::string& text, int line, int col);
    Token lexNumber();
    Token lexIdentOrKeyword();
    Token lexSymbol();
};

} // namespace minic

#endif
