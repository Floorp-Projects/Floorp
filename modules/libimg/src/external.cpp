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
#include "dummy_nc.h"

extern PRBool
il_load_image(MWContext *cx, char *image_url, NET_ReloadMethod cache_reload_policy);

#include "merrors.h"
#ifdef STANDALONE_IMAGE_LIB
#include "xpcompat.h"
#else
/* for XP_GetString() */
#include "xpgetstr.h"
#endif

#include "il_strm.h"            /* Stream converters. */

static unsigned int
il_view_write_ready(NET_StreamClass *stream)
{
    ilINetReader *reader = (ilINetReader *)stream->data_object;

    /* For some reason, the imagelib can't deliver the image.
       Trigger il_view_write(), which will abort the stream. */

	/* This originally returned (ic!=0) to trigger il_view_write()
	which would set the read_size to something reasonable. But in mkmailbox.c
	ReadMessageSock() uses the return value of one as the read_size and does not
	call il_view_write() until it has filled up the buffer one byte
	at a time. This should be addressed correctly in later versions. */

    return (reader != 0) * MAX_WRITE_READY;
}

/* Abort the stream if we get this far */
static int
il_view_write(NET_StreamClass *stream, const unsigned char *str, int32 len)
{
	void *dobj=stream->data_object;	
    /* If this assert fires, chances are that the provided URL was malformed.*/
    PR_ASSERT(dobj == (void*)1);

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

void
il_stream_complete(NET_StreamClass *stream)
{
    ilINetReader *reader = (ilINetReader *)stream->data_object;
	
	reader->StreamComplete((PRBool)stream->is_multipart);
}

void 
il_abort(NET_StreamClass *stream, int status)
{
    ilINetReader *reader = (ilINetReader *)stream->data_object;

	reader->StreamAbort(status);
	stream->data_object = 0;
}

/* Only for internal-external-reconnect. */
void
il_stream_reconnect_complete(NET_StreamClass *stream)
{
    /* Get reader before calling il_stream_complete because it 
       may set stream->data_object to NULL. */     
    ilINetReader *reader = (ilINetReader *)stream->data_object;
    PR_ASSERT(reader);

    il_stream_complete(stream);

    NS_RELEASE(reader);
    stream->data_object = NULL;
}

/* Only for internal-external-reconnect. */
void
il_reconnect_abort(NET_StreamClass *stream, int status)
{
    /* Get reader before calling il_abort because it 
       may set stream->data_object to NULL. */     
    ilINetReader *reader = (ilINetReader *)stream->data_object;
    PR_ASSERT(reader);

    il_abort(stream,status);

    NS_RELEASE(reader);
    stream->data_object = NULL;
}
unsigned int
il_write_ready(NET_StreamClass *stream)
{
    ilINetReader *reader = (ilINetReader *)stream->data_object;

	return reader->WriteReady();

}

int 
il_write(NET_StreamClass *stream, const unsigned char *str, int32 len)
{
    ilINetReader *reader = (ilINetReader *)stream->data_object;

	return reader->Write(str, len);
}

int
il_first_write(NET_StreamClass *stream, const unsigned char *str, int32 len)
{
    ilINetReader *reader = (ilINetReader *)stream->data_object;
	int ret_val;

	ret_val = reader->FirstWrite(str, len);
	if (ret_val != 0) {
	    return ret_val;
	}
	
	stream->put_block = (MKStreamWriteFunc)il_write;
	/* do first write */
	return stream->put_block(stream, (const char*) str, len);
}

/* there can be only one, highlander */
static NET_StreamClass *unconnected_stream = 0;
static URL_Struct *unconnected_urls = 0;

void
il_reconnect(il_container *ic)
{
	if (unconnected_stream)
	{
        /* Will be freed in il_stream_reconnect_complete or il_reconnect_abort */
	    ilINetReader *reader = IL_NewNetReader(ic);

		if (reader != NULL) {
    		unconnected_stream->complete       = il_stream_reconnect_complete;
		    unconnected_stream->abort          = il_reconnect_abort;
		    unconnected_stream->is_write_ready = il_write_ready;
		    unconnected_stream->data_object    = (void *)reader;
			unconnected_stream->put_block      = (MKStreamWriteFunc)il_first_write;

			ic->type = IL_UNKNOWN;
			ic->state = IC_STREAM;

      /* unconnected_urls->fe_data no longer has a ponter to an ilIURL, 
         it wasn't used anyway. */
			ic->content_length = unconnected_urls->content_length;
		}

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
    NET_StreamClass *stream = nil, 
                    *viewstream = nil;
    il_container *ic = nil;
    ilINetReader *reader = nil;
    ilIURL *iurl;
    char *org_content_type;	
    char *image_url;

	/* multi-part reconnect hack */
	iurl = (ilIURL *)urls->fe_data;
	if (iurl) {
	    reader = iurl->GetReader();
		if(reader)
		{
#if 0
/* This change is not fully merged from the MODULAR_IMGLIB_BRANCH landing. */
    /* Extreme editor hack! This value is used when loading images
       so we use the converter we did in 4.06 code.
       If we don't, this code triggers parsing of the image URL,
       which has very bad effects in the editor! */
	if((urls && urls->owner_id == 0x000000ED) || (ic && ic->multi))
#endif
		    if(reader->IsMulti()) {
			    NS_RELEASE(reader);
				return IL_NewStream(format_out, IL_UNKNOWN, urls, cx);
			}
			NS_RELEASE(reader);
		}
	}

	/* Create stream object */
    if (!(stream = PR_NEWZAP(NET_StreamClass))) {
		ILTRACE(1,("il: IL_ViewStream memory lossage"));
		return 0;
	}
	
    stream->name           = "image view";
    stream->complete       = il_view_complete;
    stream->abort          = il_view_abort;
    stream->is_write_ready = il_view_write_ready;
    stream->data_object    = NULL;
    stream->window_id      = (MWContext *)cx;
    stream->put_block      = (MKStreamWriteFunc)il_view_write;

	ILTRACE(0,("il: new view stream, %s", urls->address));

	PR_ASSERT(!unconnected_stream);
    /* Note that this URL_Struct does not have a iURL wrapper around
       it anymore.  It doesn't need it and we wouldn't have any good place
       to free it anyway. */
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

		viewstream = NET_StreamBuilder(format_out, urls, (MWContext *)cx);
		if (!viewstream) {
			PR_FREEIF(stream);
			return NULL;
		}
        buffer = PL_strdup("<HTML>");
        if (!buffer) {
            PR_FREEIF(stream);
            PR_FREEIF(viewstream);
            return NULL;
        }
		(*viewstream->put_block)(viewstream, buffer,
                                 PL_strlen(buffer)+1);
        PR_FREEIF(buffer);

	} /* !newshack */

	/* Prefetch the image.  We do this so that the image library can
	   process image data even if the parser is blocked on the fake IMG
	   tag that we send.  Note that this image request will persist until
	   the document is destroyed (when IL_DestroyImageGroup will be called.) */
    image_url = (char*) PR_MALLOC(PL_strlen(urls->address) + 29);
    if (!image_url) {
        PR_FREEIF(stream);
        PR_FREEIF(viewstream);
        return NULL;
    }
    XP_SPRINTF(image_url, "internal-external-reconnect:%s", urls->address);
    if (!il_load_image((MWContext *)cx, image_url, urls->force_reload)) {
        PR_FREEIF(stream);
        PR_FREEIF(viewstream);
        return NULL;
    }
    PR_FREEIF(image_url);

	if (!newshack) {
	    if (viewstream) {
            	char *buffer = (char*)
                PR_MALLOC(PL_strlen(fakehtml) + PL_strlen(urls->address) + 1);

            if (buffer)
            {
                XP_SPRINTF(buffer, fakehtml, urls->address);
                (*viewstream->put_block)(viewstream,
                                         buffer, PL_strlen(buffer));
                PR_FREEIF(buffer);
            }
			(*viewstream->complete)(viewstream);
		}

		/* this has to be set back for abort to work correctly */
		PR_FREEIF(urls->content_type);
		urls->content_type = org_content_type;
	} /* !newshack */

    return stream;
}

NET_StreamClass *
IL_NewStream (FO_Present_Types format_out,
              void *type,
              URL_Struct *urls,
              OPAQUE_CONTEXT *cx)
{
	NET_StreamClass *stream = nil;
	il_container *ic = nil;
	ilINetReader *reader = nil;
	ilIURL *iurl = nil;
	
    /* recover the container */
	iurl = (ilIURL *)urls->fe_data;
    reader = iurl->GetReader();

	PR_ASSERT(reader);

	if (reader->StreamCreated(iurl, (int)type) == PR_FALSE) {
	    NS_RELEASE(reader); 
	    return NULL;
	}


	/* Create stream object */
	if (!(stream = PR_NEWZAP(NET_StreamClass))) 
	{
		ILTRACE(0,("il: MEM il_newstream"));
   	NS_RELEASE(reader);
		return 0;
	}

	stream->name		   = "image decode";
	stream->complete	   = il_stream_complete;
	stream->abort		   = il_abort;
	stream->is_write_ready = il_write_ready;
	stream->data_object	   = (void *)reader;
	stream->window_id	   = (MWContext *)cx;
	stream->put_block	   = (MKStreamWriteFunc) il_first_write;

  // Careful not to call NS_RELEASE until the end, because it sets reader=NULL.
 	NS_RELEASE(reader);

	return stream;
}


IL_IMPLEMENT(PRBool)
IL_PreferredStream(URL_Struct *urls)
{
	il_container *ic = 0;
	IL_ImageReq *image_req;
	ilIURL *iurl;
	ilINetReader *reader;

	PR_ASSERT(urls);
	if (urls) {
		/* xxx this MUST be an image stream */
	    iurl = (ilIURL *)urls->fe_data;
		reader = iurl->GetReader();
		ic = IL_GetNetReaderContainer(reader);
		NS_RELEASE(reader);

		PR_ASSERT(ic);
		if (ic) {
            /*
             * It could be that layout aborted image loading by
             * calling IL_FreeImage before the netlib finished
             * transferring data.  Don't do anything.
             */
            if (ic->state == IC_ABORT_PENDING)
                return PR_FALSE;

			/* discover if layout is blocked on this image */
			for (image_req = ic->clients; image_req;
                 image_req = image_req->next) {
#ifdef MOZ_NGLAYOUT
  XP_ASSERT(0);
#else
#ifndef M12N                    /* XXXM12N Fixme.  Observer for layout?
                                   Query mechanism for FE? */
				if ((LO_BlockedOnImage(c->cx,
                                       (LO_ImageStruct*)c->client) == TRUE) ||
                    FE_ImageOnScreen(c->cx, (LO_ImageStruct*)c->client) )
#endif /* M12N */
#endif /* MOZ_NGLAYOUT */
				return PR_TRUE;
			}
		}
	}
	return PR_FALSE;
}
