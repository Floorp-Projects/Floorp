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
 * oggz_io.c
 *
 * Conrad Parker <conrad@annodex.net>
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "oggz_compat.h"
#include "oggz_private.h"

/*#define DEBUG*/

size_t
oggz_io_read (OGGZ * oggz, void * buf, size_t n)
{
  OggzIO * io;
  size_t bytes;

  if (oggz->file != NULL) {
    if ((bytes = fread (buf, 1, n, oggz->file)) == 0) {
      if (ferror (oggz->file)) {
        return (size_t) OGGZ_ERR_SYSTEM;
      }
    }
  }

  else if ((io = oggz->io) != NULL) {
    if (io->read == NULL) return (size_t) -1;
    bytes = io->read (io->read_user_handle, buf, n);
  }

  else return (size_t) OGGZ_ERR_INVALID;

  return bytes;
}

size_t
oggz_io_write (OGGZ * oggz, void * buf, size_t n)
{
  OggzIO * io;
  size_t bytes;

  if (oggz->file != NULL) {
    bytes = fwrite (buf, 1, n, oggz->file);
  }

  else if ((io = oggz->io) != NULL) {
    if (io->write == NULL) return (size_t) -1;
    bytes = io->write (io->write_user_handle, buf, n);
  }

  else {
    return (size_t) OGGZ_ERR_INVALID;
  }
    
  return bytes;
}

int
oggz_io_seek (OGGZ * oggz, long offset, int whence)
{
  OggzIO * io;

  if (oggz->file != NULL) {
    if (fseek (oggz->file, offset, whence) == -1) {
      if (errno == ESPIPE) {
	/*oggz_set_error (oggz, OGGZ_ERR_NOSEEK);*/
      } else {
	/*oggz_set_error (oggz, OGGZ_ERR_SYSTEM);*/
      }
      return OGGZ_ERR_SYSTEM;
    }
  }

  else if ((io = oggz->io) != NULL) {
    if (io->seek == NULL) return -1;
    if (io->seek (io->seek_user_handle, offset, whence) == -1)
      return -1;
  }

  else return OGGZ_ERR_INVALID;

  return 0;
}

long
oggz_io_tell (OGGZ * oggz)
{
  OggzIO * io;
  long offset;

  if (oggz->file != NULL) {
    if ((offset = ftell (oggz->file)) == -1) {
      if (errno == ESPIPE) {
	/*oggz_set_error (oggz, OGGZ_ERR_NOSEEK);*/
      } else {
	/*oggz_set_error (oggz, OGGZ_ERR_SYSTEM);*/
      }
      return -1;
    }
  }

  else if ((io = oggz->io) != NULL) {
    if (io->tell == NULL) return -1;
    if ((offset = io->tell (io->tell_user_handle)) == -1)
      return -1;
  }

  else return OGGZ_ERR_INVALID;

  return offset;
}

int
oggz_io_flush (OGGZ * oggz)
{
  OggzIO * io;

  if (oggz->file != NULL) {
    if (fflush (oggz->file) == EOF) {
      /* check errno for write-related errors */
      return OGGZ_ERR_SYSTEM;
    }
  }

  else if ((io = oggz->io) != NULL) {
    if (io->flush == NULL) return OGGZ_ERR_INVALID;

    if (io->flush (io->flush_user_handle) == -1) {
      return -1;
    }
  }

  else return OGGZ_ERR_INVALID;

  return 0;
}

/* get/set functions */

static int
oggz_io_init (OGGZ * oggz)
{
  oggz->io = (OggzIO *) oggz_malloc (sizeof (OggzIO));
  if (oggz->io == NULL) return -1;

  memset (oggz->io, 0, sizeof (OggzIO));

  return 0;
}

int
oggz_io_set_read (OGGZ * oggz, OggzIORead read, void * user_handle)
{
  if (oggz == NULL) return OGGZ_ERR_BAD_OGGZ;
  if (oggz->file != NULL) return OGGZ_ERR_INVALID;

  if (oggz->io == NULL) {
    if (oggz_io_init (oggz) == -1)
      return OGGZ_ERR_OUT_OF_MEMORY;
  }

  oggz->io->read = read;
  oggz->io->read_user_handle = user_handle;

  return 0;
}

void *
oggz_io_get_read_user_handle (OGGZ * oggz)
{
  if (oggz == NULL) return NULL;
  if (oggz->file != NULL) return NULL;

  if (oggz->io == NULL) return NULL;

  return oggz->io->read_user_handle;
}

int
oggz_io_set_write (OGGZ * oggz, OggzIOWrite write, void * user_handle)
{
  if (oggz == NULL) return OGGZ_ERR_BAD_OGGZ;
  if (oggz->file != NULL) return OGGZ_ERR_INVALID;

  if (oggz->io == NULL) {
    if (oggz_io_init (oggz) == -1)
      return OGGZ_ERR_OUT_OF_MEMORY;
  }

  oggz->io->write = write;
  oggz->io->write_user_handle = user_handle;

  return 0;
}

void *
oggz_io_get_write_user_handle (OGGZ * oggz)
{
  if (oggz == NULL) return NULL;
  if (oggz->file != NULL) return NULL;

  if (oggz->io == NULL) return NULL;

  return oggz->io->write_user_handle;
}

int
oggz_io_set_seek (OGGZ * oggz, OggzIOSeek seek, void * user_handle)
{
  if (oggz == NULL) return OGGZ_ERR_BAD_OGGZ;
  if (oggz->file != NULL) return OGGZ_ERR_INVALID;

  if (oggz->io == NULL) {
    if (oggz_io_init (oggz) == -1)
      return OGGZ_ERR_OUT_OF_MEMORY;
  }

  oggz->io->seek = seek;
  oggz->io->seek_user_handle = user_handle;

  return 0;
}

void *
oggz_io_get_seek_user_handle (OGGZ * oggz)
{
  if (oggz == NULL) return NULL;
  if (oggz->file != NULL) return NULL;

  if (oggz->io == NULL) return NULL;

  return oggz->io->seek_user_handle;
}

int
oggz_io_set_tell (OGGZ * oggz, OggzIOTell tell, void * user_handle)
{
  if (oggz == NULL) return OGGZ_ERR_BAD_OGGZ;
  if (oggz->file != NULL) return OGGZ_ERR_INVALID;

  if (oggz->io == NULL) {
    if (oggz_io_init (oggz) == -1)
      return OGGZ_ERR_OUT_OF_MEMORY;
  }

  oggz->io->tell = tell;
  oggz->io->tell_user_handle = user_handle;

  return 0;
}

void *
oggz_io_get_tell_user_handle (OGGZ * oggz)
{
  if (oggz == NULL) return NULL;
  if (oggz->file != NULL) return NULL;

  if (oggz->io == NULL) return NULL;

  return oggz->io->tell_user_handle;
}

int
oggz_io_set_flush (OGGZ * oggz, OggzIOFlush flush, void * user_handle)
{
  if (oggz == NULL) return OGGZ_ERR_BAD_OGGZ;
  if (oggz->file != NULL) return OGGZ_ERR_INVALID;

  if (oggz->io == NULL) {
    if (oggz_io_init (oggz) == -1)
      return OGGZ_ERR_OUT_OF_MEMORY;
  }

  oggz->io->flush = flush;
  oggz->io->flush_user_handle = user_handle;

  return 0;
}

void *
oggz_io_get_flush_user_handle (OGGZ * oggz)
{
  if (oggz == NULL) return NULL;
  if (oggz->file != NULL) return NULL;

  if (oggz->io == NULL) return NULL;

  return oggz->io->flush_user_handle;
}
