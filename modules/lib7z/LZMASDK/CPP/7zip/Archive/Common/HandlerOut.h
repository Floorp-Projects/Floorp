// HandlerOut.h

#ifndef __HANDLER_OUT_H
#define __HANDLER_OUT_H

#include "../../../Common/MyString.h"
#include "../../Common/MethodProps.h"

namespace NArchive {

struct COneMethodInfo
{
  CObjectVector<CProp> Props;
  UString MethodName;

  bool IsLzma() const;
};

class COutHandler
{
public:
  HRESULT SetProperty(const wchar_t *name, const PROPVARIANT &value);
  
  HRESULT SetSolidSettings(const UString &s);
  HRESULT SetSolidSettings(const PROPVARIANT &value);

  #ifdef COMPRESS_MT
  UInt32 _numThreads;
  #endif

  UInt32 _crcSize;

  CObjectVector<COneMethodInfo> _methods;
  bool _removeSfxBlock;
  
  UInt64 _numSolidFiles;
  UInt64 _numSolidBytes;
  bool _numSolidBytesDefined;
  bool _solidExtension;

  bool _compressHeaders;
  bool _encryptHeadersSpecified;
  bool _encryptHeaders;

  bool WriteCTime;
  bool WriteATime;
  bool WriteMTime;

  bool _autoFilter;
  UInt32 _level;

  bool _volumeMode;

  HRESULT SetParam(COneMethodInfo &oneMethodInfo, const UString &name, const UString &value);
  HRESULT SetParams(COneMethodInfo &oneMethodInfo, const UString &srcString);

  void SetCompressionMethod2(COneMethodInfo &oneMethodInfo
      #ifdef COMPRESS_MT
      , UInt32 numThreads
      #endif
      );

  void InitSolidFiles() { _numSolidFiles = (UInt64)(Int64)(-1); }
  void InitSolidSize()  { _numSolidBytes = (UInt64)(Int64)(-1); }
  void InitSolid()
  {
    InitSolidFiles();
    InitSolidSize();
    _solidExtension = false;
    _numSolidBytesDefined = false;
  }

  void Init();

  COutHandler() { Init(); }

  void BeforeSetProperty();

  UInt32 minNumber;
  UInt32 numProcessors;
  UInt32 mainDicSize;
  UInt32 mainDicMethodIndex;
};

}

#endif
