#ifndef __helpers_c
#define __helpers_c

#include <gtk/gtk.h>

/**
 * Return the length of passed array;
*/
#define getLength(value) (sizeof(value) / sizeof(value[0]))

/**
 * sign_extend32 - sign extend a 32-bit value using specified bit as sign-bit
 * @value: value to sign extend
 * @index: 0 based bit index (0<=index<32) to sign bit
 */
#define signExtend32(value, index) ((gint32)(value << (31 - index)) >> (31 - index))

 /**
  * Compares values of two byte arrays.
 */
gboolean isArrayEqual(const guint8* a, const guint8* b, guint size) {
    for (int i = 0; i < size; i++) if (a[i] != b[i]) return FALSE;

    return TRUE;
}

#endif