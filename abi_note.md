# ABI Note — Fase 2: Execution Modes and Syscalls

## Convención de syscalls desde modo USR

```
svc #0          ← instrucción de trap

r0 = ID         ← entrada; también valor de retorno al regresar a USR
r1 = arg1       ← 0 para SYS_YIELD
r2 = arg2       ← 0 para SYS_YIELD
r3 = arg3       ← 0 para SYS_YIELD
```

## Tabla de syscalls

| Símbolo    | ID | Acción                                      |
|------------|----|---------------------------------------------|
| SYS_YIELD  |  0 | Cede CPU → scheduler RR → retorna a USR     |

## Valores de retorno en r0

| Valor | Significado                        |
|-------|------------------------------------|
| `0`   | Éxito                              |
| `-1`  | ID desconocido (dispatcher branch) |

## Pasos del kernel al recibir SYS_YIELD (Sección 4.5)

1. Validar SPSR guardado → origen debe ser USR (CPSR[4:0] = 0x10)
2. Marcar proceso actual como PROC_READY
3. Ejecutar Round-Robin → elegir siguiente proceso
4. Restaurar contexto USR del proceso elegido
5. Escribir `0` en r0 del trap frame antes de retornar

## Flags de compilación usados en p1/ y p2/

```
-mcpu=cortex-a8 -marm -mfloat-abi=soft -ffreestanding -nostdlib -nostdinc
```

El `asm volatile("svc #0\n" ...)` con clobber `"memory"` evita
que el compilador reordene instrucciones alrededor del trap.
