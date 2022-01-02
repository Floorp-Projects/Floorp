// PropVariantConvert.cpp

#include "StdAfx.h"

#include "../Common/IntToString.h"

#include "Defs.h"
#include "PropVariantConv.h"

#define UINT_TO_STR_2(c, val) { s[0] = (c); s[1] = (char)('0' + (val) / 10); s[2] = (char)('0' + (val) % 10); s += 3; }

bool ConvertUtcFileTimeToString(const FILETIME &utc, char *s, int level) throw()
{
  *s = 0;
  FILETIME ft;
  if (!FileTimeToLocalFileTime(&utc, &ft))
    return false;

  SYSTEMTIME st;
  if (!BOOLToBool(FileTimeToSystemTime(&ft, &st)))
    return false;

  {
    unsigned val = st.wYear;
    if (val >= 10000)
    {
      *s++ = (char)('0' + val / 10000);
      val %= 10000;
    }
    s[3] = (char)('0' + val % 10); val /= 10;
    s[2] = (char)('0' + val % 10); val /= 10;
    s[1] = (char)('0' + val % 10);
    s[0] = (char)('0' + val / 10);
    s += 4;
  }
  UINT_TO_STR_2('-', st.wMonth);
  UINT_TO_STR_2('-', st.wDay);
  
  if (level > kTimestampPrintLevel_DAY)
  {
    UINT_TO_STR_2(' ', st.wHour);
    UINT_TO_STR_2(':', st.wMinute);
    
    if (level >= kTimestampPrintLevel_SEC)
    {
      UINT_TO_STR_2(':', st.wSecond);

      if (level > kTimestampPrintLevel_SEC)
      {
        *s++ = '.';
        /*
        {
          unsigned val = st.wMilliseconds;
          s[2] = (char)('0' + val % 10); val /= 10;
          s[1] = (char)('0' + val % 10);
          s[0] = (char)('0' + val / 10);
          s += 3;
        }
        *s++ = ' ';
        */
        
        {
          unsigned numDigits = 7;
          UInt32 val = (UInt32)((((UInt64)ft.dwHighDateTime << 32) + ft.dwLowDateTime) % 10000000);
          for (unsigned i = numDigits; i != 0;)
          {
            i--;
            s[i] = (char)('0' + val % 10); val /= 10;
          }
          if (numDigits > (unsigned)level)
            numDigits = (unsigned)level;
          s += numDigits;
        }
      }
    }
  }
  
  *s = 0;
  return true;
}


bool ConvertUtcFileTimeToString(const FILETIME &ft, wchar_t *dest, int level) throw()
{
  char s[32];
  bool res = ConvertUtcFileTimeToString(ft, s, level);
  for (unsigned i = 0;; i++)
  {
    unsigned char c = s[i];
    dest[i] = c;
    if (c == 0)
      break;
  }
  return res;
}


void ConvertPropVariantToShortString(const PROPVARIANT &prop, char *dest) throw()
{
  *dest = 0;
  switch (prop.vt)
  {
    case VT_EMPTY: return;
    case VT_BSTR: dest[0] = '?'; dest[1] = 0; return;
    case VT_UI1: ConvertUInt32ToString(prop.bVal, dest); return;
    case VT_UI2: ConvertUInt32ToString(prop.uiVal, dest); return;
    case VT_UI4: ConvertUInt32ToString(prop.ulVal, dest); return;
    case VT_UI8: ConvertUInt64ToString(prop.uhVal.QuadPart, dest); return;
    case VT_FILETIME: ConvertUtcFileTimeToString(prop.filetime, dest); return;
    // case VT_I1: return ConvertInt64ToString(prop.cVal, dest); return;
    case VT_I2: ConvertInt64ToString(prop.iVal, dest); return;
    case VT_I4: ConvertInt64ToString(prop.lVal, dest); return;
    case VT_I8: ConvertInt64ToString(prop.hVal.QuadPart, dest); return;
    case VT_BOOL: dest[0] = VARIANT_BOOLToBool(prop.boolVal) ? '+' : '-'; dest[1] = 0; return;
    default: dest[0] = '?'; dest[1] = ':'; ConvertUInt64ToString(prop.vt, dest + 2);
  }
}

void ConvertPropVariantToShortString(const PROPVARIANT &prop, wchar_t *dest) throw()
{
  *dest = 0;
  switch (prop.vt)
  {
    case VT_EMPTY: return;
    case VT_BSTR: dest[0] = '?'; dest[1] = 0; return;
    case VT_UI1: ConvertUInt32ToString(prop.bVal, dest); return;
    case VT_UI2: ConvertUInt32ToString(prop.uiVal, dest); return;
    case VT_UI4: ConvertUInt32ToString(prop.ulVal, dest); return;
    case VT_UI8: ConvertUInt64ToString(prop.uhVal.QuadPart, dest); return;
    case VT_FILETIME: ConvertUtcFileTimeToString(prop.filetime, dest); return;
    // case VT_I1: return ConvertInt64ToString(prop.cVal, dest); return;
    case VT_I2: ConvertInt64ToString(prop.iVal, dest); return;
    case VT_I4: ConvertInt64ToString(prop.lVal, dest); return;
    case VT_I8: ConvertInt64ToString(prop.hVal.QuadPart, dest); return;
    case VT_BOOL: dest[0] = VARIANT_BOOLToBool(prop.boolVal) ? (wchar_t)'+' : (wchar_t)'-'; dest[1] = 0; return;
    default: dest[0] = '?'; dest[1] = ':'; ConvertUInt32ToString(prop.vt, dest + 2);
  }
}
