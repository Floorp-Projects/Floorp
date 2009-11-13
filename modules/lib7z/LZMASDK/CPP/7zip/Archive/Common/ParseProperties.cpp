// ParseProperties.cpp

#include "StdAfx.h"

#include "ParseProperties.h"

#include "Common/StringToInt.h"
#include "Common/MyCom.h"

HRESULT ParsePropValue(const UString &name, const PROPVARIANT &prop, UInt32 &resValue)
{
  if (prop.vt == VT_UI4)
  {
    if (!name.IsEmpty())
      return E_INVALIDARG;
    resValue = prop.ulVal;
  }
  else if (prop.vt == VT_EMPTY)
  {
    if(!name.IsEmpty())
    {
      const wchar_t *start = name;
      const wchar_t *end;
      UInt64 v = ConvertStringToUInt64(start, &end);
      if (end - start != name.Length())
        return E_INVALIDARG;
      resValue = (UInt32)v;
    }
  }
  else
    return E_INVALIDARG;
  return S_OK;
}

static const int kLogarithmicSizeLimit = 32;
static const wchar_t kByteSymbol = L'B';
static const wchar_t kKiloByteSymbol = L'K';
static const wchar_t kMegaByteSymbol = L'M';

HRESULT ParsePropDictionaryValue(const UString &srcStringSpec, UInt32 &dicSize)
{
  UString srcString = srcStringSpec;
  srcString.MakeUpper();

  const wchar_t *start = srcString;
  const wchar_t *end;
  UInt64 number = ConvertStringToUInt64(start, &end);
  int numDigits = (int)(end - start);
  if (numDigits == 0 || srcString.Length() > numDigits + 1)
    return E_INVALIDARG;
  if (srcString.Length() == numDigits)
  {
    if (number >= kLogarithmicSizeLimit)
      return E_INVALIDARG;
    dicSize = (UInt32)1 << (int)number;
    return S_OK;
  }
  switch (srcString[numDigits])
  {
    case kByteSymbol:
      if (number >= ((UInt64)1 << kLogarithmicSizeLimit))
        return E_INVALIDARG;
      dicSize = (UInt32)number;
      break;
    case kKiloByteSymbol:
      if (number >= ((UInt64)1 << (kLogarithmicSizeLimit - 10)))
        return E_INVALIDARG;
      dicSize = (UInt32)(number << 10);
      break;
    case kMegaByteSymbol:
      if (number >= ((UInt64)1 << (kLogarithmicSizeLimit - 20)))
        return E_INVALIDARG;
      dicSize = (UInt32)(number << 20);
      break;
    default:
      return E_INVALIDARG;
  }
  return S_OK;
}

HRESULT ParsePropDictionaryValue(const UString &name, const PROPVARIANT &prop, UInt32 &resValue)
{
  if (name.IsEmpty())
  {
    if (prop.vt == VT_UI4)
    {
      UInt32 logDicSize = prop.ulVal;
      if (logDicSize >= 32)
        return E_INVALIDARG;
      resValue = (UInt32)1 << logDicSize;
      return S_OK;
    }
    if (prop.vt == VT_BSTR)
      return ParsePropDictionaryValue(prop.bstrVal, resValue);
    return E_INVALIDARG;
  }
  return ParsePropDictionaryValue(name, resValue);
}

bool StringToBool(const UString &s, bool &res)
{
  if (s.IsEmpty() || s.CompareNoCase(L"ON") == 0 || s.Compare(L"+") == 0)
  {
    res = true;
    return true;
  }
  if (s.CompareNoCase(L"OFF") == 0 || s.Compare(L"-") == 0)
  {
    res = false;
    return true;
  }
  return false;
}

HRESULT SetBoolProperty(bool &dest, const PROPVARIANT &value)
{
  switch(value.vt)
  {
    case VT_EMPTY:
      dest = true;
      return S_OK;
    case VT_BOOL:
      dest = (value.boolVal != VARIANT_FALSE);
      return S_OK;
    /*
    case VT_UI4:
      dest = (value.ulVal != 0);
      break;
    */
    case VT_BSTR:
      return StringToBool(value.bstrVal, dest) ?  S_OK : E_INVALIDARG;
  }
  return E_INVALIDARG;
}

int ParseStringToUInt32(const UString &srcString, UInt32 &number)
{
  const wchar_t *start = srcString;
  const wchar_t *end;
  UInt64 number64 = ConvertStringToUInt64(start, &end);
  if (number64 > 0xFFFFFFFF)
  {
    number = 0;
    return 0;
  }
  number = (UInt32)number64;
  return (int)(end - start);
}

HRESULT ParseMtProp(const UString &name, const PROPVARIANT &prop, UInt32 defaultNumThreads, UInt32 &numThreads)
{
  if (name.IsEmpty())
  {
    switch(prop.vt)
    {
      case VT_UI4:
        numThreads = prop.ulVal;
        break;
      default:
      {
        bool val;
        RINOK(SetBoolProperty(val, prop));
        numThreads = (val ? defaultNumThreads : 1);
        break;
      }
    }
  }
  else
  {
    UInt32 number;
    int index = ParseStringToUInt32(name, number);
    if (index != name.Length())
      return E_INVALIDARG;
    numThreads = number;
  }
  return S_OK;
}
