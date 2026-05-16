# Test Report — Fase 2: Execution Modes and Syscalls

> **Estado:** ⏳ Pendiente — completar con trazas UART reales del BeagleBone Black
> una vez que Dev A (root.s) y Dev B (scheduler.c) estén integrados.

---

## Criterios de aceptación (Sección 7)

| # | Criterio | Resultado |
|---|----------|-----------|
| 1 | Preemción estable vs. baseline Fase 1 | ⬜ |
| 2 | Dos tareas USR corriendo concurrentemente bajo el timer | ⬜ |
| 3 | SYS_YIELD correcto; trazas 4–5 del catálogo pareadas en UART | ⬜ |
| 4 | LR/SP/SPSR consistentes en runs mixtos IRQ + SVC | ⬜ |

---

## Catálogo MODE_SWITCH requerido (Sección 3.7, filas 1–5)

| # | Path | Dirección | Traza esperada |
|---|------|-----------|----------------|
| 1 | Initial boot | Kernel→User | `MODE_SWITCH KERNEL_TO_USER pid=1 reason=initial_launch` |
| 2 | Interrupt | User→Kernel | `MODE_SWITCH USER_TO_KERNEL pid=<n> reason=timer_irq` |
| 3 | Interrupt | Kernel→User | `MODE_SWITCH KERNEL_TO_USER pid=<m> reason=dispatch` |
| 4 | Syscall | User→Kernel | `MODE_SWITCH USER_TO_KERNEL pid=<n> reason=syscall id=0` |
| 5 | Syscall | Kernel→User | `MODE_SWITCH KERNEL_TO_USER pid=<m> reason=syscall_return id=0 rc=0` |

---

## Trazas UART capturadas

```
[pegar aquí el extracto de la sesión UART]
```

### Verificación traza por traza

**Traza 1 — initial_launch:**
```
[pegar]
```
☐ Presente  ☐ pid correcto

**Traza 2 — timer_irq:**
```
[pegar]
```
☐ Presente  ☐ pid corresponde a tarea activa

**Traza 3 — dispatch:**
```
[pegar]
```
☐ Presente  ☐ pid del proceso siguiente es correcto

**Traza 4 — syscall id=0:**
```
[pegar]
```
☐ Presente  ☐ pareada con traza 5

**Traza 5 — syscall_return id=0 rc=0:**
```
[pegar]
```
☐ Presente  ☐ rc=0 (éxito)

---

## Observaciones

- `g_a_yields` al finalizar captura: ______
- `g_b_yields` al finalizar captura: ______
- Herramienta UART: `minicom` / `picocom` / otra: ______
- Velocidad: 115200 baud
- Duración sesión: ______
