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
#include "Mappable.h"
#ifdef ANDROID
#include <linux/ashmem.h>
#endif
#include <sys/stat.h>
#include "ElfLoader.h"
#include "SeekableZStream.h"
#include "Logging.h"

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

#ifndef PAGE_MASK
#define PAGE_MASK (~ (PAGE_SIZE - 1))
#endif

Mappable *
MappableFile::Create(const char *path)
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

Mappable *
MappableExtractFile::Create(const char *name, Zip *zip, Zip::Stream *stream)
{
  const char *cachePath = getenv("MOZ_LINKER_CACHE");
  if (!cachePath || !*cachePath) {
    log("Warning: MOZ_LINKER_EXTRACT is set, but not MOZ_LINKER_CACHE; "
        "not extracting");
    return NULL;
  }
  mozilla::ScopedDeleteArray<char> path;
  path = new char[strlen(cachePath) + strlen(name) + 2];
  sprintf(path, "%s/%s", cachePath, name);
  struct stat cacheStat;
  if (stat(path, &cacheStat) == 0) {
    struct stat zipStat;
    stat(zip->GetName(), &zipStat);
    if (cacheStat.st_mtime > zipStat.st_mtime) {
      debug("Reusing %s", static_cast<char *>(path));
      return MappableFile::Create(path);
    }
  }
  debug("Extracting to %s", static_cast<char *>(path));
  AutoCloseFD fd;
  fd = open(path, O_TRUNC | O_RDWR | O_CREAT | O_NOATIME,
                  S_IRUSR | S_IWUSR);
  if (fd == -1) {
    log("Couldn't open %s to decompress library", path.get());
    return NULL;
  }
  AutoUnlinkFile file;
  file = path.forget();
  if (stream->GetType() == Zip::Stream::DEFLATE) {
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
  } else if (stream->GetType() == Zip::Stream::STORE) {
    SeekableZStream zStream;
    if (!zStream.Init(stream->GetBuffer())) {
      log("Couldn't initialize SeekableZStream for %s", name);
      return NULL;
    }
    if (ftruncate(fd, zStream.GetUncompressedSize()) == -1) {
      log("Couldn't ftruncate %s to decompress library", file.get());
      return NULL;
    }
    MappedPtr buffer(::mmap(NULL, zStream.GetUncompressedSize(), PROT_WRITE,
                            MAP_SHARED, fd, 0), zStream.GetUncompressedSize());
    if (buffer == MAP_FAILED) {
      log("Couldn't map %s to decompress library", file.get());
      return NULL;
    }

    if (!zStream.Decompress(buffer, 0, zStream.GetUncompressedSize())) {
      log("%s: failed to decompress", name);
      return NULL;
    }
  } else {
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
     * descriptor right away. To avoid problems with the crash reporter,
     * create an empty anonymous page after the ashmem mapping. To do so,
     * allocate one page more than requested, then replace the last page with
     * an anonymous mapping. */
    void *buf = ::mmap(NULL, length + PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (buf != MAP_FAILED) {
      ::mmap(reinterpret_cast<char *>(buf) + ((length + PAGE_SIZE - 1) & PAGE_MASK),
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
    ::munmap(*this + ((GetLength() + PAGE_SIZE - 1) & PAGE_MASK), PAGE_SIZE);
  }
#endif

private:
  _MappableBuffer(int fd, void *buf, size_t length)
  : MappedPtr(buf, length), fd(fd) { }

  /* File descriptor for the temporary file or ashmem */
  AutoCloseFD fd;
};


Mappable *
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
  /* Free zlib internal buffers */
  inflateEnd(&zStream);
  /* Free decompression buffer */
  buffer = NULL;
  /* Remove reference to Zip archive */
  zip = NULL;
}

Mappable *
MappableSeekableZStream::Create(const char *name, Zip *zip,
                                Zip::Stream *stream)
{
  MOZ_ASSERT(stream->GetType() == Zip::Stream::STORE);
  mozilla::ScopedDeletePtr<MappableSeekableZStream> mappable;
  mappable = new MappableSeekableZStream(zip);

  if (pthread_mutex_init(&mappable->mutex, NULL))
    return NULL;

  if (!mappable->zStream.Init(stream->GetBuffer()))
    return NULL;

  mappable->buffer = _MappableBuffer::Create(name,
                              mappable->zStream.GetUncompressedSize());
  if (!mappable->buffer)
    return NULL;

  mappable->chunkAvail = new unsigned char[mappable->zStream.GetChunksNum()];
  memset(mappable->chunkAvail, 0, mappable->zStream.GetChunksNum());

  return mappable.forget();
}

MappableSeekableZStream::MappableSeekableZStream(Zip *zip)
: zip(zip), chunkAvailNum(0) { }

MappableSeekableZStream::~MappableSeekableZStream()
{
  pthread_mutex_destroy(&mutex);
}

void *
MappableSeekableZStream::mmap(const void *addr, size_t length, int prot,
                              int flags, off_t offset)
{
  /* Map with PROT_NONE so that accessing the mapping would segfault, and
   * bring us to ensure() */
  void *res = buffer->mmap(addr, length, PROT_NONE, flags, offset);
  if (res == MAP_FAILED)
    return MAP_FAILED;

  /* Store the mapping, ordered by offset and length */
  std::vector<LazyMap>::reverse_iterator it;
  for (it = lazyMaps.rbegin(); it < lazyMaps.rend(); ++it) {
    if ((it->offset < offset) ||
        ((it->offset == offset) && (it->length < length)))
      break;
  }
  LazyMap map = { res, length, prot, offset };
  lazyMaps.insert(it.base(), map);
  return res;
}

void
MappableSeekableZStream::munmap(void *addr, size_t length)
{
  std::vector<LazyMap>::iterator it;
  for (it = lazyMaps.begin(); it < lazyMaps.end(); ++it)
    if ((it->addr = addr) && (it->length == length)) {
      lazyMaps.erase(it);
      ::munmap(addr, length);
      return;
    }
  MOZ_NOT_REACHED("munmap called with unknown mapping");
}

void
MappableSeekableZStream::finalize() { }

class AutoLock {
public:
  AutoLock(pthread_mutex_t *mutex): mutex(mutex)
  {
    if (pthread_mutex_lock(mutex))
      MOZ_NOT_REACHED("pthread_mutex_lock failed");
  }
  ~AutoLock()
  {
    if (pthread_mutex_unlock(mutex))
      MOZ_NOT_REACHED("pthread_mutex_unlock failed");
  }
private:
  pthread_mutex_t *mutex;
};

bool
MappableSeekableZStream::ensure(const void *addr)
{
  debug("ensure @%p", addr);
  void *addrPage = reinterpret_cast<void *>
                   (reinterpret_cast<uintptr_t>(addr) & PAGE_MASK);
  /* Find the mapping corresponding to the given page */
  std::vector<LazyMap>::iterator map;
  for (map = lazyMaps.begin(); map < lazyMaps.end(); ++map) {
    if (map->Contains(addrPage))
      break;
  }
  if (map == lazyMaps.end())
    return false;

  /* Find corresponding chunk */
  off_t mapOffset = map->offsetOf(addrPage);
  off_t chunk = mapOffset / zStream.GetChunkSize();

  /* In the typical case, we just need to decompress the chunk entirely. But
   * when the current mapping ends in the middle of the chunk, we want to
   * stop there. However, if another mapping needs the last part of the
   * chunk, we still need to continue. As mappings are ordered by offset
   * and length, we don't need to scan the entire list of mappings.
   * It is safe to run through lazyMaps here because the linker is never
   * going to call mmap (which adds lazyMaps) while this function is
   * called. */
  size_t length = zStream.GetChunkSize(chunk);
  off_t chunkStart = chunk * zStream.GetChunkSize();
  off_t chunkEnd = chunkStart + length;
  std::vector<LazyMap>::iterator it;
  for (it = map; it < lazyMaps.end(); ++it) {
    if (chunkEnd <= it->endOffset())
      break;
  }
  if ((it == lazyMaps.end()) || (chunkEnd > it->endOffset())) {
    /* The mapping "it" points at now is past the interesting one */
    --it;
    length = it->endOffset() - chunkStart;
  }

  AutoLock lock(&mutex);

  /* The very first page is mapped and accessed separately of the rest, and
   * as such, only the first page of the first chunk is decompressed this way.
   * When we fault in the remaining pages of that chunk, we want to decompress
   * the complete chunk again. Short of doing that, we would end up with
   * no data between PAGE_SIZE and chunkSize, which would effectively corrupt
   * symbol resolution in the underlying library. */
  if (chunkAvail[chunk] < (length + PAGE_SIZE - 1) / PAGE_SIZE) {
    if (!zStream.DecompressChunk(*buffer + chunkStart, chunk, length))
      return false;

#if defined(ANDROID) && defined(__arm__)
    if (map->prot & PROT_EXEC) {
      /* We just extracted data that may be executed in the future.
       * We thus need to ensure Instruction and Data cache coherency. */
      debug("cacheflush(%p, %p)", *buffer + chunkStart, *buffer + (chunkStart + length));
      cacheflush(reinterpret_cast<uintptr_t>(*buffer + chunkStart),
                 reinterpret_cast<uintptr_t>(*buffer + (chunkStart + length)), 0);
    }
#endif
    /* Only count if we haven't already decompressed parts of the chunk */
    if (chunkAvail[chunk] == 0)
      chunkAvailNum++;

    chunkAvail[chunk] = (length + PAGE_SIZE - 1) / PAGE_SIZE;
  }

  /* Flip the chunk mapping protection to the recorded flags. We could
   * also flip the protection for other mappings of the same chunk,
   * but it's easier to skip that and let further segfaults call
   * ensure again. */
  const void *chunkAddr = reinterpret_cast<const void *>
                          (reinterpret_cast<uintptr_t>(addrPage)
                           - mapOffset % zStream.GetChunkSize());
  const void *chunkEndAddr = reinterpret_cast<const void *>
                             (reinterpret_cast<uintptr_t>(chunkAddr) + length);
  
  const void *start = std::max(map->addr, chunkAddr);
  const void *end = std::min(map->end(), chunkEndAddr);
  length = reinterpret_cast<uintptr_t>(end)
           - reinterpret_cast<uintptr_t>(start);

  debug("mprotect @%p, 0x%" PRIxSize ", 0x%x", start, length, map->prot);
  if (mprotect(const_cast<void *>(start), length, map->prot) == 0)
    return true;

  log("mprotect failed");
  return false;
}

void
MappableSeekableZStream::stats(const char *when, const char *name) const
{
  size_t nEntries = zStream.GetChunksNum();
  debug("%s: %s; %" PRIdSize "/%" PRIdSize " chunks decompressed",
        name, when, chunkAvailNum, nEntries);

  size_t len = 64;
  mozilla::ScopedDeleteArray<char> map;
  map = new char[len + 3];
  map[0] = '[';

  for (size_t i = 0, j = 1; i < nEntries; i++, j++) {
    map[j] = chunkAvail[i] ? '*' : '_';
    if ((j == len) || (i == nEntries - 1)) {
      map[j + 1] = ']';
      map[j + 2] = '\0';
      debug("%s", static_cast<char *>(map));
      j = 0;
    }
  }
}
