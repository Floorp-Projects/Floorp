/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <string>

#include "Mappable.h"

#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/UniquePtr.h"

#ifdef ANDROID
#  include <linux/ashmem.h>
#endif
#include <sys/stat.h>
#include <errno.h>
#include "ElfLoader.h"
#include "XZStream.h"
#include "Logging.h"

using mozilla::MakeUnique;
using mozilla::UniquePtr;

class CacheValidator {
 public:
  CacheValidator(const char* aCachedLibPath, Zip* aZip, Zip::Stream* aStream)
      : mCachedLibPath(aCachedLibPath) {
    static const char kChecksumSuffix[] = ".crc";

    mCachedChecksumPath =
        MakeUnique<char[]>(strlen(aCachedLibPath) + sizeof(kChecksumSuffix));
    sprintf(mCachedChecksumPath.get(), "%s%s", aCachedLibPath, kChecksumSuffix);
    DEBUG_LOG("mCachedChecksumPath: %s", mCachedChecksumPath.get());

    mChecksum = aStream->GetCRC32();
    DEBUG_LOG("mChecksum: %x", mChecksum);
  }

  // Returns whether the cache is valid and up-to-date.
  bool IsValid() const {
    // Validate based on checksum.
    RefPtr<Mappable> checksumMap =
        MappableFile::Create(mCachedChecksumPath.get());
    if (!checksumMap) {
      // Force caching if checksum is missing in cache.
      return false;
    }

    DEBUG_LOG("Comparing %x with %s", mChecksum, mCachedChecksumPath.get());
    MappedPtr checksumBuf = checksumMap->mmap(nullptr, checksumMap->GetLength(),
                                              PROT_READ, MAP_PRIVATE, 0);
    if (checksumBuf == MAP_FAILED) {
      WARN("Couldn't map %s to validate checksum", mCachedChecksumPath.get());
      return false;
    }
    if (memcmp(checksumBuf, &mChecksum, sizeof(mChecksum))) {
      return false;
    }
    return !access(mCachedLibPath.c_str(), R_OK);
  }

  // Caches the APK-provided checksum used in future cache validations.
  void CacheChecksum() const {
    AutoCloseFD fd(open(mCachedChecksumPath.get(),
                        O_TRUNC | O_RDWR | O_CREAT | O_NOATIME,
                        S_IRUSR | S_IWUSR));
    if (fd == -1) {
      WARN("Couldn't open %s to update checksum", mCachedChecksumPath.get());
      return;
    }

    DEBUG_LOG("Updating checksum %s", mCachedChecksumPath.get());

    const size_t size = sizeof(mChecksum);
    size_t written = 0;
    while (written < size) {
      ssize_t ret =
          write(fd, reinterpret_cast<const uint8_t*>(&mChecksum) + written,
                size - written);
      if (ret >= 0) {
        written += ret;
      } else if (errno != EINTR) {
        WARN("Writing checksum %s failed with errno %d",
             mCachedChecksumPath.get(), errno);
        break;
      }
    }
  }

 private:
  const std::string mCachedLibPath;
  UniquePtr<char[]> mCachedChecksumPath;
  uint32_t mChecksum;
};

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

Mappable* MappableExtractFile::Create(const char* name, Zip* zip,
                                      Zip::Stream* stream) {
  MOZ_ASSERT(zip && stream);

  const char* cachePath = getenv("MOZ_LINKER_CACHE");
  if (!cachePath || !*cachePath) {
    WARN(
        "MOZ_LINKER_EXTRACT is set, but not MOZ_LINKER_CACHE; "
        "not extracting");
    return nullptr;
  }

  // Ensure that the cache dir is private.
  chmod(cachePath, 0770);

  UniquePtr<char[]> path =
      MakeUnique<char[]>(strlen(cachePath) + strlen(name) + 2);
  sprintf(path.get(), "%s/%s", cachePath, name);

  CacheValidator validator(path.get(), zip, stream);
  if (validator.IsValid()) {
    DEBUG_LOG("Reusing %s", static_cast<char*>(path.get()));
    return MappableFile::Create(path.get());
  }
  DEBUG_LOG("Extracting to %s", static_cast<char*>(path.get()));
  AutoCloseFD fd;
  fd = open(path.get(), O_TRUNC | O_RDWR | O_CREAT | O_NOATIME,
            S_IRUSR | S_IWUSR);
  if (fd == -1) {
    ERROR("Couldn't open %s to decompress library", path.get());
    return nullptr;
  }
  AutoUnlinkFile file(path.release());
  if (stream->GetType() == Zip::Stream::DEFLATE) {
    if (ftruncate(fd, stream->GetUncompressedSize()) == -1) {
      ERROR("Couldn't ftruncate %s to decompress library", file.get());
      return nullptr;
    }
    /* Map the temporary file for use as inflate buffer */
    MappedPtr buffer(MemoryRange::mmap(nullptr, stream->GetUncompressedSize(),
                                       PROT_WRITE, MAP_SHARED, fd, 0));
    if (buffer == MAP_FAILED) {
      ERROR("Couldn't map %s to decompress library", file.get());
      return nullptr;
    }

    z_stream zStream = stream->GetZStream(buffer);

    /* Decompress */
    if (inflateInit2(&zStream, -MAX_WBITS) != Z_OK) {
      ERROR("inflateInit failed: %s", zStream.msg);
      return nullptr;
    }
    if (inflate(&zStream, Z_FINISH) != Z_STREAM_END) {
      ERROR("inflate failed: %s", zStream.msg);
      return nullptr;
    }
    if (inflateEnd(&zStream) != Z_OK) {
      ERROR("inflateEnd failed: %s", zStream.msg);
      return nullptr;
    }
    if (zStream.total_out != stream->GetUncompressedSize()) {
      ERROR("File not fully uncompressed! %ld / %d", zStream.total_out,
            static_cast<unsigned int>(stream->GetUncompressedSize()));
      return nullptr;
    }
  } else if (XZStream::IsXZ(stream->GetBuffer(), stream->GetSize())) {
    XZStream xzStream(stream->GetBuffer(), stream->GetSize());

    if (!xzStream.Init()) {
      ERROR("Couldn't initialize XZ decoder");
      return nullptr;
    }
    DEBUG_LOG("XZStream created, compressed=%" PRIuPTR
              ", uncompressed=%" PRIuPTR,
              xzStream.Size(), xzStream.UncompressedSize());

    if (ftruncate(fd, xzStream.UncompressedSize()) == -1) {
      ERROR("Couldn't ftruncate %s to decompress library", file.get());
      return nullptr;
    }
    MappedPtr buffer(MemoryRange::mmap(nullptr, xzStream.UncompressedSize(),
                                       PROT_WRITE, MAP_SHARED, fd, 0));
    if (buffer == MAP_FAILED) {
      ERROR("Couldn't map %s to decompress library", file.get());
      return nullptr;
    }
    const size_t written = xzStream.Decode(buffer, buffer.GetLength());
    DEBUG_LOG("XZStream decoded %" PRIuPTR, written);
    if (written != buffer.GetLength()) {
      ERROR("Error decoding XZ file %s", file.get());
      return nullptr;
    }
  } else {
    return nullptr;
  }

  validator.CacheChecksum();
  return new MappableExtractFile(fd.forget(), file.release());
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
#ifdef ANDROID
    /* On Android, initialize an ashmem region with the given length */
    fd = open("/" ASHMEM_NAME_DEF, O_RDWR, 0600);
    if (fd == -1) return nullptr;
    char str[ASHMEM_NAME_LEN];
    strlcpy(str, name, sizeof(str));
    ioctl(fd, ASHMEM_SET_NAME, str);
    if (ioctl(fd, ASHMEM_SET_SIZE, length)) return nullptr;

      /* The Gecko crash reporter is confused by adjacent memory mappings of
       * the same file and chances are we're going to map from the same file
       * descriptor right away. To avoid problems with the crash reporter,
       * create an empty anonymous page before or after the ashmem mapping,
       * depending on how mappings grow in the address space.
       */
#  if defined(__arm__)
    // Address increases on ARM.
    void* buf = ::mmap(nullptr, length + PAGE_SIZE, PROT_READ | PROT_WRITE,
                       MAP_SHARED, fd, 0);
    if (buf != MAP_FAILED) {
      ::mmap(AlignedEndPtr(reinterpret_cast<char*>(buf) + length, PAGE_SIZE),
             PAGE_SIZE, PROT_NONE, MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1,
             0);
      DEBUG_LOG("Decompression buffer of size 0x%" PRIxPTR
                " in ashmem \"%s\", mapped @%p",
                length, str, buf);
      return new _MappableBuffer(fd.forget(), buf, length);
    }
#  elif defined(__i386__) || defined(__x86_64__) || defined(__aarch64__)
    // Address decreases on x86, x86-64, and AArch64.
    size_t anon_mapping_length = length + PAGE_SIZE;
    void* buf = ::mmap(nullptr, anon_mapping_length, PROT_NONE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (buf != MAP_FAILED) {
      char* first_page = reinterpret_cast<char*>(buf);
      char* map_page = first_page + PAGE_SIZE;

      void* actual_buf = ::mmap(map_page, length, PROT_READ | PROT_WRITE,
                                MAP_FIXED | MAP_SHARED, fd, 0);
      if (actual_buf == MAP_FAILED) {
        ::munmap(buf, anon_mapping_length);
        DEBUG_LOG("Fixed allocation of decompression buffer at %p failed",
                  map_page);
        return nullptr;
      }

      DEBUG_LOG("Decompression buffer of size 0x%" PRIxPTR
                " in ashmem \"%s\", mapped @%p",
                length, str, actual_buf);
      return new _MappableBuffer(fd.forget(), actual_buf, length);
    }
#  else
#    error need to add a case for your CPU
#  endif
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

    void* buf =
        ::mmap(nullptr, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (buf != MAP_FAILED) {
      DEBUG_LOG("Decompression buffer of size %ld in \"%s\", mapped @%p",
                length, path, buf);
      return new _MappableBuffer(fd.forget(), buf, length);
    }
#endif
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

#ifdef ANDROID
  ~_MappableBuffer() {
    /* Free the additional page we allocated. See _MappableBuffer::Create */
#  if defined(__arm__)
    ::munmap(AlignedEndPtr(*this + GetLength(), PAGE_SIZE), PAGE_SIZE);
#  elif defined(__i386__) || defined(__x86_64__) || defined(__aarch64__)
    ::munmap(*this - PAGE_SIZE, GetLength() + PAGE_SIZE);
#  else
#    error need to add a case for your CPU
#  endif
  }
#endif

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
