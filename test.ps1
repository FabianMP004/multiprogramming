<#
.SYNOPSIS
  PowerShell test script to verify P1/P2 are linked at expected base addresses.

.DESCRIPTION
  - Builds P1 and P2 (runs `make p1 p2`)
  - Verifies that symbol `main` resides at:
      P1 -> 0x82100000
      P2 -> 0x82200000
  - Optionally inspects the .text VMA using the cross readelf if available

.NOTES
  - Uses CROSS environment variable prefix (default: arm-linux-gnueabihf-)
  - Run from the repository root (where the Makefile lives)
  - Example:
      $env:CROSS = 'arm-linux-gnueabihf-'
      .\test.ps1
#>

#region Settings
$CROSS = if ($env:CROSS) { $env:CROSS } else { 'arm-linux-gnueabihf-' }
$NM = "${CROSS}nm"
$READELF = "${CROSS}readelf"

$ExpectedP1 = '0x82100000'
$ExpectedP2 = '0x82200000'
#endregion

function Write-Ok { Write-Host "  [OK]   $args" -ForegroundColor Green }
function Write-Fail { Write-Host "  [FAIL] $args" -ForegroundColor Red }
function Write-Info { Write-Host ""; Write-Host "--- $args ---" -ForegroundColor Cyan }

function Command-Exists($cmdName) {
    try {
        $cmd = Get-Command -ErrorAction Stop $cmdName
        return $true
    } catch {
        return $false
    }
}

function Get-SymAddr([string]$elfPath, [string]$symbol) {
    if (-not (Command-Exists $NM)) { return $null }
    try {
        $lines = & $NM --numeric-sort $elfPath 2>$null
    } catch {
        return $null
    }
    foreach ($ln in $lines) {
        # nm output like: 82100000 T main
        if ($ln -match "^\s*([0-9A-Fa-f]+)\s+\w+\s+$symbol$") {
            $hex = $matches[1]
            return ("0x{0}" -f $hex)
        }
    }
    return $null
}

function Get-SectionVMA([string]$elfPath, [string]$section = '.text') {
    if (-not (Command-Exists $READELF)) { return $null }
    try {
        $out = & $READELF -S $elfPath 2>$null
    } catch {
        return $null
    }
    foreach ($ln in $out) {
        if ($ln -match "\b$section\b") {
            # Try to extract a hex address like 0x82100000 or an 8-hex number
            if ($ln -match "0x[0-9A-Fa-f]+") { return $matches[0] }
            if ($ln -match "\b([0-9A-Fa-f]{8,16})\b") { return ("0x{0}" -f $matches[1]) }
            # else continue searching
        }
    }
    return $null
}

function Run-MakeQuiet([string[]]$targets) {
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = 'make'
    $psi.Arguments = ($targets -join ' ')
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.UseShellExecute = $false
    $proc = [System.Diagnostics.Process]::Start($psi)
    $stdOut = $proc.StandardOutput.ReadToEnd()
    $stdErr = $proc.StandardError.ReadToEnd()
    $proc.WaitForExit()
    return @{ ExitCode = $proc.ExitCode; StdOut = $stdOut; StdErr = $stdErr }
}

# Begin tests
Write-Info "1. Check cross toolchain"
if (Command-Exists ("${CROSS}gcc")) {
    Write-Ok "${CROSS}gcc found"
} else {
    Write-Fail "${CROSS}gcc NOT found in PATH. Set CROSS or install the cross toolchain."
    exit 1
}

Write-Info "2. Build P1 and P2"
$result = Run-MakeQuiet -targets @('p1','p2')
if ($result.ExitCode -eq 0) {
    Write-Ok "make p1 p2 succeeded"
} else {
    Write-Fail "make p1 p2 failed (exit $($result.ExitCode))"
    Write-Host $result.StdOut
    Write-Host $result.StdErr
    exit 1
}

Write-Info "3. Check ELF presence"
$p1elf = Join-Path -Path 'p1' -ChildPath 'p1.elf'
$p2elf = Join-Path -Path 'p2' -ChildPath 'p2.elf'

if (Test-Path $p1elf) { Write-Ok "$p1elf present" } else { Write-Fail "$p1elf missing"; exit 1 }
if (Test-Path $p2elf) { Write-Ok "$p2elf present" } else { Write-Fail "$p2elf missing"; exit 1 }

Write-Info "4. Inspect 'main' symbol addresses"
$p1Main = Get-SymAddr $p1elf 'main'
$p2Main = Get-SymAddr $p2elf 'main'

if ($p1Main -and $p1Main -eq $ExpectedP1) {
    Write-Ok "P1 main = $p1Main (expected)"
} else {
    Write-Fail "P1 main = ${p1Main:-MISSING} (expected $ExpectedP1)"
}

if ($p2Main -and $p2Main -eq $ExpectedP2) {
    Write-Ok "P2 main = $p2Main (expected)"
} else {
    Write-Fail "P2 main = ${p2Main:-MISSING} (expected $ExpectedP2)"
}

Write-Info "5. .text VMA check (if readelf present)"
$p1Text = Get-SectionVMA $p1elf '.text'
$p2Text = Get-SectionVMA $p2elf '.text'

if ($p1Text) {
    Write-Host "  P1 .text VMA: $p1Text"
    if ($p1Text -eq $ExpectedP1) { Write-Ok "P1 .text VMA matches expected" } else { Write-Fail "P1 .text VMA mismatch (expected $ExpectedP1)" }
} else {
    Write-Host "  [SKIP] readelf not available or .text not found for p1"
}

if ($p2Text) {
    Write-Host "  P2 .text VMA: $p2Text"
    if ($p2Text -eq $ExpectedP2) { Write-Ok "P2 .text VMA matches expected" } else { Write-Fail "P2 .text VMA mismatch (expected $ExpectedP2)" }
} else {
    Write-Host "  [SKIP] readelf not available or .text not found for p2"
}

# Summary: determine failure by comparing values
$failCount = 0
if ($p1Main -ne $ExpectedP1) { $failCount++ }
if ($p2Main -ne $ExpectedP2) { $failCount++ }
if ($p1Text -and ($p1Text -ne $ExpectedP1)) { $failCount++ }
if ($p2Text -and ($p2Text -ne $ExpectedP2)) { $failCount++ }

Write-Host ""
if ($failCount -eq 0) {
    Write-Host "=== All checks passed ===" -ForegroundColor Green
    exit 0
} else {
    Write-Host "=== Some checks FAILED (count: $failCount) ===" -ForegroundColor Red
    exit 1
}
