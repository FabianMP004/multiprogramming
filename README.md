# Multiprogramming OS — BeagleBone Black (AM335x)

Bare-metal multiprogramming system: OS + 2 user processes with Round-Robin scheduling.

## Repo structure

```
multiprogramming/
├── OS/
│   ├── root.s      ← Dev A: boot, vector table, IRQ handler (ASM)
│   ├── os.c        ← Dev A: HW init (WDT, UART, DMTimer2, INTC) + IRQ stub
│   ├── os.h        ← Dev A: shared header (globals, types, memory map)
│   └── os.ld       ← Dev A: OS linker script (0x82000000)
├── P1/
│   ├── main.c      ← Dev C: digits 0-9 loop
│   └── p1.ld       ← Dev A: P1 linker script (0x82100000)
├── P2/
│   ├── main.c      ← Dev C: letters a-z loop
│   └── p2.ld       ← Dev A: P2 linker script (0x82200000)
├── lib/
│   ├── stdio.h/c   ← Dev B: PRINT implementation
│   └── string.h/c  ← Dev B: string utilities
├── Makefile        ← Dev A
├── verify.sh       ← Dev A: automated build + symbol check
└── README.md
```

## Memory map

| Region       | Start        | Size  | Description                    |
|--------------|--------------|-------|--------------------------------|
| OS code/data | `0x82000000` | 64 KB | Vector table, code, BSS        |
| OS stack     | `0x82010000` | 8 KB  | Grows down from `0x82012000`   |
| P1 code/data | `0x82100000` | 64 KB | Process 1 code + data          |
| P1 stack     | `0x82110000` | 8 KB  | Grows down from `0x82112000`   |
| P2 code/data | `0x82200000` | 64 KB | Process 2 code + data          |
| P2 stack     | `0x82210000` | 8 KB  | Grows down from `0x82212000`   |

## Dev A — Context switch interface (for Dev B)

`os.h` exports these globals that the IRQ handler (`root.s`) saves/restores:

```c
unsigned int saved_regs[13];  // R0-R12 of interrupted process
unsigned int saved_lr;         // LR_irq  (= interrupted PC + 4)
unsigned int saved_svc_sp;     // SVC SP of interrupted process
unsigned int saved_svc_lr;     // SVC LR (R14_svc) of interrupted process
```

**Dev B must:**
1. Implement the body of `timer_irq_handler()` in `os.c` (replace the stub).
2. Read the above globals to save the current PCB.
3. Write the above globals to load the next PCB before returning.

The IRQ return sequence in `root.s` then automatically restores the next process.

---

## Prerequisites

```bash
# Ubuntu / Debian
sudo apt-get install gcc-arm-linux-gnueabihf binutils-arm-linux-gnueabihf

# Optional: QEMU for smoke testing
sudo apt-get install qemu-system-arm
```

## Build

```bash
make all       # builds OS, P1, P2
make verify    # build + check symbols and memory map
make disasm    # generate disassembly listings
make uboot     # print U-Boot load commands
make clean
```

## Verify (Dev A checklist — run before pushing)

```bash
chmod +x verify.sh
./verify.sh
```

Expected output: `Results: 11 passed, 0 failed`

The script checks (without needing a BeagleBone):
- Cross-compiler installed
- All 3 images compile and link
- `_start` at exactly `0x82000000`
- IRQ vector at `0x82000018` targets `irq_handler`
- `__bss_start__` / `__bss_end__` symbols present
- `saved_regs`, `saved_lr`, `saved_svc_sp`, `saved_svc_lr` exported
- OS stack top at `0x82012000`
- P1 entry at `0x82100000`
- P2 entry at `0x82200000`
- No memory region overlaps

## Load and run (U-Boot on BeagleBone Black)

Connect via serial (115200 baud), interrupt U-Boot, then:

```
# Load OS image
loady 0x82000000
# [send OS/os.bin via Ymodem in your terminal: Ctrl+A S in minicom]

# Load P1 image
loady 0x82100000
# [send P1/p1.bin]

# Load P2 image
loady 0x82200000
# [send P2/p2.bin]

# Start execution
go 0x82000000
```

Expected serial output (after Dev B + C integrate):
```
[OS] Multiprogramming OS starting...
[OS] DMTimer2 configured
[OS] INTC configured, IRQ 68 unmasked
[OS] IRQ enabled
[OS] Spinning — waiting for timer interrupts...
[OS] tick          ← appears once per second (Dev A stub)
[OS] tick
----From P1: 0     ← after Dev B+C integrate
----From P2: a
----From P1: 1
----From P2: b
...
```

## Timer configuration

In `os.c`, change `TIMER_LOAD_VALUE` to adjust the time slice:

```c
#define TIMER_LOAD_1S    (0xFFFFFFFFU - 24000000U + 1U)  // 1 second
#define TIMER_LOAD_FAST  0xFF000000U                       // ~44 ms (testing)
#define TIMER_LOAD_VALUE TIMER_LOAD_1S                     // change here
```
