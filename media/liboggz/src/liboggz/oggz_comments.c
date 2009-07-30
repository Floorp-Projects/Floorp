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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h> /* ULONG_MAX */
#ifndef WIN32
#include <strings.h>
#endif

#include <assert.h>

#include "oggz_private.h"
#include "oggz_vector.h"

#include <oggz/oggz_stream.h>

/*#define DEBUG*/

#ifdef WIN32                                                                   
#define strcasecmp _stricmp
#endif

/* Ensure comment vector length can be expressed in 32 bits
 * including space for the trailing NUL */
#define MAX_COMMENT_LENGTH 0xFFFFFFFE
#define oggz_comment_clamp(c) MIN((c),MAX_COMMENT_LENGTH)

static size_t
oggz_comment_len (const char * s)
{
  size_t len;

  if (s == NULL) return 0;

  len = strlen (s);
  return oggz_comment_clamp(len);
}

static char *
oggz_strdup (const char * s)
{
  char * ret;
  if (s == NULL) return NULL;
  ret = oggz_malloc (oggz_comment_len(s) + 1);
  if (ret == NULL) return NULL;

  return strcpy (ret, s);
}

static char *
oggz_strdup_len (const char * s, size_t len)
{
  char * ret;
  if (s == NULL) return NULL;
  if (len == 0) return NULL;
  len = oggz_comment_clamp(len);
  ret = oggz_malloc (len + 1);
  if (!ret) return NULL;
  if (strncpy (ret, s, len) == NULL) {
    oggz_free (ret);
    return NULL;
  }

  ret[len] = '\0';
  return ret;
}

static char *
oggz_index_len (const char * s, char c, int len)
{
  int i;

  for (i = 0; *s && i < len; i++, s++) {
    if (*s == c) return (char *)s;
  }

  return NULL;
}

/*
 Comments will be stored in the Vorbis style.
 It is describled in the "Structure" section of
    http://www.xiph.org/ogg/vorbis/doc/v-comment.html

The comment header is decoded as follows:
  1) [vendor_length] = read an unsigned integer of 32 bits
  2) [vendor_string] = read a UTF-8 vector as [vendor_length] octets
  3) [user_comment_list_length] = read an unsigned integer of 32 bits
  4) iterate [user_comment_list_length] times {
     5) [length] = read an unsigned integer of 32 bits
     6) this iteration's user comment = read a UTF-8 vector as [length] octets
     }
  7) [framing_bit] = read a single bit as boolean
  8) if ( [framing_bit]  unset or end of packet ) then ERROR
  9) done.

  If you have troubles, please write to ymnk@jcraft.com.
 */

#define readint(buf, base) (((buf[base+3]<<24)&0xff000000)| \
                           ((buf[base+2]<<16)&0xff0000)| \
                           ((buf[base+1]<<8)&0xff00)| \
  	           	    (buf[base]&0xff))
#define writeint(buf, base, val) buf[base+3]=(char)(((val)>>24)&0xff); \
                                 buf[base+2]=(char)(((val)>>16)&0xff); \
                                 buf[base+1]=(char)(((val)>>8)&0xff); \
                                 buf[base]=(char)((val)&0xff);

/* The FLAC header will encode the length of the comments in
   24bit BE format.
*/
#define writeint24be(buf, base, val) buf[base]=(char)(((val)>>16)&0xff); \
                                     buf[base+1]=(char)(((val)>>8)&0xff); \
                                     buf[base+2]=(char)((val)&0xff);

static int
oggz_comment_validate_byname (const char * name, const char * value)
{
  const char * c;

  if (!name || !value) return 0;

  for (c = name; *c; c++) {
    if (*c < 0x20 || *c > 0x7D || *c == 0x3D) {
#ifdef DEBUG
      printf ("XXX char %c in %s invalid\n", *c, name);
#endif
      return 0;
    }
  }

  /* XXX: we really should validate value as UTF-8 here, but ... */

  return 1;
}

static OggzComment *
oggz_comment_new (const char * name, const char * value)
{
  OggzComment * comment;

  if (!oggz_comment_validate_byname (name, value)) return NULL;
  /* Ensures that name != NULL, value != NULL, and validates strings */

  comment = oggz_malloc (sizeof (OggzComment));
  if (comment == NULL) return NULL;

  comment->name = oggz_strdup (name);
  if (comment->name == NULL) {
    oggz_free (comment);
    return NULL;
  }

  comment->value = oggz_strdup (value);
  if (comment->value == NULL) {
    oggz_free (comment->name);
    oggz_free (comment);
    return NULL;
  }

  return comment;
}

static void
oggz_comment_free (OggzComment * comment)
{
  if (!comment) return;
  if (comment->name) oggz_free (comment->name);
  if (comment->value) oggz_free (comment->value);
  oggz_free (comment);
}

static int
oggz_comment_cmp (const OggzComment * comment1, const OggzComment * comment2)
{
  if (comment1 == comment2) return 1;
  if (!comment1 || !comment2) return 0;

  if (strcasecmp (comment1->name, comment2->name)) return 0;
  if (strcmp (comment1->value, comment2->value)) return 0;

  return 1;
}

static int
_oggz_comment_set_vendor (OGGZ * oggz, long serialno,
			  const char * vendor_string)
{
  oggz_stream_t * stream;

  if (oggz == NULL) return OGGZ_ERR_BAD_OGGZ;

  stream = oggz_get_stream (oggz, serialno);
  if (stream == NULL) return OGGZ_ERR_BAD_SERIALNO;

  if (stream->vendor) oggz_free (stream->vendor);

  if ((stream->vendor = oggz_strdup (vendor_string)) == NULL)
    return OGGZ_ERR_OUT_OF_MEMORY;

  return 0;
}

/* Public API */

const char *
oggz_comment_get_vendor (OGGZ * oggz, long serialno)
{
  oggz_stream_t * stream;

  if (oggz == NULL) return NULL;

  stream = oggz_get_stream (oggz, serialno);
  if (stream == NULL) return NULL;

  return stream->vendor;
}

int
oggz_comment_set_vendor (OGGZ * oggz, long serialno, const char * vendor_string)
{
  oggz_stream_t * stream;
  if (oggz == NULL) return OGGZ_ERR_BAD_OGGZ;

  stream = oggz_get_stream (oggz, serialno);
  if (stream == NULL)
    stream = oggz_add_stream (oggz, serialno);
  if (stream == NULL)
    return OGGZ_ERR_OUT_OF_MEMORY;

  if (oggz->flags & OGGZ_WRITE) {
    if (OGGZ_CONFIG_WRITE) {

      return _oggz_comment_set_vendor (oggz, serialno, vendor_string);

    } else {
      return OGGZ_ERR_DISABLED;
    }
  } else {
    return OGGZ_ERR_INVALID;
  }
}


const OggzComment *
oggz_comment_first (OGGZ * oggz, long serialno)
{
  oggz_stream_t * stream;

  if (oggz == NULL) return NULL;

  stream = oggz_get_stream (oggz, serialno);
  if (stream == NULL) return NULL;

  return oggz_vector_nth_p (stream->comments, 0);
}

const OggzComment *
oggz_comment_first_byname (OGGZ * oggz, long serialno, char * name)
{
  oggz_stream_t * stream;
  OggzComment * comment;
  int i;

  if (oggz == NULL) return NULL;

  stream = oggz_get_stream (oggz, serialno);
  if (stream == NULL) return NULL;

  if (name == NULL) return oggz_vector_nth_p (stream->comments, 0);

  if (!oggz_comment_validate_byname (name, ""))
    return NULL;

  for (i = 0; i < oggz_vector_size (stream->comments); i++) {
    comment = (OggzComment *) oggz_vector_nth_p (stream->comments, i);
    if (comment->name && !strcasecmp (name, comment->name))
      return comment;
  }

  return NULL;
}

const OggzComment *
oggz_comment_next (OGGZ * oggz, long serialno, const OggzComment * comment)
{
  oggz_stream_t * stream;
  int i;

  if (oggz == NULL || comment == NULL) return NULL;

  stream = oggz_get_stream (oggz, serialno);
  if (stream == NULL) return NULL;

  i = oggz_vector_find_index_p (stream->comments, comment);

  return oggz_vector_nth_p (stream->comments, i+1);
}

const OggzComment *
oggz_comment_next_byname (OGGZ * oggz, long serialno,
                          const OggzComment * comment)
{
  oggz_stream_t * stream;
  OggzComment * v_comment;
  int i;

  if (oggz == NULL || comment == NULL) return NULL;

  stream = oggz_get_stream (oggz, serialno);
  if (stream == NULL) return NULL;

  i = oggz_vector_find_index_p (stream->comments, comment);

  for (i++; i < oggz_vector_size (stream->comments); i++) {
    v_comment = (OggzComment *) oggz_vector_nth_p (stream->comments, i);
    if (v_comment->name && !strcasecmp (comment->name, v_comment->name))
      return v_comment;
  }

  return NULL;
}

#define _oggz_comment_add(f,c) oggz_vector_insert_p ((f)->comments, (c))

int
oggz_comment_add (OGGZ * oggz, long serialno, const OggzComment * comment)
{
  oggz_stream_t * stream;
  OggzComment * new_comment;

  if (oggz == NULL) return OGGZ_ERR_BAD_OGGZ;

  stream = oggz_get_stream (oggz, serialno);
  if (stream == NULL)
    stream = oggz_add_stream (oggz, serialno);
  if (stream == NULL)
    return OGGZ_ERR_OUT_OF_MEMORY;

  if (oggz->flags & OGGZ_WRITE) {
    if (OGGZ_CONFIG_WRITE) {

      if (!oggz_comment_validate_byname (comment->name, comment->value))
        return OGGZ_ERR_COMMENT_INVALID;

      if ((new_comment = oggz_comment_new (comment->name, comment->value)) == NULL)
        return OGGZ_ERR_OUT_OF_MEMORY;

      if (_oggz_comment_add (stream, new_comment) == NULL)
        return OGGZ_ERR_OUT_OF_MEMORY;

      return 0;
    } else {
      return OGGZ_ERR_DISABLED;
    }
  } else {
    return OGGZ_ERR_INVALID;
  }
}

int
oggz_comment_add_byname (OGGZ * oggz, long serialno,
                         const char * name, const char * value)
{
  oggz_stream_t * stream;
  OggzComment * new_comment;

  if (oggz == NULL) return OGGZ_ERR_BAD_OGGZ;

  stream = oggz_get_stream (oggz, serialno);
  if (stream == NULL)
    stream = oggz_add_stream (oggz, serialno);
  if (stream == NULL)
    return OGGZ_ERR_OUT_OF_MEMORY;

  if (oggz->flags & OGGZ_WRITE) {
    if (OGGZ_CONFIG_WRITE) {

      if (!oggz_comment_validate_byname (name, value))
        return OGGZ_ERR_COMMENT_INVALID;

      if ((new_comment = oggz_comment_new (name, value)) == NULL)
        return OGGZ_ERR_OUT_OF_MEMORY;

      if (_oggz_comment_add (stream, new_comment) == NULL)
        return OGGZ_ERR_OUT_OF_MEMORY;

      return 0;
    } else {
      return OGGZ_ERR_DISABLED;
    }
  } else {
    return OGGZ_ERR_INVALID;
  }
}

int
oggz_comment_remove (OGGZ * oggz, long serialno, OggzComment * comment)
{
  oggz_stream_t * stream;
  OggzComment * v_comment;

  if (oggz == NULL) return OGGZ_ERR_BAD_OGGZ;

  stream = oggz_get_stream (oggz, serialno);
  if (stream == NULL) return OGGZ_ERR_BAD_SERIALNO;

  if (oggz->flags & OGGZ_WRITE) {
    if (OGGZ_CONFIG_WRITE) {
      v_comment = oggz_vector_find_p (stream->comments, comment);

      if (v_comment == NULL) return 0;

      oggz_vector_remove_p (stream->comments, v_comment);
      oggz_comment_free (v_comment);

      return 1;

    } else {
      return OGGZ_ERR_DISABLED;
    }
  } else {
    return OGGZ_ERR_INVALID;
  }
}

int
oggz_comment_remove_byname (OGGZ * oggz, long serialno, char * name)
{
  oggz_stream_t * stream;
  OggzComment * comment;
  int i, ret = 0;

  if (oggz == NULL) return OGGZ_ERR_BAD_OGGZ;

  stream = oggz_get_stream (oggz, serialno);
  if (stream == NULL) return OGGZ_ERR_BAD_SERIALNO;

  if (oggz->flags & OGGZ_WRITE) {
    if (OGGZ_CONFIG_WRITE) {
      for (i = 0; i < oggz_vector_size (stream->comments); i++) {
        comment = (OggzComment *) oggz_vector_nth_p (stream->comments, i);
        if (!strcasecmp (name, comment->name)) {
          oggz_comment_remove (oggz, serialno, comment);
          i--;
          ret++;
        }
      }
      return ret;
    } else {
      return OGGZ_ERR_DISABLED;
    }
  } else {
    return OGGZ_ERR_INVALID;
  }
}

int
oggz_comments_copy (OGGZ * src, long src_serialno,
                    OGGZ * dest, long dest_serialno)
{
  const OggzComment * comment;

  if (src == NULL || dest == NULL) return OGGZ_ERR_BAD_OGGZ;

  if (dest->flags & OGGZ_WRITE) {
    if (OGGZ_CONFIG_WRITE) {
      oggz_comment_set_vendor (dest, dest_serialno,
                               oggz_comment_get_vendor (src, src_serialno));

      for (comment = oggz_comment_first (src, src_serialno); comment;
           comment = oggz_comment_next (src, src_serialno, comment)) {
        oggz_comment_add (dest, dest_serialno, comment);
      }
    } else {
      return OGGZ_ERR_DISABLED;
    }
  } else {
    return OGGZ_ERR_INVALID;
  }

  return 0;
}

/* Internal API */
int
oggz_comments_init (oggz_stream_t * stream)
{
  stream->vendor = NULL;
  stream->comments = oggz_vector_new ();
  if (stream->comments == NULL) return -1;

  oggz_vector_set_cmp (stream->comments, (OggzCmpFunc) oggz_comment_cmp, NULL);

  return 0;
}

int
oggz_comments_free (oggz_stream_t * stream)
{
  oggz_vector_foreach (stream->comments, (OggzFunc)oggz_comment_free);
  oggz_vector_delete (stream->comments);
  stream->comments = NULL;

  if (stream->vendor) oggz_free (stream->vendor);
  stream->vendor = NULL;

  return 0;
}

int
oggz_comments_decode (OGGZ * oggz, long serialno,
                      unsigned char * comments, long length)
{
   oggz_stream_t * stream;
   char *c= (char *)comments;
   int i, nb_fields, n;
   size_t len;
   char *end;
   char * name, * value, * nvalue = NULL;
   OggzComment * comment;

   if (length<8)
      return -1;

   end = c+length;
   len=readint(c, 0);

   c+=4;
   if (len>(size_t)(end-c)) return -1;

   stream = oggz_get_stream (oggz, serialno);
   if (stream == NULL) return OGGZ_ERR_BAD_SERIALNO;

   /* Vendor */
   if (len > 0) {
     if ((nvalue = oggz_strdup_len (c, len)) == NULL)
       return OGGZ_ERR_OUT_OF_MEMORY;

     if (_oggz_comment_set_vendor (oggz, serialno, nvalue) == OGGZ_ERR_OUT_OF_MEMORY)
       return OGGZ_ERR_OUT_OF_MEMORY;

     oggz_free (nvalue);
   }

#ifdef DEBUG
   fwrite(c, 1, len, stderr); fputc ('\n', stderr);
#endif
   c+=len;

   if (c+4>end) return -1;

   /* This value gets checked effectively by the 'for' condition
      and the checks within the loop for c running off the end.  */
   nb_fields=readint(c, 0);
   c+=4;
   for (i=0;i<nb_fields;i++) {
      if (c+4>end) return -1;

      len=readint(c, 0);

      c+=4;
      if (len>(size_t)(end-c)) return -1;

      name = c;
      value = oggz_index_len (c, '=', len);
      if (value) {
         *value = '\0';
         value++;

         n = c+len - value;
         if ((nvalue = oggz_strdup_len (value, n)) == NULL)
           return OGGZ_ERR_OUT_OF_MEMORY;

#ifdef DEBUG
         printf ("oggz_comments_decode: %s -> %s (length %d)\n",
         name, nvalue, n);
#endif
         if ((comment = oggz_comment_new (name, nvalue)) == NULL)
           return OGGZ_ERR_OUT_OF_MEMORY;

         if (_oggz_comment_add (stream, comment) == NULL)
           return OGGZ_ERR_OUT_OF_MEMORY;

         oggz_free (nvalue);
      } else {
         if ((nvalue = oggz_strdup_len (name, len)) == NULL)
           return OGGZ_ERR_OUT_OF_MEMORY;

         if ((comment = oggz_comment_new (nvalue, NULL)) == NULL)
           return OGGZ_ERR_OUT_OF_MEMORY;

         if (_oggz_comment_add (stream, comment) == NULL)
           return OGGZ_ERR_OUT_OF_MEMORY;

         oggz_free (nvalue);
      }

      c+=len;
   }

#ifdef DEBUG
   printf ("oggz_comments_decode: done\n");
#endif

   return 0;
}

/*
 * Pre-condition: at least one of accum, delta are non-zero,
 * ie. don't call accum_length (0, 0);
 * \retval 0 Failure: integer overflow
 */
static unsigned long
accum_length (unsigned long * accum, unsigned long delta)
{
  /* Pre-condition: don't call accum_length (0, 0) */
  assert (*accum != 0 || delta != 0);

  /* Check for integer overflow */
  if (delta > ULONG_MAX - (*accum))
    return 0;

  *accum += delta;

  return *accum;
}

long
oggz_comments_encode (OGGZ * oggz, long serialno,
                      unsigned char * buf, long length)
{
  oggz_stream_t * stream;
  char * c = (char *)buf;
  const OggzComment * comment;
  int nb_fields = 0, vendor_length = 0;
  unsigned long actual_length = 0, remaining = length, field_length;

  /* Deal with sign of length first */
  if (length < 0) return 0;

  stream = oggz_get_stream (oggz, serialno);
  if (stream == NULL) return OGGZ_ERR_BAD_SERIALNO;

  /* Vendor string */
  if (stream->vendor)
    vendor_length = oggz_comment_len (stream->vendor);
  if (accum_length (&actual_length, 4 + vendor_length) == 0)
    return 0;
#ifdef DEBUG
  printf ("oggz_comments_encode: vendor = %s\n", stream->vendor);
#endif

  /* user comment list length */
  if (accum_length (&actual_length, 4) == 0)
    return 0;


  for (comment = oggz_comment_first (oggz, serialno); comment;
       comment = oggz_comment_next (oggz, serialno, comment)) {
    /* [size]"name" */
    if (accum_length (&actual_length, 4 + oggz_comment_len (comment->name)) == 0)
      return 0;
    if (comment->value) {
      /* "=value" */
      if (accum_length (&actual_length, 1 + oggz_comment_len (comment->value)) == 0)
        return 0;
    }

#ifdef DEBUG
    printf ("oggz_comments_encode: %s = %s\n",
	    comment->name, comment->value);
#endif

    nb_fields++;
  }

  /* framing bit */
  if (accum_length (&actual_length, 1) == 0)
    return 0;

  /* NB. actual_length is not modified from here onwards */

  if (buf == NULL) return actual_length;

  remaining -= 4;
  if (remaining <= 0) return actual_length;
  writeint (c, 0, vendor_length);
  c += 4;

  if (stream->vendor) {
    field_length = oggz_comment_len (stream->vendor);
    memcpy (c, stream->vendor, MIN (field_length, remaining));
    c += field_length; remaining -= field_length;
    if (remaining <= 0) return actual_length;
  }

  remaining -= 4;
  if (remaining <= 0) return actual_length;
  writeint (c, 0, nb_fields);
  c += 4;

  for (comment = oggz_comment_first (oggz, serialno); comment;
       comment = oggz_comment_next (oggz, serialno, comment)) {

    field_length = oggz_comment_len (comment->name);     /* [size]"name" */
    if (comment->value)
      field_length += 1 + oggz_comment_len (comment->value); /* "=value" */

    remaining -= 4;
    if (remaining <= 0) return actual_length;
    writeint (c, 0, field_length);
    c += 4;

    field_length = oggz_comment_len (comment->name);
    memcpy (c, comment->name, MIN (field_length, remaining));
    c += field_length; remaining -= field_length;
    if (remaining <= 0) return actual_length;

    if (comment->value) {
      remaining --;
      if (remaining <= 0) return actual_length;
      *c = '=';
      c++;

      field_length = oggz_comment_len (comment->value);
      memcpy (c, comment->value, MIN (field_length, remaining));
      c += field_length; remaining -= field_length;
      if (remaining <= 0) return actual_length;
    }
  }

  if (remaining <= 0) return actual_length;
  *c = 0x01;

  return actual_length;
}

/* NB. Public use of this function is deprecated; the simpler
 * oggz_comments_generate() automatically determines the packet_type */
ogg_packet *
oggz_comment_generate(OGGZ * oggz, long serialno,
		      OggzStreamContent packet_type,
		      int FLAC_final_metadata_block)
{
  ogg_packet *c_packet;

  unsigned char *buffer;
  unsigned const char *preamble;
  long preamble_length, comment_length, buf_size;

  /* Some types use preambles in the comment packet. FLAC is notable;
     n9-32 should contain the length of the comment data as 24bit unsigned
     BE, and the first octet should be ORed with 0x80 if this is the last
     (only) metadata block. Any user doing FLAC has to know how to do the
     encapsulation anyway. */
  const unsigned char preamble_vorbis[7] =
    {0x03, 0x76, 0x6f, 0x72, 0x62, 0x69, 0x73};
  const unsigned char preamble_theora[7] =
    {0x81, 0x74, 0x68, 0x65, 0x6f, 0x72, 0x61};
  const unsigned char preamble_flac[4] =
    {0x04, 0x00, 0x00, 0x00};
  const unsigned char preamble_kate[9] =
    {0x81, 0x6b, 0x61, 0x74, 0x65, 0x00, 0x00, 0x00, 0x00};


  switch(packet_type) {
    case OGGZ_CONTENT_VORBIS:
      preamble_length = sizeof preamble_vorbis;
      preamble = preamble_vorbis;
      break;
    case OGGZ_CONTENT_THEORA:
      preamble_length = sizeof preamble_theora;
      preamble = preamble_theora;
      break;
    case OGGZ_CONTENT_FLAC:
      preamble_length = sizeof preamble_flac;
      preamble = preamble_flac;
      break;
    case OGGZ_CONTENT_KATE:
      preamble_length = sizeof preamble_kate;
      preamble = preamble_kate;
      break;
    case OGGZ_CONTENT_PCM:
    case OGGZ_CONTENT_SPEEX:
      preamble_length = 0;
      preamble = 0;
      /* No preamble for these */
      break;
    default:
      return NULL;
  }

  comment_length = oggz_comments_encode (oggz, serialno, 0, 0);
  if(comment_length <= 0) {
    return NULL;
  }

  buf_size = preamble_length + comment_length;

  if(packet_type == OGGZ_CONTENT_FLAC && comment_length >= 0x00ffffff) {
    return NULL;
  }

  c_packet = oggz_malloc(sizeof *c_packet);
  if(c_packet) {
    memset(c_packet, 0, sizeof *c_packet);
    c_packet->packetno = 1;
    c_packet->packet = oggz_malloc(buf_size);
  }

  if(c_packet && c_packet->packet) {
    buffer = c_packet->packet;
    if(preamble_length) {
      memcpy(buffer, preamble, preamble_length);
      if(packet_type == OGGZ_CONTENT_FLAC) {
	/* Use comment_length-1 as we will be stripping the Vorbis
	   framing byte. */
	/* MACRO */
	writeint24be(c_packet->packet, 1, (comment_length-1) );
	if(FLAC_final_metadata_block) 
	  {
	    c_packet->packet[0] |= 0x80;
	  }
      }
      buffer += preamble_length;
    }
    oggz_comments_encode (oggz, serialno, buffer, comment_length);
    c_packet->bytes = buf_size;
    /* The framing byte for Vorbis shouldn't affect any of the other
       types, but strip it anyway. */
    if(packet_type != OGGZ_CONTENT_VORBIS)
      {
	c_packet->bytes -= 1;
      }
  } else {
    oggz_free(c_packet);
    c_packet = 0;
  }

  return c_packet;
}

/* In Flac, OggPCM, Speex, Theora Vorbis, and Kate the comment packet will
   be second in the stream, i.e. packetno=1, and it will have granulepos=0 */
ogg_packet *
oggz_comments_generate(OGGZ * oggz, long serialno,
		      int FLAC_final_metadata_block)
{
  OggzStreamContent packet_type;

  packet_type = oggz_stream_get_content (oggz, serialno);

  return oggz_comment_generate (oggz, serialno, packet_type,
                                FLAC_final_metadata_block);
}

void oggz_packet_destroy(ogg_packet *packet) {
  if(packet) {
    if(packet->packet)
      {
	oggz_free(packet->packet);
      }
    oggz_free(packet);
  }
  return;
}
