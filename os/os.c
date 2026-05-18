/* =============================================================================
 * os.c — Operating System: Hardware Initialization
 * ============================================================================= */

#include "os.h"
#include "scheduler.h"

#define WDT1_BASE       0x44E35000U
#define UART0_BASE      0x44E09000U
#define DMTIMER2_BASE   0x48040000U
#define INTC_BASE       0x48200000U

#define WDT_WSPR        0x48
#define WDT_WWPS        0x34
#define WDT_WWPS_W_PEND_WSPR  (1 << 4)

#define UART_THR        0x00
#define UART_LSR        0x14
#define UART_LSR_THRE   (1 << 5)

#define TIMER_TISR      0x28
#define TIMER_TIER      0x2C
#define TIMER_TCLR      0x38
#define TIMER_TCRR      0x3C
#define TIMER_TLDR      0x40

#define TIMER_TIER_OVF_EN   (1 << 1)
#define TIMER_TISR_OVF      (1 << 1)
#define TIMER_TCLR_ST       (1 << 0)
#define TIMER_TCLR_AR       (1 << 1)

#define TIMER_FREQ_HZ       24000000U
#define TIMER_LOAD_1S       (0xFFFFFFFFU - TIMER_FREQ_HZ + 1U)
#define TIMER_LOAD_FAST     0xFF000000U
#define TIMER_LOAD_VALUE    TIMER_LOAD_1S

#define INTC_SYSCONFIG      0x10
#define INTC_SYSSTATUS      0x14
#define INTC_CONTROL        0x48
#define INTC_MIR_CLEAR2     0xC8

#define DMTIMER2_IRQ_BIT    (1U << 4)

#define REG(base, offset)   (*((volatile unsigned int *)((base) + (offset))))

extern unsigned int __os_stack_top[];

unsigned int saved_regs[13];
unsigned int saved_lr;
unsigned int saved_svc_sp;
unsigned int saved_svc_lr;

static void cpu_irq_enable(void);
static void init_exception_stacks(void);

void uart_putc(char c)
{
    while (!(REG(UART0_BASE, UART_LSR) & UART_LSR_THRE))
        ;
    REG(UART0_BASE, UART_THR) = (unsigned int)c;
}

void uart_puts(const char *s)
{
    unsigned int cpsr_save;
    __asm__ volatile (
        "mrs %0, cpsr        \n"
        "orr r1, %0, #0x80   \n"
        "msr cpsr_c, r1      \n"
        : "=r"(cpsr_save)
        :
        : "r1", "memory"
    );

    while (*s)
        uart_putc(*s++);

    __asm__ volatile (
        "msr cpsr_c, %0\n"
        :
        : "r"(cpsr_save)
        : "memory"
    );
}

void uart_put_uint(unsigned int n)
{
    char tmp[10];
    int i = 0;

    if (n == 0) {
        uart_putc('0');
        return;
    }
    while (n > 0) {
        tmp[i++] = (char)('0' + (n % 10U));
        n /= 10U;
    }
    while (i > 0)
        uart_putc(tmp[--i]);
}

/* Una sola uart_puts por traza — menos stack en SVC/IRQ */
static char *append_str(char *p, const char *s)
{
    while (*s)
        *p++ = *s++;
    return p;
}

static char *append_uint(char *p, unsigned int n)
{
    char tmp[10];
    int i = 0;
    int j;

    if (n == 0) {
        *p++ = '0';
        return p;
    }
    while (n > 0) {
        tmp[i++] = (char)('0' + (n % 10U));
        n /= 10U;
    }
    for (j = i - 1; j >= 0; j--)
        *p++ = tmp[j];
    return p;
}

static void trace_emit(const char *head, unsigned int pid, const char *tail)
{
    static char line[80];
    char *p = line;

    p = append_str(p, head);
    p = append_uint(p, pid);
    p = append_str(p, tail);
    *p = '\0';
    uart_puts(line);
}

void trace_initial_launch(void)
{
    trace_emit("MODE_SWITCH KERNEL_TO_USER pid=", g_current_pid,
               " reason=initial_launch\r\n");
}

void trace_user_to_kernel_irq(void)
{
    trace_emit("MODE_SWITCH USER_TO_KERNEL pid=", g_current_pid,
               " reason=timer_irq\r\n");
}

void trace_kernel_to_user_dispatch(void)
{
    trace_emit("MODE_SWITCH KERNEL_TO_USER pid=", g_current_pid,
               " reason=dispatch\r\n");
}

void trace_user_to_kernel_svc(void)
{
    trace_emit("MODE_SWITCH USER_TO_KERNEL pid=", g_current_pid,
               " reason=syscall id=0\r\n");
}

void trace_kernel_to_user_svc(void)
{
    trace_emit("MODE_SWITCH KERNEL_TO_USER pid=", g_current_pid,
               " reason=syscall_return id=0 rc=0\r\n");
}

static void wdt_disable(void)
{
    REG(WDT1_BASE, WDT_WSPR) = 0xAAAAU;
    while (REG(WDT1_BASE, WDT_WWPS) & WDT_WWPS_W_PEND_WSPR)
        ;
    REG(WDT1_BASE, WDT_WSPR) = 0x5555U;
    while (REG(WDT1_BASE, WDT_WWPS) & WDT_WWPS_W_PEND_WSPR)
        ;
}

static void timer_init(void)
{
    REG(DMTIMER2_BASE, TIMER_TCLR) = 0;
    REG(DMTIMER2_BASE, TIMER_TLDR) = TIMER_LOAD_VALUE;
    REG(DMTIMER2_BASE, TIMER_TCRR) = TIMER_LOAD_VALUE;
    REG(DMTIMER2_BASE, TIMER_TISR) = TIMER_TISR_OVF;
    REG(DMTIMER2_BASE, TIMER_TIER) = TIMER_TIER_OVF_EN;
    REG(DMTIMER2_BASE, TIMER_TCLR) = TIMER_TCLR_AR | TIMER_TCLR_ST;
}

static void intc_init(void)
{
    REG(INTC_BASE, INTC_SYSCONFIG) = 0x2;
    while (!(REG(INTC_BASE, INTC_SYSSTATUS) & 0x1))
        ;
    REG(INTC_BASE, INTC_MIR_CLEAR2) = DMTIMER2_IRQ_BIT;
}

static void init_exception_stacks(void)
{
    __asm__ volatile (
        "mrs  r1, cpsr           \n"  /* save original CPSR */
        
        /* Initialize SP_svc */
        "mrs  r0, cpsr           \n"
        "bic  r0, r0, #0x1F      \n"
        "orr  r0, r0, #0x13      \n"  /* MODE_SVC = 0x13 */
        "msr  cpsr_c, r0         \n"
        "ldr  sp, =__os_stack_top\n"  /* SP_svc = __os_stack_top */
        
        /* Initialize SP_irq */
        "mrs  r0, cpsr           \n"
        "bic  r0, r0, #0x1F      \n"
        "orr  r0, r0, #0x12      \n"  /* MODE_IRQ = 0x12 */
        "msr  cpsr_c, r0         \n"
        "ldr  sp, =__os_stack_top\n"  /* SP_irq = __os_stack_top */
        
        /* Initialize SP_abt (Data/Prefetch Abort) */
        "mrs  r0, cpsr           \n"
        "bic  r0, r0, #0x1F      \n"
        "orr  r0, r0, #0x17      \n"  /* MODE_ABT = 0x17 */
        "msr  cpsr_c, r0         \n"
        "ldr  sp, =__os_stack_top\n"  /* SP_abt = __os_stack_top */
        
        /* Initialize SP_und (Undefined Instruction) */
        "mrs  r0, cpsr           \n"
        "bic  r0, r0, #0x1F      \n"
        "orr  r0, r0, #0x1B      \n"  /* MODE_UND = 0x1B */
        "msr  cpsr_c, r0         \n"
        "ldr  sp, =__os_stack_top\n"  /* SP_und = __os_stack_top */
        
        /* Initialize SP_fiq (FIQ) */
        "mrs  r0, cpsr           \n"
        "bic  r0, r0, #0x1F      \n"
        "orr  r0, r0, #0x11      \n"  /* MODE_FIQ = 0x11 */
        "msr  cpsr_c, r0         \n"
        "ldr  sp, =__os_stack_top\n"  /* SP_fiq = __os_stack_top */
        
        /* Return to original CPSR (SYS mode) */
        "msr  cpsr_c, r1         \n"
        ::: "r0", "r1", "memory"
    );
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

void timer_irq_handler_body(void)
{
    REG(DMTIMER2_BASE, TIMER_TISR) = TIMER_TISR_OVF;
    REG(INTC_BASE, INTC_CONTROL) = 0x1;

    trace_user_to_kernel_irq();

    scheduler_tick();

    trace_kernel_to_user_dispatch();
}

__attribute__((naked))
void timer_irq_handler(void)
{
    __asm__ volatile (
        "mrs  r1, cpsr           \n"
        "mrs  r0, cpsr           \n"
        "bic  r0, r0, #0x1F      \n"
        "orr  r0, r0, #0x12      \n"
        "msr  cpsr_c, r0         \n"
        "ldr  sp, =__os_stack_top\n"
        "msr  cpsr_c, r1         \n"
        "b    timer_irq_handler_body\n"
    );
}

int main(void)
{
    wdt_disable();
    init_exception_stacks();

    uart_puts("\r\n[OS] Multiprogramming OS - Fase 2 (Execution Modes)\r\n");

    timer_init();
    uart_puts("[OS] DMTimer2 configurado\r\n");

    intc_init();
    uart_puts("[OS] INTC configurado, IRQ 68 activo\r\n");

    scheduler_init();
    uart_puts("[OS] Scheduler inicializado (PCBs en USR mode)\r\n");

    scheduler_start_first();

    __asm__ volatile ("dsb" ::: "memory");
    __asm__ volatile ("isb");

    cpu_irq_enable();
    uart_puts("[OS] IRQ habilitado\r\n");

    uart_puts("[OS] Lanzando primer proceso en USR mode...\r\n");
    trace_initial_launch();

    volatile unsigned int __d = 0;
    for (__d = 0; __d < 20000000U; ++__d) __asm__ volatile ("nop");

    launch_first_task();

    return 0;
}

/* busy-wait delay for approximately 1 second */
void delay_1s(void)
{
    volatile unsigned int i = 250000000U;  /* ~250M iterations ≈ 1 sec @ 1 GHz */
    while (i--);
}
