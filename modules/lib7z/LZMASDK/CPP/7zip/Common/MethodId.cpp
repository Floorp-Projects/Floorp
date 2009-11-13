// MethodId.cpp

#include "StdAfx.h"

#include "MethodId.h"
#include "../../Common/MyString.h"

static inline wchar_t GetHex(Byte value)
{
  return (wchar_t)((value < 10) ? ('0' + value) : ('A' + (value - 10)));
}

UString ConvertMethodIdToString(UInt64 id)
{
  wchar_t s[32];
  int len = 32;
  s[--len] = 0;
  do
  {
    s[--len] = GetHex((Byte)id & 0xF);
    id >>= 4;
    s[--len] = GetHex((Byte)id & 0xF);
    id >>= 4;
  }
  while (id != 0);
  return s + len;
}
