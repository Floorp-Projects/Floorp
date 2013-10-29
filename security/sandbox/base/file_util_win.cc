// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"

#include <windows.h>
#include <psapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <time.h>

#include <algorithm>
#include <limits>
#include <string>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/process/process_handle.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"

namespace base {

namespace {

const DWORD kFileShareAll =
    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;

bool ShellCopy(const FilePath& from_path,
               const FilePath& to_path,
               bool recursive) {
  // WinXP SHFileOperation doesn't like trailing separators.
  FilePath stripped_from = from_path.StripTrailingSeparators();
  FilePath stripped_to = to_path.StripTrailingSeparators();

  ThreadRestrictions::AssertIOAllowed();

  // NOTE: I suspect we could support longer paths, but that would involve
  // analyzing all our usage of files.
  if (stripped_from.value().length() >= MAX_PATH ||
      stripped_to.value().length() >= MAX_PATH) {
    return false;
  }

  // SHFILEOPSTRUCT wants the path to be terminated with two NULLs,
  // so we have to use wcscpy because wcscpy_s writes non-NULLs
  // into the rest of the buffer.
  wchar_t double_terminated_path_from[MAX_PATH + 1] = {0};
  wchar_t double_terminated_path_to[MAX_PATH + 1] = {0};
#pragma warning(suppress:4996)  // don't complain about wcscpy deprecation
  wcscpy(double_terminated_path_from, stripped_from.value().c_str());
#pragma warning(suppress:4996)  // don't complain about wcscpy deprecation
  wcscpy(double_terminated_path_to, stripped_to.value().c_str());

  SHFILEOPSTRUCT file_operation = {0};
  file_operation.wFunc = FO_COPY;
  file_operation.pFrom = double_terminated_path_from;
  file_operation.pTo = double_terminated_path_to;
  file_operation.fFlags = FOF_NOERRORUI | FOF_SILENT | FOF_NOCONFIRMATION |
                          FOF_NOCONFIRMMKDIR;
  if (!recursive)
    file_operation.fFlags |= FOF_NORECURSION | FOF_FILESONLY;

  return (SHFileOperation(&file_operation) == 0);
}

}  // namespace

FilePath MakeAbsoluteFilePath(const FilePath& input) {
  ThreadRestrictions::AssertIOAllowed();
  wchar_t file_path[MAX_PATH];
  if (!_wfullpath(file_path, input.value().c_str(), MAX_PATH))
    return FilePath();
  return FilePath(file_path);
}

bool DeleteFile(const FilePath& path, bool recursive) {
  ThreadRestrictions::AssertIOAllowed();

  if (path.value().length() >= MAX_PATH)
    return false;

  if (!recursive) {
    // If not recursing, then first check to see if |path| is a directory.
    // If it is, then remove it with RemoveDirectory.
    PlatformFileInfo file_info;
    if (file_util::GetFileInfo(path, &file_info) && file_info.is_directory)
      return RemoveDirectory(path.value().c_str()) != 0;

    // Otherwise, it's a file, wildcard or non-existant. Try DeleteFile first
    // because it should be faster. If DeleteFile fails, then we fall through
    // to SHFileOperation, which will do the right thing.
    if (::DeleteFile(path.value().c_str()) != 0)
      return true;
  }

  // SHFILEOPSTRUCT wants the path to be terminated with two NULLs,
  // so we have to use wcscpy because wcscpy_s writes non-NULLs
  // into the rest of the buffer.
  wchar_t double_terminated_path[MAX_PATH + 1] = {0};
#pragma warning(suppress:4996)  // don't complain about wcscpy deprecation
  if (g_bug108724_debug)
    LOG(WARNING) << "copying ";
  wcscpy(double_terminated_path, path.value().c_str());

  SHFILEOPSTRUCT file_operation = {0};
  file_operation.wFunc = FO_DELETE;
  file_operation.pFrom = double_terminated_path;
  file_operation.fFlags = FOF_NOERRORUI | FOF_SILENT | FOF_NOCONFIRMATION;
  if (!recursive)
    file_operation.fFlags |= FOF_NORECURSION | FOF_FILESONLY;
  if (g_bug108724_debug)
    LOG(WARNING) << "Performing shell operation";
  int err = SHFileOperation(&file_operation);
  if (g_bug108724_debug)
    LOG(WARNING) << "Done: " << err;

  // Since we're passing flags to the operation telling it to be silent,
  // it's possible for the operation to be aborted/cancelled without err
  // being set (although MSDN doesn't give any scenarios for how this can
  // happen).  See MSDN for SHFileOperation and SHFILEOPTSTRUCT.
  if (file_operation.fAnyOperationsAborted)
    return false;

  // Some versions of Windows return ERROR_FILE_NOT_FOUND (0x2) when deleting
  // an empty directory and some return 0x402 when they should be returning
  // ERROR_FILE_NOT_FOUND. MSDN says Vista and up won't return 0x402.
  return (err == 0 || err == ERROR_FILE_NOT_FOUND || err == 0x402);
}

bool DeleteFileAfterReboot(const FilePath& path) {
  ThreadRestrictions::AssertIOAllowed();

  if (path.value().length() >= MAX_PATH)
    return false;

  return MoveFileEx(path.value().c_str(), NULL,
                    MOVEFILE_DELAY_UNTIL_REBOOT |
                        MOVEFILE_REPLACE_EXISTING) != FALSE;
}

bool ReplaceFile(const FilePath& from_path,
                 const FilePath& to_path,
                 PlatformFileError* error) {
  ThreadRestrictions::AssertIOAllowed();
  // Try a simple move first.  It will only succeed when |to_path| doesn't
  // already exist.
  if (::MoveFile(from_path.value().c_str(), to_path.value().c_str()))
    return true;
  // Try the full-blown replace if the move fails, as ReplaceFile will only
  // succeed when |to_path| does exist. When writing to a network share, we may
  // not be able to change the ACLs. Ignore ACL errors then
  // (REPLACEFILE_IGNORE_MERGE_ERRORS).
  if (::ReplaceFile(to_path.value().c_str(), from_path.value().c_str(), NULL,
                    REPLACEFILE_IGNORE_MERGE_ERRORS, NULL, NULL)) {
    return true;
  }
  if (error)
    *error = LastErrorToPlatformFileError(GetLastError());
  return false;
}

bool CopyDirectory(const FilePath& from_path, const FilePath& to_path,
                   bool recursive) {
  ThreadRestrictions::AssertIOAllowed();

  if (recursive)
    return ShellCopy(from_path, to_path, true);

  // The following code assumes that from path is a directory.
  DCHECK(DirectoryExists(from_path));

  // Instead of creating a new directory, we copy the old one to include the
  // security information of the folder as part of the copy.
  if (!PathExists(to_path)) {
    // Except that Vista fails to do that, and instead do a recursive copy if
    // the target directory doesn't exist.
    if (base::win::GetVersion() >= base::win::VERSION_VISTA)
      file_util::CreateDirectory(to_path);
    else
      ShellCopy(from_path, to_path, false);
  }

  FilePath directory = from_path.Append(L"*.*");
  return ShellCopy(directory, to_path, false);
}

bool PathExists(const FilePath& path) {
  ThreadRestrictions::AssertIOAllowed();
  return (GetFileAttributes(path.value().c_str()) != INVALID_FILE_ATTRIBUTES);
}

bool PathIsWritable(const FilePath& path) {
  ThreadRestrictions::AssertIOAllowed();
  HANDLE dir =
      CreateFile(path.value().c_str(), FILE_ADD_FILE, kFileShareAll,
                 NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);

  if (dir == INVALID_HANDLE_VALUE)
    return false;

  CloseHandle(dir);
  return true;
}

bool DirectoryExists(const FilePath& path) {
  ThreadRestrictions::AssertIOAllowed();
  DWORD fileattr = GetFileAttributes(path.value().c_str());
  if (fileattr != INVALID_FILE_ATTRIBUTES)
    return (fileattr & FILE_ATTRIBUTE_DIRECTORY) != 0;
  return false;
}

}  // namespace base

// -----------------------------------------------------------------------------

namespace file_util {

using base::DirectoryExists;
using base::FilePath;
using base::kFileShareAll;

bool GetTempDir(FilePath* path) {
  base::ThreadRestrictions::AssertIOAllowed();

  wchar_t temp_path[MAX_PATH + 1];
  DWORD path_len = ::GetTempPath(MAX_PATH, temp_path);
  if (path_len >= MAX_PATH || path_len <= 0)
    return false;
  // TODO(evanm): the old behavior of this function was to always strip the
  // trailing slash.  We duplicate this here, but it shouldn't be necessary
  // when everyone is using the appropriate FilePath APIs.
  *path = FilePath(temp_path).StripTrailingSeparators();
  return true;
}

bool GetShmemTempDir(FilePath* path, bool executable) {
  return GetTempDir(path);
}

bool CreateTemporaryFile(FilePath* path) {
  base::ThreadRestrictions::AssertIOAllowed();

  FilePath temp_file;

  if (!GetTempDir(path))
    return false;

  if (CreateTemporaryFileInDir(*path, &temp_file)) {
    *path = temp_file;
    return true;
  }

  return false;
}

FILE* CreateAndOpenTemporaryShmemFile(FilePath* path, bool executable) {
  base::ThreadRestrictions::AssertIOAllowed();
  return CreateAndOpenTemporaryFile(path);
}

// On POSIX we have semantics to create and open a temporary file
// atomically.
// TODO(jrg): is there equivalent call to use on Windows instead of
// going 2-step?
FILE* CreateAndOpenTemporaryFileInDir(const FilePath& dir, FilePath* path) {
  base::ThreadRestrictions::AssertIOAllowed();
  if (!CreateTemporaryFileInDir(dir, path)) {
    return NULL;
  }
  // Open file in binary mode, to avoid problems with fwrite. On Windows
  // it replaces \n's with \r\n's, which may surprise you.
  // Reference: http://msdn.microsoft.com/en-us/library/h9t88zwz(VS.71).aspx
  return OpenFile(*path, "wb+");
}

bool CreateTemporaryFileInDir(const FilePath& dir,
                              FilePath* temp_file) {
  base::ThreadRestrictions::AssertIOAllowed();

  wchar_t temp_name[MAX_PATH + 1];

  if (!GetTempFileName(dir.value().c_str(), L"", 0, temp_name)) {
    DPLOG(WARNING) << "Failed to get temporary file name in " << dir.value();
    return false;
  }

  wchar_t long_temp_name[MAX_PATH + 1];
  DWORD long_name_len = GetLongPathName(temp_name, long_temp_name, MAX_PATH);
  if (long_name_len > MAX_PATH || long_name_len == 0) {
    // GetLongPathName() failed, but we still have a temporary file.
    *temp_file = FilePath(temp_name);
    return true;
  }

  FilePath::StringType long_temp_name_str;
  long_temp_name_str.assign(long_temp_name, long_name_len);
  *temp_file = FilePath(long_temp_name_str);
  return true;
}

bool CreateTemporaryDirInDir(const FilePath& base_dir,
                             const FilePath::StringType& prefix,
                             FilePath* new_dir) {
  base::ThreadRestrictions::AssertIOAllowed();

  FilePath path_to_create;

  for (int count = 0; count < 50; ++count) {
    // Try create a new temporary directory with random generated name. If
    // the one exists, keep trying another path name until we reach some limit.
    string16 new_dir_name;
    new_dir_name.assign(prefix);
    new_dir_name.append(base::IntToString16(::base::GetCurrentProcId()));
    new_dir_name.push_back('_');
    new_dir_name.append(base::IntToString16(base::RandInt(0, kint16max)));

    path_to_create = base_dir.Append(new_dir_name);
    if (::CreateDirectory(path_to_create.value().c_str(), NULL)) {
      *new_dir = path_to_create;
      return true;
    }
  }

  return false;
}

bool CreateNewTempDirectory(const FilePath::StringType& prefix,
                            FilePath* new_temp_path) {
  base::ThreadRestrictions::AssertIOAllowed();

  FilePath system_temp_dir;
  if (!GetTempDir(&system_temp_dir))
    return false;

  return CreateTemporaryDirInDir(system_temp_dir, prefix, new_temp_path);
}

bool CreateDirectoryAndGetError(const FilePath& full_path,
                                base::PlatformFileError* error) {
  base::ThreadRestrictions::AssertIOAllowed();

  // If the path exists, we've succeeded if it's a directory, failed otherwise.
  const wchar_t* full_path_str = full_path.value().c_str();
  DWORD fileattr = ::GetFileAttributes(full_path_str);
  if (fileattr != INVALID_FILE_ATTRIBUTES) {
    if ((fileattr & FILE_ATTRIBUTE_DIRECTORY) != 0) {
      DVLOG(1) << "CreateDirectory(" << full_path_str << "), "
               << "directory already exists.";
      return true;
    }
    DLOG(WARNING) << "CreateDirectory(" << full_path_str << "), "
                  << "conflicts with existing file.";
    if (error) {
      *error = base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY;
    }
    return false;
  }

  // Invariant:  Path does not exist as file or directory.

  // Attempt to create the parent recursively.  This will immediately return
  // true if it already exists, otherwise will create all required parent
  // directories starting with the highest-level missing parent.
  FilePath parent_path(full_path.DirName());
  if (parent_path.value() == full_path.value()) {
    if (error) {
      *error = base::PLATFORM_FILE_ERROR_NOT_FOUND;
    }
    return false;
  }
  if (!CreateDirectoryAndGetError(parent_path, error)) {
    DLOG(WARNING) << "Failed to create one of the parent directories.";
    if (error) {
      DCHECK(*error != base::PLATFORM_FILE_OK);
    }
    return false;
  }

  if (!::CreateDirectory(full_path_str, NULL)) {
    DWORD error_code = ::GetLastError();
    if (error_code == ERROR_ALREADY_EXISTS && DirectoryExists(full_path)) {
      // This error code ERROR_ALREADY_EXISTS doesn't indicate whether we
      // were racing with someone creating the same directory, or a file
      // with the same path.  If DirectoryExists() returns true, we lost the
      // race to create the same directory.
      return true;
    } else {
      if (error)
        *error = base::LastErrorToPlatformFileError(error_code);
      DLOG(WARNING) << "Failed to create directory " << full_path_str
                    << ", last error is " << error_code << ".";
      return false;
    }
  } else {
    return true;
  }
}

// TODO(rkc): Work out if we want to handle NTFS junctions here or not, handle
// them if we do decide to.
bool IsLink(const FilePath& file_path) {
  return false;
}

bool GetFileInfo(const FilePath& file_path, base::PlatformFileInfo* results) {
  base::ThreadRestrictions::AssertIOAllowed();

  WIN32_FILE_ATTRIBUTE_DATA attr;
  if (!GetFileAttributesEx(file_path.value().c_str(),
                           GetFileExInfoStandard, &attr)) {
    return false;
  }

  ULARGE_INTEGER size;
  size.HighPart = attr.nFileSizeHigh;
  size.LowPart = attr.nFileSizeLow;
  results->size = size.QuadPart;

  results->is_directory =
      (attr.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
  results->last_modified = base::Time::FromFileTime(attr.ftLastWriteTime);
  results->last_accessed = base::Time::FromFileTime(attr.ftLastAccessTime);
  results->creation_time = base::Time::FromFileTime(attr.ftCreationTime);

  return true;
}

FILE* OpenFile(const FilePath& filename, const char* mode) {
  base::ThreadRestrictions::AssertIOAllowed();
  std::wstring w_mode = ASCIIToWide(std::string(mode));
  return _wfsopen(filename.value().c_str(), w_mode.c_str(), _SH_DENYNO);
}

FILE* OpenFile(const std::string& filename, const char* mode) {
  base::ThreadRestrictions::AssertIOAllowed();
  return _fsopen(filename.c_str(), mode, _SH_DENYNO);
}

int ReadFile(const FilePath& filename, char* data, int size) {
  base::ThreadRestrictions::AssertIOAllowed();
  base::win::ScopedHandle file(CreateFile(filename.value().c_str(),
                                          GENERIC_READ,
                                          FILE_SHARE_READ | FILE_SHARE_WRITE,
                                          NULL,
                                          OPEN_EXISTING,
                                          FILE_FLAG_SEQUENTIAL_SCAN,
                                          NULL));
  if (!file)
    return -1;

  DWORD read;
  if (::ReadFile(file, data, size, &read, NULL) &&
      static_cast<int>(read) == size)
    return read;
  return -1;
}

int WriteFile(const FilePath& filename, const char* data, int size) {
  base::ThreadRestrictions::AssertIOAllowed();
  base::win::ScopedHandle file(CreateFile(filename.value().c_str(),
                                          GENERIC_WRITE,
                                          0,
                                          NULL,
                                          CREATE_ALWAYS,
                                          0,
                                          NULL));
  if (!file) {
    DLOG_GETLASTERROR(WARNING) << "CreateFile failed for path "
                               << filename.value();
    return -1;
  }

  DWORD written;
  BOOL result = ::WriteFile(file, data, size, &written, NULL);
  if (result && static_cast<int>(written) == size)
    return written;

  if (!result) {
    // WriteFile failed.
    DLOG_GETLASTERROR(WARNING) << "writing file " << filename.value()
                               << " failed";
  } else {
    // Didn't write all the bytes.
    DLOG(WARNING) << "wrote" << written << " bytes to "
                  << filename.value() << " expected " << size;
  }
  return -1;
}

int AppendToFile(const FilePath& filename, const char* data, int size) {
  base::ThreadRestrictions::AssertIOAllowed();
  base::win::ScopedHandle file(CreateFile(filename.value().c_str(),
                                          FILE_APPEND_DATA,
                                          0,
                                          NULL,
                                          OPEN_EXISTING,
                                          0,
                                          NULL));
  if (!file) {
    DLOG_GETLASTERROR(WARNING) << "CreateFile failed for path "
                               << filename.value();
    return -1;
  }

  DWORD written;
  BOOL result = ::WriteFile(file, data, size, &written, NULL);
  if (result && static_cast<int>(written) == size)
    return written;

  if (!result) {
    // WriteFile failed.
    DLOG_GETLASTERROR(WARNING) << "writing file " << filename.value()
                               << " failed";
  } else {
    // Didn't write all the bytes.
    DLOG(WARNING) << "wrote" << written << " bytes to "
                  << filename.value() << " expected " << size;
  }
  return -1;
}

// Gets the current working directory for the process.
bool GetCurrentDirectory(FilePath* dir) {
  base::ThreadRestrictions::AssertIOAllowed();

  wchar_t system_buffer[MAX_PATH];
  system_buffer[0] = 0;
  DWORD len = ::GetCurrentDirectory(MAX_PATH, system_buffer);
  if (len == 0 || len > MAX_PATH)
    return false;
  // TODO(evanm): the old behavior of this function was to always strip the
  // trailing slash.  We duplicate this here, but it shouldn't be necessary
  // when everyone is using the appropriate FilePath APIs.
  std::wstring dir_str(system_buffer);
  *dir = FilePath(dir_str).StripTrailingSeparators();
  return true;
}

// Sets the current working directory for the process.
bool SetCurrentDirectory(const FilePath& directory) {
  base::ThreadRestrictions::AssertIOAllowed();
  BOOL ret = ::SetCurrentDirectory(directory.value().c_str());
  return ret != 0;
}

bool NormalizeFilePath(const FilePath& path, FilePath* real_path) {
  base::ThreadRestrictions::AssertIOAllowed();
  FilePath mapped_file;
  if (!NormalizeToNativeFilePath(path, &mapped_file))
    return false;
  // NormalizeToNativeFilePath() will return a path that starts with
  // "\Device\Harddisk...".  Helper DevicePathToDriveLetterPath()
  // will find a drive letter which maps to the path's device, so
  // that we return a path starting with a drive letter.
  return DevicePathToDriveLetterPath(mapped_file, real_path);
}

bool DevicePathToDriveLetterPath(const FilePath& nt_device_path,
                                 FilePath* out_drive_letter_path) {
  base::ThreadRestrictions::AssertIOAllowed();

  // Get the mapping of drive letters to device paths.
  const int kDriveMappingSize = 1024;
  wchar_t drive_mapping[kDriveMappingSize] = {'\0'};
  if (!::GetLogicalDriveStrings(kDriveMappingSize - 1, drive_mapping)) {
    DLOG(ERROR) << "Failed to get drive mapping.";
    return false;
  }

  // The drive mapping is a sequence of null terminated strings.
  // The last string is empty.
  wchar_t* drive_map_ptr = drive_mapping;
  wchar_t device_path_as_string[MAX_PATH];
  wchar_t drive[] = L" :";

  // For each string in the drive mapping, get the junction that links
  // to it.  If that junction is a prefix of |device_path|, then we
  // know that |drive| is the real path prefix.
  while (*drive_map_ptr) {
    drive[0] = drive_map_ptr[0];  // Copy the drive letter.

    if (QueryDosDevice(drive, device_path_as_string, MAX_PATH)) {
      FilePath device_path(device_path_as_string);
      if (device_path == nt_device_path ||
          device_path.IsParent(nt_device_path)) {
        *out_drive_letter_path = FilePath(drive +
            nt_device_path.value().substr(wcslen(device_path_as_string)));
        return true;
      }
    }
    // Move to the next drive letter string, which starts one
    // increment after the '\0' that terminates the current string.
    while (*drive_map_ptr++);
  }

  // No drive matched.  The path does not start with a device junction
  // that is mounted as a drive letter.  This means there is no drive
  // letter path to the volume that holds |device_path|, so fail.
  return false;
}

bool NormalizeToNativeFilePath(const FilePath& path, FilePath* nt_path) {
  base::ThreadRestrictions::AssertIOAllowed();
  // In Vista, GetFinalPathNameByHandle() would give us the real path
  // from a file handle.  If we ever deprecate XP, consider changing the
  // code below to a call to GetFinalPathNameByHandle().  The method this
  // function uses is explained in the following msdn article:
  // http://msdn.microsoft.com/en-us/library/aa366789(VS.85).aspx
  base::win::ScopedHandle file_handle(
      ::CreateFile(path.value().c_str(),
                   GENERIC_READ,
                   kFileShareAll,
                   NULL,
                   OPEN_EXISTING,
                   FILE_ATTRIBUTE_NORMAL,
                   NULL));
  if (!file_handle)
    return false;

  // Create a file mapping object.  Can't easily use MemoryMappedFile, because
  // we only map the first byte, and need direct access to the handle. You can
  // not map an empty file, this call fails in that case.
  base::win::ScopedHandle file_map_handle(
      ::CreateFileMapping(file_handle.Get(),
                          NULL,
                          PAGE_READONLY,
                          0,
                          1,  // Just one byte.  No need to look at the data.
                          NULL));
  if (!file_map_handle)
    return false;

  // Use a view of the file to get the path to the file.
  void* file_view = MapViewOfFile(file_map_handle.Get(),
                                  FILE_MAP_READ, 0, 0, 1);
  if (!file_view)
    return false;

  // The expansion of |path| into a full path may make it longer.
  // GetMappedFileName() will fail if the result is longer than MAX_PATH.
  // Pad a bit to be safe.  If kMaxPathLength is ever changed to be less
  // than MAX_PATH, it would be nessisary to test that GetMappedFileName()
  // not return kMaxPathLength.  This would mean that only part of the
  // path fit in |mapped_file_path|.
  const int kMaxPathLength = MAX_PATH + 10;
  wchar_t mapped_file_path[kMaxPathLength];
  bool success = false;
  HANDLE cp = GetCurrentProcess();
  if (::GetMappedFileNameW(cp, file_view, mapped_file_path, kMaxPathLength)) {
    *nt_path = FilePath(mapped_file_path);
    success = true;
  }
  ::UnmapViewOfFile(file_view);
  return success;
}

int GetMaximumPathComponentLength(const FilePath& path) {
  base::ThreadRestrictions::AssertIOAllowed();

  wchar_t volume_path[MAX_PATH];
  if (!GetVolumePathNameW(path.NormalizePathSeparators().value().c_str(),
                          volume_path,
                          arraysize(volume_path))) {
    return -1;
  }

  DWORD max_length = 0;
  if (!GetVolumeInformationW(volume_path, NULL, 0, NULL, &max_length, NULL,
                             NULL, 0)) {
    return -1;
  }

  // Length of |path| with path separator appended.
  size_t prefix = path.StripTrailingSeparators().value().size() + 1;
  // The whole path string must be shorter than MAX_PATH. That is, it must be
  // prefix + component_length < MAX_PATH (or equivalently, <= MAX_PATH - 1).
  int whole_path_limit = std::max(0, MAX_PATH - 1 - static_cast<int>(prefix));
  return std::min(whole_path_limit, static_cast<int>(max_length));
}

}  // namespace file_util

namespace base {
namespace internal {

bool MoveUnsafe(const FilePath& from_path, const FilePath& to_path) {
  ThreadRestrictions::AssertIOAllowed();

  // NOTE: I suspect we could support longer paths, but that would involve
  // analyzing all our usage of files.
  if (from_path.value().length() >= MAX_PATH ||
      to_path.value().length() >= MAX_PATH) {
    return false;
  }
  if (MoveFileEx(from_path.value().c_str(), to_path.value().c_str(),
                 MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING) != 0)
    return true;

  // Keep the last error value from MoveFileEx around in case the below
  // fails.
  bool ret = false;
  DWORD last_error = ::GetLastError();

  if (DirectoryExists(from_path)) {
    // MoveFileEx fails if moving directory across volumes. We will simulate
    // the move by using Copy and Delete. Ideally we could check whether
    // from_path and to_path are indeed in different volumes.
    ret = internal::CopyAndDeleteDirectory(from_path, to_path);
  }

  if (!ret) {
    // Leave a clue about what went wrong so that it can be (at least) picked
    // up by a PLOG entry.
    ::SetLastError(last_error);
  }

  return ret;
}

bool CopyFileUnsafe(const FilePath& from_path, const FilePath& to_path) {
  ThreadRestrictions::AssertIOAllowed();

  // NOTE: I suspect we could support longer paths, but that would involve
  // analyzing all our usage of files.
  if (from_path.value().length() >= MAX_PATH ||
      to_path.value().length() >= MAX_PATH) {
    return false;
  }
  return (::CopyFile(from_path.value().c_str(), to_path.value().c_str(),
                     false) != 0);
}

bool CopyAndDeleteDirectory(const FilePath& from_path,
                            const FilePath& to_path) {
  ThreadRestrictions::AssertIOAllowed();
  if (CopyDirectory(from_path, to_path, true)) {
    if (DeleteFile(from_path, true))
      return true;

    // Like Move, this function is not transactional, so we just
    // leave the copied bits behind if deleting from_path fails.
    // If to_path exists previously then we have already overwritten
    // it by now, we don't get better off by deleting the new bits.
  }
  return false;
}

}  // namespace internal
}  // namespace base
