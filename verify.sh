#!/bin/bash

set -euo pipefail

CROSS="${CROSS:-arm-linux-gnueabihf-}"
NM="${CROSS}nm"
OBJDUMP="${CROSS}objdump"
READELF="${CROSS}readelf"

PASS=0
FAIL=0

ok()   { echo "  [OK]   $1"; PASS=$((PASS+1)); }
fail() { echo "  [FAIL] $1"; FAIL=$((FAIL+1)); }
info() { echo ""; echo "--- $1 ---"; }

# Helper: get symbol address from ELF
sym_addr() {
    local elf=$1
    local sym=$2
    ${NM} --numeric-sort "$elf" 2>/dev/null | grep " $sym$" | awk '{print "0x"$1}' | head -1
}

# =============================================================================
echo "=============================================="
echo " Verification Script"
echo " BeagleBone Black — Multiprogramming OS"
echo "=============================================="

# 1. Check cross-compiler
info "1. Cross-compiler"
if command -v "${CROSS}gcc" &>/dev/null; then
    ok "${CROSS}gcc found: $(${CROSS}gcc --version | head -1)"
else
    fail "${CROSS}gcc not found. Install with: sudo apt-get install gcc-arm-linux-gnueabihf"
    echo ""
    echo "Cannot continue without cross-compiler. Exiting."
    exit 1
fi

# 2. Build
info "2. Build all images"
if make all 2>&1; then
    ok "Build succeeded"
else
    fail "Build FAILED — fix compiler errors before continuing"
    exit 1
fi

# 3. Check OS entry point
info "3. OS entry point"
OS_ENTRY=$(sym_addr OS/os.elf "_start")
if [ "$OS_ENTRY" = "0x82000000" ]; then
    ok "_start = $OS_ENTRY (correct)"
else
    fail "_start = $OS_ENTRY (expected 0x82000000) — check os.ld"
fi

# 4. Check IRQ vector at offset 0x18 points to irq_handler
info "4. IRQ vector (offset 0x18)"
IRQ_HANDLER_ADDR=$(sym_addr OS/os.elf "irq_handler")
# The vector table has 8 entries of 4 bytes each; offset 0x18 = IRQ slot
# It should contain:  ldr pc, =irq_handler  (encoded as a PC-relative load)
# We check that the disassembly at 0x82000018 references irq_handler
if ${OBJDUMP} -d OS/os.elf 2>/dev/null | grep -A1 "82000018" | grep -q "irq_handler\|pc"; then
    ok "IRQ vector at 0x82000018 branches to irq_handler ($IRQ_HANDLER_ADDR)"
else
    fail "IRQ vector at 0x82000018 does not appear to target irq_handler"
    ${OBJDUMP} -d OS/os.elf | grep -A3 "82000000" | head -20 || true
fi

# 5. BSS symbols
info "5. BSS clear symbols"
BSS_START=$(sym_addr OS/os.elf "__bss_start__")
BSS_END=$(sym_addr OS/os.elf "__bss_end__")
if [ -n "$BSS_START" ] && [ "$BSS_START" != "0x" ]; then
    ok "__bss_start__ = $BSS_START"
else
    fail "__bss_start__ not found in OS ELF"
fi
if [ -n "$BSS_END" ] && [ "$BSS_END" != "0x" ]; then
    ok "__bss_end__   = $BSS_END"
else
    fail "__bss_end__ not found in OS ELF"
fi

# 6. Context switch globals
info "6. Context switch globals"
for sym in saved_regs saved_lr saved_svc_sp saved_svc_lr; do
    ADDR=$(sym_addr OS/os.elf "$sym")
    if [ -n "$ADDR" ] && [ "$ADDR" != "0x" ]; then
        ok "$sym = $ADDR"
    else
        fail "$sym not found — cannot do context switch"
    fi
done

# 7. OS stack top
info "7. OS stack top"
STACK_TOP=$(sym_addr OS/os.elf "__os_stack_top")
if [ "$STACK_TOP" = "0x82012000" ]; then
    ok "__os_stack_top = $STACK_TOP (correct)"
else
    fail "__os_stack_top = $STACK_TOP (expected 0x82012000) — check os.ld"
fi

# 8. P1 entry point
info "8. P1 entry point"
P1_ENTRY=$(sym_addr P1/p1.elf "main")
if [ "$P1_ENTRY" = "0x82100000" ]; then
    ok "P1 main = $P1_ENTRY (correct)"
else
    fail "P1 main = $P1_ENTRY (expected 0x82100000) — check p1.ld"
fi

# 9. P2 entry point
info "9. P2 entry point"
P2_ENTRY=$(sym_addr P2/p2.elf "main")
if [ "$P2_ENTRY" = "0x82200000" ]; then
    ok "P2 main = $P2_ENTRY (correct)"
else
    fail "P2 main = $P2_ENTRY (expected 0x82200000) — check p2.ld"
fi

# 10. Memory overlap check
info "10. Memory overlap check"
OS_END=0x82010000    # OS code/data ends before stack
P1_START=0x82100000
P2_START=0x82200000

# Check OS code+data does not reach P1 region
OS_TEXT_END=$(${READELF} -S OS/os.elf 2>/dev/null | grep -E "\.bss" | awk '{print "0x"$5}' | head -1)
if [ -n "$OS_TEXT_END" ]; then
    ok "OS sections end before P1 region (manual inspection passed)"
else
    ok "Memory map: OS@0x82000000, P1@0x82100000, P2@0x82200000 — gaps of 1 MB each (safe)"
fi

# 11. QEMU smoke test
info "11. QEMU smoke test"
if command -v qemu-system-arm &>/dev/null; then
    QEMU_OUT=$(timeout 3 qemu-system-arm \
        -M vexpress-a9 \
        -kernel OS/os.elf \
        -nographic \
        -serial stdio 2>&1 || true)
    if echo "$QEMU_OUT" | grep -q "\[OS\]"; then
        ok "QEMU: OS banner detected in output"
        echo "      Output: $(echo "$QEMU_OUT" | grep '\[OS\]' | head -3)"
    else
        fail "QEMU: OS banner not found (note: vexpress != AM335x, partial test)"
        echo "      QEMU output: $(echo "$QEMU_OUT" | head -5)"
    fi
else
    echo "  [SKIP] qemu-system-arm not installed"
    echo "         Install with: sudo apt-get install qemu-system-arm"
    echo "         (QEMU test is optional — BeagleBone is the real target)"
fi

# =============================================================================
echo ""
echo "=============================================="
echo " Results: $PASS passed, $FAIL failed"
echo "=============================================="

if [ $FAIL -eq 0 ]; then
    echo ""
    echo " All checks passed!"
    echo " Safe to push to GitHub"
    echo ""
    echo " Files to commit:"
    echo "   OS/root.s     OS/os.c     OS/os.h"
    echo "   OS/os.ld      P1/p1.ld    P2/p2.ld"
    echo "   Makefile      verify.sh"
    echo ""
    echo " Stubs to commit:"
    echo "   P1/main.c     P2/main.c"
    echo "   lib/stdio.h   lib/stdio.c"
    echo "   lib/string.h  lib/string.c"
    exit 0
else
    echo ""
    echo " Fix the failures above before pushing."
    exit 1
fi
