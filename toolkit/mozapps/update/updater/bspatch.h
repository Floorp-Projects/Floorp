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

#ifndef bspatch_h__
#define bspatch_h__

#include <stdint.h>
#include <stdio.h>

typedef struct MBSPatchHeader_ {
  /* "MBDIFF10" */
  char tag[8];

  /* Length of the file to be patched */
  uint32_t slen;

  /* CRC32 of the file to be patched */
  uint32_t scrc32;

  /* Length of the result file */
  uint32_t dlen;

  /* Length of the control block in bytes */
  uint32_t cblen;

  /* Length of the diff block in bytes */
  uint32_t difflen;

  /* Length of the extra block in bytes */
  uint32_t extralen;

  /* Control block (MBSPatchTriple[]) */
  /* Diff block (binary data) */
  /* Extra block (binary data) */
} MBSPatchHeader;

/**
 * Read the header of a patch file into the MBSPatchHeader structure.
 *
 * @param fd Must have been opened for reading, and be at the beginning
 *           of the file.
 */
int MBS_ReadHeader(FILE* file, MBSPatchHeader *header);

/**
 * Apply a patch. This method does not validate the checksum of the original
 * file: client code should validate the checksum before calling this method.
 *
 * @param patchfd Must have been processed by MBS_ReadHeader
 * @param fbuffer The original file read into a memory buffer of length
 *                header->slen.
 * @param filefd  Must have been opened for writing. Should be truncated
 *                to header->dlen if it is an existing file. The offset
 *                should be at the beginning of the file.
 */
int MBS_ApplyPatch(const MBSPatchHeader *header, FILE* patchFile,
                   unsigned char *fbuffer, FILE* file);

typedef struct MBSPatchTriple_ {
  uint32_t x; /* add x bytes from oldfile to x bytes from the diff block */
  uint32_t y; /* copy y bytes from the extra block */
  int32_t  z; /* seek forwards in oldfile by z bytes */
} MBSPatchTriple;

#endif  // bspatch_h__
