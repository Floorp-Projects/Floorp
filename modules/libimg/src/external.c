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


#include "if.h"
extern PRBool
il_load_image(void *cx, char *image_url, NET_ReloadMethod cache_reload_policy);

#include "merrors.h"
/* for XP_GetString() */
#include "xpgetstr.h"


#include "il_strm.h"            /* Stream converters. */

static unsigned int
il_view_write_ready(NET_StreamClass *stream)
{
	il_container *ic = (il_container *)stream->data_object;	

    /* For some reason, the imagelib can't deliver the image.
       Trigger il_view_write(), which will abort the stream. */

	/* This originally returned (ic!=0) to trigger il_view_write()
	which would set the read_size to something reasonable. But in mkmailbox.c
	ReadMessageSock() uses the return value of one as the read_size and does not
	call il_view_write() until it has filled up the buffer one byte
	at a time. This should be addressed correctly in later versions. */

    return (ic != 0) * MAX_WRITE_READY;
}

/* Abort the stream if we get this far */
static int
il_view_write(NET_StreamClass *stream, const unsigned char *str, int32 len)
{
	void *dobj=stream->data_object;	
    /* If this assert fires, chances are that the provided URL was malformed.*/
    XP_ASSERT(dobj == (void*)1);

     /* Should be MK_DATA_LOADED, but netlib ignores that. */
	return MK_INTERRUPTED;
}

static void
il_view_complete(NET_StreamClass *stream)  
{	
}

static void 
il_view_abort(NET_StreamClass *stream, int status)
{	
}

/* there can be only one, highlander */
static IL_Stream *unconnected_stream = 0;
static IL_URL *unconnected_urls = 0;

void
il_reconnect(il_container *ic)
{
	if (unconnected_stream)
	{

		unconnected_stream->complete       = il_stream_complete;
		unconnected_stream->abort          = il_abort;
		unconnected_stream->is_write_ready = il_write_ready;
		unconnected_stream->data_object    = (void *)ic;
		unconnected_stream->put_block      = (MKStreamWriteFunc)il_first_write;

		ic->type = IL_UNKNOWN;
		ic->stream = unconnected_stream;
		ic->state = IC_STREAM;

		unconnected_urls->fe_data = ic;	/* ugh */
		ic->content_length = unconnected_urls->content_length;

		unconnected_stream = 0;
		unconnected_urls = 0;
	}
}

/* We aren't going to reconnect after all.  Cause the stream to abort */
void
il_abort_reconnect()
{
    if (unconnected_stream) {
        unconnected_stream->data_object = (void *)1;

	unconnected_stream = 0;
	unconnected_urls = 0;
    }
}

static char fakehtml[] = "<IMG SRC=\"%s\">";

NET_StreamClass *
IL_ViewStream(FO_Present_Types format_out, void *newshack, URL_Struct *urls,
              OPAQUE_CONTEXT *cx)
{
    IL_Stream *stream = nil, *viewstream;
	il_container *ic = nil;
	char *org_content_type;
    char *image_url;

	/* multi-part reconnect hack */

	ic = (il_container*)urls->fe_data;
	if(ic && ic->multi)
	{
		return IL_NewStream(format_out, IL_UNKNOWN, urls, cx);
	}

	/* Create stream object */
    if (!(stream = XP_NEW_ZAP(NET_StreamClass))) {
		XP_TRACE(("il: IL_ViewStream memory lossage"));
		return 0;
	}
	
    stream->name           = "image view";
    stream->complete       = il_view_complete;
    stream->abort          = il_view_abort;
    stream->is_write_ready = il_view_write_ready;
    stream->data_object    = NULL;
    stream->window_id      = cx;
    stream->put_block      = (MKStreamWriteFunc)il_view_write;

	ILTRACE(0,("il: new view stream, %s", urls->address));

	XP_ASSERT(!unconnected_stream);
	unconnected_stream = stream;
	unconnected_urls = urls;

	if(!newshack)
	{
        char *buffer;

		org_content_type = urls->content_type; 
		urls->content_type = 0;
		StrAllocCopy(urls->content_type, TEXT_HTML);
		urls->is_binary = 1;	/* secret flag for mail-to save as */

		/* Force layout to discard the old document and start a new one.
		   We do this so that the pre-fetched image request won't be
		   destroyed by a layout call to IL_DestroyImageGroup. */

		viewstream = NET_StreamBuilder(format_out, urls, cx);
		if (!viewstream) {
			XP_FREE(stream);
			return NULL;
		}
        buffer = XP_STRDUP("<HTML>");
        if (!buffer) {
            XP_FREE(stream);
            XP_FREE(viewstream);
            return NULL;
        }
		(*viewstream->put_block)(viewstream, buffer,
                                 XP_STRLEN(buffer)+1);
        XP_FREE(buffer);

	} /* !newshack */

	/* Prefetch the image.  We do this so that the image library can
	   process image data even if the parser is blocked on the fake IMG
	   tag that we send.  Note that this image request will persist until
	   the document is destroyed (when IL_DestroyImageGroup will be called.) */
    image_url = (char*) XP_ALLOC(XP_STRLEN(urls->address) + 29);
    if (!image_url) {
        XP_FREE(stream);
        XP_FREE(viewstream);
        return NULL;
    }
    XP_SPRINTF(image_url, "internal-external-reconnect:%s", urls->address);
    if (!il_load_image(cx, image_url, urls->force_reload)) {
        XP_FREE(stream);
        XP_FREE(viewstream);
        return NULL;
    }
    XP_FREE(image_url);

	if (!newshack) {
	    if (viewstream) {
            	char *buffer = (char*)
                XP_ALLOC(XP_STRLEN(fakehtml) + XP_STRLEN(urls->address) + 1);

            if (buffer)
            {
                XP_SPRINTF(buffer, fakehtml, urls->address);
                (*viewstream->put_block)(viewstream,
                                         buffer, XP_STRLEN(buffer));
                XP_FREE(buffer);
            }
			(*viewstream->complete)(viewstream);
		}

		/* this has to be set back for abort to work correctly */
		XP_FREE(urls->content_type);
		urls->content_type = org_content_type;
	} /* !newshack */

    return stream;
}

