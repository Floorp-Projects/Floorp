// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "mmap.h"

#include <cstdint>
#include <memory>

#include "lib/jxl/base/common.h"

#if __unix__
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace jxl {

struct MemoryMappedFileImpl {
  static StatusOr<std::unique_ptr<MemoryMappedFileImpl>> Init(
      const char* path) {
    auto f = make_unique<MemoryMappedFileImpl>();
    f->fd = open(path, O_RDONLY);
    if (f->fd == -1) {
      return JXL_FAILURE("Cannot open file %s", path);
    }
    f->mmap_len = lseek(f->fd, 0, SEEK_END);
    lseek(f->fd, 0, SEEK_SET);

    f->ptr = mmap(nullptr, f->mmap_len, PROT_READ, MAP_SHARED, f->fd, 0);
    if (f->ptr == MAP_FAILED) {
      return JXL_FAILURE("mmap failure");
    }
    return f;
  }

  const uint8_t* data() const { return reinterpret_cast<const uint8_t*>(ptr); }
  size_t size() const { return mmap_len; }

  ~MemoryMappedFileImpl() {
    if (fd != -1) {
      close(fd);
    }
    if (ptr != nullptr) {
      munmap(ptr, mmap_len);
    }
  }

  int fd = -1;
  size_t mmap_len = 0;
  void* ptr = nullptr;
};

}  // namespace jxl

#elif __WIN32__
#include <string.h>
#include <windows.h>

namespace {

struct HandleDeleter {
  void operator()(const HANDLE handle) const {
    if (handle != INVALID_HANDLE_VALUE) {
      CloseHandle(handle);
    }
  }
};
using HandleUniquePtr =
    std::unique_ptr<std::remove_pointer<HANDLE>::type, HandleDeleter>;

}  // namespace

namespace jxl {

struct MemoryMappedFileImpl {
  static StatusOr<std::unique_ptr<MemoryMappedFileImpl>> Init(
      const char* path) {
    auto f = make_unique<MemoryMappedFileImpl>();
    std::wstring stemp = std::wstring(path, path + strlen(path));
    f->handle.reset(CreateFileW(stemp.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                nullptr, OPEN_EXISTING,
                                FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
    if (f->handle.get() == INVALID_HANDLE_VALUE) {
      return JXL_FAILURE("Cannot open file %s", path);
    }
    if (!GetFileSizeEx(f->handle.get(), &f->fsize)) {
      return JXL_FAILURE("Cannot get file size (%s)", path);
    }
    f->handle_mapping.reset(CreateFileMappingW(f->handle.get(), nullptr,
                                               PAGE_READONLY, 0, 0, nullptr));
    if (f->handle_mapping == nullptr) {
      return JXL_FAILURE("Cannot create memory mapping (%s)", path);
    }
    f->ptr = MapViewOfFile(f->handle_mapping.get(), FILE_MAP_READ, 0, 0, 0);
    return f;
  }

  const uint8_t* data() const { return reinterpret_cast<const uint8_t*>(ptr); }
  size_t size() const { return fsize.QuadPart; }

  HandleUniquePtr handle;
  HandleUniquePtr handle_mapping;
  LARGE_INTEGER fsize;
  void* ptr = nullptr;
};

}  // namespace jxl

#else

namespace jxl {

struct MemoryMappedFileImpl {
  static StatusOr<std::unique_ptr<MemoryMappedFileImpl>> Init(
      const char* path) {
    return JXL_FAILURE("Memory mapping not supported on this system");
  }

  const uint8_t* data() const { return nullptr; }
  size_t size() const { return 0; }
};

}  // namespace jxl

#endif

namespace jxl {

StatusOr<MemoryMappedFile> MemoryMappedFile::Init(const char* path) {
  JXL_ASSIGN_OR_RETURN(auto mmf, MemoryMappedFileImpl::Init(path));
  MemoryMappedFile ret;
  ret.impl_ = std::move(mmf);
  return ret;
}

MemoryMappedFile::MemoryMappedFile() = default;
MemoryMappedFile::~MemoryMappedFile() = default;
MemoryMappedFile::MemoryMappedFile(MemoryMappedFile&&) noexcept = default;
MemoryMappedFile& MemoryMappedFile::operator=(MemoryMappedFile&&) noexcept =
    default;

const uint8_t* MemoryMappedFile::data() const { return impl_->data(); }
size_t MemoryMappedFile::size() const { return impl_->size(); }
}  // namespace jxl
