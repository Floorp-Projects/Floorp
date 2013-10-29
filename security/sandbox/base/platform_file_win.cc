// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/platform_file.h"

#include <io.h>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/sparse_histogram.h"
#include "base/threading/thread_restrictions.h"

namespace base {
PlatformFile CreatePlatformFileUnsafe(const FilePath& name,
                                      int flags,
                                      bool* created,
                                      PlatformFileError* error) {
  base::ThreadRestrictions::AssertIOAllowed();

  DWORD disposition = 0;
  if (created)
    *created = false;

  if (flags & PLATFORM_FILE_OPEN)
    disposition = OPEN_EXISTING;

  if (flags & PLATFORM_FILE_CREATE) {
    DCHECK(!disposition);
    disposition = CREATE_NEW;
  }

  if (flags & PLATFORM_FILE_OPEN_ALWAYS) {
    DCHECK(!disposition);
    disposition = OPEN_ALWAYS;
  }

  if (flags & PLATFORM_FILE_CREATE_ALWAYS) {
    DCHECK(!disposition);
    disposition = CREATE_ALWAYS;
  }

  if (flags & PLATFORM_FILE_OPEN_TRUNCATED) {
    DCHECK(!disposition);
    DCHECK(flags & PLATFORM_FILE_WRITE);
    disposition = TRUNCATE_EXISTING;
  }

  if (!disposition) {
    NOTREACHED();
    return NULL;
  }

  DWORD access = 0;
  if (flags & PLATFORM_FILE_WRITE)
    access = GENERIC_WRITE;
  if (flags & PLATFORM_FILE_APPEND) {
    DCHECK(!access);
    access = FILE_APPEND_DATA;
  }
  if (flags & PLATFORM_FILE_READ)
    access |= GENERIC_READ;
  if (flags & PLATFORM_FILE_WRITE_ATTRIBUTES)
    access |= FILE_WRITE_ATTRIBUTES;
  if (flags & PLATFORM_FILE_EXECUTE)
    access |= GENERIC_EXECUTE;

  DWORD sharing = (flags & PLATFORM_FILE_EXCLUSIVE_READ) ? 0 : FILE_SHARE_READ;
  if (!(flags & PLATFORM_FILE_EXCLUSIVE_WRITE))
    sharing |= FILE_SHARE_WRITE;
  if (flags & PLATFORM_FILE_SHARE_DELETE)
    sharing |= FILE_SHARE_DELETE;

  DWORD create_flags = 0;
  if (flags & PLATFORM_FILE_ASYNC)
    create_flags |= FILE_FLAG_OVERLAPPED;
  if (flags & PLATFORM_FILE_TEMPORARY)
    create_flags |= FILE_ATTRIBUTE_TEMPORARY;
  if (flags & PLATFORM_FILE_HIDDEN)
    create_flags |= FILE_ATTRIBUTE_HIDDEN;
  if (flags & PLATFORM_FILE_DELETE_ON_CLOSE)
    create_flags |= FILE_FLAG_DELETE_ON_CLOSE;
  if (flags & PLATFORM_FILE_BACKUP_SEMANTICS)
    create_flags |= FILE_FLAG_BACKUP_SEMANTICS;

  HANDLE file = CreateFile(name.value().c_str(), access, sharing, NULL,
                           disposition, create_flags, NULL);

  if (created && (INVALID_HANDLE_VALUE != file)) {
    if (flags & (PLATFORM_FILE_OPEN_ALWAYS))
      *created = (ERROR_ALREADY_EXISTS != GetLastError());
    else if (flags & (PLATFORM_FILE_CREATE_ALWAYS | PLATFORM_FILE_CREATE))
      *created = true;
  }

  if (error) {
    if (file != kInvalidPlatformFileValue)
      *error = PLATFORM_FILE_OK;
    else
      *error = LastErrorToPlatformFileError(GetLastError());
  }

  return file;
}

FILE* FdopenPlatformFile(PlatformFile file, const char* mode) {
  if (file == kInvalidPlatformFileValue)
    return NULL;
  int fd = _open_osfhandle(reinterpret_cast<intptr_t>(file), 0);
  if (fd < 0)
    return NULL;
  return _fdopen(fd, mode);
}

bool ClosePlatformFile(PlatformFile file) {
  base::ThreadRestrictions::AssertIOAllowed();
  return (CloseHandle(file) != 0);
}

int64 SeekPlatformFile(PlatformFile file,
                       PlatformFileWhence whence,
                       int64 offset) {
  base::ThreadRestrictions::AssertIOAllowed();
  if (file == kInvalidPlatformFileValue || offset < 0)
    return -1;

  LARGE_INTEGER distance, res;
  distance.QuadPart = offset;
  DWORD move_method = static_cast<DWORD>(whence);
  if (!SetFilePointerEx(file, distance, &res, move_method))
    return -1;
  return res.QuadPart;
}

int ReadPlatformFile(PlatformFile file, int64 offset, char* data, int size) {
  base::ThreadRestrictions::AssertIOAllowed();
  if (file == kInvalidPlatformFileValue)
    return -1;

  LARGE_INTEGER offset_li;
  offset_li.QuadPart = offset;

  OVERLAPPED overlapped = {0};
  overlapped.Offset = offset_li.LowPart;
  overlapped.OffsetHigh = offset_li.HighPart;

  DWORD bytes_read;
  if (::ReadFile(file, data, size, &bytes_read, &overlapped) != 0)
    return bytes_read;
  else if (ERROR_HANDLE_EOF == GetLastError())
    return 0;

  return -1;
}

int ReadPlatformFileAtCurrentPos(PlatformFile file, char* data, int size) {
  return ReadPlatformFile(file, 0, data, size);
}

int ReadPlatformFileNoBestEffort(PlatformFile file, int64 offset, char* data,
                                 int size) {
  return ReadPlatformFile(file, offset, data, size);
}

int ReadPlatformFileCurPosNoBestEffort(PlatformFile file,
                                       char* data, int size) {
  return ReadPlatformFile(file, 0, data, size);
}

int WritePlatformFile(PlatformFile file, int64 offset,
                      const char* data, int size) {
  base::ThreadRestrictions::AssertIOAllowed();
  if (file == kInvalidPlatformFileValue)
    return -1;

  LARGE_INTEGER offset_li;
  offset_li.QuadPart = offset;

  OVERLAPPED overlapped = {0};
  overlapped.Offset = offset_li.LowPart;
  overlapped.OffsetHigh = offset_li.HighPart;

  DWORD bytes_written;
  if (::WriteFile(file, data, size, &bytes_written, &overlapped) != 0)
    return bytes_written;

  return -1;
}

int WritePlatformFileAtCurrentPos(PlatformFile file, const char* data,
                                  int size) {
  return WritePlatformFile(file, 0, data, size);
}

int WritePlatformFileCurPosNoBestEffort(PlatformFile file,
                                        const char* data, int size) {
  return WritePlatformFile(file, 0, data, size);
}

bool TruncatePlatformFile(PlatformFile file, int64 length) {
  base::ThreadRestrictions::AssertIOAllowed();
  if (file == kInvalidPlatformFileValue)
    return false;

  // Get the current file pointer.
  LARGE_INTEGER file_pointer;
  LARGE_INTEGER zero;
  zero.QuadPart = 0;
  if (::SetFilePointerEx(file, zero, &file_pointer, FILE_CURRENT) == 0)
    return false;

  LARGE_INTEGER length_li;
  length_li.QuadPart = length;
  // If length > file size, SetFilePointerEx() should extend the file
  // with zeroes on all Windows standard file systems (NTFS, FATxx).
  if (!::SetFilePointerEx(file, length_li, NULL, FILE_BEGIN))
    return false;

  // Set the new file length and move the file pointer to its old position.
  // This is consistent with ftruncate()'s behavior, even when the file
  // pointer points to a location beyond the end of the file.
  return ((::SetEndOfFile(file) != 0) &&
          (::SetFilePointerEx(file, file_pointer, NULL, FILE_BEGIN) != 0));
}

bool FlushPlatformFile(PlatformFile file) {
  base::ThreadRestrictions::AssertIOAllowed();
  return ((file != kInvalidPlatformFileValue) && ::FlushFileBuffers(file));
}

bool TouchPlatformFile(PlatformFile file, const base::Time& last_access_time,
                       const base::Time& last_modified_time) {
  base::ThreadRestrictions::AssertIOAllowed();
  if (file == kInvalidPlatformFileValue)
    return false;

  FILETIME last_access_filetime = last_access_time.ToFileTime();
  FILETIME last_modified_filetime = last_modified_time.ToFileTime();
  return (::SetFileTime(file, NULL, &last_access_filetime,
                        &last_modified_filetime) != 0);
}

bool GetPlatformFileInfo(PlatformFile file, PlatformFileInfo* info) {
  base::ThreadRestrictions::AssertIOAllowed();
  if (!info)
    return false;

  BY_HANDLE_FILE_INFORMATION file_info;
  if (GetFileInformationByHandle(file, &file_info) == 0)
    return false;

  LARGE_INTEGER size;
  size.HighPart = file_info.nFileSizeHigh;
  size.LowPart = file_info.nFileSizeLow;
  info->size = size.QuadPart;
  info->is_directory =
      (file_info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
  info->is_symbolic_link = false; // Windows doesn't have symbolic links.
  info->last_modified = base::Time::FromFileTime(file_info.ftLastWriteTime);
  info->last_accessed = base::Time::FromFileTime(file_info.ftLastAccessTime);
  info->creation_time = base::Time::FromFileTime(file_info.ftCreationTime);
  return true;
}

PlatformFileError LastErrorToPlatformFileError(DWORD last_error) {
  switch (last_error) {
    case ERROR_SHARING_VIOLATION:
      return PLATFORM_FILE_ERROR_IN_USE;
    case ERROR_FILE_EXISTS:
      return PLATFORM_FILE_ERROR_EXISTS;
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
      return PLATFORM_FILE_ERROR_NOT_FOUND;
    case ERROR_ACCESS_DENIED:
      return PLATFORM_FILE_ERROR_ACCESS_DENIED;
    case ERROR_TOO_MANY_OPEN_FILES:
      return PLATFORM_FILE_ERROR_TOO_MANY_OPENED;
    case ERROR_OUTOFMEMORY:
    case ERROR_NOT_ENOUGH_MEMORY:
      return PLATFORM_FILE_ERROR_NO_MEMORY;
    case ERROR_HANDLE_DISK_FULL:
    case ERROR_DISK_FULL:
    case ERROR_DISK_RESOURCES_EXHAUSTED:
      return PLATFORM_FILE_ERROR_NO_SPACE;
    case ERROR_USER_MAPPED_FILE:
      return PLATFORM_FILE_ERROR_INVALID_OPERATION;
    case ERROR_NOT_READY:
    case ERROR_SECTOR_NOT_FOUND:
    case ERROR_DEV_NOT_EXIST:
    case ERROR_IO_DEVICE:
    case ERROR_FILE_CORRUPT:
    case ERROR_DISK_CORRUPT:
      return PLATFORM_FILE_ERROR_IO;
    default:
      UMA_HISTOGRAM_SPARSE_SLOWLY("PlatformFile.UnknownErrors.Windows",
                                  last_error);
      return PLATFORM_FILE_ERROR_FAILED;
  }
}

}  // namespace base
