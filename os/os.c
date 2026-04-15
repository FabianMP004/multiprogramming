/* =============================================================================
 * os.c — Operating System: Hardware Initialization + Context Switch (Fase 3)
 *
 * Dev A + Dev B responsibilities (Fase 3):
 *   - Hardware init (watchdog, UART, DMTimer2, INTC)
 *   - PCB definition and initialization
 *   - timer_irq_handler() con Round-Robin
 *   - Primera entrada manual a P1
 * ============================================================================= */

#include "os.h"
#include "scheduler.h"

/* ---------------------------------------------------------------------------
 * AM335x Base Addresses
 * ------------------------------------------------------------------------- */
#define WDT1_BASE       0x44E35000U
#define UART0_BASE      0x44E09000U
#define DMTIMER2_BASE   0x48040000U
#define INTC_BASE       0x48200000U

/* Watchdog registers */
#define WDT_WSPR        0x48
#define WDT_WWPS        0x34
#define WDT_WWPS_W_PEND_WSPR  (1 << 4)

/* UART registers */
#define UART_THR        0x00
#define UART_LSR        0x14
#define UART_LSR_THRE   (1 << 5)

/* DMTimer2 registers */
#define TIMER_TCLR      0x38
#define TIMER_TCRR      0x3C
#define TIMER_TLDR      0x40
#define TIMER_TISR      0x28
#define TIMER_TIER      0x2C
#define TIMER_TISR_OVF  (1 << 1)
#define TIMER_TIER_OVF_EN (1 << 1)
#define TIMER_TCLR_ST   (1 << 0)
#define TIMER_TCLR_AR   (1 << 1)

#define TIMER_LOAD_1S   (0xFFFFFFFFU - 24000000U + 1U)

/* INTC registers */
#define INTC_SYSCONFIG  0x10
#define INTC_SYSSTATUS  0x14
#define INTC_CONTROL    0x48
#define INTC_MIR_CLEAR2 0xC8
#define DMTIMER2_IRQ_BIT (1U << 4)   /* IRQ 68 → bit 4 en banco 2 */

/* Register access macro */
#define REG(base, offset)   (*((volatile unsigned int *)((base) + (offset))))

/* ---------------------------------------------------------------------------
 * UART helpers (sin cambios)
 * ------------------------------------------------------------------------- */
void uart_putc(char c)
{
    while (!(REG(UART0_BASE, UART_LSR) & UART_LSR_THRE));
    REG(UART0_BASE, UART_THR) = (unsigned int)c;
}

void uart_puts(const char *s)
{
    while (*s)
        uart_putc(*s++);
}

/* ---------------------------------------------------------------------------
 * Hardware initialization functions (sin cambios)
 * ------------------------------------------------------------------------- */
static void wdt_disable(void)
{
    REG(WDT1_BASE, WDT_WSPR) = 0xAAAAU;
    while (REG(WDT1_BASE, WDT_WWPS) & WDT_WWPS_W_PEND_WSPR);
    REG(WDT1_BASE, WDT_WSPR) = 0x5555U;
    while (REG(WDT1_BASE, WDT_WWPS) & WDT_WWPS_W_PEND_WSPR);
}

static void timer_init(void)
{
    REG(DMTIMER2_BASE, TIMER_TCLR) = 0;
    REG(DMTIMER2_BASE, TIMER_TLDR) = TIMER_LOAD_1S;
    REG(DMTIMER2_BASE, TIMER_TCRR) = TIMER_LOAD_1S;
    REG(DMTIMER2_BASE, TIMER_TISR) = TIMER_TISR_OVF;
    REG(DMTIMER2_BASE, TIMER_TIER) = TIMER_TIER_OVF_EN;
    REG(DMTIMER2_BASE, TIMER_TCLR) = TIMER_TCLR_AR | TIMER_TCLR_ST;
}

static void intc_init(void)
{
    REG(INTC_BASE, INTC_SYSCONFIG) = 0x2;
    while (!(REG(INTC_BASE, INTC_SYSSTATUS) & 0x1));
    REG(INTC_BASE, INTC_MIR_CLEAR2) = DMTIMER2_IRQ_BIT;
}

static void cpu_irq_enable(void)
{
    __asm__ volatile (
        "mrs r0, cpsr       \n"
        "bic r0, r0, #0x80  \n"
        "msr cpsr_c, r0     \n"
        ::: "r0", "memory"
    );
}

/* ====================== PCB (Fase 3 - Dev B) ====================== */
typedef struct {
    int      pid;
    uint32_t sp;      // Stack Pointer (SVC mode)
    uint32_t pc;
    uint32_t lr;
    uint32_t spsr;
    uint32_t regs[13]; // R0-R12 (no se usan en esta versión simple)
} PCB;

#define NUM_PROCS 3
PCB pcb[NUM_PROCS];
int current_pid = 0;   // 0=OS, 1=P1, 2=P2

/* Símbolos definidos por los linker scripts */
extern uint32_t p1_entry;
extern uint32_t p2_entry;
extern uint32_t __p1_stack_top[];
extern uint32_t __p2_stack_top[];

/* ====================== Inicialización de PCBs ====================== */
void init_pcbs(void)
{
    pcb[0].pid = 0;                     /* OS */

    /* P1 */
    pcb[1].pid  = 1;
    pcb[1].sp   = (uint32_t)__p1_stack_top - 56;   /* 14 palabras × 4 bytes */
    pcb[1].pc   = p1_entry;
    pcb[1].lr   = p1_entry;
    pcb[1].spsr = 0x10;                 /* User mode */

    /* P2 */
    pcb[2].pid  = 2;
    pcb[2].sp   = (uint32_t)__p2_stack_top - 56;
    pcb[2].pc   = p2_entry;
    pcb[2].lr   = p2_entry;
    pcb[2].spsr = 0x10;
}

/* ====================== Timer IRQ Handler (Fase 3) ====================== */
void timer_irq_handler(void)
{
    /* 1. Acknowledgment del timer */
    REG(DMTIMER2_BASE, TIMER_TISR) = TIMER_TISR_OVF;

    /* 2. End-of-Interrupt al INTC */
    REG(INTC_BASE, INTC_CONTROL) = 0x1;

    /* 3. Round-Robin simple */
    current_pid = (current_pid == 1) ? 2 : 1;
}

/* ---------------------------------------------------------------------------
 * main — OS entry point
 * ------------------------------------------------------------------------- */
int main(void)
{
    wdt_disable();
    uart_puts("\r\n[OS] Multiprogramming OS starting...\r\n");

    timer_init();
    uart_puts("[OS] DMTimer2 configured (1s slices)\r\n");

    intc_init();
    uart_puts("[OS] INTC configured, IRQ 68 unmasked\r\n");

    /* === FASE 3: Context Switch === */
    init_pcbs();
    uart_puts("[OS] PCBs and initial stacks initialized\r\n");

    current_pid = 1;                    /* Empezamos con P1 */

    /* Primera entrada manual a P1 (context switch inicial) */
    uart_puts("[OS] Switching to P1...\r\n");

    __asm__ volatile (
        "mov sp, %0\n\t"
        "mov lr, %1\n\t"
        "movs pc, lr\n\t"
        : : "r"(pcb[1].sp), "r"(pcb[1].pc)
    );

    /* Nunca debería llegar aquí */
    while (1) {
        __asm__ volatile ("wfi");
    }

    return 0;
}