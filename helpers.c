/**
 * sign_extend32 - sign extend a 32-bit value using specified bit as sign-bit
 * @value: value to sign extend
 * @index: 0 based bit index (0<=index<32) to sign bit
 */
static gint32 sign_extend32(guint32 value, int index)
{
    guint8 shift = 31 - index;
    return (gint32)(value << shift) >> shift;
}