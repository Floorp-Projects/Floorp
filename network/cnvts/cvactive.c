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
#include "xp.h"
#include "plstr.h"
#include "prmem.h"
#include "netutils.h"
#include "mkselect.h"
#include "mktcp.h"
#include "cvactive.h"
#include "mkgeturl.h"
#include "mkstream.h"
#include "glhist.h"
#include "merrors.h"


extern int MK_OUT_OF_MEMORY;


typedef struct _DataObject {
	int               state;
	NET_StreamClass  *next_stream;
	char             *prev_buffer;
    int32             prev_buffer_len;
	MWContext        *window_id;
	int               format_out;
	URL_Struct       *URL_s;
	PRBool			  signal_at_end_of_multipart;
} DataObject;

#define NORMAL_S			1
#define FOUND_BOUNDARY_S    2

#define MAX_MIME_LINE 200

/* parse a mime/multipart stream.  The boundary is already
 * known and we start off in the content of the original
 * message/822 body.
 *
 * buffer data up to the size of the passed in data length
 * for efficiency of the upstream module.
 */
PRIVATE int net_MultipleDocumentWrite (NET_StreamClass *stream, CONST char* s, int32 l)
{
	int32 i=0;
	int32 line_length=1;
	int   rv=0;
	char *cp;
	char *line;
	char *push_buffer=NULL;
	int32 push_buffer_size=0;
	PRBool all_done=PR_FALSE;
	DataObject *obj=stream->data_object;
	BlockAllocCat(obj->prev_buffer, obj->prev_buffer_len, s, l);
	obj->prev_buffer_len += l;	

	if(!obj->prev_buffer)
		return(MK_OUT_OF_MEMORY);

	line = obj->prev_buffer;

	/* try and find a line
	 */
	for(cp=obj->prev_buffer; i < obj->prev_buffer_len; cp++, i++, line_length++)
	  {
		if(*cp == '\n')
		  {
			
			switch(obj->state)
			  {
				case NORMAL_S:
				  {
					char *cp2 = line;
					int blength = PL_strlen(obj->URL_s->boundary);

				    /* look for boundary.  We can rest assured that these
					   PL_strncmp() calls are safe, because we know that the
					   valid portion of the string starting at cp2 has a
					   newline in it (at *cp), and we know that boundary
					   strings never have newlines in them.  */
		            if((!PL_strncmp(cp2, "--",2) && 
						!PL_strncmp(cp2+2, 
							        obj->URL_s->boundary, 
							        blength))
		              	|| (!PL_strncmp(cp2, 
							        obj->URL_s->boundary, 
							        blength)))
		              {
						TRACEMSG(("Found boundary: %s", obj->URL_s->boundary));
						obj->state = FOUND_BOUNDARY_S;
	
						if(obj->next_stream)
						  {

							if(push_buffer)
							  {
							    /* strip the last newline before the
							     * boundary
							     */
							    push_buffer_size--;
								/* if there is a CR as well as a LF
								 * strip that too
								 */
								if(push_buffer_size > 0 && push_buffer[push_buffer_size-1] == CR)
									push_buffer_size--;
	        				    rv = (*obj->next_stream->put_block)
													    (obj->next_stream, 
													    push_buffer, 
													    push_buffer_size);
							    PR_Free(push_buffer);
							    push_buffer = NULL; 
							    push_buffer_size = 0;
							  }

							TRACEMSG(("Completeing an open stream"));
							/* if this stream is not the last one, set a flag
							   before completion to let completion do special stuff */
							PR_ASSERT(cp2 + blength <= cp);
								/* Because the above PL_strcmp calls succeeded.
								   Because this is true, we know the first call
								   to PL_strncmp below is always safe, but we
								   need to check lengths before we can be sure
								   the other call is safe. */

							if(   (cp2 + blength + 2 < cp && 
							 	   !PL_strncmp(cp2+2+blength, "--",2))
							   || !PL_strncmp(cp2+blength, "--",2))
							  {
								/* very last boundary */
								obj->next_stream->is_multipart = PR_FALSE;

								/* set the all_done flag when
								 * we have found the final boundary
								 */
								all_done = PR_TRUE;
							  }
							else
							  {
								obj->next_stream->is_multipart = PR_TRUE;
							  }
							
			        	    /* complete the last stream
			         	     */
			        	    (*obj->next_stream->complete)
											    (obj->next_stream);
						    PR_Free(obj->next_stream);
						    obj->next_stream = NULL;
						  }
							
						/* move the line ptr up to new data */
						line = cp+1;
						line_length=1; /* reset */

						if(all_done && obj->signal_at_end_of_multipart)
							return(MK_MULTIPART_MESSAGE_COMPLETED);
						break;
					  }
					else 
					  {
						TRACEMSG(("Pushing line (actually buffering)"));

						/* didn't find the boundary
						 */
						if(obj->next_stream)
						  {

							BlockAllocCat(push_buffer, push_buffer_size,
										  line, line_length);
							push_buffer_size += line_length;
							if(!push_buffer)
								return(MK_OUT_OF_MEMORY);
						  }

						/* move the line ptr up to new data */
						line = cp+1;
						line_length=0; /* reset */
					  }
					break;
				  }

				case FOUND_BOUNDARY_S:

					PR_ASSERT(*cp == '\n'); /* from the 'if' above */

					/* terminate at the newline.
					 * now 'line' points to a valid NULL terminated C string
					 */
					*cp = '\0';

					TRACEMSG(("Parsing header: >%s<", line));
    
				    /* parse mime headers
				     * stop when a blank line is encountered
				     */
				    if(*line == '\0' || *line == '\r')
				      {
						int format_out;
					    obj->state = NORMAL_S;
    
					    TRACEMSG(("Found end of headers"));
    					if (obj->URL_s->content_type == NULL)
							StrAllocCopy(obj->URL_s->content_type, TEXT_PLAIN);

						/* abort all existing streams.  We
						 * have to do this to prevent the
						 * image lib and other things from
						 * continuing to load the page after
						 * we left.
						 *
						 * don't abort other streams if it's
						 * just a new inline image or if the
						 * stream is not going to the screen
						 */
						if(CLEAR_CACHE_BIT(obj->format_out) != FO_INTERNAL_IMAGE
							&& (!PL_strncasecmp(obj->URL_s->content_type, 
											 "text", 4)
								||
								!PL_strncasecmp(obj->URL_s->content_type, 
                                                "image", 4)) )
						  {
							NET_SilentInterruptWindow(obj->window_id);
							format_out = obj->format_out;
						  }
						else
						  {
							/* don't cache image animations... */
							format_out = CLEAR_CACHE_BIT(obj->format_out);
						  }

					    /* build a stream
					     */
					    obj->next_stream = NET_StreamBuilder(format_out, 
														 	 obj->URL_s, 
														     obj->window_id);
	
					    if(!obj->next_stream)
						    return(MK_UNABLE_TO_CONVERT);
						
				      }
				    else if(!PL_strncasecmp(line, "CONTENT-TYPE:", 13))
				      {
	
					    strtok(line+13, ";"); /* terminate at ; */

					    StrAllocCopy(obj->URL_s->content_type, 
								     XP_StripLine(line+13));
	
					    if(!obj->URL_s->content_type || !*obj->URL_s->content_type)
						    StrAllocCopy(obj->URL_s->content_type, TEXT_PLAIN);

					    TRACEMSG(("found new content_type: %s", 
							       obj->URL_s->content_type));

				      }
					else
				      {
				    	/* Pass all other headers to the MIME header parser 
				     	 */
						char *value = PL_strchr(line, ':');
    					if(value)
        					value++;
    					NET_ParseMimeHeader(NET_AllowForeignCookies,
											obj->window_id, 
											obj->URL_s, 
											line, 
											value, PR_FALSE);
				      }
					line = cp+1;
					line_length = 0;
				    break;

				default:
				    assert(0);
					break;
				
			  }
		  } /* end if */
	  } /* end for */

	if(line_length > MAX_MIME_LINE * 2)
	  {
		int32 new_size;

		TRACEMSG(("Line too long pushing it"));

		if(obj->next_stream)
		  {
			if(push_buffer)
			  {
        		rv = (*obj->next_stream->put_block)(obj->next_stream, 
												push_buffer, 
												push_buffer_size);
	        	rv = (*obj->next_stream->put_block)(obj->next_stream, 
													line, 
													MAX_MIME_LINE);
				PR_Free(push_buffer);
				push_buffer = 0;
				push_buffer_size = 0;
			  }
			else
			  {
	        	rv = (*obj->next_stream->put_block)(obj->next_stream, 
													line, 
													MAX_MIME_LINE);
			  }

		    if(rv < 0)
			    return(rv);
		  }

		/* newsize equals the old size minus the difference
		 * between line and the start of the old buffer and
		 * the MAX_MIME_LENGTH that we just wrote out
		 */
		new_size = obj->prev_buffer_len - 
				     ((line - obj->prev_buffer) + MAX_MIME_LINE);

		memmove(obj->prev_buffer, line+MAX_MIME_LINE, new_size);
		obj->prev_buffer_len = new_size;
	
		return(0);
	  }

	if(line != obj->prev_buffer)
	  {
		/* some part of the line has been digested, get rid of
		 * the part that has been used
		 */
		obj->prev_buffer_len -= (line - obj->prev_buffer);
		memmove(obj->prev_buffer, line, obj->prev_buffer_len);
	  }

	/* if there is anything in the push buffer send it now
	 */
	if(push_buffer)
	  {
		if(obj->next_stream)
		  {
			TRACEMSG(("Pushing buffered data"));
        	rv = (*obj->next_stream->put_block)(obj->next_stream, 
												push_buffer, 
												push_buffer_size);
		  }
		PR_Free(push_buffer);
        if (rv < 0)
            return rv;
	  }
    
	return(0);
}

/* is the stream ready for writeing?
 */
PRIVATE unsigned int net_MultipleDocumentWriteReady (NET_StreamClass * stream)
{
  DataObject *obj=stream->data_object;
  if(obj->next_stream)
    return((*obj->next_stream->is_write_ready)(obj->next_stream));
  else
	return(MAX_WRITE_READY);
}


PRIVATE void net_MultipleDocumentComplete (NET_StreamClass *stream)
{
	DataObject *obj=stream->data_object;	

    if(obj->next_stream)
	  {
	    (*obj->next_stream->complete)(obj->next_stream);
		PR_Free(obj->next_stream);
	  }

	PR_FREEIF(obj->prev_buffer);
    PR_Free(obj);

    return;
}

PRIVATE void net_MultipleDocumentAbort (NET_StreamClass *stream, int status)
{
	DataObject *obj=stream->data_object;	
    if(obj->next_stream)
	  {
    	(*obj->next_stream->abort)(obj->next_stream, status);
		PR_Free(obj->next_stream);
	  }

	PR_FREEIF(obj->prev_buffer);
    PR_Free(obj);

    return;
}


PUBLIC NET_StreamClass * 
CV_MakeMultipleDocumentStream (int         format_out,
                               void       *data_object,
                               URL_Struct *URL_s,
                               MWContext  *window_id)
{
    DataObject* obj;
    NET_StreamClass* stream;
    
    TRACEMSG(("Setting up display stream. Have URL: %s\n", URL_s->address));

	GH_UpdateGlobalHistory(URL_s);

	URL_s->is_active = PR_TRUE;  /* set to disable view source */

    stream = PR_NEW(NET_StreamClass);
    if(stream == NULL) 
        return(NULL);

    obj = PR_NEW(DataObject);
    if (obj == NULL) 
        return(NULL);

	memset(obj, 0, sizeof(DataObject));

	if(CVACTIVE_SIGNAL_AT_END_OF_MULTIPART == (int) data_object)
		obj->signal_at_end_of_multipart = PR_TRUE;
    
    stream->name           = "Multiple Document";
    stream->complete       = (MKStreamCompleteFunc) net_MultipleDocumentComplete;
    stream->abort          = (MKStreamAbortFunc) net_MultipleDocumentAbort;
    stream->put_block      = (MKStreamWriteFunc) net_MultipleDocumentWrite;
    stream->is_write_ready = (MKStreamWriteReadyFunc) net_MultipleDocumentWriteReady;
    stream->data_object    = obj;  /* document info object */
    stream->window_id      = window_id;

	/* don't cache these
	format_out = CLEAR_CACHE_BIT(format_out);
	 */

	/* enable clicking since it doesnt go through the cache 
	 * code
	 */
	FE_EnableClicking(window_id);

	obj->next_stream = NULL;
	obj->window_id  = window_id;
	obj->format_out = format_out;
	obj->URL_s      = URL_s;
	obj->state      = NORMAL_S;

	/* make sure that we have a real boundary.
 	 * if not fill in '--' as a boundary
	 */
	if(!URL_s->boundary)
		StrAllocCopy(URL_s->boundary, "--");

	/* overwrite the current content type with TEXT/PLAIN since
	 * we want that as the default content-type for
	 * body parts
	 */
	StrAllocCopy(URL_s->content_type, TEXT_PLAIN);

    TRACEMSG(("Returning stream from NET_MultipleDocumentConverter\n"));

    return stream;
}
