#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>

#include "lexer.h"
#include "parser.h"
#include "sema.h"
#include "irbuilder.h"
#include "opt.h"
#include "ptxgen.h"
#include "astdump.h"
#include "irdump.h"
#include "diag.h"

using namespace minic;

static std::string readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw CompileError("cannot open input file '" + path + "'");
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

static void usage() {
    std::cerr <<
        "minic: compile MiniC to PTX\n"
        "\n"
        "usage: minic [options] <file.minic>\n"
        "\n"
        "options:\n"
        "  --emit-tokens   print the lexer token stream\n"
        "  --emit-ast      print the abstract syntax tree\n"
        "  --emit-ir       print the unoptimized IR\n"
        "  --emit-ir-opt   print the optimized IR\n"
        "  --emit-ptx      print generated PTX (default action)\n"
        "  --opt=0|1       disable/enable optimization passes (default 1)\n"
        "  --validate      run ptxas on the output if available\n"
        "  -o <file>       write PTX to <file> instead of stdout\n"
        "  -h, --help      show this help\n";
}

static bool runPtxas(const std::string& ptx) {
    const char* tmp = std::getenv("TEMP");
    std::string tmpPath = std::string(tmp ? tmp : ".") + "/minic_validate.ptx";
    std::ofstream out(tmpPath, std::ios::binary);
    if (!out) {
        std::cerr << "validate: cannot write temp file\n";
        return false;
    }
    out << ptx;
    out.close();

    std::string cmd = "ptxas -arch=sm_75 -o ";
    cmd += "\"" + tmpPath + ".cubin\" \"" + tmpPath + "\"";
    int rc = std::system(cmd.c_str());
    if (rc == 0) {
        std::cout << "validate: ptxas accepted the module\n";
        std::remove((tmpPath + ".cubin").c_str());
        std::remove(tmpPath.c_str());
        return true;
    }
    std::cerr << "validate: ptxas rejected the module (or ptxas not found)\n";
    std::remove(tmpPath.c_str());
    return false;
}

int main(int argc, char** argv) {
    std::string input;
    std::string output;
    bool emitTokens = false, emitAst = false, emitIr = false, emitIrOpt = false;
    bool emitPtx = false, validate = false;
    bool optimize = true;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-h" || a == "--help") { usage(); return 0; }
        else if (a == "--emit-tokens") emitTokens = true;
        else if (a == "--emit-ast") emitAst = true;
        else if (a == "--emit-ir") emitIr = true;
        else if (a == "--emit-ir-opt") emitIrOpt = true;
        else if (a == "--emit-ptx") emitPtx = true;
        else if (a == "--validate") validate = true;
        else if (a == "--opt=0") optimize = false;
        else if (a == "--opt=1") optimize = true;
        else if (a == "-o") {
            if (i + 1 >= argc) { std::cerr << "error: -o requires a filename\n"; return 2; }
            output = argv[++i];
        } else if (!a.empty() && a[0] == '-') {
            std::cerr << "error: unknown option '" << a << "'\n";
            return 2;
        } else {
            input = a;
        }
    }

    if (input.empty()) { usage(); return 2; }

    // Default action: emit PTX.
    if (!emitTokens && !emitAst && !emitIr && !emitIrOpt) emitPtx = true;

    try {
        std::string src = readFile(input);

        Lexer lex(src);
        std::vector<Token> toks = lex.tokenize();

        if (emitTokens) {
            for (const auto& t : toks) {
                if (t.kind == Tok::End) break;
                std::cout << t.line << ":" << t.col << "  "
                          << tokName(t.kind);
                if (!t.text.empty() && t.kind != Tok::Num)
                    std::cout << "  \"" << t.text << "\"";
                if (t.kind == Tok::Num)
                    std::cout << "  value=" << t.value;
                std::cout << "\n";
            }
            if (!emitAst && !emitIr && !emitIrOpt && !emitPtx) return 0;
        }

        Parser parser(toks);
        std::unique_ptr<Program> prog = parser.parseProgram();

        Sema sema;
        sema.analyze(*prog);

        if (emitAst) {
            std::cout << dumpAst(*prog);
            if (!emitIr && !emitIrOpt && !emitPtx) return 0;
        }

        IRBuilder builder;
        std::unique_ptr<IRProgram> ir = builder.build(*prog);

        if (emitIr) {
            std::cout << dumpIR(*ir);
            if (!emitIrOpt && !emitPtx) return 0;
        }

        if (optimize) {
            Optimizer opt;
            opt.run(*ir);
        }

        if (emitIrOpt) {
            std::cout << dumpIR(*ir);
            if (!emitPtx) return 0;
        }

        PtxGen gen;
        std::string ptx = gen.generate(*ir);

        if (!output.empty()) {
            std::ofstream of(output, std::ios::binary);
            if (!of) { std::cerr << "error: cannot write '" << output << "'\n"; return 1; }
            of << ptx;
            std::cout << "wrote " << output << "\n";
        } else if (emitPtx) {
            std::cout << ptx;
        }

        if (validate) {
            if (!runPtxas(ptx)) return 1;
        }
        return 0;
    } catch (const CompileError& e) {
        std::cerr << input << ": " << e.what() << "\n";
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "internal error: " << e.what() << "\n";
        return 1;
    }
}
