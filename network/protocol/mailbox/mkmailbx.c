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
/*
 * state machine to handle mailbox URL 
 */

#include "mkutils.h"
#include "mkgeturl.h"
#include "mktcp.h"
#include "mkparse.h"
#include "mkmailbx.h"
#include "msgcom.h"
#include "msgnet.h"
#include "msg_srch.h"
#include "libmime.h"
#include "merrors.h"
#include "mkimap4.h"
#include "netutils.h"
#ifdef XP_MAC
#include "msg_srch.h"
#endif

/* for XP_GetString() */
#include "xpgetstr.h"
extern int XP_MAIL_READING_FOLDER;
extern int XP_MAIL_READING_MESSAGE;
extern int XP_COMPRESSING_FOLDER;
extern int XP_MAIL_DELIVERING_QUEUED_MESSAGES;
extern int XP_READING_MESSAGE_DONE;
extern int XP_MAIL_READING_FOLDER_DONE;
extern int XP_MAIL_COMPRESSING_FOLDER_DONE;
extern int XP_MAIL_DELIVERING_QUEUED_MESSAGES_DONE;
extern int XP_MAIL_SEARCHING;


extern int MK_OUT_OF_MEMORY;

/* definitions of states for the state machine design
 */
typedef enum _MailboxStates {
	MAILBOX_OPEN_FOLDER,
	MAILBOX_FINISH_OPEN_FOLDER,
	MAILBOX_OPEN_MESSAGE,
	MAILBOX_OPEN_STREAM,
	MAILBOX_READ_MESSAGE,
	MAILBOX_COMPRESS_FOLDER,
	MAILBOX_FINISH_COMPRESS_FOLDER,
	MAILBOX_BACKGROUND,
	MAILBOX_NULL,
	MAILBOX_NULL2,
	MAILBOX_DELIVER_QUEUED,
	MAILBOX_FINISH_DELIVER_QUEUED,
	MAILBOX_DONE,
	MAILBOX_ERROR_DONE,
	MAILBOX_FREE,
	MAILBOX_COPY_MESSAGES,
	MAILBOX_FINISH_COPY_MESSAGES
} MailboxStates;



/* structure to hold data pertaining to the active state of
 * a transfer in progress.
 *
 */
typedef struct _MailboxConData {
	MSG_Pane		*pane;

    int     		 next_state;     /* the next state or action to be taken */
    int     		 initial_state;  /* why we are here */
    Bool    		 pause_for_read; /* Pause now for next read? */
	void            *folder_ptr;
	void            *msg_ptr;
	void            *compress_folder_ptr;
	void            *deliver_queued_ptr;
	void			*mailbox_search_ptr;
	char            *folder_name;
	char            *msg_id;
    int32			 msgnum;	/* -1 if none specified. */
	NET_StreamClass *stream;

    XP_Bool  destroy_graph_progress;       /* do we need to destroy 
											* graph progress? */    
	int32  original_content_length; /* the content length at the time of
                                     * calling graph progress */
	char			*input_buffer;
	int32			input_buffer_size;

} MailboxConData;

#define COMPLETE_STREAM   (*cd->stream->complete)(cd->stream)
#define ABORT_STREAM(s)   (*cd->stream->abort)(cd->stream, s)
#define PUT_STREAM(buf, size)   (*cd->stream->put_block) \
						  (cd->stream, buf, size)

/* forward decl */
PRIVATE int32 net_ProcessMailbox (ActiveEntry *ce);

PRIVATE int32
net_MailboxLoad (ActiveEntry * ce)
{
    /* get memory for Connection Data */
    MailboxConData * cd;
	char *path;
	char *search;
	char *part;
	char *wholeUrl;

	/* temp, until imap urls have their own identifier */
	if (!XP_STRNCASECMP(ce->URL_s->address, "IMAP://", 7) )
		return NET_IMAP4Load(ce);

	if (XP_STRCASECMP(ce->URL_s->address, "mailbox:displayattachments") == 0) {
		MIME_DisplayAttachmentPane(ce->window_id);
		return -1;
	}


    cd = XP_NEW(MailboxConData);
	path = NET_ParseURL(ce->URL_s->address, GET_PATH_PART);
	search = NET_ParseURL(ce->URL_s->address, GET_SEARCH_PART);
	part = search;

    ce->con_data = cd;
    if(!ce->con_data)
      {
		ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_OUT_OF_MEMORY);
        ce->status = MK_OUT_OF_MEMORY;
        return (ce->status);
      }

    /* init */
    XP_MEMSET(cd, 0, sizeof(MailboxConData));
	cd->msgnum = -1;

	wholeUrl = XP_STRDUP(ce->URL_s->address);

#ifndef XP_MAC /* #### Fix Mac Pathname */
	NET_UnEscape (path);
	NET_UnEscape (wholeUrl);
#endif /* !XP_MAC */

	cd->pane = ce->URL_s->msg_pane;
	if (!cd->pane)
	{
#ifdef DEBUG_phil
		XP_Trace ("NET_MailboxLoad: url->msg_pane NULL for URL: %s\n", ce->URL_s->address);
#endif
		/* If we're displaying a message, there'll be a '?' in the url */
		if (XP_STRCHR(wholeUrl, '?'))
		{
		  if (XP_STRSTR(wholeUrl, "?compress-folder") || XP_STRSTR(wholeUrl, "?deliver-queued"))
			cd->pane = MSG_FindPane(ce->window_id, MSG_FOLDERPANE); /* ###phil tar to the tarpit */
		  else
		  {
			  /* find a pane, just to get back to the MSG_Master */
             MSG_Pane *someRandomPane = MSG_FindPane (ce->window_id, MSG_ANYPANE);
			 if (someRandomPane)
			 {
				cd->pane = MSG_FindPaneFromUrl(someRandomPane, wholeUrl, MSG_MESSAGEPANE);
				/*
				** jrm and dmb, 97/02/06: temporary fix, because we MUST have a thread pane
				** in order for certain operations to succeed.  The macintosh UI can
				** forward from the thread pane, with no message pane available, and this
				** routine was setting cd->pane to a folderpane, which is a kind of
				** pane that really doesn't work.  The permanent fix will be to change the
				** behavior of MSG_FindPane.
				*/
				if (!cd->pane)
				   cd->pane = MSG_FindPaneFromUrl(someRandomPane, wholeUrl, MSG_THREADPANE);
			 }
		  }
		}
		else
		  cd->pane = MSG_FindPane(ce->window_id, MSG_FOLDERPANE);

		if (cd->pane == NULL)
		{
           /* find a pane, just to get back to the MSG_Master */
			MSG_Pane *someRandomPane = MSG_FindPane (ce->window_id, MSG_ANYPANE);
			if (someRandomPane)
				cd->pane = MSG_FindPaneFromUrl (someRandomPane, wholeUrl, MSG_MESSAGEPANE);
		}

		/* ###dmb so there! This whole FindPane stuff is too adhoc. */
		if (cd->pane == NULL)
			cd->pane = MSG_FindPane(ce->window_id, MSG_FOLDERPANE); 
		/* km!  I agree! */
		if (cd->pane == NULL)
			cd->pane = MSG_FindPane(ce->window_id, MSG_THREADPANE);
	/*	###whs this isn't really true the way things are set up. */
	/*	XP_ASSERT(cd->pane && MSG_GetContext(cd->pane) == ce->window_id); */
	}
	if (cd->pane == NULL) 
	{
		FREEIF(wholeUrl);
		return -1; /* ### */
	}

	if (XP_STRCASECMP(wholeUrl, "mailbox:copymessages") == 0)
		cd->next_state = MAILBOX_COPY_MESSAGES;
	else
	{
		cd->folder_name = path;
		cd->next_state = MAILBOX_OPEN_FOLDER;
	}

	if (part && *part == '?') part++;
	while (part) {
	  char* amp = XP_STRCHR(part, '&');
	  if (amp) *amp++ = '\0';
	  if (XP_STRNCMP(part, "id=", 3) == 0) {
		cd->msg_id = XP_STRDUP (NET_UnEscape (part+3));
	  } else if (XP_STRNCMP(part, "number=", 7) == 0) {
		cd->msgnum = atol(part + 7);
		if (cd->msgnum == 0 && part[7] != '0') cd->msgnum = -1;
	  } else if (XP_STRNCMP(part, "uidl=", 5) == 0) {
		/* ### Vile hack time.  If a UIDL was specified, then tell libmsg about
		   it, giving it a chance to arrange so that when this URL is all done,
		   MSG_GetNewMail gets called. */
		MSG_PrepareToIncUIDL(cd->pane, ce->URL_s, NET_UnEscape(part + 5));
	  } else if (ce->URL_s->internal_url &&
				 XP_STRNCMP(part, "compress-folder", 15) == 0) {
		cd->next_state = MAILBOX_COMPRESS_FOLDER;
	  } else if (ce->URL_s->internal_url &&
				 XP_STRNCMP(part, "deliver-queued", 14) == 0) {
		cd->next_state = MAILBOX_DELIVER_QUEUED;
	  } else if (ce->URL_s->internal_url &&
				 XP_STRNCMP(part, "background", 10) == 0) {
		cd->next_state = MAILBOX_BACKGROUND;
	  } else if (ce->URL_s->internal_url &&
				 XP_STRNCMP(part, "null", 10) == 0) {
		cd->next_state = MAILBOX_NULL;
	  }
	  part = amp;
	}
	FREEIF(search);
	FREEIF(wholeUrl);

	cd->initial_state = cd->next_state;

	/* don't cache mailbox url's */
	ce->format_out = CLEAR_CACHE_BIT(ce->format_out);

   	ce->local_file = TRUE;
   	ce->socket = NULL;
	NET_SetCallNetlibAllTheTime(ce->window_id, "mkmailbx");

	return(net_ProcessMailbox(ce));
}


/* This doesn't actually generate HTML - but it is our hook into the 
   message display code, so that we can get some values out of it
   after the headers-to-be-displayed have been parsed.
 */
static char *
mail_generate_html_header_fn (const char *dest, void *closure,
							  MimeHeaders *headers)
{
  ActiveEntry *ce = (ActiveEntry *) closure;
  MailboxConData * cd = (MailboxConData *)ce->con_data;
  MSG_ActivateReplyOptions (cd->pane, headers);
  return 0;
}


static char *
mail_generate_html_footer_fn (const char *dest, void *closure,
							  MimeHeaders *headers)
{
  ActiveEntry *cur_entry = (ActiveEntry *) closure;
  MailboxConData * cd = (MailboxConData *)cur_entry->con_data;

  char *uidl = (headers
				? MimeHeaders_get(headers, HEADER_X_UIDL, FALSE, FALSE)
				: 0);
  if (uidl)
	{
	  XP_FREE(uidl);
	  return MSG_GeneratePartialMessageBlurb (cd->pane,
											  cur_entry->URL_s,
											  closure, headers);
	}
  return 0;
}


static char *
mail_generate_reference_url_fn (const char *dest, void *closure,
								MimeHeaders *headers)
{
  ActiveEntry *cur_entry = (ActiveEntry *) closure;
  char *addr = cur_entry->URL_s->address;
  char *search = (addr ? XP_STRCHR (addr, '?') : 0);
  char *id2;
  char *new_dest;
  char *result;

  if (!dest || !*dest) return 0;
  id2 = XP_STRDUP (dest);
  if (!id2) return 0;
  if (id2[XP_STRLEN (id2)-1] == '>')
	id2[XP_STRLEN (id2)-1] = 0;
  if (id2[0] == '<')
	new_dest = NET_Escape (id2+1, URL_PATH);
  else
	new_dest = NET_Escape (id2, URL_PATH);

  FREEIF (id2);
  result = (char *) XP_ALLOC ((search ? search - addr : 0) +
							  (new_dest ? XP_STRLEN (new_dest) : 0) +
							  40);
  if (result && new_dest)
	{
	  if (search)
		{
		  XP_MEMCPY (result, addr, search - addr);
		  result[search - addr] = 0;
		}
	  else if (addr)
		XP_STRCPY (result, addr);
	  else
		*result = 0;
	  XP_STRCAT (result, "?id=");
	  XP_STRCAT (result, new_dest);

	  if (search && XP_STRSTR (search, "&headers=all"))
		XP_STRCAT (result, "&headers=all");
	}
  FREEIF (new_dest);
  return result;
}


static int
net_make_mail_msg_stream (ActiveEntry *ce)
{
  MailboxConData * cd = (MailboxConData *)ce->con_data;

  StrAllocCopy(ce->URL_s->content_type, MESSAGE_RFC822);

  if (ce->format_out == FO_PRESENT || ce->format_out == FO_CACHE_AND_PRESENT)
	{
	  MimeDisplayOptions *opt = XP_NEW (MimeDisplayOptions);
	  if (!opt) return MK_OUT_OF_MEMORY;
	  XP_MEMSET (opt, 0, sizeof(*opt));

	  opt->generate_reference_url_fn      = mail_generate_reference_url_fn;
	  opt->generate_header_html_fn		  = 0;
	  opt->generate_post_header_html_fn	  = mail_generate_html_header_fn;
	  opt->generate_footer_html_fn		  = mail_generate_html_footer_fn;
	  opt->html_closure					  = ce;
	  ce->URL_s->fe_data = opt;
	}
  cd->stream = NET_StreamBuilder(ce->format_out, ce->URL_s, ce->window_id);
  if (!cd->stream)
	return MK_UNABLE_TO_CONVERT;
  else
	return 0;
}


/*
 * returns negative if the transfer is finished or error'd out
 *
 * returns zero or more if the transfer needs to be continued.
 */
PRIVATE int32
net_ProcessMailbox (ActiveEntry *ce)
{
    MailboxConData * cd = (MailboxConData *)ce->con_data;

	/* temp, until imap urls have their own identifier */
	if ((!XP_STRNCASECMP(ce->URL_s->address, "IMAP://", 7) ) ||
		(!XP_STRNCASECMP(ce->URL_s->address, "view-source:IMAP://",19)))
		return NET_ProcessIMAP4(ce);

    cd->pause_for_read = FALSE; /* already paused; reset */

    while(!cd->pause_for_read)
      {
#ifdef DEBUG_username
		XP_Trace("NET_ProcessMailbox: at top of loop, state %d, status %d", cd->next_state, ce->status);
#endif

        switch(cd->next_state) {

        case MAILBOX_OPEN_FOLDER:
            if (!ce->URL_s->load_background) {
			  char *fmt = XP_GetString( XP_MAIL_READING_FOLDER );
			  char *folder = cd->folder_name;
			  char *s = XP_STRRCHR (folder, '/');
			  if (s)
				folder = s+1;
			  s = (char *) XP_ALLOC(XP_STRLEN(fmt) + XP_STRLEN(folder) + 20);
			  if (s)
				{
				  XP_SPRINTF (s, fmt, folder);
                  NET_Progress(ce->window_id, s);
				  XP_FREE(s);
				}
			}
            ce->status = MSG_BeginOpenFolderSock(cd->pane,
												 cd->folder_name, 
												 cd->msg_id,
												 cd->msgnum,
												 &cd->folder_ptr);

            if(ce->status == MK_CONNECTED)
              {
#ifdef DEBUG_username
				XP_Trace ("NET_ProcessMailBox: next state is MAILBOX_OPEN_MESSAGE");
#endif
                cd->next_state = MAILBOX_OPEN_MESSAGE;
              }
            else if(ce->status > -1)
              {
#ifdef DEBUG_username
				XP_Trace ("NET_ProcessMailBox: next state is MAILBOX_FINISH_OPEN_FOLDER");
#endif
            	cd->pause_for_read = TRUE;
                cd->next_state = MAILBOX_FINISH_OPEN_FOLDER;
              }
#ifdef DEBUG_username
			XP_Trace ("NET_ProcessMailBox: MAILBOX_OPEN_FOLDER got error %d", ce->status);
#endif
            break;

		case MAILBOX_FINISH_OPEN_FOLDER:
            ce->status = MSG_FinishOpenFolderSock(cd->pane,
												  cd->folder_name, 
												  cd->msg_id,
												  cd->msgnum,
												  &cd->folder_ptr);

            if(ce->status == MK_CONNECTED)
              {
                cd->next_state = MAILBOX_OPEN_MESSAGE;
              }
			else
              {
            	cd->pause_for_read = TRUE;
              }
            break;

       case MAILBOX_OPEN_MESSAGE:
			if(cd->msg_id == NULL && cd->msgnum < 0)
			  {
				/* only open the message if we are actuall
				 * asking for a message
			 	 */
            	cd->next_state = MAILBOX_DONE;
				NET_Progress(ce->window_id, XP_GetString( XP_MAIL_READING_FOLDER_DONE ) );
			  }
			else
			  {
                if (!ce->URL_s->load_background)
                    NET_Progress(ce->window_id,
                                 XP_GetString( XP_MAIL_READING_MESSAGE ) );
            	ce->status = MSG_OpenMessageSock(cd->pane,
												 cd->folder_name,
												 cd->msg_id,
												 cd->msgnum,
												 cd->folder_ptr, 
												 &cd->msg_ptr,
												 &ce->URL_s->content_length);
            	cd->next_state = MAILBOX_OPEN_STREAM;
			  }
            break;

	   case MAILBOX_OPEN_STREAM:
		 {
		    ce->status = net_make_mail_msg_stream (ce);
			if (ce->status < 0)
	          {
            	ce->URL_s->error_msg = NET_ExplainErrorDetails(ce->status);
				return(ce->status);
          	  }
			else
			  {
				XP_ASSERT (cd->stream);
            	cd->next_state = MAILBOX_READ_MESSAGE;
			  }

            if (!ce->URL_s->load_background) {
                FE_GraphProgressInit(ce->window_id, ce->URL_s,
                                     ce->URL_s->content_length);
                cd->destroy_graph_progress = TRUE;
            }
			cd->original_content_length = ce->URL_s->content_length;
		 }
		 break;

	   case MAILBOX_READ_MESSAGE:
			{
				int32 read_size;
	
        		cd->pause_for_read  = TRUE;
				read_size = (*cd->stream->is_write_ready)
											(cd->stream);

				if (cd->input_buffer == NULL) {
					cd->input_buffer_size = 10240;
#ifdef DEBUG_ricardob
					cd->input_buffer_size *= 2;
#endif
					while (cd->input_buffer == NULL) {
						cd->input_buffer =
							(char*) XP_ALLOC(cd->input_buffer_size);
						if (!cd->input_buffer) {
							cd->input_buffer_size /= 2;
							if (cd->input_buffer_size < 512) {
								ce->status = MK_OUT_OF_MEMORY;
								break;
							}
						}
					}
					if (cd->input_buffer == NULL) break;
				}

				if(!read_size)
      			  {
        			return(0);  /* wait until we are ready to write */
      			  }
    			else
      			  {
        			read_size = MIN(read_size, cd->input_buffer_size);
      			  }

            	ce->status = MSG_ReadMessageSock(cd->pane,
												 cd->folder_name,
												 cd->msg_ptr,
												 cd->msg_id,
												 cd->msgnum,
												 cd->input_buffer,
												 read_size);

				if(ce->status > 0)
				  {
					ce->bytes_received += ce->status;
                    if (!ce->URL_s->load_background)
                        FE_GraphProgress(ce->window_id, ce->URL_s,
                                         ce->bytes_received, ce->status,
                                         ce->URL_s->content_length);

					ce->status = PUT_STREAM(cd->input_buffer, ce->status);
				  }
				else if(ce->status == 0)
					cd->next_state = MAILBOX_DONE;
		    }
            break;

		case MAILBOX_COMPRESS_FOLDER:
			if ((cd->initial_state == MAILBOX_COMPRESS_FOLDER) &&
                (!ce->URL_s->load_background)) {
				char *fmt= XP_GetString( XP_COMPRESSING_FOLDER );
				char *folder = cd->folder_name;
				char *s = XP_STRRCHR (folder, '/');
				if (s)
				  folder = s+1;
				s = (char *)XP_ALLOC (XP_STRLEN(fmt) + XP_STRLEN(folder) + 20);
				if (s)
				  {
					XP_SPRINTF (s, fmt, folder);
					NET_Progress(ce->window_id, s);
					XP_FREE(s);
				  }
			  }
			ce->status = MSG_BeginCompressFolder(cd->pane, ce->URL_s,
												 cd->folder_name,
												 &cd->compress_folder_ptr);
			if (ce->status == MK_CONNECTED) {
			  cd->next_state = MAILBOX_DONE;
			} else {
			  cd->pause_for_read = TRUE;
			  cd->next_state = MAILBOX_FINISH_COMPRESS_FOLDER;
			}
			break;
		  
		case MAILBOX_FINISH_COMPRESS_FOLDER:
			ce->status = MSG_FinishCompressFolder(cd->pane, ce->URL_s,
												  cd->folder_name,
												  cd->compress_folder_ptr);
			if (ce->status == MK_CONNECTED) {
			  cd->next_state = MAILBOX_DONE;
			} else {
			  cd->pause_for_read = TRUE;
			}
			break;

		case MAILBOX_BACKGROUND:
			ce->status = MSG_ProcessBackground(ce->URL_s);
			if (ce->status == MK_CONNECTED) {
			  cd->next_state = MAILBOX_DONE;
			} else {
			  cd->pause_for_read = TRUE;
			}
			break;

		case MAILBOX_NULL:
			ce->status = MK_WAITING_FOR_CONNECTION;
			cd->next_state = MAILBOX_NULL2;
			cd->pause_for_read = TRUE;
			break;
		case MAILBOX_NULL2:
			ce->status = MK_CONNECTED;
			cd->next_state = MAILBOX_DONE;
			break;

		case MAILBOX_DELIVER_QUEUED:
            if (!ce->URL_s->load_background)
                NET_Progress(ce->window_id,
                             XP_GetString( XP_MAIL_DELIVERING_QUEUED_MESSAGES ) );
			ce->status = MSG_BeginDeliverQueued(cd->pane, ce->URL_s,
												&cd->deliver_queued_ptr);
			if (ce->status == MK_CONNECTED) {
			  cd->next_state = MAILBOX_DONE;
			} else {
			  cd->pause_for_read = TRUE;
			  cd->next_state = MAILBOX_FINISH_DELIVER_QUEUED;
			}
			break;

		case MAILBOX_FINISH_DELIVER_QUEUED:
			ce->status = MSG_FinishDeliverQueued(cd->pane, ce->URL_s,
												 cd->deliver_queued_ptr);
			if (ce->status == MK_CONNECTED) {
			  cd->next_state = MAILBOX_DONE;
			} else {
			  cd->pause_for_read = TRUE;
			}
			break;
		
		case MAILBOX_COPY_MESSAGES:
			ce->status = MSG_BeginCopyingMessages(ce->window_id);
			if (ce->status == MK_CONNECTED) 
				cd->next_state = MAILBOX_DONE;
			else
			{
				cd->pause_for_read = TRUE;
				cd->next_state = MAILBOX_FINISH_COPY_MESSAGES;
			}
			break;

		case MAILBOX_FINISH_COPY_MESSAGES:
			ce->status = MSG_FinishCopyingMessages(ce->window_id);
			if (ce->status == MK_CONNECTED) 
				cd->next_state = MAILBOX_DONE;
			else 
				cd->pause_for_read = TRUE;
			break;

        case MAILBOX_DONE:
			if(cd->stream)
			  {
				COMPLETE_STREAM;
				FREE(cd->stream);
			  }
            cd->next_state = MAILBOX_FREE;
            break;
        
        case MAILBOX_ERROR_DONE:
			if(cd->stream)
			  {
				ABORT_STREAM(ce->status);
				FREE(cd->stream);
			  }
            cd->next_state = MAILBOX_FREE;

			if (ce->status < -1 && !ce->URL_s->error_msg)
			  ce->URL_s->error_msg = NET_ExplainErrorDetails(ce->status);

            break;
        
        case MAILBOX_FREE:
          	NET_ClearCallNetlibAllTheTime(ce->window_id, "mkmailbx");

			if (cd->input_buffer) {
				XP_FREE(cd->input_buffer);
				cd->input_buffer = NULL;
			}

			if(cd->destroy_graph_progress)
			  FE_GraphProgressDestroy(ce->window_id, 
									  ce->URL_s, 
									  cd->original_content_length,
									  ce->bytes_received);

			if(cd->msg_ptr)
			  {
            	MSG_CloseMessageSock(cd->pane, cd->folder_name,
									 cd->msg_id, cd->msgnum, cd->msg_ptr);
				cd->msg_ptr = NULL;
                if (!ce->URL_s->load_background)
                    NET_Progress(ce->window_id,
                                 XP_GetString( XP_READING_MESSAGE_DONE ) );
			  }

			if(cd->folder_ptr)
			  {
            	MSG_CloseFolderSock(cd->pane, cd->folder_name,
									cd->msg_id, cd->msgnum, cd->folder_ptr);
				if ((cd->msg_id == NULL && cd->msgnum < 0) &&
                    (!ce->URL_s->load_background))
				  /* If we read a message, don't hide the previous message
					 with this one. */
                    NET_Progress(ce->window_id,
                                 XP_GetString( XP_MAIL_READING_FOLDER_DONE ) );
			  }

			if(cd->compress_folder_ptr)
			  {
            	MSG_CloseCompressFolderSock(cd->pane, ce->URL_s,
											cd->compress_folder_ptr);
				cd->compress_folder_ptr = NULL;
				if ((cd->initial_state == MAILBOX_COMPRESS_FOLDER) &&
                    (!ce->URL_s->load_background))
				  NET_Progress(ce->window_id,
							 XP_GetString( XP_MAIL_COMPRESSING_FOLDER_DONE ) );
			  }
			if(cd->deliver_queued_ptr)
			  {
            	MSG_CloseDeliverQueuedSock(cd->pane, ce->URL_s,
										   cd->deliver_queued_ptr);
				cd->deliver_queued_ptr = NULL;
                if (!ce->URL_s->load_background)
                    NET_Progress(ce->window_id,
                                 XP_GetString( XP_MAIL_DELIVERING_QUEUED_MESSAGES_DONE ) );
			  }
			FREEIF(cd->folder_name);
			FREEIF(cd->msg_id);
            FREE(cd);

            return(-1); /* final end */
        
        default: /* should never happen !!! */
            TRACEMSG(("MAILBOX: BAD STATE!"));
			XP_ASSERT(0);
            cd->next_state = MAILBOX_ERROR_DONE;
            break;
        }

        /* check for errors during load and call error 
         * state if found
         */
        if(ce->status < 0 && cd->next_state != MAILBOX_FREE)
          {
            cd->next_state = MAILBOX_ERROR_DONE;
            /* don't exit! loop around again and do the free case */
            cd->pause_for_read = FALSE;
          }
      } /* while(!cd->pause_for_read) */
    
#ifdef DEBUG_username
	  XP_Trace ("Leaving NET_ProcessMailbox with status %d", ce->status);
#endif

    return(ce->status);
}

/* abort the connection in progress
 */
PRIVATE int32
net_InterruptMailbox(ActiveEntry * ce)
{
    MailboxConData * cd = (MailboxConData *)ce->con_data;

	/* temp until imap urls have their own identifier */
	if (!XP_STRNCASECMP(ce->URL_s->address, "IMAP://", 7) )
		return NET_InterruptIMAP4(ce);

    cd->next_state = MAILBOX_ERROR_DONE;
    ce->status = MK_INTERRUPTED;
 
    return(net_ProcessMailbox(ce));
}

/* Free any memory that might be used in caching etc.
 */
PRIVATE void
net_CleanupMailbox(void)
{
    /* nothing so far needs freeing */
    return;
}

MODULE_PRIVATE void
NET_InitMailboxProtocol(void)
{
    static NET_ProtoImpl mailbox_proto_impl;

    mailbox_proto_impl.init = net_MailboxLoad;
    mailbox_proto_impl.process = net_ProcessMailbox;
    mailbox_proto_impl.interrupt = net_InterruptMailbox;
    mailbox_proto_impl.resume = NULL;
    mailbox_proto_impl.cleanup = net_CleanupMailbox;

    NET_RegisterProtocolImplementation(&mailbox_proto_impl, MAILBOX_TYPE_URL);
}

/* Since each search may be composed of multiple distinct URLs to
 * process, use a "search:" URL as a meta-URL which schedules the
 * subparts for processing
 */
PRIVATE int32 
net_ProcessMsgSearch (ActiveEntry *ce)
{
/*	MailboxConData *cd = (MailboxConData *) ce->con_data;*/
	int retVal = MSG_ProcessSearch (ce->window_id);
	ce->status = 0;
	
	if (0 != retVal)
	{
		/* stop this crazy thing */
       	NET_ClearCallNetlibAllTheTime(ce->window_id, "mkmailbx");
	}
	return retVal;
}

PRIVATE int32
net_MsgSearchLoad (ActiveEntry *ce)
{
	/* don't cache search urls */
	ce->format_out = CLEAR_CACHE_BIT(ce->format_out);

   	ce->local_file = TRUE;
   	ce->socket = NULL;
	NET_SetCallNetlibAllTheTime(ce->window_id, "mkmailbx");
	return 0; 
}

PRIVATE int32
net_InterruptMsgSearch (ActiveEntry *ce)
{
	return MSG_InterruptSearch (ce->window_id);
}

/* Free any memory that might be used in caching etc.
 */
PRIVATE void
net_CleanupMsgSearch(void)
{
    /* nothing so far needs freeing */
    return;
}

MODULE_PRIVATE void
NET_InitMsgSearchProtocol(void)
{
    static NET_ProtoImpl msgsearch_proto_impl;

    msgsearch_proto_impl.init = net_MsgSearchLoad;
    msgsearch_proto_impl.process = net_ProcessMsgSearch;
    msgsearch_proto_impl.interrupt = net_InterruptMsgSearch;
    msgsearch_proto_impl.resume = NULL;
    msgsearch_proto_impl.cleanup = net_CleanupMsgSearch;

    NET_RegisterProtocolImplementation(&msgsearch_proto_impl, MSG_SEARCH_TYPE_URL);
}
