/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include <sys/stat.h>
#include <cstring>
#include <zlib.h>
#include <fcntl.h>
#include <errno.h>
#include "mozilla/Assertions.h"
#include "mozilla/Scoped.h"
#include "SeekableZStream.h"
#include "Utils.h"
#include "Logging.h"

class Buffer: public MappedPtr
{
public:
  virtual bool Resize(size_t size)
  {
    void *buf = mmap(NULL, size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
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

static const size_t CHUNK = 16384;

/* Decompress a seekable compressed stream */
int do_decompress(const char *name, Buffer &origBuf,
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
int do_compress(const char *name, Buffer &origBuf,
                const char *outName, Buffer &outBuf)
{
  size_t origSize = origBuf.GetLength();
  if (origSize == 0) {
    log("Won't compress %s: it's empty", name);
    return 1;
  }
  log("Size = %" PRIuSize, origSize);

  /* Expected total number of chunks */
  size_t nChunks = ((origSize + CHUNK - 1) / CHUNK);

  /* The first chunk is going to be stored after the header and the offset
   * table */
  size_t offset = sizeof(SeekableZStreamHeader) + nChunks * sizeof(uint32_t);

  /* Give enough room for the header and the offset table, and map them */
  if (!outBuf.Resize(origSize)) {
    log("Couldn't mmap %s: %s", outName, strerror(errno));
    return 1;
  }

  SeekableZStreamHeader *header = new (outBuf) SeekableZStreamHeader;
  le_uint32 *entry = reinterpret_cast<le_uint32 *>(&header[1]);

  /* Initialize header */
  header->chunkSize = CHUNK;
  header->totalSize = offset;

  /* Initialize zlib structure */
  z_stream zStream;
  memset(&zStream, 0, sizeof(zStream));
  zStream.avail_out = origSize - offset;
  zStream.next_out = static_cast<Bytef*>(outBuf) + offset;

  Bytef *origData = static_cast<Bytef*>(origBuf);
  size_t avail = 0;
  size_t size = origSize;
  while (size) {
    avail = std::min(size, CHUNK);

    /* Compress chunk */
    int ret = deflateInit(&zStream, 9);
    MOZ_ASSERT(ret == Z_OK);
    zStream.avail_in = avail;
    zStream.next_in = origData;
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
    origData += avail;
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
  if (do_decompress("buffer", outBuf, "buffer", tmpBuf))
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

int main(int argc, char* argv[])
{
  int (*func)(const char *, Buffer &, const char *, Buffer &) = do_compress;
  char **firstArg = &argv[1];

  if ((argc > 1) && strcmp(argv[1], "-d") == 0) {
    func = do_decompress;
    firstArg++;
    argc--;
  }

  if (argc != 3 || !firstArg[0] || !firstArg[1] ||
      (strcmp(firstArg[0], firstArg[1]) == 0)) {
    log("usage: %s [-d] in_file out_file", argv[0]);
    return 1;
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

  return func(firstArg[0], origBuf, firstArg[1], outBuf);
}
