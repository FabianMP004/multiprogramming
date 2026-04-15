#include "os.h"
#include "scheduler.h"
#include <stdint.h>

/* ---------------------------------------------------------------------------
 * AM335x Base Addresses and register offsets
 * ------------------------------------------------------------------------- */
#define WDT1_BASE       0x44E35000U
#define UART0_BASE      0x44E09000U
#define DMTIMER2_BASE   0x48040000U
#define INTC_BASE       0x48200000U

/* Watchdog */
#define WDT_WSPR        0x48
#define WDT_WWPS        0x34
#define WDT_WWPS_W_PEND_WSPR  (1 << 4)

/* UART */
#define UART_THR        0x00
#define UART_LSR        0x14
#define UART_LSR_THRE   (1 << 5)

/* DMTimer2 */
#define TIMER_TCLR      0x38
#define TIMER_TCRR      0x3C
#define TIMER_TLDR      0x40
#define TIMER_TISR      0x28
#define TIMER_TIER      0x2C
#define TIMER_TISR_OVF  (1 << 1)
#define TIMER_TIER_OVF_EN (1 << 1)
#define TIMER_TCLR_ST   (1 << 0)
#define TIMER_TCLR_AR   (1 << 1)

/* Timer load values */
#define TIMER_FREQ_HZ       24000000U
#define TIMER_LOAD_1S       (0xFFFFFFFFU - TIMER_FREQ_HZ + 1U)   /* 1 second */
#define TIMER_LOAD_FAST     0xFFF00000U                          /* ~44 ms */
#define TIMER_LOAD_VALUE    TIMER_LOAD_FAST

/* INTC */
#define INTC_SYSCONFIG  0x10
#define INTC_SYSSTATUS  0x14
#define INTC_CONTROL    0x48
#define INTC_MIR_CLEAR2 0xC8
#define DMTIMER2_IRQ_BIT (1U << 4)   /* IRQ 68 → bit 4 in bank 2 */

/* CPU mode bits */
#define MODE_SVC    0x13
#define IRQ_BIT     0x80
#define FIQ_BIT     0x40

/* register access */
#define REG(base, offset)   (*((volatile unsigned int *)((base) + (offset))))

/* ---------------------------------------------------------------------------
 * UART helpers (blocking)
 * ------------------------------------------------------------------------- */

void uart_putc(char c)
{
    while (!(REG(UART0_BASE, UART_LSR) & UART_LSR_THRE))
        ;
    REG(UART0_BASE, UART_THR) = (unsigned int)c;
}

void uart_puts(const char *s)
{
    while (*s)
        uart_putc(*s++);
}

/* ---------------------------------------------------------------------------
 * WDT disable
 * ------------------------------------------------------------------------- */
static void wdt_disable(void)
{
    REG(WDT1_BASE, WDT_WSPR) = 0xAAAAU;
    while (REG(WDT1_BASE, WDT_WWPS) & WDT_WWPS_W_PEND_WSPR)
        ;
    REG(WDT1_BASE, WDT_WSPR) = 0x5555U;
    while (REG(WDT1_BASE, WDT_WWPS) & WDT_WWPS_W_PEND_WSPR)
        ;
}

/* ---------------------------------------------------------------------------
 * Timer init
 * ------------------------------------------------------------------------- */
static void timer_init(void)
{
    REG(DMTIMER2_BASE, TIMER_TCLR) = 0;

    REG(DMTIMER2_BASE, TIMER_TLDR) = TIMER_LOAD_VALUE;
    REG(DMTIMER2_BASE, TIMER_TCRR) = TIMER_LOAD_VALUE;

    REG(DMTIMER2_BASE, TIMER_TISR) = TIMER_TISR_OVF;
    REG(DMTIMER2_BASE, TIMER_TIER) = TIMER_TIER_OVF_EN;
    REG(DMTIMER2_BASE, TIMER_TCLR) = TIMER_TCLR_AR | TIMER_TCLR_ST;
}

/* ---------------------------------------------------------------------------
 * INTC init (unmask DMTIMER2 IRQ)
 * ------------------------------------------------------------------------- */
static void intc_init(void)
{
    REG(INTC_BASE, INTC_SYSCONFIG) = 0x2;
    while (!(REG(INTC_BASE, INTC_SYSSTATUS) & 0x1))
        ;
    REG(INTC_BASE, INTC_MIR_CLEAR2) = DMTIMER2_IRQ_BIT;
}

/* ---------------------------------------------------------------------------
 * Enable IRQ in CPSR (clear I-bit)
 * ------------------------------------------------------------------------- */
static void cpu_irq_enable(void)
{
    __asm__ volatile (
        "mrs r0, cpsr       \n"
        "bic r0, r0, #0x80  \n"
        "msr cpsr_c, r0     \n"
        ::: "r0", "memory"
    );
}

/* ---------------------------------------------------------------------------
 * timer_irq_handler — ACK then delegate to scheduler_tick()
 * Called from root.s IRQ handler (after saved_* are populated).
 * ------------------------------------------------------------------------- */
void timer_irq_handler(void)
{
    /* Acknowledge timer */
    REG(DMTIMER2_BASE, TIMER_TISR) = TIMER_TISR_OVF;

    /* End-of-Interrupt to INTC */
    REG(INTC_BASE, INTC_CONTROL) = 0x1;

    /* Delegate to scheduler which saves current and restores next into saved_* */
    scheduler_tick();
}

/* ---------------------------------------------------------------------------
 * main — OS entry point
 * ------------------------------------------------------------------------- */
int main(void)
{
    /* 1. Disable watchdog */
    wdt_disable();

    /* 2. Timer init */
    timer_init();

    /* 3. INTC init */
    intc_init();

    /* 4. Initialize scheduler and prepare first process context */
    scheduler_init();
    scheduler_start_first();

    /* 5. Memory barriers */
    __asm__ volatile ("dsb" ::: "memory");
    __asm__ volatile ("isb");

    /* 6. Enable IRQs in CPU */
    cpu_irq_enable();

    /* 7. Start first process: switch to SVC mode, set SVC SP and branch to PC */
    {
        pcb_t *p = scheduler_current();

        if (!p) {
            while (1) __asm__ volatile ("wfi");
        }

        uint32_t svc_sp = p->sp;
        uint32_t entry_pc = p->pc;
        uint32_t svc_bits = (MODE_SVC);

        __asm__ volatile (
            /* Build new CPSR with SVC mode (preserve I-bit = already enabled) */
            "mrs   r0, cpsr                \n\t"
            "bic   r1, r0, #0x1F           \n\t"
            "orr   r1, r1, %3              \n\t"
            "msr   cpsr_c, r1              \n\t"
            /* Now in SVC mode (IRQs remain as enabled). Set SVC SP and branch. */
            "mov   sp, %0                  \n\t" /* set SVC SP */
            "mov   lr, %1                  \n\t"
            "mov   pc, lr                  \n\t"
            :
            : "r"(svc_sp), "r"(entry_pc), "r"(svc_sp), "i"(svc_bits)
            : "r0", "r1", "memory"
        );
    }

    while (1) __asm__ volatile ("wfi");
    return 0;
}