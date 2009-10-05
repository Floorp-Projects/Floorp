/*
   Copyright (C) 2003 Commonwealth Scientific and Industrial Research
   Organisation (CSIRO) Australia

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   - Neither the name of CSIRO Australia nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
   PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE ORGANISATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/** @file
 * oggplay_reader.h
 * 
 * @authors
 * Shane Stephens <shane.stephens@annodex.net>
 * Michael Martin
 */

#ifndef __OGGPLAY_READER_H__
#define __OGGPLAY_READER_H__

#include <stdlib.h>
#include <oggz/oggz.h>
#include <ogg/ogg.h>

struct _OggPlayReader;

/** */
typedef struct _OggPlayReader {
  OggPlayErrorCode  (*initialise) (struct _OggPlayReader * me, int block);
  OggPlayErrorCode  (*destroy)    (struct _OggPlayReader * me);
  OggPlayErrorCode  (*seek)       (struct _OggPlayReader *me, OGGZ *oggz, 
                                   ogg_int64_t milliseconds);
  int               (*available)  (struct _OggPlayReader *me,
                                   ogg_int64_t current_bytes,
                                   ogg_int64_t current_time);
  ogg_int64_t       (*duration)   (struct _OggPlayReader *me);
  int               (*finished_retrieving)(struct _OggPlayReader *me);

  /* low-level io functions for oggz */
  size_t            (*io_read)(void *user_handle, void *buf, size_t n);
  int               (*io_seek)(void *user_handle, long offset, int whence);
  long              (*io_tell)(void *user_handle);
} OggPlayReader;

/**
 * Create and initialise an OggPlayReader for a given Ogg file.
 * 
 * @param filename The file to open
 * @return A new OggPlayReader handle
 * @retval NULL if error occured.
 */
OggPlayReader *
oggplay_file_reader_new(const char *filename);

/**
 * Create and initialise an OggPlayReader for an Ogg content at a given URI. 
 *
 * @param uri The URI to the Ogg file.
 * @param proxy Proxy 
 * @param proxy_port Proxy port.
 * @return A new OggPlayReader handle
 * @retval NULL on error.
 */
OggPlayReader *
oggplay_tcp_reader_new(const char *uri, const char *proxy, int proxy_port);

#endif
