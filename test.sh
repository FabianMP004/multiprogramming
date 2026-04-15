#!/usr/bin/env bash

set -euo pipefail

CROSS="${CROSS:-arm-linux-gnueabihf-}"
NM="${CROSS}nm"
READELF="${CROSS}readelf"

EXPECTED_P1="0x82100000"
EXPECTED_P2="0x82200000"

PASS=0
FAIL=0

ok()   { printf "  [OK]   %s\n" "$1"; PASS=$((PASS+1)); }
fail() { printf "  [FAIL] %s\n" "$1"; FAIL=$((FAIL+1)); }
info() { printf "\n--- %s ---\n" "$1"; }

# Helper: query numeric symbol address (0x...)
sym_addr() {
    local elf="$1"
    local sym="$2"

    if command -v "$NM" >/dev/null 2>&1; then
        "$NM" --numeric-sort "$elf" 2>/dev/null | awk -v s="$sym" '$2==s {print "0x"$1; exit}'
    else
        printf ""
    fi
}

# Helper: get section VMA using readelf (returns VMA and section name)
section_vma() {
    local elf="$1"
    local sec="$2"

    if command -v "$READELF" >/dev/null 2>&1; then
        "$READELF" -S "$elf" 2>/dev/null \
            | awk -v s="$sec" '
                $2 == ".text" && s == ".text" { print $5; exit }
                $2 == s { print $5; exit }
              '
    else
        printf ""
    fi
}

# Start checks
info "1. Cross toolchain"
if command -v "${CROSS}gcc" >/dev/null 2>&1; then
    ok "${CROSS}gcc found"
else
    fail "${CROSS}gcc not found in PATH"
    echo "Install cross toolchain or set CROSS to point to your prefixed toolchain."
    exit 1
fi

info "2. Build P1 and P2"
# Build quietly but show error if it fails
if make p1 p2 >/dev/null 2>&1; then
    ok "make p1 p2 succeeded"
else
    echo ""
    echo "make p1 p2 failed. Re-run with verbose output: make p1 p2"
    fail "Build failed"
    exit 1
fi

info "3. ELF files presence"
if [ -f p1/p1.elf ]; then ok "p1/p1.elf present"; else fail "p1/p1.elf missing"; fi
if [ -f p2/p2.elf ]; then ok "p2/p2.elf present"; else fail "p2/p2.elf missing"; fi

# If any ELF missing, stop early
if [ ! -f p1/p1.elf ] || [ ! -f p2/p2.elf ]; then
    echo ""
    echo "Missing ELF files — build failed or paths changed."
    exit 1
fi

info "4. Symbol 'main' addresses"
P1_MAIN="$(sym_addr p1/p1.elf main || true)"
P2_MAIN="$(sym_addr p2/p2.elf main || true)"

if [ -n "$P1_MAIN" ] && [ "$P1_MAIN" = "$EXPECTED_P1" ]; then
    ok "P1 main = $P1_MAIN (expected)"
else
    fail "P1 main = ${P1_MAIN:-MISSING} (expected $EXPECTED_P1)"
fi

if [ -n "$P2_MAIN" ] && [ "$P2_MAIN" = "$EXPECTED_P2" ]; then
    ok "P2 main = $P2_MAIN (expected)"
else
    fail "P2 main = ${P2_MAIN:-MISSING} (expected $EXPECTED_P2)"
fi

info "5. .text VMA (readelf)"
P1_TEXT_VMA="$(section_vma p1/p1.elf .text || true)"
P2_TEXT_VMA="$(section_vma p2/p2.elf .text || true)"

if [ -n "$P1_TEXT_VMA" ]; then
    printf "  P1 .text VMA: %s\n" "$P1_TEXT_VMA"
    if [ "$P1_TEXT_VMA" = "$EXPECTED_P1" ]; then
        ok "P1 .text VMA matches expected"
    else
        fail "P1 .text VMA mismatch (expected $EXPECTED_P1)"
    fi
else
    echo "  [SKIP] readelf not available or .text not found for p1/p1.elf"
fi

if [ -n "$P2_TEXT_VMA" ]; then
    printf "  P2 .text VMA: %s\n" "$P2_TEXT_VMA"
    if [ "$P2_TEXT_VMA" = "$EXPECTED_P2" ]; then
        ok "P2 .text VMA matches expected"
    else
        fail "P2 .text VMA mismatch (expected $EXPECTED_P2)"
    fi
else
    echo "  [SKIP] readelf not available or .text not found for p2/p2.elf"
fi

# Summary
echo ""
echo "=== Result: $PASS passed, $FAIL failed ==="
if [ $FAIL -eq 0 ]; then
    echo "All checks passed for P1/P2 link addresses."
    exit 0
else
    echo "Some checks failed. Inspect output above and adjust linker scripts or Makefile."
    exit 1
fi
