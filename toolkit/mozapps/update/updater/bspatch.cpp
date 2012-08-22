/*-
 * Copyright 2003,2004 Colin Percival
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Changelog:
 * 2005-04-26 - Define the header as a C structure, add a CRC32 checksum to
 *              the header, and make all the types 32-bit.
 *                --Benjamin Smedberg <benjamin@smedbergs.us>
 */

#include "bspatch.h"
#include "errors.h"

#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <limits.h>

#if defined(XP_WIN)
# include <io.h>
#else
# include <unistd.h>
#endif

#ifdef XP_WIN
# include <winsock2.h>
#else
# include <arpa/inet.h>
#endif

#ifndef SSIZE_MAX
# define SSIZE_MAX LONG_MAX
#endif

int
MBS_ReadHeader(FILE* file, MBSPatchHeader *header)
{
  size_t s = fread(header, 1, sizeof(MBSPatchHeader), file);
  if (s != sizeof(MBSPatchHeader))
    return READ_ERROR;

  header->slen      = ntohl(header->slen);
  header->scrc32    = ntohl(header->scrc32);
  header->dlen      = ntohl(header->dlen);
  header->cblen     = ntohl(header->cblen);
  header->difflen   = ntohl(header->difflen);
  header->extralen  = ntohl(header->extralen);

  struct stat hs;
  s = fstat(fileno(file), &hs);
  if (s)
    return READ_ERROR;

  if (memcmp(header->tag, "MBDIFF10", 8) != 0)
    return UNEXPECTED_BSPATCH_ERROR;

  if (sizeof(MBSPatchHeader) +
      header->cblen +
      header->difflen +
      header->extralen != uint32_t(hs.st_size))
    return UNEXPECTED_BSPATCH_ERROR;

  return OK;
}
         
int
MBS_ApplyPatch(const MBSPatchHeader *header, FILE* patchFile,
               unsigned char *fbuffer, FILE* file)
{
  unsigned char *fbufend = fbuffer + header->slen;

  unsigned char *buf = (unsigned char*) malloc(header->cblen +
                                               header->difflen +
                                               header->extralen);
  if (!buf)
    return BSPATCH_MEM_ERROR;

  int rv = OK;

  size_t r = header->cblen + header->difflen + header->extralen;
  unsigned char *wb = buf;
  while (r) {
    const size_t count = (r > SSIZE_MAX) ? SSIZE_MAX : r;
    size_t c = fread(wb, 1, count, patchFile);
    if (c != count) {
      rv = READ_ERROR;
      goto end;
    }

    r -= c;
    wb += c;
  }

  {
    MBSPatchTriple *ctrlsrc = (MBSPatchTriple*) buf;
    unsigned char *diffsrc = buf + header->cblen;
    unsigned char *extrasrc = diffsrc + header->difflen;

    MBSPatchTriple *ctrlend = (MBSPatchTriple*) diffsrc;
    unsigned char *diffend = extrasrc;
    unsigned char *extraend = extrasrc + header->extralen;

    do {
      ctrlsrc->x = ntohl(ctrlsrc->x);
      ctrlsrc->y = ntohl(ctrlsrc->y);
      ctrlsrc->z = ntohl(ctrlsrc->z);

#ifdef DEBUG_bsmedberg
      printf("Applying block:\n"
             " x: %u\n"
             " y: %u\n"
             " z: %i\n",
             ctrlsrc->x,
             ctrlsrc->y,
             ctrlsrc->z);
#endif

      /* Add x bytes from oldfile to x bytes from the diff block */

      if (fbuffer + ctrlsrc->x > fbufend ||
          diffsrc + ctrlsrc->x > diffend) {
        rv = UNEXPECTED_BSPATCH_ERROR;
        goto end;
      }
      for (uint32_t i = 0; i < ctrlsrc->x; ++i) {
        diffsrc[i] += fbuffer[i];
      }
      if ((uint32_t) fwrite(diffsrc, 1, ctrlsrc->x, file) != ctrlsrc->x) {
        rv = WRITE_ERROR;
        goto end;
      }
      fbuffer += ctrlsrc->x;
      diffsrc += ctrlsrc->x;

      /* Copy y bytes from the extra block */

      if (extrasrc + ctrlsrc->y > extraend) {
        rv = UNEXPECTED_BSPATCH_ERROR;
        goto end;
      }
      if ((uint32_t) fwrite(extrasrc, 1, ctrlsrc->y, file) != ctrlsrc->y) {
        rv = WRITE_ERROR;
        goto end;
      }
      extrasrc += ctrlsrc->y;

      /* "seek" forwards in oldfile by z bytes */

      if (fbuffer + ctrlsrc->z > fbufend) {
        rv = UNEXPECTED_BSPATCH_ERROR;
        goto end;
      }
      fbuffer += ctrlsrc->z;

      /* and on to the next control block */

      ++ctrlsrc;
    } while (ctrlsrc < ctrlend);
  }

end:
  free(buf);
  return rv;
}
