/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_UNIXFILESYSTEM_H_
#define WEBRTC_BASE_UNIXFILESYSTEM_H_

#include <sys/types.h>

#include "webrtc/base/fileutils.h"

namespace rtc {

class UnixFilesystem : public FilesystemInterface {
 public:
  UnixFilesystem();
  virtual ~UnixFilesystem();

#if defined(WEBRTC_ANDROID) || defined(WEBRTC_IOS)
  // Android does not have a native code API to fetch the app data or temp
  // folders. That needs to be passed into this class from Java. Similarly, iOS
  // only supports an Objective-C API for fetching the folder locations, so that
  // needs to be passed in here from Objective-C.  Or at least that used to be
  // the case; now the ctor will do the work if necessary and possible.
  // TODO(fischman): add an Android version that uses JNI and drop the
  // SetApp*Folder() APIs once external users stop using them.
  static void SetAppDataFolder(const std::string& folder);
  static void SetAppTempFolder(const std::string& folder);
#endif

  // Opens a file. Returns an open StreamInterface if function succeeds.
  // Otherwise, returns NULL.
  virtual FileStream *OpenFile(const Pathname &filename,
                               const std::string &mode);

  // Atomically creates an empty file accessible only to the current user if one
  // does not already exist at the given path, otherwise fails.
  virtual bool CreatePrivateFile(const Pathname &filename);

  // This will attempt to delete the file located at filename.
  // It will fail with VERIY if you pass it a non-existant file, or a directory.
  virtual bool DeleteFile(const Pathname &filename);

  // This will attempt to delete the folder located at 'folder'
  // It ASSERTs and returns false if you pass it a non-existant folder or a
  // plain file.
  virtual bool DeleteEmptyFolder(const Pathname &folder);

  // Creates a directory. This will call itself recursively to create /foo/bar
  // even if /foo does not exist. All created directories are created with the
  // given mode.
  // Returns TRUE if function succeeds
  virtual bool CreateFolder(const Pathname &pathname, mode_t mode);

  // As above, with mode = 0755.
  virtual bool CreateFolder(const Pathname &pathname);

  // This moves a file from old_path to new_path, where "file" can be a plain
  // file or directory, which will be moved recursively.
  // Returns true if function succeeds.
  virtual bool MoveFile(const Pathname &old_path, const Pathname &new_path);
  virtual bool MoveFolder(const Pathname &old_path, const Pathname &new_path);

  // This copies a file from old_path to _new_path where "file" can be a plain
  // file or directory, which will be copied recursively.
  // Returns true if function succeeds
  virtual bool CopyFile(const Pathname &old_path, const Pathname &new_path);

  // Returns true if a pathname is a directory
  virtual bool IsFolder(const Pathname& pathname);

  // Returns true if pathname represents a temporary location on the system.
  virtual bool IsTemporaryPath(const Pathname& pathname);

  // Returns true of pathname represents an existing file
  virtual bool IsFile(const Pathname& pathname);

  // Returns true if pathname refers to no filesystem object, every parent
  // directory either exists, or is also absent.
  virtual bool IsAbsent(const Pathname& pathname);

  virtual std::string TempFilename(const Pathname &dir,
                                   const std::string &prefix);

  // A folder appropriate for storing temporary files (Contents are
  // automatically deleted when the program exists)
  virtual bool GetTemporaryFolder(Pathname &path, bool create,
                                 const std::string *append);

  virtual bool GetFileSize(const Pathname& path, size_t* size);
  virtual bool GetFileTime(const Pathname& path, FileTimeType which,
                           time_t* time);

  // Returns the path to the running application.
  virtual bool GetAppPathname(Pathname* path);

  virtual bool GetAppDataFolder(Pathname* path, bool per_user);

  // Get a temporary folder that is unique to the current user and application.
  virtual bool GetAppTempFolder(Pathname* path);

  virtual bool GetDiskFreeSpace(const Pathname& path, int64 *freebytes);

  // Returns the absolute path of the current directory.
  virtual Pathname GetCurrentDirectory();

 private:
#if defined(WEBRTC_ANDROID) || defined(WEBRTC_IOS)
  static char* provided_app_data_folder_;
  static char* provided_app_temp_folder_;
#else
  static char* app_temp_path_;
#endif

  static char* CopyString(const std::string& str);
};

}  // namespace rtc

#endif  // WEBRTC_BASE_UNIXFILESYSTEM_H_
