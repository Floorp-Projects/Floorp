/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/testsupport/fileutils.h"

#ifdef WIN32
#include <direct.h>
#define GET_CURRENT_DIR _getcwd
#else
#include <unistd.h>
#define GET_CURRENT_DIR getcwd
#endif

#include <sys/stat.h>  // To check for directory existence.
#ifndef S_ISDIR  // Not defined in stat.h on Windows.
#define S_ISDIR(mode) (((mode) & S_IFMT) == S_IFDIR)
#endif

#include <cstdio>

#include "typedefs.h"  // For architecture defines

namespace webrtc {
namespace test {

#ifdef WIN32
static const char* kPathDelimiter = "\\";
#else
static const char* kPathDelimiter = "/";
#endif
// The file we're looking for to identify the project root dir.
static const char* kProjectRootFileName = "DEPS";
static const char* kOutputDirName = "out";
static const char* kFallbackPath = "./";
#ifdef WEBRTC_ANDROID
static const char* kResourcesDirName = "/sdcard/";
#else
static const char* kResourcesDirName = "resources";
#endif
const char* kCannotFindProjectRootDir = "ERROR_CANNOT_FIND_PROJECT_ROOT_DIR";

std::string ProjectRootPath() {
  std::string working_dir = WorkingDir();
  if (working_dir == kFallbackPath) {
    return kCannotFindProjectRootDir;
  }
  // Check for our file that verifies the root dir.
  std::string current_path(working_dir);
  FILE* file = NULL;
  int path_delimiter_index = current_path.find_last_of(kPathDelimiter);
  while (path_delimiter_index > -1) {
    std::string root_filename = current_path + kPathDelimiter +
        kProjectRootFileName;
    file = fopen(root_filename.c_str(), "r");
    if (file != NULL) {
      fclose(file);
      return current_path + kPathDelimiter;
    }
    // Move up one directory in the directory tree.
    current_path = current_path.substr(0, path_delimiter_index);
    path_delimiter_index = current_path.find_last_of(kPathDelimiter);
  }
  // Reached the root directory.
  fprintf(stderr, "Cannot find project root directory!\n");
  return kCannotFindProjectRootDir;
}

#ifdef WEBRTC_ANDROID

std::string OutputPath() {
  // We need to touch this variable so it doesn't get flagged as unused.
  (void)kOutputDirName;
  return "/sdcard/";
}

#else  // WEBRTC_ANDROID

std::string OutputPath() {
  std::string path = ProjectRootPath();
  if (path == kCannotFindProjectRootDir) {
    return kFallbackPath;
  }
  path += kOutputDirName;
  if (!CreateDirectory(path)) {
    return kFallbackPath;
  }
  return path + kPathDelimiter;
}

#endif  // !WEBRTC_ANDROID

std::string WorkingDir() {
  char path_buffer[FILENAME_MAX];
  if (!GET_CURRENT_DIR(path_buffer, sizeof(path_buffer))) {
    fprintf(stderr, "Cannot get current directory!\n");
    return kFallbackPath;
  } else {
    return std::string(path_buffer);
  }
}

bool CreateDirectory(std::string directory_name) {
  struct stat path_info = {0};
  // Check if the path exists already:
  if (stat(directory_name.c_str(), &path_info) == 0) {
    if (!S_ISDIR(path_info.st_mode)) {
      fprintf(stderr, "Path %s exists but is not a directory! Remove this "
              "file and re-run to create the directory.\n",
              directory_name.c_str());
      return false;
    }
  } else {
#ifdef WIN32
    return _mkdir(directory_name.c_str()) == 0;
#else
    return mkdir(directory_name.c_str(),  S_IRWXU | S_IRWXG | S_IRWXO) == 0;
#endif
  }
  return true;
}

bool FileExists(std::string file_name) {
  struct stat file_info = {0};
  return stat(file_name.c_str(), &file_info) == 0;
}

std::string ResourcePath(std::string name, std::string extension) {
  std::string platform = "win";
#ifdef WEBRTC_LINUX
  platform = "linux";
#endif  // WEBRTC_LINUX
#ifdef WEBRTC_MAC
  platform = "mac";
#endif  // WEBRTC_MAC

#ifdef WEBRTC_ARCH_64_BITS
  std::string architecture = "64";
#else
  std::string architecture = "32";
#endif  // WEBRTC_ARCH_64_BITS

#ifdef WEBRTC_ANDROID
  std::string resources_path = kResourcesDirName;
#else
  std::string resources_path = ProjectRootPath() + kResourcesDirName +
      kPathDelimiter;
  std::string resource_file = resources_path + name + "_" + platform + "_" +
      architecture + "." + extension;
  if (FileExists(resource_file)) {
    return resource_file;
  }
  // Try without architecture.
  resource_file = resources_path + name + "_" + platform + "." + extension;
  if (FileExists(resource_file)) {
    return resource_file;
  }
  // Try without platform.
  resource_file = resources_path + name + "_" + architecture + "." + extension;
  if (FileExists(resource_file)) {
    return resource_file;
  }
#endif
  // Fall back on name without architecture or platform.
  return resources_path + name + "." + extension;
}

size_t GetFileSize(std::string filename) {
  FILE* f = fopen(filename.c_str(), "rb");
  size_t size = 0;
  if (f != NULL) {
    if (fseek(f, 0, SEEK_END) == 0) {
      size = ftell(f);
    }
    fclose(f);
  }
  return size;
}

}  // namespace test
}  // namespace webrtc
