#ifndef STRING_H
#define STRING_H

/* ============================================================
 * lib/string.h
 * Autor : Dev B
 * Fase  : 1
 *
 * Funciones de manejo de strings y memoria para bare-metal.
 * No usa ninguna función de libc (no hay libc disponible).
 * ============================================================ */

/* ----------------------------------------------------------
 * Funciones de string
 * ---------------------------------------------------------- */

/* str_len — retorna la longitud del string (sin contar '\0')
 *   Ejemplo: str_len("hola") → 4                           */
unsigned int str_len(const char *s);

/* str_cmp — compara dos strings lexicográficamente
 *   Retorna: 0  si son iguales
 *            <0 si a viene antes que b
 *            >0 si a viene después que b
 *   Ejemplo: str_cmp("abc", "abc") → 0
 *            str_cmp("abc", "abd") → -1                    */
int str_cmp(const char *a, const char *b);

/* str_cpy — copia src en dst incluyendo el '\0'
 *   Retorna: puntero a dst
 *   Precondición: dst debe tener espacio suficiente          */
char *str_cpy(char *dst, const char *src);

/* str_ncpy — copia máximo n bytes de src en dst
 *   Si src es más corto que n, rellena con '\0'
 *   Retorna: puntero a dst                                  */
char *str_ncpy(char *dst, const char *src, unsigned int n);

/* str_cat — agrega src al final de dst
 *   Retorna: puntero a dst
 *   Precondición: dst debe tener espacio para src adicional  */
char *str_cat(char *dst, const char *src);

/* ----------------------------------------------------------
 * Funciones de memoria
 * ---------------------------------------------------------- */

/* mem_set — llena n bytes en ptr con el valor dado
 *   Retorna: puntero a ptr
 *   Ejemplo: mem_set(buffer, 0, 64) → llena 64 bytes con 0  */
void *mem_set(void *ptr, int value, unsigned int n);

/* mem_cpy — copia n bytes de src a dst (sin solapamiento)
 *   Retorna: puntero a dst                                  */
void *mem_cpy(void *dst, const void *src, unsigned int n);

/* ----------------------------------------------------------
 * Aliases con nombres estándar de C
 *   Dev C puede usar strlen(), strcmp(), etc. directamente
 *   sin necesidad de recordar el prefijo str_/mem_.
 * ---------------------------------------------------------- */
#define strlen(s)         str_len(s)
#define strcmp(a, b)      str_cmp(a, b)
#define strcpy(d, s)      str_cpy(d, s)
#define strncpy(d, s, n)  str_ncpy(d, s, n)
#define strcat(d, s)      str_cat(d, s)
#define memset(p, v, n)   mem_set(p, v, n)
#define memcpy(d, s, n)   mem_cpy(d, s, n)

#endif /* STRING_H */
