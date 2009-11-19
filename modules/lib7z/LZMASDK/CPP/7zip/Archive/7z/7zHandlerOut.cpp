// 7zHandlerOut.cpp

#include "StdAfx.h"

#include "../../../Windows/PropVariant.h"

#include "../../../Common/ComTry.h"
#include "../../../Common/StringToInt.h"

#include "../../ICoder.h"

#include "../Common/ItemNameUtils.h"
#include "../Common/ParseProperties.h"

#include "7zHandler.h"
#include "7zOut.h"
#include "7zUpdate.h"

using namespace NWindows;

namespace NArchive {
namespace N7z {

static const wchar_t *kLZMAMethodName = L"LZMA";
static const wchar_t *kCopyMethod = L"Copy";
static const wchar_t *kDefaultMethodName = kLZMAMethodName;

static const UInt32 kLzmaAlgorithmX5 = 1;
static const wchar_t *kLzmaMatchFinderForHeaders = L"BT2";
static const UInt32 kDictionaryForHeaders =
  #ifdef UNDER_CE
  1 << 18
  #else
  1 << 20
  #endif
;
static const UInt32 kNumFastBytesForHeaders = 273;
static const UInt32 kAlgorithmForHeaders = kLzmaAlgorithmX5;

static inline bool IsCopyMethod(const UString &methodName)
  { return (methodName.CompareNoCase(kCopyMethod) == 0); }

STDMETHODIMP CHandler::GetFileTimeType(UInt32 *type)
{
  *type = NFileTimeType::kWindows;
  return S_OK;
}

HRESULT CHandler::SetCompressionMethod(
    CCompressionMethodMode &methodMode,
    CCompressionMethodMode &headerMethod)
{
  HRESULT res = SetCompressionMethod(methodMode, _methods
  #ifdef COMPRESS_MT
  , _numThreads
  #endif
  );
  RINOK(res);
  methodMode.Binds = _binds;

  if (_compressHeaders)
  {
    // headerMethod.Methods.Add(methodMode.Methods.Back());

    CObjectVector<COneMethodInfo> headerMethodInfoVector;
    COneMethodInfo oneMethodInfo;
    oneMethodInfo.MethodName = kLZMAMethodName;
    {
      CProp prop;
      prop.Id = NCoderPropID::kMatchFinder;
      prop.Value = kLzmaMatchFinderForHeaders;
      oneMethodInfo.Props.Add(prop);
    }
    {
      CProp prop;
      prop.Id = NCoderPropID::kAlgorithm;
      prop.Value = kAlgorithmForHeaders;
      oneMethodInfo.Props.Add(prop);
    }
    {
      CProp prop;
      prop.Id = NCoderPropID::kNumFastBytes;
      prop.Value = (UInt32)kNumFastBytesForHeaders;
      oneMethodInfo.Props.Add(prop);
    }
    {
      CProp prop;
      prop.Id = NCoderPropID::kDictionarySize;
      prop.Value = (UInt32)kDictionaryForHeaders;
      oneMethodInfo.Props.Add(prop);
    }
    headerMethodInfoVector.Add(oneMethodInfo);
    HRESULT res = SetCompressionMethod(headerMethod, headerMethodInfoVector
      #ifdef COMPRESS_MT
      ,1
      #endif
    );
    RINOK(res);
  }
  return S_OK;
}

HRESULT CHandler::SetCompressionMethod(
    CCompressionMethodMode &methodMode,
    CObjectVector<COneMethodInfo> &methodsInfo
    #ifdef COMPRESS_MT
    , UInt32 numThreads
    #endif
    )
{
  UInt32 level = _level;
  
  if (methodsInfo.IsEmpty())
  {
    COneMethodInfo oneMethodInfo;
    oneMethodInfo.MethodName = ((level == 0) ? kCopyMethod : kDefaultMethodName);
    methodsInfo.Add(oneMethodInfo);
  }

  bool needSolid = false;
  for(int i = 0; i < methodsInfo.Size(); i++)
  {
    COneMethodInfo &oneMethodInfo = methodsInfo[i];
    SetCompressionMethod2(oneMethodInfo
      #ifdef COMPRESS_MT
      , numThreads
      #endif
      );

    if (!IsCopyMethod(oneMethodInfo.MethodName))
      needSolid = true;

    CMethodFull methodFull;

    if (!FindMethod(
        EXTERNAL_CODECS_VARS
        oneMethodInfo.MethodName, methodFull.Id, methodFull.NumInStreams, methodFull.NumOutStreams))
      return E_INVALIDARG;
    methodFull.Props = oneMethodInfo.Props;
    methodMode.Methods.Add(methodFull);

    if (!_numSolidBytesDefined)
    {
      for (int j = 0; j < methodFull.Props.Size(); j++)
      {
        const CProp &prop = methodFull.Props[j];
        if ((prop.Id == NCoderPropID::kDictionarySize ||
             prop.Id == NCoderPropID::kUsedMemorySize) && prop.Value.vt == VT_UI4)
        {
          _numSolidBytes = ((UInt64)prop.Value.ulVal) << 7;
          const UInt64 kMinSize = (1 << 24);
          if (_numSolidBytes < kMinSize)
            _numSolidBytes = kMinSize;
          _numSolidBytesDefined = true;
          break;
        }
      }
    }
  }

  if (!needSolid && !_numSolidBytesDefined)
  {
    _numSolidBytesDefined = true;
    _numSolidBytes  = 0;
  }
  return S_OK;
}

static HRESULT GetTime(IArchiveUpdateCallback *updateCallback, int index, bool writeTime, PROPID propID, UInt64 &ft, bool &ftDefined)
{
  ft = 0;
  ftDefined = false;
  if (!writeTime)
    return S_OK;
  NCOM::CPropVariant prop;
  RINOK(updateCallback->GetProperty(index, propID, &prop));
  if (prop.vt == VT_FILETIME)
  {
    ft = prop.filetime.dwLowDateTime | ((UInt64)prop.filetime.dwHighDateTime << 32);
    ftDefined = true;
  }
  else if (prop.vt != VT_EMPTY)
    return E_INVALIDARG;
  return S_OK;
}

STDMETHODIMP CHandler::UpdateItems(ISequentialOutStream *outStream, UInt32 numItems,
    IArchiveUpdateCallback *updateCallback)
{
  COM_TRY_BEGIN

  const CArchiveDatabaseEx *db = 0;
  #ifdef _7Z_VOL
  if (_volumes.Size() > 1)
    return E_FAIL;
  const CVolume *volume = 0;
  if (_volumes.Size() == 1)
  {
    volume = &_volumes.Front();
    db = &volume->Database;
  }
  #else
  if (_inStream != 0)
    db = &_db;
  #endif

  CObjectVector<CUpdateItem> updateItems;
  
  for (UInt32 i = 0; i < numItems; i++)
  {
    Int32 newData, newProps;
    UInt32 indexInArchive;
    if (!updateCallback)
      return E_FAIL;
    RINOK(updateCallback->GetUpdateItemInfo(i, &newData, &newProps, &indexInArchive));
    CUpdateItem ui;
    ui.NewProps = IntToBool(newProps);
    ui.NewData = IntToBool(newData);
    ui.IndexInArchive = indexInArchive;
    ui.IndexInClient = i;
    ui.IsAnti = false;
    ui.Size = 0;

    if (ui.IndexInArchive != -1)
    {
      if (db == 0 || ui.IndexInArchive >= db->Files.Size())
        return E_INVALIDARG;
      const CFileItem &fi = db->Files[ui.IndexInArchive];
      ui.Name = fi.Name;
      ui.IsDir = fi.IsDir;
      ui.Size = fi.Size;
      ui.IsAnti = db->IsItemAnti(ui.IndexInArchive);
      
      ui.CTimeDefined = db->CTime.GetItem(ui.IndexInArchive, ui.CTime);
      ui.ATimeDefined = db->ATime.GetItem(ui.IndexInArchive, ui.ATime);
      ui.MTimeDefined = db->MTime.GetItem(ui.IndexInArchive, ui.MTime);
    }

    if (ui.NewProps)
    {
      bool nameIsDefined;
      bool folderStatusIsDefined;
      {
        NCOM::CPropVariant prop;
        RINOK(updateCallback->GetProperty(i, kpidAttrib, &prop));
        if (prop.vt == VT_EMPTY)
          ui.AttribDefined = false;
        else if (prop.vt != VT_UI4)
          return E_INVALIDARG;
        else
        {
          ui.Attrib = prop.ulVal;
          ui.AttribDefined = true;
        }
      }
      
      // we need MTime to sort files.
      RINOK(GetTime(updateCallback, i, WriteCTime, kpidCTime, ui.CTime, ui.CTimeDefined));
      RINOK(GetTime(updateCallback, i, WriteATime, kpidATime, ui.ATime, ui.ATimeDefined));
      RINOK(GetTime(updateCallback, i, true,       kpidMTime, ui.MTime, ui.MTimeDefined));

      {
        NCOM::CPropVariant prop;
        RINOK(updateCallback->GetProperty(i, kpidPath, &prop));
        if (prop.vt == VT_EMPTY)
          nameIsDefined = false;
        else if (prop.vt != VT_BSTR)
          return E_INVALIDARG;
        else
        {
          ui.Name = NItemName::MakeLegalName(prop.bstrVal);
          nameIsDefined = true;
        }
      }
      {
        NCOM::CPropVariant prop;
        RINOK(updateCallback->GetProperty(i, kpidIsDir, &prop));
        if (prop.vt == VT_EMPTY)
          folderStatusIsDefined = false;
        else if (prop.vt != VT_BOOL)
          return E_INVALIDARG;
        else
        {
          ui.IsDir = (prop.boolVal != VARIANT_FALSE);
          folderStatusIsDefined = true;
        }
      }

      {
        NCOM::CPropVariant prop;
        RINOK(updateCallback->GetProperty(i, kpidIsAnti, &prop));
        if (prop.vt == VT_EMPTY)
          ui.IsAnti = false;
        else if (prop.vt != VT_BOOL)
          return E_INVALIDARG;
        else
          ui.IsAnti = (prop.boolVal != VARIANT_FALSE);
      }

      if (ui.IsAnti)
      {
        ui.AttribDefined = false;

        ui.CTimeDefined = false;
        ui.ATimeDefined = false;
        ui.MTimeDefined = false;
        
        ui.Size = 0;
      }

      if (!folderStatusIsDefined && ui.AttribDefined)
        ui.SetDirStatusFromAttrib();
    }

    if (ui.NewData)
    {
      NCOM::CPropVariant prop;
      RINOK(updateCallback->GetProperty(i, kpidSize, &prop));
      if (prop.vt != VT_UI8)
        return E_INVALIDARG;
      ui.Size = (UInt64)prop.uhVal.QuadPart;
      if (ui.Size != 0 && ui.IsAnti)
        return E_INVALIDARG;
    }
    updateItems.Add(ui);
  }

  CCompressionMethodMode methodMode, headerMethod;
  RINOK(SetCompressionMethod(methodMode, headerMethod));
  #ifdef COMPRESS_MT
  methodMode.NumThreads = _numThreads;
  headerMethod.NumThreads = 1;
  #endif

  CMyComPtr<ICryptoGetTextPassword2> getPassword2;
  updateCallback->QueryInterface(IID_ICryptoGetTextPassword2, (void **)&getPassword2);

  if (getPassword2)
  {
    CMyComBSTR password;
    Int32 passwordIsDefined;
    RINOK(getPassword2->CryptoGetTextPassword2(&passwordIsDefined, &password));
    methodMode.PasswordIsDefined = IntToBool(passwordIsDefined);
    if (methodMode.PasswordIsDefined)
      methodMode.Password = password;
  }
  else
    methodMode.PasswordIsDefined = false;

  bool compressMainHeader = _compressHeaders;  // check it

  bool encryptHeaders = false;

  if (methodMode.PasswordIsDefined)
  {
    if (_encryptHeadersSpecified)
      encryptHeaders = _encryptHeaders;
    #ifndef _NO_CRYPTO
    else
      encryptHeaders = _passwordIsDefined;
    #endif
    compressMainHeader = true;
    if (encryptHeaders)
    {
      headerMethod.PasswordIsDefined = methodMode.PasswordIsDefined;
      headerMethod.Password = methodMode.Password;
    }
  }

  if (numItems < 2)
    compressMainHeader = false;

  CUpdateOptions options;
  options.Method = &methodMode;
  options.HeaderMethod = (_compressHeaders || encryptHeaders) ? &headerMethod : 0;
  options.UseFilters = _level != 0 && _autoFilter;
  options.MaxFilter = _level >= 8;

  options.HeaderOptions.CompressMainHeader = compressMainHeader;
  options.HeaderOptions.WriteCTime = WriteCTime;
  options.HeaderOptions.WriteATime = WriteATime;
  options.HeaderOptions.WriteMTime = WriteMTime;
  
  options.NumSolidFiles = _numSolidFiles;
  options.NumSolidBytes = _numSolidBytes;
  options.SolidExtension = _solidExtension;
  options.RemoveSfxBlock = _removeSfxBlock;
  options.VolumeMode = _volumeMode;

  COutArchive archive;
  CArchiveDatabase newDatabase;

  CMyComPtr<ICryptoGetTextPassword> getPassword;
  updateCallback->QueryInterface(IID_ICryptoGetTextPassword, (void **)&getPassword);
  
  HRESULT res = Update(
      EXTERNAL_CODECS_VARS
      #ifdef _7Z_VOL
      volume ? volume->Stream: 0,
      volume ? db : 0,
      #else
      _inStream,
      db,
      #endif
      updateItems,
      archive, newDatabase, outStream, updateCallback, options
      #ifndef _NO_CRYPTO
      , getPassword
      #endif
      );

  RINOK(res);

  updateItems.ClearAndFree();

  return archive.WriteDatabase(EXTERNAL_CODECS_VARS
      newDatabase, options.HeaderMethod, options.HeaderOptions);

  COM_TRY_END
}

static HRESULT GetBindInfoPart(UString &srcString, UInt32 &coder, UInt32 &stream)
{
  stream = 0;
  int index = ParseStringToUInt32(srcString, coder);
  if (index == 0)
    return E_INVALIDARG;
  srcString.Delete(0, index);
  if (srcString[0] == 'S')
  {
    srcString.Delete(0);
    int index = ParseStringToUInt32(srcString, stream);
    if (index == 0)
      return E_INVALIDARG;
    srcString.Delete(0, index);
  }
  return S_OK;
}

static HRESULT GetBindInfo(UString &srcString, CBind &bind)
{
  RINOK(GetBindInfoPart(srcString, bind.OutCoder, bind.OutStream));
  if (srcString[0] != ':')
    return E_INVALIDARG;
  srcString.Delete(0);
  RINOK(GetBindInfoPart(srcString, bind.InCoder, bind.InStream));
  if (!srcString.IsEmpty())
    return E_INVALIDARG;
  return S_OK;
}

STDMETHODIMP CHandler::SetProperties(const wchar_t **names, const PROPVARIANT *values, Int32 numProperties)
{
  COM_TRY_BEGIN
  _binds.Clear();
  BeforeSetProperty();

  for (int i = 0; i < numProperties; i++)
  {
    UString name = names[i];
    name.MakeUpper();
    if (name.IsEmpty())
      return E_INVALIDARG;

    const PROPVARIANT &value = values[i];

    if (name[0] == 'B')
    {
      name.Delete(0);
      CBind bind;
      RINOK(GetBindInfo(name, bind));
      _binds.Add(bind);
      continue;
    }

    RINOK(SetProperty(name, value));
  }

  return S_OK;
  COM_TRY_END
}

}}
