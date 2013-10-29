// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PLATFORM_FILE_H_
#define BASE_PLATFORM_FILE_H_

#include "build/build_config.h"
#if defined(OS_WIN)
#include <windows.h>
#endif

#include <string>

#include "base/base_export.h"
#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/time/time.h"

namespace base {

// PLATFORM_FILE_(OPEN|CREATE).* are mutually exclusive. You should specify
// exactly one of the five (possibly combining with other flags) when opening
// or creating a file.
// PLATFORM_FILE_(WRITE|APPEND) are mutually exclusive. This is so that APPEND
// behavior will be consistent with O_APPEND on POSIX.
enum PlatformFileFlags {
  PLATFORM_FILE_OPEN = 1 << 0,             // Opens a file, only if it exists.
  PLATFORM_FILE_CREATE = 1 << 1,           // Creates a new file, only if it
                                           // does not already exist.
  PLATFORM_FILE_OPEN_ALWAYS = 1 << 2,      // May create a new file.
  PLATFORM_FILE_CREATE_ALWAYS = 1 << 3,    // May overwrite an old file.
  PLATFORM_FILE_OPEN_TRUNCATED = 1 << 4,   // Opens a file and truncates it,
                                           // only if it exists.
  PLATFORM_FILE_READ = 1 << 5,
  PLATFORM_FILE_WRITE = 1 << 6,
  PLATFORM_FILE_APPEND = 1 << 7,
  PLATFORM_FILE_EXCLUSIVE_READ = 1 << 8,   // EXCLUSIVE is opposite of Windows
                                           // SHARE
  PLATFORM_FILE_EXCLUSIVE_WRITE = 1 << 9,
  PLATFORM_FILE_ASYNC = 1 << 10,
  PLATFORM_FILE_TEMPORARY = 1 << 11,       // Used on Windows only
  PLATFORM_FILE_HIDDEN = 1 << 12,          // Used on Windows only
  PLATFORM_FILE_DELETE_ON_CLOSE = 1 << 13,

  PLATFORM_FILE_WRITE_ATTRIBUTES = 1 << 14,  // Used on Windows only
  PLATFORM_FILE_ENUMERATE = 1 << 15,         // May enumerate directory

  PLATFORM_FILE_SHARE_DELETE = 1 << 16,      // Used on Windows only

  PLATFORM_FILE_TERMINAL_DEVICE = 1 << 17,   // Serial port flags
  PLATFORM_FILE_BACKUP_SEMANTICS = 1 << 18,  // Used on Windows only

  PLATFORM_FILE_EXECUTE = 1 << 19,           // Used on Windows only
};

// PLATFORM_FILE_ERROR_ACCESS_DENIED is returned when a call fails because of
// a filesystem restriction. PLATFORM_FILE_ERROR_SECURITY is returned when a
// browser policy doesn't allow the operation to be executed.
enum PlatformFileError {
  PLATFORM_FILE_OK = 0,
  PLATFORM_FILE_ERROR_FAILED = -1,
  PLATFORM_FILE_ERROR_IN_USE = -2,
  PLATFORM_FILE_ERROR_EXISTS = -3,
  PLATFORM_FILE_ERROR_NOT_FOUND = -4,
  PLATFORM_FILE_ERROR_ACCESS_DENIED = -5,
  PLATFORM_FILE_ERROR_TOO_MANY_OPENED = -6,
  PLATFORM_FILE_ERROR_NO_MEMORY = -7,
  PLATFORM_FILE_ERROR_NO_SPACE = -8,
  PLATFORM_FILE_ERROR_NOT_A_DIRECTORY = -9,
  PLATFORM_FILE_ERROR_INVALID_OPERATION = -10,
  PLATFORM_FILE_ERROR_SECURITY = -11,
  PLATFORM_FILE_ERROR_ABORT = -12,
  PLATFORM_FILE_ERROR_NOT_A_FILE = -13,
  PLATFORM_FILE_ERROR_NOT_EMPTY = -14,
  PLATFORM_FILE_ERROR_INVALID_URL = -15,
  PLATFORM_FILE_ERROR_IO = -16,
  // Put new entries here and increment PLATFORM_FILE_ERROR_MAX.
  PLATFORM_FILE_ERROR_MAX = -17
};

// This explicit mapping matches both FILE_ on Windows and SEEK_ on Linux.
enum PlatformFileWhence {
  PLATFORM_FILE_FROM_BEGIN   = 0,
  PLATFORM_FILE_FROM_CURRENT = 1,
  PLATFORM_FILE_FROM_END     = 2
};

// Used to hold information about a given file.
// If you add more fields to this structure (platform-specific fields are OK),
// make sure to update all functions that use it in file_util_{win|posix}.cc
// too, and the ParamTraits<base::PlatformFileInfo> implementation in
// chrome/common/common_param_traits.cc.
struct BASE_EXPORT PlatformFileInfo {
  PlatformFileInfo();
  ~PlatformFileInfo();

  // The size of the file in bytes.  Undefined when is_directory is true.
  int64 size;

  // True if the file corresponds to a directory.
  bool is_directory;

  // True if the file corresponds to a symbolic link.
  bool is_symbolic_link;

  // The last modified time of a file.
  base::Time last_modified;

  // The last accessed time of a file.
  base::Time last_accessed;

  // The creation time of a file.
  base::Time creation_time;
};

#if defined(OS_WIN)
typedef HANDLE PlatformFile;
const PlatformFile kInvalidPlatformFileValue = INVALID_HANDLE_VALUE;
PlatformFileError LastErrorToPlatformFileError(DWORD saved_errno);
#elif defined(OS_POSIX)
typedef int PlatformFile;
const PlatformFile kInvalidPlatformFileValue = -1;
PlatformFileError ErrnoToPlatformFileError(int saved_errno);
#endif

// Creates or opens the given file. If |created| is provided, it will be set to
// true if a new file was created [or an old one truncated to zero length to
// simulate a new file, which can happen with PLATFORM_FILE_CREATE_ALWAYS], and
// false otherwise.  |error| can be NULL.
//
// This function fails with 'access denied' if the |name| contains path
// traversal ('..') components.
BASE_EXPORT PlatformFile CreatePlatformFile(const FilePath& name,
                                            int flags,
                                            bool* created,
                                            PlatformFileError* error);

// Same as CreatePlatformFile but allows paths with traversal (like \..\)
// components. Use only with extreme care.
BASE_EXPORT PlatformFile CreatePlatformFileUnsafe(const FilePath& name,
                                                  int flags,
                                                  bool* created,
                                                  PlatformFileError* error);

BASE_EXPORT FILE* FdopenPlatformFile(PlatformFile file, const char* mode);

// Closes a file handle. Returns |true| on success and |false| otherwise.
BASE_EXPORT bool ClosePlatformFile(PlatformFile file);

// Changes current position in the file to an |offset| relative to an origin
// defined by |whence|. Returns the resultant current position in the file
// (relative to the start) or -1 in case of error.
BASE_EXPORT int64 SeekPlatformFile(PlatformFile file,
                                   PlatformFileWhence whence,
                                   int64 offset);

// Reads the given number of bytes (or until EOF is reached) starting with the
// given offset. Returns the number of bytes read, or -1 on error. Note that
// this function makes a best effort to read all data on all platforms, so it is
// not intended for stream oriented files but instead for cases when the normal
// expectation is that actually |size| bytes are read unless there is an error.
BASE_EXPORT int ReadPlatformFile(PlatformFile file, int64 offset,
                                 char* data, int size);

// Same as above but without seek.
BASE_EXPORT int ReadPlatformFileAtCurrentPos(PlatformFile file,
                                             char* data, int size);

// Reads the given number of bytes (or until EOF is reached) starting with the
// given offset, but does not make any effort to read all data on all platforms.
// Returns the number of bytes read, or -1 on error.
BASE_EXPORT int ReadPlatformFileNoBestEffort(PlatformFile file, int64 offset,
                                             char* data, int size);

// Same as above but without seek.
BASE_EXPORT int ReadPlatformFileCurPosNoBestEffort(PlatformFile file,
                                                   char* data, int size);

// Writes the given buffer into the file at the given offset, overwritting any
// data that was previously there. Returns the number of bytes written, or -1
// on error. Note that this function makes a best effort to write all data on
// all platforms.
// Ignores the offset and writes to the end of the file if the file was opened
// with PLATFORM_FILE_APPEND.
BASE_EXPORT int WritePlatformFile(PlatformFile file, int64 offset,
                                  const char* data, int size);

// Save as above but without seek.
BASE_EXPORT int WritePlatformFileAtCurrentPos(PlatformFile file,
                                              const char* data, int size);

// Save as above but does not make any effort to write all data on all
// platforms. Returns the number of bytes written, or -1 on error.
BASE_EXPORT int WritePlatformFileCurPosNoBestEffort(PlatformFile file,
                                                    const char* data, int size);

// Truncates the given file to the given length. If |length| is greater than
// the current size of the file, the file is extended with zeros. If the file
// doesn't exist, |false| is returned.
BASE_EXPORT bool TruncatePlatformFile(PlatformFile file, int64 length);

// Flushes the buffers of the given file.
BASE_EXPORT bool FlushPlatformFile(PlatformFile file);

// Touches the given file.
BASE_EXPORT bool TouchPlatformFile(PlatformFile file,
                                   const Time& last_access_time,
                                   const Time& last_modified_time);

// Returns some information for the given file.
BASE_EXPORT bool GetPlatformFileInfo(PlatformFile file, PlatformFileInfo* info);

// Use this class to pass ownership of a PlatformFile to a receiver that may or
// may not want to accept it.  This class does not own the storage for the
// PlatformFile.
//
// EXAMPLE:
//
//  void MaybeProcessFile(PassPlatformFile pass_file) {
//    if (...) {
//      PlatformFile file = pass_file.ReleaseValue();
//      // Now, we are responsible for closing |file|.
//    }
//  }
//
//  void OpenAndMaybeProcessFile(const FilePath& path) {
//    PlatformFile file = CreatePlatformFile(path, ...);
//    MaybeProcessFile(PassPlatformFile(&file));
//    if (file != kInvalidPlatformFileValue)
//      ClosePlatformFile(file);
//  }
//
class BASE_EXPORT PassPlatformFile {
 public:
  explicit PassPlatformFile(PlatformFile* value) : value_(value) {
  }

  // Called to retrieve the PlatformFile stored in this object.  The caller
  // gains ownership of the PlatformFile and is now responsible for closing it.
  // Any subsequent calls to this method will return an invalid PlatformFile.
  PlatformFile ReleaseValue() {
    PlatformFile temp = *value_;
    *value_ = kInvalidPlatformFileValue;
    return temp;
  }

 private:
  PlatformFile* value_;
};

}  // namespace base

#endif  // BASE_PLATFORM_FILE_H_
