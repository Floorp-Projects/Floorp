/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "Mappable.h"

#include "mozilla/IntegerPrintfMacros.h"

#ifdef ANDROID
#  include "mozilla/Ashmem.h"
#endif
#include "Logging.h"

Mappable* MappableFile::Create(const char* path) {
  int fd = open(path, O_RDONLY);
  if (fd != -1) return new MappableFile(fd);
  return nullptr;
}

MemoryRange MappableFile::mmap(const void* addr, size_t length, int prot,
                               int flags, off_t offset) {
  MOZ_ASSERT(fd != -1);
  MOZ_ASSERT(!(flags & MAP_SHARED));
  flags |= MAP_PRIVATE;

  return MemoryRange::mmap(const_cast<void*>(addr), length, prot, flags, fd,
                           offset);
}

void MappableFile::finalize() {
  /* Close file ; equivalent to close(fd.forget()) */
  fd = -1;
}

size_t MappableFile::GetLength() const {
  struct stat st;
  return fstat(fd, &st) ? 0 : st.st_size;
}

/**
 * _MappableBuffer is a buffer which content can be mapped at different
 * locations in the virtual address space.
 * On Linux, uses a (deleted) temporary file on a tmpfs for sharable content.
 * On Android, uses ashmem.
 */
class _MappableBuffer : public MappedPtr {
 public:
  /**
   * Returns a _MappableBuffer instance with the given name and the given
   * length.
   */
  static _MappableBuffer* Create(const char* name, size_t length) {
    AutoCloseFD fd;
    const char* ident;
#ifdef ANDROID
    /* On Android, initialize an ashmem region with the given length */
    fd = mozilla::android::ashmem_create(name, length);
    ident = name;
#else
    /* On Linux, use /dev/shm as base directory for temporary files, assuming
     * it's on tmpfs */
    /* TODO: check that /dev/shm is tmpfs */
    char path[256];
    sprintf(path, "/dev/shm/%s.XXXXXX", name);
    fd = mkstemp(path);
    if (fd == -1) return nullptr;
    unlink(path);
    ftruncate(fd, length);
    ident = path;
#endif

    void* buf =
        ::mmap(nullptr, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (buf != MAP_FAILED) {
      DEBUG_LOG("Decompression buffer of size 0x%" PRIxPTR
                " in "
#ifdef ANDROID
                "ashmem "
#endif
                "\"%s\", mapped @%p",
                length, ident, buf);
      return new _MappableBuffer(fd.forget(), buf, length);
    }
    return nullptr;
  }

  void* mmap(const void* addr, size_t length, int prot, int flags,
             off_t offset) {
    MOZ_ASSERT(fd != -1);
#ifdef ANDROID
    /* Mapping ashmem MAP_PRIVATE is like mapping anonymous memory, even when
     * there is content in the ashmem */
    if (flags & MAP_PRIVATE) {
      flags &= ~MAP_PRIVATE;
      flags |= MAP_SHARED;
    }
#endif
    return ::mmap(const_cast<void*>(addr), length, prot, flags, fd, offset);
  }

 private:
  _MappableBuffer(int fd, void* buf, size_t length)
      : MappedPtr(buf, length), fd(fd) {}

  /* File descriptor for the temporary file or ashmem */
  AutoCloseFD fd;
};

Mappable* MappableDeflate::Create(const char* name, Zip* zip,
                                  Zip::Stream* stream) {
  MOZ_ASSERT(stream->GetType() == Zip::Stream::DEFLATE);
  _MappableBuffer* buf =
      _MappableBuffer::Create(name, stream->GetUncompressedSize());
  if (buf) return new MappableDeflate(buf, zip, stream);
  return nullptr;
}

MappableDeflate::MappableDeflate(_MappableBuffer* buf, Zip* zip,
                                 Zip::Stream* stream)
    : zip(zip), buffer(buf), zStream(stream->GetZStream(*buf)) {}

MappableDeflate::~MappableDeflate() {}

MemoryRange MappableDeflate::mmap(const void* addr, size_t length, int prot,
                                  int flags, off_t offset) {
  MOZ_ASSERT(buffer);
  MOZ_ASSERT(!(flags & MAP_SHARED));
  flags |= MAP_PRIVATE;

  /* The deflate stream is uncompressed up to the required offset + length, if
   * it hasn't previously been uncompressed */
  ssize_t missing = offset + length + zStream.avail_out - buffer->GetLength();
  if (missing > 0) {
    uInt avail_out = zStream.avail_out;
    zStream.avail_out = missing;
    if ((*buffer == zStream.next_out) &&
        (inflateInit2(&zStream, -MAX_WBITS) != Z_OK)) {
      ERROR("inflateInit failed: %s", zStream.msg);
      return MemoryRange(MAP_FAILED, 0);
    }
    int ret = inflate(&zStream, Z_SYNC_FLUSH);
    if (ret < 0) {
      ERROR("inflate failed: %s", zStream.msg);
      return MemoryRange(MAP_FAILED, 0);
    }
    if (ret == Z_NEED_DICT) {
      ERROR("zstream requires a dictionary. %s", zStream.msg);
      return MemoryRange(MAP_FAILED, 0);
    }
    zStream.avail_out = avail_out - missing + zStream.avail_out;
    if (ret == Z_STREAM_END) {
      if (inflateEnd(&zStream) != Z_OK) {
        ERROR("inflateEnd failed: %s", zStream.msg);
        return MemoryRange(MAP_FAILED, 0);
      }
      if (zStream.total_out != buffer->GetLength()) {
        ERROR("File not fully uncompressed! %ld / %d", zStream.total_out,
              static_cast<unsigned int>(buffer->GetLength()));
        return MemoryRange(MAP_FAILED, 0);
      }
    }
  }
#if defined(ANDROID) && defined(__arm__)
  if (prot & PROT_EXEC) {
    /* We just extracted data that may be executed in the future.
     * We thus need to ensure Instruction and Data cache coherency. */
    DEBUG_LOG("cacheflush(%p, %p)", *buffer + offset,
              *buffer + (offset + length));
    cacheflush(reinterpret_cast<uintptr_t>(*buffer + offset),
               reinterpret_cast<uintptr_t>(*buffer + (offset + length)), 0);
  }
#endif

  return MemoryRange(buffer->mmap(addr, length, prot, flags, offset), length);
}

void MappableDeflate::finalize() {
  /* Free zlib internal buffers */
  inflateEnd(&zStream);
  /* Free decompression buffer */
  buffer = nullptr;
  /* Remove reference to Zip archive */
  zip = nullptr;
}

size_t MappableDeflate::GetLength() const { return buffer->GetLength(); }
