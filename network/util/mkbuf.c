/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */


/*  mkbuf.c --- a netlib stream that buffers data before passing it along.
    This is used by libmime and libmsg to undo the effects of line-buffering,
    and to pass along larger chunks of data to various stream handlers (e.g.,
    layout.)

   This stuff used to be in libmsg/msgutils.c, but it's really not
   mail-specific.  And I had to move it to get libmime to work in the
   absence of libmsg, so in the meantime, I changed the prefix from MSG_
   to NET_.

    This depends on XP_ReBuffer (xp_linebuf.c) to do its dirty work.

    Created: Jamie Zawinski <jwz@mozilla.org>,  8-Aug-98.
*/

#include "xp_mcom.h"
#include "net.h"
#include "xp_linebuf.h"
#include "mkbuf.h"

struct net_rebuffering_stream_data
{
  uint32 desired_size;
  char *buffer;
  uint32 buffer_size;
  uint32 buffer_fp;
  NET_StreamClass *next_stream;
};

#undef FREEIF
#define FREEIF(obj) do { if (obj) { XP_FREE (obj); obj = 0; }} while (0)


static int32
net_rebuffering_stream_write_next_chunk (char *buffer, uint32 buffer_size,
                                         void *closure)
{
  struct net_rebuffering_stream_data *sd =
    (struct net_rebuffering_stream_data *) closure;
  XP_ASSERT (sd);
  if (!sd) return -1;
  if (!sd->next_stream) return -1;
  return (*sd->next_stream->put_block) (sd->next_stream,
                                        buffer, buffer_size);
}

static int
net_rebuffering_stream_write_chunk (NET_StreamClass *stream,
                                    const char* net_buffer,
                                    int32 net_buffer_size)
{
  struct net_rebuffering_stream_data *sd =
    (struct net_rebuffering_stream_data *) stream->data_object;  
  XP_ASSERT (sd);
  if (!sd) return -1;
  return XP_ReBuffer (net_buffer, net_buffer_size,
                      sd->desired_size,
                      &sd->buffer, &sd->buffer_size, &sd->buffer_fp,
                      net_rebuffering_stream_write_next_chunk,
                      sd);
}


/* ValidateDocData() isn't implemented anywhere, it used to be part
   of the old parser in lib/libparse/pa_parse.c.  There are 8+
   instances of this function being stubbed out, let's stop the
   insanity and put the stub here, since net_rebuffering_stream_abort()
   seems to be the only caller of ValidateDocData(). */
XP_Bool ValidateDocData(MWContext *window_id)
{
  return PR_TRUE;
}

static void
net_rebuffering_stream_abort (NET_StreamClass *stream, int status)
{
  struct net_rebuffering_stream_data *sd =
    (struct net_rebuffering_stream_data *) stream->data_object;  
  if (!sd) return;
  FREEIF (sd->buffer);
  if (sd->next_stream)
    {
      if (ValidateDocData(sd->next_stream->window_id)) /* otherwise doc_data
                                                          is gone !  */
        (*sd->next_stream->abort) (sd->next_stream, status);
      XP_FREE (sd->next_stream);
    }
  XP_FREE (sd);
}

static void
net_rebuffering_stream_complete (NET_StreamClass *stream)
{
  struct net_rebuffering_stream_data *sd =
    (struct net_rebuffering_stream_data *) stream->data_object;  
  if (!sd) return;
  sd->desired_size = 0;
  net_rebuffering_stream_write_chunk (stream, "", 0);
  FREEIF (sd->buffer);
  if (sd->next_stream)
    {
      (*sd->next_stream->complete) (sd->next_stream);
      XP_FREE (sd->next_stream);
    }
  XP_FREE (sd);
}

static unsigned int
net_rebuffering_stream_write_ready (NET_StreamClass *stream)
{
  struct net_rebuffering_stream_data *sd =
    (struct net_rebuffering_stream_data *) stream->data_object;  
  if (sd && sd->next_stream)
    return ((*sd->next_stream->is_write_ready)
            (sd->next_stream));
  else
    return (MAX_WRITE_READY);
}

NET_StreamClass * 
NET_MakeRebufferingStream (NET_StreamClass *next_stream,
                           URL_Struct *url,
                           MWContext *context)
{
  NET_StreamClass *stream;
  struct net_rebuffering_stream_data *sd;

  XP_ASSERT (next_stream);

  /*  TRACEMSG(("Setting up rebuffering stream. Have URL: %s\n", url->address)); */

  stream = XP_NEW (NET_StreamClass);
  if (!stream) return 0;

  sd = XP_NEW (struct net_rebuffering_stream_data);
  if (! sd)
    {
      XP_FREE (stream);
      return 0;
    }

  XP_MEMSET (sd, 0, sizeof(*sd));
  XP_MEMSET (stream, 0, sizeof(*stream));

  sd->next_stream = next_stream;
  sd->desired_size = 10240;

  stream->name           = "ReBuffering Stream";
  stream->complete       = net_rebuffering_stream_complete;
  stream->abort          = net_rebuffering_stream_abort;
  stream->put_block      = net_rebuffering_stream_write_chunk;
  stream->is_write_ready = net_rebuffering_stream_write_ready;
  stream->data_object    = sd;
  stream->window_id      = context;

  return stream;
}
