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
#include "prmem.h"
#include "netutils.h"
#include "mkselect.h"
#include "mktcp.h"
#include "mkgeturl.h"
#include "cvunzip.h"

#ifdef MOZILLA_CLIENT

#include "mkstream.h"
#include "zlib.h"

extern int MK_OUT_OF_MEMORY;
extern int MK_BAD_GZIP_HEADER;

typedef struct _DataObject {
	NET_StreamClass *next_stream;
	z_stream d_stream;  /* decompression stream */
	unsigned char *dcomp_buf;
	uint32 dcomp_buf_size;
	PRBool is_done;
	PRBool checking_crc_footer;
    unsigned char *incoming_buf;
    uint32 incoming_buf_size;
    PRBool header_skipped;
    URL_Struct *URL_s;
	uint32 crc_check;
} DataObject;

#define DECOMP_BUF_SIZE NET_Socket_Buffer_Size*2

enum check_header_response {
    HEADER_OK,
    BAD_HEADER,
    NEED_MORE_HEADER
};

static uint32 gz_magic[2] = {0x1f, 0x8b}; /* gzip magic header */

/* gzip flag byte */
#define ASCII_FLAG   0x01 /* bit 0 set: file probably ascii text */
#define HEAD_CRC     0x02 /* bit 1 set: header CRC present */
#define EXTRA_FIELD  0x04 /* bit 2 set: extra field present */
#define ORIG_NAME    0x08 /* bit 3 set: original file name present */
#define COMMENT      0x10 /* bit 4 set: file comment present */
#define RESERVED     0xE0 /* bits 5..7: reserved */

/* code copied from Zlib please see zlib.h for copyright statement */
/* ===========================================================================
      Check the gzip header of a gz_stream opened for reading. Set the stream
    mode to transparent if the gzip magic header is not present; set s->err
    to Z_DATA_ERROR if the magic header is present but the rest of the header
    is incorrect.
    IN assertion: the stream s has already been created sucessfully;
       s->stream.avail_in is zero for the first time, but may be non-zero
       for concatenated .gz files.
*/
static enum check_header_response
check_header(unsigned char *header, uint32 header_length, uint32 *actual_header_size)
{
    int method; /* method byte */
    int flags;  /* flags byte */
    uInt len;
    uint32 c;
    uint32 orig_header_size = header_length;

    /* header must be at least 10 bytes */
    if(header_length < 10)
        return NEED_MORE_HEADER;

    /* Check the gzip magic header */
    for (len = 0; len < 2; len++) {
	    c = (uint32) *header++;
        header_length--;
	    if (c != gz_magic[len]) {
	        return BAD_HEADER;
	    }
    }
    method = *header++;
    header_length--;
    flags = *header++;
    header_length--;
    if (method != Z_DEFLATED || (flags & RESERVED) != 0) {
	    return BAD_HEADER;
    }

    /* Discard time, xflags and OS code: */
    for (len = 0; len < 6; len++) 
    {
        header++;
        header_length--;
    }

    /* OK we now passed the safe 10 byte boundary, we need to check from here
     * on out to make sure we have enough data
     */

    if ((flags & EXTRA_FIELD) != 0) { /* skip the extra field */
        if(header_length < 2)
            return NEED_MORE_HEADER;

	    len  =  (uInt)*header++;
        header_length--;
	    len += ((uInt)*header++)<<8;
        header_length--;

        /* len is garbage if EOF but the loop below will quit anyway */

        if(header_length < len)
            return NEED_MORE_HEADER;

	    while (len-- != 0) 
        {
            header++;
            header_length--;
        }
    }
    if ((flags & ORIG_NAME) != 0) { /* skip the original file name */

        if(header_length < 1)
            return NEED_MORE_HEADER;

	    while (*header != '\0')
        {
            header++;
            header_length--;

            if(header_length == 0)
                return NEED_MORE_HEADER;
        }

        /* skip null byte */
        header++;
        header_length--;
    }
    if ((flags & COMMENT) != 0) {   /* skip the .gz file comment */

        if(header_length < 1)
            return NEED_MORE_HEADER;

	    while (*header != '\0')
        {
            header++;
            header_length--;

            if(header_length == 0)
                return NEED_MORE_HEADER;
        }

        /* skip null byte */
        header++;
        header_length--;

    }
    if ((flags & HEAD_CRC) != 0) {  /* skip the header crc */

        if(header_length < 2)
            return NEED_MORE_HEADER;

	    for (len = 0; len < 2; len++)
        {
            header++;
            header_length--;
        }           
    }

    *actual_header_size = orig_header_size - header_length;

    return HEADER_OK;
}

PRIVATE int
do_end_crc_check(DataObject *obj)
{

	if(obj->incoming_buf_size >= 8)
	{
		uint32 crc_int;
		uint32 size_int;

        obj->checking_crc_footer = PR_FALSE;
        obj->is_done = PR_TRUE;

		crc_int = (uint32)obj->incoming_buf[0];
		crc_int += (uint32)obj->incoming_buf[1]<<8;
		crc_int += (uint32)obj->incoming_buf[2]<<16;
		crc_int += (uint32)obj->incoming_buf[3]<<24;

		size_int = (uint32)obj->incoming_buf[4];
		size_int += (uint32)obj->incoming_buf[5]<<8;
		size_int += (uint32)obj->incoming_buf[6]<<16;
		size_int += (uint32)obj->incoming_buf[7]<<24;

		if(obj->crc_check != crc_int
           || obj->d_stream.total_out != size_int)
		{
			/* crc or size checksum failure */
            obj->URL_s->error_msg = NET_ExplainErrorDetails(MK_BAD_GZIP_HEADER);
            return MK_BAD_GZIP_HEADER;
		}

        return 1;
	}

    return 0; /* need more data */
}

PRIVATE int net_UnZipWrite (NET_StreamClass *stream, CONST char* s, int32 l)
{
    int err;
    uint32 prev_total_out;
    uint32 new_data_total_out;
    uint32 input_used_up, input_left_over;
	char * tempPtr = NULL;
	DataObject *obj=stream->data_object;	
    if(obj->is_done) 
    {
		/* multipart gzip? */
        PR_ASSERT(0);
		return (1);
    }
	
    BlockAllocCat(  tempPtr, obj->incoming_buf_size, s, l);
     obj->incoming_buf = (unsigned char*)tempPtr;

    if(!obj->incoming_buf)
        return MK_OUT_OF_MEMORY;
    obj->incoming_buf_size += l;

    /* parse and skip the header */
    if(!obj->header_skipped)
    {
        uint32 actual_header_size;
        enum check_header_response status;

        status = check_header((unsigned char *)obj->incoming_buf, obj->incoming_buf_size, &actual_header_size);

        if(status == HEADER_OK)
        {
            /* squash the header */
			obj->incoming_buf_size -= actual_header_size;
            memmove(obj->incoming_buf, 
                       obj->incoming_buf+actual_header_size, 
                       obj->incoming_buf_size);

            obj->header_skipped = PR_TRUE;
        }
        else if(status == BAD_HEADER)
        {
            obj->URL_s->error_msg = NET_ExplainErrorDetails(MK_BAD_GZIP_HEADER);
            return MK_BAD_GZIP_HEADER;
        }
        else if(status == NEED_MORE_HEADER)
        {
            return 1;
        }
        else
        {
            PR_ASSERT(0);
            return 1;
        }
    }
	else if(obj->checking_crc_footer)
	{
        return do_end_crc_check(obj);
	}

    obj->d_stream.next_in  = (unsigned char *)obj->incoming_buf;
    obj->d_stream.avail_in = obj->incoming_buf_size;
    obj->d_stream.next_out = (unsigned char *)obj->dcomp_buf;
    obj->d_stream.avail_out = obj->dcomp_buf_size;

	if(obj->d_stream.avail_in <= 0)
		return 1;  /* wait for more data */

    prev_total_out = obj->d_stream.total_out;

    /* need to loop to finish for small output bufs */
    while(1)
    {
        err = inflate(&obj->d_stream, Z_NO_FLUSH);

        /* the amount of new uncompressed data is: */
        new_data_total_out = obj->d_stream.total_out - prev_total_out;

        if(new_data_total_out > 0)
		{
            obj->crc_check = crc32(obj->crc_check, 
								   obj->dcomp_buf, 
								   new_data_total_out);

    	    (*obj->next_stream->put_block)(obj->next_stream, 
				     (char *)  obj->dcomp_buf, 
				       new_data_total_out);

			
		}

        obj->d_stream.avail_out = obj->dcomp_buf_size;
        obj->d_stream.next_out = (unsigned char *)obj->dcomp_buf;
        prev_total_out = obj->d_stream.total_out;

        if(err == Z_STREAM_END)
        {
    	    obj->checking_crc_footer = PR_TRUE;
            break;
        }
        else if(err != Z_OK)
        {
            /* need to get more data on next pass 
             * @@@ should check for more critical errors
             */
            break;
        }
        else if(obj->d_stream.avail_in <= 0)
        {
            /* need more data */
            break;
        }

    }

    /* remove the part that has already been decoding from the incoming buf */
    input_left_over = obj->d_stream.avail_in;

    if(input_left_over > 0)
    {
        input_used_up = obj->incoming_buf_size - input_left_over; 
        memmove(obj->incoming_buf, obj->incoming_buf+input_used_up, input_left_over);
        obj->incoming_buf_size = input_left_over;
    }
    else
    {
        obj->incoming_buf_size = 0;
    }

    if(obj->checking_crc_footer == PR_TRUE)
    {
        return do_end_crc_check(obj);
    }

    return(1);
}

/* is the stream ready for writeing?
 */
PRIVATE unsigned int net_UnZipWriteReady (NET_StreamClass * stream)
{
   DataObject *obj=stream->data_object;   
   return((*obj->next_stream->is_write_ready)(obj->next_stream));  /* always ready for writing */ 
}


PRIVATE void net_UnZipComplete (NET_StreamClass *stream)
{
	DataObject *obj=stream->data_object;
    int err;	

    (*obj->next_stream->complete)(obj->next_stream);

    err = inflateEnd(&(obj->d_stream));
    PR_ASSERT(err == Z_OK);

    if(!obj->is_done)
    {
        /* we didn't complete the crc and size checks */
        /* @@@ not sure what to do here yet */
        PR_ASSERT(0);
    }

    PR_Free(obj->dcomp_buf);
    PR_Free(obj);
    return;
}

PRIVATE void net_UnZipAbort (NET_StreamClass *stream, int status)
{
	DataObject *obj=stream->data_object;
    int err;	

    (*obj->next_stream->abort)(obj->next_stream, status);

    err = inflateEnd(&(obj->d_stream));
    PR_ASSERT(err == Z_OK);

    PR_Free(obj->dcomp_buf);
    PR_Free(obj);
    return;
}


PUBLIC NET_StreamClass * 
NET_UnZipConverter (int         format_out,
                         void       *data_obj,
                         URL_Struct *URL_s,
                         MWContext  *window_id)
{
    DataObject* obj;
    NET_StreamClass* stream;
    int err;
    
    TRACEMSG(("Setting up display stream. Have URL: %s\n", URL_s->address));

    stream = PR_NEW(NET_StreamClass);
    if(stream == NULL) 
        return(NULL);

    obj = PR_NEWZAP(DataObject);
    if (obj == NULL) 
    {
        PR_Free(stream);
        return(NULL);
    }
    
    stream->name           = "UnZiper";
    stream->complete       = (MKStreamCompleteFunc) net_UnZipComplete;
    stream->abort          = (MKStreamAbortFunc) net_UnZipAbort;
    stream->put_block      = (MKStreamWriteFunc) net_UnZipWrite;
    stream->is_write_ready = (MKStreamWriteReadyFunc) net_UnZipWriteReady;
    stream->data_object    = obj;  /* document info object */
    stream->window_id      = window_id;

    obj->dcomp_buf = PR_Malloc(DECOMP_BUF_SIZE);
    obj->dcomp_buf_size = DECOMP_BUF_SIZE;

    if(!obj->dcomp_buf)
    {
	PR_Free(stream);
	PR_Free(obj);
	return NULL;
    }

    obj->URL_s = URL_s;

    obj->d_stream.zalloc = (alloc_func)0;
    obj->d_stream.zfree = (free_func)0;
    obj->d_stream.opaque = (voidpf)0;

    err = inflateInit2(&obj->d_stream, -15);

    if(err != Z_OK)
    {
	PR_Free(stream);
	PR_Free(obj);
	return NULL;
    }

    /* create the next stream, but strip the compressed encoding */
    PR_FREEIF(URL_s->content_encoding);
    URL_s->content_encoding = NULL;
    obj->next_stream = NET_StreamBuilder(format_out, URL_s, window_id);

    if(!obj->next_stream)
    {
	inflateEnd(&obj->d_stream);
	PR_Free(stream);
	PR_Free(obj);
	return NULL;
    }

    TRACEMSG(("Returning stream from NET_UnZipConverter\n"));

    return stream;
}

#endif /* MOZILLA_CLIENT */
