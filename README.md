# minic

A compiler for MiniC, a small integer-only C subset, targeting PTX (NVIDIA's
GPU assembly). I wrote it to work through the whole compiler pipeline end to
end and to have output I can check with a real assembler.

Pipeline: lexer -> parser -> AST -> semantic analysis -> three-address-code IR
-> optimization passes -> PTX. Each stage can be dumped from the CLI.

```
source.minic -> lexer -> parser -> sema -> irbuilder -> opt -> ptxgen -> program.ptx
```

## Build

Needs a C++14 compiler. With CMake (MinGW on Windows):

```sh
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_MAKE_PROGRAM="C:\MinGW\bin\mingw32-make.exe"
cmake --build build
```

Without CMake:

```sh
g++ -std=c++14 -Wall -Wextra -Isrc src/*.cpp -o minic.exe
```

Builds clean under `-Wall -Wextra`.

## Usage

```
minic [options] <file.minic>

  --emit-tokens   print the lexer token stream
  --emit-ast      print the abstract syntax tree
  --emit-ir       print the unoptimized IR
  --emit-ir-opt   print the optimized IR
  --emit-ptx      print generated PTX (default action)
  --opt=0|1       disable/enable optimization passes (default 1)
  --validate      run ptxas on the output if available
  -o <file>       write PTX to <file> instead of stdout
  -h, --help      show this help
```

Examples:

```sh
minic examples/fib.minic                  # PTX to stdout
minic --emit-ir-opt examples/arith.minic  # see constant folding collapse the IR
minic --validate examples/loop.minic      # check the PTX with ptxas
minic -o fib.ptx examples/fib.minic       # write PTX to a file
```

## Language

Keeps C's syntax, drops anything the backend can't lower to PTX:

```
program   := function+
function  := ('int' | 'void') IDENT '(' [ param (',' param)* ] ')' block
param     := 'int' IDENT
block     := '{' statement* '}'
statement := 'int' IDENT [ '=' expr ] ';'
           | IDENT '=' expr ';'
           | 'if' '(' expr ')' statement [ 'else' statement ]
           | 'while' '(' expr ')' statement
           | 'return' [ expr ] ';'
           | block
           | expr ';'
expr      := assignment, with the usual precedence:
             =   ||   &&   == !=   < <= > >=   + -   * / %   unary(- !)   primary
primary   := NUM | IDENT | IDENT '(' args ')' | '(' expr ')'
```

- Only 32-bit signed integers (`int` -> `.s32`).
- `void` is only usable as a return type.
- Functions take parameters and can call each other, including recursively.
- Entry point is `main` with no parameters.
- Comments: `//` and `/* */`.

## Optimizations

Four intra-function passes in `src/opt.cpp`, run to a fixed point:

- Sparse constant propagation: reads of a single-definition constant become the constant.
- Constant folding: arithmetic/comparison on literals, plus constant conditions.
- Copy propagation: forward-substitute simple copies into their uses.
- Dead-code elimination: drop writes nobody reads, unreachable code after jumps/returns, and leftover nops.

`examples/arith.minic` folds all the way down to `return 42`:

```
before                          after
%t0 = 3 * 4                     return 42
%t1 = 2 + %t0
%v0(a) = %t1
%t2 = %v0(a) * 3
...
```

## Backend

Each IR function becomes a `.visible .func`; `main` becomes a `.visible .entry`.
Non-entry functions return through a register parameter `%out` and take
arguments in register parameters `%arg0..`; calls pass arguments by value in
registers.

| IR | PTX |
|----|-----|
| `t = a + b` | `add.s32 t, a, b;` |
| `t = a - b` | `sub.s32 t, a, b;` |
| `t = a * b` | `mul.lo.s32 t, a, b;` |
| `t = a / b` | `div.s32 t, a, b;` |
| `t = a % b` | `rem.s32 t, a, b;` |
| `t = -a` | `neg.s32 t, a;` |
| `t = !a` | `setp.eq.s32 p, a, 0; selp.s32 t, 1, 0, p;` |
| `t = a < b` | `setp.lt.s32 p, a, b; selp.s32 t, 1, 0, p;` |
| `if c == 0 goto L` | `setp.eq.s32 p, c, 0; @p bra L;` |
| `t = call f(a)` | `call (t), f, (a);` |
| `return a` | `mov.s32 %out, a; ret;` |

Register allocation just gives every temp and variable its own `.s32` virtual
register (`%tN` / `%vN`) and leaves physical allocation to the driver JIT, which
is how PTX is meant to be consumed. Output targets `.version 7.0` /
`.target sm_75`.

## Testing

```sh
pwsh tests/run_tests.ps1          # compile, check IR, validate with ptxas if present
pwsh tests/run_tests.ps1 -Update  # regenerate tests/golden/*.ir snapshots
pwsh tests/run_negative_tests.ps1 # error cases
```

`run_tests.ps1` compiles every `examples/*.minic`, checks the PTX structure,
compares optimized IR against the snapshots in `tests/golden/`, and runs
`ptxas -arch=sm_75` on each module if it's on `PATH` or under the CUDA toolkit.
`run_negative_tests.ps1` checks that bad programs (undeclared names, duplicate
declarations, arity mismatches, bad returns, syntax errors) are rejected.

If you change IR lowering or a pass, expect the golden snapshots to change;
re-run with `-Update` once you've confirmed the new output is right.

## Layout

```
src/
  token.h        token kinds
  diag.h         CompileError + errorAt
  lexer.{h,cpp}  hand-written scanner
  ast.h          AST nodes
  parser.{h,cpp} recursive-descent parser
  symtab.{h,cpp} scoped symbol table
  sema.{h,cpp}   semantic analysis
  ir.h           three-address-code IR
  irbuilder.{h,cpp} AST -> IR lowering
  opt.{h,cpp}    optimization passes
  ptxgen.{h,cpp} PTX code generation
  astdump.cpp    AST printer
  irdump.cpp     IR printer
  main.cpp       CLI driver
examples/        sample programs
tests/
  run_tests.ps1  end-to-end harness
  run_negative_tests.ps1
  golden/        optimized-IR snapshots
```

## Limitations

- `int` only. No floats, pointers, arrays, structs, or memory model.
- Optimizations are intra-function only (no interprocedural analysis).
- One register per value; heavy functions emit many virtual registers (still
  valid PTX, just not pressure-optimal).
- `--validate` only proves ptxas accepts the module; nothing is run on a GPU.

## Roadmap

See [`roadmap.md`](roadmap.md) for milestones and [`plan.md`](plan.md) for the
full design.
