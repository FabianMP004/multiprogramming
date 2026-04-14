/* =============================================================================
 * os.c — Operating System: Hardware Initialization (Dev A, Fase 1 + 2)
 *
 * Responsibilities:
 *   - Disable Watchdog Timer (WDT1)
 *   - Initialize UART0 for serial output
 *   - Configure DMTimer2 for 1-second periodic interrupts
 *   - Configure INTC to unmask IRQ 68 (DMTimer2 overflow)
 *   - Enable IRQ in CPU (clear I-bit in CPSR)
 *   - Provide globals shared between root.s IRQ handler and Dev B's scheduler
 *   - Provide stub timer_irq_handler() — Dev B will REPLACE the body of this
 *     function with the actual Round-Robin scheduler logic.
 *
 * Dev B interface contract:
 *   - Reads/writes:  saved_regs[13], saved_lr, saved_svc_sp, saved_svc_lr
 *   - Calls:         uart_putc() / uart_puts() for debug output
 *   - Implements:    the body of timer_irq_handler() (scheduler + PCB logic)
 * ============================================================================= */

#include "os.h"
#include "pcb.h"

/* ---------------------------------------------------------------------------
 * AM335x Base Addresses (from TRM)
 * ------------------------------------------------------------------------- */
#define WDT1_BASE       0x44E35000U   /* Watchdog Timer 1                    */
#define UART0_BASE      0x44E09000U   /* UART0 (serial console)              */
#define DMTIMER2_BASE   0x48040000U   /* DMTimer2                            */
#define INTC_BASE       0x48200000U   /* ARM Interrupt Controller            */

/* ---------------------------------------------------------------------------
 * Watchdog Timer 1 registers (offsets from WDT1_BASE)
 * ------------------------------------------------------------------------- */
#define WDT_WIDR        0x00          /* Revision                            */
#define WDT_WDSC        0x10          /* System config                       */
#define WDT_WDST        0x14          /* Status                              */
#define WDT_WSPR        0x48          /* Start/Stop register                 */
#define WDT_WWPS        0x34          /* Write posting bits                  */

#define WDT_WWPS_W_PEND_WSPR  (1 << 4)

/* ---------------------------------------------------------------------------
 * UART0 registers (offsets from UART0_BASE)
 * ------------------------------------------------------------------------- */
#define UART_THR        0x00          /* Transmit Holding Register           */
#define UART_LSR        0x14          /* Line Status Register                */
#define UART_LSR_THRE   (1 << 5)      /* Transmit Holding Register Empty bit */

/* ---------------------------------------------------------------------------
 * DMTimer2 registers (offsets from DMTIMER2_BASE)
 * ------------------------------------------------------------------------- */
#define TIMER_TIDR      0x00          /* Revision                            */
#define TIMER_TIOCP_CFG 0x10          /* Config                              */
#define TIMER_TISR      0x28          /* Interrupt Status Register           */
#define TIMER_TIER      0x2C          /* Interrupt Enable Register           */
#define TIMER_TCLR      0x38          /* Timer Control Register              */
#define TIMER_TCRR      0x3C          /* Counter Register                    */
#define TIMER_TLDR      0x40          /* Load Register                       */
#define TIMER_TTGR      0x44          /* Trigger Register                    */
#define TIMER_TWPS      0x48          /* Write Posting Bits                  */

#define TIMER_TIER_OVF_EN   (1 << 1)  /* Overflow interrupt enable           */
#define TIMER_TISR_OVF      (1 << 1)  /* Overflow status bit                 */
#define TIMER_TCLR_ST       (1 << 0)  /* Start timer                         */
#define TIMER_TCLR_AR       (1 << 1)  /* Auto-reload                         */

/*
 * DMTimer2 runs at 24 MHz (CLKSEL = 32 kHz or 24 MHz depending on board state;
 * U-Boot typically leaves it at 24 MHz).
 *
 * To get a 1-second overflow:
 *   overflow_count = 2^32 - (24,000,000 * 1) = 0xFE980000 + small correction
 *
 * Exact value used: 0xFF000000 ≈ 0.044 s   (fast, for easy testing)
 * Change to TIMER_LOAD_1S for 1-second slices in production.
 */
#define TIMER_FREQ_HZ       24000000U
#define TIMER_LOAD_1S       (0xFFFFFFFFU - TIMER_FREQ_HZ + 1U)   /* 1 second  */
#define TIMER_LOAD_FAST     0xFF000000U                           /* ~44 ms    */
#define TIMER_LOAD_VALUE    TIMER_LOAD_1S

/* ---------------------------------------------------------------------------
 * INTC registers (offsets from INTC_BASE)
 * ------------------------------------------------------------------------- */
#define INTC_SYSCONFIG      0x10
#define INTC_SYSSTATUS      0x14
#define INTC_SIR_IRQ        0x40      /* Active IRQ number                   */
#define INTC_CONTROL        0x48      /* New IRQ/FIQ agreement               */
#define INTC_MIR0           0x84      /* Mask IRQ  0-31  (1=masked)          */
#define INTC_MIR1           0xA4      /* Mask IRQ 32-63                      */
#define INTC_MIR2           0xC4      /* Mask IRQ 64-95                      */
#define INTC_MIR_CLEAR0     0x88
#define INTC_MIR_CLEAR1     0xA8
#define INTC_MIR_CLEAR2     0xC8      /* Writing 1 clears (unmasks) IRQ      */
#define INTC_ISR_CLEAR2     0xD4

/*
 * DMTimer2 overflow = IRQ 68
 * IRQ 68 is in bank 2 (IRQs 64-95), bit position = 68 - 64 = 4
 */
#define DMTIMER2_IRQ        68
#define DMTIMER2_IRQ_BIT    (1U << (DMTIMER2_IRQ - 64))  /* bit 4 in MIR2   */

/* ---------------------------------------------------------------------------
 * Register access macro
 * ------------------------------------------------------------------------- */
#define REG(base, offset)   (*((volatile unsigned int *)((base) + (offset))))

/* ---------------------------------------------------------------------------
 * Globals shared with root.s IRQ handler and Dev B scheduler
 *
 * root.s reads/writes these directly by symbol name.
 * Dev B's PCB save/restore logic also reads/writes these.
 * All are declared in os.h so everyone can include them.
 * ------------------------------------------------------------------------- */
unsigned int saved_regs[13];   /* R0-R12 of interrupted process              */
unsigned int saved_lr;         /* LR_irq from IRQ handler                    */
unsigned int saved_svc_sp;     /* SVC mode SP of interrupted process         */
unsigned int saved_svc_lr;     /* SVC mode LR (R14_svc) of interrupted proc  */

/* ---------------------------------------------------------------------------
 * uart_putc — blocking single-character transmit on UART0
 * Polls THRE (Transmit Holding Register Empty) before writing.
 * ------------------------------------------------------------------------- */
void uart_putc(char c)
{
    /* Wait until Transmit Holding Register is empty */
    while (!(REG(UART0_BASE, UART_LSR) & UART_LSR_THRE))
        ;
    REG(UART0_BASE, UART_THR) = (unsigned int)c;
}

/* ---------------------------------------------------------------------------
 * uart_puts — null-terminated string output
 * ------------------------------------------------------------------------- */
void uart_puts(const char *s)
{
    while (*s)
        uart_putc(*s++);
}

/* ---------------------------------------------------------------------------
 * wdt_disable — disable Watchdog Timer 1
 *
 * AM335x TRM sequence:
 *   1. Write 0xAAAA to WDT_WSPR
 *   2. Wait until W_PEND_WSPR clears in WDT_WWPS
 *   3. Write 0x5555 to WDT_WSPR
 *   4. Wait until W_PEND_WSPR clears in WDT_WWPS
 *
 * Without this the board resets after ~60 seconds.
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
 * timer_init — configure DMTimer2 for periodic overflow interrupts
 *
 * Steps:
 *   1. Stop timer (clear ST bit)
 *   2. Set load register (TLDR) = TIMER_LOAD_VALUE
 *   3. Trigger immediate reload (write any value to TTGR)
 *   4. Clear any pending overflow interrupt (TISR)
 *   5. Enable overflow interrupt (TIER)
 *   6. Start timer with auto-reload (TCLR = AR | ST)
 * ------------------------------------------------------------------------- */
static void timer_init(void)
{
    /* 1. Stop timer */
    REG(DMTIMER2_BASE, TIMER_TCLR) = 0;

    /* 2. Set load value for desired period */
    REG(DMTIMER2_BASE, TIMER_TLDR) = TIMER_LOAD_VALUE;

    /* 3. Force immediate reload into counter */
    REG(DMTIMER2_BASE, TIMER_TCRR) = TIMER_LOAD_VALUE;

    /* 4. Clear pending overflow status */
    REG(DMTIMER2_BASE, TIMER_TISR) = TIMER_TISR_OVF;

    /* 5. Enable overflow interrupt */
    REG(DMTIMER2_BASE, TIMER_TIER) = TIMER_TIER_OVF_EN;

    /* 6. Start with auto-reload */
    REG(DMTIMER2_BASE, TIMER_TCLR) = TIMER_TCLR_AR | TIMER_TCLR_ST;
}

/* ---------------------------------------------------------------------------
 * intc_init — initialize INTC and unmask DMTimer2 IRQ (IRQ 68)
 *
 * Steps:
 *   1. Software reset INTC (SYSCONFIG bit 1)
 *   2. Wait for reset to complete (SYSSTATUS bit 0)
 *   3. Unmask IRQ 68 via MIR_CLEAR2 (writing 1 to a bit clears/unmasks it)
 * ------------------------------------------------------------------------- */
static void intc_init(void)
{
    /* 1. Software reset */
    REG(INTC_BASE, INTC_SYSCONFIG) = 0x2;

    /* 2. Wait for reset done */
    while (!(REG(INTC_BASE, INTC_SYSSTATUS) & 0x1))
        ;

    /* 3. Unmask IRQ 68 (DMTimer2 overflow) — write 1 to bit 4 of MIR_CLEAR2 */
    REG(INTC_BASE, INTC_MIR_CLEAR2) = DMTIMER2_IRQ_BIT;
}

/* ---------------------------------------------------------------------------
 * cpu_irq_enable — clear the I-bit in CPSR so the CPU accepts IRQs
 * ------------------------------------------------------------------------- */
static void cpu_irq_enable(void)
{
    __asm__ volatile (
        "mrs r0, cpsr       \n"
        "bic r0, r0, #0x80  \n"   /* clear I-bit */
        "msr cpsr_c, r0     \n"
        ::: "r0", "memory"
    );
}

/* ---------------------------------------------------------------------------
 * timer_irq_handler — called from root.s IRQ handler (Fase 2 stub)
 *
 * DEV A provides:
 *   - Acknowledge the timer interrupt (clear TISR overflow bit)
 *   - Send End-of-Interrupt to INTC
 *   - A minimal stub that just returns (so it compiles and runs)
 *
 * DEV B will REPLACE the body below (between the ACK and return) with:
 *   - Save current process context into its PCB (using saved_regs, etc.)
 *   - Select next process (Round-Robin)
 *   - Load next process context from its PCB into saved_regs, saved_lr, etc.
 *
 * CONTRACT: This function must always ack the timer and INTC.
 *           Dev B adds scheduler logic AFTER the acks.
 * ------------------------------------------------------------------------- */
void timer_irq_handler(void)
{
    REG(DMTIMER2_BASE, TIMER_TISR) = TIMER_TISR_OVF;
    REG(INTC_BASE, INTC_CONTROL) = 0x1;
}

/* ---------------------------------------------------------------------------
 * main — OS entry point (called from reset_handler in root.s)
 * ------------------------------------------------------------------------- */
int main(void)
{
    /* 1. Disable watchdog FIRST — prevents board reset after ~60s */
    wdt_disable();

    /* 2. UART is initialized by U-Boot bootloader.
     *    We rely on that and just use the correct base address.
     *    Send a startup banner to confirm we are running. */
    uart_puts("\r\n[OS] Multiprogramming OS starting...\r\n");

    /* 3. Configure DMTimer2 for periodic interrupts */
    timer_init();
    uart_puts("[OS] DMTimer2 configured\r\n");

    /* 4. Configure INTC — unmask IRQ 68 */
    intc_init();
    uart_puts("[OS] INTC configured, IRQ 68 unmasked\r\n");

    /* 5. Memory barrier — ensure all HW writes are committed
     *    before enabling interrupts */
    __asm__ volatile ("dsb" ::: "memory");
    __asm__ volatile ("isb");

    /* 6. Enable IRQ in CPU */
    cpu_irq_enable();
    uart_puts("[OS] IRQ enabled\r\n");

    scheduler_init();

    while (1) {
        __asm__ volatile ("wfi");
    }

    return 0;
}