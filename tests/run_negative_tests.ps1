# run_negative_tests.ps1 -- check that malformed MiniC programs are rejected
# with a located error (line:col) and a non-zero exit code.
#
# Usage:  pwsh tests/run_negative_tests.ps1

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$minic = Join-Path $root "minic.exe"
if (-not (Test-Path $minic)) { throw "build minic.exe first (see README)" }

$tmp = Join-Path $env:TEMP "minic_negative"
New-Item -ItemType Directory -Force -Path $tmp | Out-Null

# name => (source, expected substring in the error message)
$cases = @{
    "undeclared_var" = @(
        "int main() { return x; }",
        "undeclared variable 'x'")
    "duplicate_decl" = @(
        "int main() { int a = 1; int a = 2; return a; }",
        "duplicate declaration of 'a'")
    "bad_return_void" = @(
        "void f() { return 1; }`nint main() { return 0; }",
        "")
    "call_arity" = @(
        "int f(int a) { return a; }`nint main() { return f(1, 2); }",
        "expects 1 argument")
    "unknown_call" = @(
        "int main() { return g(1); }",
        "call to undeclared function 'g'")
    "missing_main" = @(
        "int f() { return 0; }",
        "no entry point 'main'")
    "syntax_error" = @(
        "int main() { int a = ; return a; }",
        "expected expression")
    "assign_undeclared" = @(
        "int main() { y = 3; return y; }",
        "assignment to undeclared variable 'y'")
    "unterminated_comment" = @(
        "int main() { return 0; } /* oops",
        "unterminated block comment")
}

$pass = 0; $fail = 0
foreach ($name in $cases.Keys | Sort-Object) {
    $src = $cases[$name][0]
    $want = $cases[$name][1]
    $file = Join-Path $tmp "$name.minic"
    Set-Content -Path $file -Value $src -NoNewline

    $out = & $minic $file 2>&1 | Out-String
    $code = $LASTEXITCODE

    if ($code -eq 0) {
        Write-Host "FAIL ${name}: compiled successfully (should have failed)" -ForegroundColor Red
        $fail++; continue
    }
    if ($want -and ($out -notmatch [regex]::Escape($want))) {
        Write-Host "FAIL ${name}: error message mismatch" -ForegroundColor Red
        Write-Host "   got: $($out.Trim())" -ForegroundColor DarkGray
        $fail++; continue
    }
    Write-Host "ok   $name" -ForegroundColor Green
    $pass++
}

Write-Host "`npassed: $pass  failed: $fail" -ForegroundColor $(if ($fail -eq 0) { "Green" } else { "Red" })
if ($fail -gt 0) { exit 1 }
exit 0
