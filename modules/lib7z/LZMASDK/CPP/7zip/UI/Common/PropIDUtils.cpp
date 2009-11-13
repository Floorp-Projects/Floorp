// PropIDUtils.cpp

#include "StdAfx.h"

#include "Common/IntToString.h"

#include "Windows/FileFind.h"
#include "Windows/PropVariantConversions.h"

#include "../../PropID.h"

#include "PropIDUtils.h"

using namespace NWindows;

void ConvertUInt32ToHex(UInt32 value, wchar_t *s)
{
  for (int i = 0; i < 8; i++)
  {
    int t = value & 0xF;
    value >>= 4;
    s[7 - i] = (wchar_t)((t < 10) ? (L'0' + t) : (L'A' + (t - 10)));
  }
  s[8] = L'\0';
}

#define MY_ATTR_CHAR(a, n, c) ((a )& (1 << (n))) ? c : L'-';

UString ConvertPropertyToString(const PROPVARIANT &prop, PROPID propID, bool full)
{
  switch(propID)
  {
    case kpidCTime:
    case kpidATime:
    case kpidMTime:
    {
      if (prop.vt != VT_FILETIME)
        break;
      FILETIME localFileTime;
      if ((prop.filetime.dwHighDateTime == 0 &&
          prop.filetime.dwLowDateTime == 0) ||
          !::FileTimeToLocalFileTime(&prop.filetime, &localFileTime))
        return UString();
      return ConvertFileTimeToString(localFileTime, true, full);
    }
    case kpidCRC:
    {
      if (prop.vt != VT_UI4)
        break;
      wchar_t temp[12];
      ConvertUInt32ToHex(prop.ulVal, temp);
      return temp;
    }
    case kpidAttrib:
    {
      if (prop.vt != VT_UI4)
        break;
      UString res;
      UInt32 a = prop.ulVal;
      if (NFile::NFind::NAttributes::IsReadOnly(a)) res += L'R';
      if (NFile::NFind::NAttributes::IsHidden(a)) res += L'H';
      if (NFile::NFind::NAttributes::IsSystem(a)) res += L'S';
      if (NFile::NFind::NAttributes::IsDir(a)) res += L'D';
      if (NFile::NFind::NAttributes::IsArchived(a)) res += L'A';
      if (NFile::NFind::NAttributes::IsCompressed(a)) res += L'C';
      if (NFile::NFind::NAttributes::IsEncrypted(a)) res += L'E';
      return res;
    }
    case kpidPosixAttrib:
    {
      if (prop.vt != VT_UI4)
        break;
      UString res;
      UInt32 a = prop.ulVal;
      wchar_t temp[16];
      temp[0] = MY_ATTR_CHAR(a, 14, L'd');
      for (int i = 6; i >= 0; i -= 3)
      {
        temp[7 - i] = MY_ATTR_CHAR(a, i + 2, L'r');
        temp[8 - i] = MY_ATTR_CHAR(a, i + 1, L'w');
        temp[9 - i] = MY_ATTR_CHAR(a, i + 0, L'x');
      }
      temp[10] = 0;
      res = temp;
      a &= ~0x1FF;
      a &= ~0xC000;
      if (a != 0)
      {
        ConvertUInt32ToHex(a, temp);
        res = UString(temp) + L' ' + res;
      }
      return res;
    }
  }
  return ConvertPropVariantToString(prop);
}
