/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(XP_WIN)
#include <windows.h>
#include <winioctl.h> // for FSCTL_GET_REPARSE_POINT
#endif


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include "updatecommon.h"
#ifdef XP_WIN
#include "updatehelper.h"
#include "nsWindowsHelpers.h"
#include "mozilla/UniquePtr.h"

// This struct isn't in any SDK header, so this definition was copied from:
// https://docs.microsoft.com/en-us/windows-hardware/drivers/ddi/content/ntifs/ns-ntifs-_reparse_data_buffer
typedef struct _REPARSE_DATA_BUFFER
{
  ULONG  ReparseTag;
  USHORT ReparseDataLength;
  USHORT Reserved;
  union
  {
    struct
    {
      USHORT SubstituteNameOffset;
      USHORT SubstituteNameLength;
      USHORT PrintNameOffset;
      USHORT PrintNameLength;
      ULONG  Flags;
      WCHAR  PathBuffer[1];
    } SymbolicLinkReparseBuffer;
    struct
    {
      USHORT SubstituteNameOffset;
      USHORT SubstituteNameLength;
      USHORT PrintNameOffset;
      USHORT PrintNameLength;
      WCHAR  PathBuffer[1];
    } MountPointReparseBuffer;
    struct
    {
      UCHAR DataBuffer[1];
    } GenericReparseBuffer;
  } DUMMYUNIONNAME;
} REPARSE_DATA_BUFFER, *PREPARSE_DATA_BUFFER;
#endif

UpdateLog::UpdateLog() : logFP(nullptr)
{
}

void UpdateLog::Init(NS_tchar* sourcePath,
                     const NS_tchar* fileName)
{
  if (logFP) {
    return;
  }

  int dstFilePathLen = NS_tsnprintf(mDstFilePath,
    sizeof(mDstFilePath)/sizeof(mDstFilePath[0]),
    NS_T("%s/%s"), sourcePath, fileName);
  // If the destination path was over the length limit,
  // disable logging by skipping opening the file and setting logFP.
  if ((dstFilePathLen > 0) &&
      (dstFilePathLen <
         static_cast<int>(sizeof(mDstFilePath)/sizeof(mDstFilePath[0])))) {
#ifdef XP_WIN
    if (GetUUIDTempFilePath(sourcePath, L"log", mTmpFilePath)) {
      logFP = NS_tfopen(mTmpFilePath, NS_T("w"));
      // Delete this file now so it is possible to tell from the unelevated
      // updater process if the elevated updater process has written the log.
      DeleteFileW(mDstFilePath);
    }
#elif XP_MACOSX
    logFP = NS_tfopen(mDstFilePath, NS_T("w"));
#else
    // On platforms that have an updates directory in the installation directory
    // (e.g. platforms other than Windows and Mac) the update log is written to
    // a temporary file and then to the update log file. This is needed since
    // the installation directory is moved during a replace request. This can be
    // removed when the platform's updates directory is located outside of the
    // installation directory.
    logFP = tmpfile();
#endif
  }
}

void UpdateLog::Finish()
{
  if (!logFP) {
    return;
  }

#if !defined(XP_WIN) && !defined(XP_MACOSX)
  const int blockSize = 1024;
  char buffer[blockSize];
  fflush(logFP);
  rewind(logFP);

  FILE *updateLogFP = NS_tfopen(mDstFilePath, NS_T("wb+"));
  while (!feof(logFP)) {
    size_t read = fread(buffer, 1, blockSize, logFP);
    if (ferror(logFP)) {
      fclose(logFP);
      logFP = nullptr;
      fclose(updateLogFP);
      updateLogFP = nullptr;
      return;
    }

    size_t written = 0;

    while (written < read) {
      size_t chunkWritten = fwrite(buffer, 1, read - written, updateLogFP);
      if (chunkWritten <= 0) {
        fclose(logFP);
        logFP = nullptr;
        fclose(updateLogFP);
        updateLogFP = nullptr;
        return;
      }

      written += chunkWritten;
    }
  }
  fclose(updateLogFP);
  updateLogFP = nullptr;
#endif

  fclose(logFP);
  logFP = nullptr;

#ifdef XP_WIN
  // When the log file already exists then the elevated updater has already
  // written the log file and the temp file for the log should be discarded.
  if (!NS_taccess(mDstFilePath, F_OK)) {
    DeleteFileW(mTmpFilePath);
  } else {
    MoveFileW(mTmpFilePath, mDstFilePath);
  }
#endif
}

void UpdateLog::Flush()
{
  if (!logFP) {
    return;
  }

  fflush(logFP);
}

void UpdateLog::Printf(const char *fmt, ... )
{
  if (!logFP) {
    return;
  }

  va_list ap;
  va_start(ap, fmt);
  vfprintf(logFP, fmt, ap);
  fprintf(logFP, "\n");
  va_end(ap);
}

void UpdateLog::WarnPrintf(const char *fmt, ... )
{
  if (!logFP) {
    return;
  }

  va_list ap;
  va_start(ap, fmt);
  fprintf(logFP, "*** Warning: ");
  vfprintf(logFP, fmt, ap);
  fprintf(logFP, "***\n");
  va_end(ap);
}

#ifdef XP_WIN
/**
 * Determine if a path contains symlinks or junctions to disallowed locations
 *
 * @param fullPath  The full path to check.
 * @return true if the path contains invalid links or on errors,
 *         false if the check passes and the path can be used
 */
bool
PathContainsInvalidLinks(wchar_t * const fullPath)
{
  wchar_t pathCopy[MAXPATHLEN + 1] = L"";
  wcsncpy(pathCopy, fullPath, MAXPATHLEN);
  wchar_t* remainingPath = nullptr;
  wchar_t* nextToken = wcstok(pathCopy, L"\\", &remainingPath);
  wchar_t* partialPath = nextToken;

  while (nextToken) {
    if ((GetFileAttributesW(partialPath) & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
      nsAutoHandle h(CreateFileW(partialPath, 0,
                                 FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                 nullptr, OPEN_EXISTING,
                                 FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
                                 nullptr));
      if (h == INVALID_HANDLE_VALUE) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND) {
          // The path can't be an invalid link if it doesn't exist.
          return false;
        } else {
          return true;
        }
      }

      mozilla::UniquePtr<UINT8[]> byteBuffer =
        mozilla::MakeUnique<UINT8[]>(MAXIMUM_REPARSE_DATA_BUFFER_SIZE);
      if (!byteBuffer) {
        return true;
      }
      ZeroMemory(byteBuffer.get(), MAXIMUM_REPARSE_DATA_BUFFER_SIZE);
      REPARSE_DATA_BUFFER* buffer = (REPARSE_DATA_BUFFER*)byteBuffer.get();
      DWORD bytes = 0;
      if (!DeviceIoControl(h, FSCTL_GET_REPARSE_POINT, nullptr, 0, buffer,
                           MAXIMUM_REPARSE_DATA_BUFFER_SIZE, &bytes, nullptr)) {
        return true;
      }

      wchar_t* reparseTarget = nullptr;
      switch (buffer->ReparseTag) {
        case IO_REPARSE_TAG_MOUNT_POINT:
          reparseTarget = buffer->MountPointReparseBuffer.PathBuffer;
          break;
        case IO_REPARSE_TAG_SYMLINK:
          reparseTarget = buffer->SymbolicLinkReparseBuffer.PathBuffer;
          break;
        default:
          return true;
          break;
      }

      if (wcsncmp(reparseTarget, L"\\??\\", ARRAYSIZE(L"\\??\\") - 1) != 0) {
        return true;
      }
    }

    nextToken = wcstok(nullptr, L"\\", &remainingPath);
    PathAppendW(partialPath, nextToken);
  }

  return false;
}
#endif

/**
 * Performs checks of a full path for validity for this application.
 *
 * @param  origFullPath
 *         The full path to check.
 * @return true if the path is valid for this application and false otherwise.
 */
bool
IsValidFullPath(NS_tchar* origFullPath)
{
  // Subtract 1 from MAXPATHLEN for null termination.
  if (NS_tstrlen(origFullPath) > MAXPATHLEN - 1) {
    // The path is longer than acceptable for this application.
    return false;
  }

#ifdef XP_WIN
  NS_tchar testPath[MAXPATHLEN] = {NS_T('\0')};
  // GetFullPathNameW will replace / with \ which PathCanonicalizeW requires.
  if (GetFullPathNameW(origFullPath, MAXPATHLEN, testPath, nullptr) == 0) {
    // Unable to get the full name for the path (e.g. invalid path).
    return false;
  }

  NS_tchar canonicalPath[MAXPATHLEN] = {NS_T('\0')};
  if (!PathCanonicalizeW(canonicalPath, testPath)) {
    // Path could not be canonicalized (e.g. invalid path).
    return false;
  }

  // Check if the path passed in resolves to a differerent path.
  if (NS_tstricmp(origFullPath, canonicalPath) != 0) {
    // Case insensitive string comparison between the supplied path and the
    // canonical path are not equal. This will prevent directory traversal and
    // the use of / in paths since they are converted to \.
    return false;
  }

  NS_tstrncpy(testPath, origFullPath, MAXPATHLEN);
  if (!PathStripToRootW(testPath)) {
    // It should always be possible to strip a valid path to its root.
    return false;
  }

  if (origFullPath[0] == NS_T('\\')) {
    // Only allow UNC server share paths.
    if (!PathIsUNCServerShareW(testPath)) {
      return false;
    }
  }

  if (PathContainsInvalidLinks(canonicalPath)) {
    return false;
  }
#else
  // Only allow full paths.
  if (origFullPath[0] != NS_T('/')) {
    return false;
  }

  // The path must not traverse directories
  if (NS_tstrstr(origFullPath, NS_T("..")) != nullptr ||
      NS_tstrstr(origFullPath, NS_T("./")) != nullptr) {
    return false;
  }
#endif
  return true;
}
