// HandlerOut.cpp

#include "StdAfx.h"

#include "../../../Common/StringToInt.h"

#include "../../../Windows/PropVariant.h"

#ifdef COMPRESS_MT
#include "../../../Windows/System.h"
#endif

#include "../../ICoder.h"

#include "../Common/ParseProperties.h"

#include "HandlerOut.h"

using namespace NWindows;

namespace NArchive {

static const wchar_t *kCopyMethod = L"Copy";
static const wchar_t *kLZMAMethodName = L"LZMA";
static const wchar_t *kLZMA2MethodName = L"LZMA2";
static const wchar_t *kBZip2MethodName = L"BZip2";
static const wchar_t *kPpmdMethodName = L"PPMd";
static const wchar_t *kDeflateMethodName = L"Deflate";
static const wchar_t *kDeflate64MethodName = L"Deflate64";

static const wchar_t *kLzmaMatchFinderX1 = L"HC4";
static const wchar_t *kLzmaMatchFinderX5 = L"BT4";

static const UInt32 kLzmaAlgoX1 = 0;
static const UInt32 kLzmaAlgoX5 = 1;

static const UInt32 kLzmaDicSizeX1 = 1 << 16;
static const UInt32 kLzmaDicSizeX3 = 1 << 20;
static const UInt32 kLzmaDicSizeX5 = 1 << 24;
static const UInt32 kLzmaDicSizeX7 = 1 << 25;
static const UInt32 kLzmaDicSizeX9 = 1 << 26;

static const UInt32 kLzmaFastBytesX1 = 32;
static const UInt32 kLzmaFastBytesX7 = 64;

static const UInt32 kPpmdMemSizeX1 = (1 << 22);
static const UInt32 kPpmdMemSizeX5 = (1 << 24);
static const UInt32 kPpmdMemSizeX7 = (1 << 26);
static const UInt32 kPpmdMemSizeX9 = (192 << 20);

static const UInt32 kPpmdOrderX1 = 4;
static const UInt32 kPpmdOrderX5 = 6;
static const UInt32 kPpmdOrderX7 = 16;
static const UInt32 kPpmdOrderX9 = 32;

static const UInt32 kDeflateAlgoX1 = 0;
static const UInt32 kDeflateAlgoX5 = 1;

static const UInt32 kDeflateFastBytesX1 = 32;
static const UInt32 kDeflateFastBytesX7 = 64;
static const UInt32 kDeflateFastBytesX9 = 128;

static const UInt32 kDeflatePassesX1 = 1;
static const UInt32 kDeflatePassesX7 = 3;
static const UInt32 kDeflatePassesX9 = 10;

static const UInt32 kBZip2NumPassesX1 = 1;
static const UInt32 kBZip2NumPassesX7 = 2;
static const UInt32 kBZip2NumPassesX9 = 7;

static const UInt32 kBZip2DicSizeX1 = 100000;
static const UInt32 kBZip2DicSizeX3 = 500000;
static const UInt32 kBZip2DicSizeX5 = 900000;

static const wchar_t *kDefaultMethodName = kLZMAMethodName;

static const wchar_t *kLzmaMatchFinderForHeaders = L"BT2";
static const UInt32 kDictionaryForHeaders = 1 << 20;
static const UInt32 kNumFastBytesForHeaders = 273;
static const UInt32 kAlgorithmForHeaders = kLzmaAlgoX5;

static bool AreEqual(const UString &methodName, const wchar_t *s)
  { return (methodName.CompareNoCase(s) == 0); }

bool COneMethodInfo::IsLzma() const
{
  return
    AreEqual(MethodName, kLZMAMethodName) ||
    AreEqual(MethodName, kLZMA2MethodName);
}

static inline bool IsBZip2Method(const UString &methodName)
  { return AreEqual(methodName, kBZip2MethodName); }

static inline bool IsPpmdMethod(const UString &methodName)
  { return AreEqual(methodName, kPpmdMethodName); }

static inline bool IsDeflateMethod(const UString &methodName)
{
  return
    AreEqual(methodName, kDeflateMethodName) ||
    AreEqual(methodName, kDeflate64MethodName);
}

struct CNameToPropID
{
  PROPID PropID;
  VARTYPE VarType;
  const wchar_t *Name;
};

static CNameToPropID g_NameToPropID[] =
{
  { NCoderPropID::kBlockSize, VT_UI4, L"C" },
  { NCoderPropID::kDictionarySize, VT_UI4, L"D" },
  { NCoderPropID::kUsedMemorySize, VT_UI4, L"MEM" },

  { NCoderPropID::kOrder, VT_UI4, L"O" },
  { NCoderPropID::kPosStateBits, VT_UI4, L"PB" },
  { NCoderPropID::kLitContextBits, VT_UI4, L"LC" },
  { NCoderPropID::kLitPosBits, VT_UI4, L"LP" },
  { NCoderPropID::kEndMarker, VT_BOOL, L"eos" },

  { NCoderPropID::kNumPasses, VT_UI4, L"Pass" },
  { NCoderPropID::kNumFastBytes, VT_UI4, L"fb" },
  { NCoderPropID::kMatchFinderCycles, VT_UI4, L"mc" },
  { NCoderPropID::kAlgorithm, VT_UI4, L"a" },
  { NCoderPropID::kMatchFinder, VT_BSTR, L"mf" },
  { NCoderPropID::kNumThreads, VT_UI4, L"mt" },
  { NCoderPropID::kDefaultProp, VT_UI4, L"" }
};

static bool ConvertProperty(PROPVARIANT srcProp, VARTYPE varType, NCOM::CPropVariant &destProp)
{
  if (varType == srcProp.vt)
  {
    destProp = srcProp;
    return true;
  }
  if (varType == VT_UI1)
  {
    if (srcProp.vt == VT_UI4)
    {
      UInt32 value = srcProp.ulVal;
      if (value > 0xFF)
        return false;
      destProp = (Byte)value;
      return true;
    }
  }
  else if (varType == VT_BOOL)
  {
    bool res;
    if (SetBoolProperty(res, srcProp) != S_OK)
      return false;
    destProp = res;
    return true;
  }
  return false;
}
    
static int FindPropIdExact(const UString &name)
{
  for (int i = 0; i < sizeof(g_NameToPropID) / sizeof(g_NameToPropID[0]); i++)
    if (name.CompareNoCase(g_NameToPropID[i].Name) == 0)
      return i;
  return -1;
}

static int FindPropIdStart(const UString &name)
{
  for (int i = 0; i < sizeof(g_NameToPropID) / sizeof(g_NameToPropID[0]); i++)
  {
    UString t = g_NameToPropID[i].Name;
    if (t.CompareNoCase(name.Left(t.Length())) == 0)
      return i;
  }
  return -1;
}

static void SetMethodProp(COneMethodInfo &m, PROPID propID, const NCOM::CPropVariant &value)
{
  for (int j = 0; j < m.Props.Size(); j++)
    if (m.Props[j].Id == propID)
      return;
  CProp prop;
  prop.Id = propID;
  prop.Value = value;
  m.Props.Add(prop);
}

void COutHandler::SetCompressionMethod2(COneMethodInfo &oneMethodInfo
    #ifdef COMPRESS_MT
    , UInt32 numThreads
    #endif
    )
{
  UInt32 level = _level;
  if (oneMethodInfo.MethodName.IsEmpty())
    oneMethodInfo.MethodName = kDefaultMethodName;
  
  if (oneMethodInfo.IsLzma())
  {
    UInt32 dicSize =
      (level >= 9 ? kLzmaDicSizeX9 :
      (level >= 7 ? kLzmaDicSizeX7 :
      (level >= 5 ? kLzmaDicSizeX5 :
      (level >= 3 ? kLzmaDicSizeX3 :
                    kLzmaDicSizeX1))));
    
    UInt32 algo =
      (level >= 5 ? kLzmaAlgoX5 :
                    kLzmaAlgoX1);
    
    UInt32 fastBytes =
      (level >= 7 ? kLzmaFastBytesX7 :
                    kLzmaFastBytesX1);
    
    const wchar_t *matchFinder =
      (level >= 5 ? kLzmaMatchFinderX5 :
                    kLzmaMatchFinderX1);
    
    SetMethodProp(oneMethodInfo, NCoderPropID::kDictionarySize, dicSize);
    SetMethodProp(oneMethodInfo, NCoderPropID::kAlgorithm, algo);
    SetMethodProp(oneMethodInfo, NCoderPropID::kNumFastBytes, fastBytes);
    SetMethodProp(oneMethodInfo, NCoderPropID::kMatchFinder, matchFinder);
    #ifdef COMPRESS_MT
    SetMethodProp(oneMethodInfo, NCoderPropID::kNumThreads, numThreads);
    #endif
  }
  else if (IsDeflateMethod(oneMethodInfo.MethodName))
  {
    UInt32 fastBytes =
      (level >= 9 ? kDeflateFastBytesX9 :
      (level >= 7 ? kDeflateFastBytesX7 :
                    kDeflateFastBytesX1));
    
    UInt32 numPasses =
      (level >= 9 ? kDeflatePassesX9 :
      (level >= 7 ? kDeflatePassesX7 :
                    kDeflatePassesX1));
    
    UInt32 algo =
      (level >= 5 ? kDeflateAlgoX5 :
                    kDeflateAlgoX1);
    
    SetMethodProp(oneMethodInfo, NCoderPropID::kAlgorithm, algo);
    SetMethodProp(oneMethodInfo, NCoderPropID::kNumFastBytes, fastBytes);
    SetMethodProp(oneMethodInfo, NCoderPropID::kNumPasses, numPasses);
  }
  else if (IsBZip2Method(oneMethodInfo.MethodName))
  {
    UInt32 numPasses =
      (level >= 9 ? kBZip2NumPassesX9 :
      (level >= 7 ? kBZip2NumPassesX7 :
                    kBZip2NumPassesX1));
    
    UInt32 dicSize =
      (level >= 5 ? kBZip2DicSizeX5 :
      (level >= 3 ? kBZip2DicSizeX3 :
                    kBZip2DicSizeX1));
    
    SetMethodProp(oneMethodInfo, NCoderPropID::kNumPasses, numPasses);
    SetMethodProp(oneMethodInfo, NCoderPropID::kDictionarySize, dicSize);
    #ifdef COMPRESS_MT
    SetMethodProp(oneMethodInfo, NCoderPropID::kNumThreads, numThreads);
    #endif
  }
  else if (IsPpmdMethod(oneMethodInfo.MethodName))
  {
    UInt32 useMemSize =
      (level >= 9 ? kPpmdMemSizeX9 :
      (level >= 7 ? kPpmdMemSizeX7 :
      (level >= 5 ? kPpmdMemSizeX5 :
                    kPpmdMemSizeX1)));
    
    UInt32 order =
      (level >= 9 ? kPpmdOrderX9 :
      (level >= 7 ? kPpmdOrderX7 :
      (level >= 5 ? kPpmdOrderX5 :
                    kPpmdOrderX1)));
    
    SetMethodProp(oneMethodInfo, NCoderPropID::kUsedMemorySize, useMemSize);
    SetMethodProp(oneMethodInfo, NCoderPropID::kOrder, order);
  }
}

static void SplitParams(const UString &srcString, UStringVector &subStrings)
{
  subStrings.Clear();
  UString name;
  int len = srcString.Length();
  if (len == 0)
    return;
  for (int i = 0; i < len; i++)
  {
    wchar_t c = srcString[i];
    if (c == L':')
    {
      subStrings.Add(name);
      name.Empty();
    }
    else
      name += c;
  }
  subStrings.Add(name);
}

static void SplitParam(const UString &param, UString &name, UString &value)
{
  int eqPos = param.Find(L'=');
  if (eqPos >= 0)
  {
    name = param.Left(eqPos);
    value = param.Mid(eqPos + 1);
    return;
  }
  for(int i = 0; i < param.Length(); i++)
  {
    wchar_t c = param[i];
    if (c >= L'0' && c <= L'9')
    {
      name = param.Left(i);
      value = param.Mid(i);
      return;
    }
  }
  name = param;
}

HRESULT COutHandler::SetParam(COneMethodInfo &oneMethodInfo, const UString &name, const UString &value)
{
  CProp prop;
  int index = FindPropIdExact(name);
  if (index < 0)
    return E_INVALIDARG;
  const CNameToPropID &nameToPropID = g_NameToPropID[index];
  prop.Id = nameToPropID.PropID;

  if (prop.Id == NCoderPropID::kBlockSize ||
      prop.Id == NCoderPropID::kDictionarySize ||
      prop.Id == NCoderPropID::kUsedMemorySize)
  {
    UInt32 dicSize;
    RINOK(ParsePropDictionaryValue(value, dicSize));
    prop.Value = dicSize;
  }
  else
  {
    NCOM::CPropVariant propValue;
    
    if (nameToPropID.VarType == VT_BSTR)
      propValue = value;
    else if (nameToPropID.VarType == VT_BOOL)
    {
      bool res;
      if (!StringToBool(value, res))
        return E_INVALIDARG;
      propValue = res;
    }
    else
    {
      UInt32 number;
      if (ParseStringToUInt32(value, number) == value.Length())
        propValue = number;
      else
        propValue = value;
    }
    
    if (!ConvertProperty(propValue, nameToPropID.VarType, prop.Value))
      return E_INVALIDARG;
  }
  oneMethodInfo.Props.Add(prop);
  return S_OK;
}

HRESULT COutHandler::SetParams(COneMethodInfo &oneMethodInfo, const UString &srcString)
{
  UStringVector params;
  SplitParams(srcString, params);
  if (params.Size() > 0)
    oneMethodInfo.MethodName = params[0];
  for (int i = 1; i < params.Size(); i++)
  {
    const UString &param = params[i];
    UString name, value;
    SplitParam(param, name, value);
    RINOK(SetParam(oneMethodInfo, name, value));
  }
  return S_OK;
}

HRESULT COutHandler::SetSolidSettings(const UString &s)
{
  UString s2 = s;
  s2.MakeUpper();
  for (int i = 0; i < s2.Length();)
  {
    const wchar_t *start = ((const wchar_t *)s2) + i;
    const wchar_t *end;
    UInt64 v = ConvertStringToUInt64(start, &end);
    if (start == end)
    {
      if (s2[i++] != 'E')
        return E_INVALIDARG;
      _solidExtension = true;
      continue;
    }
    i += (int)(end - start);
    if (i == s2.Length())
      return E_INVALIDARG;
    wchar_t c = s2[i++];
    switch(c)
    {
      case 'F':
        if (v < 1)
          v = 1;
        _numSolidFiles = v;
        break;
      case 'B':
        _numSolidBytes = v;
        _numSolidBytesDefined = true;
        break;
      case 'K':
        _numSolidBytes = (v << 10);
        _numSolidBytesDefined = true;
        break;
      case 'M':
        _numSolidBytes = (v << 20);
        _numSolidBytesDefined = true;
        break;
      case 'G':
        _numSolidBytes = (v << 30);
        _numSolidBytesDefined = true;
        break;
      default:
        return E_INVALIDARG;
    }
  }
  return S_OK;
}

HRESULT COutHandler::SetSolidSettings(const PROPVARIANT &value)
{
  bool isSolid;
  switch(value.vt)
  {
    case VT_EMPTY:
      isSolid = true;
      break;
    case VT_BOOL:
      isSolid = (value.boolVal != VARIANT_FALSE);
      break;
    case VT_BSTR:
      if (StringToBool(value.bstrVal, isSolid))
        break;
      return SetSolidSettings(value.bstrVal);
    default:
      return E_INVALIDARG;
  }
  if (isSolid)
    InitSolid();
  else
    _numSolidFiles = 1;
  return S_OK;
}

void COutHandler::Init()
{
  _removeSfxBlock = false;
  _compressHeaders = true;
  _encryptHeadersSpecified = false;
  _encryptHeaders = false;
  
  WriteCTime = false;
  WriteATime = false;
  WriteMTime = true;
  
  #ifdef COMPRESS_MT
  _numThreads = NSystem::GetNumberOfProcessors();
  #endif
  
  _level = 5;
  _autoFilter = true;
  _volumeMode = false;
  _crcSize = 4;
  InitSolid();
}

void COutHandler::BeforeSetProperty()
{
  Init();
  #ifdef COMPRESS_MT
  numProcessors = NSystem::GetNumberOfProcessors();
  #endif

  mainDicSize = 0xFFFFFFFF;
  mainDicMethodIndex = 0xFFFFFFFF;
  minNumber = 0;
  _crcSize = 4;
}

HRESULT COutHandler::SetProperty(const wchar_t *nameSpec, const PROPVARIANT &value)
{
  UString name = nameSpec;
  name.MakeUpper();
  if (name.IsEmpty())
    return E_INVALIDARG;
  
  if (name[0] == 'X')
  {
    name.Delete(0);
    _level = 9;
    return ParsePropValue(name, value, _level);
  }
  
  if (name[0] == L'S')
  {
    name.Delete(0);
    if (name.IsEmpty())
      return SetSolidSettings(value);
    if (value.vt != VT_EMPTY)
      return E_INVALIDARG;
    return SetSolidSettings(name);
  }
  
  if (name == L"CRC")
  {
    _crcSize = 4;
    name.Delete(0, 3);
    return ParsePropValue(name, value, _crcSize);
  }
  
  UInt32 number;
  int index = ParseStringToUInt32(name, number);
  UString realName = name.Mid(index);
  if (index == 0)
  {
    if(name.Left(2).CompareNoCase(L"MT") == 0)
    {
      #ifdef COMPRESS_MT
      RINOK(ParseMtProp(name.Mid(2), value, numProcessors, _numThreads));
      #endif
      return S_OK;
    }
    if (name.CompareNoCase(L"RSFX") == 0)  return SetBoolProperty(_removeSfxBlock, value);
    if (name.CompareNoCase(L"F") == 0) return SetBoolProperty(_autoFilter, value);
    if (name.CompareNoCase(L"HC") == 0) return SetBoolProperty(_compressHeaders, value);
    if (name.CompareNoCase(L"HCF") == 0)
    {
      bool compressHeadersFull = true;
      RINOK(SetBoolProperty(compressHeadersFull, value));
      if (!compressHeadersFull)
        return E_INVALIDARG;
      return S_OK;
    }
    if (name.CompareNoCase(L"HE") == 0)
    {
      RINOK(SetBoolProperty(_encryptHeaders, value));
      _encryptHeadersSpecified = true;
      return S_OK;
    }
    if (name.CompareNoCase(L"TC") == 0) return SetBoolProperty(WriteCTime, value);
    if (name.CompareNoCase(L"TA") == 0) return SetBoolProperty(WriteATime, value);
    if (name.CompareNoCase(L"TM") == 0) return SetBoolProperty(WriteMTime, value);
    if (name.CompareNoCase(L"V") == 0) return SetBoolProperty(_volumeMode, value);
    number = 0;
  }
  if (number > 10000)
    return E_FAIL;
  if (number < minNumber)
    return E_INVALIDARG;
  number -= minNumber;
  for(int j = _methods.Size(); j <= (int)number; j++)
  {
    COneMethodInfo oneMethodInfo;
    _methods.Add(oneMethodInfo);
  }
  
  COneMethodInfo &oneMethodInfo = _methods[number];
  
  if (realName.Length() == 0)
  {
    if (value.vt != VT_BSTR)
      return E_INVALIDARG;
    
    RINOK(SetParams(oneMethodInfo, value.bstrVal));
  }
  else
  {
    int index = FindPropIdStart(realName);
    if (index < 0)
      return E_INVALIDARG;
    const CNameToPropID &nameToPropID = g_NameToPropID[index];
    CProp prop;
    prop.Id = nameToPropID.PropID;

    if (prop.Id == NCoderPropID::kBlockSize ||
        prop.Id == NCoderPropID::kDictionarySize ||
        prop.Id == NCoderPropID::kUsedMemorySize)
    {
      UInt32 dicSize;
      RINOK(ParsePropDictionaryValue(realName.Mid(MyStringLen(nameToPropID.Name)), value, dicSize));
      prop.Value = dicSize;
      if (number <= mainDicMethodIndex)
        mainDicSize = dicSize;
    }
    else
    {
      int index = FindPropIdExact(realName);
      if (index < 0)
        return E_INVALIDARG;
      const CNameToPropID &nameToPropID = g_NameToPropID[index];
      prop.Id = nameToPropID.PropID;
      if (!ConvertProperty(value, nameToPropID.VarType, prop.Value))
        return E_INVALIDARG;
    }
    oneMethodInfo.Props.Add(prop);
  }
  return S_OK;
}

}
