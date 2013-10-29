// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains utility functions for dealing with the local
// filesystem.

#ifndef BASE_FILE_UTIL_H_
#define BASE_FILE_UTIL_H_

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#elif defined(OS_POSIX)
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <stdio.h>

#include <set>
#include <string>
#include <vector>

#include "base/base_export.h"
#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/platform_file.h"
#include "base/strings/string16.h"

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#endif

namespace base {

class Time;

extern bool g_bug108724_debug;

//-----------------------------------------------------------------------------
// Functions that involve filesystem access or modification:

// Returns an absolute version of a relative path. Returns an empty path on
// error. On POSIX, this function fails if the path does not exist. This
// function can result in I/O so it can be slow.
BASE_EXPORT FilePath MakeAbsoluteFilePath(const FilePath& input);

// Returns the total number of bytes used by all the files under |root_path|.
// If the path does not exist the function returns 0.
//
// This function is implemented using the FileEnumerator class so it is not
// particularly speedy in any platform.
BASE_EXPORT int64 ComputeDirectorySize(const FilePath& root_path);

// Deletes the given path, whether it's a file or a directory.
// If it's a directory, it's perfectly happy to delete all of the
// directory's contents.  Passing true to recursive deletes
// subdirectories and their contents as well.
// Returns true if successful, false otherwise. It is considered successful
// to attempt to delete a file that does not exist.
//
// In posix environment and if |path| is a symbolic link, this deletes only
// the symlink. (even if the symlink points to a non-existent file)
//
// WARNING: USING THIS WITH recursive==true IS EQUIVALENT
//          TO "rm -rf", SO USE WITH CAUTION.
BASE_EXPORT bool DeleteFile(const FilePath& path, bool recursive);

#if defined(OS_WIN)
// Schedules to delete the given path, whether it's a file or a directory, until
// the operating system is restarted.
// Note:
// 1) The file/directory to be deleted should exist in a temp folder.
// 2) The directory to be deleted must be empty.
BASE_EXPORT bool DeleteFileAfterReboot(const FilePath& path);
#endif

// Moves the given path, whether it's a file or a directory.
// If a simple rename is not possible, such as in the case where the paths are
// on different volumes, this will attempt to copy and delete. Returns
// true for success.
// This function fails if either path contains traversal components ('..').
BASE_EXPORT bool Move(const FilePath& from_path, const FilePath& to_path);

// Renames file |from_path| to |to_path|. Both paths must be on the same
// volume, or the function will fail. Destination file will be created
// if it doesn't exist. Prefer this function over Move when dealing with
// temporary files. On Windows it preserves attributes of the target file.
// Returns true on success, leaving *error unchanged.
// Returns false on failure and sets *error appropriately, if it is non-NULL.
BASE_EXPORT bool ReplaceFile(const FilePath& from_path,
                             const FilePath& to_path,
                             PlatformFileError* error);

// Copies a single file. Use CopyDirectory to copy directories.
// This function fails if either path contains traversal components ('..').
BASE_EXPORT bool CopyFile(const FilePath& from_path, const FilePath& to_path);

// Copies the given path, and optionally all subdirectories and their contents
// as well.
//
// If there are files existing under to_path, always overwrite. Returns true
// if successful, false otherwise. Wildcards on the names are not supported.
//
// If you only need to copy a file use CopyFile, it's faster.
BASE_EXPORT bool CopyDirectory(const FilePath& from_path,
                               const FilePath& to_path,
                               bool recursive);

// Returns true if the given path exists on the local filesystem,
// false otherwise.
BASE_EXPORT bool PathExists(const FilePath& path);

// Returns true if the given path is writable by the user, false otherwise.
BASE_EXPORT bool PathIsWritable(const FilePath& path);

// Returns true if the given path exists and is a directory, false otherwise.
BASE_EXPORT bool DirectoryExists(const FilePath& path);

// Returns true if the contents of the two files given are equal, false
// otherwise.  If either file can't be read, returns false.
BASE_EXPORT bool ContentsEqual(const FilePath& filename1,
                               const FilePath& filename2);

// Returns true if the contents of the two text files given are equal, false
// otherwise.  This routine treats "\r\n" and "\n" as equivalent.
BASE_EXPORT bool TextContentsEqual(const FilePath& filename1,
                                   const FilePath& filename2);

// Read the file at |path| into |contents|, returning true on success.
// This function fails if the |path| contains path traversal components ('..').
// |contents| may be NULL, in which case this function is useful for its
// side effect of priming the disk cache.
// Useful for unit tests.
BASE_EXPORT bool ReadFileToString(const FilePath& path, std::string* contents);

}  // namespace base

// -----------------------------------------------------------------------------

namespace file_util {

#if defined(OS_POSIX)
// Read exactly |bytes| bytes from file descriptor |fd|, storing the result
// in |buffer|. This function is protected against EINTR and partial reads.
// Returns true iff |bytes| bytes have been successfully read from |fd|.
BASE_EXPORT bool ReadFromFD(int fd, char* buffer, size_t bytes);

// Creates a symbolic link at |symlink| pointing to |target|.  Returns
// false on failure.
BASE_EXPORT bool CreateSymbolicLink(const base::FilePath& target,
                                    const base::FilePath& symlink);

// Reads the given |symlink| and returns where it points to in |target|.
// Returns false upon failure.
BASE_EXPORT bool ReadSymbolicLink(const base::FilePath& symlink,
                                  base::FilePath* target);

// Bits ans masks of the file permission.
enum FilePermissionBits {
  FILE_PERMISSION_MASK              = S_IRWXU | S_IRWXG | S_IRWXO,
  FILE_PERMISSION_USER_MASK         = S_IRWXU,
  FILE_PERMISSION_GROUP_MASK        = S_IRWXG,
  FILE_PERMISSION_OTHERS_MASK       = S_IRWXO,

  FILE_PERMISSION_READ_BY_USER      = S_IRUSR,
  FILE_PERMISSION_WRITE_BY_USER     = S_IWUSR,
  FILE_PERMISSION_EXECUTE_BY_USER   = S_IXUSR,
  FILE_PERMISSION_READ_BY_GROUP     = S_IRGRP,
  FILE_PERMISSION_WRITE_BY_GROUP    = S_IWGRP,
  FILE_PERMISSION_EXECUTE_BY_GROUP  = S_IXGRP,
  FILE_PERMISSION_READ_BY_OTHERS    = S_IROTH,
  FILE_PERMISSION_WRITE_BY_OTHERS   = S_IWOTH,
  FILE_PERMISSION_EXECUTE_BY_OTHERS = S_IXOTH,
};

// Reads the permission of the given |path|, storing the file permission
// bits in |mode|. If |path| is symbolic link, |mode| is the permission of
// a file which the symlink points to.
BASE_EXPORT bool GetPosixFilePermissions(const base::FilePath& path,
                                         int* mode);
// Sets the permission of the given |path|. If |path| is symbolic link, sets
// the permission of a file which the symlink points to.
BASE_EXPORT bool SetPosixFilePermissions(const base::FilePath& path,
                                         int mode);
#endif  // defined(OS_POSIX)

// Return true if the given directory is empty
BASE_EXPORT bool IsDirectoryEmpty(const base::FilePath& dir_path);

// Get the temporary directory provided by the system.
// WARNING: DON'T USE THIS. If you want to create a temporary file, use one of
// the functions below.
BASE_EXPORT bool GetTempDir(base::FilePath* path);
// Get a temporary directory for shared memory files.
// Only useful on POSIX; redirects to GetTempDir() on Windows.
BASE_EXPORT bool GetShmemTempDir(base::FilePath* path, bool executable);

// Get the home directory.  This is more complicated than just getenv("HOME")
// as it knows to fall back on getpwent() etc.
BASE_EXPORT base::FilePath GetHomeDir();

// Creates a temporary file. The full path is placed in |path|, and the
// function returns true if was successful in creating the file. The file will
// be empty and all handles closed after this function returns.
BASE_EXPORT bool CreateTemporaryFile(base::FilePath* path);

// Same as CreateTemporaryFile but the file is created in |dir|.
BASE_EXPORT bool CreateTemporaryFileInDir(const base::FilePath& dir,
                                          base::FilePath* temp_file);

// Create and open a temporary file.  File is opened for read/write.
// The full path is placed in |path|.
// Returns a handle to the opened file or NULL if an error occurred.
BASE_EXPORT FILE* CreateAndOpenTemporaryFile(base::FilePath* path);
// Like above but for shmem files.  Only useful for POSIX.
// The executable flag says the file needs to support using
// mprotect with PROT_EXEC after mapping.
BASE_EXPORT FILE* CreateAndOpenTemporaryShmemFile(base::FilePath* path,
                                                  bool executable);
// Similar to CreateAndOpenTemporaryFile, but the file is created in |dir|.
BASE_EXPORT FILE* CreateAndOpenTemporaryFileInDir(const base::FilePath& dir,
                                                  base::FilePath* path);

// Create a new directory. If prefix is provided, the new directory name is in
// the format of prefixyyyy.
// NOTE: prefix is ignored in the POSIX implementation.
// If success, return true and output the full path of the directory created.
BASE_EXPORT bool CreateNewTempDirectory(
    const base::FilePath::StringType& prefix,
    base::FilePath* new_temp_path);

// Create a directory within another directory.
// Extra characters will be appended to |prefix| to ensure that the
// new directory does not have the same name as an existing directory.
BASE_EXPORT bool CreateTemporaryDirInDir(
    const base::FilePath& base_dir,
    const base::FilePath::StringType& prefix,
    base::FilePath* new_dir);

// Creates a directory, as well as creating any parent directories, if they
// don't exist. Returns 'true' on successful creation, or if the directory
// already exists.  The directory is only readable by the current user.
// Returns true on success, leaving *error unchanged.
// Returns false on failure and sets *error appropriately, if it is non-NULL.
BASE_EXPORT bool CreateDirectoryAndGetError(const base::FilePath& full_path,
                                            base::PlatformFileError* error);

// Backward-compatible convenience method for the above.
BASE_EXPORT bool CreateDirectory(const base::FilePath& full_path);

// Returns the file size. Returns true on success.
BASE_EXPORT bool GetFileSize(const base::FilePath& file_path, int64* file_size);

// Sets |real_path| to |path| with symbolic links and junctions expanded.
// On windows, make sure the path starts with a lettered drive.
// |path| must reference a file.  Function will fail if |path| points to
// a directory or to a nonexistent path.  On windows, this function will
// fail if |path| is a junction or symlink that points to an empty file,
// or if |real_path| would be longer than MAX_PATH characters.
BASE_EXPORT bool NormalizeFilePath(const base::FilePath& path,
                                   base::FilePath* real_path);

#if defined(OS_WIN)

// Given a path in NT native form ("\Device\HarddiskVolumeXX\..."),
// return in |drive_letter_path| the equivalent path that starts with
// a drive letter ("C:\...").  Return false if no such path exists.
BASE_EXPORT bool DevicePathToDriveLetterPath(const base::FilePath& device_path,
                                             base::FilePath* drive_letter_path);

// Given an existing file in |path|, set |real_path| to the path
// in native NT format, of the form "\Device\HarddiskVolumeXX\..".
// Returns false if the path can not be found. Empty files cannot
// be resolved with this function.
BASE_EXPORT bool NormalizeToNativeFilePath(const base::FilePath& path,
                                           base::FilePath* nt_path);
#endif

// This function will return if the given file is a symlink or not.
BASE_EXPORT bool IsLink(const base::FilePath& file_path);

// Returns information about the given file path.
BASE_EXPORT bool GetFileInfo(const base::FilePath& file_path,
                             base::PlatformFileInfo* info);

// Sets the time of the last access and the time of the last modification.
BASE_EXPORT bool TouchFile(const base::FilePath& path,
                           const base::Time& last_accessed,
                           const base::Time& last_modified);

// Set the time of the last modification. Useful for unit tests.
BASE_EXPORT bool SetLastModifiedTime(const base::FilePath& path,
                                     const base::Time& last_modified);

#if defined(OS_POSIX)
// Store inode number of |path| in |inode|. Return true on success.
BASE_EXPORT bool GetInode(const base::FilePath& path, ino_t* inode);
#endif

// Wrapper for fopen-like calls. Returns non-NULL FILE* on success.
BASE_EXPORT FILE* OpenFile(const base::FilePath& filename, const char* mode);

// Closes file opened by OpenFile. Returns true on success.
BASE_EXPORT bool CloseFile(FILE* file);

// Truncates an open file to end at the location of the current file pointer.
// This is a cross-platform analog to Windows' SetEndOfFile() function.
BASE_EXPORT bool TruncateFile(FILE* file);

// Reads the given number of bytes from the file into the buffer.  Returns
// the number of read bytes, or -1 on error.
BASE_EXPORT int ReadFile(const base::FilePath& filename, char* data, int size);

// Writes the given buffer into the file, overwriting any data that was
// previously there.  Returns the number of bytes written, or -1 on error.
BASE_EXPORT int WriteFile(const base::FilePath& filename, const char* data,
                          int size);
#if defined(OS_POSIX)
// Append the data to |fd|. Does not close |fd| when done.
BASE_EXPORT int WriteFileDescriptor(const int fd, const char* data, int size);
#endif
// Append the given buffer into the file. Returns the number of bytes written,
// or -1 on error.
BASE_EXPORT int AppendToFile(const base::FilePath& filename,
                             const char* data, int size);

// Gets the current working directory for the process.
BASE_EXPORT bool GetCurrentDirectory(base::FilePath* path);

// Sets the current working directory for the process.
BASE_EXPORT bool SetCurrentDirectory(const base::FilePath& path);

// Attempts to find a number that can be appended to the |path| to make it
// unique. If |path| does not exist, 0 is returned.  If it fails to find such
// a number, -1 is returned. If |suffix| is not empty, also checks the
// existence of it with the given suffix.
BASE_EXPORT int GetUniquePathNumber(const base::FilePath& path,
                                    const base::FilePath::StringType& suffix);

#if defined(OS_POSIX)
// Creates a directory with a guaranteed unique name based on |path|, returning
// the pathname if successful, or an empty path if there was an error creating
// the directory. Does not create parent directories.
BASE_EXPORT base::FilePath MakeUniqueDirectory(const base::FilePath& path);
#endif

#if defined(OS_POSIX)
// Test that |path| can only be changed by a given user and members of
// a given set of groups.
// Specifically, test that all parts of |path| under (and including) |base|:
// * Exist.
// * Are owned by a specific user.
// * Are not writable by all users.
// * Are owned by a member of a given set of groups, or are not writable by
//   their group.
// * Are not symbolic links.
// This is useful for checking that a config file is administrator-controlled.
// |base| must contain |path|.
BASE_EXPORT bool VerifyPathControlledByUser(const base::FilePath& base,
                                            const base::FilePath& path,
                                            uid_t owner_uid,
                                            const std::set<gid_t>& group_gids);
#endif  // defined(OS_POSIX)

#if defined(OS_MACOSX) && !defined(OS_IOS)
// Is |path| writable only by a user with administrator privileges?
// This function uses Mac OS conventions.  The super user is assumed to have
// uid 0, and the administrator group is assumed to be named "admin".
// Testing that |path|, and every parent directory including the root of
// the filesystem, are owned by the superuser, controlled by the group
// "admin", are not writable by all users, and contain no symbolic links.
// Will return false if |path| does not exist.
BASE_EXPORT bool VerifyPathControlledByAdmin(const base::FilePath& path);
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)

// Returns the maximum length of path component on the volume containing
// the directory |path|, in the number of FilePath::CharType, or -1 on failure.
BASE_EXPORT int GetMaximumPathComponentLength(const base::FilePath& path);

// A class to handle auto-closing of FILE*'s.
class ScopedFILEClose {
 public:
  inline void operator()(FILE* x) const {
    if (x) {
      fclose(x);
    }
  }
};

typedef scoped_ptr_malloc<FILE, ScopedFILEClose> ScopedFILE;

#if defined(OS_POSIX)
// A class to handle auto-closing of FDs.
class ScopedFDClose {
 public:
  inline void operator()(int* x) const {
    if (x && *x >= 0) {
      if (HANDLE_EINTR(close(*x)) < 0)
        DPLOG(ERROR) << "close";
    }
  }
};

typedef scoped_ptr_malloc<int, ScopedFDClose> ScopedFD;
#endif  // OS_POSIX

#if defined(OS_LINUX)
// Broad categories of file systems as returned by statfs() on Linux.
enum FileSystemType {
  FILE_SYSTEM_UNKNOWN,  // statfs failed.
  FILE_SYSTEM_0,        // statfs.f_type == 0 means unknown, may indicate AFS.
  FILE_SYSTEM_ORDINARY,       // on-disk filesystem like ext2
  FILE_SYSTEM_NFS,
  FILE_SYSTEM_SMB,
  FILE_SYSTEM_CODA,
  FILE_SYSTEM_MEMORY,         // in-memory file system
  FILE_SYSTEM_CGROUP,         // cgroup control.
  FILE_SYSTEM_OTHER,          // any other value.
  FILE_SYSTEM_TYPE_COUNT
};

// Attempts determine the FileSystemType for |path|.
// Returns false if |path| doesn't exist.
BASE_EXPORT bool GetFileSystemType(const base::FilePath& path,
                                   FileSystemType* type);
#endif

}  // namespace file_util

// Internal --------------------------------------------------------------------

namespace base {
namespace internal {

// Same as Move but allows paths with traversal components.
// Use only with extreme care.
BASE_EXPORT bool MoveUnsafe(const FilePath& from_path,
                            const FilePath& to_path);

// Same as CopyFile but allows paths with traversal components.
// Use only with extreme care.
BASE_EXPORT bool CopyFileUnsafe(const FilePath& from_path,
                                const FilePath& to_path);

#if defined(OS_WIN)
// Copy from_path to to_path recursively and then delete from_path recursively.
// Returns true if all operations succeed.
// This function simulates Move(), but unlike Move() it works across volumes.
// This function is not transactional.
BASE_EXPORT bool CopyAndDeleteDirectory(const FilePath& from_path,
                                        const FilePath& to_path);
#endif  // defined(OS_WIN)

}  // namespace internal
}  // namespace base

#endif  // BASE_FILE_UTIL_H_
