/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include <map>
#include <sys/stat.h>
#include <string>
#include <sstream>
#include <cstring>
#include <cstdlib>
#include <zlib.h>
#include <fcntl.h>
#include <errno.h>
#include "mozilla/Assertions.h"
#include "mozilla/Scoped.h"
#include "mozilla/UniquePtr.h"
#include "SeekableZStream.h"
#include "Utils.h"
#include "Logging.h"

Logging Logging::Singleton;

const char *filterName[] = {
  "none",
  "thumb",
  "arm",
  "x86",
  "auto"
};

/* Maximum supported size for chunkSize */
static const size_t maxChunkSize =
  1 << (8 * std::min(sizeof(((SeekableZStreamHeader *)nullptr)->chunkSize),
                     sizeof(((SeekableZStreamHeader *)nullptr)->lastChunkSize)) - 1);

class Buffer: public MappedPtr
{
public:
  virtual ~Buffer() { }

  virtual bool Resize(size_t size)
  {
    MemoryRange buf = mmap(nullptr, size, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANON, -1, 0);
    if (buf == MAP_FAILED)
      return false;
    if (*this != MAP_FAILED)
      memcpy(buf, *this, std::min(size, GetLength()));
    Assign(buf);
    return true;
  }

  bool Fill(Buffer &other)
  {
    size_t size = other.GetLength();
    if (!size || !Resize(size))
      return false;
    memcpy(static_cast<void *>(*this), static_cast<void *>(other), size);
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
    Assign(MemoryRange::mmap(nullptr, size,
                             PROT_READ | (writable ? PROT_WRITE : 0),
                             writable ? MAP_SHARED : MAP_PRIVATE, fd, 0));
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

class FilteredBuffer: public Buffer
{
public:
  void Filter(Buffer &other, SeekableZStream::FilterId filter, size_t chunkSize)
  {
    SeekableZStream::ZStreamFilter filterCB =
      SeekableZStream::GetFilter(filter);
    MOZ_ASSERT(filterCB);
    Fill(other);
    size_t size = other.GetLength();
    Bytef *data = reinterpret_cast<Bytef *>(static_cast<void *>(*this));
    size_t avail = 0;
    /* Filter needs to be applied in chunks. */
    while (size) {
      avail = std::min(size, chunkSize);
      filterCB(data - static_cast<unsigned char *>(static_cast<void *>(*this)),
               SeekableZStream::FILTER, data, avail);
      size -= avail;
      data += avail;
    }
  }
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
    if (!size || !Resize(size))
      return;
    DEBUG_LOG("Creating dictionary");
    piece *origBufPieces = reinterpret_cast<piece *>(
                           static_cast<void *>(inBuf));
    std::map<piece, int> stats;
    for (unsigned int i = 0; i < inBuf.GetLength() / sizeof(piece); i++) {
      stats[origBufPieces[i]]++;
    }
    std::vector<stat_pair> statsVec(stats.begin(), stats.end());
    std::sort(statsVec.begin(), statsVec.end(), stat_cmp);

    piece *dictPieces = reinterpret_cast<piece *>(
                        static_cast<void *>(*this));
    typename std::vector<stat_pair>::reverse_iterator it = statsVec.rbegin();
    for (int i = size / sizeof(piece); i > 0 && it < statsVec.rend();
         i--, ++it) {
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
  , filter(aFilter)
  , dictSize(aDictSize)
  {}

  const static signed char winSizeLog = 15;
  const static size_t winSize = 1 << winSizeLog;

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

private:

  int do_compress(Buffer &origBuf, Buffer &outBuf, const unsigned char *aDict,
                  size_t aDictSize, SeekableZStream::FilterId aFilter);

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
    ERROR("%s is not compressed", name);
    return 0;
  }

  SeekableZStream zstream;
  if (!zstream.Init(origBuf, origSize))
    return 0;

  size_t size = zstream.GetUncompressedSize();

  /* Give enough room for the uncompressed data */
  if (!outBuf.Resize(size)) {
    ERROR("Error resizing %s: %s", outName, strerror(errno));
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
    ERROR("Won't compress %s: it's empty", name);
    return 1;
  }
  if (SeekableZStreamHeader::validate(origBuf)) {
    WARN("Skipping %s: it's already a szip", name);
    return 0;
  }
  bool compressed = false;
  LOG("Size = %" PRIuSize, origSize);

  /* Allocate a buffer the size of the uncompressed data: we don't want
   * a compressed file larger than that anyways. */
  if (!outBuf.Resize(origSize)) {
    ERROR("Couldn't allocate output buffer: %s", strerror(errno));
    return 1;
  }

  /* Find the most appropriate filter */
  SeekableZStream::FilterId firstFilter, lastFilter;
  bool scanFilters;
  if (filter == SeekableZStream::FILTER_MAX) {
    firstFilter = SeekableZStream::NONE;
    lastFilter = SeekableZStream::FILTER_MAX;
    scanFilters = true;
  } else {
    firstFilter = lastFilter = filter;
    ++lastFilter;
    scanFilters = false;
  }

  mozilla::UniquePtr<Buffer> filteredBuf;
  Buffer *origData;
  for (SeekableZStream::FilterId f = firstFilter; f < lastFilter; ++f) {
    mozilla::UniquePtr<FilteredBuffer> filteredTmp;
    Buffer tmpBuf;
    if (f != SeekableZStream::NONE) {
      DEBUG_LOG("Applying filter \"%s\"", filterName[f]);
      filteredTmp = mozilla::MakeUnique<FilteredBuffer>();
      filteredTmp->Filter(origBuf, f, chunkSize);
      origData = filteredTmp.get();
    } else {
      origData = &origBuf;
    }
    if (dictSize  && !scanFilters) {
      filteredBuf = mozilla::Move(filteredTmp);
      break;
    }
    DEBUG_LOG("Compressing with no dictionary");
    if (do_compress(*origData, tmpBuf, nullptr, 0, f) == 0) {
      if (tmpBuf.GetLength() < outBuf.GetLength()) {
        outBuf.Fill(tmpBuf);
        compressed = true;
        filter = f;
        filteredBuf = mozilla::Move(filteredTmp);
        continue;
      }
    }
  }

  origData = filteredBuf ? filteredBuf.get() : &origBuf;

  if (dictSize) {
    Dictionary<uint64_t> dict(*origData, dictSize ? SzipCompress::winSize : 0);

    /* Find the most appropriate dictionary size */
    size_t firstDictSize, lastDictSize;
    if (dictSize == (size_t) -1) {
      /* If we scanned for filters, we effectively already tried dictSize=0 */
      firstDictSize = scanFilters ? 4096 : 0;
      lastDictSize = SzipCompress::winSize;
    } else {
      firstDictSize = lastDictSize = dictSize;
    }

    Buffer tmpBuf;
    for (size_t d = firstDictSize; d <= lastDictSize; d += 4096) {
      DEBUG_LOG("Compressing with dictionary of size %" PRIuSize, d);
      if (do_compress(*origData, tmpBuf, static_cast<unsigned char *>(dict)
                      + SzipCompress::winSize - d, d, filter))
        continue;
      if (!compressed || tmpBuf.GetLength() < outBuf.GetLength()) {
        outBuf.Fill(tmpBuf);
        compressed = true;
        dictSize = d;
      }
    }
  }

  if (!compressed) {
    outBuf.Fill(origBuf);
    LOG("Not compressed");
    return 0;
  }

  if (dictSize == (size_t) -1)
    dictSize = 0;

  DEBUG_LOG("Used filter \"%s\" and dictionary size of %" PRIuSize,
            filterName[filter], dictSize);
  LOG("Compressed size is %" PRIuSize, outBuf.GetLength());

  /* Sanity check */
  Buffer tmpBuf;
  SzipDecompress decompress;
  if (decompress.run("buffer", outBuf, "buffer", tmpBuf))
    return 1;

  size_t size = tmpBuf.GetLength();
  if (size != origSize) {
    ERROR("Compression error: %" PRIuSize " != %" PRIuSize, size, origSize);
    return 1;
  }
  if (memcmp(static_cast<void *>(origBuf), static_cast<void *>(tmpBuf), size)) {
    ERROR("Compression error: content mismatch");
    return 1;
  }
  return 0;
}

int SzipCompress::do_compress(Buffer &origBuf, Buffer &outBuf,
                              const unsigned char *aDict, size_t aDictSize,
                              SeekableZStream::FilterId aFilter)
{
  size_t origSize = origBuf.GetLength();
  MOZ_ASSERT(origSize != 0);

  /* Expected total number of chunks */
  size_t nChunks = ((origSize + chunkSize - 1) / chunkSize);

  /* The first chunk is going to be stored after the header, the dictionary
   * and the offset table */
  size_t offset = sizeof(SeekableZStreamHeader) + aDictSize
                  + nChunks * sizeof(uint32_t);

  if (offset >= origSize)
    return 1;

    /* Allocate a buffer the size of the uncompressed data: we don't want
   * a compressed file larger than that anyways. */
  if (!outBuf.Resize(origSize)) {
    ERROR("Couldn't allocate output buffer: %s", strerror(errno));
    return 1;
  }

  SeekableZStreamHeader *header = new (outBuf) SeekableZStreamHeader;
  unsigned char *dictionary = static_cast<unsigned char *>(
                              outBuf + sizeof(SeekableZStreamHeader));
  le_uint32 *entry =
    reinterpret_cast<le_uint32 *>(dictionary + aDictSize);

  /* Initialize header */
  header->chunkSize = chunkSize;
  header->dictSize = aDictSize;
  header->totalSize = offset;
  header->windowBits = -SzipCompress::winSizeLog; // Raw stream,
                                                  // window size of 32k.
  header->filter = aFilter;
  if (aDictSize)
    memcpy(dictionary, aDict, aDictSize);

  /* Initialize zlib structure */
  z_stream zStream;
  memset(&zStream, 0, sizeof(zStream));
  zStream.avail_out = origSize - offset;
  zStream.next_out = static_cast<Bytef*>(outBuf) + offset;

  size_t avail = 0;
  size_t size = origSize;
  unsigned char *data = reinterpret_cast<unsigned char *>(
                        static_cast<void *>(origBuf));
  while (size) {
    avail = std::min(size, chunkSize);

    /* Compress chunk */
    int ret = deflateInit2(&zStream, 9, Z_DEFLATED, header->windowBits,
                           MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
    if (aDictSize)
      deflateSetDictionary(&zStream, dictionary, aDictSize);
    MOZ_ASSERT(ret == Z_OK);
    zStream.avail_in = avail;
    zStream.next_in = data;
    ret = deflate(&zStream, Z_FINISH);
    /* Under normal conditions, deflate returns Z_STREAM_END. If there is not
     * enough room to compress, deflate returns Z_OK and avail_out is 0. We
     * still want to deflateEnd in that case, so fall through. It will bail
     * on the avail_out test that follows. */
    MOZ_ASSERT(ret == Z_STREAM_END || ret == Z_OK);
    ret = deflateEnd(&zStream);
    MOZ_ASSERT(ret == Z_OK);
    if (zStream.avail_out <= 0)
      return 1;

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
  MOZ_ASSERT(header->totalSize == offset);
  MOZ_ASSERT(header->nChunks == nChunks);

  if (!outBuf.Resize(offset)) {
    ERROR("Error truncating output: %s", strerror(errno));
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
  mozilla::UniquePtr<SzipAction> action;
  char **firstArg;
  bool compress = true;
  size_t chunkSize = 0;
  SeekableZStream::FilterId filter = SzipCompress::DEFAULT_FILTER;
  size_t dictSize = (size_t) 0;

  Logging::Init();

  for (firstArg = &argv[1]; argc > 2; argc--, firstArg++) {
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
        ERROR("Invalid chunk size");
        return 1;
      }
    } else if (strcmp(firstArg[0], "-f") == 0) {
      firstArg++;
      argc--;
      if (!firstArg[0])
        break;
      bool matched = false;
      for (unsigned int i = 0; i < sizeof(filterName) / sizeof(char *); ++i) {
        if (strcmp(firstArg[0], filterName[i]) == 0) {
          filter = static_cast<SeekableZStream::FilterId>(i);
          matched = true;
          break;
        }
      }
      if (!matched) {
        ERROR("Invalid filter");
        return 1;
      }
    } else if (strcmp(firstArg[0], "-D") == 0) {
      firstArg++;
      argc--;
      if (!firstArg[0])
        break;
      if (strcmp(firstArg[0], "auto") == 0) {
        dictSize = -1;
      } else if (!GetSize(firstArg[0], &dictSize) || (dictSize >= 1 << 16)) {
        ERROR("Invalid dictionary size");
        return 1;
      }
    }
  }

  if (argc != 2 || !firstArg[0]) {
    LOG("usage: %s [-d] [-c CHUNKSIZE] [-f FILTER] [-D DICTSIZE] file",
        argv[0]);
    return 1;
  }

  if (compress) {
    action.reset(new SzipCompress(chunkSize, filter, dictSize));
  } else {
    if (chunkSize) {
      ERROR("-c is incompatible with -d");
      return 1;
    }
    if (dictSize) {
      ERROR("-D is incompatible with -d");
      return 1;
    }
    action.reset(new SzipDecompress());
  }

  std::stringstream tmpOutStream;
  tmpOutStream << firstArg[0] << ".sz." << getpid();
  std::string tmpOut(tmpOutStream.str());
  int ret;
  struct stat st;
  {
    FileBuffer origBuf;
    if (!origBuf.Init(firstArg[0])) {
      ERROR("Couldn't open %s: %s", firstArg[0], strerror(errno));
      return 1;
    }

    ret = fstat(origBuf.getFd(), &st);
    if (ret == -1) {
      ERROR("Couldn't stat %s: %s", firstArg[0], strerror(errno));
      return 1;
    }

    size_t origSize = st.st_size;

    /* Mmap the original file */
    if (!origBuf.Resize(origSize)) {
      ERROR("Couldn't mmap %s: %s", firstArg[0], strerror(errno));
      return 1;
    }

    /* Create the compressed file */
    FileBuffer outBuf;
    if (!outBuf.Init(tmpOut.c_str(), true)) {
      ERROR("Couldn't open %s: %s", tmpOut.c_str(), strerror(errno));
      return 1;
    }

    ret = action->run(firstArg[0], origBuf, tmpOut.c_str(), outBuf);
    if ((ret == 0) && (fstat(outBuf.getFd(), &st) == -1)) {
      st.st_size = 0;
    }
  }

  if ((ret == 0) && st.st_size) {
    rename(tmpOut.c_str(), firstArg[0]);
  } else {
    unlink(tmpOut.c_str());
  }
  return ret;
}
