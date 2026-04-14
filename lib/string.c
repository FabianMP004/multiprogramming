/* ============================================================
 * lib/string.c
 * Autor : Dev B
 * Fase  : 1
 *
 * Implementación de funciones de string y memoria.
 * Cero dependencias de libc.
 * ============================================================ */

#include "string.h"

/* ============================================================
 * str_len()
 *   Recorre el string carácter a carácter hasta encontrar '\0'.
 *   Retorna cuántos caracteres recorrió (sin contar el '\0').
 *
 *   Ejemplo con s = "hola":
 *     s[0]='h' → len=1
 *     s[1]='o' → len=2
 *     s[2]='l' → len=3
 *     s[3]='a' → len=4
 *     s[4]='\0'→ stop, retorna 4
 * ============================================================ */
unsigned int str_len(const char *s)
{
    unsigned int len = 0;
    while (s[len] != '\0') {
        len++;
    }
    return len;
}

/* ============================================================
 * str_cmp()
 *   Compara carácter a carácter mientras sean iguales y no
 *   llegue al final de alguno de los strings.
 *   El resultado es la diferencia del primer par diferente.
 *
 *   Ejemplo con a="abc", b="abd":
 *     'a'=='a' → avanzar
 *     'b'=='b' → avanzar
 *     'c'!='d' → retorna 'c'-'d' = -1
 * ============================================================ */
int str_cmp(const char *a, const char *b)
{
    while (*a && (*a == *b)) {
        a++;
        b++;
    }
    /* Cast a unsigned char para comparación correcta de
     * caracteres con valores > 127 */
    return (int)((unsigned char)*a) - (int)((unsigned char)*b);
}

/* ============================================================
 * str_cpy()
 *   Copia src en dst un carácter a la vez, incluyendo el '\0'
 *   final.  El truco del while((*dst++ = *src++)) aprovecha
 *   que la asignación retorna el valor asignado: cuando se
 *   asigna '\0' el while termina.
 * ============================================================ */
char *str_cpy(char *dst, const char *src)
{
    char *ret = dst;    /* guardar inicio de dst para retornar */
    while ((*dst++ = *src++) != '\0');
    return ret;
}

/* ============================================================
 * str_ncpy()
 *   Como str_cpy pero copia máximo n bytes.
 *   Si src termina antes de n, rellena el resto con '\0'.
 *
 *   Ejemplo con dst=buf, src="hi", n=5:
 *     copia 'h','i','\0','\0','\0' → buf = "hi\0\0\0"
 * ============================================================ */
char *str_ncpy(char *dst, const char *src, unsigned int n)
{
    char *ret = dst;

    /* Copiar mientras hay fuente y no llegamos al límite */
    while (n > 0 && *src != '\0') {
        *dst++ = *src++;
        n--;
    }

    /* Rellenar el resto con '\0' */
    while (n > 0) {
        *dst++ = '\0';
        n--;
    }

    return ret;
}

/* ============================================================
 * str_cat()
 *   Agrega src al final de dst.
 *   Primero avanza dst hasta el '\0' final,
 *   luego copia src desde ahí.
 *
 *   Ejemplo con dst="hola", src=" mundo":
 *     avanza dst hasta el '\0' de "hola"
 *     copia " mundo\0" → dst = "hola mundo"
 * ============================================================ */
char *str_cat(char *dst, const char *src)
{
    char *ret = dst;

    /* Avanzar hasta el final de dst */
    while (*dst) {
        dst++;
    }

    /* Copiar src desde el '\0' de dst en adelante */
    while ((*dst++ = *src++) != '\0');

    return ret;
}

/* ============================================================
 * mem_set()
 *   Llena n bytes en la dirección ptr con el valor dado.
 *   El valor se trunca a 8 bits (un byte).
 *
 *   Uso más común: limpiar buffers
 *       mem_set(buffer, 0, sizeof(buffer));
 * ============================================================ */
void *mem_set(void *ptr, int value, unsigned int n)
{
    unsigned char *p = (unsigned char *)ptr;
    unsigned char  v = (unsigned char)value;

    while (n--) {
        *p++ = v;
    }

    return ptr;
}

/* ============================================================
 * mem_cpy()
 *   Copia n bytes de src a dst, byte a byte.
 *   Asume que las regiones NO se solapan.
 *
 *   Uso: copiar bloques de memoria (ej. contexto de proceso)
 *       mem_cpy(destino, origen, sizeof(pcb_t));
 * ============================================================ */
void *mem_cpy(void *dst, const void *src, unsigned int n)
{
    unsigned char       *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;

    while (n--) {
        *d++ = *s++;
    }

    return dst;
}
