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

#include "config.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#include <ogg/ogg.h>

#include "oggz_compat.h"
#include "oggz_private.h"
#include "oggz_vector.h"

/* Definitions for oggz_run() */
long oggz_read (OGGZ * oggz, long n);
long oggz_write (OGGZ * oggz, long n);

/*#define DEBUG*/

static int
oggz_flags_disabled (int flags)
{
 if (flags & OGGZ_WRITE) {
   if (!OGGZ_CONFIG_WRITE) return OGGZ_ERR_DISABLED;
 } else {
   if (!OGGZ_CONFIG_READ) return OGGZ_ERR_DISABLED;
 }

 return 0;
}

OGGZ *
oggz_new (int flags)
{
  OGGZ * oggz;

  if (oggz_flags_disabled (flags)) return NULL;

  oggz = (OGGZ *) oggz_malloc (sizeof (OGGZ));
  if (oggz == NULL) return NULL;

  oggz->flags = flags;
  oggz->file = NULL;
  oggz->io = NULL;

  oggz->offset = 0;
  oggz->offset_data_begin = 0;

  oggz->run_blocksize = 1024;
  oggz->cb_next = 0;

  oggz->streams = oggz_vector_new ();
  if (oggz->streams == NULL) {
    goto err_oggz_new;
  }
  
  oggz->all_at_eos = 0;

  oggz->metric = NULL;
  oggz->metric_user_data = NULL;
  oggz->metric_internal = 0;

  oggz->order = NULL;
  oggz->order_user_data = NULL;

  oggz->packet_buffer = oggz_dlist_new ();
  if (oggz->packet_buffer == NULL) {
    goto err_streams_new;
  }

  if (OGGZ_CONFIG_WRITE && (oggz->flags & OGGZ_WRITE)) {
    if (oggz_write_init (oggz) == NULL)
      goto err_packet_buffer_new;
  } else if (OGGZ_CONFIG_READ) {
    oggz_read_init (oggz);
  }

  return oggz;

err_packet_buffer_new:
  oggz_free (oggz->packet_buffer);
err_streams_new:
  oggz_free (oggz->streams);
err_oggz_new:
  oggz_free (oggz);
  return NULL;
}

OGGZ *
oggz_open (char * filename, int flags)
{
  OGGZ * oggz = NULL;
  FILE * file = NULL;

  if (oggz_flags_disabled (flags)) return NULL;

  if (flags & OGGZ_WRITE) {
    file = fopen (filename, "wb");
  } else {
    file = fopen (filename, "rb");
  }
  if (file == NULL) return NULL;

  if ((oggz = oggz_new (flags)) == NULL) {
    fclose (file);
    return NULL;
  }

  oggz->file = file;

  return oggz;
}

OGGZ *
oggz_open_stdio (FILE * file, int flags)
{
  OGGZ * oggz = NULL;

  if (oggz_flags_disabled (flags)) return NULL;

  if ((oggz = oggz_new (flags)) == NULL)
    return NULL;

  oggz->file = file;

  return oggz;
}

int
oggz_flush (OGGZ * oggz)
{
  if (oggz == NULL) return OGGZ_ERR_BAD_OGGZ;

  if (OGGZ_CONFIG_WRITE && (oggz->flags & OGGZ_WRITE)) {
    oggz_write_flush (oggz);
  }

  return oggz_io_flush (oggz);
}

static int
oggz_stream_clear (void * data)
{
  oggz_stream_t * stream = (oggz_stream_t *) data;

  oggz_comments_free (stream);

  if (stream->ogg_stream.serialno != -1)
    ogg_stream_clear (&stream->ogg_stream);

  if (stream->metric_internal)
    oggz_free (stream->metric_user_data);

  if (stream->calculate_data != NULL)
    oggz_free (stream->calculate_data);
  
  oggz_free (stream);

  return 0;
}

int
oggz_close (OGGZ * oggz)
{
  if (oggz == NULL) return OGGZ_ERR_BAD_OGGZ;

  if (OGGZ_CONFIG_WRITE && (oggz->flags & OGGZ_WRITE)) {
    oggz_write_close (oggz);
  } else if (OGGZ_CONFIG_READ) {
    oggz_read_close (oggz);
  }

  oggz_vector_foreach (oggz->streams, oggz_stream_clear);
  oggz_vector_delete (oggz->streams);

  oggz_dlist_deliter(oggz->packet_buffer, oggz_read_free_pbuffers);
  oggz_dlist_delete(oggz->packet_buffer);
  
  if (oggz->metric_internal)
    oggz_free (oggz->metric_user_data);

  if (oggz->file != NULL) {
    if (fclose (oggz->file) == EOF) {
      return OGGZ_ERR_SYSTEM;
    }
  }

  if (oggz->io != NULL) {
    oggz_io_flush (oggz);
    oggz_free (oggz->io);
  }

  oggz_free (oggz);

  return 0;
}

oggz_off_t
oggz_tell (OGGZ * oggz)
{
  if (oggz == NULL) return OGGZ_ERR_BAD_OGGZ;

  return oggz->offset;
}

ogg_int64_t
oggz_tell_units (OGGZ * oggz)
{
  OggzReader * reader;

  if (oggz == NULL) return OGGZ_ERR_BAD_OGGZ;

  if (oggz->flags & OGGZ_WRITE) {
    return OGGZ_ERR_INVALID;
  }

  reader = &oggz->x.reader;

  if (OGGZ_CONFIG_READ) {
    return reader->current_unit;
  } else {
    return OGGZ_ERR_DISABLED;
  }
}

ogg_int64_t
oggz_tell_granulepos (OGGZ * oggz)
{
  if (oggz == NULL) return OGGZ_ERR_BAD_OGGZ;
  
  if (oggz->flags & OGGZ_WRITE) {
    return OGGZ_ERR_INVALID;
  }

  if (OGGZ_CONFIG_READ) {
    return oggz->x.reader.current_granulepos;
  } else {
    return OGGZ_ERR_DISABLED;
  }
}

long
oggz_run (OGGZ * oggz)
{
  long n = OGGZ_ERR_DISABLED;

  if (oggz == NULL) return OGGZ_ERR_BAD_OGGZ;

  if (OGGZ_CONFIG_WRITE && (oggz->flags & OGGZ_WRITE)) {
    while ((n = oggz_write (oggz, oggz->run_blocksize)) > 0);
  } else if (OGGZ_CONFIG_READ) {
    while ((n = oggz_read (oggz, oggz->run_blocksize)) > 0);
  }

  return n;
}

int
oggz_run_set_blocksize (OGGZ * oggz, long blocksize)
{
  if (oggz == NULL) return OGGZ_ERR_BAD_OGGZ;

  if (blocksize <= 0) return OGGZ_ERR_INVALID;

  oggz->run_blocksize = blocksize;

  return 0;
}

/******** oggz_stream management ********/

static int
oggz_find_stream (void * data, long serialno)
{
  oggz_stream_t * stream = (oggz_stream_t *) data;

  return (stream->ogg_stream.serialno == serialno);
}

oggz_stream_t *
oggz_get_stream (OGGZ * oggz, long serialno)
{
  if (serialno == -1) return NULL;

  return oggz_vector_find_with (oggz->streams, oggz_find_stream, serialno);
}

oggz_stream_t *
oggz_add_stream (OGGZ * oggz, long serialno)
{
  oggz_stream_t * stream;

  stream = oggz_malloc (sizeof (oggz_stream_t));
  if (stream == NULL) return NULL;

  ogg_stream_init (&stream->ogg_stream, (int)serialno);

  if (oggz_comments_init (stream) == -1) {
    oggz_free (stream);
    return NULL;
  }

  stream->content = OGGZ_CONTENT_UNKNOWN;
  stream->numheaders = 3; /* Default to 3 headers for Ogg logical bitstreams */
  stream->preroll = 0;
  stream->granulerate_n = 1;
  stream->granulerate_d = 1;
  stream->first_granule = 0;
  stream->basegranule = 0;
  stream->granuleshift = 0;

  stream->delivered_non_b_o_s = 0;
  stream->b_o_s = 1;
  stream->e_o_s = 0;
  stream->granulepos = 0;
  stream->packetno = -1; /* will be incremented on first read or write */

  stream->metric = NULL;
  stream->metric_user_data = NULL;
  stream->metric_internal = 0;
  stream->order = NULL;
  stream->order_user_data = NULL;
  stream->read_packet = NULL;
  stream->read_user_data = NULL;
  stream->read_page = NULL;
  stream->read_page_user_data = NULL;

  stream->calculate_data = NULL;
  
  oggz_vector_insert_p (oggz->streams, stream);

  return stream;
}

int
oggz_get_bos (OGGZ * oggz, long serialno)
{
  oggz_stream_t * stream;
  int i, size;

  if (oggz == NULL) return OGGZ_ERR_BAD_OGGZ;

  if (serialno == -1) {
    size = oggz_vector_size (oggz->streams);
    for (i = 0; i < size; i++) {
      stream = (oggz_stream_t *)oggz_vector_nth_p (oggz->streams, i);
#if 1
      /* If this stream has delivered a non bos packet, return FALSE */
      if (stream->delivered_non_b_o_s) return 0;
#else
      /* If this stream has delivered its bos packet, return FALSE */
      if (!stream->b_o_s) return 0;
#endif
    }
    return 1;
  } else {
    stream = oggz_get_stream (oggz, serialno);
    if (stream == NULL)
      return OGGZ_ERR_BAD_SERIALNO;

    return stream->b_o_s;
  }
}

int
oggz_get_eos (OGGZ * oggz, long serialno)
{
  oggz_stream_t * stream;
  int i, size;

  if (oggz == NULL) return OGGZ_ERR_BAD_OGGZ;

  if (serialno == -1) {
    size = oggz_vector_size (oggz->streams);
    for (i = 0; i < size; i++) {
      stream = (oggz_stream_t *)oggz_vector_nth_p (oggz->streams, i);
      if (!stream->e_o_s) return 0;
    }
    return 1;
  } else {
    stream = oggz_get_stream (oggz, serialno);
    if (stream == NULL)
      return OGGZ_ERR_BAD_SERIALNO;

    return stream->e_o_s;
  }
}

int
oggz_set_eos (OGGZ * oggz, long serialno)
{
  oggz_stream_t * stream;
  int i, size;

  if (oggz == NULL) return OGGZ_ERR_BAD_OGGZ;

  if (serialno == -1) {
    size = oggz_vector_size (oggz->streams);
    for (i = 0; i < size; i++) {
      stream = (oggz_stream_t *) oggz_vector_nth_p (oggz->streams, i);
      stream->e_o_s = 1;
    }
    oggz->all_at_eos = 1;
  } else {
    stream = oggz_get_stream (oggz, serialno);
    if (stream == NULL)
      return OGGZ_ERR_BAD_SERIALNO;

    stream->e_o_s = 1;

    if (oggz_get_eos (oggz, -1))
      oggz->all_at_eos = 1;
  }

  return 0;
}

int
oggz_get_numtracks (OGGZ * oggz)
{
  if (oggz == NULL) return OGGZ_ERR_BAD_OGGZ;
  return oggz_vector_size (oggz->streams);
}

/* Generate a pseudorandom serialno on request, ensuring that the number
 * selected is not -1 or the serialno of an existing logical bitstream.
 * NB. This inlines a simple linear congruential generator to avoid problems
 * of portability of rand() vs. the POSIX random()/initstate()/getstate(), and
 * in the case of rand() in order to avoid interfering with the random number
 * sequence.
 * Adapated from a patch by Erik de Castro Lopo, July 2007.
 */
long
oggz_serialno_new (OGGZ * oggz)
{
  /* Ensure that the returned value is within the range of an int, so that
   * it passes cleanly through ogg_stream_init(). In any case, the size of
   * a serialno in the Ogg page header is 32 bits; it should never have been
   * declared a long in ogg.h's ogg_packet in the first place. */
  static ogg_int32_t serialno = 0;
  int k;

  if (serialno == 0) serialno = time(NULL);

  do {
    for (k = 0; k < 3 || serialno == 0; k++)
      serialno = 11117 * serialno + 211231;
  } while (serialno == -1 || oggz_get_stream (oggz, serialno) != NULL);

  /* Cast the result back to a long for API consistency */
  return (long)serialno;
}

/******** OggzMetric management ********/

int
oggz_set_metric_internal (OGGZ * oggz, long serialno,
			  OggzMetric metric, void * user_data, int internal)
{
  oggz_stream_t * stream;

  if (oggz == NULL) return OGGZ_ERR_BAD_OGGZ;

  if (serialno == -1) {
    if (oggz->metric_internal && oggz->metric_user_data)
      oggz_free (oggz->metric_user_data);
    oggz->metric = metric;
    oggz->metric_user_data = user_data;
    oggz->metric_internal = internal;
  } else {
    stream = oggz_get_stream (oggz, serialno);
    if (stream == NULL) return OGGZ_ERR_BAD_SERIALNO;

    if (stream->metric_internal && stream->metric_user_data)
      oggz_free (stream->metric_user_data);
    stream->metric = metric;
    stream->metric_user_data = user_data;
    stream->metric_internal = internal;
  }

  return 0;
}

int
oggz_set_metric (OGGZ * oggz, long serialno,
		 OggzMetric metric, void * user_data)
{
  return oggz_set_metric_internal (oggz, serialno, metric, user_data, 0);
}

/*
 * Check if a stream in an oggz has a metric
 */
int
oggz_stream_has_metric (OGGZ * oggz, long serialno)
{
  oggz_stream_t * stream;

  if (oggz->metric != NULL) return 1;

  stream = oggz_get_stream (oggz, serialno);
  if (stream == NULL) return OGGZ_ERR_BAD_SERIALNO;

  if (stream->metric != NULL) return 1;

  return 0;
}

/*
 * Check if an oggz has metrics for all streams
 */
int
oggz_has_metrics (OGGZ * oggz)
{
  int i, size;
  oggz_stream_t * stream;

  if (oggz->metric != NULL) return 1;

  size = oggz_vector_size (oggz->streams);
  for (i = 0; i < size; i++) {
    stream = (oggz_stream_t *)oggz_vector_nth_p (oggz->streams, i);
    if (stream->metric == NULL) return 0;
  }

  return 1;
}

ogg_int64_t
oggz_get_unit (OGGZ * oggz, long serialno, ogg_int64_t granulepos)
{
  oggz_stream_t * stream;

  if (oggz == NULL) return OGGZ_ERR_BAD_OGGZ;

  if (granulepos == -1) return -1;

  if (serialno == -1) {
    if (oggz->metric)
      return oggz->metric (oggz, serialno, granulepos,
			   oggz->metric_user_data);
  } else {
    stream = oggz_get_stream (oggz, serialno);
    if (!stream) return -1;

    if (stream->metric) {
      return stream->metric (oggz, serialno, granulepos,
			     stream->metric_user_data);
    } else if (oggz->metric) {
      return oggz->metric (oggz, serialno, granulepos,
			   oggz->metric_user_data);
    }
  }

  return -1;
}

int
oggz_set_order (OGGZ * oggz, long serialno,
		OggzOrder order, void * user_data)
{
  oggz_stream_t * stream;

  if (oggz == NULL) return OGGZ_ERR_BAD_OGGZ;

  if (oggz->flags & OGGZ_WRITE) {
    return OGGZ_ERR_INVALID;
  }

  if (serialno == -1) {
    oggz->order = order;
    oggz->order_user_data = user_data;
  } else {
    stream = oggz_get_stream (oggz, serialno);
    if (stream == NULL) return OGGZ_ERR_BAD_SERIALNO;

    stream->order = order;
    stream->order_user_data = user_data;
  }

  return 0;
}

/* Map callback return values to error return values */
int
oggz_map_return_value_to_error (int cb_ret)
{
  switch (cb_ret) {
  case OGGZ_CONTINUE:
    return OGGZ_ERR_OK;
  case OGGZ_STOP_OK:
    return OGGZ_ERR_STOP_OK;
  case OGGZ_STOP_ERR:
    return OGGZ_ERR_STOP_ERR;
  default:
    return cb_ret;
  }
}

const char *
oggz_content_type (OggzStreamContent content)
{
  /* 20080805:
   * Re: http://lists.xiph.org/pipermail/ogg-dev/2008-July/001108.html
   *
   * "The ISO C standard, in section 6.7.2.2 "enumeration specifiers",
   * paragraph 4, says
   *
   *   Each enumerated type shall be compatible with *char*, a signed
   *   integer type, or an unsigned integer type.  The choice of type is
   *   implementation-defined, but shall be capable of representing the
   *   values of all the members of the declaration."
   *
   * -- http://gcc.gnu.org/ml/gcc-bugs/2000-09/msg00271.html
   *
   *  Hence, we cannot remove the (content < 0) guard, even though current
   *  GCC gives a warning for it -- other compilers (including earlier GCC
   *  versions) may use a signed type for enum OggzStreamContent.
   */
  if (
#ifdef ALLOW_SIGNED_ENUMS
      content < OGGZ_CONTENT_THEORA ||
#endif
      content >= OGGZ_CONTENT_UNKNOWN)
    return NULL;

  return oggz_auto_codec_ident[content].content_type;
}
