# run_tests.ps1 -- MiniC-to-PTX test harness
#
# For each examples/*.minic checks:
#   1. the compiler exits 0
#   2. --emit-ptx produces a module with .version/.target and a .entry/.func
#   3. optimized IR matches the golden snapshot (tests/golden/<name>.ir), if one exists
#   4. ptxas accepts the PTX, if ptxas is available
#
# Usage:  pwsh tests/run_tests.ps1 [-Update]

param(
    [switch]$Update
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$minic = Join-Path $root "minic.exe"
$examples = Join-Path $root "examples"
$golden = Join-Path $PSScriptRoot "golden"

if (-not (Test-Path $minic)) {
    Write-Host "building minic.exe ..." -ForegroundColor Cyan
    & g++ -std=c++14 -Wall -Wextra -I (Join-Path $root "src") `
        (Join-Path $root "src/*.cpp") -o $minic
    if ($LASTEXITCODE -ne 0) { throw "build failed" }
}

New-Item -ItemType Directory -Force -Path $golden | Out-Null

function Find-Ptxas {
    $cmd = Get-Command ptxas -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    $base = "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA"
    if (Test-Path $base) {
        $hit = Get-ChildItem $base -Recurse -Filter ptxas.exe -ErrorAction SilentlyContinue |
               Sort-Object FullName -Descending | Select-Object -First 1
        if ($hit) { return $hit.FullName }
    }
    return $null
}

$ptxas = Find-Ptxas
if ($ptxas) {
    Write-Host "ptxas: $ptxas" -ForegroundColor Green
} else {
    Write-Host "ptxas: not found (validation step will be skipped)" -ForegroundColor Yellow
}

$pass = 0
$fail = 0
$failures = @()

foreach ($src in Get-ChildItem $examples -Filter *.minic | Sort-Object Name) {
    $name = $src.BaseName
    Write-Host "`n=== $name ===" -ForegroundColor Cyan

    # 1. compile
    $ptx = & $minic $src.FullName 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) {
        $fail++; $failures += "${name}: compile failed`n$ptx"
        Write-Host "  FAIL compile" -ForegroundColor Red
        continue
    }
    Write-Host "  ok   compile" -ForegroundColor Green

    # 2. structural checks on PTX
    if ($ptx -notmatch '\.version' -or $ptx -notmatch '\.target') {
        $fail++; $failures += "${name}: PTX missing .version/.target"
        Write-Host "  FAIL ptx header" -ForegroundColor Red
        continue
    }
    if ($ptx -notmatch '\.(entry|func)') {
        $fail++; $failures += "${name}: PTX has no function/entry"
        Write-Host "  FAIL ptx function" -ForegroundColor Red
        continue
    }
    Write-Host "  ok   ptx structure" -ForegroundColor Green

    # 3. optimized IR + golden snapshot
    $ir = & $minic --emit-ir-opt $src.FullName 2>&1 | Out-String
    if ([string]::IsNullOrWhiteSpace($ir)) {
        $fail++; $failures += "${name}: empty optimized IR"
        Write-Host "  FAIL ir" -ForegroundColor Red
        continue
    }
    $goldenFile = Join-Path $golden "$name.ir"
    if ($Update) {
        Set-Content -Path $goldenFile -Value $ir -NoNewline
        Write-Host "  ok   golden updated" -ForegroundColor Yellow
    } elseif (Test-Path $goldenFile) {
        $expected = Get-Content -Raw $goldenFile
        if ($expected.Trim() -ne $ir.Trim()) {
            $fail++; $failures += "${name}: optimized IR differs from golden"
            Write-Host "  FAIL ir golden mismatch" -ForegroundColor Red
            continue
        }
        Write-Host "  ok   ir golden" -ForegroundColor Green
    } else {
        Write-Host "  --   ir (no golden)" -ForegroundColor DarkGray
    }

    # 4. ptxas validation
    if ($ptxas) {
        $tmp = Join-Path $env:TEMP "minic_$name.ptx"
        Set-Content -Path $tmp -Value $ptx -NoNewline
        $cubin = "$tmp.cubin"
        & $ptxas -arch=sm_75 -o $cubin $tmp 2>&1 | Out-Null
        if ($LASTEXITCODE -ne 0) {
            $fail++; $failures += "${name}: ptxas rejected the module"
            Write-Host "  FAIL ptxas" -ForegroundColor Red
            Remove-Item $tmp -ErrorAction SilentlyContinue
            continue
        }
        Remove-Item $tmp, $cubin -ErrorAction SilentlyContinue
        Write-Host "  ok   ptxas validated" -ForegroundColor Green
    }

    $pass++
}

Write-Host "`n=========================" -ForegroundColor Cyan
Write-Host "passed: $pass  failed: $fail" -ForegroundColor $(if ($fail -eq 0) { "Green" } else { "Red" })
if ($failures.Count -gt 0) {
    Write-Host "`nfailures:" -ForegroundColor Red
    $failures | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
    exit 1
}
exit 0
