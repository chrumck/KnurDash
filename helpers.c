#pragma once

#include "appData.c"

#define logError(...) appData.shutdownRequested = TRUE,  g_critical(__VA_ARGS__);

#define logErrorAndKill(...) g_critical(__VA_ARGS__), exit(EXIT_FAILURE)

#define getLength(value) (sizeof(value) / sizeof(value[0]))

/**
 * sign_extend32 - sign extend a 32-bit value using specified bit as sign-bit
 * @value: value to sign extend
 * @index: 0 based bit index (0<=index<32) to sign bit
 */
#define signExtend32(value, index) ((gint32)(value << (31 - index)) >> (31 - index))

gboolean isArrayEqual(const guint8* a, const guint8* b, guint size) {
  for (int i = 0; i < size; i++) if (a[i] != b[i]) return FALSE;

  return TRUE;
}

