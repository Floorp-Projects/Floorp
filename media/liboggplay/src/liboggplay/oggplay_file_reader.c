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

/*
 * oggplay_file_reader.c
 *
 * Shane Stephens <shane.stephens@annodex.net>
 * Michael Martin
 */

#include "oggplay_private.h"
#include "oggplay_file_reader.h"

#include <stdlib.h>
#include <string.h>

OggPlayErrorCode
oggplay_file_reader_initialise(OggPlayReader * opr, int block) {

  OggPlayFileReader * me = (OggPlayFileReader *)opr;
  (void)block; /* unused for file readers */

  if (me == NULL) {
    return E_OGGPLAY_BAD_READER;
  }

  me->file = fopen(me->file_name, "rb");

  if (me->file == NULL) {
    return E_OGGPLAY_BAD_INPUT;
  }

  fseek(me->file, 0L, SEEK_END);
  me->size = ftell(me->file);
  fseek(me->file, 0L, SEEK_SET);

  me->current_position = 0;

  return E_OGGPLAY_OK;
}

OggPlayErrorCode
oggplay_file_reader_destroy(OggPlayReader * opr) {

  OggPlayFileReader * me;

  me = (OggPlayFileReader *)opr;

  fclose(me->file);
  oggplay_free(me);

  return E_OGGPLAY_OK;
}

int
oggplay_file_reader_available(OggPlayReader * opr, ogg_int64_t current_bytes,
    ogg_int64_t current_time) {

  OggPlayFileReader *me = (OggPlayFileReader *)opr;
  return me->size;

}

int
oggplay_file_reader_finished_retrieving(OggPlayReader *opr) {

  return 1;

}


static size_t
oggplay_file_reader_io_read(void * user_handle, void * buf, size_t n) {

  OggPlayFileReader *me = (OggPlayFileReader *)user_handle;
  int r;
  r = fread(buf, 1, n, me->file);
  if (r > 0) {
    me->current_position += r;
  }

  return r;
}

static int
oggplay_file_reader_io_seek(void * user_handle, long offset, int whence) {

  OggPlayFileReader * me = (OggPlayFileReader *)user_handle;
  int                 r;

  r = fseek(me->file, offset, whence);
  me->current_position = ftell(me->file);
  return r;

}

static long
oggplay_file_reader_io_tell(void * user_handle) {

  OggPlayFileReader * me = (OggPlayFileReader *)user_handle;

  return ftell(me->file);

}

OggPlayReader *
oggplay_file_reader_new(char *file_name) {

  OggPlayFileReader * me = oggplay_malloc (sizeof (OggPlayFileReader));

  if (me == NULL)
    return NULL;

  me->current_position = 0;
  me->file_name = file_name;
  me->file = NULL;

  me->functions.initialise = &oggplay_file_reader_initialise;
  me->functions.destroy = &oggplay_file_reader_destroy;
  me->functions.available = &oggplay_file_reader_available;
  me->functions.finished_retrieving = &oggplay_file_reader_finished_retrieving;
  me->functions.seek = NULL;
  me->functions.duration = NULL;
  me->functions.io_read = &oggplay_file_reader_io_read;
  me->functions.io_seek = &oggplay_file_reader_io_seek;
  me->functions.io_tell = &oggplay_file_reader_io_tell;
  me->functions.duration = NULL;

  return (OggPlayReader *)me;

}
