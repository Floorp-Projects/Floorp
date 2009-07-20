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
 * oggz_read.c
 *
 * Conrad Parker <conrad@annodex.net>
 */

#include "config.h"

#if OGGZ_CONFIG_READ

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

#include <oggz/oggz_stream.h>

/* #define DEBUG */
/* #define DEBUG_VERBOSE */

#define CHUNKSIZE 65536

#define OGGZ_READ_EMPTY (-404)

OGGZ *
oggz_read_init (OGGZ * oggz)
{
  OggzReader * reader = &oggz->x.reader;

  ogg_sync_init (&reader->ogg_sync);
  ogg_stream_init (&reader->ogg_stream, (int)-1);
  reader->current_serialno = -1;

  reader->read_packet = NULL;
  reader->read_user_data = NULL;

  reader->read_page = NULL;
  reader->read_page_user_data = NULL;

  reader->current_unit = 0;

  reader->current_page_bytes = 0;

  return oggz;
}

OGGZ *
oggz_read_close (OGGZ * oggz)
{
  OggzReader * reader = &oggz->x.reader;

  ogg_stream_clear (&reader->ogg_stream);
  ogg_sync_clear (&reader->ogg_sync);

  return oggz;
}

int
oggz_set_read_callback (OGGZ * oggz, long serialno,
                        OggzReadPacket read_packet, void * user_data)
{
  OggzReader * reader;
  oggz_stream_t * stream;

  if (oggz == NULL) return OGGZ_ERR_BAD_OGGZ;

  reader =  &oggz->x.reader;

  if (oggz->flags & OGGZ_WRITE) {
    return OGGZ_ERR_INVALID;
  }

  if (serialno == -1) {
    reader->read_packet = read_packet;
    reader->read_user_data = user_data;
  } else {
    stream = oggz_get_stream (oggz, serialno);
    if (stream == NULL)
      stream = oggz_add_stream (oggz, serialno);
    if (stream == NULL)
      return OGGZ_ERR_OUT_OF_MEMORY;

    stream->read_packet = read_packet;
    stream->read_user_data = user_data;
  }

  return 0;
}

int
oggz_set_read_page (OGGZ * oggz, long serialno, OggzReadPage read_page,
                    void * user_data)
{
  OggzReader * reader;
  oggz_stream_t * stream;

  if (oggz == NULL) return OGGZ_ERR_BAD_OGGZ;

  reader =  &oggz->x.reader;

  if (oggz->flags & OGGZ_WRITE) {
    return OGGZ_ERR_INVALID;
  }

  if (serialno == -1) {
    reader->read_page = read_page;
    reader->read_page_user_data = user_data;
  } else {
    stream = oggz_get_stream (oggz, serialno);
    if (stream == NULL)
      stream = oggz_add_stream (oggz, serialno);
    if (stream == NULL)
      return OGGZ_ERR_OUT_OF_MEMORY;

    stream->read_page = read_page;
    stream->read_page_user_data = user_data;
  }

  return 0;
}

/*
 * oggz_read_get_next_page (oggz, og, do_read)
 *
 * This differs from oggz_get_next_page() in oggz_seek.c in that it
 * does not attempt to call oggz_io_read() if the sync buffer is empty.
 *
 * retrieves the next page.
 * returns >= 0 if found; return value is offset of page start
 * returns -1 on error
 * returns -2 if EOF was encountered
 */
static oggz_off_t
oggz_read_get_next_page (OGGZ * oggz, ogg_page * og)
{
  OggzReader * reader = &oggz->x.reader;
  long more = 0, page_offset = 0;
  int found = 0;

  /* Increment oggz->offset by length of the last page processed */
  oggz->offset += reader->current_page_bytes;

  do {
    more = ogg_sync_pageseek (&reader->ogg_sync, og);

    if (more == 0) {
      /* No page available */
      reader->current_page_bytes = 0;
      return -2;
    } else if (more < 0) {
#ifdef DEBUG_VERBOSE
      printf ("get_next_page: skipped %ld bytes\n", -more);
#endif
      page_offset += (-more);
    } else {
#ifdef DEBUG_VERBOSE
      printf ("get_next_page: page has %ld bytes\n", more);
#endif
      reader->current_page_bytes = more;
      found = 1;
    }

  } while (!found);

  oggz->offset += page_offset;

  return oggz->offset;
}

typedef struct {
  ogg_packet      packet;
  ogg_int64_t     calced_granulepos;
  oggz_stream_t * stream;
  OggzReader    * reader;
  OGGZ          * oggz;
  long            serialno;
} OggzBufferedPacket;

OggzBufferedPacket *
oggz_read_new_pbuffer_entry(OGGZ *oggz, ogg_packet *packet, 
            ogg_int64_t granulepos, long serialno, oggz_stream_t * stream, 
            OggzReader *reader) {

  OggzBufferedPacket *p = oggz_malloc(sizeof(OggzBufferedPacket));
  if (p == NULL) return NULL;

  memcpy(&(p->packet), packet, sizeof(ogg_packet));
  p->packet.packet = oggz_malloc(packet->bytes);
  memcpy(p->packet.packet, packet->packet, packet->bytes);

  p->calced_granulepos = granulepos;
  p->stream = stream;
  p->serialno = serialno;
  p->reader = reader;
  p->oggz = oggz;

  return p;
}

void
oggz_read_free_pbuffer_entry(OggzBufferedPacket *p) {
  
  oggz_free(p->packet.packet);
  oggz_free(p);

}

OggzDListIterResponse
oggz_read_update_gp(void *elem) {

  OggzBufferedPacket *p = (OggzBufferedPacket *)elem;

  if (p->calced_granulepos == -1 && p->stream->last_granulepos != -1) {
    int content = oggz_stream_get_content(p->oggz, p->serialno);
    p->calced_granulepos = 
      oggz_auto_calculate_gp_backwards(content, p->stream->last_granulepos,
      p->stream, &(p->packet), p->stream->last_packet);
      
    p->stream->last_granulepos = p->calced_granulepos;
    p->stream->last_packet = &(p->packet);
  }

  return DLIST_ITER_CONTINUE;

}

OggzDListIterResponse
oggz_read_deliver_packet(void *elem) {

  OggzBufferedPacket *p = (OggzBufferedPacket *)elem;
  ogg_int64_t gp_stored;
  ogg_int64_t unit_stored;

  if (p->calced_granulepos == -1) {
    return DLIST_ITER_CANCEL;
  }

  gp_stored = p->reader->current_granulepos;
  unit_stored = p->reader->current_unit;

  p->reader->current_granulepos = p->calced_granulepos;

  p->reader->current_unit =
    oggz_get_unit (p->oggz, p->serialno, p->calced_granulepos);

  if (p->stream->read_packet) {
    if (p->stream->read_packet(p->oggz, &(p->packet), p->serialno, 
			       p->stream->read_user_data) != 0) {
      return DLIST_ITER_ERROR;
    }
  } else if (p->reader->read_packet) {
    if (p->reader->read_packet(p->oggz, &(p->packet), p->serialno, 
			       p->reader->read_user_data) != 0) {
      return DLIST_ITER_ERROR;
    }
  }

  p->reader->current_granulepos = gp_stored;
  p->reader->current_unit = unit_stored;

  oggz_read_free_pbuffer_entry(p);

  return DLIST_ITER_CONTINUE;
}

static int
oggz_read_sync (OGGZ * oggz)
{
  OggzReader * reader = &oggz->x.reader;

  oggz_stream_t * stream;
  ogg_stream_state * os;
  ogg_packet * op;
  long serialno;

  ogg_packet packet;
  ogg_page og;

  int cb_ret = 0;

  /*os = &reader->ogg_stream;*/
  op = &packet;

  /* handle one packet.  Try to fetch it from current stream state */
  /* extract packets from page */
  while(cb_ret == 0) {

    if (reader->current_serialno != -1) {
    /* process a packet if we can.  If the machine isn't loaded,
       neither is a page */
      while(cb_ret == 0) {
        ogg_int64_t granulepos;
        int result;

        serialno = reader->current_serialno;

        stream = oggz_get_stream (oggz, serialno);

        if (stream == NULL) {
          /* new stream ... check bos etc. */
          if ((stream = oggz_add_stream (oggz, serialno)) == NULL) {
            /* error -- could not add stream */
            return OGGZ_ERR_OUT_OF_MEMORY;
          }
        }
        os = &stream->ogg_stream;

        result = ogg_stream_packetout(os, op);

        /*
         * libogg flags "holes in the data" (which are really inconsistencies
         * in the page sequence number) by returning -1.
         */
        if(result == -1) {
#ifdef DEBUG
          printf ("oggz_read_sync: hole in the data\n");
#endif
          /* We can't tolerate holes in headers, so bail out. NB. as stream->packetno
           * has not yet been incremented, the current value refers to how many packets
           * have been processed prior to this one. */
          if (stream->packetno < 2) return OGGZ_ERR_HOLE_IN_DATA;

          /* Holes in content occur in some files and pretty much don't matter,
           * so we silently swallow the notification and reget the packet.
           */
          result = ogg_stream_packetout(os, op);
          if (result == -1) {
            /* If the result is *still* -1 then something strange is
             * happening.
             */
#ifdef DEBUG
            printf ("Multiple holes in data!");
#endif
            return OGGZ_ERR_HOLE_IN_DATA;
          }
        }

        if(result > 0){
          int content;

          stream->packetno++;
          
          /* got a packet.  process it */
          granulepos = op->granulepos;

          content = oggz_stream_get_content(oggz, serialno);
  
          /*
           * if we have no metrics for this stream yet, then generate them
           */      
          if 
          (
            (!stream->metric || (content == OGGZ_CONTENT_SKELETON)) 
            && 
            (oggz->flags & OGGZ_AUTO)
          ) 
          {
            oggz_auto_read_bos_packet (oggz, op, serialno, NULL);
          }

          /* attempt to determine granulepos for this packet */
          if (oggz->flags & OGGZ_AUTO) {
            reader->current_granulepos = 
              oggz_auto_calculate_granulepos (content, granulepos, stream, op); 
            /* make sure that we accept any "real" gaps in the granulepos
             */
            if (granulepos != -1 && reader->current_granulepos < granulepos) {
              reader->current_granulepos = granulepos;
            }
          } else {
            reader->current_granulepos = granulepos;
          }
          stream->last_granulepos = reader->current_granulepos;
        
          /* set unit on last packet of page */
          if 
          (
            (oggz->metric || stream->metric) && reader->current_granulepos != -1
          ) 
          {
            reader->current_unit =
              oggz_get_unit (oggz, serialno, reader->current_granulepos);
          }

          if (stream->packetno == 1) {
            oggz_auto_read_comments (oggz, stream, serialno, op);
          }
          
          if (oggz->flags & OGGZ_AUTO) {
          
            /*
             * while we are getting invalid granulepos values, store the 
             * incoming packets in a dlist */
            if (reader->current_granulepos == -1) {
              OggzBufferedPacket *p = oggz_read_new_pbuffer_entry(
                                oggz, &packet, reader->current_granulepos, 
                                serialno, stream, reader);

              oggz_dlist_append(oggz->packet_buffer, p);
              continue;
            } else if (!oggz_dlist_is_empty(oggz->packet_buffer)) {
              /*
               * move backward through the list assigning gp values based upon
               * the granulepos we just recieved.  Then move forward through
               * the list delivering any packets at the beginning with valid
               * gp values
               */
              ogg_int64_t gp_stored = stream->last_granulepos;
              stream->last_packet = &packet;
              oggz_dlist_reverse_iter(oggz->packet_buffer, oggz_read_update_gp);
              if (oggz_dlist_deliter(oggz->packet_buffer, oggz_read_deliver_packet) == -1) {
		return OGGZ_ERR_HOLE_IN_DATA;
	      }

              /*
               * fix up the stream granulepos 
               */
              stream->last_granulepos = gp_stored;

              if (!oggz_dlist_is_empty(oggz->packet_buffer)) {
                OggzBufferedPacket *p = oggz_read_new_pbuffer_entry(
                                oggz, &packet, reader->current_granulepos, 
                                serialno, stream, reader);

                oggz_dlist_append(oggz->packet_buffer, p);
                continue;
              }
            }
          }

          if (stream->read_packet) {
            cb_ret =
              stream->read_packet (oggz, op, serialno, stream->read_user_data);
          } else if (reader->read_packet) {
            cb_ret =
              reader->read_packet (oggz, op, serialno, reader->read_user_data);
          }

          /* Mark this stream as having delivered a non b_o_s packet if so.
           * In the case where there is no packet reading callback, this is
           * also valid as the page reading callback has already been called.
           */
          if (!op->b_o_s) stream->delivered_non_b_o_s = 1;
        }
        else
          break;
      }
    }

    /* If we've got a stop already, don't read more data in */
    if (cb_ret == OGGZ_STOP_OK || 
	cb_ret == OGGZ_STOP_ERR || 
	cb_ret == OGGZ_ERR_HOLE_IN_DATA) 
      return cb_ret;

    if(oggz_read_get_next_page (oggz, &og) < 0)
      return OGGZ_READ_EMPTY; /* eof. leave uninitialized */

    serialno = ogg_page_serialno (&og);
    reader->current_serialno = serialno; /* XXX: maybe not necessary */

    stream = oggz_get_stream (oggz, serialno);

    if (stream == NULL) {
      /* new stream ... check bos etc. */
      if ((stream = oggz_add_stream (oggz, serialno)) == NULL) {
        /* error -- could not add stream */
        return OGGZ_ERR_OUT_OF_MEMORY;
      }

      /* identify stream type */
      oggz_auto_identify_page (oggz, &og, serialno);

      /* read bos data */
      if (oggz->flags & OGGZ_AUTO)
        oggz_auto_read_bos_page (oggz, &og, serialno, NULL);
    }
    else if (oggz_stream_get_content(oggz, serialno) == OGGZ_CONTENT_ANXDATA)
    {
      /*
       * re-identify ANXDATA streams as these are now content streams
       */
      oggz_auto_identify_page (oggz, &og, serialno);
    }
    
    os = &stream->ogg_stream;

    {
      ogg_int64_t granulepos;

      granulepos = ogg_page_granulepos (&og);
      stream->page_granulepos = granulepos;

      if ((oggz->metric || stream->metric) && granulepos != -1) {
       reader->current_unit = oggz_get_unit (oggz, serialno, granulepos);
      } else if (granulepos == 0) {
       reader->current_unit = 0;
      }
    }

    if (stream->read_page) {
      cb_ret =
        stream->read_page (oggz, &og, serialno, stream->read_page_user_data);
    } else if (reader->read_page) {
      cb_ret =
        reader->read_page (oggz, &og, serialno, reader->read_page_user_data);
    }

    ogg_stream_pagein(os, &og);
  }

  return cb_ret;
}

long
oggz_read (OGGZ * oggz, long n)
{
  OggzReader * reader;
  char * buffer;
  long bytes, bytes_read = 1, remaining = n, nread = 0;
  int cb_ret = 0;

  if (oggz == NULL) return OGGZ_ERR_BAD_OGGZ;

  if (oggz->flags & OGGZ_WRITE) {
    return OGGZ_ERR_INVALID;
  }

  if ((cb_ret = oggz->cb_next) != OGGZ_CONTINUE) {
    oggz->cb_next = 0;
    return oggz_map_return_value_to_error (cb_ret);
  }

  reader = &oggz->x.reader;

  cb_ret = oggz_read_sync (oggz);
  if (cb_ret == OGGZ_ERR_OUT_OF_MEMORY)
    return cb_ret;

  while (cb_ret != OGGZ_STOP_ERR && cb_ret != OGGZ_STOP_OK &&
         bytes_read > 0 && remaining > 0) {
    bytes = MIN (remaining, CHUNKSIZE);
    buffer = ogg_sync_buffer (&reader->ogg_sync, bytes);
    bytes_read = (long) oggz_io_read (oggz, buffer, bytes);
    if (bytes_read == OGGZ_ERR_SYSTEM) {
      return OGGZ_ERR_SYSTEM;
    }

    if (bytes_read > 0) {
      ogg_sync_wrote (&reader->ogg_sync, bytes_read);
      
      remaining -= bytes_read;
      nread += bytes_read;
      
      cb_ret = oggz_read_sync (oggz);
      if (cb_ret == OGGZ_ERR_OUT_OF_MEMORY || cb_ret == OGGZ_ERR_HOLE_IN_DATA) {
        return cb_ret;
      }
    }
  }

  if (cb_ret == OGGZ_STOP_ERR) oggz_purge (oggz);

  /* Don't return 0 unless it's actually an EOF condition */
  if (nread == 0) {
    switch (bytes_read) {
    case OGGZ_ERR_IO_AGAIN:
    case OGGZ_ERR_SYSTEM:
      return bytes_read; break;
    default: break;
    }

    if (cb_ret == OGGZ_READ_EMPTY) {
      return 0;
    } else {
      return oggz_map_return_value_to_error (cb_ret);
    }

  } else {
    if (cb_ret == OGGZ_READ_EMPTY) cb_ret = OGGZ_CONTINUE;
    oggz->cb_next = cb_ret;
  }

  return nread;
}

/* generic */
long
oggz_read_input (OGGZ * oggz, unsigned char * buf, long n)
{
  OggzReader * reader;
  char * buffer;
  long bytes, remaining = n, nread = 0;
  int cb_ret = 0;

  if (oggz == NULL) return OGGZ_ERR_BAD_OGGZ;

  if (oggz->flags & OGGZ_WRITE) {
    return OGGZ_ERR_INVALID;
  }

  if ((cb_ret = oggz->cb_next) != OGGZ_CONTINUE) {
    oggz->cb_next = 0;
    return oggz_map_return_value_to_error (cb_ret);
  }

  reader = &oggz->x.reader;

  cb_ret = oggz_read_sync (oggz);
  if (cb_ret == OGGZ_ERR_OUT_OF_MEMORY)
    return cb_ret;

  while (cb_ret != OGGZ_STOP_ERR && cb_ret != OGGZ_STOP_OK  &&
         /* !oggz->eos && */ remaining > 0) {
    bytes = MIN (remaining, 4096);
    buffer = ogg_sync_buffer (&reader->ogg_sync, bytes);
    memcpy (buffer, buf, bytes);
    ogg_sync_wrote (&reader->ogg_sync, bytes);

    buf += bytes;
    remaining -= bytes;
    nread += bytes;

    cb_ret = oggz_read_sync (oggz);
    if (cb_ret == OGGZ_ERR_OUT_OF_MEMORY)
      return cb_ret;
  }

  if (cb_ret == OGGZ_STOP_ERR) oggz_purge (oggz);

  if (nread == 0) {
    /* Don't return 0 unless it's actually an EOF condition */
    if (cb_ret == OGGZ_READ_EMPTY) {
      return OGGZ_ERR_STOP_OK;
    } else {
      return oggz_map_return_value_to_error (cb_ret);
    }
  } else {
    if (cb_ret == OGGZ_READ_EMPTY) cb_ret = OGGZ_CONTINUE;
    oggz->cb_next = cb_ret;
  }

  return nread;
}


#else /* OGGZ_CONFIG_READ */

#include <ogg/ogg.h>
#include "oggz_private.h"

OGGZ *
oggz_read_init (OGGZ * oggz)
{
  return NULL;
}

OGGZ *
oggz_read_close (OGGZ * oggz)
{
  return NULL;
}

int
oggz_set_read_callback (OGGZ * oggz, long serialno,
                        OggzReadPacket read_packet, void * user_data)
{
  return OGGZ_ERR_DISABLED;
}

long
oggz_read (OGGZ * oggz, long n)
{
  return OGGZ_ERR_DISABLED;
}

long
oggz_read_input (OGGZ * oggz, unsigned char * buf, long n)
{
  return OGGZ_ERR_DISABLED;
}

#endif
