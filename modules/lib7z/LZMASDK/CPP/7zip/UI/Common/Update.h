// Update.h

#ifndef __UPDATE_H
#define __UPDATE_H

#include "Common/Wildcard.h"
#include "Windows/FileFind.h"
#include "../../Archive/IArchive.h"

#include "UpdateAction.h"
#include "ArchiveOpenCallback.h"
#include "UpdateCallback.h"
#include "Property.h"
#include "LoadCodecs.h"

struct CArchivePath
{
  UString Prefix;   // path(folder) prefix including slash
  UString Name; // base name
  UString BaseExtension; // archive type extension or "exe" extension
  UString VolExtension;  // archive type extension for volumes

  bool Temp;
  UString TempPrefix;  // path(folder) for temp location
  UString TempPostfix;

  CArchivePath(): Temp(false) {};
  
  void ParseFromPath(const UString &path)
  {
    SplitPathToParts(path, Prefix, Name);
    if (Name.IsEmpty())
      return;
    int dotPos = Name.ReverseFind(L'.');
    if (dotPos <= 0)
      return;
    if (dotPos == Name.Length() - 1)
    {
      Name = Name.Left(dotPos);
      BaseExtension.Empty();
      return;
    }
    if (BaseExtension.CompareNoCase(Name.Mid(dotPos + 1)) == 0)
    {
      BaseExtension = Name.Mid(dotPos + 1);
      Name = Name.Left(dotPos);
    }
    else
      BaseExtension.Empty();
  }

  UString GetPathWithoutExt() const
  {
    return Prefix + Name;
  }

  UString GetFinalPath() const
  {
    UString path = GetPathWithoutExt();
    if (!BaseExtension.IsEmpty())
      path += UString(L'.') + BaseExtension;
    return path;
  }

  
  UString GetTempPath() const
  {
    UString path = TempPrefix + Name;
    if (!BaseExtension.IsEmpty())
      path += UString(L'.') + BaseExtension;
    path += L".tmp";
    path += TempPostfix;
    return path;
  }
};

struct CUpdateArchiveCommand
{
  UString UserArchivePath;
  CArchivePath ArchivePath;
  NUpdateArchive::CActionSet ActionSet;
};

struct CCompressionMethodMode
{
  int FormatIndex;
  CObjectVector<CProperty> Properties;
  CCompressionMethodMode(): FormatIndex(-1) {}
};

struct CUpdateOptions
{
  CCompressionMethodMode MethodMode;

  CObjectVector<CUpdateArchiveCommand> Commands;
  bool UpdateArchiveItself;
  CArchivePath ArchivePath;
  
  bool SfxMode;
  UString SfxModule;
  
  bool OpenShareForWrite;

  bool StdInMode;
  UString StdInFileName;
  bool StdOutMode;
  
  bool EMailMode;
  bool EMailRemoveAfter;
  UString EMailAddress;

  UString WorkingDir;

  bool Init(const CCodecs *codecs, const CIntVector &formatIndices, const UString &arcPath);

  CUpdateOptions():
    UpdateArchiveItself(true),
    SfxMode(false),
    StdInMode(false),
    StdOutMode(false),
    EMailMode(false),
    EMailRemoveAfter(false),
    OpenShareForWrite(false)
      {};

  void SetAddActionCommand()
  {
    Commands.Clear();
    CUpdateArchiveCommand c;
    c.ActionSet = NUpdateArchive::kAddActionSet;
    Commands.Add(c);
  }

  CRecordVector<UInt64> VolumesSizes;
};

struct CErrorInfo
{
  DWORD SystemError;
  UString FileName;
  UString FileName2;
  UString Message;
  // UStringVector ErrorPaths;
  // CRecordVector<DWORD> ErrorCodes;
  CErrorInfo(): SystemError(0) {};
};

struct CUpdateErrorInfo: public CErrorInfo
{
};

#define INTERFACE_IUpdateCallbackUI2(x) \
  INTERFACE_IUpdateCallbackUI(x) \
  virtual HRESULT OpenResult(const wchar_t *name, HRESULT result) x; \
  virtual HRESULT StartScanning() x; \
  virtual HRESULT ScanProgress(UInt64 numFolders, UInt64 numFiles, const wchar_t *path) x; \
  virtual HRESULT CanNotFindError(const wchar_t *name, DWORD systemError) x; \
  virtual HRESULT FinishScanning() x; \
  virtual HRESULT StartArchive(const wchar_t *name, bool updating) x; \
  virtual HRESULT FinishArchive() x; \

struct IUpdateCallbackUI2: public IUpdateCallbackUI
{
  INTERFACE_IUpdateCallbackUI2(=0)
};

HRESULT UpdateArchive(
    CCodecs *codecs,
    const NWildcard::CCensor &censor,
    CUpdateOptions &options,
    CUpdateErrorInfo &errorInfo,
    IOpenCallbackUI *openCallback,
    IUpdateCallbackUI2 *callback);

#endif
