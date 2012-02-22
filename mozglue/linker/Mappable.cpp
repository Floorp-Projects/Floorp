/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <cstring>
#include <cstdlib>
#include "Mappable.h"
#ifdef ANDROID
#include <linux/ashmem.h>
#endif
#include "ElfLoader.h"
#include "Logging.h"

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

#ifndef PAGE_MASK
#define PAGE_MASK (~ (PAGE_SIZE - 1))
#endif

MappableFile *MappableFile::Create(const char *path)
{
  int fd = open(path, O_RDONLY);
  if (fd != -1)
    return new MappableFile(fd);
  return NULL;
}

void *
MappableFile::mmap(const void *addr, size_t length, int prot, int flags,
                   off_t offset)
{
  MOZ_ASSERT(fd != -1);
  MOZ_ASSERT(!(flags & MAP_SHARED));
  flags |= MAP_PRIVATE;

  void *mapped = ::mmap(const_cast<void *>(addr), length, prot, flags,
                        fd, offset);
  if (mapped == MAP_FAILED)
    return mapped;

  /* Fill the remainder of the last page with zeroes when the requested
   * protection has write bits. */
  if ((mapped != MAP_FAILED) && (prot & PROT_WRITE) &&
      (length & (PAGE_SIZE - 1))) {
    memset(reinterpret_cast<char *>(mapped) + length, 0,
           PAGE_SIZE - (length & ~(PAGE_MASK)));
  }
  return mapped;
}

void
MappableFile::finalize()
{
  /* Close file ; equivalent to close(fd.forget()) */
  fd = -1;
}

MappableExtractFile *
MappableExtractFile::Create(const char *name, Zip::Stream *stream)
{
  const char *cachePath = getenv("MOZ_LINKER_CACHE");
  if (!cachePath || !*cachePath) {
    log("Warning: MOZ_LINKER_EXTRACT is set, but not MOZ_LINKER_CACHE; "
        "not extracting");
    return NULL;
  }
  AutoDeleteArray<char> path = new char[strlen(cachePath) + strlen(name) + 2];
  sprintf(path, "%s/%s", cachePath, name);
  debug("Extracting to %s", (char *)path);
  AutoCloseFD fd = open(path, O_TRUNC | O_RDWR | O_CREAT | O_NOATIME,
                              S_IRUSR | S_IWUSR);
  if (fd == -1) {
    log("Couldn't open %s to decompress library", path.get());
    return NULL;
  }
  AutoUnlinkFile file = path.forget();
  if (ftruncate(fd, stream->GetUncompressedSize()) == -1) {
    log("Couldn't ftruncate %s to decompress library", file.get());
    return NULL;
  }
  /* Map the temporary file for use as inflate buffer */
  MappedPtr buffer(::mmap(NULL, stream->GetUncompressedSize(), PROT_WRITE,
                          MAP_SHARED, fd, 0), stream->GetUncompressedSize());
  if (buffer == MAP_FAILED) {
    log("Couldn't map %s to decompress library", file.get());
    return NULL;
  }
  z_stream zStream = stream->GetZStream(buffer);

  /* Decompress */
  if (inflateInit2(&zStream, -MAX_WBITS) != Z_OK) {
    log("inflateInit failed: %s", zStream.msg);
    return NULL;
  }
  if (inflate(&zStream, Z_FINISH) != Z_STREAM_END) {
    log("inflate failed: %s", zStream.msg);
    return NULL;
  }
  if (inflateEnd(&zStream) != Z_OK) {
    log("inflateEnd failed: %s", zStream.msg);
    return NULL;
  }
  if (zStream.total_out != stream->GetUncompressedSize()) {
    log("File not fully uncompressed! %ld / %d", zStream.total_out,
        static_cast<unsigned int>(stream->GetUncompressedSize()));
    return NULL;
  }

  return new MappableExtractFile(fd.forget(), file.forget());
}

MappableExtractFile::~MappableExtractFile()
{
  /* When destroying from a forked process, we don't want the file to be
   * removed, as the main process is still using the file. Although it
   * doesn't really matter, it helps e.g. valgrind that the file is there.
   * The string still needs to be delete[]d, though */
  if (pid != getpid())
    delete [] path.forget();
}

/**
 * _MappableBuffer is a buffer which content can be mapped at different
 * locations in the virtual address space.
 * On Linux, uses a (deleted) temporary file on a tmpfs for sharable content.
 * On Android, uses ashmem.
 */
class _MappableBuffer: public MappedPtr
{
public:
  /**
   * Returns a _MappableBuffer instance with the given name and the given
   * length.
   */
  static _MappableBuffer *Create(const char *name, size_t length)
  {
    AutoCloseFD fd;
#ifdef ANDROID
    /* On Android, initialize an ashmem region with the given length */
    fd = open("/" ASHMEM_NAME_DEF, O_RDWR, 0600);
    if (fd == -1)
      return NULL;
    char str[ASHMEM_NAME_LEN];
    strlcpy(str, name, sizeof(str));
    ioctl(fd, ASHMEM_SET_NAME, str);
    if (ioctl(fd, ASHMEM_SET_SIZE, length))
      return NULL;

    /* The Gecko crash reporter is confused by adjacent memory mappings of
     * the same file. On Android, subsequent mappings are growing in memory
     * address, and chances are we're going to map from the same file
     * descriptor right away. Allocate one page more than requested so that
     * there is a gap between this mapping and the subsequent one. */
    void *buf = ::mmap(NULL, length + PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (buf != MAP_FAILED) {
      /* Actually create the gap with anonymous memory */
      ::mmap(reinterpret_cast<char *>(buf) + ((length + PAGE_SIZE) & PAGE_MASK),
             PAGE_SIZE, PROT_NONE, MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS,
             -1, 0);
      debug("Decompression buffer of size %d in ashmem \"%s\", mapped @%p",
            length, str, buf);
      return new _MappableBuffer(fd.forget(), buf, length);
    }
#else
    /* On Linux, use /dev/shm as base directory for temporary files, assuming
     * it's on tmpfs */
    /* TODO: check that /dev/shm is tmpfs */
    char path[256];
    sprintf(path, "/dev/shm/%s.XXXXXX", name);
    fd = mkstemp(path);
    if (fd == -1)
      return NULL;
    unlink(path);
    ftruncate(fd, length);

    void *buf = ::mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (buf != MAP_FAILED) {
      debug("Decompression buffer of size %ld in \"%s\", mapped @%p",
            length, path, buf);
      return new _MappableBuffer(fd.forget(), buf, length);
    }
#endif
    return NULL;
  }

  void *mmap(const void *addr, size_t length, int prot, int flags, off_t offset)
  {
    MOZ_ASSERT(fd != -1);
#ifdef ANDROID
    /* Mapping ashmem MAP_PRIVATE is like mapping anonymous memory, even when
     * there is content in the ashmem */
    if (flags & MAP_PRIVATE) {
      flags &= ~MAP_PRIVATE;
      flags |= MAP_SHARED;
    }
#endif
    return ::mmap(const_cast<void *>(addr), length, prot, flags, fd, offset);
  }

#ifdef ANDROID
  ~_MappableBuffer() {
    /* Free the additional page we allocated. See _MappableBuffer::Create */
    ::munmap(this + ((GetLength() + PAGE_SIZE) & ~(PAGE_SIZE - 1)), PAGE_SIZE);
  }
#endif

private:
  _MappableBuffer(int fd, void *buf, size_t length)
  : MappedPtr(buf, length), fd(fd) { }

  /* File descriptor for the temporary file or ashmem */
  AutoCloseFD fd;
};


MappableDeflate *
MappableDeflate::Create(const char *name, Zip *zip, Zip::Stream *stream)
{
  MOZ_ASSERT(stream->GetType() == Zip::Stream::DEFLATE);
  _MappableBuffer *buf = _MappableBuffer::Create(name, stream->GetUncompressedSize());
  if (buf)
    return new MappableDeflate(buf, zip, stream);
  return NULL;
}

MappableDeflate::MappableDeflate(_MappableBuffer *buf, Zip *zip,
                                 Zip::Stream *stream)
: zip(zip), buffer(buf), zStream(stream->GetZStream(*buf)) { }

MappableDeflate::~MappableDeflate() { }

void *
MappableDeflate::mmap(const void *addr, size_t length, int prot, int flags, off_t offset)
{
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
      log("inflateInit failed: %s", zStream.msg);
      return MAP_FAILED;
    }
    int ret = inflate(&zStream, Z_SYNC_FLUSH);
    if (ret < 0) {
      log("inflate failed: %s", zStream.msg);
      return MAP_FAILED;
    }
    if (ret == Z_NEED_DICT) {
      log("zstream requires a dictionary. %s", zStream.msg);
      return MAP_FAILED;
    }
    zStream.avail_out = avail_out - missing + zStream.avail_out;
    if (ret == Z_STREAM_END) {
      if (inflateEnd(&zStream) != Z_OK) {
        log("inflateEnd failed: %s", zStream.msg);
        return MAP_FAILED;
      }
      if (zStream.total_out != buffer->GetLength()) {
        log("File not fully uncompressed! %ld / %d", zStream.total_out,
            static_cast<unsigned int>(buffer->GetLength()));
        return MAP_FAILED;
      }
    }
  }
#if defined(ANDROID) && defined(__arm__)
  if (prot & PROT_EXEC) {
    /* We just extracted data that may be executed in the future.
     * We thus need to ensure Instruction and Data cache coherency. */
    debug("cacheflush(%p, %p)", *buffer + offset, *buffer + (offset + length));
    cacheflush(reinterpret_cast<uintptr_t>(*buffer + offset),
               reinterpret_cast<uintptr_t>(*buffer + (offset + length)), 0);
  }
#endif

  return buffer->mmap(addr, length, prot, flags, offset);
}

void
MappableDeflate::finalize()
{
  /* Free decompression buffer */
  buffer = NULL;
  /* Remove reference to Zip archive */
  zip = NULL;
}
