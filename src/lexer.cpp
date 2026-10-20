#include "lexer.h"
#include "diag.h"
#include <cctype>
#include <cstdlib>

namespace minic {

const char* tokName(Tok k) {
    switch (k) {
        case Tok::End: return "end of file";
        case Tok::Ident: return "identifier";
        case Tok::Num: return "number";
        case Tok::KwInt: return "'int'"; case Tok::KwVoid: return "'void'";
        case Tok::KwIf: return "'if'"; case Tok::KwElse: return "'else'";
        case Tok::KwWhile: return "'while'"; case Tok::KwReturn: return "'return'";
        case Tok::LParen: return "'('"; case Tok::RParen: return "')'";
        case Tok::LBrace: return "'{'"; case Tok::RBrace: return "'}'";
        case Tok::Semi: return "';'"; case Tok::Comma: return "','";
        case Tok::Assign: return "'='";
        case Tok::Plus: return "'+'"; case Tok::Minus: return "'-'";
        case Tok::Star: return "'*'"; case Tok::Slash: return "'/'";
        case Tok::Percent: return "'%'";
        case Tok::Eq: return "'=='"; case Tok::Ne: return "'!='";
        case Tok::Lt: return "'<'"; case Tok::Le: return "'<='";
        case Tok::Gt: return "'>'"; case Tok::Ge: return "'>='";
        case Tok::AndAnd: return "'&&'"; case Tok::OrOr: return "'||'";
        case Tok::Not: return "'!'";
    }
    return "?";
}

char Lexer::peek(int ahead) const {
    size_t p = pos_ + (size_t)ahead;
    return p < src_.size() ? src_[p] : '\0';
}

char Lexer::advance() {
    char c = src_[pos_++];
    if (c == '\n') { line_++; col_ = 1; } else { col_++; }
    return c;
}

bool Lexer::atEnd() const { return pos_ >= src_.size(); }

Token Lexer::make(Tok kind, const std::string& text, int line, int col) {
    Token t;
    t.kind = kind; t.text = text; t.line = line; t.col = col;
    return t;
}

void Lexer::skipWhitespaceAndComments() {
    for (;;) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();
        } else if (c == '/' && peek(1) == '/') {
            while (!atEnd() && peek() != '\n') advance();
        } else if (c == '/' && peek(1) == '*') {
            int sl = line_, sc = col_;
            advance(); advance();
            while (!atEnd() && !(peek() == '*' && peek(1) == '/')) advance();
            if (atEnd()) errorAt(sl, sc, "unterminated block comment");
            advance(); advance();
        } else {
            break;
        }
    }
}

Token Lexer::lexNumber() {
    int line = line_, col = col_;
    std::string text;
    while (std::isdigit((unsigned char)peek())) text += advance();
    Token t = make(Tok::Num, text, line, col);
    if (!text.empty() && text.size() > 2 && text[0] == '0' &&
        (text[1] == 'x' || text[1] == 'X')) {
        t.value = std::strtol(text.c_str(), nullptr, 16);
    } else if (!text.empty() && text.size() > 1 && text[0] == '0') {
        t.value = std::strtol(text.c_str(), nullptr, 8);
    } else {
        t.value = std::strtol(text.c_str(), nullptr, 10);
    }
    return t;
}

Token Lexer::lexIdentOrKeyword() {
    int line = line_, col = col_;
    std::string text;
    while (std::isalnum((unsigned char)peek()) || peek() == '_') text += advance();
    if (text == "int") return make(Tok::KwInt, text, line, col);
    if (text == "void") return make(Tok::KwVoid, text, line, col);
    if (text == "if") return make(Tok::KwIf, text, line, col);
    if (text == "else") return make(Tok::KwElse, text, line, col);
    if (text == "while") return make(Tok::KwWhile, text, line, col);
    if (text == "return") return make(Tok::KwReturn, text, line, col);
    return make(Tok::Ident, text, line, col);
}

Token Lexer::lexSymbol() {
    int line = line_, col = col_;
    char c = advance();
    switch (c) {
        case '(': return make(Tok::LParen, "(", line, col);
        case ')': return make(Tok::RParen, ")", line, col);
        case '{': return make(Tok::LBrace, "{", line, col);
        case '}': return make(Tok::RBrace, "}", line, col);
        case ';': return make(Tok::Semi, ";", line, col);
        case ',': return make(Tok::Comma, ",", line, col);
        case '+': return make(Tok::Plus, "+", line, col);
        case '-': return make(Tok::Minus, "-", line, col);
        case '*': return make(Tok::Star, "*", line, col);
        case '%': return make(Tok::Percent, "%", line, col);
        case '=':
            if (peek() == '=') { advance(); return make(Tok::Eq, "==", line, col); }
            return make(Tok::Assign, "=", line, col);
        case '!':
            if (peek() == '=') { advance(); return make(Tok::Ne, "!=", line, col); }
            return make(Tok::Not, "!", line, col);
        case '<':
            if (peek() == '=') { advance(); return make(Tok::Le, "<=", line, col); }
            return make(Tok::Lt, "<", line, col);
        case '>':
            if (peek() == '=') { advance(); return make(Tok::Ge, ">=", line, col); }
            return make(Tok::Gt, ">", line, col);
        case '/': return make(Tok::Slash, "/", line, col);
        case '&':
            if (peek() == '&') { advance(); return make(Tok::AndAnd, "&&", line, col); }
            break;
        case '|':
            if (peek() == '|') { advance(); return make(Tok::OrOr, "||", line, col); }
            break;
        default: break;
    }
    errorAt(line, col, std::string("unexpected character '") + c + "'");
    return make(Tok::End, "", line, col);
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> out;
    for (;;) {
        skipWhitespaceAndComments();
        if (atEnd()) break;
        char c = peek();
        if (std::isdigit((unsigned char)c)) out.push_back(lexNumber());
        else if (std::isalpha((unsigned char)c) || c == '_') out.push_back(lexIdentOrKeyword());
        else out.push_back(lexSymbol());
    }
    out.push_back(make(Tok::End, "", line_, col_));
    return out;
}

} // namespace minic
