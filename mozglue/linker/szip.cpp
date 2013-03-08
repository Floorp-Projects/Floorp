/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include <map>
#include <sys/stat.h>
#include <cstring>
#include <cstdlib>
#include <zlib.h>
#include <fcntl.h>
#include <errno.h>
#include "mozilla/Assertions.h"
#include "mozilla/Scoped.h"
#include "SeekableZStream.h"
#include "Utils.h"
#include "Logging.h"

/* Maximum supported size for chunkSize */
static const size_t maxChunkSize =
  1 << (8 * std::min(sizeof(((SeekableZStreamHeader *)NULL)->chunkSize),
                     sizeof(((SeekableZStreamHeader *)NULL)->lastChunkSize)) - 1);

class Buffer: public MappedPtr
{
public:
  virtual bool Resize(size_t size)
  {
    void *buf = mmap(NULL, size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANON, -1, 0);
    if (buf == MAP_FAILED)
      return false;
    if (*this != MAP_FAILED)
      memcpy(buf, *this, std::min(size, GetLength()));
    Assign(buf, size);
    return true;
  }
};

class FileBuffer: public Buffer
{
public:
  bool Init(const char *name, bool writable_ = false)
  {
    fd = open(name, writable_ ? O_RDWR | O_CREAT | O_TRUNC : O_RDONLY, 0666);
    if (fd == -1)
      return false;
    writable = writable_;
    return true;
  }

  virtual bool Resize(size_t size)
  {
    if (writable) {
      if (ftruncate(fd, size) == -1)
        return false;
    }
    Assign(mmap(NULL, size, PROT_READ | (writable ? PROT_WRITE : 0),
                writable ? MAP_SHARED : MAP_PRIVATE, fd, 0), size);
    return this != MAP_FAILED;
  }

  int getFd()
  {
    return fd;
  }

private:
  AutoCloseFD fd;
  bool writable;
};

template <typename T>
class Dictionary: public Buffer
{
  typedef T piece;
  typedef std::pair<piece, int> stat_pair;

  static bool stat_cmp(stat_pair a, stat_pair b)
  {
    return a.second < b.second;
  }

public:
  Dictionary(Buffer &inBuf, size_t size)
  {
    if (!Resize(size))
      return;
    piece *origBufPieces = reinterpret_cast<piece *>(
                           static_cast<void *>(inBuf));
    std::map<piece, int> stats;
    for (int i = 0; i < inBuf.GetLength() / sizeof(piece); i++) {
      stats[origBufPieces[i]]++;
    }
    std::vector<stat_pair> statsVec(stats.begin(), stats.end());
    std::sort(statsVec.begin(), statsVec.end(), stat_cmp);

    piece *dictPieces = reinterpret_cast<piece *>(
                        static_cast<void *>(*this));
    typename std::vector<stat_pair>::reverse_iterator it = statsVec.rbegin();
    for (int i = size / sizeof(piece); i > 0 && it < statsVec.rend(); i--, ++it) {
      dictPieces[i - 1] = it->first;
    }
  }
};

class SzipAction
{
public:
  virtual int run(const char *name, Buffer &origBuf,
                  const char *outName, Buffer &outBuf) = 0;

  virtual ~SzipAction() {}
};

class SzipDecompress: public SzipAction
{
public:
  int run(const char *name, Buffer &origBuf,
          const char *outName, Buffer &outBuf);
};


class SzipCompress: public SzipAction
{
public:
  int run(const char *name, Buffer &origBuf,
          const char *outName, Buffer &outBuf);

  SzipCompress(size_t aChunkSize, SeekableZStream::FilterId aFilter,
               size_t aDictSize)
  : chunkSize(aChunkSize ? aChunkSize : 16384)
  , filter(aFilter == SeekableZStream::FILTER_MAX ? DEFAULT_FILTER : aFilter)
  , dictSize(aDictSize != (size_t) -1 ? aDictSize : winSize)
  {}

  const static signed char winSizeLog = 15;
  const static size_t winSize = 1 << winSizeLog;

private:

  const static SeekableZStream::FilterId DEFAULT_FILTER =
#if defined(TARGET_THUMB)
    SeekableZStream::BCJ_THUMB;
#elif defined(TARGET_ARM)
    SeekableZStream::BCJ_ARM;
#elif defined(TARGET_X86)
    SeekableZStream::BCJ_X86;
#else
    SeekableZStream::NONE;
#endif

  size_t chunkSize;
  SeekableZStream::FilterId filter;
  size_t dictSize;
};

/* Decompress a seekable compressed stream */
int SzipDecompress::run(const char *name, Buffer &origBuf,
                        const char *outName, Buffer &outBuf)
{
  size_t origSize = origBuf.GetLength();
  if (origSize < sizeof(SeekableZStreamHeader)) {
    log("%s is not a seekable zstream", name);
    return 1;
  }

  SeekableZStream zstream;
  if (!zstream.Init(origBuf, origSize))
    return 1;

  size_t size = zstream.GetUncompressedSize();

  /* Give enough room for the uncompressed data */
  if (!outBuf.Resize(size)) {
    log("Error resizing %s: %s", outName, strerror(errno));
    return 1;
  }

  if (!zstream.Decompress(outBuf, 0, size))
    return 1;

  return 0;
}

/* Generate a seekable compressed stream. */
int SzipCompress::run(const char *name, Buffer &origBuf,
                      const char *outName, Buffer &outBuf)
{
  size_t origSize = origBuf.GetLength();
  if (origSize == 0) {
    log("Won't compress %s: it's empty", name);
    return 1;
  }
  log("Size = %" PRIuSize, origSize);

  /* Expected total number of chunks */
  size_t nChunks = ((origSize + chunkSize - 1) / chunkSize);

  /* The first chunk is going to be stored after the header, the dictionary
   * and the offset table */
  size_t offset = sizeof(SeekableZStreamHeader) + dictSize
                  + nChunks * sizeof(uint32_t);

  /* Give enough room for the header, the dictionary, and the offset table,
   * and map them */
  if (!outBuf.Resize(origSize)) {
    log("Couldn't mmap %s: %s", outName, strerror(errno));
    return 1;
  }

  SeekableZStreamHeader *header = new (outBuf) SeekableZStreamHeader;
  unsigned char *dictionary = static_cast<unsigned char *>(
                              outBuf + sizeof(SeekableZStreamHeader));
  le_uint32 *entry =
    reinterpret_cast<le_uint32 *>(dictionary + dictSize);

  /* Initialize header */
  header->chunkSize = chunkSize;
  header->dictSize = dictSize;
  header->totalSize = offset;
  header->windowBits = -winSizeLog; // Raw stream, window size of 32k.
  header->filter = filter;

  /* Initialize zlib structure */
  z_stream zStream;
  memset(&zStream, 0, sizeof(zStream));
  zStream.avail_out = origSize - offset;
  zStream.next_out = static_cast<Bytef*>(outBuf) + offset;

  /* Filter buffer */
  SeekableZStream::ZStreamFilter filter =
    SeekableZStream::GetFilter(header->filter);
  Buffer filteredData;
  Buffer *origData = &origBuf;
  if (filter) {
    filteredData.Resize(origSize);
    origData = &filteredData;
    memcpy(static_cast<void *>(*origData), static_cast<void *>(origBuf), origSize);
    size_t size = origSize;
    Bytef *data = reinterpret_cast<Bytef *>(static_cast<void *>(*origData));
    size_t avail = 0;
    /* Filter needs to be applied in chunks. */
    while (size) {
      avail = std::min(size, chunkSize);
      filter(data - static_cast<unsigned char *>(static_cast<void *>(*origData)), SeekableZStream::FILTER, data, avail);
      size -= avail;
      data += avail;
    }
  }

  Dictionary<uint64_t> dict(*origData, dictSize);
  memcpy(dictionary, dict, dictSize);

  size_t avail = 0;
  size_t size = origSize;
  unsigned char *data = reinterpret_cast<unsigned char *>(static_cast<void *>(*origData));
  while (size) {
    avail = std::min(size, chunkSize);

    /* Compress chunk */
    int ret = deflateInit2(&zStream, 9, Z_DEFLATED, header->windowBits,
                           MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
    if (dictSize)
      deflateSetDictionary(&zStream, dictionary, dictSize);
    MOZ_ASSERT(ret == Z_OK);
    zStream.avail_in = avail;
    zStream.next_in = data;
    ret = deflate(&zStream, Z_FINISH);
    MOZ_ASSERT(ret == Z_STREAM_END);
    ret = deflateEnd(&zStream);
    MOZ_ASSERT(ret == Z_OK);
    MOZ_ASSERT(zStream.avail_out > 0);

    size_t len = origSize - offset - zStream.avail_out;

    /* Adjust headers */
    header->totalSize += len;
    *entry++ = offset;
    header->nChunks++;

    /* Prepare for next iteration */
    size -= avail;
    data += avail;
    offset += len;
  }
  header->lastChunkSize = avail;
  if (!outBuf.Resize(offset)) {
    log("Error resizing %s: %s", outName, strerror(errno));
    return 1;
  }

  MOZ_ASSERT(header->nChunks == nChunks);
  log("Compressed size is %" PRIuSize, offset);

  /* Sanity check */
  Buffer tmpBuf;
  SzipDecompress decompress;
  if (decompress.run("buffer", outBuf, "buffer", tmpBuf))
    return 1;

  size = tmpBuf.GetLength();
  if (size != origSize) {
    log("Compression error: %" PRIuSize " != %" PRIuSize, size, origSize);
    return 1;
  }
  if (memcmp(static_cast<void *>(origBuf), static_cast<void *>(tmpBuf), size)) {
    log("Compression error: content mismatch");
    return 1;
  }

  return 0;
}

bool GetSize(const char *str, size_t *out)
{
  char *end;
  MOZ_ASSERT(out);
  errno = 0;
  *out = strtol(str, &end, 10);
  return (!errno && !*end);
}

int main(int argc, char* argv[])
{
  mozilla::ScopedDeletePtr<SzipAction> action;
  char **firstArg;
  bool compress = true;
  size_t chunkSize = 0;
  SeekableZStream::FilterId filter = SeekableZStream::FILTER_MAX;
  size_t dictSize = (size_t) -1;

  for (firstArg = &argv[1]; argc > 3; argc--, firstArg++) {
    if (!firstArg[0] || firstArg[0][0] != '-')
      break;
    if (strcmp(firstArg[0], "-d") == 0) {
      compress = false;
    } else if (strcmp(firstArg[0], "-c") == 0) {
      firstArg++;
      argc--;
      if (!firstArg[0])
        break;
      if (!GetSize(firstArg[0], &chunkSize) || !chunkSize ||
          (chunkSize % 4096) || (chunkSize > maxChunkSize)) {
        log("Invalid chunk size");
        return 1;
      }
    } else if (strcmp(firstArg[0], "-f") == 0) {
      firstArg++;
      argc--;
      if (!firstArg[0])
        break;
      if (strcmp(firstArg[0], "arm") == 0)
        filter = SeekableZStream::BCJ_ARM;
      else if (strcmp(firstArg[0], "thumb") == 0)
        filter = SeekableZStream::BCJ_THUMB;
      else if (strcmp(firstArg[0], "x86") == 0)
        filter = SeekableZStream::BCJ_X86;
      else {
        log("Invalid filter");
        return 1;
      }
    } else if (strcmp(firstArg[0], "-D") == 0) {
      firstArg++;
      argc--;
      if (!firstArg[0])
        break;
      if (!GetSize(firstArg[0], &dictSize) || (dictSize >= 1 << 16)) {
        log("Invalid dictionary size");
        return 1;
      }
    }
  }

  if (argc != 3 || !firstArg[0] || !firstArg[1] ||
      (strcmp(firstArg[0], firstArg[1]) == 0)) {
    log("usage: %s [-d] [-c CHUNKSIZE] [-f FILTER] [-D DICTSIZE] "
        "in_file out_file", argv[0]);
    return 1;
  }

  if (compress) {
    action = new SzipCompress(chunkSize, filter, dictSize);
  } else {
    if (chunkSize) {
      log("-c is incompatible with -d");
      return 1;
    }
    if (dictSize != (size_t) -1) {
      log("-D is incompatible with -d");
      return 1;
    }
    action = new SzipDecompress();
  }

  FileBuffer origBuf;
  if (!origBuf.Init(firstArg[0])) {
    log("Couldn't open %s: %s", firstArg[0], strerror(errno));
    return 1;
  }

  struct stat st;
  int ret = fstat(origBuf.getFd(), &st);
  if (ret == -1) {
    log("Couldn't stat %s: %s", firstArg[0], strerror(errno));
    return 1;
  }

  size_t origSize = st.st_size;

  /* Mmap the original file */
  if (!origBuf.Resize(origSize)) {
    log("Couldn't mmap %s: %s", firstArg[0], strerror(errno));
    return 1;
  }

  /* Create the compressed file */
  FileBuffer outBuf;
  if (!outBuf.Init(firstArg[1], true)) {
    log("Couldn't open %s: %s", firstArg[1], strerror(errno));
    return 1;
  }

  ret = action->run(firstArg[0], origBuf, firstArg[1], outBuf);
  return ret;
}
