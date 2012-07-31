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

static const size_t CHUNK = 16384;

/* Generate a seekable compressed stream. */

int main(int argc, char* argv[])
{
  if (argc != 3 || !argv[1] || !argv[2] || (strcmp(argv[1], argv[2]) == 0)) {
    log("usage: %s file_to_compress out_file", argv[0]);
    return 1;
  }

  AutoCloseFD origFd;
  origFd = open(argv[1], O_RDONLY);
  if (origFd == -1) {
    log("Couldn't open %s: %s", argv[1], strerror(errno));
    return 1;
  }

  struct stat st;
  int ret = fstat(origFd, &st);
  if (ret == -1) {
    log("Couldn't seek %s: %s", argv[1], strerror(errno));
    return 1;
  }

  size_t origSize = st.st_size;
  log("Size = %lu", origSize);
  if (origSize == 0) {
    log("Won't compress %s: it's empty", argv[1]);
    return 1;
  }

  /* Mmap the original file */
  MappedPtr origBuf;
  origBuf.Assign(mmap(NULL, origSize, PROT_READ, MAP_PRIVATE, origFd, 0), origSize);
  if (origBuf == MAP_FAILED) {
    log("Couldn't mmap %s: %s", argv[1], strerror(errno));
    return 1;
  }

  /* Create the compressed file */
  AutoCloseFD outFd;
  outFd = open(argv[2], O_RDWR | O_CREAT | O_TRUNC, 0666);
  if (outFd == -1) {
    log("Couldn't open %s: %s", argv[2], strerror(errno));
    return 1;
  }

  /* Expected total number of chunks */
  size_t nChunks = ((origSize + CHUNK - 1) / CHUNK);

  /* The first chunk is going to be stored after the header and the offset
   * table */
  size_t offset = sizeof(SeekableZStreamHeader) + nChunks * sizeof(uint32_t);

  /* Give enough room for the header and the offset table, and map them */
  ret = ftruncate(outFd, offset);
  MOZ_ASSERT(ret == 0);
  MappedPtr headerMap;
  headerMap.Assign(mmap(NULL, offset, PROT_READ | PROT_WRITE, MAP_SHARED,
                        outFd, 0), offset);
  if (headerMap == MAP_FAILED) {
    log("Couldn't mmap %s: %s", argv[1], strerror(errno));
    return 1;
  }

  SeekableZStreamHeader *header = new (headerMap) SeekableZStreamHeader;
  le_uint32 *entry = reinterpret_cast<le_uint32 *>(&header[1]);

  /* Initialize header */
  header->chunkSize = CHUNK;
  header->totalSize = offset;

  /* Seek at the end of the output file, where we're going to append
   * compressed streams */
  lseek(outFd, offset, SEEK_SET);

  /* Initialize zlib structure */
  z_stream zStream;
  memset(&zStream, 0, sizeof(zStream));

  /* Compression buffer */
  mozilla::ScopedDeleteArray<Bytef> outBuf;
  outBuf = new Bytef[CHUNK * 2];

  Bytef *origData = static_cast<Bytef*>(origBuf);
  size_t avail = 0;
  while (origSize) {
    avail = std::min(origSize, CHUNK);

    /* Compress chunk */
    ret = deflateInit(&zStream, 9);
    MOZ_ASSERT(ret == Z_OK);
    zStream.avail_in = avail;
    zStream.next_in = origData;
    zStream.avail_out = CHUNK * 2;
    zStream.next_out = outBuf;
    ret = deflate(&zStream, Z_FINISH);
    MOZ_ASSERT(ret == Z_STREAM_END);
    ret = deflateEnd(&zStream);
    MOZ_ASSERT(ret == Z_OK);
    MOZ_ASSERT(zStream.avail_out > 0);

    /* Write chunk */
    size_t len = write(outFd, outBuf, 2 * CHUNK - zStream.avail_out);
    MOZ_ASSERT(len == 2 * CHUNK - zStream.avail_out);

    /* Adjust headers */
    header->totalSize += len;
    *entry++ = offset;
    header->nChunks++;

    /* Prepare for next iteration */
    origSize -= avail;
    origData += avail;
    offset += len;
  }
  header->lastChunkSize = avail;

  MOZ_ASSERT(header->nChunks == nChunks);
  log("Compressed size is %lu", offset);

  return 0;
}
