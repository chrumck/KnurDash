#include <gtk/gtk.h>

/**
 * Return the size of passed value;
*/
#define getSize(value) (sizeof(value) / sizeof(value[0]))

/**
 * sign_extend32 - sign extend a 32-bit value using specified bit as sign-bit
 * @value: value to sign extend
 * @index: 0 based bit index (0<=index<32) to sign bit
 */
#define signExtend32(value, index) ((gint32)(value << (31 - index)) >> (31 - index))

