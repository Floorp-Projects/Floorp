// Windows/FileDir.h

#ifndef __WINDOWS_FILEDIR_H
#define __WINDOWS_FILEDIR_H

#include "../Common/MyString.h"
#include "Defs.h"

namespace NWindows {
namespace NFile {
namespace NDirectory {

#ifdef WIN_LONG_PATH
bool GetLongPaths(LPCWSTR s1, LPCWSTR s2, UString &d1, UString &d2);
#endif

bool MyGetWindowsDirectory(CSysString &path);
bool MyGetSystemDirectory(CSysString &path);
#ifndef _UNICODE
bool MyGetWindowsDirectory(UString &path);
bool MyGetSystemDirectory(UString &path);
#endif

bool SetDirTime(LPCWSTR fileName, const FILETIME *cTime, const FILETIME *aTime, const FILETIME *mTime);

bool MySetFileAttributes(LPCTSTR fileName, DWORD fileAttributes);
bool MyMoveFile(LPCTSTR existFileName, LPCTSTR newFileName);
bool MyRemoveDirectory(LPCTSTR pathName);
bool MyCreateDirectory(LPCTSTR pathName);
bool CreateComplexDirectory(LPCTSTR pathName);
bool DeleteFileAlways(LPCTSTR name);
bool RemoveDirectoryWithSubItems(const CSysString &path);

#ifndef _UNICODE
bool MySetFileAttributes(LPCWSTR fileName, DWORD fileAttributes);
bool MyMoveFile(LPCWSTR existFileName, LPCWSTR newFileName);
bool MyRemoveDirectory(LPCWSTR pathName);
bool MyCreateDirectory(LPCWSTR pathName);
bool CreateComplexDirectory(LPCWSTR pathName);
bool DeleteFileAlways(LPCWSTR name);
bool RemoveDirectoryWithSubItems(const UString &path);
#endif

bool GetOnlyDirPrefix(LPCTSTR fileName, CSysString &resultName);
bool GetOnlyName(LPCTSTR fileName, CSysString &resultName);
#ifdef UNDER_CE
bool MyGetFullPathName(LPCWSTR fileName, UString &resultPath);
bool MyGetFullPathName(LPCWSTR fileName, UString &resultPath, int &fileNamePartStartIndex);
#else
bool MyGetShortPathName(LPCTSTR longPath, CSysString &shortPath);

bool MyGetFullPathName(LPCTSTR fileName, CSysString &resultPath, int &fileNamePartStartIndex);
bool MyGetFullPathName(LPCTSTR fileName, CSysString &resultPath);
#ifndef _UNICODE
bool MyGetFullPathName(LPCWSTR fileName, UString &resultPath,
    int &fileNamePartStartIndex);
bool MyGetFullPathName(LPCWSTR fileName, UString &resultPath);
bool GetOnlyName(LPCWSTR fileName, UString &resultName);
bool GetOnlyDirPrefix(LPCWSTR fileName, UString &resultName);
#endif

inline bool MySetCurrentDirectory(LPCTSTR path)
  { return BOOLToBool(::SetCurrentDirectory(path)); }
bool MyGetCurrentDirectory(CSysString &resultPath);
#ifndef _UNICODE
bool MySetCurrentDirectory(LPCWSTR path);
bool MyGetCurrentDirectory(UString &resultPath);
#endif

bool MySearchPath(LPCTSTR path, LPCTSTR fileName, LPCTSTR extension, CSysString &resultPath, UINT32 &filePart);
#ifndef _UNICODE
bool MySearchPath(LPCWSTR path, LPCWSTR fileName, LPCWSTR extension, UString &resultPath, UINT32 &filePart);
#endif

inline bool MySearchPath(LPCTSTR path, LPCTSTR fileName, LPCTSTR extension, CSysString &resultPath)
{
  UINT32 value;
  return MySearchPath(path, fileName, extension, resultPath, value);
}

#ifndef _UNICODE
inline bool MySearchPath(LPCWSTR path, LPCWSTR fileName, LPCWSTR extension, UString &resultPath)
{
  UINT32 value;
  return MySearchPath(path, fileName, extension, resultPath, value);
}
#endif

#endif

bool MyGetTempPath(CSysString &resultPath);
#ifndef _UNICODE
bool MyGetTempPath(UString &resultPath);
#endif

UINT MyGetTempFileName(LPCTSTR dirPath, LPCTSTR prefix, CSysString &resultPath);
#ifndef _UNICODE
UINT MyGetTempFileName(LPCWSTR dirPath, LPCWSTR prefix, UString &resultPath);
#endif

class CTempFile
{
  bool _mustBeDeleted;
  CSysString _fileName;
public:
  CTempFile(): _mustBeDeleted(false) {}
  ~CTempFile() { Remove(); }
  void DisableDeleting() { _mustBeDeleted = false; }
  UINT Create(LPCTSTR dirPath, LPCTSTR prefix, CSysString &resultPath);
  bool Create(LPCTSTR prefix, CSysString &resultPath);
  bool Remove();
};

#ifdef _UNICODE
typedef CTempFile CTempFileW;
#else
class CTempFileW
{
  bool _mustBeDeleted;
  UString _fileName;
public:
  CTempFileW(): _mustBeDeleted(false) {}
  ~CTempFileW() { Remove(); }
  void DisableDeleting() { _mustBeDeleted = false; }
  UINT Create(LPCWSTR dirPath, LPCWSTR prefix, UString &resultPath);
  bool Create(LPCWSTR prefix, UString &resultPath);
  bool Remove();
};
#endif

bool CreateTempDirectory(LPCTSTR prefixChars, CSysString &dirName);

class CTempDirectory
{
  bool _mustBeDeleted;
  CSysString _tempDir;
public:
  const CSysString &GetPath() const { return _tempDir; }
  CTempDirectory(): _mustBeDeleted(false) {}
  ~CTempDirectory() { Remove();  }
  bool Create(LPCTSTR prefix) ;
  bool Remove()
  {
    if (!_mustBeDeleted)
      return true;
    _mustBeDeleted = !RemoveDirectoryWithSubItems(_tempDir);
    return (!_mustBeDeleted);
  }
  void DisableDeleting() { _mustBeDeleted = false; }
};

#ifdef _UNICODE
typedef CTempDirectory CTempDirectoryW;
#else
class CTempDirectoryW
{
  bool _mustBeDeleted;
  UString _tempDir;
public:
  const UString &GetPath() const { return _tempDir; }
  CTempDirectoryW(): _mustBeDeleted(false) {}
  ~CTempDirectoryW() { Remove();  }
  bool Create(LPCWSTR prefix) ;
  bool Remove()
  {
    if (!_mustBeDeleted)
      return true;
    _mustBeDeleted = !RemoveDirectoryWithSubItems(_tempDir);
    return (!_mustBeDeleted);
  }
  void DisableDeleting() { _mustBeDeleted = false; }
};
#endif

}}}

#endif
