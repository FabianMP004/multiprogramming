/* =============================================================================
 * os.c — Operating System: Hardware Initialization
 *
 * FASE 2: cambios marcados con [FASE 2]
 *   1. Nuevo global: saved_spsr
 *   2. main() llama scheduler_init() antes de habilitar IRQs
 *   3. main() llama launch_first_task() en lugar del while(1)
 *   4. timer_irq_handler() llama scheduler_tick() (igual que Fase 1)
 *   5. svc_dispatch() declarada aquí como weak stub — Dev B la reemplaza
 * ============================================================================= */
 
#include "os.h"
 
/* ---------------------------------------------------------------------------
 * AM335x Base Addresses
 * ------------------------------------------------------------------------- */
#define WDT1_BASE       0x44E35000U
#define UART0_BASE      0x44E09000U
#define DMTIMER2_BASE   0x48040000U
#define INTC_BASE       0x48200000U
 
/* ---------------------------------------------------------------------------
 * Watchdog Timer 1 registers
 * ------------------------------------------------------------------------- */
#define WDT_WSPR        0x48
#define WDT_WWPS        0x34
#define WDT_WWPS_W_PEND_WSPR  (1 << 4)
 
/* ---------------------------------------------------------------------------
 * UART0 registers
 * ------------------------------------------------------------------------- */
#define UART_THR        0x00
#define UART_LSR        0x14
#define UART_LSR_THRE   (1 << 5)
 
/* ---------------------------------------------------------------------------
 * DMTimer2 registers
 * ------------------------------------------------------------------------- */
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
 
/* ---------------------------------------------------------------------------
 * INTC registers
 * ------------------------------------------------------------------------- */
#define INTC_SYSCONFIG      0x10
#define INTC_SYSSTATUS      0x14
#define INTC_CONTROL        0x48
#define INTC_MIR_CLEAR2     0xC8
 
#define DMTIMER2_IRQ_BIT    (1U << 4)   /* IRQ 68 = bit 4 de banco 2 */
 
/* ---------------------------------------------------------------------------
 * Macro de acceso a registros
 * ------------------------------------------------------------------------- */
#define REG(base, offset)   (*((volatile unsigned int *)((base) + (offset))))
 
/* ---------------------------------------------------------------------------
 * Globals del trap frame — compartidos con root.s y Dev B
 *
 * [FASE 2] Agregado: saved_spsr
 * ------------------------------------------------------------------------- */
unsigned int saved_regs[13];   /* R0-R12 del proceso interrumpido            */
unsigned int saved_lr;         /* LR_irq / LR_svc (return address)           */
unsigned int saved_svc_sp;     /* SP_usr del proceso (vía SYS mode)          */
unsigned int saved_svc_lr;     /* LR_usr del proceso (vía SYS mode)          */
unsigned int saved_spsr;       /* [FASE 2] CPSR del proceso (USR=0x10)       */
 
/* ---------------------------------------------------------------------------
 * uart_putc — transmisión bloqueante de un caracter por UART0
 * ------------------------------------------------------------------------- */
void uart_putc(char c)
{
    while (!(REG(UART0_BASE, UART_LSR) & UART_LSR_THRE))
        ;
    REG(UART0_BASE, UART_THR) = (unsigned int)c;
}
 
/* ---------------------------------------------------------------------------
 * uart_puts — imprime string null-terminated
 * ------------------------------------------------------------------------- */
void uart_puts(const char *s)
{
    while (*s)
        uart_putc(*s++);
}
 
/* ---------------------------------------------------------------------------
 * wdt_disable — deshabilitar Watchdog Timer 1
 * Secuencia exacta del TRM AM335x: 0xAAAA → esperar → 0x5555 → esperar
 * Sin esto la board se resetea a los ~60 segundos.
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
 * timer_init — configurar DMTimer2 para interrupciones periódicas
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
 * intc_init — reset INTC y desenmascarar IRQ 68 (DMTimer2 overflow)
 * ------------------------------------------------------------------------- */
static void intc_init(void)
{
    REG(INTC_BASE, INTC_SYSCONFIG) = 0x2;
    while (!(REG(INTC_BASE, INTC_SYSSTATUS) & 0x1))
        ;
    REG(INTC_BASE, INTC_MIR_CLEAR2) = DMTIMER2_IRQ_BIT;
}
 
/* ---------------------------------------------------------------------------
 * cpu_irq_enable — limpiar I-bit en CPSR para aceptar IRQs
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
 * timer_irq_handler — llamado desde irq_handler en root.s
 *
 * Ackea el timer + INTC y llama scheduler_tick() (Dev B).
 * scheduler_tick() actualiza saved_regs/saved_lr/saved_svc_sp/
 * saved_svc_lr/saved_spsr con el contexto del siguiente proceso.
 * ------------------------------------------------------------------------- */
void timer_irq_handler(void)
{
    /* 1. Ack DMTimer2 overflow */
    REG(DMTIMER2_BASE, TIMER_TISR) = TIMER_TISR_OVF;
 
    /* 2. End-of-Interrupt al INTC */
    REG(INTC_BASE, INTC_CONTROL) = 0x1;
 
    /* 3. Context switch Round-Robin — implementado por Dev B */
    extern void scheduler_tick(void);
    scheduler_tick();
}
 
/* ---------------------------------------------------------------------------
 * svc_dispatch — weak stub, Dev B reemplaza con la implementación real
 *
 * Cuando root.s llama a esta función:
 *   - saved_regs[0] = syscall ID (0 = SYS_YIELD)
 *   - saved_spsr    = CPSR del proceso llamante (0x10 para USR)
 *
 * Dev B debe:
 *   - Leer saved_regs[0] para saber el ID
 *   - Ejecutar la syscall (SYS_YIELD: scheduler RR)
 *   - Actualizar todos los saved_* con el siguiente proceso
 *   - Escribir saved_regs[0] = rc (0 para éxito)
 * ------------------------------------------------------------------------- */
__attribute__((weak)) void svc_dispatch(void)
{
    /* Stub de Dev A: no hace nada — Dev B lo reemplaza */
    uart_puts("[OS] svc_dispatch stub — Dev B debe implementar\r\n");
}
 
/* ---------------------------------------------------------------------------
 * main — punto de entrada C del OS (llamado desde reset_handler en root.s)
 *
 * FASE 2: cambios señalados con [FASE 2]
 * ------------------------------------------------------------------------- */
int main(void)
{
    /* 1. Deshabilitar watchdog — PRIMERO, antes de todo */
    wdt_disable();
 
    /* 2. Banner de arranque por UART */
    uart_puts("\r\n[OS] Multiprogramming OS - Fase 2 (Execution Modes)\r\n");
 
    /* 3. Configurar DMTimer2 */
    timer_init();
    uart_puts("[OS] DMTimer2 configurado\r\n");
 
    /* 4. Configurar INTC — desenmascarar IRQ 68 */
    intc_init();
    uart_puts("[OS] INTC configurado, IRQ 68 activo\r\n");
 
    /* 5. [FASE 2] Inicializar PCBs con spsr=0x10 (USR mode)
     *    Dev B implementa scheduler_init() en scheduler.c
     *    Llena: saved_svc_sp, saved_lr, saved_spsr para P1 */
    extern void scheduler_init(void);
    scheduler_init();
    uart_puts("[OS] Scheduler inicializado (PCBs en USR mode)\r\n");
 
    /* 6. Memory barrier antes de habilitar IRQs */
    __asm__ volatile ("dsb" ::: "memory");
    __asm__ volatile ("isb");
 
    /* 7. Habilitar IRQ en CPU */
    cpu_irq_enable();
    uart_puts("[OS] IRQ habilitado\r\n");
 
    /* 8. [FASE 2] Saltar al primer proceso en USR mode
     *    launch_first_task() está en root.s.
     *    Emite traza MODE_SWITCH #1 y NO retorna.
     *    (reemplaza el while(1) de Fase 1) */
    uart_puts("[OS] Lanzando primer proceso en USR mode...\r\n");
    launch_first_task();   /* ← no retorna nunca */
 
    /* Nunca llega aquí */
    return 0;
}