/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/TextUtils.h"
#include "mozilla/UniquePtrExtensions.h"
#include "mozilla/Utf8.h"
#include "mozilla/WinHeaderOnlyUtils.h"

#include "nsCOMPtr.h"
#include "nsMemory.h"

#include "nsLocalFile.h"
#include "nsLocalFileCommon.h"
#include "nsIDirectoryEnumerator.h"
#include "nsNativeCharsetUtils.h"

#include "nsSimpleEnumerator.h"
#include "prio.h"
#include "private/pprio.h"  // To get PR_ImportFile
#include "nsHashKeys.h"

#include "nsString.h"
#include "nsReadableUtils.h"

#include <direct.h>
#include <fileapi.h>
#include <windows.h>
#include <shlwapi.h>
#include <aclapi.h>

#include "shellapi.h"
#include "shlguid.h"

#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <mbstring.h>

#include "prproces.h"
#include "prlink.h"

#include "mozilla/FilePreferences.h"
#include "mozilla/Mutex.h"
#include "SpecialSystemDirectory.h"

#include "nsTraceRefcnt.h"
#include "nsXPCOMCIDInternal.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "nsIWindowMediator.h"

#include "mozIDOMWindow.h"
#include "nsPIDOMWindow.h"
#include "nsIWidget.h"
#include "mozilla/ShellHeaderOnlyUtils.h"
#include "mozilla/WidgetUtils.h"
#include "WinUtils.h"

using namespace mozilla;
using mozilla::FilePreferences::kDevicePathSpecifier;
using mozilla::FilePreferences::kPathSeparator;

#define CHECK_mWorkingPath()                                     \
  do {                                                           \
    if (mWorkingPath.IsEmpty()) return NS_ERROR_NOT_INITIALIZED; \
  } while (0)

#ifndef FILE_ATTRIBUTE_NOT_CONTENT_INDEXED
#  define FILE_ATTRIBUTE_NOT_CONTENT_INDEXED 0x00002000
#endif

#ifndef DRIVE_REMOTE
#  define DRIVE_REMOTE 4
#endif

// MinGW does not know about this error, ensure we do.
#ifndef ERROR_DEVICE_HARDWARE_ERROR
#  define ERROR_DEVICE_HARDWARE_ERROR 483L
#endif

namespace {

nsresult NewLocalFile(const nsAString& aPath, bool aUseDOSDevicePathSyntax,
                      nsIFile** aResult) {
  RefPtr<nsLocalFile> file = new nsLocalFile();

  file->SetUseDOSDevicePathSyntax(aUseDOSDevicePathSyntax);

  if (!aPath.IsEmpty()) {
    nsresult rv = file->InitWithPath(aPath);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  file.forget(aResult);
  return NS_OK;
}

}  // anonymous namespace

static HWND GetMostRecentNavigatorHWND() {
  nsresult rv;
  nsCOMPtr<nsIWindowMediator> winMediator(
      do_GetService(NS_WINDOWMEDIATOR_CONTRACTID, &rv));
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  nsCOMPtr<mozIDOMWindowProxy> navWin;
  rv = winMediator->GetMostRecentWindow(u"navigator:browser",
                                        getter_AddRefs(navWin));
  if (NS_FAILED(rv) || !navWin) {
    return nullptr;
  }

  nsPIDOMWindowOuter* win = nsPIDOMWindowOuter::From(navWin);
  nsCOMPtr<nsIWidget> widget = widget::WidgetUtils::DOMWindowToWidget(win);
  if (!widget) {
    return nullptr;
  }

  return reinterpret_cast<HWND>(widget->GetNativeData(NS_NATIVE_WINDOW));
}

nsresult nsLocalFile::RevealFile(const nsString& aResolvedPath) {
  MOZ_ASSERT(!NS_IsMainThread(), "Don't run on the main thread");

  DWORD attributes = GetFileAttributesW(aResolvedPath.get());
  if (INVALID_FILE_ATTRIBUTES == attributes) {
    return NS_ERROR_FILE_INVALID_PATH;
  }

  HRESULT hr;
  if (attributes & FILE_ATTRIBUTE_DIRECTORY) {
    // We have a directory so we should open the directory itself.
    LPITEMIDLIST dir = ILCreateFromPathW(aResolvedPath.get());
    if (!dir) {
      return NS_ERROR_FAILURE;
    }

    LPCITEMIDLIST selection[] = {dir};
    UINT count = ArrayLength(selection);

    // Perform the open of the directory.
    hr = SHOpenFolderAndSelectItems(dir, count, selection, 0);
    CoTaskMemFree(dir);
  } else {
    int32_t len = aResolvedPath.Length();
    // We don't currently handle UNC long paths of the form \\?\ anywhere so
    // this should be fine.
    if (len > MAX_PATH) {
      return NS_ERROR_FILE_INVALID_PATH;
    }
    WCHAR parentDirectoryPath[MAX_PATH + 1] = {0};
    wcsncpy(parentDirectoryPath, aResolvedPath.get(), MAX_PATH);
    PathRemoveFileSpecW(parentDirectoryPath);

    // We have a file so we should open the parent directory.
    LPITEMIDLIST dir = ILCreateFromPathW(parentDirectoryPath);
    if (!dir) {
      return NS_ERROR_FAILURE;
    }

    // Set the item in the directory to select to the file we want to reveal.
    LPITEMIDLIST item = ILCreateFromPathW(aResolvedPath.get());
    if (!item) {
      CoTaskMemFree(dir);
      return NS_ERROR_FAILURE;
    }

    LPCITEMIDLIST selection[] = {item};
    UINT count = ArrayLength(selection);

    // Perform the selection of the file.
    hr = SHOpenFolderAndSelectItems(dir, count, selection, 0);

    CoTaskMemFree(dir);
    CoTaskMemFree(item);
  }

  return SUCCEEDED(hr) ? NS_OK : NS_ERROR_FAILURE;
}

class nsDriveEnumerator : public nsSimpleEnumerator,
                          public nsIDirectoryEnumerator {
 public:
  explicit nsDriveEnumerator(bool aUseDOSDevicePathSyntax);
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSISIMPLEENUMERATOR
  NS_FORWARD_NSISIMPLEENUMERATORBASE(nsSimpleEnumerator::)
  nsresult Init();

  const nsID& DefaultInterface() override { return NS_GET_IID(nsIFile); }

  NS_IMETHOD GetNextFile(nsIFile** aResult) override {
    bool hasMore = false;
    nsresult rv = HasMoreElements(&hasMore);
    if (NS_FAILED(rv) || !hasMore) {
      return rv;
    }
    nsCOMPtr<nsISupports> next;
    rv = GetNext(getter_AddRefs(next));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIFile> result = do_QueryInterface(next);
    result.forget(aResult);
    return NS_OK;
  }

  NS_IMETHOD Close() override { return NS_OK; }

 private:
  virtual ~nsDriveEnumerator();

  /* mDrives stores the null-separated drive names.
   * Init sets them.
   * HasMoreElements checks mStartOfCurrentDrive.
   * GetNext advances mStartOfCurrentDrive.
   */
  nsString mDrives;
  nsAString::const_iterator mStartOfCurrentDrive;
  nsAString::const_iterator mEndOfDrivesString;
  const bool mUseDOSDevicePathSyntax;
};

//-----------------------------------------------------------------------------
// static helper functions
//-----------------------------------------------------------------------------

/**
 * While not comprehensive, this will map many common Windows error codes to a
 * corresponding nsresult. If an unmapped error is encountered, the hex error
 * code will be logged to stderr. Error codes, names, and descriptions can be
 * found at the following MSDN page:
 * https://docs.microsoft.com/en-us/windows/win32/debug/system-error-codes
 *
 * \note When adding more mappings here, it must be checked if there's code that
 * depends on the current generic NS_ERROR_MODULE_WIN32 mapping for such error
 * codes.
 */
static nsresult ConvertWinError(DWORD aWinErr) {
  nsresult rv;

  switch (aWinErr) {
    case ERROR_FILE_NOT_FOUND:
      [[fallthrough]];  // to NS_ERROR_FILE_NOT_FOUND
    case ERROR_PATH_NOT_FOUND:
      [[fallthrough]];  // to NS_ERROR_FILE_NOT_FOUND
    case ERROR_INVALID_DRIVE:
      rv = NS_ERROR_FILE_NOT_FOUND;
      break;
    case ERROR_ACCESS_DENIED:
      [[fallthrough]];  // to NS_ERROR_FILE_ACCESS_DENIED
    case ERROR_NOT_SAME_DEVICE:
      [[fallthrough]];  // to NS_ERROR_FILE_ACCESS_DENIED
    case ERROR_CANNOT_MAKE:
      rv = NS_ERROR_FILE_ACCESS_DENIED;
      break;
    case ERROR_SHARING_VIOLATION:  // CreateFile without sharing flags
      [[fallthrough]];             // to NS_ERROR_FILE_IS_LOCKED
    case ERROR_LOCK_VIOLATION:     // LockFile, LockFileEx
      rv = NS_ERROR_FILE_IS_LOCKED;
      break;
    case ERROR_NOT_ENOUGH_MEMORY:
      rv = NS_ERROR_OUT_OF_MEMORY;
      break;
    case ERROR_DIR_NOT_EMPTY:
      [[fallthrough]];  // to NS_ERROR_FILE_DIR_NOT_EMPTY
    case ERROR_CURRENT_DIRECTORY:
      rv = NS_ERROR_FILE_DIR_NOT_EMPTY;
      break;
    case ERROR_WRITE_PROTECT:
      rv = NS_ERROR_FILE_READ_ONLY;
      break;
    case ERROR_HANDLE_DISK_FULL:
      [[fallthrough]];  // to NS_ERROR_FILE_NO_DEVICE_SPACE
    case ERROR_DISK_FULL:
      rv = NS_ERROR_FILE_NO_DEVICE_SPACE;
      break;
    case ERROR_FILE_EXISTS:
      [[fallthrough]];  // to NS_ERROR_FILE_ALREADY_EXISTS
    case ERROR_ALREADY_EXISTS:
      rv = NS_ERROR_FILE_ALREADY_EXISTS;
      break;
    case ERROR_FILENAME_EXCED_RANGE:
      rv = NS_ERROR_FILE_NAME_TOO_LONG;
      break;
    case ERROR_DIRECTORY:
      rv = NS_ERROR_FILE_NOT_DIRECTORY;
      break;
    case ERROR_FILE_CORRUPT:
      rv = NS_ERROR_FILE_FS_CORRUPTED;
      break;
    case ERROR_DEVICE_HARDWARE_ERROR:
      [[fallthrough]];  // to NS_ERROR_FILE_DEVICE_FAILURE
    case ERROR_DEVICE_NOT_CONNECTED:
      [[fallthrough]];  // to NS_ERROR_FILE_DEVICE_FAILURE
    case ERROR_IO_DEVICE:
      rv = NS_ERROR_FILE_DEVICE_FAILURE;
      break;
    case ERROR_NOT_READY:
      rv = NS_ERROR_FILE_DEVICE_TEMPORARY_FAILURE;
      break;
    case ERROR_INVALID_NAME:
      rv = NS_ERROR_FILE_INVALID_PATH;
      break;
    case ERROR_INVALID_BLOCK:
      [[fallthrough]];  // to NS_ERROR_FILE_INVALID_HANDLE
    case ERROR_INVALID_HANDLE:
      [[fallthrough]];  // to NS_ERROR_FILE_INVALID_HANDLE
    case ERROR_ARENA_TRASHED:
      rv = NS_ERROR_FILE_INVALID_HANDLE;
      break;
    case 0:
      rv = NS_OK;
      break;
    default:
      printf_stderr(
          "ConvertWinError received an unrecognized WinError: 0x%" PRIx32 "\n",
          static_cast<uint32_t>(aWinErr));
      MOZ_ASSERT((aWinErr & 0xFFFF) == aWinErr);
      rv = NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_WIN32, aWinErr & 0xFFFF);
      break;
  }
  return rv;
}

// as suggested in the MSDN documentation on SetFilePointer
static __int64 MyFileSeek64(HANDLE aHandle, __int64 aDistance,
                            DWORD aMoveMethod) {
  LARGE_INTEGER li;

  li.QuadPart = aDistance;
  li.LowPart = SetFilePointer(aHandle, li.LowPart, &li.HighPart, aMoveMethod);
  if (li.LowPart == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR) {
    li.QuadPart = -1;
  }

  return li.QuadPart;
}

// Check whether a path is a volume root. Expects paths to be \-terminated.
static bool IsRootPath(const nsAString& aPath) {
  // Easy cases first:
  if (aPath.Last() != L'\\') {
    return false;
  }
  if (StringEndsWith(aPath, u":\\"_ns)) {
    return true;
  }

  nsAString::const_iterator begin, end;
  aPath.BeginReading(begin);
  aPath.EndReading(end);
  // We know we've got a trailing slash, skip that:
  end--;
  // Find the next last slash:
  if (RFindInReadable(u"\\"_ns, begin, end)) {
    // Reset iterator:
    aPath.EndReading(end);
    end--;
    auto lastSegment = Substring(++begin, end);
    if (lastSegment.IsEmpty()) {
      return false;
    }

    // Check if we end with e.g. "c$", a drive letter in UNC or network shares
    if (lastSegment.Last() == L'$' && lastSegment.Length() == 2 &&
        IsAsciiAlpha(lastSegment.First())) {
      return true;
    }
    // Volume GUID paths:
    if (StringBeginsWith(lastSegment, u"Volume{"_ns) &&
        lastSegment.Last() == L'}') {
      return true;
    }
  }
  return false;
}

static auto kSpecialNTFSFilesInRoot = {
    u"$MFT"_ns,     u"$MFTMirr"_ns, u"$LogFile"_ns, u"$Volume"_ns,
    u"$AttrDef"_ns, u"$Bitmap"_ns,  u"$Boot"_ns,    u"$BadClus"_ns,
    u"$Secure"_ns,  u"$UpCase"_ns,  u"$Extend"_ns};
static bool IsSpecialNTFSPath(const nsAString& aFilePath) {
  nsAString::const_iterator begin, end;
  aFilePath.BeginReading(begin);
  aFilePath.EndReading(end);
  auto iter = begin;
  // Early exit if there's no '$' (common case)
  if (!FindCharInReadable(L'$', iter, end)) {
    return false;
  }

  iter = begin;
  // Any use of ':$' is illegal in filenames anyway; while we support some
  // ADS stuff (ie ":Zone.Identifier"), none of them use the ':$' syntax:
  if (FindInReadable(u":$"_ns, iter, end)) {
    return true;
  }

  auto normalized = mozilla::MakeUniqueFallible<wchar_t[]>(MAX_PATH);
  if (!normalized) {
    return true;
  }
  auto flatPath = PromiseFlatString(aFilePath);
  auto fullPathRV =
      GetFullPathNameW(flatPath.get(), MAX_PATH - 1, normalized.get(), nullptr);
  if (fullPathRV == 0 || fullPathRV > MAX_PATH - 1) {
    return false;
  }

  nsString normalizedPath(normalized.get());
  normalizedPath.BeginReading(begin);
  normalizedPath.EndReading(end);
  iter = begin;
  auto kDelimiters = u"\\:"_ns;
  while (iter != end && FindCharInReadable(L'$', iter, end)) {
    for (auto str : kSpecialNTFSFilesInRoot) {
      if (StringBeginsWith(Substring(iter, end), str,
                           nsCaseInsensitiveStringComparator)) {
        // If we're enclosed by separators or the beginning/end of the string,
        // this is one of the special files. Check if we're on a volume root.
        auto iterCopy = iter;
        iterCopy.advance(str.Length());
        // We check for both \ and : here because the filename could be
        // followd by a colon and a stream name/type, which shouldn't affect
        // our check:
        if (iterCopy == end || kDelimiters.Contains(*iterCopy)) {
          iterCopy = iter;
          // At the start of this path component, we don't need to care about
          // colons: we would have caught those in the check for `:$` above.
          if (iterCopy == begin || *(--iterCopy) == L'\\') {
            return IsRootPath(Substring(begin, iter));
          }
        }
      }
    }
    iter++;
  }

  return false;
}

//-----------------------------------------------------------------------------
// We need the following three definitions to make |OpenFile| convert a file
// handle to an NSPR file descriptor correctly when |O_APPEND| flag is
// specified. It is defined in a private header of NSPR (primpl.h) we can't
// include. As a temporary workaround until we decide how to extend
// |PR_ImportFile|, we define it here. Currently, |_PR_HAVE_PEEK_BUFFER|
// and |PR_STRICT_ADDR_LEN| are not defined for the 'w95'-dependent portion
// of NSPR so that fields of |PRFilePrivate| #ifdef'd by them are not copied.
// Similarly, |_MDFileDesc| is taken from nsprpub/pr/include/md/_win95.h.
// In an unlikely case we switch to 'NT'-dependent NSPR AND this temporary
// workaround last beyond the switch, |PRFilePrivate| and |_MDFileDesc|
// need to be changed to match the definitions for WinNT.
//-----------------------------------------------------------------------------
typedef enum {
  _PR_TRI_TRUE = 1,
  _PR_TRI_FALSE = 0,
  _PR_TRI_UNKNOWN = -1
} _PRTriStateBool;

struct _MDFileDesc {
  PROsfd osfd;
};

struct PRFilePrivate {
  int32_t state;
  bool nonblocking;
  _PRTriStateBool inheritable;
  PRFileDesc* next;
  int lockCount; /*   0: not locked
                  *  -1: a native lockfile call is in progress
                  * > 0: # times the file is locked */
  bool appendMode;
  _MDFileDesc md;
};

//-----------------------------------------------------------------------------
// Six static methods defined below (OpenFile,  FileTimeToPRTime, GetFileInfo,
// OpenDir, CloseDir, ReadDir) should go away once the corresponding
// UTF-16 APIs are implemented on all the supported platforms (or at least
// Windows 9x/ME) in NSPR. Currently, they're only implemented on
// Windows NT4 or later. (bug 330665)
//-----------------------------------------------------------------------------

// copied from nsprpub/pr/src/{io/prfile.c | md/windows/w95io.c} :
// PR_Open and _PR_MD_OPEN
nsresult OpenFile(const nsString& aName, int aOsflags, int aMode,
                  bool aShareDelete, PRFileDesc** aFd) {
  int32_t access = 0;

  int32_t shareMode = FILE_SHARE_READ | FILE_SHARE_WRITE;
  int32_t disposition = 0;
  int32_t attributes = 0;

  if (aShareDelete) {
    shareMode |= FILE_SHARE_DELETE;
  }

  if (aOsflags & PR_SYNC) {
    attributes = FILE_FLAG_WRITE_THROUGH;
  }
  if (aOsflags & PR_RDONLY || aOsflags & PR_RDWR) {
    access |= GENERIC_READ;
  }
  if (aOsflags & PR_WRONLY || aOsflags & PR_RDWR) {
    access |= GENERIC_WRITE;
  }

  if (aOsflags & PR_CREATE_FILE && aOsflags & PR_EXCL) {
    disposition = CREATE_NEW;
  } else if (aOsflags & PR_CREATE_FILE) {
    if (aOsflags & PR_TRUNCATE) {
      disposition = CREATE_ALWAYS;
    } else {
      disposition = OPEN_ALWAYS;
    }
  } else {
    if (aOsflags & PR_TRUNCATE) {
      disposition = TRUNCATE_EXISTING;
    } else {
      disposition = OPEN_EXISTING;
    }
  }

  if (aOsflags & nsIFile::DELETE_ON_CLOSE) {
    attributes |= FILE_FLAG_DELETE_ON_CLOSE;
  }

  if (aOsflags & nsIFile::OS_READAHEAD) {
    attributes |= FILE_FLAG_SEQUENTIAL_SCAN;
  }

  // If no write permissions are requested, and if we are possibly creating
  // the file, then set the new file as read only.
  // The flag has no effect if we happen to open the file.
  if (!(aMode & (PR_IWUSR | PR_IWGRP | PR_IWOTH)) &&
      disposition != OPEN_EXISTING) {
    attributes |= FILE_ATTRIBUTE_READONLY;
  }

  HANDLE file = ::CreateFileW(aName.get(), access, shareMode, nullptr,
                              disposition, attributes, nullptr);

  if (file == INVALID_HANDLE_VALUE) {
    *aFd = nullptr;
    return ConvertWinError(GetLastError());
  }

  *aFd = PR_ImportFile((PROsfd)file);
  if (*aFd) {
    // On Windows, _PR_HAVE_O_APPEND is not defined so that we have to
    // add it manually. (see |PR_Open| in nsprpub/pr/src/io/prfile.c)
    (*aFd)->secret->appendMode = (PR_APPEND & aOsflags) ? true : false;
    return NS_OK;
  }

  nsresult rv = NS_ErrorAccordingToNSPR();

  CloseHandle(file);

  return rv;
}

// copied from nsprpub/pr/src/{io/prfile.c | md/windows/w95io.c} :
// PR_FileTimeToPRTime and _PR_FileTimeToPRTime
static void FileTimeToPRTime(const FILETIME* aFiletime, PRTime* aPrtm) {
#ifdef __GNUC__
  const PRTime _pr_filetime_offset = 116444736000000000LL;
#else
  const PRTime _pr_filetime_offset = 116444736000000000i64;
#endif

  MOZ_ASSERT(sizeof(FILETIME) == sizeof(PRTime));
  ::CopyMemory(aPrtm, aFiletime, sizeof(PRTime));
#ifdef __GNUC__
  *aPrtm = (*aPrtm - _pr_filetime_offset) / 10LL;
#else
  *aPrtm = (*aPrtm - _pr_filetime_offset) / 10i64;
#endif
}

// copied from nsprpub/pr/src/{io/prfile.c | md/windows/w95io.c} with some
// changes : PR_GetFileInfo64, _PR_MD_GETFILEINFO64
static nsresult GetFileInfo(const nsString& aName, PRFileInfo64* aInfo) {
  if (aName.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  // Checking u"?*" for the file path excluding the kDevicePathSpecifier.
  // ToDo: Check if checking "?" for the file path is still needed.
  const int32_t offset = StringBeginsWith(aName, kDevicePathSpecifier)
                             ? kDevicePathSpecifier.Length()
                             : 0;

  if (aName.FindCharInSet(u"?*", offset) != kNotFound) {
    return NS_ERROR_INVALID_ARG;
  }

  WIN32_FILE_ATTRIBUTE_DATA fileData;
  if (!::GetFileAttributesExW(aName.get(), GetFileExInfoStandard, &fileData)) {
    return ConvertWinError(GetLastError());
  }

  if (fileData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
    aInfo->type = PR_FILE_OTHER;
  } else if (fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
    aInfo->type = PR_FILE_DIRECTORY;
  } else {
    aInfo->type = PR_FILE_FILE;
  }

  aInfo->size = fileData.nFileSizeHigh;
  aInfo->size = (aInfo->size << 32) + fileData.nFileSizeLow;

  FileTimeToPRTime(&fileData.ftLastWriteTime, &aInfo->modifyTime);

  if (0 == fileData.ftCreationTime.dwLowDateTime &&
      0 == fileData.ftCreationTime.dwHighDateTime) {
    aInfo->creationTime = aInfo->modifyTime;
  } else {
    FileTimeToPRTime(&fileData.ftCreationTime, &aInfo->creationTime);
  }

  return NS_OK;
}

struct nsDir {
  HANDLE handle;
  WIN32_FIND_DATAW data;
  bool firstEntry;
};

static nsresult OpenDir(const nsString& aName, nsDir** aDir) {
  if (NS_WARN_IF(!aDir)) {
    return NS_ERROR_INVALID_ARG;
  }

  *aDir = nullptr;

  nsDir* d = new nsDir();
  nsAutoString filename(aName);

  // If |aName| ends in a slash or backslash, do not append another backslash.
  if (filename.Last() == L'/' || filename.Last() == L'\\') {
    filename.Append('*');
  } else {
    filename.AppendLiteral("\\*");
  }

  filename.ReplaceChar(L'/', L'\\');

  // FindFirstFileW Will have a last error of ERROR_DIRECTORY if
  // <file_path>\* is passed in.  If <unknown_path>\* is passed in then
  // ERROR_PATH_NOT_FOUND will be the last error.
  d->handle = ::FindFirstFileW(filename.get(), &(d->data));

  if (d->handle == INVALID_HANDLE_VALUE) {
    delete d;
    return ConvertWinError(GetLastError());
  }
  d->firstEntry = true;

  *aDir = d;
  return NS_OK;
}

static nsresult ReadDir(nsDir* aDir, PRDirFlags aFlags, nsString& aName) {
  aName.Truncate();
  if (NS_WARN_IF(!aDir)) {
    return NS_ERROR_INVALID_ARG;
  }

  while (1) {
    BOOL rv;
    if (aDir->firstEntry) {
      aDir->firstEntry = false;
      rv = 1;
    } else {
      rv = ::FindNextFileW(aDir->handle, &(aDir->data));
    }

    if (rv == 0) {
      break;
    }

    const wchar_t* fileName;
    fileName = (aDir)->data.cFileName;

    if ((aFlags & PR_SKIP_DOT) && (fileName[0] == L'.') &&
        (fileName[1] == L'\0')) {
      continue;
    }
    if ((aFlags & PR_SKIP_DOT_DOT) && (fileName[0] == L'.') &&
        (fileName[1] == L'.') && (fileName[2] == L'\0')) {
      continue;
    }

    DWORD attrib = aDir->data.dwFileAttributes;
    if ((aFlags & PR_SKIP_HIDDEN) && (attrib & FILE_ATTRIBUTE_HIDDEN)) {
      continue;
    }

    aName = fileName;
    return NS_OK;
  }

  DWORD err = GetLastError();
  return err == ERROR_NO_MORE_FILES ? NS_OK : ConvertWinError(err);
}

static nsresult CloseDir(nsDir*& aDir) {
  if (NS_WARN_IF(!aDir)) {
    return NS_ERROR_INVALID_ARG;
  }

  BOOL isOk = FindClose(aDir->handle);
  delete aDir;
  aDir = nullptr;
  return isOk ? NS_OK : ConvertWinError(GetLastError());
}

//-----------------------------------------------------------------------------
// nsDirEnumerator
//-----------------------------------------------------------------------------

class nsDirEnumerator final : public nsSimpleEnumerator,
                              public nsIDirectoryEnumerator {
 private:
  ~nsDirEnumerator() { Close(); }

 public:
  NS_DECL_ISUPPORTS_INHERITED

  NS_FORWARD_NSISIMPLEENUMERATORBASE(nsSimpleEnumerator::)

  nsDirEnumerator() : mDir(nullptr) {}

  const nsID& DefaultInterface() override { return NS_GET_IID(nsIFile); }

  nsresult Init(nsIFile* aParent) {
    nsAutoString filepath;
    aParent->GetTarget(filepath);

    if (filepath.IsEmpty()) {
      aParent->GetPath(filepath);
    }

    if (filepath.IsEmpty()) {
      return NS_ERROR_UNEXPECTED;
    }

    // IsDirectory is not needed here because OpenDir will return
    // NS_ERROR_FILE_NOT_DIRECTORY if the passed in path is a file.
    nsresult rv = OpenDir(filepath, &mDir);
    if (NS_FAILED(rv)) {
      return rv;
    }

    mParent = aParent;
    return NS_OK;
  }

  NS_IMETHOD HasMoreElements(bool* aResult) override {
    nsresult rv;
    if (!mNext && mDir) {
      nsString name;
      rv = ReadDir(mDir, PR_SKIP_BOTH, name);
      if (NS_FAILED(rv)) {
        return rv;
      }
      if (name.IsEmpty()) {
        // end of dir entries
        rv = CloseDir(mDir);
        if (NS_FAILED(rv)) {
          return rv;
        }

        *aResult = false;
        return NS_OK;
      }

      nsCOMPtr<nsIFile> file;
      rv = mParent->Clone(getter_AddRefs(file));
      if (NS_FAILED(rv)) {
        return rv;
      }

      rv = file->Append(name);
      if (NS_FAILED(rv)) {
        return rv;
      }

      mNext = file.forget();
    }
    *aResult = mNext != nullptr;
    if (!*aResult) {
      Close();
    }
    return NS_OK;
  }

  NS_IMETHOD GetNext(nsISupports** aResult) override {
    nsresult rv;
    bool hasMore;
    rv = HasMoreElements(&hasMore);
    if (NS_FAILED(rv)) {
      return rv;
    }
    if (!hasMore) {
      return NS_ERROR_FAILURE;
    }

    mNext.forget(aResult);
    return NS_OK;
  }

  NS_IMETHOD GetNextFile(nsIFile** aResult) override {
    *aResult = nullptr;
    bool hasMore = false;
    nsresult rv = HasMoreElements(&hasMore);
    if (NS_FAILED(rv) || !hasMore) {
      return rv;
    }
    mNext.forget(aResult);
    return NS_OK;
  }

  NS_IMETHOD Close() override {
    if (mDir) {
      nsresult rv = CloseDir(mDir);
      NS_ASSERTION(NS_SUCCEEDED(rv), "close failed");
      if (NS_FAILED(rv)) {
        return NS_ERROR_FAILURE;
      }
    }
    return NS_OK;
  }

 protected:
  nsDir* mDir;
  nsCOMPtr<nsIFile> mParent;
  nsCOMPtr<nsIFile> mNext;
};

NS_IMPL_ISUPPORTS_INHERITED(nsDirEnumerator, nsSimpleEnumerator,
                            nsIDirectoryEnumerator)

//-----------------------------------------------------------------------------
// nsLocalFile <public>
//-----------------------------------------------------------------------------

nsLocalFile::nsLocalFile()
    : mDirty(true), mResolveDirty(true), mUseDOSDevicePathSyntax(false) {}

nsLocalFile::nsLocalFile(const nsAString& aFilePath)
    : mUseDOSDevicePathSyntax(false) {
  InitWithPath(aFilePath);
}

nsresult nsLocalFile::nsLocalFileConstructor(nsISupports* aOuter,
                                             const nsIID& aIID,
                                             void** aInstancePtr) {
  if (NS_WARN_IF(!aInstancePtr)) {
    return NS_ERROR_INVALID_ARG;
  }
  if (NS_WARN_IF(aOuter)) {
    return NS_ERROR_NO_AGGREGATION;
  }

  nsLocalFile* inst = new nsLocalFile();
  nsresult rv = inst->QueryInterface(aIID, aInstancePtr);
  if (NS_FAILED(rv)) {
    delete inst;
    return rv;
  }
  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsLocalFile::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS(nsLocalFile, nsIFile, nsILocalFileWin)

//-----------------------------------------------------------------------------
// nsLocalFile <private>
//-----------------------------------------------------------------------------

nsLocalFile::nsLocalFile(const nsLocalFile& aOther)
    : mDirty(true),
      mResolveDirty(true),
      mUseDOSDevicePathSyntax(aOther.mUseDOSDevicePathSyntax),
      mWorkingPath(aOther.mWorkingPath) {}

nsresult nsLocalFile::ResolveSymlink() {
  std::wstring workingPath(mWorkingPath.Data());
  if (!widget::WinUtils::ResolveJunctionPointsAndSymLinks(workingPath)) {
    return NS_ERROR_FAILURE;
  }
  mResolvedPath.Assign(workingPath.c_str(), workingPath.length());
  return NS_OK;
}

// Resolve any shortcuts and stat the resolved path. After a successful return
// the path is guaranteed valid and the members of mFileInfo64 can be used.
nsresult nsLocalFile::ResolveAndStat() {
  // if we aren't dirty then we are already done
  if (!mDirty) {
    return NS_OK;
  }

  AUTO_PROFILER_LABEL("nsLocalFile::ResolveAndStat", OTHER);
  // we can't resolve/stat anything that isn't a valid NSPR addressable path
  if (mWorkingPath.IsEmpty()) {
    return NS_ERROR_FILE_INVALID_PATH;
  }

  // this is usually correct
  mResolvedPath.Assign(mWorkingPath);

  // Make sure root paths have a trailing slash.
  nsAutoString nsprPath(mWorkingPath);
  if (mWorkingPath.Length() == 2 && mWorkingPath.CharAt(1) == u':') {
    nsprPath.Append('\\');
  }

  // first we will see if the working path exists. If it doesn't then
  // there is nothing more that can be done
  nsresult rv = GetFileInfo(nsprPath, &mFileInfo64);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (mFileInfo64.type != PR_FILE_OTHER) {
    mResolveDirty = false;
    mDirty = false;
    return NS_OK;
  }

  // OTHER from GetFileInfo currently means a symlink
  rv = ResolveSymlink();
  // Even if it fails we need to have the resolved path equal to working path
  // for those functions that always use the resolved path.
  if (NS_FAILED(rv)) {
    mResolvedPath.Assign(mWorkingPath);
    return rv;
  }

  mResolveDirty = false;
  // get the details of the resolved path
  rv = GetFileInfo(mResolvedPath, &mFileInfo64);
  if (NS_FAILED(rv)) {
    return rv;
  }

  mDirty = false;
  return NS_OK;
}

/**
 * Fills the mResolvedPath member variable with the file or symlink target
 * if follow symlinks is on.  This is a copy of the Resolve parts from
 * ResolveAndStat. ResolveAndStat is much slower though because of the stat.
 *
 * @return NS_OK on success.
 */
nsresult nsLocalFile::Resolve() {
  // if we aren't dirty then we are already done
  if (!mResolveDirty) {
    return NS_OK;
  }

  // we can't resolve/stat anything that isn't a valid NSPR addressable path
  if (mWorkingPath.IsEmpty()) {
    return NS_ERROR_FILE_INVALID_PATH;
  }

  // this is usually correct
  mResolvedPath.Assign(mWorkingPath);

  // TODO: Implement symlink support

  mResolveDirty = false;
  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsLocalFile::nsIFile
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsLocalFile::Clone(nsIFile** aFile) {
  // Just copy-construct ourselves
  RefPtr<nsLocalFile> file = new nsLocalFile(*this);
  file.forget(aFile);

  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::InitWithFile(nsIFile* aFile) {
  if (NS_WARN_IF(!aFile)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsAutoString path;
  aFile->GetPath(path);
  if (path.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }
  return InitWithPath(path);
}

NS_IMETHODIMP
nsLocalFile::InitWithPath(const nsAString& aFilePath) {
  MakeDirty();

  nsAString::const_iterator begin, end;
  aFilePath.BeginReading(begin);
  aFilePath.EndReading(end);

  // input string must not be empty
  if (begin == end) {
    return NS_ERROR_FAILURE;
  }

  char16_t firstChar = *begin;
  char16_t secondChar = *(++begin);

  // just do a sanity check.  if it has any forward slashes, it is not a Native
  // path on windows.  Also, it must have a colon at after the first char.
  if (FindCharInReadable(L'/', begin, end)) {
    return NS_ERROR_FILE_UNRECOGNIZED_PATH;
  }

  if (FilePreferences::IsBlockedUNCPath(aFilePath)) {
    return NS_ERROR_FILE_ACCESS_DENIED;
  }

  if (secondChar != L':' && (secondChar != L'\\' || firstChar != L'\\')) {
    return NS_ERROR_FILE_UNRECOGNIZED_PATH;
  }

  if (secondChar == L':') {
    // Make sure we have a valid drive, later code assumes the drive letter
    // is a single char a-z or A-Z.
    if (MozPathGetDriveNumber<wchar_t>(aFilePath.Data()) == -1) {
      return NS_ERROR_FILE_UNRECOGNIZED_PATH;
    }
  }

  if (IsSpecialNTFSPath(aFilePath)) {
    return NS_ERROR_FILE_UNRECOGNIZED_PATH;
  }

  mWorkingPath = aFilePath;
  // kill any trailing '\'
  if (mWorkingPath.Last() == L'\\') {
    mWorkingPath.Truncate(mWorkingPath.Length() - 1);
  }

  // Bug 1626514: make sure that we don't end up with multiple prefixes.

  // Prepend the "\\?\" prefix if the useDOSDevicePathSyntax is set and the path
  // starts with a disk designator and backslash.
  if (mUseDOSDevicePathSyntax &&
      FilePreferences::StartsWithDiskDesignatorAndBackslash(mWorkingPath)) {
    mWorkingPath = kDevicePathSpecifier + mWorkingPath;
  }

  return NS_OK;
}

// Strip a handler command string of its quotes and parameters.
static void CleanupHandlerPath(nsString& aPath) {
  // Example command strings passed into this routine:

  // 1) C:\Program Files\Company\some.exe -foo -bar
  // 2) C:\Program Files\Company\some.dll
  // 3) C:\Windows\some.dll,-foo -bar
  // 4) C:\Windows\some.cpl,-foo -bar

  int32_t lastCommaPos = aPath.RFindChar(',');
  if (lastCommaPos != kNotFound) aPath.Truncate(lastCommaPos);

  aPath.Append(' ');

  // case insensitive
  int32_t index = aPath.Find(".exe ", true);
  if (index == kNotFound) index = aPath.Find(".dll ", true);
  if (index == kNotFound) index = aPath.Find(".cpl ", true);

  if (index != kNotFound) aPath.Truncate(index + 4);
  aPath.Trim(" ", true, true);
}

// Strip the windows host process bootstrap executable rundll32.exe
// from a handler's command string if it exists.
static void StripRundll32(nsString& aCommandString) {
  // Example rundll formats:
  // C:\Windows\System32\rundll32.exe "path to dll"
  // rundll32.exe "path to dll"
  // C:\Windows\System32\rundll32.exe "path to dll", var var
  // rundll32.exe "path to dll", var var

  constexpr auto rundllSegment = u"rundll32.exe "_ns;
  constexpr auto rundllSegmentShort = u"rundll32 "_ns;

  // case insensitive
  int32_t strLen = rundllSegment.Length();
  int32_t index = aCommandString.Find(rundllSegment, true);
  if (index == kNotFound) {
    strLen = rundllSegmentShort.Length();
    index = aCommandString.Find(rundllSegmentShort, true);
  }

  if (index != kNotFound) {
    uint32_t rundllSegmentLength = index + strLen;
    aCommandString.Cut(0, rundllSegmentLength);
  }
}

// Returns the fully qualified path to an application handler based on
// a parameterized command string. Note this routine should not be used
// to launch the associated application as it strips parameters and
// rundll.exe from the string. Designed for retrieving display information
// on a particular handler.
/* static */
bool nsLocalFile::CleanupCmdHandlerPath(nsAString& aCommandHandler) {
  nsAutoString handlerCommand(aCommandHandler);

  // Straight command path:
  //
  // %SystemRoot%\system32\NOTEPAD.EXE var
  // "C:\Program Files\iTunes\iTunes.exe" var var
  // C:\Program Files\iTunes\iTunes.exe var var
  //
  // Example rundll handlers:
  //
  // rundll32.exe "%ProgramFiles%\Win...ery\PhotoViewer.dll", var var
  // rundll32.exe "%ProgramFiles%\Windows Photo Gallery\PhotoViewer.dll"
  // C:\Windows\System32\rundll32.exe "path to dll", var var
  // %SystemRoot%\System32\rundll32.exe "%ProgramFiles%\Win...ery\Photo
  //    Viewer.dll", var var

  // Expand environment variables so we have full path strings.
  uint32_t bufLength =
      ::ExpandEnvironmentStringsW(handlerCommand.get(), nullptr, 0);
  if (bufLength == 0)  // Error
    return false;

  auto destination = mozilla::MakeUniqueFallible<wchar_t[]>(bufLength);
  if (!destination) return false;
  if (!::ExpandEnvironmentStringsW(handlerCommand.get(), destination.get(),
                                   bufLength))
    return false;

  handlerCommand.Assign(destination.get());

  // Remove quotes around paths
  handlerCommand.StripChars("\"");

  // Strip windows host process bootstrap so we can get to the actual
  // handler.
  StripRundll32(handlerCommand);

  // Trim any command parameters so that we have a native path we can
  // initialize a local file with.
  CleanupHandlerPath(handlerCommand);

  aCommandHandler.Assign(handlerCommand);
  return true;
}

NS_IMETHODIMP
nsLocalFile::InitWithCommandLine(const nsAString& aCommandLine) {
  nsAutoString commandLine(aCommandLine);
  if (!CleanupCmdHandlerPath(commandLine)) {
    return NS_ERROR_FILE_UNRECOGNIZED_PATH;
  }
  return InitWithPath(commandLine);
}

NS_IMETHODIMP
nsLocalFile::OpenNSPRFileDesc(int32_t aFlags, int32_t aMode,
                              PRFileDesc** aResult) {
  nsresult rv = OpenNSPRFileDescMaybeShareDelete(aFlags, aMode, false, aResult);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::OpenANSIFileDesc(const char* aMode, FILE** aResult) {
  *aResult = _wfopen(mWorkingPath.get(), NS_ConvertASCIItoUTF16(aMode).get());
  if (*aResult) {
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

static nsresult do_create(nsIFile* aFile, const nsString& aPath,
                          uint32_t aAttributes) {
  PRFileDesc* file;
  nsresult rv =
      OpenFile(aPath, PR_RDONLY | PR_CREATE_FILE | PR_APPEND | PR_EXCL,
               aAttributes, false, &file);
  if (file) {
    PR_Close(file);
  }

  if (rv == NS_ERROR_FILE_ACCESS_DENIED) {
    // need to return already-exists for directories (bug 452217)
    bool isdir;
    if (NS_SUCCEEDED(aFile->IsDirectory(&isdir)) && isdir) {
      rv = NS_ERROR_FILE_ALREADY_EXISTS;
    }
  }
  return rv;
}

static nsresult do_mkdir(nsIFile*, const nsString& aPath, uint32_t) {
  if (!::CreateDirectoryW(aPath.get(), nullptr)) {
    return ConvertWinError(GetLastError());
  }
  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::Create(uint32_t aType, uint32_t aAttributes, bool aSkipAncestors) {
  if (aType != NORMAL_FILE_TYPE && aType != DIRECTORY_TYPE) {
    return NS_ERROR_FILE_UNKNOWN_TYPE;
  }

  auto* createFunc = (aType == NORMAL_FILE_TYPE ? do_create : do_mkdir);

  nsresult rv = createFunc(this, mWorkingPath, aAttributes);

  if (NS_SUCCEEDED(rv) || NS_ERROR_FILE_ALREADY_EXISTS == rv ||
      aSkipAncestors) {
    return rv;
  }

  // create directories to target
  //
  // A given local file can be either one of these forms:
  //
  //   - normal:    X:\some\path\on\this\drive
  //                       ^--- start here
  //
  //   - UNC path:  \\machine\volume\some\path\on\this\drive
  //                                     ^--- start here
  //
  // Skip the first 'X:\' for the first form, and skip the first full
  // '\\machine\volume\' segment for the second form.

  wchar_t* path = char16ptr_t(mWorkingPath.BeginWriting());

  if (path[0] == L'\\' && path[1] == L'\\') {
    // dealing with a UNC path here; skip past '\\machine\'
    path = wcschr(path + 2, L'\\');
    if (!path) {
      return NS_ERROR_FILE_INVALID_PATH;
    }
    ++path;
  }

  // search for first slash after the drive (or volume) name
  wchar_t* slash = wcschr(path, L'\\');

  nsresult directoryCreateError = NS_OK;
  if (slash) {
    // skip the first '\\'
    ++slash;
    slash = wcschr(slash, L'\\');

    while (slash) {
      *slash = L'\0';

      if (!::CreateDirectoryW(mWorkingPath.get(), nullptr)) {
        rv = ConvertWinError(GetLastError());
        if (NS_ERROR_FILE_NOT_FOUND == rv &&
            NS_ERROR_FILE_ACCESS_DENIED == directoryCreateError) {
          // If a previous CreateDirectory failed due to access, return that.
          return NS_ERROR_FILE_ACCESS_DENIED;
        }
        // perhaps the base path already exists, or perhaps we don't have
        // permissions to create the directory.  NOTE: access denied could
        // occur on a parent directory even though it exists.
        else if (rv != NS_ERROR_FILE_ALREADY_EXISTS &&
                 rv != NS_ERROR_FILE_ACCESS_DENIED) {
          return rv;
        }

        directoryCreateError = rv;
      }
      *slash = L'\\';
      ++slash;
      slash = wcschr(slash, L'\\');
    }
  }

  // If our last CreateDirectory failed due to access, return that.
  if (NS_ERROR_FILE_ACCESS_DENIED == directoryCreateError) {
    return directoryCreateError;
  }

  return createFunc(this, mWorkingPath, aAttributes);
}

NS_IMETHODIMP
nsLocalFile::Append(const nsAString& aNode) {
  // append this path, multiple components are not permitted
  return AppendInternal(PromiseFlatString(aNode), false);
}

NS_IMETHODIMP
nsLocalFile::AppendRelativePath(const nsAString& aNode) {
  // append this path, multiple components are permitted
  return AppendInternal(PromiseFlatString(aNode), true);
}

nsresult nsLocalFile::AppendInternal(const nsString& aNode,
                                     bool aMultipleComponents) {
  if (aNode.IsEmpty()) {
    return NS_OK;
  }

  // check the relative path for validity
  if (aNode.First() == L'\\' ||   // can't start with an '\'
      aNode.Contains(L'/') ||     // can't contain /
      aNode.EqualsASCII("..")) {  // can't be ..
    return NS_ERROR_FILE_UNRECOGNIZED_PATH;
  }

  if (aMultipleComponents) {
    // can't contain .. as a path component. Ensure that the valid components
    // "foo..foo", "..foo", and "foo.." are not falsely detected,
    // but the invalid paths "..\", "foo\..", "foo\..\foo",
    // "..\foo", etc are.
    constexpr auto doubleDot = u"\\.."_ns;
    nsAString::const_iterator start, end, offset;
    aNode.BeginReading(start);
    aNode.EndReading(end);
    offset = end;
    while (FindInReadable(doubleDot, start, offset)) {
      if (offset == end || *offset == L'\\') {
        return NS_ERROR_FILE_UNRECOGNIZED_PATH;
      }
      start = offset;
      offset = end;
    }

    // catches the remaining cases of prefixes
    if (StringBeginsWith(aNode, u"..\\"_ns)) {
      return NS_ERROR_FILE_UNRECOGNIZED_PATH;
    }
  }
  // single components can't contain '\'
  else if (aNode.Contains(L'\\')) {
    return NS_ERROR_FILE_UNRECOGNIZED_PATH;
  }

  MakeDirty();

  mWorkingPath.Append('\\');
  mWorkingPath.Append(aNode);

  if (IsSpecialNTFSPath(mWorkingPath)) {
    // Revert changes to mWorkingPath:
    mWorkingPath.SetLength(mWorkingPath.Length() - aNode.Length() - 1);
    return NS_ERROR_FILE_UNRECOGNIZED_PATH;
  }

  return NS_OK;
}

nsresult nsLocalFile::OpenNSPRFileDescMaybeShareDelete(int32_t aFlags,
                                                       int32_t aMode,
                                                       bool aShareDelete,
                                                       PRFileDesc** aResult) {
  return OpenFile(mWorkingPath, aFlags, aMode, aShareDelete, aResult);
}

#define TOUPPER(u) (((u) >= L'a' && (u) <= L'z') ? (u) - (L'a' - L'A') : (u))

NS_IMETHODIMP
nsLocalFile::Normalize() {
  // XXX See bug 187957 comment 18 for possible problems with this
  // implementation.

  if (mWorkingPath.IsEmpty()) {
    return NS_OK;
  }

  nsAutoString path(mWorkingPath);

  // find the index of the root backslash for the path. Everything before
  // this is considered fully normalized and cannot be ascended beyond
  // using ".."  For a local drive this is the first slash (e.g. "c:\").
  // For a UNC path it is the slash following the share name
  // (e.g. "\\server\share\").
  int32_t rootIdx = 2;                  // default to local drive
  if (path.First() == L'\\') {          // if a share then calculate the rootIdx
    rootIdx = path.FindChar(L'\\', 2);  // skip \\ in front of the server
    if (rootIdx == kNotFound) {
      return NS_OK;  // already normalized
    }
    rootIdx = path.FindChar(L'\\', rootIdx + 1);
    if (rootIdx == kNotFound) {
      return NS_OK;  // already normalized
    }
  } else if (path.CharAt(rootIdx) != L'\\') {
    // The path has been specified relative to the current working directory
    // for that drive. To normalize it, the current working directory for
    // that drive needs to be inserted before the supplied relative path
    // which will provide an absolute path (and the rootIdx will still be 2).
    WCHAR cwd[MAX_PATH];
    WCHAR* pcwd = cwd;
    int drive = TOUPPER(path.First()) - 'A' + 1;
    /* We need to worry about IPH, for details read bug 419326.
     * _getdrives - http://msdn2.microsoft.com/en-us/library/xdhk0xd2.aspx
     * uses a bitmask, bit 0 is 'a:'
     * _chdrive - http://msdn2.microsoft.com/en-us/library/0d1409hb.aspx
     * _getdcwd - http://msdn2.microsoft.com/en-us/library/7t2zk3s4.aspx
     * take an int, 1 is 'a:'.
     *
     * Because of this, we need to do some math. Subtract 1 to convert from
     * _chdrive/_getdcwd format to _getdrives drive numbering.
     * Shift left x bits to convert from integer indexing to bitfield indexing.
     * And of course, we need to find out if the drive is in the bitmask.
     *
     * If we're really unlucky, we can still lose, but only if the user
     * manages to eject the drive between our call to _getdrives() and
     * our *calls* to _wgetdcwd.
     */
    if (!((1 << (drive - 1)) & _getdrives())) {
      return NS_ERROR_FILE_INVALID_PATH;
    }
    if (!_wgetdcwd(drive, pcwd, MAX_PATH)) {
      pcwd = _wgetdcwd(drive, 0, 0);
    }
    if (!pcwd) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    nsAutoString currentDir(pcwd);
    if (pcwd != cwd) {
      free(pcwd);
    }

    if (currentDir.Last() == '\\') {
      path.Replace(0, 2, currentDir);
    } else {
      path.Replace(0, 2, currentDir + u"\\"_ns);
    }
  }

  MOZ_ASSERT(0 < rootIdx && rootIdx < (int32_t)path.Length(),
             "rootIdx is invalid");
  MOZ_ASSERT(path.CharAt(rootIdx) == '\\', "rootIdx is invalid");

  // if there is nothing following the root path then it is already normalized
  if (rootIdx + 1 == (int32_t)path.Length()) {
    return NS_OK;
  }

  // assign the root
  const char16_t* pathBuffer = path.get();  // simplify access to the buffer
  mWorkingPath.SetCapacity(path.Length());  // it won't ever grow longer
  mWorkingPath.Assign(pathBuffer, rootIdx);

  // Normalize the path components. The actions taken are:
  //
  //  "\\"    condense to single backslash
  //  "."     remove from path
  //  ".."    up a directory
  //  "..."   remove from path (any number of dots > 2)
  //
  // The last form is something that Windows 95 and 98 supported and
  // is a shortcut for changing up multiple directories. Windows XP
  // and ilk ignore it in a path, as is done here.
  int32_t len, begin, end = rootIdx;
  while (end < (int32_t)path.Length()) {
    // find the current segment (text between the backslashes) to
    // be examined, this will set the following variables:
    //  begin == index of first char in segment
    //  end   == index 1 char after last char in segment
    //  len   == length of segment
    begin = end + 1;
    end = path.FindChar('\\', begin);
    if (end == kNotFound) {
      end = path.Length();
    }
    len = end - begin;

    // ignore double backslashes
    if (len == 0) {
      continue;
    }

    // len != 0, and interesting paths always begin with a dot
    if (pathBuffer[begin] == '.') {
      // ignore single dots
      if (len == 1) {
        continue;
      }

      // handle multiple dots
      if (len >= 2 && pathBuffer[begin + 1] == L'.') {
        // back up a path component on double dot
        if (len == 2) {
          int32_t prev = mWorkingPath.RFindChar('\\');
          if (prev >= rootIdx) {
            mWorkingPath.Truncate(prev);
          }
          continue;
        }

        // length is > 2 and the first two characters are dots.
        // if the rest of the string is dots, then ignore it.
        int idx = len - 1;
        for (; idx >= 2; --idx) {
          if (pathBuffer[begin + idx] != L'.') {
            break;
          }
        }

        // this is true if the loop above didn't break
        // and all characters in this segment are dots.
        if (idx < 2) {
          continue;
        }
      }
    }

    // add the current component to the path, including the preceding backslash
    mWorkingPath.Append(pathBuffer + begin - 1, len + 1);
  }

  // kill trailing dots and spaces.
  int32_t filePathLen = mWorkingPath.Length() - 1;
  while (filePathLen > 0 && (mWorkingPath[filePathLen] == L' ' ||
                             mWorkingPath[filePathLen] == L'.')) {
    mWorkingPath.Truncate(filePathLen--);
  }

  MakeDirty();
  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::GetLeafName(nsAString& aLeafName) {
  aLeafName.Truncate();

  if (mWorkingPath.IsEmpty()) {
    return NS_ERROR_FILE_UNRECOGNIZED_PATH;
  }

  int32_t offset = mWorkingPath.RFindChar(L'\\');

  // if the working path is just a node without any lashes.
  if (offset == kNotFound) {
    aLeafName = mWorkingPath;
  } else {
    aLeafName = Substring(mWorkingPath, offset + 1);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::SetLeafName(const nsAString& aLeafName) {
  MakeDirty();

  if (mWorkingPath.IsEmpty()) {
    return NS_ERROR_FILE_UNRECOGNIZED_PATH;
  }

  // cannot use nsCString::RFindChar() due to 0x5c problem
  int32_t offset = mWorkingPath.RFindChar(L'\\');
  nsString newDir;
  if (offset) {
    newDir = Substring(mWorkingPath, 0, offset + 1) + aLeafName;
  } else {
    newDir = mWorkingPath + aLeafName;
  }
  if (IsSpecialNTFSPath(newDir)) {
    return NS_ERROR_FILE_UNRECOGNIZED_PATH;
  }

  mWorkingPath.Assign(newDir);

  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::GetPath(nsAString& aResult) {
  MOZ_ASSERT_IF(
      mUseDOSDevicePathSyntax,
      !FilePreferences::StartsWithDiskDesignatorAndBackslash(mWorkingPath));
  aResult = mWorkingPath;
  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::GetCanonicalPath(nsAString& aResult) {
  EnsureShortPath();
  aResult.Assign(mShortWorkingPath);
  return NS_OK;
}

typedef struct {
  WORD wLanguage;
  WORD wCodePage;
} LANGANDCODEPAGE;

NS_IMETHODIMP
nsLocalFile::GetVersionInfoField(const char* aField, nsAString& aResult) {
  nsresult rv = NS_ERROR_FAILURE;

  const WCHAR* path = mWorkingPath.get();

  DWORD dummy;
  DWORD size = ::GetFileVersionInfoSizeW(path, &dummy);
  if (!size) {
    return rv;
  }

  void* ver = moz_xcalloc(size, 1);
  if (::GetFileVersionInfoW(path, 0, size, ver)) {
    LANGANDCODEPAGE* translate = nullptr;
    UINT pageCount;
    BOOL queryResult = ::VerQueryValueW(ver, L"\\VarFileInfo\\Translation",
                                        (void**)&translate, &pageCount);
    if (queryResult && translate) {
      for (int32_t i = 0; i < 2; ++i) {
        wchar_t subBlock[MAX_PATH];
        _snwprintf(subBlock, MAX_PATH, L"\\StringFileInfo\\%04x%04x\\%s",
                   (i == 0 ? translate[0].wLanguage : ::GetUserDefaultLangID()),
                   translate[0].wCodePage,
                   NS_ConvertASCIItoUTF16(nsDependentCString(aField)).get());
        subBlock[MAX_PATH - 1] = 0;
        LPVOID value = nullptr;
        UINT size;
        queryResult = ::VerQueryValueW(ver, subBlock, &value, &size);
        if (queryResult && value) {
          aResult.Assign(static_cast<char16_t*>(value));
          if (!aResult.IsEmpty()) {
            rv = NS_OK;
            break;
          }
        }
      }
    }
  }
  free(ver);

  return rv;
}

NS_IMETHODIMP
nsLocalFile::OpenNSPRFileDescShareDelete(int32_t aFlags, int32_t aMode,
                                         PRFileDesc** aResult) {
  nsresult rv = OpenNSPRFileDescMaybeShareDelete(aFlags, aMode, true, aResult);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_OK;
}

/**
 * Determines if the drive type for the specified file is rmeote or local.
 *
 * @param path   The path of the file to check
 * @param remote Out parameter, on function success holds true if the specified
 *               file path is remote, or false if the file path is local.
 * @return true  on success. The return value implies absolutely nothing about
 *               wether the file is local or remote.
 */
static bool IsRemoteFilePath(LPCWSTR aPath, bool& aRemote) {
  // Obtain the parent directory path and make sure it ends with
  // a trailing backslash.
  WCHAR dirPath[MAX_PATH + 1] = {0};
  wcsncpy(dirPath, aPath, MAX_PATH);
  if (!PathRemoveFileSpecW(dirPath)) {
    return false;
  }
  size_t len = wcslen(dirPath);
  // In case the dirPath holds exaclty MAX_PATH and remains unchanged, we
  // recheck the required length here since we need to terminate it with
  // a backslash.
  if (len >= MAX_PATH) {
    return false;
  }

  dirPath[len] = L'\\';
  dirPath[len + 1] = L'\0';
  UINT driveType = GetDriveTypeW(dirPath);
  aRemote = driveType == DRIVE_REMOTE;
  return true;
}

nsresult nsLocalFile::CopySingleFile(nsIFile* aSourceFile, nsIFile* aDestParent,
                                     const nsAString& aNewName,
                                     uint32_t aOptions) {
  nsresult rv = NS_OK;
  nsAutoString filePath;

  bool move = aOptions & (Move | Rename);

  // get the path that we are going to copy to.
  // Since windows does not know how to auto
  // resolve shortcuts, we must work with the
  // target.
  nsAutoString destPath;
  rv = aDestParent->GetTarget(destPath);
  if (NS_FAILED(rv)) {
    return rv;
  }

  destPath.Append('\\');

  if (aNewName.IsEmpty()) {
    nsAutoString aFileName;
    aSourceFile->GetLeafName(aFileName);
    destPath.Append(aFileName);
  } else {
    destPath.Append(aNewName);
  }

  if (aOptions & FollowSymlinks) {
    rv = aSourceFile->GetTarget(filePath);
    if (filePath.IsEmpty()) {
      rv = aSourceFile->GetPath(filePath);
    }
  } else {
    rv = aSourceFile->GetPath(filePath);
  }

  if (NS_FAILED(rv)) {
    return rv;
  }

#ifdef DEBUG
  nsCOMPtr<nsILocalFileWin> srcWinFile = do_QueryInterface(aSourceFile);
  MOZ_ASSERT(srcWinFile);

  bool srcUseDOSDevicePathSyntax;
  srcWinFile->GetUseDOSDevicePathSyntax(&srcUseDOSDevicePathSyntax);

  nsCOMPtr<nsILocalFileWin> destWinFile = do_QueryInterface(aDestParent);
  MOZ_ASSERT(destWinFile);

  bool destUseDOSDevicePathSyntax;
  destWinFile->GetUseDOSDevicePathSyntax(&destUseDOSDevicePathSyntax);

  MOZ_ASSERT(srcUseDOSDevicePathSyntax == destUseDOSDevicePathSyntax,
             "Copy or Move files with different values for "
             "useDOSDevicePathSyntax would fail");
#endif

  if (FilePreferences::IsBlockedUNCPath(destPath)) {
    return NS_ERROR_FILE_ACCESS_DENIED;
  }

  int copyOK = 0;
  if (move) {
    copyOK = ::MoveFileExW(filePath.get(), destPath.get(),
                           MOVEFILE_REPLACE_EXISTING);
  }

  // If we either failed to move the file, or this is a copy, try copying:
  if (!copyOK && (!move || GetLastError() == ERROR_NOT_SAME_DEVICE)) {
    // Failed renames here should just return access denied.
    if (move && (aOptions & Rename)) {
      return NS_ERROR_FILE_ACCESS_DENIED;
    }

    // Pass the flag COPY_FILE_NO_BUFFERING to CopyFileEx as we may be copying
    // to a SMBV2 remote drive. Without this parameter subsequent append mode
    // file writes can cause the resultant file to become corrupt. We only need
    // to do this if the major version of Windows is > 5(Only Windows Vista and
    // above can support SMBV2).  With a 7200RPM hard drive: Copying a 1KB file
    // with COPY_FILE_NO_BUFFERING takes about 30-60ms. Copying a 1KB file
    // without COPY_FILE_NO_BUFFERING takes < 1ms. So we only use
    // COPY_FILE_NO_BUFFERING when we have a remote drive.
    DWORD dwCopyFlags = COPY_FILE_ALLOW_DECRYPTED_DESTINATION;
    bool path1Remote, path2Remote;
    if (!IsRemoteFilePath(filePath.get(), path1Remote) ||
        !IsRemoteFilePath(destPath.get(), path2Remote) || path1Remote ||
        path2Remote) {
      dwCopyFlags |= COPY_FILE_NO_BUFFERING;
    }

    copyOK = ::CopyFileExW(filePath.get(), destPath.get(), nullptr, nullptr,
                           nullptr, dwCopyFlags);
    // On Windows 10, copying without buffering has started failing, so try
    // with buffering...
    if (!copyOK && (dwCopyFlags & COPY_FILE_NO_BUFFERING) &&
        GetLastError() == ERROR_INVALID_PARAMETER) {
      dwCopyFlags &= ~COPY_FILE_NO_BUFFERING;
      copyOK = ::CopyFileExW(filePath.get(), destPath.get(), nullptr, nullptr,
                             nullptr, dwCopyFlags);
    }

    if (move && copyOK) {
      DeleteFileW(filePath.get());
    }
  }

  if (!copyOK) {  // CopyFileEx and MoveFileEx return zero at failure.
    rv = ConvertWinError(GetLastError());
  } else if (move && !(aOptions & SkipNtfsAclReset)) {
    // Set security permissions to inherit from parent.
    // Note: propagates to all children: slow for big file trees
    PACL pOldDACL = nullptr;
    PSECURITY_DESCRIPTOR pSD = nullptr;
    ::GetNamedSecurityInfoW((LPWSTR)destPath.get(), SE_FILE_OBJECT,
                            DACL_SECURITY_INFORMATION, nullptr, nullptr,
                            &pOldDACL, nullptr, &pSD);
    if (pOldDACL)
      ::SetNamedSecurityInfoW(
          (LPWSTR)destPath.get(), SE_FILE_OBJECT,
          DACL_SECURITY_INFORMATION | UNPROTECTED_DACL_SECURITY_INFORMATION,
          nullptr, nullptr, pOldDACL, nullptr);
    if (pSD) {
      LocalFree((HLOCAL)pSD);
    }
  }

  return rv;
}

nsresult nsLocalFile::CopyMove(nsIFile* aParentDir, const nsAString& aNewName,
                               uint32_t aOptions) {
  bool move = aOptions & (Move | Rename);
  bool followSymlinks = aOptions & FollowSymlinks;
  // If we're not provided with a new parent, we're copying or moving to
  // another file in the same directory and can safely skip checking if the
  // destination directory exists:
  bool targetInSameDirectory = !aParentDir;

  nsCOMPtr<nsIFile> newParentDir = aParentDir;
  // check to see if this exists, otherwise return an error.
  // we will check this by resolving.  If the user wants us
  // to follow links, then we are talking about the target,
  // hence we can use the |FollowSymlinks| option.
  nsresult rv = ResolveAndStat();
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (!newParentDir) {
    // no parent was specified.  We must rename.
    if (aNewName.IsEmpty()) {
      return NS_ERROR_INVALID_ARG;
    }

    rv = GetParent(getter_AddRefs(newParentDir));
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  if (!newParentDir) {
    return NS_ERROR_FILE_DESTINATION_NOT_DIR;
  }

  if (!targetInSameDirectory) {
    // make sure it exists and is a directory.  Create it if not there.
    bool exists = false;
    rv = newParentDir->Exists(&exists);
    if (NS_FAILED(rv)) {
      return rv;
    }

    if (!exists) {
      rv = newParentDir->Create(DIRECTORY_TYPE,
                                0644);  // TODO, what permissions should we use
      if (NS_FAILED(rv)) {
        return rv;
      }
    } else {
      bool isDir = false;
      rv = newParentDir->IsDirectory(&isDir);
      if (NS_FAILED(rv)) {
        return rv;
      }

      if (!isDir) {
        if (followSymlinks) {
          bool isLink = false;
          rv = newParentDir->IsSymlink(&isLink);
          if (NS_FAILED(rv)) {
            return rv;
          }

          if (isLink) {
            nsAutoString target;
            rv = newParentDir->GetTarget(target);
            if (NS_FAILED(rv)) {
              return rv;
            }

            nsCOMPtr<nsIFile> realDest = new nsLocalFile();
            rv = realDest->InitWithPath(target);
            if (NS_FAILED(rv)) {
              return rv;
            }

            return CopyMove(realDest, aNewName, aOptions);
          }
        } else {
          return NS_ERROR_FILE_DESTINATION_NOT_DIR;
        }
      }
    }
  }

  // Try different ways to move/copy files/directories
  bool done = false;

  bool isDir = false;
  rv = IsDirectory(&isDir);
  if (NS_FAILED(rv)) {
    return rv;
  }

  bool isSymlink = false;
  rv = IsSymlink(&isSymlink);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Try to move the file or directory, or try to copy a single file (or
  // non-followed symlink)
  if (move || !isDir || (isSymlink && !followSymlinks)) {
    // Copy/Move single file, or move a directory
    if (!aParentDir) {
      aOptions |= SkipNtfsAclReset;
    }
    rv = CopySingleFile(this, newParentDir, aNewName, aOptions);
    done = NS_SUCCEEDED(rv);
    // If we are moving a directory and that fails, fallback on directory
    // enumeration.  See bug 231300 for details.
    if (!done && !(move && isDir)) {
      return rv;
    }
  }

  // Not able to copy or move directly, so enumerate it
  if (!done) {
    // create a new target destination in the new parentDir;
    nsCOMPtr<nsIFile> target;
    rv = newParentDir->Clone(getter_AddRefs(target));
    if (NS_FAILED(rv)) {
      return rv;
    }

    nsAutoString allocatedNewName;
    if (aNewName.IsEmpty()) {
      bool isLink = false;
      rv = IsSymlink(&isLink);
      if (NS_FAILED(rv)) {
        return rv;
      }

      if (isLink) {
        nsAutoString temp;
        rv = GetTarget(temp);
        if (NS_FAILED(rv)) {
          return rv;
        }

        int32_t offset = temp.RFindChar(L'\\');
        if (offset == kNotFound) {
          allocatedNewName = temp;
        } else {
          allocatedNewName = Substring(temp, offset + 1);
        }
      } else {
        GetLeafName(allocatedNewName);  // this should be the leaf name of the
      }
    } else {
      allocatedNewName = aNewName;
    }

    rv = target->Append(allocatedNewName);
    if (NS_FAILED(rv)) {
      return rv;
    }

    allocatedNewName.Truncate();

    bool exists = false;
    // check if the destination directory already exists
    rv = target->Exists(&exists);
    if (NS_FAILED(rv)) {
      return rv;
    }

    if (!exists) {
      // if the destination directory cannot be created, return an error
      rv = target->Create(DIRECTORY_TYPE,
                          0644);  // TODO, what permissions should we use
      if (NS_FAILED(rv)) {
        return rv;
      }
    } else {
      // check if the destination directory is writable and empty
      bool isWritable = false;
      rv = target->IsWritable(&isWritable);
      if (NS_FAILED(rv)) {
        return rv;
      }

      if (!isWritable) {
        return NS_ERROR_FILE_ACCESS_DENIED;
      }

      nsCOMPtr<nsIDirectoryEnumerator> targetIterator;
      rv = target->GetDirectoryEntries(getter_AddRefs(targetIterator));
      if (NS_FAILED(rv)) {
        return rv;
      }

      bool more;
      targetIterator->HasMoreElements(&more);
      // return error if target directory is not empty
      if (more) {
        return NS_ERROR_FILE_DIR_NOT_EMPTY;
      }
    }

    RefPtr<nsDirEnumerator> dirEnum = new nsDirEnumerator();

    rv = dirEnum->Init(this);
    if (NS_FAILED(rv)) {
      NS_WARNING("dirEnum initialization failed");
      return rv;
    }

    nsCOMPtr<nsIFile> file;
    while (NS_SUCCEEDED(dirEnum->GetNextFile(getter_AddRefs(file))) && file) {
      bool isDir = false;
      rv = file->IsDirectory(&isDir);
      if (NS_FAILED(rv)) {
        return rv;
      }

      bool isLink = false;
      rv = file->IsSymlink(&isLink);
      if (NS_FAILED(rv)) {
        return rv;
      }

      if (move) {
        if (followSymlinks) {
          return NS_ERROR_FAILURE;
        }

        rv = file->MoveTo(target, u""_ns);
        if (NS_FAILED(rv)) {
          return rv;
        }
      } else {
        if (followSymlinks) {
          rv = file->CopyToFollowingLinks(target, u""_ns);
        } else {
          rv = file->CopyTo(target, u""_ns);
        }
        if (NS_FAILED(rv)) {
          return rv;
        }
      }
    }
    // we've finished moving all the children of this directory
    // in the new directory.  so now delete the directory
    // note, we don't need to do a recursive delete.
    // MoveTo() is recursive.  At this point,
    // we've already moved the children of the current folder
    // to the new location.  nothing should be left in the folder.
    if (move) {
      rv = Remove(false /* recursive */);
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
  }

  // If we moved, we want to adjust this.
  if (move) {
    MakeDirty();

    nsAutoString newParentPath;
    newParentDir->GetPath(newParentPath);

    if (newParentPath.IsEmpty()) {
      return NS_ERROR_FAILURE;
    }

    if (aNewName.IsEmpty()) {
      nsAutoString aFileName;
      GetLeafName(aFileName);

      InitWithPath(newParentPath);
      Append(aFileName);
    } else {
      InitWithPath(newParentPath);
      Append(aNewName);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::CopyTo(nsIFile* aNewParentDir, const nsAString& aNewName) {
  return CopyMove(aNewParentDir, aNewName, 0);
}

NS_IMETHODIMP
nsLocalFile::CopyToFollowingLinks(nsIFile* aNewParentDir,
                                  const nsAString& aNewName) {
  return CopyMove(aNewParentDir, aNewName, FollowSymlinks);
}

NS_IMETHODIMP
nsLocalFile::MoveTo(nsIFile* aNewParentDir, const nsAString& aNewName) {
  return CopyMove(aNewParentDir, aNewName, Move);
}

NS_IMETHODIMP
nsLocalFile::MoveToFollowingLinks(nsIFile* aNewParentDir,
                                  const nsAString& aNewName) {
  return CopyMove(aNewParentDir, aNewName, Move | FollowSymlinks);
}

NS_IMETHODIMP
nsLocalFile::RenameTo(nsIFile* aNewParentDir, const nsAString& aNewName) {
  // If we're not provided with a new parent, we're renaming inside one and
  // the same directory and can safely skip checking if the destination
  // directory exists:
  bool targetInSameDirectory = !aNewParentDir;

  nsCOMPtr<nsIFile> targetParentDir = aNewParentDir;
  // check to see if this exists, otherwise return an error.
  // we will check this by resolving.  If the user wants us
  // to follow links, then we are talking about the target,
  // hence we can use the |followSymlinks| parameter.
  nsresult rv = ResolveAndStat();
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (!targetParentDir) {
    // no parent was specified.  We must rename.
    if (aNewName.IsEmpty()) {
      return NS_ERROR_INVALID_ARG;
    }
    rv = GetParent(getter_AddRefs(targetParentDir));
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  if (!targetParentDir) {
    return NS_ERROR_FILE_DESTINATION_NOT_DIR;
  }

  if (!targetInSameDirectory) {
    // make sure it exists and is a directory.  Create it if not there.
    bool exists = false;
    rv = targetParentDir->Exists(&exists);
    if (NS_FAILED(rv)) {
      return rv;
    }

    if (!exists) {
      rv = targetParentDir->Create(DIRECTORY_TYPE, 0644);
      if (NS_FAILED(rv)) {
        return rv;
      }
    } else {
      bool isDir = false;
      rv = targetParentDir->IsDirectory(&isDir);
      if (NS_FAILED(rv)) {
        return rv;
      }
      if (!isDir) {
        return NS_ERROR_FILE_DESTINATION_NOT_DIR;
      }
    }
  }

  uint32_t options = Rename;
  if (!aNewParentDir) {
    options |= SkipNtfsAclReset;
  }
  // Move single file, or move a directory
  return CopySingleFile(this, targetParentDir, aNewName, options);
}

NS_IMETHODIMP
nsLocalFile::RenameToNative(nsIFile* aNewParentDir,
                            const nsACString& aNewName) {
  nsAutoString tmp;
  nsresult rv = NS_CopyNativeToUnicode(aNewName, tmp);
  if (NS_SUCCEEDED(rv)) {
    return RenameTo(aNewParentDir, tmp);
  }

  return rv;
}

NS_IMETHODIMP
nsLocalFile::Load(PRLibrary** aResult) {
  // Check we are correctly initialized.
  CHECK_mWorkingPath();
  if (NS_WARN_IF(!aResult)) {
    return NS_ERROR_INVALID_ARG;
  }

#ifdef NS_BUILD_REFCNT_LOGGING
  nsTraceRefcnt::SetActivityIsLegal(false);
#endif

  PRLibSpec libSpec;
  libSpec.value.pathname_u = mWorkingPath.get();
  libSpec.type = PR_LibSpec_PathnameU;
  *aResult = PR_LoadLibraryWithFlags(libSpec, 0);

#ifdef NS_BUILD_REFCNT_LOGGING
  nsTraceRefcnt::SetActivityIsLegal(true);
#endif

  if (*aResult) {
    return NS_OK;
  }
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsLocalFile::Remove(bool aRecursive) {
  // NOTE:
  //
  // if the working path points to a shortcut, then we will only
  // delete the shortcut itself.  even if the shortcut points to
  // a directory, we will not recurse into that directory or
  // delete that directory itself.  likewise, if the shortcut
  // points to a normal file, we will not delete the real file.
  // this is done to be consistent with the other platforms that
  // behave this way.  we do this even if the followLinks attribute
  // is set to true.  this helps protect against misuse that could
  // lead to security bugs (e.g., bug 210588).
  //
  // Since shortcut files are no longer permitted to be used as unix-like
  // symlinks interspersed in the path (e.g. "c:/file.lnk/foo/bar.txt")
  // this processing is a lot simpler. Even if the shortcut file is
  // pointing to a directory, only the mWorkingPath value is used and so
  // only the shortcut file will be deleted.

  // Check we are correctly initialized.
  CHECK_mWorkingPath();

  nsresult rv = NS_OK;

  bool isLink = false;
  rv = IsSymlink(&isLink);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // only check to see if we have a directory if it isn't a link
  bool isDir = false;
  if (!isLink) {
    rv = IsDirectory(&isDir);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  if (isDir) {
    if (aRecursive) {
      RefPtr<nsDirEnumerator> dirEnum = new nsDirEnumerator();

      rv = dirEnum->Init(this);
      if (NS_FAILED(rv)) {
        return rv;
      }

      bool more = false;
      while (NS_SUCCEEDED(dirEnum->HasMoreElements(&more)) && more) {
        nsCOMPtr<nsISupports> item;
        dirEnum->GetNext(getter_AddRefs(item));
        nsCOMPtr<nsIFile> file = do_QueryInterface(item);
        if (file) {
          file->Remove(aRecursive);
        }
      }
    }
    if (RemoveDirectoryW(mWorkingPath.get()) == 0) {
      return ConvertWinError(GetLastError());
    }
  } else {
    if (DeleteFileW(mWorkingPath.get()) == 0) {
      return ConvertWinError(GetLastError());
    }
  }

  MakeDirty();
  return rv;
}

NS_IMETHODIMP
nsLocalFile::GetLastModifiedTime(PRTime* aLastModifiedTime) {
  // Check we are correctly initialized.
  CHECK_mWorkingPath();

  if (NS_WARN_IF(!aLastModifiedTime)) {
    return NS_ERROR_INVALID_ARG;
  }

  // get the modified time of the target as determined by mFollowSymlinks
  // If true, then this will be for the target of the shortcut file,
  // otherwise it will be for the shortcut file itself (i.e. the same
  // results as GetLastModifiedTimeOfLink)

  nsresult rv = ResolveAndStat();
  if (NS_FAILED(rv)) {
    return rv;
  }

  // microseconds -> milliseconds
  *aLastModifiedTime = mFileInfo64.modifyTime / PR_USEC_PER_MSEC;
  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::GetLastModifiedTimeOfLink(PRTime* aLastModifiedTime) {
  // Check we are correctly initialized.
  CHECK_mWorkingPath();

  if (NS_WARN_IF(!aLastModifiedTime)) {
    return NS_ERROR_INVALID_ARG;
  }

  // The caller is assumed to have already called IsSymlink
  // and to have found that this file is a link.

  PRFileInfo64 info;
  nsresult rv = GetFileInfo(mWorkingPath, &info);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // microseconds -> milliseconds
  *aLastModifiedTime = info.modifyTime / PR_USEC_PER_MSEC;
  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::SetLastModifiedTime(PRTime aLastModifiedTime) {
  // Check we are correctly initialized.
  CHECK_mWorkingPath();

  nsresult rv = SetModDate(aLastModifiedTime, mWorkingPath.get());
  if (NS_SUCCEEDED(rv)) {
    MakeDirty();
  }

  return rv;
}

NS_IMETHODIMP
nsLocalFile::SetLastModifiedTimeOfLink(PRTime aLastModifiedTime) {
  return SetLastModifiedTime(aLastModifiedTime);
}

NS_IMETHODIMP
nsLocalFile::GetCreationTime(PRTime* aCreationTime) {
  CHECK_mWorkingPath();

  if (NS_WARN_IF(!aCreationTime)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsresult rv = ResolveAndStat();
  NS_ENSURE_SUCCESS(rv, rv);

  *aCreationTime = mFileInfo64.creationTime / PR_USEC_PER_MSEC;

  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::GetCreationTimeOfLink(PRTime* aCreationTime) {
  CHECK_mWorkingPath();

  if (NS_WARN_IF(!aCreationTime)) {
    return NS_ERROR_INVALID_ARG;
  }

  PRFileInfo64 info;
  nsresult rv = GetFileInfo(mWorkingPath, &info);
  NS_ENSURE_SUCCESS(rv, rv);

  *aCreationTime = info.creationTime / PR_USEC_PER_MSEC;

  return NS_OK;
}

nsresult nsLocalFile::SetModDate(PRTime aLastModifiedTime,
                                 const wchar_t* aFilePath) {
  // The FILE_FLAG_BACKUP_SEMANTICS is required in order to change the
  // modification time for directories.
  HANDLE file = ::CreateFileW(aFilePath,      // pointer to name of the file
                              GENERIC_WRITE,  // access (write) mode
                              0,              // share mode
                              nullptr,        // pointer to security attributes
                              OPEN_EXISTING,  // how to create
                              FILE_FLAG_BACKUP_SEMANTICS,  // file attributes
                              nullptr);

  if (file == INVALID_HANDLE_VALUE) {
    return ConvertWinError(GetLastError());
  }

  FILETIME ft;
  SYSTEMTIME st;
  PRExplodedTime pret;

  // PR_ExplodeTime expects usecs...
  PR_ExplodeTime(aLastModifiedTime * PR_USEC_PER_MSEC, PR_GMTParameters, &pret);
  st.wYear = pret.tm_year;
  st.wMonth =
      pret.tm_month + 1;  // Convert start offset -- Win32: Jan=1; NSPR: Jan=0
  st.wDayOfWeek = pret.tm_wday;
  st.wDay = pret.tm_mday;
  st.wHour = pret.tm_hour;
  st.wMinute = pret.tm_min;
  st.wSecond = pret.tm_sec;
  st.wMilliseconds = pret.tm_usec / 1000;

  nsresult rv = NS_OK;
  // if at least one of these fails...
  if (!(SystemTimeToFileTime(&st, &ft) != 0 &&
        SetFileTime(file, nullptr, &ft, &ft) != 0)) {
    rv = ConvertWinError(GetLastError());
  }

  CloseHandle(file);
  return rv;
}

NS_IMETHODIMP
nsLocalFile::GetPermissions(uint32_t* aPermissions) {
  if (NS_WARN_IF(!aPermissions)) {
    return NS_ERROR_INVALID_ARG;
  }

  // get the permissions of the target as determined by mFollowSymlinks
  // If true, then this will be for the target of the shortcut file,
  // otherwise it will be for the shortcut file itself (i.e. the same
  // results as GetPermissionsOfLink)
  nsresult rv = ResolveAndStat();
  if (NS_FAILED(rv)) {
    return rv;
  }

  bool isWritable = false;
  rv = IsWritable(&isWritable);
  if (NS_FAILED(rv)) {
    return rv;
  }

  bool isExecutable = false;
  rv = IsExecutable(&isExecutable);
  if (NS_FAILED(rv)) {
    return rv;
  }

  *aPermissions = PR_IRUSR | PR_IRGRP | PR_IROTH;  // all read
  if (isWritable) {
    *aPermissions |= PR_IWUSR | PR_IWGRP | PR_IWOTH;  // all write
  }
  if (isExecutable) {
    *aPermissions |= PR_IXUSR | PR_IXGRP | PR_IXOTH;  // all execute
  }

  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::GetPermissionsOfLink(uint32_t* aPermissions) {
  // Check we are correctly initialized.
  CHECK_mWorkingPath();

  if (NS_WARN_IF(!aPermissions)) {
    return NS_ERROR_INVALID_ARG;
  }

  // The caller is assumed to have already called IsSymlink
  // and to have found that this file is a link. It is not
  // possible for a link file to be executable.

  DWORD word = ::GetFileAttributesW(mWorkingPath.get());
  if (word == INVALID_FILE_ATTRIBUTES) {
    return NS_ERROR_FILE_INVALID_PATH;
  }

  bool isWritable = !(word & FILE_ATTRIBUTE_READONLY);
  *aPermissions = PR_IRUSR | PR_IRGRP | PR_IROTH;  // all read
  if (isWritable) {
    *aPermissions |= PR_IWUSR | PR_IWGRP | PR_IWOTH;  // all write
  }

  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::SetPermissions(uint32_t aPermissions) {
  // Check we are correctly initialized.
  CHECK_mWorkingPath();

  // set the permissions of the target as determined by mFollowSymlinks
  // If true, then this will be for the target of the shortcut file,
  // otherwise it will be for the shortcut file itself (i.e. the same
  // results as SetPermissionsOfLink)
  nsresult rv = Resolve();
  if (NS_FAILED(rv)) {
    return rv;
  }

  // windows only knows about the following permissions
  int mode = 0;
  if (aPermissions & (PR_IRUSR | PR_IRGRP | PR_IROTH)) {  // any read
    mode |= _S_IREAD;
  }
  if (aPermissions & (PR_IWUSR | PR_IWGRP | PR_IWOTH)) {  // any write
    mode |= _S_IWRITE;
  }

  if (_wchmod(mResolvedPath.get(), mode) == -1) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::SetPermissionsOfLink(uint32_t aPermissions) {
  // The caller is assumed to have already called IsSymlink
  // and to have found that this file is a link.

  // windows only knows about the following permissions
  int mode = 0;
  if (aPermissions & (PR_IRUSR | PR_IRGRP | PR_IROTH)) {  // any read
    mode |= _S_IREAD;
  }
  if (aPermissions & (PR_IWUSR | PR_IWGRP | PR_IWOTH)) {  // any write
    mode |= _S_IWRITE;
  }

  if (_wchmod(mWorkingPath.get(), mode) == -1) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::GetFileSize(int64_t* aFileSize) {
  if (NS_WARN_IF(!aFileSize)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsresult rv = ResolveAndStat();
  if (NS_FAILED(rv)) {
    return rv;
  }

  *aFileSize = mFileInfo64.size;
  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::GetFileSizeOfLink(int64_t* aFileSize) {
  // Check we are correctly initialized.
  CHECK_mWorkingPath();

  if (NS_WARN_IF(!aFileSize)) {
    return NS_ERROR_INVALID_ARG;
  }

  // The caller is assumed to have already called IsSymlink
  // and to have found that this file is a link.

  PRFileInfo64 info;
  if (NS_FAILED(GetFileInfo(mWorkingPath, &info))) {
    return NS_ERROR_FILE_INVALID_PATH;
  }

  *aFileSize = info.size;
  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::SetFileSize(int64_t aFileSize) {
  // Check we are correctly initialized.
  CHECK_mWorkingPath();

  HANDLE hFile =
      ::CreateFileW(mWorkingPath.get(),     // pointer to name of the file
                    GENERIC_WRITE,          // access (write) mode
                    FILE_SHARE_READ,        // share mode
                    nullptr,                // pointer to security attributes
                    OPEN_EXISTING,          // how to create
                    FILE_ATTRIBUTE_NORMAL,  // file attributes
                    nullptr);
  if (hFile == INVALID_HANDLE_VALUE) {
    return ConvertWinError(GetLastError());
  }

  // seek the file pointer to the new, desired end of file
  // and then truncate the file at that position
  nsresult rv = NS_ERROR_FAILURE;
  aFileSize = MyFileSeek64(hFile, aFileSize, FILE_BEGIN);
  if (aFileSize != -1 && SetEndOfFile(hFile)) {
    MakeDirty();
    rv = NS_OK;
  }

  CloseHandle(hFile);
  return rv;
}

static nsresult GetDiskSpaceAttributes(const nsString& aResolvedPath,
                                       int64_t* aFreeBytesAvailable,
                                       int64_t* aTotalBytes) {
  ULARGE_INTEGER liFreeBytesAvailableToCaller;
  ULARGE_INTEGER liTotalNumberOfBytes;
  if (::GetDiskFreeSpaceExW(aResolvedPath.get(), &liFreeBytesAvailableToCaller,
                            &liTotalNumberOfBytes, nullptr)) {
    *aFreeBytesAvailable = liFreeBytesAvailableToCaller.QuadPart;
    *aTotalBytes = liTotalNumberOfBytes.QuadPart;

    return NS_OK;
  }

  return ConvertWinError(::GetLastError());
}

NS_IMETHODIMP
nsLocalFile::GetDiskSpaceAvailable(int64_t* aDiskSpaceAvailable) {
  // Check we are correctly initialized.
  CHECK_mWorkingPath();

  if (NS_WARN_IF(!aDiskSpaceAvailable)) {
    return NS_ERROR_INVALID_ARG;
  }

  *aDiskSpaceAvailable = 0;

  nsresult rv = ResolveAndStat();
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (mFileInfo64.type == PR_FILE_FILE) {
    // Since GetDiskFreeSpaceExW works only on directories, use the parent.
    nsCOMPtr<nsIFile> parent;
    if (NS_SUCCEEDED(GetParent(getter_AddRefs(parent))) && parent) {
      return parent->GetDiskSpaceAvailable(aDiskSpaceAvailable);
    }
  }

  int64_t dummy = 0;
  return GetDiskSpaceAttributes(mResolvedPath, aDiskSpaceAvailable, &dummy);
}

NS_IMETHODIMP
nsLocalFile::GetDiskCapacity(int64_t* aDiskCapacity) {
  // Check we are correctly initialized.
  CHECK_mWorkingPath();

  if (NS_WARN_IF(!aDiskCapacity)) {
    return NS_ERROR_INVALID_ARG;
  }

  *aDiskCapacity = 0;

  nsresult rv = ResolveAndStat();
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (mFileInfo64.type == PR_FILE_FILE) {
    // Since GetDiskFreeSpaceExW works only on directories, use the parent.
    nsCOMPtr<nsIFile> parent;
    if (NS_SUCCEEDED(GetParent(getter_AddRefs(parent))) && parent) {
      return parent->GetDiskCapacity(aDiskCapacity);
    }
  }

  int64_t dummy = 0;
  return GetDiskSpaceAttributes(mResolvedPath, &dummy, aDiskCapacity);
}

NS_IMETHODIMP
nsLocalFile::GetParent(nsIFile** aParent) {
  // Check we are correctly initialized.
  CHECK_mWorkingPath();

  if (NS_WARN_IF(!aParent)) {
    return NS_ERROR_INVALID_ARG;
  }

  // A two-character path must be a drive such as C:, so it has no parent
  if (mWorkingPath.Length() == 2) {
    *aParent = nullptr;
    return NS_OK;
  }

  int32_t offset = mWorkingPath.RFindChar(char16_t('\\'));
  // adding this offset check that was removed in bug 241708 fixes mail
  // directories that aren't relative to/underneath the profile dir.
  // e.g., on a different drive. Before you remove them, please make
  // sure local mail directories that aren't underneath the profile dir work.
  if (offset == kNotFound) {
    return NS_ERROR_FILE_UNRECOGNIZED_PATH;
  }

  // A path of the form \\NAME is a top-level path and has no parent
  if (offset == 1 && mWorkingPath[0] == L'\\') {
    *aParent = nullptr;
    return NS_OK;
  }

  nsAutoString parentPath(mWorkingPath);

  if (offset > 0) {
    parentPath.Truncate(offset);
  } else {
    parentPath.AssignLiteral("\\\\.");
  }

  nsCOMPtr<nsIFile> localFile;
  nsresult rv = NewLocalFile(parentPath, mUseDOSDevicePathSyntax,
                             getter_AddRefs(localFile));
  if (NS_FAILED(rv)) {
    return rv;
  }

  localFile.forget(aParent);
  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::Exists(bool* aResult) {
  // Check we are correctly initialized.
  CHECK_mWorkingPath();

  if (NS_WARN_IF(!aResult)) {
    return NS_ERROR_INVALID_ARG;
  }
  *aResult = false;

  MakeDirty();
  nsresult rv = ResolveAndStat();
  *aResult = NS_SUCCEEDED(rv) || rv == NS_ERROR_FILE_IS_LOCKED;

  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::IsWritable(bool* aIsWritable) {
  // Check we are correctly initialized.
  CHECK_mWorkingPath();

  // The read-only attribute on a FAT directory only means that it can't
  // be deleted. It is still possible to modify the contents of the directory.
  nsresult rv = IsDirectory(aIsWritable);
  if (rv == NS_ERROR_FILE_ACCESS_DENIED) {
    *aIsWritable = true;
    return NS_OK;
  } else if (rv == NS_ERROR_FILE_IS_LOCKED) {
    // If the file is normally allowed write access
    // we should still return that the file is writable.
  } else if (NS_FAILED(rv)) {
    return rv;
  }
  if (*aIsWritable) {
    return NS_OK;
  }

  // writable if the file doesn't have the readonly attribute
  rv = HasFileAttribute(FILE_ATTRIBUTE_READONLY, aIsWritable);
  if (rv == NS_ERROR_FILE_ACCESS_DENIED) {
    *aIsWritable = false;
    return NS_OK;
  } else if (rv == NS_ERROR_FILE_IS_LOCKED) {
    // If the file is normally allowed write access
    // we should still return that the file is writable.
  } else if (NS_FAILED(rv)) {
    return rv;
  }
  *aIsWritable = !*aIsWritable;

  // If the read only attribute is not set, check to make sure
  // we can open the file with write access.
  if (*aIsWritable) {
    PRFileDesc* file;
    rv = OpenFile(mResolvedPath, PR_WRONLY, 0, false, &file);
    if (NS_SUCCEEDED(rv)) {
      PR_Close(file);
    } else if (rv == NS_ERROR_FILE_ACCESS_DENIED) {
      *aIsWritable = false;
    } else if (rv == NS_ERROR_FILE_IS_LOCKED) {
      // If it is locked and read only we would have
      // gotten access denied
      *aIsWritable = true;
    } else {
      return rv;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::IsReadable(bool* aResult) {
  // Check we are correctly initialized.
  CHECK_mWorkingPath();

  if (NS_WARN_IF(!aResult)) {
    return NS_ERROR_INVALID_ARG;
  }
  *aResult = false;

  nsresult rv = ResolveAndStat();
  if (NS_FAILED(rv)) {
    return rv;
  }

  *aResult = true;
  return NS_OK;
}

nsresult nsLocalFile::LookupExtensionIn(const char* const* aExtensionsArray,
                                        size_t aArrayLength, bool* aResult) {
  // Check we are correctly initialized.
  CHECK_mWorkingPath();

  if (NS_WARN_IF(!aResult)) {
    return NS_ERROR_INVALID_ARG;
  }
  *aResult = false;

  nsresult rv;

  // only files can be executables
  bool isFile;
  rv = IsFile(&isFile);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (!isFile) {
    return NS_OK;
  }

  // TODO: shouldn't we be checking mFollowSymlinks here?
  bool symLink = false;
  rv = IsSymlink(&symLink);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsAutoString path;
  if (symLink) {
    GetTarget(path);
  } else {
    GetPath(path);
  }

  // kill trailing dots and spaces.
  int32_t filePathLen = path.Length() - 1;
  while (filePathLen > 0 &&
         (path[filePathLen] == L' ' || path[filePathLen] == L'.')) {
    path.Truncate(filePathLen--);
  }

  // Get extension.
  int32_t dotIdx = path.RFindChar(char16_t('.'));
  if (dotIdx != kNotFound) {
    // Convert extension to lower case.
    char16_t* p = path.BeginWriting();
    for (p += dotIdx + 1; *p; ++p) {
      *p += (*p >= L'A' && *p <= L'Z') ? 'a' - 'A' : 0;
    }

    nsDependentSubstring ext = Substring(path, dotIdx);
    for (size_t i = 0; i < aArrayLength; ++i) {
      if (ext.EqualsASCII(aExtensionsArray[i])) {
        // Found a match.  Set result and quit.
        *aResult = true;
        break;
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::IsExecutable(bool* aResult) {
  return LookupExtensionIn(sExecutableExts, ArrayLength(sExecutableExts),
                           aResult);
}

NS_IMETHODIMP
nsLocalFile::IsDirectory(bool* aResult) {
  return HasFileAttribute(FILE_ATTRIBUTE_DIRECTORY, aResult);
}

NS_IMETHODIMP
nsLocalFile::IsFile(bool* aResult) {
  nsresult rv = HasFileAttribute(FILE_ATTRIBUTE_DIRECTORY, aResult);
  if (NS_SUCCEEDED(rv)) {
    *aResult = !*aResult;
  }
  return rv;
}

NS_IMETHODIMP
nsLocalFile::IsHidden(bool* aResult) {
  return HasFileAttribute(FILE_ATTRIBUTE_HIDDEN, aResult);
}

nsresult nsLocalFile::HasFileAttribute(DWORD aFileAttrib, bool* aResult) {
  if (NS_WARN_IF(!aResult)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsresult rv = Resolve();
  if (NS_FAILED(rv)) {
    return rv;
  }

  DWORD attributes = GetFileAttributesW(mResolvedPath.get());
  if (INVALID_FILE_ATTRIBUTES == attributes) {
    return ConvertWinError(GetLastError());
  }

  *aResult = ((attributes & aFileAttrib) != 0);
  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::IsSymlink(bool* aResult) {
  // Check we are correctly initialized.
  CHECK_mWorkingPath();

  if (NS_WARN_IF(!aResult)) {
    return NS_ERROR_INVALID_ARG;
  }

  // TODO: Implement symlink support
  *aResult = false;
  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::IsSpecial(bool* aResult) {
  return HasFileAttribute(FILE_ATTRIBUTE_SYSTEM, aResult);
}

NS_IMETHODIMP
nsLocalFile::Equals(nsIFile* aInFile, bool* aResult) {
  if (NS_WARN_IF(!aInFile)) {
    return NS_ERROR_INVALID_ARG;
  }
  if (NS_WARN_IF(!aResult)) {
    return NS_ERROR_INVALID_ARG;
  }

  EnsureShortPath();

  nsCOMPtr<nsILocalFileWin> lf(do_QueryInterface(aInFile));
  if (!lf) {
    *aResult = false;
    return NS_OK;
  }

  nsAutoString inFilePath;
  lf->GetCanonicalPath(inFilePath);

  bool inUseDOSDevicePathSyntax;
  lf->GetUseDOSDevicePathSyntax(&inUseDOSDevicePathSyntax);

  // Remove the prefix for both inFilePath and mShortWorkingPath if the
  // useDOSDevicePathSyntax from them are not the same.
  // This is added because of Omnijar. It compare files from different moduals
  // with itself
  nsAutoString shortWorkingPath;
  if (inUseDOSDevicePathSyntax == mUseDOSDevicePathSyntax) {
    shortWorkingPath = mShortWorkingPath;
  } else if (inUseDOSDevicePathSyntax &&
             StringBeginsWith(inFilePath, kDevicePathSpecifier)) {
    MOZ_ASSERT(!StringBeginsWith(mShortWorkingPath, kDevicePathSpecifier));

    shortWorkingPath = mShortWorkingPath;
    inFilePath = Substring(inFilePath, kDevicePathSpecifier.Length());
  } else if (mUseDOSDevicePathSyntax &&
             StringBeginsWith(mShortWorkingPath, kDevicePathSpecifier)) {
    MOZ_ASSERT(!StringBeginsWith(inFilePath, kDevicePathSpecifier));

    shortWorkingPath =
        Substring(mShortWorkingPath, kDevicePathSpecifier.Length());
  }

  // Ok : Win9x
  *aResult = _wcsicmp(shortWorkingPath.get(), inFilePath.get()) == 0;

  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::Contains(nsIFile* aInFile, bool* aResult) {
  // Check we are correctly initialized.
  CHECK_mWorkingPath();

  *aResult = false;

  nsAutoString myFilePath;
  if (NS_FAILED(GetTarget(myFilePath))) {
    GetPath(myFilePath);
  }

  uint32_t myFilePathLen = myFilePath.Length();

  nsAutoString inFilePath;
  if (NS_FAILED(aInFile->GetTarget(inFilePath))) {
    aInFile->GetPath(inFilePath);
  }

  // make sure that the |aInFile|'s path has a trailing separator.
  if (inFilePath.Length() >= myFilePathLen &&
      inFilePath[myFilePathLen] == L'\\') {
    if (_wcsnicmp(myFilePath.get(), inFilePath.get(), myFilePathLen) == 0) {
      *aResult = true;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::GetTarget(nsAString& aResult) {
  aResult.Truncate();
  Resolve();

  MOZ_ASSERT_IF(
      mUseDOSDevicePathSyntax,
      !FilePreferences::StartsWithDiskDesignatorAndBackslash(mResolvedPath));

  aResult = mResolvedPath;
  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::GetDirectoryEntriesImpl(nsIDirectoryEnumerator** aEntries) {
  nsresult rv;

  *aEntries = nullptr;
  if (mWorkingPath.EqualsLiteral("\\\\.")) {
    RefPtr<nsDriveEnumerator> drives =
        new nsDriveEnumerator(mUseDOSDevicePathSyntax);
    rv = drives->Init();
    if (NS_FAILED(rv)) {
      return rv;
    }
    drives.forget(aEntries);
    return NS_OK;
  }

  RefPtr<nsDirEnumerator> dirEnum = new nsDirEnumerator();
  rv = dirEnum->Init(this);
  if (NS_FAILED(rv)) {
    return rv;
  }

  dirEnum.forget(aEntries);

  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::GetPersistentDescriptor(nsACString& aPersistentDescriptor) {
  CopyUTF16toUTF8(mWorkingPath, aPersistentDescriptor);
  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::SetPersistentDescriptor(const nsACString& aPersistentDescriptor) {
  if (IsUtf8(aPersistentDescriptor)) {
    return InitWithPath(NS_ConvertUTF8toUTF16(aPersistentDescriptor));
  } else {
    return InitWithNativePath(aPersistentDescriptor);
  }
}

NS_IMETHODIMP
nsLocalFile::GetReadOnly(bool* aReadOnly) {
  NS_ENSURE_ARG_POINTER(aReadOnly);

  DWORD dwAttrs = GetFileAttributesW(mWorkingPath.get());
  if (dwAttrs == INVALID_FILE_ATTRIBUTES) {
    return NS_ERROR_FILE_INVALID_PATH;
  }

  *aReadOnly = dwAttrs & FILE_ATTRIBUTE_READONLY;

  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::SetReadOnly(bool aReadOnly) {
  DWORD dwAttrs = GetFileAttributesW(mWorkingPath.get());
  if (dwAttrs == INVALID_FILE_ATTRIBUTES) {
    return NS_ERROR_FILE_INVALID_PATH;
  }

  if (aReadOnly) {
    dwAttrs |= FILE_ATTRIBUTE_READONLY;
  } else {
    dwAttrs &= ~FILE_ATTRIBUTE_READONLY;
  }

  if (SetFileAttributesW(mWorkingPath.get(), dwAttrs) == 0) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::GetUseDOSDevicePathSyntax(bool* aUseDOSDevicePathSyntax) {
  MOZ_ASSERT(aUseDOSDevicePathSyntax);

  *aUseDOSDevicePathSyntax = mUseDOSDevicePathSyntax;
  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::SetUseDOSDevicePathSyntax(bool aUseDOSDevicePathSyntax) {
  if (mUseDOSDevicePathSyntax == aUseDOSDevicePathSyntax) {
    return NS_OK;
  }

  if (mUseDOSDevicePathSyntax) {
    if (StringBeginsWith(mWorkingPath, kDevicePathSpecifier)) {
      MakeDirty();
      // Remove the prefix
      mWorkingPath = Substring(mWorkingPath, kDevicePathSpecifier.Length());
    }
  } else {
    if (FilePreferences::StartsWithDiskDesignatorAndBackslash(mWorkingPath)) {
      MakeDirty();
      // Prepend the prefix
      mWorkingPath = kDevicePathSpecifier + mWorkingPath;
    }
  }

  mUseDOSDevicePathSyntax = aUseDOSDevicePathSyntax;
  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::Reveal() {
  // This API should be main thread only
  MOZ_ASSERT(NS_IsMainThread());

  // make sure mResolvedPath is set
  nsresult rv = Resolve();
  if (NS_FAILED(rv) && rv != NS_ERROR_FILE_NOT_FOUND) {
    return rv;
  }

  nsCOMPtr<nsIRunnable> task =
      NS_NewRunnableFunction("nsLocalFile::Reveal", [path = mResolvedPath]() {
        MOZ_ASSERT(!NS_IsMainThread(), "Don't run on the main thread");

        bool doCoUninitialize = SUCCEEDED(CoInitializeEx(
            nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE));
        RevealFile(path);
        if (doCoUninitialize) {
          CoUninitialize();
        }
      });

  return NS_DispatchBackgroundTask(task,
                                   nsIEventTarget::DISPATCH_EVENT_MAY_BLOCK);
}

NS_IMETHODIMP
nsLocalFile::GetWindowsFileAttributes(uint32_t* aAttrs) {
  NS_ENSURE_ARG_POINTER(aAttrs);

  DWORD dwAttrs = ::GetFileAttributesW(mWorkingPath.get());
  if (dwAttrs == INVALID_FILE_ATTRIBUTES) {
    return ConvertWinError(GetLastError());
  }

  *aAttrs = dwAttrs;

  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::SetWindowsFileAttributes(uint32_t aSetAttrs,
                                      uint32_t aClearAttrs) {
  DWORD dwAttrs = ::GetFileAttributesW(mWorkingPath.get());
  if (dwAttrs == INVALID_FILE_ATTRIBUTES) {
    return ConvertWinError(GetLastError());
  }

  dwAttrs = (dwAttrs & ~aClearAttrs) | aSetAttrs;

  if (::SetFileAttributesW(mWorkingPath.get(), dwAttrs) == 0) {
    return ConvertWinError(GetLastError());
  }
  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::Launch() {
  // This API should be main thread only
  MOZ_ASSERT(NS_IsMainThread());

  // use the app registry name to launch a shell execute....
  _bstr_t execPath(mWorkingPath.get());

  _variant_t args;
  // Pass VT_ERROR/DISP_E_PARAMNOTFOUND to omit an optional RPC parameter
  // to execute a file with the default verb.
  _variant_t verbDefault(DISP_E_PARAMNOTFOUND, VT_ERROR);
  _variant_t showCmd(SW_SHOWNORMAL);

  // Use the directory of the file we're launching as the working
  // directory. That way if we have a self extracting EXE it won't
  // suggest to extract to the install directory.
  wchar_t* workingDirectoryPtr = nullptr;
  WCHAR workingDirectory[MAX_PATH + 1] = {L'\0'};
  wcsncpy(workingDirectory, mWorkingPath.get(), MAX_PATH);
  if (PathRemoveFileSpecW(workingDirectory)) {
    workingDirectoryPtr = workingDirectory;
  } else {
    NS_WARNING("Could not set working directory for launched file.");
  }

  // We have two methods to launch a file: ShellExecuteExW and
  // ShellExecuteByExplorer.  ShellExecuteExW starts a new process as a child
  // of the current process, while ShellExecuteByExplorer starts a new process
  // as a child of explorer.exe.
  //
  // We prefer launching a process via ShellExecuteByExplorer because
  // applications may not support the mitigation policies inherited from our
  // process.  For example, Skype for Business does not start correctly with
  // the PreferSystem32Images policy which is one of the policies we use.
  //
  // If ShellExecuteByExplorer fails for some reason e.g. a system without
  // running explorer.exe or VDI environment like Citrix, we fall back to
  // ShellExecuteExW which still works in those special environments.
  //
  // There is an exception where we go straight to ShellExecuteExW without
  // trying ShellExecuteByExplorer.  When the extension of a downloaded file is
  // "exe", we prefer security rather than compatibility.
  //
  // When a user launches a downloaded executable, the directory containing
  // the downloaded file may contain a malicious DLL with a common name, which
  // may have been downloaded before.  If the downloaded executable is launched
  // without the PreferSystem32Images policy, the process can be tricked into
  // loading the malicious DLL in the same directory if its name is in the
  // executable's dependent modules.  Therefore, we always launch ".exe"
  // executables via ShellExecuteExW so they inherit our process's mitigation
  // policies including PreferSystem32Images.
  //
  // If the extension is not "exe", then we assume that we are launching an
  // installed application, and therefore the security risk described above
  // is lessened, as a malicious DLL is less likely to be installed in the
  // application's directory.  In that case, we attempt to preserve
  // compatibility and try ShellExecuteByExplorer first.

  static const char* const onlyExeExt[] = {".exe"};
  bool isExecutable;
  nsresult rv =
      LookupExtensionIn(onlyExeExt, ArrayLength(onlyExeExt), &isExecutable);
  if (NS_FAILED(rv)) {
    isExecutable = false;
  }

  // If the file is an executable, go straight to ShellExecuteExW.
  // Otherwise try ShellExecuteByExplorer first, and if it fails,
  // run ShellExecuteExW.
  if (!isExecutable) {
    mozilla::LauncherVoidResult shellExecuteOk =
        mozilla::ShellExecuteByExplorer(execPath, args, verbDefault,
                                        workingDirectoryPtr, showCmd);
    if (shellExecuteOk.isOk()) {
      return NS_OK;
    }
  }

  SHELLEXECUTEINFOW seinfo = {sizeof(SHELLEXECUTEINFOW)};
  seinfo.fMask = SEE_MASK_ASYNCOK;
  seinfo.hwnd = GetMostRecentNavigatorHWND();
  seinfo.lpVerb = nullptr;
  seinfo.lpFile = mWorkingPath.get();
  seinfo.lpParameters = nullptr;
  seinfo.lpDirectory = workingDirectoryPtr;
  seinfo.nShow = SW_SHOWNORMAL;

  if (!ShellExecuteExW(&seinfo)) {
    return NS_ERROR_FILE_EXECUTION_FAILED;
  }

  return NS_OK;
}

nsresult NS_NewLocalFile(const nsAString& aPath, bool aFollowLinks,
                         nsIFile** aResult) {
  RefPtr<nsLocalFile> file = new nsLocalFile();

  if (!aPath.IsEmpty()) {
    nsresult rv = file->InitWithPath(aPath);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  file.forget(aResult);
  return NS_OK;
}

//-----------------------------------------------------------------------------
// Native (lossy) interface
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsLocalFile::InitWithNativePath(const nsACString& aFilePath) {
  nsAutoString tmp;
  nsresult rv = NS_CopyNativeToUnicode(aFilePath, tmp);
  if (NS_SUCCEEDED(rv)) {
    return InitWithPath(tmp);
  }

  return rv;
}

NS_IMETHODIMP
nsLocalFile::AppendNative(const nsACString& aNode) {
  nsAutoString tmp;
  nsresult rv = NS_CopyNativeToUnicode(aNode, tmp);
  if (NS_SUCCEEDED(rv)) {
    return Append(tmp);
  }

  return rv;
}

NS_IMETHODIMP
nsLocalFile::AppendRelativeNativePath(const nsACString& aNode) {
  nsAutoString tmp;
  nsresult rv = NS_CopyNativeToUnicode(aNode, tmp);
  if (NS_SUCCEEDED(rv)) {
    return AppendRelativePath(tmp);
  }
  return rv;
}

NS_IMETHODIMP
nsLocalFile::GetNativeLeafName(nsACString& aLeafName) {
  // NS_WARNING("This API is lossy. Use GetLeafName !");
  nsAutoString tmp;
  nsresult rv = GetLeafName(tmp);
  if (NS_SUCCEEDED(rv)) {
    rv = NS_CopyUnicodeToNative(tmp, aLeafName);
  }

  return rv;
}

NS_IMETHODIMP
nsLocalFile::SetNativeLeafName(const nsACString& aLeafName) {
  nsAutoString tmp;
  nsresult rv = NS_CopyNativeToUnicode(aLeafName, tmp);
  if (NS_SUCCEEDED(rv)) {
    return SetLeafName(tmp);
  }

  return rv;
}

nsString nsLocalFile::NativePath() { return mWorkingPath; }

nsCString nsIFile::HumanReadablePath() {
  nsString path;
  DebugOnly<nsresult> rv = GetPath(path);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  return NS_ConvertUTF16toUTF8(path);
}

NS_IMETHODIMP
nsLocalFile::GetNativeCanonicalPath(nsACString& aResult) {
  NS_WARNING("This method is lossy. Use GetCanonicalPath !");
  EnsureShortPath();
  NS_CopyUnicodeToNative(mShortWorkingPath, aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::CopyToNative(nsIFile* aNewParentDir, const nsACString& aNewName) {
  // Check we are correctly initialized.
  CHECK_mWorkingPath();

  if (aNewName.IsEmpty()) {
    return CopyTo(aNewParentDir, u""_ns);
  }

  nsAutoString tmp;
  nsresult rv = NS_CopyNativeToUnicode(aNewName, tmp);
  if (NS_SUCCEEDED(rv)) {
    return CopyTo(aNewParentDir, tmp);
  }

  return rv;
}

NS_IMETHODIMP
nsLocalFile::CopyToFollowingLinksNative(nsIFile* aNewParentDir,
                                        const nsACString& aNewName) {
  if (aNewName.IsEmpty()) {
    return CopyToFollowingLinks(aNewParentDir, u""_ns);
  }

  nsAutoString tmp;
  nsresult rv = NS_CopyNativeToUnicode(aNewName, tmp);
  if (NS_SUCCEEDED(rv)) {
    return CopyToFollowingLinks(aNewParentDir, tmp);
  }

  return rv;
}

NS_IMETHODIMP
nsLocalFile::MoveToNative(nsIFile* aNewParentDir, const nsACString& aNewName) {
  // Check we are correctly initialized.
  CHECK_mWorkingPath();

  if (aNewName.IsEmpty()) {
    return MoveTo(aNewParentDir, u""_ns);
  }

  nsAutoString tmp;
  nsresult rv = NS_CopyNativeToUnicode(aNewName, tmp);
  if (NS_SUCCEEDED(rv)) {
    return MoveTo(aNewParentDir, tmp);
  }

  return rv;
}

NS_IMETHODIMP
nsLocalFile::MoveToFollowingLinksNative(nsIFile* aNewParentDir,
                                        const nsACString& aNewName) {
  // Check we are correctly initialized.
  CHECK_mWorkingPath();

  if (aNewName.IsEmpty()) {
    return MoveToFollowingLinks(aNewParentDir, u""_ns);
  }

  nsAutoString tmp;
  nsresult rv = NS_CopyNativeToUnicode(aNewName, tmp);
  if (NS_SUCCEEDED(rv)) {
    return MoveToFollowingLinks(aNewParentDir, tmp);
  }

  return rv;
}

NS_IMETHODIMP
nsLocalFile::GetNativeTarget(nsACString& aResult) {
  // Check we are correctly initialized.
  CHECK_mWorkingPath();

  NS_WARNING("This API is lossy. Use GetTarget !");
  nsAutoString tmp;
  nsresult rv = GetTarget(tmp);
  if (NS_SUCCEEDED(rv)) {
    rv = NS_CopyUnicodeToNative(tmp, aResult);
  }

  return rv;
}

nsresult NS_NewNativeLocalFile(const nsACString& aPath, bool aFollowLinks,
                               nsIFile** aResult) {
  nsAutoString buf;
  nsresult rv = NS_CopyNativeToUnicode(aPath, buf);
  if (NS_FAILED(rv)) {
    *aResult = nullptr;
    return rv;
  }
  return NS_NewLocalFile(buf, aFollowLinks, aResult);
}

void nsLocalFile::EnsureShortPath() {
  if (!mShortWorkingPath.IsEmpty()) {
    return;
  }

  WCHAR shortPath[MAX_PATH + 1];
  DWORD lengthNeeded = ::GetShortPathNameW(mWorkingPath.get(), shortPath,
                                           ArrayLength(shortPath));
  // If an error occurred then lengthNeeded is set to 0 or the length of the
  // needed buffer including null termination.  If it succeeds the number of
  // wide characters not including null termination is returned.
  if (lengthNeeded != 0 && lengthNeeded < ArrayLength(shortPath)) {
    mShortWorkingPath.Assign(shortPath);
  } else {
    mShortWorkingPath.Assign(mWorkingPath);
  }
}

NS_IMPL_ISUPPORTS_INHERITED(nsDriveEnumerator, nsSimpleEnumerator,
                            nsIDirectoryEnumerator)

nsDriveEnumerator::nsDriveEnumerator(bool aUseDOSDevicePathSyntax)
    : mUseDOSDevicePathSyntax(aUseDOSDevicePathSyntax) {}

nsDriveEnumerator::~nsDriveEnumerator() {}

nsresult nsDriveEnumerator::Init() {
  /* If the length passed to GetLogicalDriveStrings is smaller
   * than the length of the string it would return, it returns
   * the length required for the string. */
  DWORD length = GetLogicalDriveStringsW(0, 0);
  /* The string is null terminated */
  if (!mDrives.SetLength(length + 1, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  if (!GetLogicalDriveStringsW(length, mDrives.get())) {
    return NS_ERROR_FAILURE;
  }
  mDrives.BeginReading(mStartOfCurrentDrive);
  mDrives.EndReading(mEndOfDrivesString);
  return NS_OK;
}

NS_IMETHODIMP
nsDriveEnumerator::HasMoreElements(bool* aHasMore) {
  *aHasMore = *mStartOfCurrentDrive != L'\0';
  return NS_OK;
}

NS_IMETHODIMP
nsDriveEnumerator::GetNext(nsISupports** aNext) {
  /* GetLogicalDrives stored in mDrives is a concatenation
   * of null terminated strings, followed by a null terminator.
   * mStartOfCurrentDrive is an iterator pointing at the first
   * character of the current drive. */
  if (*mStartOfCurrentDrive == L'\0') {
    *aNext = nullptr;
    return NS_ERROR_FAILURE;
  }

  nsAString::const_iterator driveEnd = mStartOfCurrentDrive;
  FindCharInReadable(L'\0', driveEnd, mEndOfDrivesString);
  nsString drive(Substring(mStartOfCurrentDrive, driveEnd));
  mStartOfCurrentDrive = ++driveEnd;

  nsIFile* file;
  nsresult rv = NewLocalFile(drive, mUseDOSDevicePathSyntax, &file);

  *aNext = file;
  return rv;
}
