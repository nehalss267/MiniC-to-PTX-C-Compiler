#ifndef MINIC_TOKEN_H
#define MINIC_TOKEN_H

#include <string>
#include <cstdint>

namespace minic {

enum class Tok {
    End,
    Ident,
    Num,
    // keywords
    KwInt, KwVoid, KwIf, KwElse, KwWhile, KwReturn,
    // punctuation
    LParen, RParen, LBrace, RBrace,
    Semi, Comma, Assign,
    Plus, Minus, Star, Slash, Percent,
    Eq, Ne, Lt, Le, Gt, Ge,
    AndAnd, OrOr, Not,
};

struct Token {
    Tok kind;
    std::string text;
    int line;
    int col;
    long value; // for Num

    Token() : kind(Tok::End), line(0), col(0), value(0) {}
};

const char* tokName(Tok k);

} // namespace minic

#endif
