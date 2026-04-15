# =============================================================================
# Makefile — BeagleBone Black Multiprogramming OS
#
# Builds three independent images:
#   os/os.bin  — linked at 0x82000000
#   p1/p1.bin  — linked at 0x82100000
#   p2/p2.bin  — linked at 0x82200000
#   os/os.bin  — linked at 0x82000000
#   p1/p1.bin  — linked at 0x82100000
#   p2/p2.bin  — linked at 0x82200000
#
# Cross-compiler: arm-linux-gnueabihf-  (adjust CROSS if yours differs)
# Install on Ubuntu/Debian:
#   sudo apt-get install gcc-arm-linux-gnueabihf binutils-arm-linux-gnueabihf
# =============================================================================

CROSS   ?= arm-linux-gnueabihf-
CC       = $(CROSS)gcc
AS       = $(CROSS)as
LD       = $(CROSS)ld
OBJCOPY  = $(CROSS)objcopy
OBJDUMP  = $(CROSS)objdump
NM       = $(CROSS)nm
SIZE     = $(CROSS)size

# Compiler flags
CFLAGS   = -mcpu=cortex-a8 -marm \
            -mfloat-abi=soft \
            -ffreestanding -nostdlib -nostdinc \
            -Wall -Wextra -O1 -g

# Assembler flags
ASFLAGS  = -mcpu=cortex-a8

# Linker flags (no standard startup files or libraries)
LDFLAGS  = -nostdlib --no-undefined

# =============================================================================
# OS image
# =============================================================================
OS_SRCS  = os/root.s os/os.c os/scheduler.c
OS_OBJS  = os/root.o os/os.o os/scheduler.o
OS_LD    = os/os.ld
OS_ELF   = os/os.elf
OS_BIN   = os/os.bin
OS_LST   = os/os.lst
OS_MAP   = os/os.map
OS_SRCS  = os/root.s os/os.c os/scheduler.c
OS_OBJS  = os/root.o os/os.o os/scheduler.o
OS_LD    = os/os.ld
OS_ELF   = os/os.elf
OS_BIN   = os/os.bin
OS_LST   = os/os.lst
OS_MAP   = os/os.map

# =============================================================================
# P1 image — Dev C provides P1/main.c and lib/stdio.c, lib/string.c
# =============================================================================
P1_SRCS  = p1/main.c lib/stdio.c lib/string.c
P1_OBJS  = p1/main.o lib/stdio.o lib/string.o
P1_LD    = p1/p1.ld
P1_ELF   = p1/p1.elf
P1_BIN   = p1/p1.bin
P1_SRCS  = p1/main.c lib/stdio.c lib/string.c
P1_OBJS  = p1/main.o lib/stdio.o lib/string.o
P1_LD    = p1/p1.ld
P1_ELF   = p1/p1.elf
P1_BIN   = p1/p1.bin

# =============================================================================
# P2 image — Dev C provides P2/main.c
# =============================================================================
P2_SRCS  = p2/main.c lib/stdio.c lib/string.c
P2_OBJS  = p2/main.o lib/stdio.o lib/string.o
P2_LD    = p2/p2.ld
P2_ELF   = p2/p2.elf
P2_BIN   = p2/p2.bin
P2_SRCS  = p2/main.c lib/stdio.c lib/string.c
P2_OBJS  = p2/main.o lib/stdio.o lib/string.o
P2_LD    = p2/p2.ld
P2_ELF   = p2/p2.elf
P2_BIN   = p2/p2.bin

# =============================================================================
# Default target
# =============================================================================
.PHONY: all clean os p1 p2 verify disasm uboot disasm uboot

all: os p1 p2
	@echo ""
	@echo "=== Build complete ==="
	@$(SIZE) $(OS_ELF) $(P1_ELF) $(P2_ELF)

# =============================================================================
# OS build rules
# =============================================================================
os: $(OS_BIN)

os/root.o: os/root.s
os/root.o: os/root.s
	$(CC) $(CFLAGS) -c $< -o $@

os/os.o: os/os.c os/os.h os/scheduler.h
	$(CC) $(CFLAGS) -I os -c $< -o $@

os/scheduler.o: os/scheduler.c os/scheduler.h
	$(CC) $(CFLAGS) -I os -c $< -o $@

$(OS_ELF): $(OS_OBJS) $(OS_LD)
	$(LD) $(LDFLAGS) -T $(OS_LD) -Map=$(OS_MAP) -o $@ $(OS_OBJS)

$(OS_BIN): $(OS_ELF)
	$(OBJCOPY) -O binary $< $@
	$(OBJDUMP) -d $< > $(OS_LST)
	@echo "[os] Binary: $@ ($$(wc -c < $@) bytes)"
	@echo "[os] Binary: $@ ($$(wc -c < $@) bytes)"

# =============================================================================
# P1 build rules
# =============================================================================
p1: $(P1_BIN)

p1/main.o: p1/main.c os/os.h
	$(CC) $(CFLAGS) -I os -I lib -c $< -o $@
p1/main.o: p1/main.c os/os.h
	$(CC) $(CFLAGS) -I OS -I lib -c $< -o $@

lib/stdio.o: lib/stdio.c lib/stdio.h os/os.h
	$(CC) $(CFLAGS) -I os -I lib -c $< -o $@
lib/stdio.o: lib/stdio.c lib/stdio.h os/os.h
	$(CC) $(CFLAGS) -I OS -I lib -c $< -o $@

lib/string.o: lib/string.c lib/string.h
	$(CC) $(CFLAGS) -I lib -c $< -o $@

$(P1_ELF): $(P1_OBJS) $(P1_LD)
	$(LD) $(LDFLAGS) -T $(P1_LD) -o $@ $(P1_OBJS)

$(P1_BIN): $(P1_ELF)
	$(OBJCOPY) -O binary $< $@
	@echo "[p1] Binary: $@ ($$(wc -c < $@) bytes)"
	@echo "[p1] Binary: $@ ($$(wc -c < $@) bytes)"

# =============================================================================
# P2 build rules
# =============================================================================
p2: $(P2_BIN)

p2/main.o: p2/main.c os/os.h
	$(CC) $(CFLAGS) -I os -I lib -c $< -o $@
p2/main.o: p2/main.c os/os.h
	$(CC) $(CFLAGS) -I OS -I lib -c $< -o $@

$(P2_ELF): $(P2_OBJS) $(P2_LD)
	$(LD) $(LDFLAGS) -T $(P2_LD) -o $@ $(P2_OBJS)

$(P2_BIN): $(P2_ELF)
	$(OBJCOPY) -O binary $< $@
	@echo "[p2] Binary: $@ ($$(wc -c < $@) bytes)"
	@echo "[p2] Binary: $@ ($$(wc -c < $@) bytes)"

# =============================================================================
# Verification target — checks binary layout is correct
# =============================================================================
verify: all
	@echo ""
	@echo "=== Verifying memory map ==="
	@$(NM) --numeric-sort $(OS_ELF) | grep -E "(_start|__bss|__os_stack|main)"
	@echo "--- os entry point (should be 0x82000000) ---"
	@echo "--- os entry point (should be 0x82000000) ---"
	@$(NM) $(OS_ELF) | grep "_start"
	@echo "--- os stack top (should be 0x82012000) ---"
	@echo "--- os stack top (should be 0x82012000) ---"
	@$(NM) $(OS_ELF) | grep "__os_stack_top"
	@echo "--- p1 entry (should be 0x82100000) ---"
	@echo "--- p1 entry (should be 0x82100000) ---"
	@$(NM) $(P1_ELF) | grep " main"
	@echo "--- p2 entry (should be 0x82200000) ---"
	@echo "--- p2 entry (should be 0x82200000) ---"
	@$(NM) $(P2_ELF) | grep " main"
	@echo ""
	@echo "=== Checking .bss symbols exist ==="
	@$(NM) $(OS_ELF) | grep "__bss_start__" && echo "OK: __bss_start__" || echo "MISSING: __bss_start__"
	@$(NM) $(OS_ELF) | grep "__bss_end__"   && echo "OK: __bss_end__"   || echo "MISSING: __bss_end__"
	@echo ""
	@echo "=== Checking saved_regs global (used by root.s) ==="
	@$(NM) $(OS_ELF) | grep "saved_regs"    && echo "OK" || echo "MISSING: saved_regs"
	@$(NM) $(OS_ELF) | grep "saved_lr"      && echo "OK" || echo "MISSING: saved_lr"
	@$(NM) $(OS_ELF) | grep "saved_svc_sp" && echo "OK" || echo "MISSING: saved_svc_sp"
	@echo ""
	@echo "=== IRQ vector points to irq_handler ==="
	@$(OBJDUMP) -d $(OS_ELF) | grep -A2 "82000018" || true
	@echo ""
	@echo "=== All checks done ==="

# =============================================================================
# Disassembly (useful for debugging)
# =============================================================================
disasm: all
	$(OBJDUMP) -d $(OS_ELF) > os/os_full.lst
	$(OBJDUMP) -d $(P1_ELF) > p1/p1_full.lst
	$(OBJDUMP) -d $(P2_ELF) > p2/p2_full.lst
	@echo "Disassembly written to os/os_full.lst, p1/p1_full.lst, p2/p2_full.lst"
	$(OBJDUMP) -d $(OS_ELF) > os/os_full.lst
	$(OBJDUMP) -d $(P1_ELF) > p1/p1_full.lst
	$(OBJDUMP) -d $(P2_ELF) > p2/p2_full.lst
	@echo "Disassembly written to os/os_full.lst, p1/p1_full.lst, p2/p2_full.lst"

# =============================================================================
# U-Boot load commands — print to screen for convenience
# =============================================================================
uboot:
	@echo ""
	@echo "=== U-Boot load commands ==="
	@echo "Run these in the U-Boot console (serial terminal):"
	@echo ""
	@echo "  loady 0x82000000   # then send os/os.bin via Ymodem"
	@echo "  loady 0x82100000   # then send p1/p1.bin via Ymodem"
	@echo "  loady 0x82200000   # then send p2/p2.bin via Ymodem"
	@echo "  loady 0x82000000   # then send os/os.bin via Ymodem"
	@echo "  loady 0x82100000   # then send p1/p1.bin via Ymodem"
	@echo "  loady 0x82200000   # then send p2/p2.bin via Ymodem"
	@echo "  go    0x82000000   # start execution"
	@echo ""

# =============================================================================
# Clean
# =============================================================================
clean:
	rm -f $(OS_OBJS) $(OS_ELF) $(OS_BIN) $(OS_LST) $(OS_MAP)
	rm -f $(P1_OBJS) $(P1_ELF) $(P1_BIN)
	rm -f $(P2_OBJS) $(P2_ELF) $(P2_BIN)
	@echo "Clean done."