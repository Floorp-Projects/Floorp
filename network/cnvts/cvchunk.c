/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/* Please leave outside of ifdef for windows precompiled headers */
#include "xp.h"
#include "plstr.h"
#include "prmem.h"
#include "netutils.h"
#include "mkselect.h"
#include "mktcp.h"
#include "mkgeturl.h"

#ifdef MOZILLA_CLIENT

#include "cvchunk.h"	/* prototype */

extern int MK_OUT_OF_MEMORY;

typedef enum {
	FIND_CHUNK_SIZE,
	READ_CHUNK,
    STRIP_CRLF,
    PARSE_FOOTER
} States;

typedef struct _DataObject {
	NET_StreamClass *next_stream;
	char *in_buf;
	uint32 in_buf_size;
	uint32 chunk_size;
	uint32 amount_of_chunk_parsed;
	States cur_state;
    FO_Present_Types format_out;
    MWContext *context;
    URL_Struct *URL_s;
} DataObject;


/* unchunk the message and return MK_MULTIPART_MESSAGE_COMPLETED
 * when end detected
 */
PRIVATE int net_ChunkedWrite (NET_StreamClass *stream, char* s, int32 l)
{
	DataObject *obj=stream->data_object;	
    BlockAllocCat(obj->in_buf, obj->in_buf_size, s, l);
    if(!obj->in_buf)
    	return MK_OUT_OF_MEMORY;
    obj->in_buf_size += l;

	while(obj->in_buf_size > 0)
	{
		if(obj->cur_state == FIND_CHUNK_SIZE)
		{
			char *line_feed;
            char *semicolon;
            char *end;

			/* we don't have a current chunk size 
	 	 	 * look for a new chunk size 
		 	 *
		 	 * make sure the line has a CRLF
	 	 	 */
			if((line_feed = PL_strnchr(obj->in_buf, LF, obj->in_buf_size)) == NULL)
			{
				return 1;  /* need more data */
			}
			
            *line_feed = '\0';

            semicolon = PL_strnchr(obj->in_buf, ';', line_feed-obj->in_buf);

            if(semicolon)
                *semicolon = '\0';

            end = semicolon ? semicolon : line_feed;

			/* read the first integer and ignore any thing
		 	 * else on the line.  Extensions are allowed
		 	 */
            obj->chunk_size = strtol(obj->in_buf, &end, 16);
	
			/* strip everything up to the line feed */
			obj->in_buf_size -= (line_feed+1) - obj->in_buf;
            if(obj->in_buf_size)
                memmove(obj->in_buf, 
				   	    line_feed+1, 
				   	    obj->in_buf_size);
	
			if(obj->chunk_size == 0)
			{
				/* the stream should be done now */
			    obj->cur_state = PARSE_FOOTER;
			}
            else
            {
			    obj->cur_state = READ_CHUNK;
            }

		}
		else if(obj->cur_state == READ_CHUNK)
		{
			uint32 data_size;
			int32 status;

			/* take as much data as we have and push it up the stream
			 */

			data_size = MIN(obj->in_buf_size, obj->chunk_size-obj->amount_of_chunk_parsed);

			status = (obj->next_stream->put_block)(obj->next_stream,
										  		   obj->in_buf,
										  		   data_size);
			if(status < 0)
				return status;

			/* remove the part that has been pushed */
			obj->in_buf_size -= data_size;
            if(obj->in_buf_size)
			    memmove(obj->in_buf, 
				   	    obj->in_buf+data_size, 
				   	    obj->in_buf_size);

			obj->amount_of_chunk_parsed += data_size;
	
			if(obj->amount_of_chunk_parsed >= obj->chunk_size)
			{
				PR_ASSERT(obj->amount_of_chunk_parsed == obj->chunk_size);
				/* reinit */
				obj->amount_of_chunk_parsed = 0;
				obj->cur_state = STRIP_CRLF;
			}
		}
        else if(obj->cur_state == STRIP_CRLF)
        {
            if(obj->in_buf_size > 1 && obj->in_buf[0] == CR && obj->in_buf[1] == LF)
            {
                /* strip two bytes */ 
                obj->in_buf_size -= 2;
                if(obj->in_buf_size)
                    memmove(obj->in_buf, 
				   	        obj->in_buf+2, 
				   	        obj->in_buf_size);
                obj->cur_state = FIND_CHUNK_SIZE;
            }
            else if(obj->in_buf[0] == LF)
            {
                /* strip one bytes */ 
                obj->in_buf_size -= 1;
                if(obj->in_buf_size)
                    memmove(obj->in_buf, 
				   	        obj->in_buf+1, 
				   	        obj->in_buf_size);
                obj->cur_state = FIND_CHUNK_SIZE;
            }
            else
            {
                if(obj->in_buf_size >= 2)
                {
                    int status;
                    
                    /* a fatal parse error */
                    PR_ASSERT(0);

                    /* just spew the buf to the screen */
           			status = (obj->next_stream->put_block)(obj->next_stream,
										  		   obj->in_buf,
										  		   obj->in_buf_size);
			        if(status < 0)
				        return status;

			        /* remove the part that has been pushed */
			        obj->in_buf_size = 0;
                }
            }
        }
        else if(obj->cur_state == PARSE_FOOTER)
        {
            char *line_feed;
            char *value;

            /* parse until we see two CRLF's in a row */
            if((line_feed = PL_strnchr(obj->in_buf, LF, obj->in_buf_size)) == NULL)
			{
				return 1;  /* need more data */
			}

            *line_feed = '\0';

            /* strip the CR */
            if(line_feed != obj->in_buf && *(line_feed-1) == CR)
                *(line_feed-1) = '\0';
		
            if(*obj->in_buf == '\0')
            {
                /* end of parse stream */
    			return MK_MULTIPART_MESSAGE_COMPLETED;
            }

            /* names are separated from values with a colon
            */
            value = PL_strchr(obj->in_buf, ':');
            if(value)
                value++;

            /* otherwise parse the line as a mime header */
            NET_ParseMimeHeader(obj->format_out,
                                obj->context,
                                obj->URL_s,
                                obj->in_buf,
                                value,
								FALSE);

            /* strip the line from the buffer */
			obj->in_buf_size -= (line_feed+1) - obj->in_buf;
            if(obj->in_buf_size)
			    memmove(obj->in_buf, 
				   	    line_feed+1, 
				   	    obj->in_buf_size);
        }
	}
	
	PR_ASSERT(obj->in_buf_size == 0);

    return(1);
}

/* is the stream ready for writeing?
 */
PRIVATE unsigned int net_ChunkedWriteReady (NET_StreamClass * stream)
{
   DataObject *obj=stream->data_object;
   return (*obj->next_stream->is_write_ready)(obj->next_stream);
}


PRIVATE void net_ChunkedComplete (NET_StreamClass *stream)
{
	DataObject *obj=stream->data_object;	
	(*obj->next_stream->complete)(obj->next_stream);

    PR_Free(obj);
    return;
}

PRIVATE void net_ChunkedAbort (NET_StreamClass *stream, int status)
{
	DataObject *obj=stream->data_object;	
	(*obj->next_stream->abort)(obj->next_stream, status);

    return;
}


MODULE_PRIVATE NET_StreamClass * 
NET_ChunkedDecoderStream (int         format_out,
                         void       *data_obj,
                         URL_Struct *URL_s,
                         MWContext  *window_id)
{
    DataObject* obj;
    NET_StreamClass* stream;
    
    TRACEMSG(("Setting up display stream. Have URL: %s\n", URL_s->address));

    stream = PR_NEW(NET_StreamClass);
    if(stream == NULL) 
        return(NULL);

    obj = PR_NEWZAP(DataObject);
    if (obj == NULL) 
        return(NULL);
    
    stream->name           = "Chunked decoder";
    stream->complete       = (MKStreamCompleteFunc) net_ChunkedComplete;
    stream->abort          = (MKStreamAbortFunc) net_ChunkedAbort;
    stream->put_block      = (MKStreamWriteFunc) net_ChunkedWrite;
    stream->is_write_ready = (MKStreamWriteReadyFunc) net_ChunkedWriteReady;
    stream->data_object    = obj;  /* document info object */
    stream->window_id      = window_id;

    /* clear the "chunked" encoding */
    if(URL_s->transfer_encoding)
    {
    	PR_FREEIF(URL_s->transfer_encoding);
    	URL_s->transfer_encoding = NULL;
    }
    else
    {
    	PR_FREEIF(URL_s->content_encoding);
    	URL_s->content_encoding = NULL;
    }
    obj->next_stream = NET_StreamBuilder(format_out, URL_s, window_id);

	if(!obj->next_stream)
	{
		PR_Free(obj);
		PR_Free(stream);
		return NULL;
	}

    obj->context = window_id;
    obj->format_out = format_out;
    obj->URL_s = URL_s;

    TRACEMSG(("Returning stream from NET_ChunkedConverter\n"));

    return stream;
}

#endif /* MOZILLA_CLIENT */
