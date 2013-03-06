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

class FileBuffer: public MappedPtr
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

  bool Resize(size_t size)
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

/* Generate a seekable compressed stream. */

int main(int argc, char* argv[])
{
  if (argc != 3 || !argv[1] || !argv[2] || (strcmp(argv[1], argv[2]) == 0)) {
    log("usage: %s file_to_compress out_file", argv[0]);
    return 1;
  }

  FileBuffer origBuf;
  if (!origBuf.Init(argv[1])) {
    log("Couldn't open %s: %s", argv[1], strerror(errno));
    return 1;
  }

  struct stat st;
  int ret = fstat(origBuf.getFd(), &st);
  if (ret == -1) {
    log("Couldn't stat %s: %s", argv[1], strerror(errno));
    return 1;
  }

  size_t origSize = st.st_size;
  log("Size = %lu", origSize);
  if (origSize == 0) {
    log("Won't compress %s: it's empty", argv[1]);
    return 1;
  }

  /* Mmap the original file */
  if (!origBuf.Resize(origSize)) {
    log("Couldn't mmap %s: %s", argv[1], strerror(errno));
    return 1;
  }

  /* Create the compressed file */
  FileBuffer outBuf;
  if (!outBuf.Init(argv[2], true)) {
    log("Couldn't open %s: %s", argv[2], strerror(errno));
    return 1;
  }

  /* Expected total number of chunks */
  size_t nChunks = ((origSize + CHUNK - 1) / CHUNK);

  /* The first chunk is going to be stored after the header and the offset
   * table */
  size_t offset = sizeof(SeekableZStreamHeader) + nChunks * sizeof(uint32_t);

  /* Give enough room for the header and the offset table, and map them */
  if (!outBuf.Resize(origSize)) {
    log("Couldn't mmap %s: %s", argv[2], strerror(errno));
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
    ret = deflateInit(&zStream, 9);
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
    log("Error resizing %s: %s", argv[2], strerror(errno));
    return 1;
  }

  MOZ_ASSERT(header->nChunks == nChunks);
  log("Compressed size is %lu", offset);

  return 0;
}
