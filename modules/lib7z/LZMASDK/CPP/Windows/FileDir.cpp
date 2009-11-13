// Windows/FileDir.cpp

#include "StdAfx.h"

#ifndef _UNICODE
#include "../Common/StringConvert.h"
#endif

#include "FileDir.h"
#include "FileFind.h"
#include "FileName.h"

#ifndef _UNICODE
extern bool g_IsNT;
#endif

namespace NWindows {
namespace NFile {

#if defined(WIN_LONG_PATH) && defined(_UNICODE)
#define WIN_LONG_PATH2
#endif

// SetCurrentDirectory doesn't support \\?\ prefix

#ifdef WIN_LONG_PATH
bool GetLongPathBase(LPCWSTR fileName, UString &res);
bool GetLongPath(LPCWSTR fileName, UString &res);
#endif

namespace NDirectory {

#ifndef _UNICODE
static inline UINT GetCurrentCodePage() { return ::AreFileApisANSI() ? CP_ACP : CP_OEMCP; }
static UString GetUnicodePath(const CSysString &sysPath)
  { return MultiByteToUnicodeString(sysPath, GetCurrentCodePage()); }
static CSysString GetSysPath(LPCWSTR sysPath)
  { return UnicodeStringToMultiByte(sysPath, GetCurrentCodePage()); }
#endif

#ifndef UNDER_CE

bool MyGetWindowsDirectory(CSysString &path)
{
  UINT needLength = ::GetWindowsDirectory(path.GetBuffer(MAX_PATH + 1), MAX_PATH + 1);
  path.ReleaseBuffer();
  return (needLength > 0 && needLength <= MAX_PATH);
}

bool MyGetSystemDirectory(CSysString &path)
{
  UINT needLength = ::GetSystemDirectory(path.GetBuffer(MAX_PATH + 1), MAX_PATH + 1);
  path.ReleaseBuffer();
  return (needLength > 0 && needLength <= MAX_PATH);
}

#endif

#ifndef _UNICODE
bool MyGetWindowsDirectory(UString &path)
{
  if (g_IsNT)
  {
    UINT needLength = ::GetWindowsDirectoryW(path.GetBuffer(MAX_PATH + 1), MAX_PATH + 1);
    path.ReleaseBuffer();
    return (needLength > 0 && needLength <= MAX_PATH);
  }
  CSysString sysPath;
  if (!MyGetWindowsDirectory(sysPath))
    return false;
  path = GetUnicodePath(sysPath);
  return true;
}

bool MyGetSystemDirectory(UString &path)
{
  if (g_IsNT)
  {
    UINT needLength = ::GetSystemDirectoryW(path.GetBuffer(MAX_PATH + 1), MAX_PATH + 1);
    path.ReleaseBuffer();
    return (needLength > 0 && needLength <= MAX_PATH);
  }
  CSysString sysPath;
  if (!MyGetSystemDirectory(sysPath))
    return false;
  path = GetUnicodePath(sysPath);
  return true;
}
#endif

bool SetDirTime(LPCWSTR fileName, const FILETIME *cTime, const FILETIME *aTime, const FILETIME *mTime)
{
  #ifndef _UNICODE
  if (!g_IsNT)
  {
    ::SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return false;
  }
  #endif
  HANDLE hDir = ::CreateFileW(fileName, GENERIC_WRITE,
      FILE_SHARE_READ | FILE_SHARE_WRITE,
      NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
  #ifdef WIN_LONG_PATH
  if (hDir == INVALID_HANDLE_VALUE)
  {
    UString longPath;
    if (GetLongPath(fileName, longPath))
      hDir = ::CreateFileW(longPath, GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
  }
  #endif

  bool res = false;
  if (hDir != INVALID_HANDLE_VALUE)
  {
    res = BOOLToBool(::SetFileTime(hDir, cTime, aTime, mTime));
    ::CloseHandle(hDir);
  }
  return res;
}

bool MySetFileAttributes(LPCTSTR fileName, DWORD fileAttributes)
{
  if (::SetFileAttributes(fileName, fileAttributes))
    return true;
  #ifdef WIN_LONG_PATH2
  UString longPath;
  if (GetLongPath(fileName, longPath))
    return BOOLToBool(::SetFileAttributesW(longPath, fileAttributes));
  #endif
  return false;
}

bool MyRemoveDirectory(LPCTSTR pathName)
{
  if (::RemoveDirectory(pathName))
    return true;
  #ifdef WIN_LONG_PATH2
  UString longPath;
  if (GetLongPath(pathName, longPath))
    return BOOLToBool(::RemoveDirectoryW(longPath));
  #endif
  return false;
}

#ifdef WIN_LONG_PATH
bool GetLongPaths(LPCWSTR s1, LPCWSTR s2, UString &d1, UString &d2)
{
  if (!GetLongPathBase(s1, d1) || !GetLongPathBase(s2, d2))
    return false;
  if (d1.IsEmpty() && d2.IsEmpty()) return false;
  if (d1.IsEmpty()) d1 = s1;
  if (d2.IsEmpty()) d2 = s2;
  return true;
}
#endif

bool MyMoveFile(LPCTSTR existFileName, LPCTSTR newFileName)
{
  if (::MoveFile(existFileName, newFileName))
    return true;
  #ifdef WIN_LONG_PATH2
  UString d1, d2;
  if (GetLongPaths(existFileName, newFileName, d1, d2))
    return BOOLToBool(::MoveFileW(d1, d2));
  #endif
  return false;
}

#ifndef _UNICODE
bool MySetFileAttributes(LPCWSTR fileName, DWORD fileAttributes)
{
  if (!g_IsNT)
    return MySetFileAttributes(GetSysPath(fileName), fileAttributes);
  if (::SetFileAttributesW(fileName, fileAttributes))
    return true;
  #ifdef WIN_LONG_PATH
  UString longPath;
  if (GetLongPath(fileName, longPath))
    return BOOLToBool(::SetFileAttributesW(longPath, fileAttributes));
  #endif
  return false;
}


bool MyRemoveDirectory(LPCWSTR pathName)
{
  if (!g_IsNT)
    return MyRemoveDirectory(GetSysPath(pathName));
  if (::RemoveDirectoryW(pathName))
    return true;
  #ifdef WIN_LONG_PATH
  UString longPath;
  if (GetLongPath(pathName, longPath))
    return BOOLToBool(::RemoveDirectoryW(longPath));
  #endif
  return false;
}

bool MyMoveFile(LPCWSTR existFileName, LPCWSTR newFileName)
{
  if (!g_IsNT)
    return MyMoveFile(GetSysPath(existFileName), GetSysPath(newFileName));
  if (::MoveFileW(existFileName, newFileName))
    return true;
  #ifdef WIN_LONG_PATH
  UString d1, d2;
  if (GetLongPaths(existFileName, newFileName, d1, d2))
    return BOOLToBool(::MoveFileW(d1, d2));
  #endif
  return false;
}
#endif

bool MyCreateDirectory(LPCTSTR pathName)
{
  if (::CreateDirectory(pathName, NULL))
    return true;
  #ifdef WIN_LONG_PATH2
  if (::GetLastError() != ERROR_ALREADY_EXISTS)
  {
    UString longPath;
    if (GetLongPath(pathName, longPath))
      return BOOLToBool(::CreateDirectoryW(longPath, NULL));
  }
  #endif
  return false;
}

#ifndef _UNICODE
bool MyCreateDirectory(LPCWSTR pathName)
{
  if (!g_IsNT)
    return MyCreateDirectory(GetSysPath(pathName));
  if (::CreateDirectoryW(pathName, NULL))
    return true;
  #ifdef WIN_LONG_PATH
  if (::GetLastError() != ERROR_ALREADY_EXISTS)
  {
    UString longPath;
    if (GetLongPath(pathName, longPath))
      return BOOLToBool(::CreateDirectoryW(longPath, NULL));
  }
  #endif
  return false;
}
#endif

/*
bool CreateComplexDirectory(LPCTSTR pathName)
{
  NName::CParsedPath path;
  path.ParsePath(pathName);
  CSysString fullPath = path.Prefix;
  DWORD errorCode = ERROR_SUCCESS;
  for (int i = 0; i < path.PathParts.Size(); i++)
  {
    const CSysString &string = path.PathParts[i];
    if (string.IsEmpty())
    {
      if (i != path.PathParts.Size() - 1)
        return false;
      return true;
    }
    fullPath += path.PathParts[i];
    if (!MyCreateDirectory(fullPath))
    {
      DWORD errorCode = GetLastError();
      if (errorCode != ERROR_ALREADY_EXISTS)
        return false;
    }
    fullPath += NName::kDirDelimiter;
  }
  return true;
}
*/

bool CreateComplexDirectory(LPCTSTR _aPathName)
{
  CSysString pathName = _aPathName;
  int pos = pathName.ReverseFind(TEXT(CHAR_PATH_SEPARATOR));
  if (pos > 0 && pos == pathName.Length() - 1)
  {
    if (pathName.Length() == 3 && pathName[1] == ':')
      return true; // Disk folder;
    pathName.Delete(pos);
  }
  CSysString pathName2 = pathName;
  pos = pathName.Length();
  for (;;)
  {
    if (MyCreateDirectory(pathName))
      break;
    if (::GetLastError() == ERROR_ALREADY_EXISTS)
    {
      NFind::CFileInfo fileInfo;
      if (!fileInfo.Find(pathName)) // For network folders
        return true;
      if (!fileInfo.IsDir())
        return false;
      break;
    }
    pos = pathName.ReverseFind(TEXT(CHAR_PATH_SEPARATOR));
    if (pos < 0 || pos == 0)
      return false;
    if (pathName[pos - 1] == ':')
      return false;
    pathName = pathName.Left(pos);
  }
  pathName = pathName2;
  while (pos < pathName.Length())
  {
    pos = pathName.Find(TEXT(CHAR_PATH_SEPARATOR), pos + 1);
    if (pos < 0)
      pos = pathName.Length();
    if (!MyCreateDirectory(pathName.Left(pos)))
      return false;
  }
  return true;
}

#ifndef _UNICODE

bool CreateComplexDirectory(LPCWSTR _aPathName)
{
  UString pathName = _aPathName;
  int pos = pathName.ReverseFind(WCHAR_PATH_SEPARATOR);
  if (pos > 0 && pos == pathName.Length() - 1)
  {
    if (pathName.Length() == 3 && pathName[1] == L':')
      return true; // Disk folder;
    pathName.Delete(pos);
  }
  UString pathName2 = pathName;
  pos = pathName.Length();
  for (;;)
  {
    if (MyCreateDirectory(pathName))
      break;
    if (::GetLastError() == ERROR_ALREADY_EXISTS)
    {
      NFind::CFileInfoW fileInfo;
      if (!fileInfo.Find(pathName)) // For network folders
        return true;
      if (!fileInfo.IsDir())
        return false;
      break;
    }
    pos = pathName.ReverseFind(WCHAR_PATH_SEPARATOR);
    if (pos < 0 || pos == 0)
      return false;
    if (pathName[pos - 1] == L':')
      return false;
    pathName = pathName.Left(pos);
  }
  pathName = pathName2;
  while (pos < pathName.Length())
  {
    pos = pathName.Find(WCHAR_PATH_SEPARATOR, pos + 1);
    if (pos < 0)
      pos = pathName.Length();
    if (!MyCreateDirectory(pathName.Left(pos)))
      return false;
  }
  return true;
}

#endif

bool DeleteFileAlways(LPCTSTR name)
{
  if (!MySetFileAttributes(name, 0))
    return false;
  if (::DeleteFile(name))
    return true;
  #ifdef WIN_LONG_PATH2
  UString longPath;
  if (GetLongPath(name, longPath))
    return BOOLToBool(::DeleteFileW(longPath));
  #endif
  return false;
}

#ifndef _UNICODE
bool DeleteFileAlways(LPCWSTR name)
{
  if (!g_IsNT)
    return DeleteFileAlways(GetSysPath(name));
  if (!MySetFileAttributes(name, 0))
    return false;
  if (::DeleteFileW(name))
    return true;
  #ifdef WIN_LONG_PATH
  UString longPath;
  if (GetLongPath(name, longPath))
    return BOOLToBool(::DeleteFileW(longPath));
  #endif
  return false;
}
#endif

static bool RemoveDirectorySubItems2(const CSysString pathPrefix, const NFind::CFileInfo &fileInfo)
{
  if (fileInfo.IsDir())
    return RemoveDirectoryWithSubItems(pathPrefix + fileInfo.Name);
  return DeleteFileAlways(pathPrefix + fileInfo.Name);
}

bool RemoveDirectoryWithSubItems(const CSysString &path)
{
  NFind::CFileInfo fileInfo;
  CSysString pathPrefix = path + NName::kDirDelimiter;
  {
    NFind::CEnumerator enumerator(pathPrefix + TCHAR(NName::kAnyStringWildcard));
    while (enumerator.Next(fileInfo))
      if (!RemoveDirectorySubItems2(pathPrefix, fileInfo))
        return false;
  }
  if (!MySetFileAttributes(path, 0))
    return false;
  return MyRemoveDirectory(path);
}

#ifndef _UNICODE
static bool RemoveDirectorySubItems2(const UString pathPrefix, const NFind::CFileInfoW &fileInfo)
{
  if (fileInfo.IsDir())
    return RemoveDirectoryWithSubItems(pathPrefix + fileInfo.Name);
  return DeleteFileAlways(pathPrefix + fileInfo.Name);
}
bool RemoveDirectoryWithSubItems(const UString &path)
{
  NFind::CFileInfoW fileInfo;
  UString pathPrefix = path + UString(NName::kDirDelimiter);
  {
    NFind::CEnumeratorW enumerator(pathPrefix + UString(NName::kAnyStringWildcard));
    while (enumerator.Next(fileInfo))
      if (!RemoveDirectorySubItems2(pathPrefix, fileInfo))
        return false;
  }
  if (!MySetFileAttributes(path, 0))
    return false;
  return MyRemoveDirectory(path);
}
#endif

bool GetOnlyDirPrefix(LPCTSTR fileName, CSysString &resultName)
{
  int index;
  if (!MyGetFullPathName(fileName, resultName, index))
    return false;
  resultName = resultName.Left(index);
  return true;
}

bool GetOnlyName(LPCTSTR fileName, CSysString &resultName)
{
  int index;
  if (!MyGetFullPathName(fileName, resultName, index))
    return false;
  resultName = resultName.Mid(index);
  return true;
}

#ifdef UNDER_CE
bool MyGetFullPathName(LPCWSTR fileName, UString &resultPath)
{
  resultPath = fileName;
  return true;
}

bool MyGetFullPathName(LPCWSTR fileName, UString &resultPath, int &fileNamePartStartIndex)
{
  resultPath = fileName;
  // change it
  fileNamePartStartIndex = resultPath.ReverseFind('\\');
  fileNamePartStartIndex++;
  return true;
}

#else

bool MyGetShortPathName(LPCTSTR longPath, CSysString &shortPath)
{
  DWORD needLength = ::GetShortPathName(longPath, shortPath.GetBuffer(MAX_PATH + 1), MAX_PATH + 1);
  shortPath.ReleaseBuffer();
  return (needLength > 0 && needLength < MAX_PATH);
}

bool MyGetFullPathName(LPCTSTR fileName, CSysString &resultPath, int &fileNamePartStartIndex)
{
  resultPath.Empty();
  LPTSTR fileNamePointer = 0;
  LPTSTR buffer = resultPath.GetBuffer(MAX_PATH);
  DWORD needLength = ::GetFullPathName(fileName, MAX_PATH + 1, buffer, &fileNamePointer);
  resultPath.ReleaseBuffer();
  if (needLength == 0)
    return false;
  if (needLength >= MAX_PATH)
  {
    #ifdef WIN_LONG_PATH2
    needLength++;
    buffer = resultPath.GetBuffer(needLength + 1);
    DWORD needLength2 = ::GetFullPathNameW(fileName, needLength, buffer, &fileNamePointer);
    resultPath.ReleaseBuffer();
    if (needLength2 == 0 || needLength2 > needLength)
    #endif
      return false;
  }
  if (fileNamePointer == 0)
    fileNamePartStartIndex = lstrlen(fileName);
  else
    fileNamePartStartIndex = (int)(fileNamePointer - buffer);
  return true;
}

#ifndef _UNICODE
bool MyGetFullPathName(LPCWSTR fileName, UString &resultPath, int &fileNamePartStartIndex)
{
  resultPath.Empty();
  if (g_IsNT)
  {
    LPWSTR fileNamePointer = 0;
    LPWSTR buffer = resultPath.GetBuffer(MAX_PATH);
    DWORD needLength = ::GetFullPathNameW(fileName, MAX_PATH + 1, buffer, &fileNamePointer);
    resultPath.ReleaseBuffer();
    if (needLength == 0)
      return false;
    if (needLength >= MAX_PATH)
    {
      #ifdef WIN_LONG_PATH
      needLength++;
      buffer = resultPath.GetBuffer(needLength + 1);
      DWORD needLength2 = ::GetFullPathNameW(fileName, needLength, buffer, &fileNamePointer);
      resultPath.ReleaseBuffer();
      if (needLength2 == 0 || needLength2 > needLength)
      #endif
        return false;
    }
    if (fileNamePointer == 0)
      fileNamePartStartIndex = MyStringLen(fileName);
    else
      fileNamePartStartIndex = (int)(fileNamePointer - buffer);
  }
  else
  {
    CSysString sysPath;
    if (!MyGetFullPathName(GetSysPath(fileName), sysPath, fileNamePartStartIndex))
      return false;
    UString resultPath1 = GetUnicodePath(sysPath.Left(fileNamePartStartIndex));
    UString resultPath2 = GetUnicodePath(sysPath.Mid(fileNamePartStartIndex));
    fileNamePartStartIndex = resultPath1.Length();
    resultPath = resultPath1 + resultPath2;
  }
  return true;
}
#endif


bool MyGetFullPathName(LPCTSTR fileName, CSysString &path)
{
  int index;
  return MyGetFullPathName(fileName, path, index);
}

#ifndef _UNICODE
bool MyGetFullPathName(LPCWSTR fileName, UString &path)
{
  int index;
  return MyGetFullPathName(fileName, path, index);
}
#endif

#ifndef _UNICODE
bool GetOnlyName(LPCWSTR fileName, UString &resultName)
{
  int index;
  if (!MyGetFullPathName(fileName, resultName, index))
    return false;
  resultName = resultName.Mid(index);
  return true;
}
#endif

#ifndef _UNICODE
bool GetOnlyDirPrefix(LPCWSTR fileName, UString &resultName)
{
  int index;
  if (!MyGetFullPathName(fileName, resultName, index))
    return false;
  resultName = resultName.Left(index);
  return true;
}
#endif

bool MyGetCurrentDirectory(CSysString &path)
{
  DWORD needLength = ::GetCurrentDirectory(MAX_PATH + 1, path.GetBuffer(MAX_PATH + 1));
  path.ReleaseBuffer();
  return (needLength > 0 && needLength <= MAX_PATH);
}

#ifndef _UNICODE
bool MySetCurrentDirectory(LPCWSTR path)
{
  if (g_IsNT)
    return BOOLToBool(::SetCurrentDirectoryW(path));
  return MySetCurrentDirectory(GetSysPath(path));
}
bool MyGetCurrentDirectory(UString &path)
{
  if (g_IsNT)
  {
    DWORD needLength = ::GetCurrentDirectoryW(MAX_PATH + 1, path.GetBuffer(MAX_PATH + 1));
    path.ReleaseBuffer();
    return (needLength > 0 && needLength <= MAX_PATH);
  }
  CSysString sysPath;
  if (!MyGetCurrentDirectory(sysPath))
    return false;
  path = GetUnicodePath(sysPath);
  return true;
}
#endif

bool MySearchPath(LPCTSTR path, LPCTSTR fileName, LPCTSTR extension,
  CSysString &resultPath, UINT32 &filePart)
{
  LPTSTR filePartPointer;
  DWORD value = ::SearchPath(path, fileName, extension,
    MAX_PATH, resultPath.GetBuffer(MAX_PATH + 1), &filePartPointer);
  filePart = (UINT32)(filePartPointer - (LPCTSTR)resultPath);
  resultPath.ReleaseBuffer();
  return (value > 0 && value <= MAX_PATH);
}
#endif

#ifndef _UNICODE
bool MySearchPath(LPCWSTR path, LPCWSTR fileName, LPCWSTR extension,
  UString &resultPath, UINT32 &filePart)
{
  if (g_IsNT)
  {
    LPWSTR filePartPointer = 0;
    DWORD value = ::SearchPathW(path, fileName, extension,
        MAX_PATH, resultPath.GetBuffer(MAX_PATH + 1), &filePartPointer);
    filePart = (UINT32)(filePartPointer - (LPCWSTR)resultPath);
    resultPath.ReleaseBuffer();
    return (value > 0 && value <= MAX_PATH);
  }
  
  CSysString sysPath;
  if (!MySearchPath(
      path != 0 ? (LPCTSTR)GetSysPath(path): 0,
      fileName != 0 ? (LPCTSTR)GetSysPath(fileName): 0,
      extension != 0 ? (LPCTSTR)GetSysPath(extension): 0,
      sysPath, filePart))
    return false;
  UString resultPath1 = GetUnicodePath(sysPath.Left(filePart));
  UString resultPath2 = GetUnicodePath(sysPath.Mid(filePart));
  filePart = resultPath1.Length();
  resultPath = resultPath1 + resultPath2;
  return true;
}
#endif

bool MyGetTempPath(CSysString &path)
{
  DWORD needLength = ::GetTempPath(MAX_PATH + 1, path.GetBuffer(MAX_PATH + 1));
  path.ReleaseBuffer();
  return (needLength > 0 && needLength <= MAX_PATH);
}

#ifndef _UNICODE
bool MyGetTempPath(UString &path)
{
  path.Empty();
  if (g_IsNT)
  {
    DWORD needLength = ::GetTempPathW(MAX_PATH + 1, path.GetBuffer(MAX_PATH + 1));
    path.ReleaseBuffer();
    return (needLength > 0 && needLength <= MAX_PATH);
  }
  CSysString sysPath;
  if (!MyGetTempPath(sysPath))
    return false;
  path = GetUnicodePath(sysPath);
  return true;
}
#endif

UINT MyGetTempFileName(LPCTSTR dirPath, LPCTSTR prefix, CSysString &path)
{
  UINT number = ::GetTempFileName(dirPath, prefix, 0, path.GetBuffer(MAX_PATH + 1));
  path.ReleaseBuffer();
  return number;
}

#ifndef _UNICODE
UINT MyGetTempFileName(LPCWSTR dirPath, LPCWSTR prefix, UString &path)
{
  if (g_IsNT)
  {
    UINT number = ::GetTempFileNameW(dirPath, prefix, 0, path.GetBuffer(MAX_PATH));
    path.ReleaseBuffer();
    return number;
  }
  CSysString sysPath;
  UINT number = MyGetTempFileName(
      dirPath ? (LPCTSTR)GetSysPath(dirPath): 0,
      prefix ? (LPCTSTR)GetSysPath(prefix): 0,
      sysPath);
  path = GetUnicodePath(sysPath);
  return number;
}
#endif

UINT CTempFile::Create(LPCTSTR dirPath, LPCTSTR prefix, CSysString &resultPath)
{
  Remove();
  UINT number = MyGetTempFileName(dirPath, prefix, resultPath);
  if (number != 0)
  {
    _fileName = resultPath;
    _mustBeDeleted = true;
  }
  return number;
}

bool CTempFile::Create(LPCTSTR prefix, CSysString &resultPath)
{
  CSysString tempPath;
  if (!MyGetTempPath(tempPath))
    return false;
  if (Create(tempPath, prefix, resultPath) != 0)
    return true;
  #ifdef UNDER_CE
  return false;
  #else
  if (!MyGetWindowsDirectory(tempPath))
    return false;
  return (Create(tempPath, prefix, resultPath) != 0);
  #endif
}

bool CTempFile::Remove()
{
  if (!_mustBeDeleted)
    return true;
  _mustBeDeleted = !DeleteFileAlways(_fileName);
  return !_mustBeDeleted;
}

#ifndef _UNICODE

UINT CTempFileW::Create(LPCWSTR dirPath, LPCWSTR prefix, UString &resultPath)
{
  Remove();
  UINT number = MyGetTempFileName(dirPath, prefix, resultPath);
  if (number != 0)
  {
    _fileName = resultPath;
    _mustBeDeleted = true;
  }
  return number;
}

bool CTempFileW::Create(LPCWSTR prefix, UString &resultPath)
{
  UString tempPath;
  if (!MyGetTempPath(tempPath))
    return false;
  if (Create(tempPath, prefix, resultPath) != 0)
    return true;
  if (!MyGetWindowsDirectory(tempPath))
    return false;
  return (Create(tempPath, prefix, resultPath) != 0);
}

bool CTempFileW::Remove()
{
  if (!_mustBeDeleted)
    return true;
  _mustBeDeleted = !DeleteFileAlways(_fileName);
  return !_mustBeDeleted;
}

#endif

bool CreateTempDirectory(LPCTSTR prefix, CSysString &dirName)
{
  /*
  CSysString prefix = tempPath + prefixChars;
  CRandom random;
  random.Init();
  */
  for (;;)
  {
    {
      CTempFile tempFile;
      if (!tempFile.Create(prefix, dirName))
        return false;
      if (!tempFile.Remove())
        return false;
    }
    /*
    UINT32 randomNumber = random.Generate();
    TCHAR randomNumberString[32];
    _stprintf(randomNumberString, _T("%04X"), randomNumber);
    dirName = prefix + randomNumberString;
    */
    if (NFind::DoesFileOrDirExist(dirName))
      continue;
    if (MyCreateDirectory(dirName))
      return true;
    if (::GetLastError() != ERROR_ALREADY_EXISTS)
      return false;
  }
}

bool CTempDirectory::Create(LPCTSTR prefix)
{
  Remove();
  return (_mustBeDeleted = CreateTempDirectory(prefix, _tempDir));
}

#ifndef _UNICODE

bool CreateTempDirectory(LPCWSTR prefix, UString &dirName)
{
  /*
  CSysString prefix = tempPath + prefixChars;
  CRandom random;
  random.Init();
  */
  for (;;)
  {
    {
      CTempFileW tempFile;
      if (!tempFile.Create(prefix, dirName))
        return false;
      if (!tempFile.Remove())
        return false;
    }
    /*
    UINT32 randomNumber = random.Generate();
    TCHAR randomNumberString[32];
    _stprintf(randomNumberString, _T("%04X"), randomNumber);
    dirName = prefix + randomNumberString;
    */
    if (NFind::DoesFileOrDirExist(dirName))
      continue;
    if (MyCreateDirectory(dirName))
      return true;
    if (::GetLastError() != ERROR_ALREADY_EXISTS)
      return false;
  }
}

bool CTempDirectoryW::Create(LPCWSTR prefix)
{
  Remove();
  return (_mustBeDeleted = CreateTempDirectory(prefix, _tempDir));
}

#endif

}}}
