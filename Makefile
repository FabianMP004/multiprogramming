# =============================================================================
# Makefile — BeagleBone Black Multiprogramming OS  (Fase 1 + Fase 2)
#
# Builds tres imágenes independientes:
#   os/os.bin  — kernel, linked at 0x82000000
#   p1/p1.bin  — Task A (USR mode, SYS_YIELD), linked at 0x82100000
#   p2/p2.bin  — Task B (USR mode, SYS_YIELD), linked at 0x82200000
#
# Cross-compiler: arm-none-eabi-  (adjust CROSS if yours differs)
# Install on Ubuntu/Debian:
#   sudo apt-get install gcc-arm-linux-gnueabihf binutils-arm-linux-gnueabihf
# =============================================================================

CROSS   ?= arm-none-eabi-
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

# P1/P2: -I os for user_syscalls.h; allow stdint.h (no -nostdinc)
USER_CFLAGS = -mcpu=cortex-a8 -marm \
            -mfloat-abi=soft \
            -ffreestanding -nostdlib \
            -Wall -Wextra -O1 -g -I os

# Assembler flags
ASFLAGS  = -mcpu=cortex-a8

# Linker flags
LDFLAGS  = -nostdlib --no-undefined

# =============================================================================
# OS image  (Dev A + Dev B — no tocar)
# =============================================================================
OS_SRCS  = os/root.s os/os.c os/scheduler.c
OS_OBJS  = os/root.o os/os.o os/scheduler.o
OS_LD    = os/os.ld
OS_ELF   = os/os.elf
OS_BIN   = os/os.bin
OS_LST   = os/os.lst
OS_MAP   = os/os.map

# =============================================================================
# P1 image — Task A en USR mode (Fase 2)
# Usa syscalls directamente (sin lib/stdio)
# =============================================================================
P1_SRCS  = p1/main.c
P1_OBJS  = p1/main.o
P1_LD    = p1/p1.ld
P1_ELF   = p1/p1.elf
P1_BIN   = p1/p1.bin

# =============================================================================
# P2 image — Task B en USR mode (Fase 2)
# Usa syscalls directamente (sin lib/stdio)
# =============================================================================
P2_SRCS  = p2/main.c
P2_OBJS  = p2/main.o
P2_LD    = p2/p2.ld
P2_ELF   = p2/p2.elf
P2_BIN   = p2/p2.bin

.PHONY: all clean os p1 p2 verify disasm uboot

all: os p1 p2
	@echo ""
	@echo "=== Build complete ==="
	@$(SIZE) $(OS_ELF) $(P1_ELF) $(P2_ELF)

# =============================================================================
# OS build rules  (sin cambios respecto a Fase 1)
# =============================================================================
os: $(OS_BIN)

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

# =============================================================================
# P1 build rules  (Fase 2: solo main.c, sin lib/)
# =============================================================================
p1: $(P1_BIN)

p1/main.o: p1/main.c os/user_syscalls.h
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(P1_ELF): $(P1_OBJS) $(P1_LD)
	$(LD) $(LDFLAGS) -T $(P1_LD) -o $@ $(P1_OBJS)

$(P1_BIN): $(P1_ELF)
	$(OBJCOPY) -O binary $< $@
	@echo "[p1] Binary: $@ ($$(wc -c < $@) bytes)"

# =============================================================================
# P2 build rules  (Fase 2: solo main.c, sin lib/)
# =============================================================================
p2: $(P2_BIN)

p2/main.o: p2/main.c
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(P2_ELF): $(P2_OBJS) $(P2_LD)
	$(LD) $(LDFLAGS) -T $(P2_LD) -o $@ $(P2_OBJS)

$(P2_BIN): $(P2_ELF)
	$(OBJCOPY) -O binary $< $@
	@echo "[p2] Binary: $@ ($$(wc -c < $@) bytes)"

# =============================================================================
# Verification target
# =============================================================================
verify: all
	@echo ""
	@echo "=== Verifying memory map ==="
	@$(NM) --numeric-sort $(OS_ELF) | grep -E "(_start|__bss|__os_stack|main)"
	@echo "--- os entry point (should be 0x82000000) ---"
	@$(NM) $(OS_ELF) | grep "_start"
	@echo "--- os stack top (should be 0x82012000) ---"
	@$(NM) $(OS_ELF) | grep "__os_stack_top"
	@echo "--- p1 entry (should be 0x82100000) ---"
	@$(NM) $(P1_ELF) | grep " main"
	@echo "--- p2 entry (should be 0x82200000) ---"
	@$(NM) $(P2_ELF) | grep " main"
	@echo ""
	@echo "=== Checking .bss symbols exist ==="
	@$(NM) $(OS_ELF) | grep "__bss_start__" && echo "OK: __bss_start__" || echo "MISSING: __bss_start__"
	@$(NM) $(OS_ELF) | grep "__bss_end__"   && echo "OK: __bss_end__"   || echo "MISSING: __bss_end__"
	@echo ""
	@echo "=== Checking saved_regs global (used by root.s) ==="
	@$(NM) $(OS_ELF) | grep "saved_regs"   && echo "OK" || echo "MISSING: saved_regs"
	@$(NM) $(OS_ELF) | grep "saved_lr"     && echo "OK" || echo "MISSING: saved_lr"
	@$(NM) $(OS_ELF) | grep "saved_svc_sp" && echo "OK" || echo "MISSING: saved_svc_sp"
	@echo ""
	@echo "=== IRQ vector points to irq_handler ==="
	@$(OBJDUMP) -d $(OS_ELF) | grep -A2 "82000018" || true
	@echo ""
	@echo "=== All checks done ==="

# =============================================================================
# Disassembly
# =============================================================================
disasm: all
	$(OBJDUMP) -d $(OS_ELF) > os/os_full.lst
	$(OBJDUMP) -d $(P1_ELF) > p1/p1_full.lst
	$(OBJDUMP) -d $(P2_ELF) > p2/p2_full.lst
	@echo "Disassembly written to os/os_full.lst, p1/p1_full.lst, p2/p2_full.lst"

# =============================================================================
# U-Boot load commands
# =============================================================================
uboot:
	@echo ""
	@echo "=== U-Boot load commands ==="
	@echo ""
	@echo "--- Cargar tres binarios separados ---"
	@echo "  loady 0x82000000   # enviar os/os.bin por Ymodem"
	@echo "  loady 0x82100000   # enviar p1/p1.bin por Ymodem"
	@echo "  loady 0x82200000   # enviar p2/p2.bin por Ymodem"
	@echo "  go    0x82000000"
	@echo ""
	@echo "NOTA: los TRES loady son obligatorios."
	@echo "Si falta p1.bin o p2.bin en memoria -> Data Abort."
	@echo ""

# =============================================================================
# Clean
# =============================================================================
clean:
	rm -f $(OS_OBJS) $(OS_ELF) $(OS_BIN) $(OS_LST) $(OS_MAP)
	rm -f $(P1_OBJS) $(P1_ELF) $(P1_BIN)
	rm -f $(P2_OBJS) $(P2_ELF) $(P2_BIN)
	rm -f lib/stdio.o lib/string.o
	rm -f os/os_full.lst p1/p1_full.lst p2/p2_full.lst
	@echo "Clean done."