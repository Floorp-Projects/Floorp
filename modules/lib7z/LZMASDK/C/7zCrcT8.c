/* 7zCrcT8.c -- CRC32 calculation with 8 tables
2008-03-19
Igor Pavlov
Public domain */

#include "7zCrc.h"

#define kCrcPoly 0xEDB88320
#define CRC_NUM_TABLES 8

UInt32 g_CrcTable[256 * CRC_NUM_TABLES];

void MY_FAST_CALL CrcGenerateTable()
{
  UInt32 i;
  for (i = 0; i < 256; i++)
  {
    UInt32 r = i;
    int j;
    for (j = 0; j < 8; j++)
      r = (r >> 1) ^ (kCrcPoly & ~((r & 1) - 1));
    g_CrcTable[i] = r;
  }
  #if CRC_NUM_TABLES > 1
  for (; i < 256 * CRC_NUM_TABLES; i++)
  {
    UInt32 r = g_CrcTable[i - 256];
    g_CrcTable[i] = g_CrcTable[r & 0xFF] ^ (r >> 8);
  }
  #endif
}

UInt32 MY_FAST_CALL CrcUpdateT8(UInt32 v, const void *data, size_t size, const UInt32 *table);

UInt32 MY_FAST_CALL CrcUpdate(UInt32 v, const void *data, size_t size)
{
  return CrcUpdateT8(v, data, size, g_CrcTable);
}

UInt32 MY_FAST_CALL CrcCalc(const void *data, size_t size)
{
  return CrcUpdateT8(CRC_INIT_VAL, data, size, g_CrcTable) ^ 0xFFFFFFFF;
}
