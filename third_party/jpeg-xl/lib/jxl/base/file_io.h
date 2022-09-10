// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JXL_BASE_FILE_IO_H_
#define LIB_JXL_BASE_FILE_IO_H_

// Helper functions for reading/writing files.

#include <stdio.h>
#include <sys/stat.h>

#include <list>
#include <string>
#include <vector>

#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/padded_bytes.h"
#include "lib/jxl/base/status.h"

namespace jxl {

// Returns extension including the dot, or empty string if none. Assumes
// filename is not a hidden file (e.g. ".bashrc"). May be called with a pathname
// if the filename contains a dot and/or no other path component does.
static inline std::string Extension(const std::string& filename) {
  const size_t pos = filename.rfind('.');
  if (pos == std::string::npos) return std::string();
  return filename.substr(pos);
}

// RAII, ensures files are closed even when returning early.
class FileWrapper {
 public:
  FileWrapper(const FileWrapper& other) = delete;
  FileWrapper& operator=(const FileWrapper& other) = delete;

  explicit FileWrapper(const std::string& pathname, const char* mode)
      : file_(pathname == "-" ? (mode[0] == 'r' ? stdin : stdout)
                              : fopen(pathname.c_str(), mode)),
        close_on_delete_(pathname != "-") {
#ifdef _WIN32
    struct __stat64 s = {};
    const int err = _stat64(pathname.c_str(), &s);
    const bool is_file = (s.st_mode & S_IFREG) != 0;
#else
    struct stat s = {};
    const int err = stat(pathname.c_str(), &s);
    const bool is_file = S_ISREG(s.st_mode);
#endif
    if (err == 0 && is_file) {
      size_ = s.st_size;
    }
  }

  ~FileWrapper() {
    if (file_ != nullptr && close_on_delete_) {
      const int err = fclose(file_);
      JXL_CHECK(err == 0);
    }
  }

  // We intend to use FileWrapper as a replacement of FILE.
  // NOLINTNEXTLINE(google-explicit-constructor)
  operator FILE*() const { return file_; }

  int64_t size() { return size_; }

 private:
  FILE* const file_;
  bool close_on_delete_ = true;
  int64_t size_ = -1;
};

template <typename ContainerType>
static inline Status ReadFile(const std::string& pathname,
                              ContainerType* JXL_RESTRICT bytes) {
  FileWrapper f(pathname, "rb");
  if (f == nullptr)
    return JXL_FAILURE("Failed to open file for reading: %s", pathname.c_str());

  // Get size of file in bytes
  const int64_t size = f.size();
  if (size < 0) {
    // Size is unknown, loop reading chunks until EOF.
    bytes->clear();
    std::list<std::vector<uint8_t>> chunks;

    size_t total_size = 0;
    while (true) {
      std::vector<uint8_t> chunk(16 * 1024);
      const size_t bytes_read = fread(chunk.data(), 1, chunk.size(), f);
      if (ferror(f) || bytes_read > chunk.size()) {
        return JXL_FAILURE("Error reading %s", pathname.c_str());
      }

      chunk.resize(bytes_read);
      total_size += bytes_read;
      if (bytes_read != 0) {
        chunks.emplace_back(std::move(chunk));
      }
      if (feof(f)) {
        break;
      }
    }
    bytes->resize(total_size);
    size_t pos = 0;
    for (const auto& chunk : chunks) {
      // Needed in case ContainerType is std::string, whose data() is const.
      char* bytes_writable = reinterpret_cast<char*>(&(*bytes)[0]);
      memcpy(bytes_writable + pos, chunk.data(), chunk.size());
      pos += chunk.size();
    }
  } else {
    // Size is known, read the file directly.
    bytes->resize(static_cast<size_t>(size));
    size_t pos = 0;
    while (pos < bytes->size()) {
      // Needed in case ContainerType is std::string, whose data() is const.
      char* bytes_writable = reinterpret_cast<char*>(&(*bytes)[0]);
      const size_t bytes_read =
          fread(bytes_writable + pos, 1, bytes->size() - pos, f);
      if (bytes_read == 0) return JXL_FAILURE("Failed to read");
      pos += bytes_read;
    }
    JXL_ASSERT(pos == bytes->size());
  }
  return true;
}

template <typename ContainerType>
static inline Status WriteFile(const ContainerType& bytes,
                               const std::string& pathname) {
  FileWrapper f(pathname, "wb");
  if (f == nullptr)
    return JXL_FAILURE("Failed to open file for writing: %s", pathname.c_str());

  size_t pos = 0;
  while (pos < bytes.size()) {
    const size_t bytes_written =
        fwrite(bytes.data() + pos, 1, bytes.size() - pos, f);
    if (bytes_written == 0) return JXL_FAILURE("Failed to write");
    pos += bytes_written;
  }
  JXL_ASSERT(pos == bytes.size());

  return true;
}

}  // namespace jxl

#endif  // LIB_JXL_BASE_FILE_IO_H_
