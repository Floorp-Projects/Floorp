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
 * State machine to implement NNTP access
 *
 * Designed and originally implemented by Lou Montulli '94
 * Heavily modified by libmsg folks
 */

/* Please leave outside of ifdef for windows precompiled headers */
#define FORCE_PR_LOG /* Allow logging in the release build (sorry this breaks the PCH) */
#include "rosetta.h"
#include "mkutils.h"

#ifdef MOZILLA_CLIENT

#include "merrors.h"

#include "mime.h"
#include "shist.h"
#include "glhist.h"
#include "xp_reg.h"
#include "mknews.h"
#include "mktcp.h"
#include "mkparse.h"
#include "mkformat.h"
#include "mkstream.h"
#include "mkaccess.h"
#include "mksort.h"
#include "mkcache.h"
#include "mkextcac.h"
#include "mknewsgr.h"
#include "libi18n.h"
#include "gui.h"
#include "ssl.h"
#include "mknews.h"

#include "msgcom.h"
#include "msgnet.h"
#include "msg_srch.h"
#include "libmime.h"

#include "prtime.h"
#include "prlog.h"

#include "secnav.h"
#include "prefapi.h"	

/*#define CACHE_NEWSGRP_PASSWORD*/

/* for XP_GetString() */
#include "xpgetstr.h"
extern int MK_MALFORMED_URL_ERROR;
extern int MK_NEWS_ERROR_FMT;
extern int MK_NNTP_CANCEL_CONFIRM;
extern int MK_NNTP_CANCEL_DISALLOWED;
extern int MK_NNTP_NOT_CANCELLED;
extern int MK_OUT_OF_MEMORY;
extern int XP_CONFIRM_SAVE_NEWSGROUPS;
extern int XP_HTML_ARTICLE_EXPIRED;
extern int XP_HTML_NEWS_ERROR;
extern int XP_PROGRESS_READ_NEWSGROUPINFO;
extern int XP_PROGRESS_RECEIVE_ARTICLE;
extern int XP_PROGRESS_RECEIVE_LISTARTICLES;
extern int XP_PROGRESS_RECEIVE_NEWSGROUP;
extern int XP_PROGRESS_SORT_ARTICLES;
extern int XP_PROGRESS_READ_NEWSGROUP_COUNTS;
extern int XP_THERMO_PERCENT_FORM;
extern int XP_PROMPT_ENTER_USERNAME;
extern int MK_BAD_NNTP_CONNECTION;
extern int MK_NNTP_AUTH_FAILED;
extern int MK_NNTP_ERROR_MESSAGE;
extern int MK_NNTP_NEWSGROUP_SCAN_ERROR;
extern int MK_NNTP_SERVER_ERROR;
extern int MK_NNTP_SERVER_NOT_CONFIGURED;
extern int MK_SECURE_NEWS_PROXY_ERROR;
extern int MK_TCP_READ_ERROR;
extern int MK_TCP_WRITE_ERROR;
extern int MK_NNTP_CANCEL_ERROR;
extern int XP_CONNECT_NEWS_HOST_CONTACTED_WAITING_FOR_REPLY;
extern int XP_PLEASE_ENTER_A_PASSWORD_FOR_NEWS_SERVER_ACCESS;
extern int XP_GARBAGE_COLLECTING;
extern int XP_MESSAGE_SENT_WAITING_NEWS_REPLY;
extern int MK_MSG_DELIV_NEWS;
extern int MK_MSG_COLLABRA_DISABLED;
extern int MK_MSG_EXPIRE_NEWS_ARTICLES;

/* Logging stuff */
#ifndef NSPR20
PR_LOG_DEFINE(NNTP);
#define NNTP_LOG_READ(buf) PR_LOG(NNTP, out, ("Receiving: %s", buf))
#define NNTP_LOG_WRITE(buf)	PR_LOG(NNTP, out, ("Sending: %s", buf))
#define NNTP_LOG_NOTE(buf) PR_LOG(NNTP, out, buf)
#else
PRLogModuleInfo* NNTP = NULL;
#define out		PR_LOG_ALWAYS

#define NNTP_LOG_READ(buf) \
if (NNTP==NULL) \
    NNTP = PR_NewLogModule("NNTP"); \
PR_LOG(NNTP, out, ("Receiving: %s", buf)) ;

#define NNTP_LOG_WRITE(buf) \
if (NNTP==NULL) \
    NNTP = PR_NewLogModule("NNTP"); \
PR_LOG(NNTP, out, ("Sending: %s", buf)) ;

#define NNTP_LOG_NOTE(buf) \
if (NNTP==NULL) \
    NNTP = PR_NewLogModule("NNTP"); \
PR_LOG(NNTP, out, buf) ;

#endif /* NSPR20 */

/* Forward declarations */
static int net_xpat_send (ActiveEntry*);
static int net_list_pretty_names(ActiveEntry*);
PRIVATE int32 net_ProcessNews (ActiveEntry *ce);

#ifdef PROFILE
#pragma profile on
#endif

#define LIST_WANTED     0
#define ARTICLE_WANTED  1
#define CANCEL_WANTED   2
#define GROUP_WANTED    3
#define NEWS_POST       4
#define READ_NEWS_RC    5
#define NEW_GROUPS      6
#define SEARCH_WANTED   7
#define PRETTY_NAMES_WANTED 8
#define PROFILE_WANTED	9
#define IDS_WANTED		10

/* the output_buffer_size must be larger than the largest possible line
 * 2000 seems good for news
 *
 * jwz: I increased this to 4k since it must be big enough to hold the
 * entire button-bar HTML, and with the new "mailto" format, that can
 * contain arbitrarily long header fields like "references".
 *
 * fortezza: proxy auth is huge, buffer increased to 8k (sigh).
 */
#define OUTPUT_BUFFER_SIZE (4096*2)

/* the amount of time to subtract from the machine time
 * for the newgroup command sent to the nntp server
 */
#define NEWGROUPS_TIME_OFFSET 60L*60L*12L   /* 12 hours */

/* Allow the user to open at most this many connections to one news host*/
#define kMaxConnectionsPerHost 3

/* Keep this many connections cached. The cache is global, not per host */
#define kMaxCachedConnections 2

/* globals
 */
PRIVATE char * NET_NewsHost = NULL;
PRIVATE XP_List * nntp_connection_list=0;

PRIVATE XP_Bool net_news_last_username_probably_valid=FALSE;
PRIVATE int32 net_NewsChunkSize=-1;  /* default */

#define PUTSTRING(s)      (*cd->stream->put_block) \
                    (cd->stream, s, PL_strlen(s))
#define COMPLETE_STREAM   (*cd->stream->complete)  \
                    (cd->stream)
#define ABORT_STREAM(s)   (*cd->stream->abort) \
                    (cd->stream, s)
#define PUT_STREAM(buf, size)   (*cd->stream->put_block) \
						  (cd->stream, buf, size)

/* states of the machine
 */
typedef enum _StatesEnum {
NNTP_RESPONSE,
#ifdef BLOCK_UNTIL_AVAILABLE_CONNECTION
NNTP_BLOCK_UNTIL_CONNECTIONS_ARE_AVAILABLE,
NNTP_CONNECTIONS_ARE_AVAILABLE,
#endif
NNTP_CONNECT,
NNTP_CONNECT_WAIT,
HG86722
NNTP_LOGIN_RESPONSE,
NNTP_SEND_MODE_READER,
NNTP_SEND_MODE_READER_RESPONSE,
SEND_LIST_EXTENSIONS,
SEND_LIST_EXTENSIONS_RESPONSE,
SEND_LIST_SEARCHES,
SEND_LIST_SEARCHES_RESPONSE,
NNTP_LIST_SEARCH_HEADERS,
NNTP_LIST_SEARCH_HEADERS_RESPONSE,
NNTP_GET_PROPERTIES,
NNTP_GET_PROPERTIES_RESPONSE,
SEND_LIST_SUBSCRIPTIONS,
SEND_LIST_SUBSCRIPTIONS_RESPONSE,
SEND_FIRST_NNTP_COMMAND,
SEND_FIRST_NNTP_COMMAND_RESPONSE,
SETUP_NEWS_STREAM,
NNTP_BEGIN_AUTHORIZE,
NNTP_AUTHORIZE_RESPONSE,
NNTP_PASSWORD_RESPONSE,
NNTP_READ_LIST_BEGIN,
NNTP_READ_LIST,
DISPLAY_NEWSGROUPS,
NNTP_NEWGROUPS_BEGIN,
NNTP_NEWGROUPS,
NNTP_BEGIN_ARTICLE,
NNTP_READ_ARTICLE,
NNTP_XOVER_BEGIN,
NNTP_FIGURE_NEXT_CHUNK,
NNTP_XOVER_SEND,
NNTP_XOVER_RESPONSE,
NNTP_XOVER,
NEWS_PROCESS_XOVER,
NNTP_READ_GROUP,
NNTP_READ_GROUP_RESPONSE,
NNTP_READ_GROUP_BODY,
NNTP_SEND_GROUP_FOR_ARTICLE,
NNTP_SEND_GROUP_FOR_ARTICLE_RESPONSE,
NNTP_PROFILE_ADD,
NNTP_PROFILE_ADD_RESPONSE,
NNTP_PROFILE_DELETE,
NNTP_PROFILE_DELETE_RESPONSE,
NNTP_SEND_ARTICLE_NUMBER,
NEWS_PROCESS_BODIES,
NNTP_PRINT_ARTICLE_HEADERS,
NNTP_SEND_POST_DATA,
NNTP_SEND_POST_DATA_RESPONSE,
NNTP_CHECK_FOR_MESSAGE,
NEWS_NEWS_RC_POST,
NEWS_DISPLAY_NEWS_RC,
NEWS_DISPLAY_NEWS_RC_RESPONSE,
NEWS_START_CANCEL,
NEWS_DO_CANCEL,
NNTP_XPAT_SEND,
NNTP_XPAT_RESPONSE,
NNTP_SEARCH,
NNTP_SEARCH_RESPONSE,
NNTP_SEARCH_RESULTS,
NNTP_LIST_PRETTY_NAMES,
NNTP_LIST_PRETTY_NAMES_RESPONSE,
NNTP_LIST_XACTIVE,
NNTP_LIST_XACTIVE_RESPONSE,
NNTP_LIST_GROUP,
NNTP_LIST_GROUP_RESPONSE,
NEWS_DONE,
NEWS_ERROR,
NNTP_ERROR,
NEWS_FREE
} StatesEnum;

#ifdef DEBUG
char *stateLabels[] = {
"NNTP_RESPONSE",
#ifdef BLOCK_UNTIL_AVAILABLE_CONNECTION
"NNTP_BLOCK_UNTIL_CONNECTIONS_ARE_AVAILABLE",
"NNTP_CONNECTIONS_ARE_AVAILABLE",
#endif
"NNTP_CONNECT",
"NNTP_CONNECT_WAIT",
HG87489
"NNTP_LOGIN_RESPONSE",
"NNTP_SEND_MODE_READER",
"NNTP_SEND_MODE_READER_RESPONSE",
"SEND_LIST_EXTENSIONS",
"SEND_LIST_EXTENSIONS_RESPONSE",
"SEND_LIST_SEARCHES",
"SEND_LIST_SEARCHES_RESPONSE",
"NNTP_LIST_SEARCH_HEADERS",
"NNTP_LIST_SEARCH_HEADERS_RESPONSE",
"NNTP_GET_PROPERTIES",
"NNTP_GET_PROPERTIES_RESPONSE",
"SEND_LIST_SUBSCRIPTIONS",
"SEND_LIST_SUBSCRIPTIONS_RESPONSE",
"SEND_FIRST_NNTP_COMMAND",
"SEND_FIRST_NNTP_COMMAND_RESPONSE",
"SETUP_NEWS_STREAM",
"NNTP_BEGIN_AUTHORIZE",
"NNTP_AUTHORIZE_RESPONSE",
"NNTP_PASSWORD_RESPONSE",
"NNTP_READ_LIST_BEGIN",
"NNTP_READ_LIST",
"DISPLAY_NEWSGROUPS",
"NNTP_NEWGROUPS_BEGIN",
"NNTP_NEWGROUPS",
"NNTP_BEGIN_ARTICLE",
"NNTP_READ_ARTICLE",
"NNTP_XOVER_BEGIN",
"NNTP_FIGURE_NEXT_CHUNK",
"NNTP_XOVER_SEND",
"NNTP_XOVER_RESPONSE",
"NNTP_XOVER",
"NEWS_PROCESS_XOVER",
"NNTP_READ_GROUP",
"NNTP_READ_GROUP_RESPONSE",
"NNTP_READ_GROUP_BODY",
"NNTP_SEND_GROUP_FOR_ARTICLE",
"NNTP_SEND_GROUP_FOR_ARTICLE_RESPONSE",
"NNTP_PROFILE_ADD",
"NNTP_PROFILE_ADD_RESPONSE",
"NNTP_PROFILE_DELETE",
"NNTP_PROFILE_DELETE_RESPONSE",
"NNTP_SEND_ARTICLE_NUMBER",
"NEWS_PROCESS_BODIES",
"NNTP_PRINT_ARTICLE_HEADERS",
"NNTP_SEND_POST_DATA",
"NNTP_SEND_POST_DATA_RESPONSE",
"NNTP_CHECK_FOR_MESSAGE",
"NEWS_NEWS_RC_POST",
"NEWS_DISPLAY_NEWS_RC",
"NEWS_DISPLAY_NEWS_RC_RESPONSE",
"NEWS_START_CANCEL",
"NEWS_DO_CANCEL",
"NNTP_XPAT_SEND",
"NNTP_XPAT_RESPONSE",
"NNTP_SEARCH",
"NNTP_SEARCH_RESPONSE",
"NNTP_SEARCH_RESULTS",
"NNTP_LIST_PRETTY_NAMES",
"NNTP_LIST_PRETTY_NAMES_RESPONSE",
"NNTP_LIST_XACTIVE_RESPONSE",
"NNTP_LIST_XACTIVE",
"NNTP_LIST_GROUP",
"NNTP_LIST_GROUP_RESPONSE",
"NEWS_DONE",
"NEWS_ERROR",
"NNTP_ERROR",
"NEWS_FREE"
};

#endif

/* structure to hold data about a tcp connection
 * to a news host
 */
typedef struct _NNTPConnection {
    char   *hostname;       /* hostname string (may contain :port) */
    PRFileDesc *csock;      /* control socket */
    XP_Bool busy;           /* is the connection in use already? */
    XP_Bool prev_cache;     /* did this connection come from the cache? */
    XP_Bool posting_allowed;/* does this server allow posting */
	XP_Bool default_host;
	XP_Bool no_xover;       /* xover command is not supported here */
	XP_Bool secure;         /* is it a secure connection? */

    char *current_group;	/* last GROUP command sent on this connection */

} NNTPConnection;


/* structure to hold all the state and data
 * for the state machine
 */
typedef struct _NewsConData {
	MSG_Pane     *pane;
	MSG_NewsHost *host;

    StatesEnum  next_state;
    StatesEnum  next_state_after_response;
    int         type_wanted;     /* Article, List, or Group */
    int         response_code;   /* code returned from NNTP server */
    char       *response_txt;    /* text returned from NNTP server */
    Bool     pause_for_read;  /* should we pause for the next read? */

	char       *ssl_proxy_server;    /* ssl proxy server hostname */
	XP_Bool     proxy_auth_required; /* is auth required */
	XP_Bool     sent_proxy_auth;     /* have we sent a proxy auth? */

	XP_Bool     newsrc_performed;    /* have we done a newsrc update? */
	XP_Bool		mode_reader_performed; /* have we sent any cmds to the server yet? */

#ifdef XP_WIN
	XP_Bool     calling_netlib_all_the_time;
#endif
    NET_StreamClass * stream;

    NNTPConnection * control_con;  /* all the info about a connection */

    char   *data_buf;
    int32   data_buf_size;

    /* for group command */
    char   *path; /* message id */

    char   *group_name;
    int32   first_art;
    int32   last_art;
    int32   first_possible_art;
    int32   last_possible_art;

	int32	num_loaded;			/* How many articles we got XOVER lines for. */
	int32	num_wanted;			/* How many articles we wanted to get XOVER
								   lines for. */

    /* for cancellation: the headers to be used */
    char *cancel_from;
    char *cancel_newsgroups;
    char *cancel_distribution;
    char *cancel_id;
    char *cancel_msg_file;
    int cancel_status;

	/* variables for ReadNewsRC */
	int32    newsrc_list_index;
	int32    newsrc_list_count;

    XP_Bool  use_fancy_newsgroup_listing;  /* use LIST XACTIVE or LIST */
    XP_Bool  destroy_graph_progress;       /* do we need to destroy 
											* graph progress? 
											*/    

    char  *output_buffer;                 /* use it to write out lines */

    int32  article_num;   /* current article number */

    char  *message_id;    /* for reading groups */
    char  *author;
    char  *subject;

	int32  original_content_length; /* the content length at the time of
                                     * calling graph progress
                                     */

    /* random pointer for libmsg state */
    void *xover_parse_state;
	void *newsgroup_parse_state;

	TCP_ConData * tcp_con_data;

	void * write_post_data_data;

	XP_Bool some_protocol_succeeded; /* We know we have done some protocol
										with this newsserver recently, so don't
										respond to an error by closing and
										reopening it.  */
	char *command_specific_data;
	char *current_search;
	void *offlineState;				/* offline news state machine for article retrieval */
	int previous_response_code;

} NewsConData;


/* publicly available function to set the news host
 * this will be a useful front end callable routine
 */
PUBLIC void NET_SetNewsHost (const char * value)
{
	/* ### This routine is obsolete and should go away. */
}



/* set the default number of articles retrieved by news at any one time
 */
PUBLIC void 
NET_SetNumberOfNewsArticlesInListing(int32 number)
{
	net_NewsChunkSize = number;
}

/* Whether we cache XOVER data.  NYI. */
PUBLIC void
NET_SetCacheXOVER(XP_Bool value)
{
}



PUBLIC void NET_CleanTempXOVERCache(void)
{
}



static void net_nntp_close (PRFileDesc *sock, int status)
{
	if (status != MK_INTERRUPTED)
	{
		const char *command = "QUIT" CRLF;
		NET_BlockingWrite (sock, command, PL_strlen(command));
		NNTP_LOG_WRITE (command);
	}
	PR_Close(sock);
}


/* user has removed a news host from the UI. 
 * be sure it has also been removed from the
 * connection cache so we renegotiate news host
 * capabilities if we try to use it again
 */
PUBLIC void NET_OnNewsHostDeleted (const char *hostName)
{
	NNTPConnection *conn;
	XP_List *list = nntp_connection_list;

	while ((conn = (NNTPConnection*) XP_ListNextObject(list)) != NULL)
	{
		if (!(PL_strcasecmp(conn->hostname, hostName)))
			net_nntp_close (conn->csock, conn->busy ? MK_INTERRUPTED : 0);

		list = list->prev ? list->prev : nntp_connection_list;
		XP_ListRemoveObject (nntp_connection_list, conn);

		FREEIF(conn->current_group);
        FREEIF(conn->hostname);
		FREE(conn);
	}
}


/* display HTML error state in the context window
 * returns negative error status
 */
PRIVATE int
net_display_html_error_state (ActiveEntry *ce)
{
    NewsConData * cd = (NewsConData *)ce->con_data;
	XP_Bool inMsgPane = FALSE;
	int major_opcode = cd->response_code/100;
	/* #### maybe make this a dialog box
	   to do that, just return an error.
	 */

	/* ERROR STATE
     *  Set up the HTML stream
     */
	ce->format_out = CLEAR_CACHE_BIT(ce->format_out);
    StrAllocCopy(ce->URL_s->content_type, TEXT_HTML);

	/* If we're not in a message pane, don't try to spew HTML
	 * This allows error reporting from the folder pane, subscribe UI, etc.
	 */
	if (ce->URL_s->msg_pane)
		inMsgPane = (MSG_MESSAGEPANE == MSG_GetPaneType(ce->URL_s->msg_pane));

	if (ce->format_out == FO_PRESENT && inMsgPane)
	  {
		cd->stream = NET_StreamBuilder(ce->format_out, ce->URL_s,
									  ce->window_id);
		if(!cd->stream)
		  {
			ce->URL_s->error_msg =
			  NET_ExplainErrorDetails(MK_UNABLE_TO_CONVERT);
			return MK_UNABLE_TO_CONVERT;
		  }

		PR_snprintf(cd->output_buffer,
					OUTPUT_BUFFER_SIZE,
					XP_GetString(XP_HTML_NEWS_ERROR),
					cd->response_txt);
		if(ce->status > -1)
		  ce->status = PUTSTRING(cd->output_buffer);

		if(cd->type_wanted == ARTICLE_WANTED ||
		   cd->type_wanted == CANCEL_WANTED)
		  {
			PL_strcpy(cd->output_buffer,
					   XP_GetString(XP_HTML_ARTICLE_EXPIRED));
			if(ce->status > -1)
			  PUTSTRING(cd->output_buffer);

			if (cd->path && *cd->path && ce->status > -1)
			  {
				PR_snprintf(cd->output_buffer, 
							OUTPUT_BUFFER_SIZE,
							"<P>&lt;%.512s&gt;", cd->path);
				if (cd->article_num > 0)
				{
				  PR_snprintf(cd->output_buffer+PL_strlen(cd->output_buffer),
							 OUTPUT_BUFFER_SIZE-PL_strlen(cd->output_buffer),
							 " (%lu)\n", (unsigned long) cd->article_num);
				  if (ce->URL_s->msg_pane)
					MSG_MarkMessageKeyRead(ce->URL_s->msg_pane, cd->article_num, "");
				}
				PUTSTRING(cd->output_buffer);
				PR_snprintf(cd->output_buffer, OUTPUT_BUFFER_SIZE, "<P> <A HREF=\"news://%s/%s?list-ids\">%s</A> </P>\n", cd->control_con->hostname, cd->group_name, XP_GetString(MK_MSG_EXPIRE_NEWS_ARTICLES));
				PUTSTRING(cd->output_buffer);
			  }

			if(ce->status > -1)
			  {
				COMPLETE_STREAM;
				cd->stream = 0;
			  }
		  }
		else
			if (cd->type_wanted == SEARCH_WANTED)
			ce->status = net_xpat_send(ce);

		ce->URL_s->expires = 1;
		/* ce->URL_s->error_msg =
		   NET_ExplainErrorDetails(MK_NNTP_SERVER_ERROR); 
		 */
	  }
	else
	  {
		/* FORMAT_OUT is not FO_PRESENT - so instead of emitting an HTML
		   error message, present it in a dialog box. */

		PR_snprintf(cd->output_buffer,  /* #### i18n */
				   OUTPUT_BUFFER_SIZE,
				   XP_GetString(MK_NEWS_ERROR_FMT),
				   cd->response_txt);
		ce->URL_s->error_msg = PL_strdup (cd->output_buffer);
	  }

	/* if the server returned a 400 error then it is an expected
	 * error.  the NEWS_ERROR state will not sever the connection
	 */
	if(major_opcode == 4)
	  cd->next_state = NEWS_ERROR;
	else
	  cd->next_state = NNTP_ERROR;

	/* If we ever get an error, let's re-issue the GROUP command
	   before the next ARTICLE command; the overhead isn't very high,
	   and weird stuff seems to be happening... */
	FREEIF(cd->control_con->current_group);

    /* cd->control_con->prev_cache = FALSE;  to keep if from reconnecting */
    return MK_NNTP_SERVER_ERROR;
}


/* gets the response code from the nntp server and the
 * response line
 *
 * returns the TCP return code from the read
 */
PRIVATE int
net_news_response (ActiveEntry * ce)
{
    char * line;
    NewsConData * cd = (NewsConData *)ce->con_data;

    ce->status = NET_BufferedReadLine(ce->socket, &line, &cd->data_buf, 
                    &cd->data_buf_size, (Bool*)&cd->pause_for_read);

	NNTP_LOG_READ(cd->data_buf);

    if(ce->status == 0)
      {
        cd->next_state = NNTP_ERROR;
        cd->pause_for_read = FALSE;
		ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_NNTP_SERVER_ERROR);
        return(MK_NNTP_SERVER_ERROR);
      }

    /* if TCP error of if there is not a full line yet return
     */
    if(ce->status < 0)
	  {
        ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_TCP_READ_ERROR, 
													  PR_GetOSError());

        /* return TCP error
         */
        return MK_TCP_READ_ERROR;
	  }
	else if(!line)
	  {
         return ce->status;
	  }

    cd->pause_for_read = FALSE; /* don't pause if we got a line */

	if(ce->bytes_received == 0)
	  {
		/* this is where kipp says that I can finally query
         * for the security data.  We can't do it after the
         * connect since the handshake isn't done yet...
         */
        /* clear existing data
         */
        FREEIF(ce->URL_s->sec_info);
        ce->URL_s->sec_info = SECNAV_SSLSocketStatus(ce->socket,
                                                     &ce->URL_s->security_on);
	  }

    /* almost correct
     */
    if(ce->status > 1)
	  {
        ce->bytes_received += ce->status;
        FE_GraphProgress(ce->window_id, ce->URL_s, ce->bytes_received, ce->status, ce->URL_s->content_length); 
	  }

    StrAllocCopy(cd->response_txt, line+4);

	cd->previous_response_code = cd->response_code;

    sscanf(line, "%d", &cd->response_code);

	/* authentication required can come at any time
	 */
#ifdef CACHE_NEWSGRP_PASSWORD
	/*
	 * This is an effort of trying to cache the username/password
	 * per newsgroup. It is extremely hard to make it work along with
	 * nntp voluntary password checking mechanism. We are backing this 
	 * feature out. Instead of touching various of backend msg files
	 * at this late Dogbert 4.0 beta4 game, the infrastructure will
	 * remain in the msg library. We only modify codes within this file.
	 * Maybe one day we will try to do it again. Zzzzz -- jht
	 */
	if(480 == cd->response_code || 450 == cd->response_code || 
	   502 == cd->response_code)
	  {
        cd->next_state = NNTP_BEGIN_AUTHORIZE;
		if (502 == cd->response_code) {
			if (2 == cd->previous_response_code/100) {
				if (net_news_last_username_probably_valid) {
				  net_news_last_username_probably_valid = FALSE;
				}
				else {
				  MSG_SetNewsgroupUsername(cd->pane, NULL);
				  MSG_SetNewsgroupPassword(cd->pane, NULL);
				}
			}
			else {
			  net_news_last_username_probably_valid = FALSE;
			  if (NNTP_PASSWORD_RESPONSE == cd->next_state_after_response) {
				MSG_SetNewsgroupUsername(cd->pane, NULL);
				MSG_SetNewsgroupPassword(cd->pane, NULL);
			  }
			}

		}
	  }
#else
	if (480 == cd->response_code || 450 == cd->response_code) 
	{
		cd->next_state = NNTP_BEGIN_AUTHORIZE;
	}
	else if (502 == cd->response_code)
	{
	    net_news_last_username_probably_valid = FALSE;
		return net_display_html_error_state(ce);
	}
#endif
	else
	  {
    	cd->next_state = cd->next_state_after_response;
	  }

    return(0);  /* everything ok */
}

HG40560

/* interpret the server response after the connect
 *
 * returns negative if the server responds unexpectedly
 */
PRIVATE int
net_nntp_login_response (ActiveEntry * ce)
{
    NewsConData * cd = (NewsConData *)ce->con_data;
	XP_Bool postingAllowed = cd->response_code == 200;

    if((cd->response_code/100)!=2)
      {
		ce->URL_s->error_msg  = NET_ExplainErrorDetails(MK_NNTP_ERROR_MESSAGE, cd->response_txt);

    	cd->next_state = NNTP_ERROR;
        cd->control_con->prev_cache = FALSE; /* to keep if from reconnecting */
        return MK_BAD_NNTP_CONNECTION;
      }

	cd->control_con->posting_allowed = postingAllowed; /* ###phil dead code */
	MSG_SetNewsHostPostingAllowed (cd->host, postingAllowed);
    
    cd->next_state = NNTP_SEND_MODE_READER;
    return(0);  /* good */
}

PRIVATE int
net_nntp_send_mode_reader (ActiveEntry * ce)
{
    NewsConData * cd = (NewsConData *)ce->con_data;

    PL_strcpy(cd->output_buffer, "MODE READER" CRLF);

    ce->status = (int) NET_BlockingWrite(ce->socket,cd->output_buffer,PL_strlen(cd->output_buffer));
	NNTP_LOG_WRITE(cd->output_buffer);

    cd->next_state = NNTP_RESPONSE;
    cd->next_state_after_response = NNTP_SEND_MODE_READER_RESPONSE;
    cd->pause_for_read = TRUE;

    return(ce->status);

}

PRIVATE int
net_nntp_send_mode_reader_response (ActiveEntry * ce)
{
    NewsConData * cd = (NewsConData *)ce->con_data;
	cd->mode_reader_performed = TRUE;

	/* ignore the response code and continue
	 */

	if (MSG_GetNewsHostPushAuth(cd->host))
		/* if the news host is set up to require volunteered (pushed) authentication,
		 * do that before we do anything else
		 */
		cd->next_state = NNTP_BEGIN_AUTHORIZE;
	else
		cd->next_state = SEND_LIST_EXTENSIONS;

	return(0);
}

PRIVATE int net_nntp_send_list_extensions (ActiveEntry *ce)
{
	NewsConData *cd = (NewsConData*) ce->con_data;
    PL_strcpy(cd->output_buffer, "LIST EXTENSIONS" CRLF);
    ce->status = (int) NET_BlockingWrite(ce->socket,cd->output_buffer,PL_strlen(cd->output_buffer));
	NNTP_LOG_WRITE(cd->output_buffer);

	cd->next_state = NNTP_RESPONSE;
	cd->next_state_after_response = SEND_LIST_EXTENSIONS_RESPONSE;
	cd->pause_for_read = TRUE;
	return ce->status;
}

PRIVATE int net_nntp_send_list_extensions_response (ActiveEntry *ce)
{
	NewsConData *cd = (NewsConData*) ce->con_data;

	if (cd->response_code > 200 && cd->response_code < 300)
	{
		char *line = NULL;
		MSG_NewsHost *host = cd->host;

		ce->status = NET_BufferedReadLine (ce->socket, &line, &cd->data_buf,
			&cd->data_buf_size, (Bool*)&cd->pause_for_read);

		if(ce->status == 0)
		{
			cd->next_state = NNTP_ERROR;
			cd->pause_for_read = FALSE;
			ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_NNTP_SERVER_ERROR);
			return MK_NNTP_SERVER_ERROR;
		}
		if (!line)
			return ce->status;  /* no line yet */
		if (ce->status < 0)
		{
			ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_TCP_READ_ERROR, PR_GetOSError());
			/* return TCP error */
			return MK_TCP_READ_ERROR;
		}

		if ('.' != line[0])
			MSG_AddNewsExtension (host, line);
		else
		{
			/* tell libmsg that it's ok to ask this news host for extensions */
			MSG_SupportsNewsExtensions (cd->host, TRUE);

			/* all extensions received */
			cd->next_state = SEND_LIST_SEARCHES;
			cd->pause_for_read = FALSE;
		}
	}
	else
	{
		/* LIST EXTENSIONS not recognized 
		 * tell libmsg not to ask for any more extensions and move on to
		 * the real NNTP command we were trying to do.
		 */
		MSG_SupportsNewsExtensions (cd->host, FALSE);
		cd->next_state = SEND_FIRST_NNTP_COMMAND;
	}

	return ce->status;
}

PRIVATE int net_nntp_send_list_searches (ActiveEntry *ce)
{
	NewsConData *cd = (NewsConData*) ce->con_data;

	if (MSG_QueryNewsExtension (cd->host, "SEARCH"))
	{
		PL_strcpy(cd->output_buffer, "LIST SEARCHES" CRLF);
		ce->status = (int) NET_BlockingWrite(ce->socket,cd->output_buffer,PL_strlen(cd->output_buffer));
		NNTP_LOG_WRITE(cd->output_buffer);

		cd->next_state = NNTP_RESPONSE;
		cd->next_state_after_response = SEND_LIST_SEARCHES_RESPONSE;
		cd->pause_for_read = TRUE;
	}
	else
	{
		/* since SEARCH isn't supported, move on to GET */
		cd->next_state = NNTP_GET_PROPERTIES;
		cd->pause_for_read = FALSE;
	}

	return ce->status;
}

PRIVATE int net_nntp_send_list_searches_response (ActiveEntry *ce)
{
	NewsConData *cd = (NewsConData*) ce->con_data;

	char *line = NULL;
	ce->status = NET_BufferedReadLine (ce->socket, &line, &cd->data_buf,
		&cd->data_buf_size, (Bool*)&cd->pause_for_read);

	if(ce->status == 0)
	{
		cd->next_state = NNTP_ERROR;
		cd->pause_for_read = FALSE;
		ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_NNTP_SERVER_ERROR);
		return MK_NNTP_SERVER_ERROR;
	}
	if (!line)
		return ce->status;  /* no line yet */
	if (ce->status < 0)
	{
		ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_TCP_READ_ERROR, PR_GetOSError());
		/* return TCP error */
		return MK_TCP_READ_ERROR;
	}

	if ('.' != line[0])
	{
		MSG_AddSearchableGroup (cd->host, line);
	}
	else
	{
		/* all searchable groups received */
		/* LIST SRCHFIELDS is legal if the server supports the SEARCH extension, which */
		/* we already know it does */
		cd->next_state = NNTP_LIST_SEARCH_HEADERS;
		cd->pause_for_read = FALSE;
	}

	return ce->status;
}

PRIVATE int net_send_list_search_headers (ActiveEntry *ce)
{
	NewsConData *cd = (NewsConData*) ce->con_data;
    PL_strcpy(cd->output_buffer, "LIST SRCHFIELDS" CRLF);
    ce->status = (int) NET_BlockingWrite(ce->socket,cd->output_buffer,PL_strlen(cd->output_buffer));
	NNTP_LOG_WRITE(cd->output_buffer);

	cd->next_state = NNTP_RESPONSE;
	cd->next_state_after_response = NNTP_LIST_SEARCH_HEADERS_RESPONSE;
	cd->pause_for_read = TRUE;

	return ce->status;
}

PRIVATE int net_send_list_search_headers_response (ActiveEntry *ce)
{
	NewsConData *cd = (NewsConData*) ce->con_data;
	MSG_NewsHost *host = cd->host;

	char *line = NULL;
	ce->status = NET_BufferedReadLine (ce->socket, &line, &cd->data_buf,
		&cd->data_buf_size, (Bool*)&cd->pause_for_read);

	if(ce->status == 0)
	{
		cd->next_state = NNTP_ERROR;
		cd->pause_for_read = FALSE;
		ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_NNTP_SERVER_ERROR);
		return MK_NNTP_SERVER_ERROR;
	}
	if (!line)
		return ce->status;  /* no line yet */
	if (ce->status < 0)
	{
		ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_TCP_READ_ERROR, PR_GetOSError());
		/* return TCP error */
		return MK_TCP_READ_ERROR;
	}

	if ('.' != line[0])
		MSG_AddSearchableHeader (host, line);
	else
	{
		cd->next_state = NNTP_GET_PROPERTIES;
		cd->pause_for_read = FALSE;
	}

	return ce->status;
}

PRIVATE int nntp_get_properties (ActiveEntry *ce)
{
	NewsConData *cd = (NewsConData*) ce->con_data;

	if (MSG_QueryNewsExtension (cd->host, "SETGET"))
	{
		PL_strcpy(cd->output_buffer, "GET" CRLF);
		ce->status = (int) NET_BlockingWrite(ce->socket,cd->output_buffer,PL_strlen(cd->output_buffer));
		NNTP_LOG_WRITE(cd->output_buffer);

		cd->next_state = NNTP_RESPONSE;
		cd->next_state_after_response = NNTP_GET_PROPERTIES_RESPONSE;
		cd->pause_for_read = TRUE;
	}
	else
	{
		/* since GET isn't supported, move on LIST SUBSCRIPTIONS */
		cd->next_state = SEND_LIST_SUBSCRIPTIONS;
		cd->pause_for_read = FALSE;
	}
	return ce->status;
}

PRIVATE int nntp_get_properties_response (ActiveEntry *ce)
{
	NewsConData *cd = (NewsConData*) ce->con_data;

	char *line = NULL;
	ce->status = NET_BufferedReadLine (ce->socket, &line, &cd->data_buf,
		&cd->data_buf_size, (Bool*)&cd->pause_for_read);

	if(ce->status == 0)
	{
		cd->next_state = NNTP_ERROR;
		cd->pause_for_read = FALSE;
		ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_NNTP_SERVER_ERROR);
		return MK_NNTP_SERVER_ERROR;
	}
	if (!line)
		return ce->status;  /* no line yet */
	if (ce->status < 0)
	{
		ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_TCP_READ_ERROR, PR_GetOSError());
		/* return TCP error */
		return MK_TCP_READ_ERROR;
	}

	if ('.' != line[0])
	{
		char *propertyName = PL_strdup(line);
		if (propertyName)
		{
			char *space = PL_strchr(propertyName, ' ');
			if (space)
			{
				char *propertyValue = space + 1;
				*space = '\0';
				MSG_AddPropertyForGet (cd->host, 
					propertyName, propertyValue);
			}
			PR_Free(propertyName);
		}
	}
	else
	{
		/* all GET properties received, move on to LIST SUBSCRIPTIONS */
		cd->next_state = SEND_LIST_SUBSCRIPTIONS;
		cd->pause_for_read = FALSE;
	}

	return ce->status;
}

PRIVATE int net_send_list_subscriptions (ActiveEntry *ce)
{
	NewsConData *cd = (NewsConData*) ce->con_data;

#ifdef LATER
	if (MSG_QueryNewsExtension (cd->host, "LISTSUBSCR"))
#else
	if (0)
#endif
	{
		PL_strcpy(cd->output_buffer, "LIST SUBSCRIPTIONS" CRLF);
		ce->status = (int) NET_BlockingWrite(ce->socket,cd->output_buffer,PL_strlen(cd->output_buffer));
		NNTP_LOG_WRITE(cd->output_buffer);

		cd->next_state = NNTP_RESPONSE;
		cd->next_state_after_response = SEND_LIST_SUBSCRIPTIONS_RESPONSE;
		cd->pause_for_read = TRUE;
	}
	else
	{
		/* since LIST SUBSCRIPTIONS isn't supported, move on to real work */
		cd->next_state = SEND_FIRST_NNTP_COMMAND;
		cd->pause_for_read = FALSE;
	}

	return ce->status;
}

PRIVATE int net_send_list_subscriptions_response (ActiveEntry *ce)
{
	NewsConData *cd = (NewsConData*) ce->con_data;

	char *line = NULL;
	ce->status = NET_BufferedReadLine (ce->socket, &line, &cd->data_buf,
		&cd->data_buf_size, (Bool*)&cd->pause_for_read);

	if(ce->status == 0)
	{
		cd->next_state = NNTP_ERROR;
		cd->pause_for_read = FALSE;
		ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_NNTP_SERVER_ERROR);
		return MK_NNTP_SERVER_ERROR;
	}
	if (!line)
		return ce->status;  /* no line yet */
	if (ce->status < 0)
	{
		ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_TCP_READ_ERROR, PR_GetOSError());
		/* return TCP error */
		return MK_TCP_READ_ERROR;
	}

	if ('.' != line[0])
	{
#ifdef LATER
		char *urlScheme = cd->control_con->secure ? "snews:" : "news:";
		char *url = PR_smprintf ("%s//%s/%s", urlScheme, cd->control_con->hostname, line);
		if (url)
			MSG_AddSubscribedNewsgroup (cd->pane, url);
#endif
	}
	else
	{
		/* all default subscriptions received */
		cd->next_state = SEND_FIRST_NNTP_COMMAND;
		cd->pause_for_read = FALSE;
	}

	return ce->status;
}

/* figure out what the first command is and send it
 *
 * returns the status from the NETWrite
 */
PRIVATE int
net_send_first_nntp_command (ActiveEntry *ce)
{
    char *command=0;
    NewsConData * cd = (NewsConData *)ce->con_data;

	if (cd->type_wanted == ARTICLE_WANTED)
	  {
		const char *group = 0;
		uint32 number = 0;

		MSG_NewsGroupAndNumberOfID (cd->pane, cd->path,
									&group, &number);
		if (group && number)
		  {
			cd->article_num = number;
			StrAllocCopy (cd->group_name, group);

			if (cd->control_con->current_group && !PL_strcmp (cd->control_con->current_group, group))
			  cd->next_state = NNTP_SEND_ARTICLE_NUMBER;
			else
			  cd->next_state = NNTP_SEND_GROUP_FOR_ARTICLE;

			cd->pause_for_read = FALSE;
			return 0;
		  }
	  }

    if(cd->type_wanted == NEWS_POST && !ce->URL_s->post_data)
      {
		PR_ASSERT(0);
        return(-1);
      }
    else if(cd->type_wanted == NEWS_POST)
      {  /* posting to the news group */
        StrAllocCopy(command, "POST");
      }
    else if(cd->type_wanted == READ_NEWS_RC)
      {
		if(ce->URL_s->method == URL_POST_METHOD ||
								PL_strchr(ce->URL_s->address, '?'))
        	cd->next_state = NEWS_NEWS_RC_POST;
		else
        	cd->next_state = NEWS_DISPLAY_NEWS_RC;
		return(0);
      } 
	else if(cd->type_wanted == NEW_GROUPS)
	  {
		time_t last_update =
			MSG_NewsgroupsLastUpdatedTime(cd->host);
		char small_buf[64];
#ifndef NSPR20
        PRTime  expandedTime;
#else
        PRExplodedTime  expandedTime;
#endif /* NSPR20 */

		if(!last_update)
	 	  {
			
			ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_NNTP_NEWSGROUP_SCAN_ERROR);
			cd->next_state = NEWS_ERROR;
			return(MK_INTERRUPTED);
		  }
	
		/* subtract some hours just to be sure */
		last_update -= NEWGROUPS_TIME_OFFSET;

        {
           int64  secToUSec, timeInSec, timeInUSec;
           LL_I2L(timeInSec, last_update);
           LL_I2L(secToUSec, PR_USEC_PER_SEC);
           LL_MUL(timeInUSec, timeInSec, secToUSec);
#ifndef NSPR20
           PR_ExplodeTime( &expandedTime, timeInUSec );
#else
           PR_ExplodeTime(timeInUSec, PR_LocalTimeParameters, &expandedTime);
#endif /* NSPR20 */
        }
		PR_FormatTimeUSEnglish(small_buf, sizeof(small_buf), 
                               "NEWGROUPS %y%m%d %H%M%S", &expandedTime);
		
        StrAllocCopy(command, small_buf);

	  }
    else if(cd->type_wanted == LIST_WANTED)
    {
		
		cd->use_fancy_newsgroup_listing = FALSE;
		if (MSG_NewsgroupsLastUpdatedTime(cd->host))
		{
			cd->next_state = DISPLAY_NEWSGROUPS;
        	return(0);
	    }
		else
		{
#ifdef BUG_21013
			if(!FE_Confirm(ce->window_id, XP_GetString(XP_CONFIRM_SAVE_NEWSGROUPS)))
	  		  {
				cd->next_state = NEWS_ERROR;
				return(MK_INTERRUPTED);
	  		  }
#endif /* BUG_21013 */

			if (MSG_QueryNewsExtension(cd->host, "XACTIVE"))
			{
				StrAllocCopy(command, "LIST XACTIVE");
				cd->use_fancy_newsgroup_listing = TRUE;
			}
			else
			{
       			StrAllocCopy(command, "LIST");
			}
		}
    } 
    else if(cd->type_wanted == GROUP_WANTED) 
    {
        /* Don't use MKParse because the news: access URL doesn't follow traditional
         * rules. For instance, if the article reference contains a '#',
         * the rest of it is lost.
         */
        char * slash;

        StrAllocCopy(command, "GROUP ");

        slash = PL_strchr(cd->group_name, '/');  /* look for a slash */
        cd->first_art = 0;
        cd->last_art = 0;
        if (slash)
          {
            *slash = '\0';
#ifdef __alpha
            (void) sscanf(slash+1, "%d-%d", &cd->first_art, &cd->last_art);
#else
            (void) sscanf(slash+1, "%ld-%ld", &cd->first_art, &cd->last_art);
#endif
          }

		StrAllocCopy (cd->control_con->current_group, cd->group_name);
        StrAllocCat(command, cd->control_con->current_group);
      }
	else if (cd->type_wanted == SEARCH_WANTED)
	{
		if (MSG_QueryNewsExtension(cd->host, "SEARCH"))
		{
			/* use the SEARCH extension */
			char *slash = PL_strchr (cd->command_specific_data, '/');
			if (slash)
			{
				char *allocatedCommand = MSG_UnEscapeSearchUrl (slash + 1);
				if (allocatedCommand)
				{
					StrAllocCopy (command, allocatedCommand);
					PR_Free(allocatedCommand);
				}
			}
			cd->next_state = NNTP_RESPONSE;
			cd->next_state_after_response = NNTP_SEARCH_RESPONSE;
		}
		else
		{
			/* for XPAT, we have to GROUP into the group before searching */
			StrAllocCopy (command, "GROUP ");
			StrAllocCat (command, cd->group_name);
			cd->next_state = NNTP_RESPONSE;
			cd->next_state_after_response = NNTP_XPAT_SEND;
		}
	}
	else if (cd->type_wanted == PRETTY_NAMES_WANTED)
	{
		if (MSG_QueryNewsExtension(cd->host, "LISTPRETTY"))
		{
			cd->next_state = NNTP_LIST_PRETTY_NAMES;
			return 0;
		}
		else
		{
			PR_ASSERT(FALSE);
			cd->next_state = NNTP_ERROR;
		}
	}
	else if (cd->type_wanted == PROFILE_WANTED)
	{
		char *slash = PL_strchr (cd->command_specific_data, '/');
		if (slash)
		{
			char *allocatedCommand = MSG_UnEscapeSearchUrl (slash + 1);
			if (allocatedCommand)
			{
				StrAllocCopy (command, allocatedCommand);
				PR_Free(allocatedCommand);
			}
		}
		cd->next_state = NNTP_RESPONSE;
		if (PL_strstr(ce->URL_s->address, "PROFILE NEW"))
			cd->next_state_after_response = NNTP_PROFILE_ADD_RESPONSE;
		else
			cd->next_state_after_response = NNTP_PROFILE_DELETE_RESPONSE;
	}
	else if (cd->type_wanted == IDS_WANTED)
	{
        char *slash = PL_strchr(cd->group_name, '/');  /* look for a slash */
        if (slash)
            *slash = '\0';

		cd->next_state = NNTP_LIST_GROUP;
		return 0;
	}
    else  /* article or cancel */
	{
		if (cd->type_wanted == CANCEL_WANTED)
			StrAllocCopy(command, "HEAD ");
		else
			StrAllocCopy(command, "ARTICLE ");
		if (*cd->path != '<')
			StrAllocCat(command,"<");
		StrAllocCat(command, cd->path);
		if (PL_strchr(command+8, '>')==0) 
			StrAllocCat(command,">");
	}

    StrAllocCat(command, CRLF);
    ce->status = (int) NET_BlockingWrite(ce->socket, command, PL_strlen(command));
	NNTP_LOG_WRITE(command);
    PR_Free(command);

	cd->next_state = NNTP_RESPONSE;
	if (cd->type_wanted != SEARCH_WANTED && cd->type_wanted != PROFILE_WANTED)
		cd->next_state_after_response = SEND_FIRST_NNTP_COMMAND_RESPONSE;
	cd->pause_for_read = TRUE;
    return(ce->status);

} /* sent first command */


/* interprets the server response from the first command sent
 *
 * returns negative if the server responds unexpectedly
 */
PRIVATE int
net_send_first_nntp_command_response (ActiveEntry *ce)
{
    NewsConData * cd = (NewsConData *)ce->con_data;
	int major_opcode = cd->response_code/100;

	if((major_opcode == 3 && cd->type_wanted == NEWS_POST)
	 	|| (major_opcode == 2 && cd->type_wanted != NEWS_POST) )
      {

        cd->next_state = SETUP_NEWS_STREAM;
		cd->some_protocol_succeeded = TRUE;

        return(0);  /* good */
      }
    else
      {
		if (cd->response_code == 411 && cd->type_wanted == GROUP_WANTED)
			MSG_GroupNotFound(cd->pane, cd->host, 
								cd->control_con->current_group, TRUE /* opening group */);
		return net_display_html_error_state(ce);
      }

	/* start the graph progress indicator
     */
    FE_GraphProgressInit(ce->window_id, ce->URL_s, ce->URL_s->content_length);
    cd->destroy_graph_progress = TRUE;  /* we will need to destroy it */
    cd->original_content_length = ce->URL_s->content_length;

	return(ce->status);
}


PRIVATE int
net_send_group_for_article (ActiveEntry * ce)
{
  NewsConData * cd = (NewsConData *)ce->con_data;

  StrAllocCopy (cd->control_con->current_group, cd->group_name);
  PR_snprintf(cd->output_buffer, 
			OUTPUT_BUFFER_SIZE, 
			"GROUP %.512s" CRLF, 
			cd->control_con->current_group);

   ce->status = (int) NET_BlockingWrite(ce->socket,cd->output_buffer,
									  PL_strlen(cd->output_buffer));
	NNTP_LOG_WRITE(cd->output_buffer);

    cd->next_state = NNTP_RESPONSE;
    cd->next_state_after_response = NNTP_SEND_GROUP_FOR_ARTICLE_RESPONSE;
    cd->pause_for_read = TRUE;

    return(ce->status);
}

PRIVATE int
net_send_group_for_article_response (ActiveEntry * ce)
{
  NewsConData * cd = (NewsConData *)ce->con_data;

  /* ignore the response code and continue
   */
  cd->next_state = NNTP_SEND_ARTICLE_NUMBER;

  return(0);
}


PRIVATE int
net_send_article_number (ActiveEntry * ce)
{
  NewsConData * cd = (NewsConData *)ce->con_data;

  PR_snprintf(cd->output_buffer, 
			OUTPUT_BUFFER_SIZE, 
			"ARTICLE %lu" CRLF, 
			cd->article_num);

  ce->status = (int) NET_BlockingWrite(ce->socket,cd->output_buffer,
									  PL_strlen(cd->output_buffer));
	NNTP_LOG_WRITE(cd->output_buffer);

    cd->next_state = NNTP_RESPONSE;
    cd->next_state_after_response = SEND_FIRST_NNTP_COMMAND_RESPONSE;
    cd->pause_for_read = TRUE;

    return(ce->status);
}




static char *
news_generate_news_url_fn (const char *dest, void *closure,
						   MimeHeaders *headers)
{
  ActiveEntry *ce = (ActiveEntry *) closure;
  /* NOTE: you can't use NewsConData here, because ce might refer
	 to a file in the disk cache rather than an NNTP connection. */
  char *prefix = NET_ParseURL(ce->URL_s->address,
							  (GET_PROTOCOL_PART | GET_HOST_PART));
  char *new_dest = NET_Escape (dest, URL_XALPHAS);
  char *result = (char *) PR_Malloc (PL_strlen (prefix) +
									(new_dest ? PL_strlen (new_dest) : 0)
									+ 40);
  if (result && prefix)
	{
	  PL_strcpy (result, prefix);
	  if (prefix[PL_strlen(prefix) - 1] != ':')
		/* If there is a host, it must be terminated with a slash. */
		PL_strcat (result, "/");
	  PL_strcat (result, new_dest);
	}
  FREEIF (prefix);
  FREEIF (new_dest);
  return result;
}

static char *
news_generate_reference_url_fn (const char *dest, void *closure,
								MimeHeaders *headers)
{
  return news_generate_news_url_fn (dest, closure, headers);
}



/* Initiates the stream and sets state for the transfer
 *
 * returns negative if unable to setup stream
 */
PRIVATE int
net_setup_news_stream (ActiveEntry * ce)
{
    NewsConData * cd = (NewsConData *)ce->con_data;
    
    if (cd->type_wanted == NEWS_POST)
	  {
        NET_ClearReadSelect(ce->window_id, ce->socket);
        NET_SetConnectSelect(ce->window_id, ce->socket);
#ifdef XP_WIN
		cd->calling_netlib_all_the_time = TRUE;
		NET_SetCallNetlibAllTheTime(ce->window_id, "mknews");
#endif /* XP_WIN */

		ce->con_sock = ce->socket;
        cd->next_state = NNTP_SEND_POST_DATA;

		NET_Progress(ce->window_id, XP_GetString(MK_MSG_DELIV_NEWS));
	  }
    else if(cd->type_wanted == LIST_WANTED)
	{
		if (cd->use_fancy_newsgroup_listing)
			cd->next_state = NNTP_LIST_XACTIVE_RESPONSE;
		else
			cd->next_state = NNTP_READ_LIST_BEGIN;
	}
    else if(cd->type_wanted == GROUP_WANTED)
        cd->next_state = NNTP_XOVER_BEGIN;
	else if(cd->type_wanted == NEW_GROUPS)
		cd->next_state = NNTP_NEWGROUPS_BEGIN;
    else if(cd->type_wanted == ARTICLE_WANTED ||
			cd->type_wanted == CANCEL_WANTED)
	    cd->next_state = NNTP_BEGIN_ARTICLE;
	else if (cd->type_wanted == SEARCH_WANTED)
		cd->next_state = NNTP_XPAT_SEND;
	else if (cd->type_wanted == PRETTY_NAMES_WANTED)
		cd->next_state = NNTP_LIST_PRETTY_NAMES;
	else if (cd->type_wanted == PROFILE_WANTED)
	{
		if (PL_strstr(ce->URL_s->address, "PROFILE NEW"))
			cd->next_state = NNTP_PROFILE_ADD;
		else
			cd->next_state = NNTP_PROFILE_DELETE;
	}
	else
	  {
		PR_ASSERT(0);
		return -1;
	  }

   return(0); /* good */
}

/* This doesn't actually generate HTML - but it is our hook into the 
   message display code, so that we can get some values out of it
   after the headers-to-be-displayed have been parsed.
 */
static char *
news_generate_html_header_fn (const char *dest, void *closure,
							  MimeHeaders *headers)
{
  ActiveEntry *ce = (ActiveEntry *) closure;
  /* NOTE: you can't always use NewsConData here if, because ce might
	 refer to a file in the disk cache rather than an NNTP connection. */
  NewsConData *cd = 0;

  PR_ASSERT(ce->protocol == NEWS_TYPE_URL ||
			ce->protocol == FILE_CACHE_TYPE_URL ||
			ce->protocol == MEMORY_CACHE_TYPE_URL);
  if (ce->protocol == NEWS_TYPE_URL)
	cd = (NewsConData *)ce->con_data;

  if (cd && cd->type_wanted == CANCEL_WANTED)
	{
	  /* Save these for later (used in NEWS_DO_CANCEL state.) */
	  cd->cancel_from =
		MimeHeaders_get(headers, HEADER_FROM, FALSE, FALSE);
	  cd->cancel_newsgroups =
		MimeHeaders_get(headers, HEADER_NEWSGROUPS, FALSE, TRUE);
	  cd->cancel_distribution =
		MimeHeaders_get(headers, HEADER_DISTRIBUTION, FALSE, FALSE);
	  cd->cancel_id =
		MimeHeaders_get(headers, HEADER_MESSAGE_ID, FALSE, FALSE);
	}
  else
	{
	  MSG_Pane *messagepane = ce->URL_s->msg_pane;
	  if (!messagepane)
	  {
		NNTP_LOG_NOTE(("news_generate_html_header_fn: url->msg_pane NULL for URL: %s", ce->URL_s->address));
	    messagepane = MSG_FindPane(ce->window_id, MSG_MESSAGEPANE);
	  }
      PR_ASSERT (!cd || cd->type_wanted == ARTICLE_WANTED);

	  if (messagepane)
		MSG_ActivateReplyOptions (messagepane, headers);
	}
  return 0;
}



XP_Bool NET_IsNewsMessageURL (const char *url);

int
net_InitializeNewsFeData (ActiveEntry * ce)
{
  MimeDisplayOptions *opt;

  if (!NET_IsNewsMessageURL (ce->URL_s->address))
	{
	  PR_ASSERT(0);
	  return -1;
	}

  if (ce->URL_s->fe_data)
	{
	  PR_ASSERT(0);
	  return -1;
	}

  opt = PR_NEW (MimeDisplayOptions);
  if (!opt) return MK_OUT_OF_MEMORY;
  memset (opt, 0, sizeof(*opt));

  opt->generate_reference_url_fn      = news_generate_reference_url_fn;
  opt->generate_news_url_fn           = news_generate_news_url_fn;
  opt->generate_header_html_fn		  = 0;
  opt->generate_post_header_html_fn	  = news_generate_html_header_fn;
  opt->html_closure					  = ce;

  ce->URL_s->fe_data = opt;
  return 0;
}


PRIVATE int
net_begin_article(ActiveEntry * ce)
{
  NewsConData * cd = (NewsConData *)ce->con_data;
  if (cd->type_wanted != ARTICLE_WANTED &&
	  cd->type_wanted != CANCEL_WANTED)
	return 0;

  /*  Set up the HTML stream
   */ 
  FREEIF (ce->URL_s->content_type);
  ce->URL_s->content_type = PL_strdup (MESSAGE_RFC822);

#ifdef NO_ARTICLE_CACHEING
  ce->format_out = CLEAR_CACHE_BIT (ce->format_out);
#endif

  if (cd->type_wanted == CANCEL_WANTED)
	{
	  PR_ASSERT(ce->format_out == FO_PRESENT);
	  ce->format_out = FO_PRESENT;
	}

  /* Only put stuff in the fe_data if this URL is going to get
	 passed to MIME_MessageConverter(), since that's the only
	 thing that knows what to do with this structure. */
  if (CLEAR_CACHE_BIT(ce->format_out) == FO_PRESENT)
	{
	  ce->status = net_InitializeNewsFeData (ce);
	  if (ce->status < 0)
		{
		  /* #### what error message? */
		  return ce->status;
		}
	}

  cd->stream = NET_StreamBuilder(ce->format_out, ce->URL_s, ce->window_id);
  PR_ASSERT (cd->stream);
  if (!cd->stream) return -1;

  cd->next_state = NNTP_READ_ARTICLE;

  return 0;
}


PRIVATE int
net_news_begin_authorize(ActiveEntry * ce)
{
	char * command = 0;
    NewsConData * cd = (NewsConData *)ce->con_data;
	char * username = 0, * munged_username = 0;
	char * cp;
	static char * last_username=0;
	static char * last_username_hostname=0;

#ifdef CACHE_NEWSGRP_PASSWORD
	/* reuse cached username from newsgroup folder info*/
	if (cd->pane && 
		(!net_news_last_username_probably_valid ||
		 (last_username_hostname && 
		  PL_strcasecmp(last_username_hostname, cd->control_con->hostname)))) {
	  username = SECNAV_UnMungeString(MSG_GetNewsgroupUsername(cd->pane));
	  if (username && last_username &&
		  !PL_strcmp (username, last_username) &&
		  (cd->previous_response_code == 281 || 
		   cd->previous_response_code == 250 ||
		   cd->previous_response_code == 211)) {
		FREEIF (username);
		MSG_SetNewsgroupUsername(cd->pane, NULL);
		MSG_SetNewsgroupPassword(cd->pane, NULL);
	  }
	}
#endif
	
	if (cd->pane) 
	{
	/* Following a snews://username:password@newhost.domain.com/newsgroup.topic
	 * backend calls MSG_Master::FindNewsHost() to locate the folderInfo and setting 
	 * the username/password to the newsgroup folderInfo
	 */
	  username = SECNAV_UnMungeString(MSG_GetNewsgroupUsername(cd->pane));
	  if (username && *username)
	  {
		StrAllocCopy(last_username, username);
		StrAllocCopy(last_username_hostname, cd->control_con->hostname);
		/* use it for only once */
		MSG_SetNewsgroupUsername(cd->pane, NULL);
	  }
	  else {
		  /* empty username; free and clear it so it will work with
		   * our logic
		   */
		  FREEIF(username);
	  }
	}

	/* If the URL/cd->control_con->hostname contains @ this must be triggered
	 * from the bookmark. Use the embed username if we could.
	 */
	if ((cp = PL_strchr(cd->control_con->hostname, '@')) != NULL)
	  {
		/* in this case the username and possibly
		 * the password are in the URL
		 */
		char * colon;
		*cp = '\0';

		colon = PL_strchr(cd->control_con->hostname, ':');
		if(colon)
			*colon = '\0';

		StrAllocCopy(username, cd->control_con->hostname);
		StrAllocCopy(last_username, cd->control_con->hostname);
		StrAllocCopy(last_username_hostname, cp+1);

		*cp = '@';

		if(colon)
			*colon = ':';
	  }
	/* reuse global saved username if we think it is
	 * valid
	 */
    if (!username && net_news_last_username_probably_valid)
	  {
		if( last_username_hostname &&
			!PL_strcasecmp(last_username_hostname, cd->control_con->hostname) )
			StrAllocCopy(username, last_username);
		else
			net_news_last_username_probably_valid = FALSE;
	  }


	if (!username) {
#if defined(SingleSignon)
          username = SI_Prompt(ce->window_id,
                               XP_GetString(XP_PROMPT_ENTER_USERNAME),
                               "",
                               cd->control_con->hostname);

#else
	  username = FE_Prompt(ce->window_id,
						   XP_GetString(XP_PROMPT_ENTER_USERNAME),
						   username ? username : "");
#endif
	  
	  /* reset net_news_last_username_probably_valid to false */
	  net_news_last_username_probably_valid = FALSE;
	  if(!username) {
		ce->URL_s->error_msg = 
		  NET_ExplainErrorDetails( MK_NNTP_AUTH_FAILED, "Aborted by user");
		return(MK_NNTP_AUTH_FAILED);
	  }
	  else {
		StrAllocCopy(last_username, username);
		StrAllocCopy(last_username_hostname, cd->control_con->hostname);
	  }
	}

#ifdef CACHE_NEWSGRP_PASSWORD
	if (cd->pane && !MSG_GetNewsgroupUsername(cd->pane)) {
	  munged_username = SECNAV_MungeString(username);
	  MSG_SetNewsgroupUsername(cd->pane, munged_username);
	  PR_FREEIF(munged_username);
	}
#endif

	StrAllocCopy(command, "AUTHINFO user ");
	StrAllocCat(command, username);
	StrAllocCat(command, CRLF);

	ce->status = (int) NET_BlockingWrite(ce->socket, command, PL_strlen(command));
	NNTP_LOG_WRITE(command);

	FREE(command);
	FREE(username);

	cd->next_state = NNTP_RESPONSE;
	cd->next_state_after_response = NNTP_AUTHORIZE_RESPONSE;;

	cd->pause_for_read = TRUE;

	return ce->status;
}

PRIVATE int
net_news_authorize_response(ActiveEntry * ce)
{
    NewsConData * cd = (NewsConData *)ce->con_data;
	static char * last_password = 0;
	static char * last_password_hostname = 0;

    if (281 == cd->response_code || 250 == cd->response_code) 
	  {
		/* successful login */

		/* If we're here because the host demanded authentication before we
		 * even sent a single command, then jump back to the beginning of everything
		 */
		if (!cd->mode_reader_performed)
			cd->next_state = NNTP_SEND_MODE_READER;
		/* If we're here because the host needs pushed authentication, then we 
		 * should jump back to SEND_LIST_EXTENSIONS
		 */
		else if (MSG_GetNewsHostPushAuth(cd->host))
			cd->next_state = SEND_LIST_EXTENSIONS;
		else
			/* Normal authentication */
			cd->next_state = SEND_FIRST_NNTP_COMMAND;

		net_news_last_username_probably_valid = TRUE;
		return(0); 
	  }
	else if (381 == cd->response_code)
	  {
		/* password required
		 */	
		char * command = 0;
		char * password = 0, * munged_password = 0;
		char * cp;

		if (cd->pane)
		{
			password = SECNAV_UnMungeString(MSG_GetNewsgroupPassword(cd->pane));
			/* use it only once */
			MSG_SetNewsgroupPassword(cd->pane, NULL);
		}

        if (net_news_last_username_probably_valid 
			&& last_password
			&& last_password_hostname
			&& !PL_strcasecmp(last_password_hostname, cd->control_con->hostname))
          {
#ifdef CACHE_NEWSGRP_PASSWORD
			if (cd->pane)
				password = SECNAV_UnMungeString(MSG_GetNewsgroupPassword(cd->pane));
#else
            StrAllocCopy(password, last_password);
#endif
          }
        else if ((cp = PL_strchr(cd->control_con->hostname, '@')) != NULL)
          {
            /* in this case the username and possibly
             * the password are in the URL
             */
            char * colon;
            *cp = '\0';
    
            colon = PL_strchr(cd->control_con->hostname, ':');
            if(colon)
			  {
                *colon = '\0';
    
            	StrAllocCopy(password, colon+1);
            	StrAllocCopy(last_password, colon+1);
            	StrAllocCopy(last_password_hostname, cp+1);

                *colon = ':';
			  }
    
            *cp = '@';
    
          }
		if (!password) {
#if defined(SingleSignon)
                  password = SI_PromptPassword
                      (ce->window_id,
                      XP_GetString
                          (XP_PLEASE_ENTER_A_PASSWORD_FOR_NEWS_SERVER_ACCESS),
                      cd->control_con->hostname,
                      TRUE, TRUE);
#else
		  password = 
			FE_PromptPassword(ce->window_id, XP_GetString( 
			           XP_PLEASE_ENTER_A_PASSWORD_FOR_NEWS_SERVER_ACCESS ) );
#endif
		  net_news_last_username_probably_valid = FALSE;
		}
		  
		if(!password)	{
		  ce->URL_s->error_msg = 
			NET_ExplainErrorDetails(MK_NNTP_AUTH_FAILED, "Aborted by user");
		  return(MK_NNTP_AUTH_FAILED);
		}
		else {
		  StrAllocCopy(last_password, password);
		  StrAllocCopy(last_password_hostname, cd->control_con->hostname);
		}

#ifdef CACHE_NEWSGRP_PASSWORD
		if (cd->pane && !MSG_GetNewsgroupPassword(cd->pane)) {
		  munged_password = SECNAV_MungeString(password);
		  MSG_SetNewsgroupPassword(cd->pane, munged_password);
		  PR_FREEIF(munged_password);
		}
#endif

		StrAllocCopy(command, "AUTHINFO pass ");
		StrAllocCat(command, password);
		StrAllocCat(command, CRLF);
	
		ce->status = (int) NET_BlockingWrite(ce->socket, command, PL_strlen(command));
		NNTP_LOG_WRITE(command);

		FREE(command);
		FREE(password);

		cd->next_state = NNTP_RESPONSE;
		cd->next_state_after_response = NNTP_PASSWORD_RESPONSE;
		cd->pause_for_read = TRUE;

		return ce->status;
	  }
	else
	  {
		ce->URL_s->error_msg = NET_ExplainErrorDetails(
									MK_NNTP_AUTH_FAILED,
									cd->response_txt ? cd->response_txt : "");
#ifdef CACHE_NEWSGRP_PASSWORD
		if (cd->pane)
		  MSG_SetNewsgroupUsername(cd->pane, NULL);
#endif
		net_news_last_username_probably_valid = FALSE;

        return(MK_NNTP_AUTH_FAILED);
	  }
		
	PR_ASSERT(0); /* should never get here */
	return(-1);

}

PRIVATE int
net_news_password_response(ActiveEntry * ce)
{
    NewsConData * cd = (NewsConData *)ce->con_data;

    if (281 == cd->response_code || 250 == cd->response_code) 
	  {
        /* successful login */
         
		/* If we're here because the host demanded authentication before we
		 * even sent a single command, then jump back to the beginning of everything
		 */
		if (!cd->mode_reader_performed)
			cd->next_state = NNTP_SEND_MODE_READER;
		/* If we're here because the host needs pushed authentication, then we 
		 * should jump back to SEND_LIST_EXTENSIONS
		 */
		else if (MSG_GetNewsHostPushAuth(cd->host))
			cd->next_state = SEND_LIST_EXTENSIONS;
		else
			/* Normal authentication */
			cd->next_state = SEND_FIRST_NNTP_COMMAND;

		net_news_last_username_probably_valid = TRUE;
		MSG_ResetXOVER( ce->URL_s->msg_pane, &cd->xover_parse_state );
        return(0);
	  }
	else
	  {
		ce->URL_s->error_msg = NET_ExplainErrorDetails(
									MK_NNTP_AUTH_FAILED,
									cd->response_txt ? cd->response_txt : "");
#ifdef CACHE_NEWSGRP_PASSWORD
		if (cd->pane)
		  MSG_SetNewsgroupPassword(cd->pane, NULL);
#endif
        return(MK_NNTP_AUTH_FAILED);
	  }
		
	PR_ASSERT(0); /* should never get here */
	return(-1);
}

PRIVATE int
net_display_newsgroups (ActiveEntry *ce)
{
    NewsConData * cd = (NewsConData *)ce->con_data;

	cd->next_state = NEWS_DONE;
    cd->pause_for_read = FALSE;

	NNTP_LOG_NOTE(("about to display newsgroups. path: %s",cd->path));

#if 0
	/* #### Now ignoring "news:alt.fan.*"
	   Need to open the root tree of the default news host and keep
	   opening one child at each level until we've exhausted the
	   wildcard...
	 */
	if(rv < 0)
       return(rv);  
	else
#endif
       return(MK_DATA_LOADED);  /* all finished */
}

PRIVATE int
net_newgroups_begin (ActiveEntry *ce)
{
    NewsConData * cd = (NewsConData *)ce->con_data;

	cd->next_state = NNTP_NEWGROUPS;
	NET_Progress(ce->window_id, XP_GetString(XP_PROGRESS_RECEIVE_NEWSGROUP));

	ce->bytes_received = 0;

	return(ce->status);

}

PRIVATE int
net_process_newgroups (ActiveEntry *ce)
{
    NewsConData * cd = (NewsConData *)ce->con_data;
	char *line, *s, *s1, *s2, *flag;
	int32 oldest, youngest;

    ce->status = NET_BufferedReadLine(ce->socket, &line, &cd->data_buf,
                                        &cd->data_buf_size, (Bool*)&cd->pause_for_read);

    if(ce->status == 0)
      {
        cd->next_state = NNTP_ERROR;
        cd->pause_for_read = FALSE;
        ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_NNTP_SERVER_ERROR);
        return(MK_NNTP_SERVER_ERROR);
      }

    if(!line)
        return(ce->status);  /* no line yet */

    if(ce->status<0)
      {
        ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_TCP_READ_ERROR, PR_GetOSError());

        /* return TCP error
         */
        return MK_TCP_READ_ERROR;
      }

    /* End of list? 
	 */
    if (line[0]=='.' && line[1]=='\0')
      {
		cd->pause_for_read = FALSE;
		if (MSG_QueryNewsExtension(cd->host, "XACTIVE"))
		{
			cd->group_name = MSG_GetFirstGroupNeedingExtraInfo(cd->host);
			if (cd->group_name)
			{
				cd->next_state = NNTP_LIST_XACTIVE;
#ifdef DEBUG_bienvenu
				PR_LogPrint("listing xactive for %s\n", cd->group_name);
#endif
				return 0;
			}
		}
		cd->next_state = NEWS_DONE;

		if(ce->bytes_received == 0)
		  {
			/* #### no new groups */
		  }

		NET_SortNewsgroups(cd->control_con->hostname, cd->control_con->secure);
		NET_SaveNewsgroupsToDisk(cd->control_con->hostname, cd->control_con->secure);
		if(ce->status > 0)
        	return MK_DATA_LOADED;
		else
        	return ce->status;
      }
    else if (line [0] == '.' && line [1] == '.')
      /* The NNTP server quotes all lines beginning with "." by doubling it. */
      line++;

    /* almost correct
     */
    if(ce->status > 1)
      {
        ce->bytes_received += ce->status;
        FE_GraphProgress(ce->window_id, ce->URL_s, ce->bytes_received, ce->status, ce->URL_s->content_length);
      }

    /* format is "rec.arts.movies.past-films 7302 7119 y"
	 */
	s = PL_strchr (line, ' ');
	if (s)
	  {
		*s = 0;
		s1 = s+1;
		s = PL_strchr (s1, ' ');
		if (s)
		  {
			*s = 0;
			s2 = s+1;
			s = PL_strchr (s2, ' ');
			if (s)
			{
			  *s = 0;
			  flag = s+1;
			}
		  }
	  }
	youngest = s2 ? atol(s1) : 0;
	oldest   = s1 ? atol(s2) : 0;

	ce->bytes_received++;  /* small numbers of groups never seem
						   * to trigger this
						   */

	MSG_AddNewNewsGroup(ce->URL_s->msg_pane, cd->host,
						line, oldest, youngest, flag, FALSE);
	if (MSG_QueryNewsExtension(cd->host, "XACTIVE"))
	{
		MSG_SetGroupNeedsExtraInfo(cd->host, line, TRUE);
	}
    return(ce->status);
}

		
/* Ahhh, this like print's out the headers and stuff
 *
 * always returns 0
 */
PRIVATE int
net_read_news_list_begin (ActiveEntry *ce)
{
    NewsConData * cd = (NewsConData *)ce->con_data;

    cd->next_state = NNTP_READ_LIST;

	NET_Progress(ce->window_id, XP_GetString(XP_PROGRESS_RECEIVE_NEWSGROUP));
	 
    return(ce->status);


}

/* display a list of all or part of the newsgroups list
 * from the news server
 */
PRIVATE int
net_read_news_list (ActiveEntry *ce)
{
    char * line;
    char * description;
    int i=0;
    NewsConData * cd = (NewsConData *)ce->con_data;
   
    ce->status = NET_BufferedReadLine(ce->socket, &line, &cd->data_buf, 
                       					&cd->data_buf_size, (Bool*) &cd->pause_for_read);

    if(ce->status == 0)
      {
        cd->next_state = NNTP_ERROR;
        cd->pause_for_read = FALSE;
		ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_NNTP_SERVER_ERROR);
        return(MK_NNTP_SERVER_ERROR);
      }

    if(!line)
        return(ce->status);  /* no line yet */

    if(ce->status<0)
	  {
        ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_TCP_READ_ERROR, PR_GetOSError());

        /* return TCP error
         */
        return MK_TCP_READ_ERROR;
	  }

            /* End of list? */
    if (line[0]=='.' && line[1]=='\0')
      {
		if (MSG_QueryNewsExtension(cd->host, "LISTPNAMES"))
		{
			cd->next_state = NNTP_LIST_PRETTY_NAMES;
		}
		else
		{
			cd->next_state = DISPLAY_NEWSGROUPS;
		}
        cd->pause_for_read = FALSE;

		NET_SortNewsgroups(cd->control_con->hostname, cd->control_con->secure);
		NET_SaveNewsgroupsToDisk(cd->control_con->hostname, cd->control_con->secure);
        return 0;  
      }
	else if (line [0] == '.' && line [1] == '.')
	  /* The NNTP server quotes all lines beginning with "." by doubling it. */
	  line++;

    /* almost correct
     */
    if(ce->status > 1)
      {
    	ce->bytes_received += ce->status;
        FE_GraphProgress(ce->window_id, ce->URL_s, ce->bytes_received, ce->status, ce->URL_s->content_length);
	  }

    /* find whitespace seperator if it exits */
    for(i=0; line[i] != '\0' && !NET_IS_SPACE(line[i]); i++)
        ;  /* null body */

    if(line[i] == '\0')
        description = &line[i];
    else
        description = &line[i+1];

    line[i] = 0; /* terminate group name */

	/* store all the group names 
	 */
	MSG_AddNewNewsGroup(ce->URL_s->msg_pane, cd->host,
						line, 0, 0, "", FALSE);


    return(ce->status);
}




/* start the xover command
 */
PRIVATE int
net_read_xover_begin (ActiveEntry *ce)
{
    NewsConData * cd = (NewsConData *)ce->con_data;
    int32 count;     /* Response fields */

	/* Make sure we never close and automatically reopen the connection at this
	   point; we'll confuse libmsg too much... */

	cd->some_protocol_succeeded = TRUE;

	/* We have just issued a GROUP command and read the response.
	   Now parse that response to help decide which articles to request
	   xover data for.
	 */
    sscanf(cd->response_txt,
#ifdef __alpha
		   "%d %d %d", 
#else
		   "%ld %ld %ld", 
#endif
		   &count, 
		   &cd->first_possible_art, 
		   &cd->last_possible_art);

	/* We now know there is a summary line there; make sure it has the
	   right numbers in it. */
	ce->status = MSG_DisplaySubscribedGroup(cd->pane,
											cd->host,
											cd->group_name,
											cd->first_possible_art,
											cd->last_possible_art,
											count, TRUE);
	if (ce->status < 0) return ce->status;

	cd->num_loaded = 0;
	cd->num_wanted = net_NewsChunkSize > 0 ? net_NewsChunkSize : 1L << 30;

	cd->next_state = NNTP_FIGURE_NEXT_CHUNK;
	cd->pause_for_read = FALSE;
	return 0;
}



PRIVATE int
net_figure_next_chunk(ActiveEntry *ce)
{
    NewsConData * cd = (NewsConData *)ce->con_data;

	XP_Bool secure_p = (ce->URL_s->address[0] == 's' ||
						ce->URL_s->address[0] == 'S');
	char *host_and_port = NET_ParseURL (ce->URL_s->address, GET_HOST_PART);

	if (!host_and_port) return MK_OUT_OF_MEMORY;

	if (cd->first_art > 0) {
	  ce->status = MSG_AddToKnownArticles(cd->pane, cd->host,
										 cd->group_name,
										 cd->first_art, cd->last_art);
	  if (ce->status < 0) {
		FREEIF (host_and_port);
		return ce->status;
	  }
	}
										 

	if (cd->num_loaded >= cd->num_wanted) {
	  FREEIF (host_and_port);
	  cd->next_state = NEWS_PROCESS_XOVER;
	  cd->pause_for_read = FALSE;
	  return 0;
	}


	ce->status = MSG_GetRangeOfArtsToDownload(cd->pane,
											 &cd->xover_parse_state,
											 cd->host,
											 cd->group_name,
											 cd->first_possible_art,
											 cd->last_possible_art,
											 cd->num_wanted - cd->num_loaded,
											 &(cd->first_art),
											 &(cd->last_art));

	if (ce->status < 0) {
	  FREEIF (host_and_port);
	  return ce->status;
	}


	if (cd->first_art <= 0 || cd->first_art > cd->last_art) {
	  /* Nothing more to get. */
	  FREEIF (host_and_port);
	  cd->next_state = NEWS_PROCESS_XOVER;
	  cd->pause_for_read = FALSE;
	  return 0;
	}

    NNTP_LOG_NOTE(("    Chunk will be (%ld-%ld)", cd->first_art, cd->last_art));

	cd->article_num = cd->first_art;
	ce->status = MSG_InitXOVER (cd->pane,
							   cd->host, cd->group_name,
							   cd->first_art, cd->last_art,
							   cd->first_possible_art, cd->last_possible_art,
							   &cd->xover_parse_state);
	FREEIF (host_and_port);

	if (ce->status < 0) {
	  return ce->status;
	}

	cd->pause_for_read = FALSE;
	if (cd->control_con->no_xover) cd->next_state = NNTP_READ_GROUP;
	else cd->next_state = NNTP_XOVER_SEND;

	return 0;
}

PRIVATE int
net_xover_send (ActiveEntry *ce)
{		
    NewsConData * cd = (NewsConData *)ce->con_data;

    PR_snprintf(cd->output_buffer, 
				OUTPUT_BUFFER_SIZE,
				"XOVER %ld-%ld" CRLF, 
				cd->first_art, 
				cd->last_art);

	/* printf("XOVER %ld-%ld\n", cd->first_art, cd->last_art); */

	NNTP_LOG_WRITE(cd->output_buffer);

    cd->next_state = NNTP_RESPONSE;
    cd->next_state_after_response = NNTP_XOVER_RESPONSE;
    cd->pause_for_read = TRUE;

	NET_Progress(ce->window_id, XP_GetString(XP_PROGRESS_RECEIVE_LISTARTICLES));

    return((int) NET_BlockingWrite(ce->socket, 
								   cd->output_buffer, 
								   PL_strlen(cd->output_buffer)));
	NNTP_LOG_WRITE(cd->output_buffer);

}

/* see if the xover response is going to return us data
 * if the proper code isn't returned then assume xover
 * isn't supported and use
 * normal read_group
 */
PRIVATE int
net_read_xover_response (ActiveEntry *ce)
{
    NewsConData * cd = (NewsConData *)ce->con_data;

#ifdef TEST_NO_XOVER_SUPPORT
	cd->response_code = 500; /* pretend XOVER generated an error */
#endif

    if(cd->response_code != 224)
      {
        /* If we didn't get back "224 data follows" from the XOVER request,
		   then that must mean that this server doesn't support XOVER.  Or
		   maybe the server's XOVER support is busted or something.  So,
		   in that case, fall back to the very slow HEAD method.

		   But, while debugging here at HQ, getting into this state means
		   something went very wrong, since our servers do XOVER.  Thus
		   the assert.
         */
		/*PR_ASSERT (0);*/
		cd->next_state = NNTP_READ_GROUP;
		cd->control_con->no_xover = TRUE;
      }
    else
      {
        cd->next_state = NNTP_XOVER;
      }

    return(0);  /* continue */
}

/* process the xover list as it comes from the server
 * and load it into the sort list.  
 */
PRIVATE int
net_read_xover (ActiveEntry *ce)
{
    NewsConData * cd = (NewsConData *)ce->con_data;
    char *line;

    ce->status = NET_BufferedReadLine(ce->socket, &line, &cd->data_buf,
									 &cd->data_buf_size,
									 (Bool*)&cd->pause_for_read);

    if(ce->status == 0)
      {
		NNTP_LOG_NOTE(("received unexpected TCP EOF!!!!  aborting!"));
        cd->next_state = NNTP_ERROR;
        cd->pause_for_read = FALSE;
		ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_NNTP_SERVER_ERROR);
        return(MK_NNTP_SERVER_ERROR);
      }

    if(!line)
	  {
	    return(ce->status);  /* no line yet or TCP error */
	  }

	if(ce->status<0) 
	  {
        ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_TCP_READ_ERROR,
													  PR_GetOSError());

        /* return TCP error
         */
        return MK_TCP_READ_ERROR;
	  }

    if(line[0] == '.' && line[1] == '\0')
      {
		cd->next_state = NNTP_FIGURE_NEXT_CHUNK;
		cd->pause_for_read = FALSE;
		return(0);
      }
	else if (line [0] == '.' && line [1] == '.')
	  /* The NNTP server quotes all lines beginning with "." by doubling it. */
	  line++;

    /* almost correct
     */
    if(ce->status > 1)
      {
        ce->bytes_received += ce->status;
/*        FE_GraphProgress(ce->window_id, ce->URL_s, ce->bytes_received, ce->status,
						 ce->URL_s->content_length);
*/
	  }

	ce->status = MSG_ProcessXOVER (cd->pane, line, &cd->xover_parse_state);

	cd->num_loaded++;

    return ce->status; /* keep going */
}


/* Finished processing all the XOVER data.
 */
PRIVATE int
net_process_xover (ActiveEntry *ce)
{
    NewsConData * cd = (NewsConData *)ce->con_data;


	/*	if (cd->xover_parse_state) { ### dmb - we need a different check */
	  ce->status = MSG_FinishXOVER (cd->pane, &cd->xover_parse_state, 0);
	  PR_ASSERT (!cd->xover_parse_state);
	  if (ce->status < 0) return ce->status;

	cd->next_state = NEWS_DONE;

    return(MK_DATA_LOADED);
}
 

PRIVATE int net_profile_add (ActiveEntry *ce)
{
#if 0
	NewsConData *cd = (NewsConData*) ce->con_data;
	char *slash = PL_strrchr(ce->URL_s->address, '/');
	if (slash)
	{
		PL_strcpy (cd->output_buffer, slash + 1);
		PL_strcat (cd->output_buffer, CRLF);
		ce->status = (int) NET_BlockingWrite(ce->socket,cd->output_buffer,PL_strlen(cd->output_buffer));
		NNTP_LOG_WRITE(cd->output_buffer);

		cd->next_state = NNTP_RESPONSE;
		cd->next_state_after_response = NNTP_PROFILE_ADD_RESPONSE;
		cd->pause_for_read = TRUE;
	}
	else
	{
		cd->next_state = NNTP_ERROR;
		ce->status = -1;
	}

	return ce->status;
#else
	PR_ASSERT(FALSE);
	return -1;
#endif
}

PRIVATE int net_profile_add_response (ActiveEntry *ce)
{
	NewsConData *cd = (NewsConData*) ce->con_data;

	if (cd->response_code >= 200 && cd->response_code <= 299)
	{
		MSG_AddProfileGroup (cd->pane, cd->host, cd->response_txt);
		cd->next_state = NEWS_DONE;
	}
	else
	{
		cd->next_state = NNTP_ERROR;
		cd->pause_for_read = FALSE;
		ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_NNTP_SERVER_ERROR);
		return MK_NNTP_SERVER_ERROR;
	}

	return ce->status;
}

PRIVATE int net_profile_delete (ActiveEntry *ce)
{
#if 0
	NewsConData *cd = (NewsConData*) ce->con_data;
	char *slash = PL_strrchr(ce->URL_s->address, '/');
	if (slash)
	{
		PL_strcpy (cd->output_buffer, slash + 1);
		PL_strcat (cd->output_buffer, CRLF);
		ce->status = (int) NET_BlockingWrite(ce->socket,cd->output_buffer,PL_strlen(cd->output_buffer));
		NNTP_LOG_WRITE(cd->output_buffer);

		cd->next_state = NNTP_RESPONSE;
		cd->next_state_after_response = NNTP_PROFILE_DELETE_RESPONSE;
		cd->pause_for_read = TRUE;
	}
	else
	{
		cd->next_state = NNTP_ERROR;
		ce->status = -1;
	}

	return ce->status;
#else
	PR_ASSERT(FALSE);
	return -1;
#endif
}

PRIVATE int net_profile_delete_response (ActiveEntry *ce)
{
	NewsConData *cd = (NewsConData*) ce->con_data;

	if (cd->response_code >= 200 && cd->response_code <= 299)
		cd->next_state = NEWS_DONE;
	else
	{
		cd->next_state = NNTP_ERROR;
		cd->pause_for_read = FALSE;
		ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_NNTP_SERVER_ERROR);
		return MK_NNTP_SERVER_ERROR;
	}

	return ce->status;
}

PRIVATE
int
net_read_news_group (ActiveEntry *ce)
{
    NewsConData * cd = (NewsConData *)ce->con_data;

    if(cd->article_num > cd->last_art)
      {  /* end of groups */

		cd->next_state = NNTP_FIGURE_NEXT_CHUNK;
		cd->pause_for_read = FALSE;
		return(0);
      }
    else
      {
        PR_snprintf(cd->output_buffer, 
				   OUTPUT_BUFFER_SIZE,  
				   "HEAD %ld" CRLF, 
				   cd->article_num++);
        cd->next_state = NNTP_RESPONSE;
		cd->next_state_after_response = NNTP_READ_GROUP_RESPONSE;

        cd->pause_for_read = TRUE;

		NNTP_LOG_WRITE(cd->output_buffer);
        return((int) NET_BlockingWrite(ce->socket, cd->output_buffer, PL_strlen(cd->output_buffer)));
      }
}

/* See if the "HEAD" command was successful
 */
PRIVATE int
net_read_news_group_response (ActiveEntry *ce)
{
  NewsConData * cd = (NewsConData *)ce->con_data;

  if (cd->response_code == 221)
	{     /* Head follows - parse it:*/
	  cd->next_state = NNTP_READ_GROUP_BODY;

	  if(cd->message_id)
		*cd->message_id = '\0';

	  /* Give the message number to the header parser. */
	  return MSG_ProcessNonXOVER (cd->pane, cd->response_txt,
								  &cd->xover_parse_state);
	}
  else
	{
	  NNTP_LOG_NOTE(("Bad group header found!"));
	  cd->next_state = NNTP_READ_GROUP;
	  return(0);
	}
}

/* read the body of the "HEAD" command
 */
PRIVATE int
net_read_news_group_body (ActiveEntry *ce)
{
  NewsConData * cd = (NewsConData *)ce->con_data;
  char *line;

  ce->status = NET_BufferedReadLine(ce->socket, &line, &cd->data_buf,
								   &cd->data_buf_size, (Bool*)&cd->pause_for_read);

  if(ce->status == 0)
	{
	  cd->next_state = NNTP_ERROR;
	  cd->pause_for_read = FALSE;
	  ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_NNTP_SERVER_ERROR);
	  return(MK_NNTP_SERVER_ERROR);
	}

  /* if TCP error of if there is not a full line yet return
   */
  if(!line)
	return ce->status;

  if(ce->status < 0)
	{
	  ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_TCP_READ_ERROR, PR_GetOSError());

	  /* return TCP error
	   */
	  return MK_TCP_READ_ERROR;
	}

	NNTP_LOG_NOTE(("read_group_body: got line: %s|",line));

  /* End of body? */
  if (line[0]=='.' && line[1]=='\0')
	{
	  cd->next_state = NNTP_READ_GROUP;
	  cd->pause_for_read = FALSE;
	}
  else if (line [0] == '.' && line [1] == '.')
	/* The NNTP server quotes all lines beginning with "." by doubling it. */
	line++;

  return MSG_ProcessNonXOVER (cd->pane, line, &cd->xover_parse_state);
}


PRIVATE int
net_send_news_post_data(ActiveEntry * ce)
{
    NewsConData * cd = (NewsConData *) ce->con_data;

    /* returns 0 on done and negative on error
     * positive if it needs to continue.
     */
    ce->status = NET_WritePostData(ce->window_id, ce->URL_s,
                                  ce->socket,
								  &cd->write_post_data_data,
								  TRUE);

    cd->pause_for_read = TRUE;

    if(ce->status == 0)
      {
        /* normal done
         */
        PL_strcpy(cd->output_buffer, CRLF "." CRLF);
        NNTP_LOG_WRITE(cd->output_buffer);
        ce->status = (int) NET_BlockingWrite(ce->socket,
                                            cd->output_buffer,
                                            PL_strlen(cd->output_buffer));
		NNTP_LOG_WRITE(cd->output_buffer);

		NET_Progress(ce->window_id,
					XP_GetString(XP_MESSAGE_SENT_WAITING_NEWS_REPLY));

        NET_ClearConnectSelect(ce->window_id, ce->socket);
#ifdef XP_WIN
		if(cd->calling_netlib_all_the_time)
		{
			cd->calling_netlib_all_the_time = FALSE;
			NET_ClearCallNetlibAllTheTime(ce->window_id, "mknews");
		}
#endif
        NET_SetReadSelect(ce->window_id, ce->socket);
		ce->con_sock = 0;

        cd->next_state = NNTP_RESPONSE;
        cd->next_state_after_response = NNTP_SEND_POST_DATA_RESPONSE;
        return(0);
      }

    return(ce->status);

}


/* interpret the response code from the server
 * after the post is done
 */   
PRIVATE int
net_send_news_post_data_response(ActiveEntry * ce)
{
    NewsConData * cd = (NewsConData *) ce->con_data;


	if (cd->response_code != 240) {
	  ce->URL_s->error_msg =
		NET_ExplainErrorDetails(MK_NNTP_ERROR_MESSAGE, 
								cd->response_txt ? cd->response_txt : "");
	  if (cd->response_code == 441 
		  && MSG_GetPaneType(cd->pane) == MSG_COMPOSITIONPANE
		  && MSG_IsDuplicatePost(cd->pane) &&
		  MSG_GetCompositionMessageID(cd->pane)) {
		/* The news server won't let us post.  We suspect that we're submitting
		   a duplicate post, and that's why it's failing.  So, let's go see
		   if there really is a message out there with the same message-id.
		   If so, we'll just silently pretend everything went well. */
		PR_snprintf(cd->output_buffer, OUTPUT_BUFFER_SIZE, "STAT %s" CRLF,
					MSG_GetCompositionMessageID(cd->pane));
		cd->next_state = NNTP_RESPONSE;
		cd->next_state_after_response = NNTP_CHECK_FOR_MESSAGE;
		NNTP_LOG_WRITE(cd->output_buffer);
		return (int) NET_BlockingWrite(ce->socket, cd->output_buffer,
									   PL_strlen(cd->output_buffer));
	  }

	  MSG_ClearCompositionMessageID(cd->pane); /* So that if the user tries
													  to just post again, we
													  won't immediately decide
													  that this was a duplicate
													  message and ignore the
													  error. */
	  cd->next_state = NEWS_ERROR;
	  return(MK_NNTP_ERROR_MESSAGE);
	}
    cd->next_state = NEWS_ERROR; /* even though it worked */
	cd->pause_for_read = FALSE;
    return(MK_DATA_LOADED);
}


PRIVATE int
net_check_for_message(ActiveEntry* ce)
{
  NewsConData * cd = (NewsConData *) ce->con_data;

  cd->next_state = NEWS_ERROR;
  if (cd->response_code >= 220 && cd->response_code <= 223) {
	/* Yes, this article is already there, we're all done. */
	return MK_DATA_LOADED;
  } else {
	/* The article isn't there, so the failure we had earlier wasn't due to
	   a duplicate message-id.  Return the error from that previous
	   posting attempt (which is already in ce->URL_s->error_msg). */
	MSG_ClearCompositionMessageID(cd->pane);
	return MK_NNTP_ERROR_MESSAGE;
  }
}

PRIVATE int
net_DisplayNewsRC(ActiveEntry * ce)
{
    NewsConData * cd = (NewsConData *) ce->con_data;

	if(!cd->newsrc_performed)
	  {
		cd->newsrc_performed = TRUE;

		cd->newsrc_list_count = MSG_GetNewsRCCount(cd->pane, cd->host);
	  }
	

	
	FREEIF(cd->control_con->current_group);
	cd->control_con->current_group = MSG_GetNewsRCGroup(cd->pane, cd->host);
	

	if(cd->control_con->current_group)
      {
		/* send group command to server
		 */
		int32 percent;
		char *statusText;

		PR_snprintf(NET_Socket_Buffer, OUTPUT_BUFFER_SIZE, "GROUP %.512s" CRLF,
					cd->control_con->current_group);
    	ce->status = (int) NET_BlockingWrite(ce->socket, NET_Socket_Buffer,
											PL_strlen(NET_Socket_Buffer));
		NNTP_LOG_WRITE(NET_Socket_Buffer);

		percent = (int32) (100 * (((double) cd->newsrc_list_index) /
						 ((double) cd->newsrc_list_count)));
		FE_SetProgressBarPercent (ce->window_id, percent);
		statusText = PR_smprintf (XP_GetString(XP_PROGRESS_READ_NEWSGROUP_COUNTS), 
								(long) cd->newsrc_list_index, (long) cd->newsrc_list_count);
		if (statusText)
		{
			FE_Progress (ce->window_id, statusText);
			PR_Free(statusText);
		}

		cd->newsrc_list_index++;

		cd->pause_for_read = TRUE;
		cd->next_state = NNTP_RESPONSE;
		cd->next_state_after_response = NEWS_DISPLAY_NEWS_RC_RESPONSE;
      }
	else
	  {
		if (cd->newsrc_list_count)
		  {
			FE_SetProgressBarPercent (ce->window_id, -1);
			cd->newsrc_list_count = 0;
		  }
		else if (cd->response_code == 215)  
		  {
			/*
			 * 5-9-96 jefft 
			 * If for some reason the news server returns an empty 
			 * newsgroups list with a nntp response code 215 -- list of
			 * newsgroups follows. We set ce->status to MK_EMPTY_NEWS_LIST
			 * to end the infinite dialog loop.
			 */
			ce->status = MK_EMPTY_NEWS_LIST;
		  }
		cd->next_state = NEWS_DONE;
	
		if(ce->status > -1)
		  return MK_DATA_LOADED; 
		else
		  return(ce->status);
	  }

	return(ce->status); /* keep going */

}

/* Parses output of GROUP command */
PRIVATE int
net_DisplayNewsRCResponse(ActiveEntry * ce)
{
    NewsConData * cd = (NewsConData *) ce->con_data;

    if(cd->response_code == 211)
      {
		char *num_arts = 0, *low = 0, *high = 0, *group = 0;
		int32 first_art, last_art;

		/* line looks like:
		 *     211 91 3693 3789 comp.infosystems
		 */

		num_arts = cd->response_txt;
		low = PL_strchr(num_arts, ' ');

		if(low)
		  {
			first_art = atol(low);
			*low++ = '\0';
			high= PL_strchr(low, ' ');
		  }
		if(high)
		  {
			*high++ = '\0';
			group = PL_strchr(high, ' ');
		  }
		if(group)
		  {
			*group++ = '\0';
			/* the group name may be contaminated by "group selected" at
			   the end.  This will be space separated from the group name.
			   If a space is found in the group name terminate at that
			   point. */
			strtok(group, " ");
			last_art = atol(high);
		  }
    
		ce->status = MSG_DisplaySubscribedGroup(cd->pane,
												cd->host,
												group,
												low  ? atol(low)  : 0,
												high ? atol(high) : 0,
												atol(num_arts), FALSE);
		if (ce->status < 0)
		  return ce->status;
	  }
	  else if (cd->response_code == 411)
	  {
		  MSG_GroupNotFound(cd->pane, cd->host, cd->control_con->current_group, FALSE);
	  }
	  /* it turns out subscribe ui depends on getting this displaysubscribedgroup call,
	     even if there was an error.
	  */
	  if(cd->response_code != 211)
	  {
		/* only on news server error or when zero articles
		 */
		ce->status = MSG_DisplaySubscribedGroup(cd->pane,
												cd->host,
												cd->control_con->current_group,
												0, 0, 0, FALSE);

	  }

	cd->next_state = NEWS_DISPLAY_NEWS_RC;
		
	return 0;
}


PRIVATE int
net_NewsRCProcessPost(ActiveEntry *ce)
{
	return(0);
}



static void
net_cancel_done_cb (MWContext *context, void *data, int status,
					const char *file_name)
{
  ActiveEntry *ce = (ActiveEntry *) data;
  NewsConData *cd = (NewsConData *) ce->con_data;
  cd->cancel_status = status;
  PR_ASSERT(status < 0 || file_name);
  cd->cancel_msg_file = (status < 0 ? 0 : PL_strdup(file_name));
}


static int
net_start_cancel (ActiveEntry *ce)
{
  NewsConData *cd = (NewsConData *) ce->con_data;
  char *command = "POST" CRLF;

  ce->status = (int) NET_BlockingWrite(ce->socket, command, PL_strlen(command));
	NNTP_LOG_WRITE(command);

  cd->next_state = NNTP_RESPONSE;
  cd->next_state_after_response = NEWS_DO_CANCEL;
  cd->pause_for_read = TRUE;
  return (ce->status);
}


static int
net_do_cancel (ActiveEntry *ce)
{
  int status = 0;
  NewsConData *cd = (NewsConData *) ce->con_data;
  char *id, *subject, *newsgroups, *distribution, *other_random_headers, *body;
  char *from, *old_from, *news_url;
  int L;
  MSG_CompositionFields *fields = NULL;


  /* #### Should we do a more real check than this?  If the POST command
	 didn't respond with "340 Ok", then it's not ready for us to throw a
	 message at it...   But the normal posting code doesn't do this check.
	 Why?
   */
  PR_ASSERT (cd->response_code == 340);

  /* These shouldn't be set yet, since the headers haven't been "flushed" */
  PR_ASSERT (!cd->cancel_id &&
			 !cd->cancel_from &&
			 !cd->cancel_newsgroups &&
			 !cd->cancel_distribution);

  /* Write out a blank line.  This will tell mimehtml.c that the headers
	 are done, and it will call news_generate_html_header_fn which will
	 notice the fields we're interested in.
   */
  PL_strcpy (cd->output_buffer, CRLF); /* CRLF used to be LINEBREAK. 
  										 LINEBREAK is platform dependent
  										 and is only <CR> on a mac. This
										 CRLF is the protocol delimiter 
										 and not platform dependent  -km */
  ce->status = PUTSTRING(cd->output_buffer);
  if (ce->status < 0) return ce->status;

  /* Now news_generate_html_header_fn should have been called, and these
	 should have values. */
  id = cd->cancel_id;
  old_from = cd->cancel_from;
  newsgroups = cd->cancel_newsgroups;
  distribution = cd->cancel_distribution;

  PR_ASSERT (id && newsgroups);
  if (!id || !newsgroups) return -1; /* "unknown error"... */

  cd->cancel_newsgroups = 0;
  cd->cancel_distribution = 0;
  cd->cancel_from = 0;
  cd->cancel_id = 0;

  L = PL_strlen (id);

  from = MIME_MakeFromField ();
  subject = (char *) PR_Malloc (L + 20);
  other_random_headers = (char *) PR_Malloc (L + 20);
  body = (char *) PR_Malloc (PL_strlen (XP_AppCodeName) + 100);

  /* Make sure that this loser isn't cancelling someone else's posting.
	 Yes, there are occasionally good reasons to do so.  Those people
	 capable of making that decision (news admins) have other tools with
	 which to cancel postings (like telnet.)
	 Don't do this if server tells us it will validate user. DMB 3/19/97
   */
  if (!MSG_QueryNewsExtension(cd->host, "CANCELCHK"))
  {
	char *us = MSG_ExtractRFC822AddressMailboxes (from);
	char *them = MSG_ExtractRFC822AddressMailboxes (old_from);
	XP_Bool ok = (us && them && !PL_strcasecmp (us, them));
	FREEIF(us);
	FREEIF(them);
	if (!ok)
	  {
		status = MK_NNTP_CANCEL_DISALLOWED;
		ce->URL_s->error_msg = PL_strdup (XP_GetString(status));
		cd->next_state = NEWS_ERROR; /* even though it worked */
		cd->pause_for_read = FALSE;
		goto FAIL;
	  }
  }

  /* Last chance to cancel the cancel.
   */
  if (!FE_Confirm (ce->window_id, XP_GetString(MK_NNTP_CANCEL_CONFIRM)))
	{
	  status = MK_NNTP_NOT_CANCELLED;
	  goto FAIL;
	}

  news_url = ce->URL_s->address;  /* we can just post here. */

  if (!from || !subject || !other_random_headers || !body)
	{
	  status = MK_OUT_OF_MEMORY;
	  goto FAIL;
	}

  PL_strcpy (subject, "cancel ");
  PL_strcat (subject, id);

  PL_strcpy (other_random_headers, "Control: cancel ");
  PL_strcat (other_random_headers, id);
  PL_strcat (other_random_headers, CRLF);
  if (distribution)
	{
	  PL_strcat (other_random_headers, "Distribution: ");
	  PL_strcat (other_random_headers, distribution);
	  PL_strcat (other_random_headers, CRLF);
	}

  PL_strcpy (body, "This message was cancelled from within ");
  PL_strcat (body, XP_AppCodeName);
  PL_strcat (body, "." CRLF);

  /* #### Would it be a good idea to S/MIME-sign all "cancel" messages? */

  fields = MSG_CreateCompositionFields(from, 0, 0, 0, 0, 0, newsgroups,
									   0, 0, subject, id, other_random_headers,
									   0, 0, news_url,
									   FALSE, /* encrypt_p */
									   FALSE  /* sign_p */
									   );
  if (!fields)
  {
	  status = MK_OUT_OF_MEMORY;
	  goto FAIL;
  }
  MSG_SetCompFieldsBoolHeader(fields, MSG_ENCRYPTED_BOOL_HEADER_MASK, FALSE);
  MSG_SetCompFieldsBoolHeader(fields, MSG_SIGNED_BOOL_HEADER_MASK, FALSE);

  cd->cancel_status = 0;
  MSG_StartMessageDelivery (cd->pane, (void *) ce,
							fields,
							FALSE, /* digest_p */
							TRUE,  /* dont_deliver_p */
							TEXT_PLAIN, body, PL_strlen (body),
							0, /* other attachments */
							NULL, /* multipart/related chunk */
							net_cancel_done_cb);

  /* Since there are no attachments, MSG_StartMessageDelivery will run
	 net_cancel_done_cb right away (it will be called before the return.) */

  if (!cd->cancel_msg_file)
	{
	  status = cd->cancel_status;
	  PR_ASSERT (status < 0);
	  if (status >= 0) status = -1;
	  goto FAIL;
	}

  /* Now send the data - do it blocking, who cares; the message is known
	 to be very small.  First suck the whole file into memory.  Then delete
	 the file.  Then do a blocking write of the data.

	 (We could use file-posting, maybe, but I couldn't figure out how.)
   */
  {
	char *data;
	uint32 data_size, data_fp;
	XP_StatStruct st;
	int nread = 0;
	XP_File file = XP_FileOpen (cd->cancel_msg_file,
								xpFileToPost, XP_FILE_READ);
	if (! file) return -1; /* "unknown error"... */
	XP_Stat (cd->cancel_msg_file, &st, xpFileToPost);

	data_fp = 0;
	data_size = st.st_size + 20;
	data = (char *) PR_Malloc (data_size);
	if (! data)
	  {
		status = MK_OUT_OF_MEMORY;
		goto FAIL;
	  }

	while ((nread = XP_FileRead (data + data_fp, data_size - data_fp, file))
		   > 0)
	  data_fp += nread;
	data [data_fp] = 0;
	XP_FileClose (file);
	XP_FileRemove (cd->cancel_msg_file, xpFileToPost);

	PL_strcat (data, CRLF "." CRLF CRLF);
	status = NET_BlockingWrite(ce->socket, data, PL_strlen(data));
	NNTP_LOG_WRITE(data);
	PR_Free (data);
    if (status < 0)
	  {
		ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_TCP_WRITE_ERROR,
													  status);
		goto FAIL;
	  }

    cd->pause_for_read = TRUE;
	cd->next_state = NNTP_RESPONSE;
	cd->next_state_after_response = NNTP_SEND_POST_DATA_RESPONSE;
  }

 FAIL:
  FREEIF (id);
  FREEIF (from);
  FREEIF (old_from);
  FREEIF (subject);
  FREEIF (newsgroups);
  FREEIF (distribution);
  FREEIF (other_random_headers);
  FREEIF (body);
  FREEIF (cd->cancel_msg_file);
  if (fields)
	  MSG_DestroyCompositionFields(fields);

  return status;
}


static int net_xpat_send (ActiveEntry *ce)
{
	NewsConData *cd = (NewsConData *) ce->con_data;
	int status = 0;
	char *thisTerm = NULL;

	if (cd->current_search &&
		(thisTerm = PL_strchr(cd->current_search, '/')) != NULL)
	{
		/* extract the XPAT encoding for one query term */
/*		char *next_search = NULL; */
		char *command = NULL;
		char *unescapedCommand = NULL;
		char *endOfTerm = NULL;
		StrAllocCopy (command, ++thisTerm);
		endOfTerm = PL_strchr(command, '/');
		if (endOfTerm)
			*endOfTerm = '\0';
		StrAllocCat (command, CRLF);
	
		unescapedCommand = MSG_UnEscapeSearchUrl(command);

		/* send one term off to the server */
		NNTP_LOG_WRITE(command);
		status = NET_BlockingWrite(ce->socket, unescapedCommand, PL_strlen(unescapedCommand));
		NNTP_LOG_WRITE(unescapedCommand);

		cd->next_state = NNTP_RESPONSE;
		cd->next_state_after_response = NNTP_XPAT_RESPONSE;
	    cd->pause_for_read = TRUE;

		PR_Free(command);
		PR_Free(unescapedCommand);
	}
	else
	{
		cd->next_state = NEWS_DONE;
		status = MK_DATA_LOADED;
	}
	return status;
}

static int net_list_pretty_names(ActiveEntry *ce)
{
	NewsConData *cd = (NewsConData*) ce->con_data;
	PR_snprintf(cd->output_buffer, 
			OUTPUT_BUFFER_SIZE, 
			"LIST PRETTYNAMES %.512s" CRLF, 
			cd->group_name ? cd->group_name : "");
    ce->status = (int) NET_BlockingWrite(ce->socket,cd->output_buffer,PL_strlen(cd->output_buffer));
	NNTP_LOG_WRITE(cd->output_buffer);
#ifdef DEBUG_bienvenu
	PR_LogPrint(cd->output_buffer);
#endif
	cd->next_state = NNTP_RESPONSE;
	cd->next_state_after_response = NNTP_LIST_PRETTY_NAMES_RESPONSE;

	return ce->status;
}

static int net_list_pretty_names_response(ActiveEntry *ce)
{
	char *line;
	char *prettyName;

	NewsConData * cd = (NewsConData *)ce->con_data;

	if (cd->response_code != 215)
	{
		cd->next_state = DISPLAY_NEWSGROUPS;
/*		cd->next_state = NEWS_DONE; */
		cd->pause_for_read = FALSE;
		return 0;
	}

	ce->status = NET_BufferedReadLine(ce->socket, &line,
									 &cd->data_buf, 
									 &cd->data_buf_size,
									 (Bool*)&cd->pause_for_read);
	NNTP_LOG_READ(line);

	if(ce->status == 0)
	{
		cd->next_state = NNTP_ERROR;
		cd->pause_for_read = FALSE;
		ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_NNTP_SERVER_ERROR);
		return(MK_NNTP_SERVER_ERROR);
	}

	if (line)
	{
		if (line[0] != '.')
		{
			int i;
			/* find whitespace seperator if it exits */
			for (i=0; line[i] != '\0' && !NET_IS_SPACE(line[i]); i++)
				;  /* null body */

			if(line[i] == '\0')
				prettyName = &line[i];
			else
				prettyName = &line[i+1];

			line[i] = 0; /* terminate group name */
			if (i > 0)
				MSG_AddPrettyName(cd->host, line, prettyName);
#ifdef DEBUG_bienvenu
			PR_LogPrint("adding pretty name %s\n", prettyName);
#endif
		}
		else
		{
			cd->next_state = DISPLAY_NEWSGROUPS;	/* this assumes we were doing a list */
/*			cd->next_state = NEWS_DONE;	 */ /* ### dmb - don't really know */
			cd->pause_for_read = FALSE;
			return 0;
		}
	}
	return 0;
}

static int net_list_xactive(ActiveEntry *ce)
{
	NewsConData *cd = (NewsConData*) ce->con_data;
	PR_snprintf(cd->output_buffer, 
			OUTPUT_BUFFER_SIZE, 
			"LIST XACTIVE %.512s" CRLF, 
			cd->group_name);
    ce->status = (int) NET_BlockingWrite(ce->socket,cd->output_buffer,PL_strlen(cd->output_buffer));
	NNTP_LOG_WRITE(cd->output_buffer);

	cd->next_state = NNTP_RESPONSE;
	cd->next_state_after_response = NNTP_LIST_XACTIVE_RESPONSE;

	return ce->status;
}


static int net_list_xactive_response(ActiveEntry *ce)
{
	char *line;

	NewsConData * cd = (NewsConData *)ce->con_data;

	PR_ASSERT(cd->response_code == 215);
	if (cd->response_code != 215)
	{
		cd->next_state = DISPLAY_NEWSGROUPS;
/*		cd->next_state = NEWS_DONE; */
		cd->pause_for_read = FALSE;
		return MK_DATA_LOADED;
	}

	ce->status = NET_BufferedReadLine(ce->socket, &line,
									 &cd->data_buf, 
									 &cd->data_buf_size,
									 (Bool*)&cd->pause_for_read);
	NNTP_LOG_READ(line);

	if(ce->status == 0)
	{
		cd->next_state = NNTP_ERROR;
		cd->pause_for_read = FALSE;
		ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_NNTP_SERVER_ERROR);
		return(MK_NNTP_SERVER_ERROR);
	}

	/* Bug fix (sfraser) -- show download progress here
	*/
	if(ce->status > 1)
	{
		ce->bytes_received += ce->status;
		FE_GraphProgress(ce->window_id, ce->URL_s, ce->bytes_received, ce->status, ce->URL_s->content_length);
	}

	if (line)
	{
		if (line[0] != '.')
		{
			char *s = line;
			/* format is "rec.arts.movies.past-films 7302 7119 csp"
			 */
			while (*s && !NET_IS_SPACE(*s))
				s++;
			if (s)
			{
				char flags[32];	/* ought to be big enough */
				*s = 0;
				sscanf(s + 1,
			#ifdef __alpha
					   "%d %d %31s", 
			#else
					   "%ld %ld %31s", 
			#endif
					   &cd->first_possible_art, 
					   &cd->last_possible_art,
					   flags);
				MSG_AddNewNewsGroup(cd->pane, cd->host, line,
									cd->first_possible_art,
									cd->last_possible_art, flags, TRUE /* xactive flags */);
				/* we're either going to list prettynames first, or list all prettynames 
				   every time, so we won't care so much if it gets interrupted. */
#ifdef DEBUG_bienvenu
				PR_LogPrint("got xactive for %s of %s\n", line, flags);
#endif
				MSG_SetGroupNeedsExtraInfo(cd->host, line, FALSE);
			}
		}
		else
		{
			if (cd->type_wanted == NEW_GROUPS && MSG_QueryNewsExtension(cd->host, "XACTIVE"))
			{
				char *old_group_name = cd->group_name;
				cd->group_name = MSG_GetFirstGroupNeedingExtraInfo(cd->host);
				/* make sure we're not stuck on the same group... */
				if (old_group_name && cd->group_name && PL_strcmp(old_group_name, cd->group_name))
				{
					PR_Free(old_group_name);
#ifdef DEBUG_bienvenu
					PR_LogPrint("listing xactive for %s\n", cd->group_name);
#endif
					cd->next_state = NNTP_LIST_XACTIVE;
			        cd->pause_for_read = FALSE;
					return 0;
				}
				else
				{
					FREEIF(old_group_name);
					cd->group_name = NULL;
				}
			}
			if (MSG_QueryNewsExtension(cd->host, "LISTPNAMES"))
				cd->next_state = NNTP_LIST_PRETTY_NAMES;
			else
				cd->next_state = DISPLAY_NEWSGROUPS;	/* this assumes we were doing a list - who knows? */
/*			cd->next_state = NEWS_DONE;	 */ /* ### dmb - don't really know */
			cd->pause_for_read = FALSE;
			return 0;
		}
	}
	return 0;
}

static int net_list_group(ActiveEntry *ce)
{
	NewsConData *cd = (NewsConData*) ce->con_data;
	PR_snprintf(cd->output_buffer, 
			OUTPUT_BUFFER_SIZE, 
			"listgroup %.512s" CRLF, 
			cd->group_name);
	MSG_InitAddArticleKeyToGroup(cd->pane, cd->host, cd->group_name,
							   &cd->newsgroup_parse_state);
    ce->status = (int) NET_BlockingWrite(ce->socket,cd->output_buffer,PL_strlen(cd->output_buffer));
	NNTP_LOG_WRITE(cd->output_buffer);

	cd->next_state = NNTP_RESPONSE;
	cd->next_state_after_response = NNTP_LIST_GROUP_RESPONSE;

	return ce->status;
}

static int net_list_group_response(ActiveEntry *ce)
{
	char *line;

	NewsConData * cd = (NewsConData *)ce->con_data;

	PR_ASSERT(cd->response_code == 211);
	if (cd->response_code != 211)
	{
		cd->next_state = NEWS_DONE; 
		cd->pause_for_read = FALSE;
		return MK_DATA_LOADED;
	}

	ce->status = NET_BufferedReadLine(ce->socket, &line,
									 &cd->data_buf, 
									 &cd->data_buf_size,
									 (Bool*)&cd->pause_for_read);
	NNTP_LOG_READ(line);

	if(ce->status == 0)
	{
		cd->next_state = NNTP_ERROR;
		cd->pause_for_read = FALSE;
		ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_NNTP_SERVER_ERROR);
		return(MK_NNTP_SERVER_ERROR);
	}

	if (line)
	{
		if (line[0] != '.')
		{
			long found_id = MSG_MESSAGEKEYNONE;
			sscanf(line, "%ld", &found_id);
			MSG_AddArticleKeyToGroup(cd->newsgroup_parse_state, (int32) found_id);
		}
		else
		{
			cd->next_state = NEWS_DONE;	 /* ### dmb - don't really know */
			cd->pause_for_read = FALSE;
			return 0;
		}
	}
	return 0;
}

static int net_xpat_response (ActiveEntry *ce)
{
	char *line;
	NewsConData * cd = (NewsConData *)ce->con_data;

	if (cd->response_code != 221)
	{
		ce->URL_s->error_msg  = NET_ExplainErrorDetails(MK_NNTP_ERROR_MESSAGE, cd->response_txt);
    	cd->next_state = NNTP_ERROR;
		cd->pause_for_read = FALSE;
		return MK_NNTP_SERVER_ERROR;
	}

	ce->status = NET_BufferedReadLine(ce->socket, &line,
									 &cd->data_buf, 
									 &cd->data_buf_size,
									 (Bool*)&cd->pause_for_read);
	NNTP_LOG_READ(line);

	if(ce->status == 0)
	{
		cd->next_state = NNTP_ERROR;
		cd->pause_for_read = FALSE;
		ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_NNTP_SERVER_ERROR);
		return(MK_NNTP_SERVER_ERROR);
	}

	if (line)
	{
		if (line[0] != '.')
		{
			long articleNumber;
			sscanf(line, "%ld", &articleNumber);
			MSG_AddNewsXpatHit (ce->window_id, (uint32) articleNumber);
		}
		else
		{
			/* set up the next term for next time around */
			char *nextTerm = PL_strchr(cd->current_search, '/');
			if (nextTerm)
				cd->current_search = ++nextTerm;
			else
				cd->current_search = NULL;

			cd->next_state = NNTP_XPAT_SEND;
			cd->pause_for_read = FALSE;
			return 0;
		}
	}
	return 0;
}


PRIVATE int net_nntp_search(ActiveEntry *ce)
{
	PR_ASSERT(FALSE);
	return 0;
}

PRIVATE int net_nntp_search_response(ActiveEntry *ce)
{
	NewsConData * cd = (NewsConData *)ce->con_data;
	
	if (cd->response_code >= 200 && cd->response_code <= 299)
		cd->next_state = NNTP_SEARCH_RESULTS;
	else
		cd->next_state = NEWS_DONE;
	cd->pause_for_read = FALSE;
	return 0;
}

PRIVATE int net_nntp_search_results (ActiveEntry *ce)
{
	NewsConData *cd = (NewsConData*) ce->con_data;

	char *line = NULL;
	ce->status = NET_BufferedReadLine (ce->socket, &line, &cd->data_buf,
		&cd->data_buf_size, (Bool*)&cd->pause_for_read);

	if(ce->status == 0)
	{
		cd->next_state = NNTP_ERROR;
		cd->pause_for_read = FALSE;
		ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_NNTP_SERVER_ERROR);
		return MK_NNTP_SERVER_ERROR;
	}
	if (!line)
		return ce->status;  /* no line yet */
	if (ce->status < 0)
	{
		ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_TCP_READ_ERROR,
                                                       PR_GetOSError());
		/* return TCP error */
		return MK_TCP_READ_ERROR;
	}

	if ('.' != line[0])
		MSG_AddNewsSearchHit (ce->window_id, line);
	else
	{
		/* all overview lines received */
		cd->next_state = NEWS_DONE;
		cd->pause_for_read = FALSE;
	}

	return ce->status;
}

/* The main news load init routine, and URL parser.
   The syntax of news URLs is:

	 To list all hosts:
	   news:

	 To list all groups on a host, or to post a new message:
	   news://HOST

	 To list some articles in a group:
	   news:GROUP
	   news:/GROUP
	   news://HOST/GROUP

	 To list a particular range of articles in a group:
	   news:GROUP/N1-N2
	   news:/GROUP/N1-N2
	   news://HOST/GROUP/N1-N2

	 To retrieve an article:
	   news:MESSAGE-ID                (message-id must contain "@")

    To cancel an article:
	   news:MESSAGE-ID?cancel

	 (Note that message IDs may contain / before the @:)

	   news:SOME/ID@HOST?headers=all
	   news:/SOME/ID@HOST?headers=all
	   news:/SOME?ID@HOST?headers=all
        are the same as
	   news://HOST/SOME/ID@HOST?headers=all
	   news://HOST//SOME/ID@HOST?headers=all
	   news://HOST//SOME?ID@HOST?headers=all
        bug: if the ID is <//xxx@host> we will parse this correctly:
          news://non-default-host///xxx@host
        but will mis-parse it if it's on the default host:
          news://xxx@host
        whoever had the idea to leave the <> off the IDs should be flogged.
		So, we'll make sure we quote / in message IDs as %2F.

 */
int
NET_parse_news_url (const char *url,
					char **host_and_portP,
					XP_Bool *securepP,
					char **groupP,
					char **message_idP,
					char **command_specific_dataP)
{
  int status = 0;
  XP_Bool secure_p = FALSE;
  char *host_and_port = 0;
  unsigned long port = 0;
  char *group = 0;
  char *message_id = 0;
  char *command_specific_data = 0;
  const char *path_part;
  char *s;

  if (url[0] == 's' || url[1] == 'S')
	{
	  secure_p = TRUE;
	  url++;
	}

  host_and_port = NET_ParseURL (url, GET_HOST_PART);
  if (!host_and_port || !*host_and_port)
	{
	  char* defhost = NULL;
	  int32 defport = 0;
	  FREEIF(host_and_port);
	  PREF_CopyCharPref("network.hosts.nntp_server", &defhost);
	  if (defhost && *defhost == '\0') {
		PR_Free(defhost);
		defhost = NULL;
	  }
	  if (defhost) {
		PREF_GetIntPref("news.server_port", &defport);
		if (defport == (secure_p ? SECURE_NEWS_PORT : NEWS_PORT)) {
		  defport = 0;
		}
		if (defport) {
		  host_and_port = PR_smprintf("%s:%ld", defhost, (long) defport);
		  PR_Free(defhost);
		} else {
		  host_and_port = defhost;
		}
		if (!host_and_port) {
		  status = MK_OUT_OF_MEMORY;
		  goto FAIL;
		}
	  }
	}

  if (!host_and_port)
	{
	  status = MK_NO_NEWS_SERVER;
	  goto FAIL;
	}

  /* If a port was specified, but it was the default port, pretend
	 it wasn't specified.
   */
  s = PL_strchr (host_and_port, ':');
  if (s && sscanf (s+1, " %lu ", &port) == 1 && port == (secure_p ? SECURE_NEWS_PORT : NEWS_PORT))
	*s = 0;

  path_part = PL_strchr (url, ':');
  PR_ASSERT (path_part);
  if (!path_part)
	{
	  status = -1;
	  goto FAIL;
	}
  path_part++;
  if (path_part[0] == '/' && path_part[1] == '/')
	{
	  /* Skip over host name. */
	  path_part = PL_strchr (path_part + 2, '/');
	  if (path_part)
		path_part++;
	}
  if (!path_part)
	path_part = "";

  group = PL_strdup (path_part);
  if (!group)
	{
	  status = MK_OUT_OF_MEMORY;
	  goto FAIL;
	}

  NET_UnEscape (group);

  /* "group" now holds the part after the host name:
	 "message@id?search" or "/group/xxx?search" or "/message?id@xx?search"

	 If there is an @, this is a message ID; else it is a group.
	 Either way, there may be search data at the end.
   */

  s = PL_strchr (group, '@');
  if (s)
	{
	  message_id = group;
	  group = 0;
	}
  else if (!*group)
	{
	  PR_Free (group);
	  group = 0;
	}

  /* At this point, the search data is attached to `message_id' (if there
	 is one) or `group' (if there is one) or `host_and_port' (otherwise.)
	 Seperate the search data from what it is clinging to, being careful
	 to interpret the "?" only if it comes after the "@" in an ID, since
	 the syntax of message IDs is tricky.  (There may be a ? in the
	 random-characters part of the ID (before @), but not in the host part
	 (after @).)
   */
  if (message_id || group || host_and_port)
	{
	  char *start;
	  if (message_id)
		{
		  /* Move past the @. */
		  PR_ASSERT (s && *s == '@');
		  start = s;
		}
	  else
		{
		  start = group ? group : host_and_port;
		}

	  /* Take off the "?" or "#" search data */
	  for (s = start; *s; s++)
		if (*s == '?' || *s == '#')
		  break;
	  if (*s)
		{
		  command_specific_data = PL_strdup (s);
		  *s = 0;
		  if (!command_specific_data)
			{
			  status = MK_OUT_OF_MEMORY;
			  goto FAIL;
			}
		}

	  /* Discard any now-empty strings. */
	  if (message_id && !*message_id)
		{
		  PR_Free (message_id);
		  message_id = 0;
		}
	  else if (group && !*group)
		{
		  PR_Free (group);
		  group = 0;
		}
	}

  FAIL:
  PR_ASSERT (!message_id || message_id != group);
  if (status >= 0)
	{
	  *host_and_portP = host_and_port;
	  *securepP = secure_p;
	  *groupP = group;
	  *message_idP = message_id;
	  *command_specific_dataP = command_specific_data;
	}
  else
	{
	  FREEIF (host_and_port);
	  FREEIF (group);
	  FREEIF (message_id);
	  FREEIF (command_specific_data);
	}
  return status;
}


#if 0
static void
test (const char *url)
{
  char *host_and_port = 0;
  XP_Bool secure_p = FALSE;
  char *group = 0;
  char *message_id = 0;
  char *command_specific_data = 0;
  int status = NET_parse_news_url (url, &host_and_port, &secure_p,
								   &group, &message_id, &command_specific_data);
  if (status < 0)
	fprintf (stderr, "%s: status %d\n", url, status);
  else
	{
	  fprintf (stderr,
			   "%s:\n"
			   " host \"%s\", %ssecure,\n"
			   " group=\"%s\",\n"
			   " id=\"%s\",\n"
			   " search=\"%s\"\n\n",
			   url,
			   (host_and_port ? host_and_port : "(none)"),
			   (secure_p ? "" : "in"),
			   (group ? group : "(none)"),
			   (message_id ? message_id : "(none)"),
			   (command_specific_data ?  command_specific_data : "(none)"));
	}
  FREEIF (host_and_port);
  FREEIF (group);
  FREEIF (message_id);
  FREEIF (command_specific_data);
}

static void
test2 (void)
{
  test ("news:");
  test ("news:?search");
  test ("news:#anchor");
  test ("news:?search#anchor");
  test ("news:#anchor?search");
  test ("news://HOST");
  test ("news://HOST?search");
  test ("news://HOST#anchor");
  test ("news://HOST?search#anchor");
  test ("news://HOST#anchor?search");
  test ("news://HOST/");
  test ("news://HOST/?search");
  test ("news://HOST/#anchor");
  test ("news://HOST/?search#anchor");
  test ("news://HOST/#anchor?search");
  test ("news:GROUP");
  test ("news:GROUP?search");
  test ("news:GROUP#anchor");
  test ("news:GROUP?search#anchor");
  test ("news:GROUP#anchor?search");
  test ("news:/GROUP");
  test ("news:/GROUP?search");
  test ("news:/GROUP#anchor");
  test ("news:/GROUP?search#anchor");
  test ("news:/GROUP#anchor?search");
  test ("news://HOST/GROUP");
  test ("news://HOST/GROUP?search");
  test ("news://HOST/GROUP#anchor");
  test ("news://HOST/GROUP?search#anchor");
  test ("news://HOST/GROUP#anchor?search");

  test ("news:GROUP/N1-N2");
  test ("news:GROUP/N1-N2?search");
  test ("news:GROUP/N1-N2#anchor");
  test ("news:GROUP/N1-N2?search#anchor");
  test ("news:GROUP/N1-N2#anchor?search");
  test ("news:/GROUP/N1-N2");
  test ("news:/GROUP/N1-N2?search");
  test ("news:/GROUP/N1-N2#anchor");
  test ("news:/GROUP/N1-N2?search#anchor");
  test ("news:/GROUP/N1-N2#anchor?search");

  test ("news://HOST/GROUP/N1-N2");
  test ("news://HOST/GROUP/N1-N2?search");
  test ("news://HOST/GROUP/N1-N2#anchor");
  test ("news://HOST/GROUP/N1-N2?search#anchor");
  test ("news://HOST/GROUP/N1-N2#anchor?search");
  test ("news:<ID??##??ID@HOST>");
  test ("news:<ID??##??ID@HOST>?search");
  test ("news:<ID??##??ID@HOST>#anchor");
  test ("news:<ID??##??ID@HOST>?search#anchor");
  test ("news:<ID??##??ID@HOST>#anchor?search");

  test ("news:<ID/ID/ID??##??ID@HOST>");
  test ("news:<ID/ID/ID??##??ID@HOST>?search");
  test ("news:<ID/ID/ID??##??ID@HOST>#anchor");
  test ("news:<ID/ID/ID??##??ID@HOST>?search#anchor");
  test ("news:<ID/ID/ID??##??ID@HOST>#anchor?search");
  test ("news:</ID/ID/ID??##??ID@HOST>");
  test ("news:</ID/ID/ID??##??ID@HOST>?search");
  test ("news:</ID/ID/ID??##??ID@HOST>#anchor");
  test ("news:</ID/ID/ID??##??ID@HOST>?search#anchor");
  test ("news:</ID/ID/ID??##??ID@HOST>#anchor?search");

  test ("news://HOST/<ID??##??ID@HOST>");
  test ("news://HOST/<ID??##??ID@HOST>?search");
  test ("news://HOST/<ID??##??ID@HOST>#anchor");
  test ("news://HOST/<ID??##??ID@HOST>?search#anchor");
  test ("news://HOST/<ID??##??ID@HOST>#anchor?search");
  test ("news://HOST/<ID/ID/ID??##??ID@HOST>");
  test ("news://HOST/<ID/ID/ID??##??ID@HOST>?search");
  test ("news://HOST/<ID/ID/ID??##??ID@HOST>#anchor");
  test ("news://HOST/<ID/ID/ID??##??ID@HOST>?search#anchor");
  test ("news://HOST/<ID/ID/ID??##??ID@HOST>#anchor?search");

  test ("news:ID/ID/ID??##??ID@HOST");
  test ("news:ID/ID/ID??##??ID@HOST?search");
  test ("news:ID/ID/ID??##??ID@HOST#anchor");
  test ("news:ID/ID/ID??##??ID@HOST?search#anchor");
  test ("news:ID/ID/ID??##??ID@HOST#anchor?search");
  test ("news:/ID/ID/ID??##??ID@HOST");
  test ("news:/ID/ID/ID??##??ID@HOST?search");
  test ("news:/ID/ID/ID??##??ID@HOST#anchor");
  test ("news:/ID/ID/ID??##??ID@HOST?search#anchor");
  test ("news:/ID/ID/ID??##??ID@HOST#anchor?search");

  test ("news://HOST/ID??##??ID@HOST");
  test ("news://HOST/ID??##??ID@HOST?search");
  test ("news://HOST/ID??##??ID@HOST#anchor");
  test ("news://HOST/ID??##??ID@HOST?search#anchor");
  test ("news://HOST/ID??##??ID@HOST#anchor?search");
  test ("news://HOST/ID/ID/ID??##??ID@HOST");
  test ("news://HOST/ID/ID/ID??##??ID@HOST?search");
  test ("news://HOST/ID/ID/ID??##??ID@HOST#anchor");
  test ("news://HOST/ID/ID/ID??##??ID@HOST?search#anchor");
  test ("news://HOST/ID/ID/ID??##??ID@HOST#anchor?search");
}
#endif /* 0 */


/* Returns true if this URL is a reference to a news message,
   that is, the kind of news URL entity that can be displayed
   in a regular browser window, and doesn't involve all kinds
   of other magic state and callbacks like listing a group.
 */
XP_Bool
NET_IsNewsMessageURL (const char *url)
{
  char *host_and_port = 0;
  XP_Bool secure_p = FALSE;
  char *group = 0;
  char *message_id = 0;
  char *command_specific_data = 0;
  int status = NET_parse_news_url (url, &host_and_port, &secure_p,
								   &group, &message_id, &command_specific_data);
  XP_Bool result = FALSE;
  if (status >= 0 && message_id && *message_id)
	result = TRUE;
  FREEIF (host_and_port);
  FREEIF (group);
  FREEIF (message_id);
  FREEIF (command_specific_data);
  return result;
}

static XP_Bool nntp_are_connections_available (NNTPConnection *conn)
{
	int connCount = 0;

	NNTPConnection *tmpConn;
	XP_List *tmpList = nntp_connection_list;

	while ((tmpConn = (NNTPConnection*) XP_ListNextObject(tmpList)) != NULL)
	{
		NNTP_LOG_NOTE(("nntp_are_connections_available: comparing %08lX %s", tmpConn, tmpConn->busy ? "busy" : "available"));

		if (conn != tmpConn && tmpConn->busy && !PL_strcasecmp(tmpConn->hostname, conn->hostname))
			connCount++;
	}

	return connCount < kMaxConnectionsPerHost;
}

static XP_Bool isOnline = TRUE;
static XP_Bool prefInitialized = FALSE;

/* fix Mac warning of missing prototype */
MODULE_PRIVATE int PR_CALLBACK 
NET_OnlinePrefChangedFunc(const char *pref, void *data);

MODULE_PRIVATE int PR_CALLBACK NET_OnlinePrefChangedFunc(const char *pref, void *data) 
{
	int status;
	int32 port=0;
	char * socksHost = NULL;
	char text[MAXHOSTNAMELEN + 8];

	if (!PL_strcasecmp(pref,"network.online")) 
		status = PREF_GetBoolPref("network.online", &isOnline);

	if ( isOnline ) {
		CACHE_CloseAllOpenSARCache();

		/* If the user wants to use a socks server set it up. */
		if ( (NET_GetProxyStyle() == PROXY_STYLE_MANUAL) ) {
			PREF_CopyCharPref("network.hosts.socks_server",&socksHost);
			PREF_GetIntPref("network.hosts.socks_serverport",&port);
			if (socksHost && *socksHost && port) {
				PR_snprintf(text, sizeof(text), "%s:%d", socksHost, port);  
				NET_SetSocksHost(text);
			}
			else {
				NET_SetSocksHost(socksHost); /* NULL is ok */
			}
		}
	}
	else {
		CACHE_OpenAllSARCache();
	}

	return status;
}

/* fix Mac warning of missing prototype */
MODULE_PRIVATE int PR_CALLBACK
NET_NewsMaxArticlesChangedFunc(const char *pref, void *data);

MODULE_PRIVATE int PR_CALLBACK NET_NewsMaxArticlesChangedFunc(const char *pref, void *data) 
{
	int status;
	if (!PL_strcasecmp(pref,"news.max_articles")) 
	{
		int32 maxArticles;

		status = PREF_GetIntPref("news.max_articles", &maxArticles);
		NET_SetNumberOfNewsArticlesInListing(maxArticles);
	}
	return status;
}

MODULE_PRIVATE XP_Bool
NET_IsOffline()
{
	/*  Cache this value, and register a pref callback to
		find out when it changes. 
	*/
	if (!prefInitialized)
	{
		/*int status =*/ PREF_GetBoolPref("network.online", &isOnline);
		PREF_RegisterCallback("network.online",NET_OnlinePrefChangedFunc, NULL);
		/* because this routine gets called so often, we can register this callback here too. */
		PREF_RegisterCallback("news.max_articles", NET_NewsMaxArticlesChangedFunc, NULL);
		prefInitialized = TRUE;
	}
	return !isOnline;
}

PRIVATE int32
net_NewsLoad (ActiveEntry *ce)
{
  int status = 0;
  NewsConData *cd = PR_NEW(NewsConData);
  XP_Bool secure_p = FALSE;
  XP_Bool default_host = FALSE;
  char *url = ce->URL_s->address;
  char *host_and_port = 0;
  int32 port = 0;
  char *group = 0;
  char *message_id = 0;
  char *command_specific_data = 0;
  XP_Bool cancel_p = FALSE;
  char* colon;
  MSG_NewsHost* defhost = NULL;
  static XP_Bool collabra_enabled = TRUE;
  static XP_Bool got_enabled_pref = FALSE;

  if (!got_enabled_pref)
  {
	  PREF_GetBoolPref("news.enabled",&collabra_enabled);
	  got_enabled_pref = TRUE;
  }

  /* fail on protected url types */
  if(ce->protocol == INTERNAL_NEWS_TYPE_URL
	 && !ce->URL_s->internal_url)
  {
	status = MK_MALFORMED_URL_ERROR;
	goto FAIL;
  }

  if (!collabra_enabled)
  {
	status = MK_MSG_COLLABRA_DISABLED;
	goto FAIL;
  }


#ifndef NSPR20
  PR_LogInit (&NNTPLog);
#endif

  if(!cd)
	{
	  status = MK_OUT_OF_MEMORY;
	  goto FAIL;
	}

  memset(cd, 0, sizeof(NewsConData));
  ce->URL_s->content_length = 0;

  if(!ce->proxy_addr)
  {
	/* look for an HTTPS proxy and use it if available,
	 * but the passed in one takes precedence
	 */
    StrAllocCopy(ce->proxy_addr,
                 NET_FindProxyHostForUrl(SECURE_HTTP_TYPE_URL, 
                                         ce->URL_s->address));
  }
  else
  {
  	StrAllocCopy(cd->ssl_proxy_server, ce->proxy_addr);
  }

  cd->pane = ce->URL_s->msg_pane;
  if (!cd->pane)
  {
    NNTP_LOG_NOTE(("NET_NewsLoad: url->msg_pane NULL for URL: %s\n", ce->URL_s->address));
    cd->pane = MSG_FindPane(ce->window_id, MSG_ANYPANE);
  }
  PR_ASSERT(cd->pane && MSG_GetContext(cd->pane) == ce->window_id);
  if (!cd->pane) {
	status = -1;				/* ### */
	goto FAIL;
  }



  NNTP_LOG_NOTE (("NET_NewsLoad: %s", ce->URL_s->address));

  cd->output_buffer = (char *) PR_Malloc(OUTPUT_BUFFER_SIZE);
  if(!cd->output_buffer)
	{
	  status = MK_OUT_OF_MEMORY;
	  goto FAIL;
	}

  ce->con_data = cd;
  ce->socket = NULL;
  cd->article_num = -1;

  PR_ASSERT (url);
  if (!url)
	{
	  status = -1;
	  goto FAIL;
	}


  status = NET_parse_news_url (url, &host_and_port, &secure_p,
							   &group, &message_id, &command_specific_data);
  if (status < 0)
	goto FAIL;

  colon = PL_strchr (host_and_port, ':');
  if (colon) {
    unsigned long naturalLong;
	*colon = '\0';
    sscanf (colon+1, " %lu ", &naturalLong); /* %l is 64 bits on OSF1 */
	port = (int32) naturalLong;
  }
  cd->host = MSG_CreateNewsHost(MSG_GetMaster(cd->pane), host_and_port,
								secure_p, port);
  if (colon) *colon = ':';

  PR_ASSERT(cd->host);
  if (!cd->host) {
	status = -1;
	goto FAIL;
  }

  if (message_id && command_specific_data && !PL_strcmp (command_specific_data, "?cancel"))
	cancel_p = TRUE;

  StrAllocCopy(cd->path, message_id);
  StrAllocCopy(cd->group_name, group);

  /* make sure the user has a news host configured */
  
  
  defhost = MSG_GetDefaultNewsHost(MSG_GetMaster(cd->pane));

  if (defhost == NULL)
    {
	  char* alert = NET_ExplainErrorDetails(MK_NNTP_SERVER_NOT_CONFIGURED);
	  FE_Alert(ce->window_id, alert);
#ifdef XP_MAC
	  /* AFTER the alert, not before! */
	  FE_EditPreference(PREF_NewsHost);
#endif
      status = -1;
      goto FAIL;
    }

  default_host = (cd->host == defhost);

  if (cancel_p && !ce->URL_s->internal_url)
	{
	  /* Don't allow manual "news:ID?cancel" URLs, only internal ones. */
	  status = -1;
	  goto FAIL;
	}
  else if (ce->URL_s->method == URL_POST_METHOD)
	{
	  /* news://HOST done with a POST instead of a GET;
		 this means a new message is being posted.
		 Don't allow this unless it's an internal URL.
	   */
	  if (!ce->URL_s->internal_url)
		{
		  status = -1;
		  goto FAIL;
		}
	  PR_ASSERT (!group && !message_id && !command_specific_data);
	  cd->type_wanted = NEWS_POST;
	  StrAllocCopy(cd->path, "");
	}
  else if (message_id)
	{
	  /* news:MESSAGE_ID
		 news:/GROUP/MESSAGE_ID (useless)
		 news://HOST/MESSAGE_ID
		 news://HOST/GROUP/MESSAGE_ID (useless)
	   */
	  if (cancel_p)
		cd->type_wanted = CANCEL_WANTED;
	  else
		cd->type_wanted = ARTICLE_WANTED;
	}
  else if (command_specific_data)
	{
	  if (PL_strstr (command_specific_data, "?newgroups"))
	    /* news://HOST/?newsgroups
	        news:/GROUP/?newsgroups (useless)
	        news:?newsgroups (default host)
	     */
	    cd->type_wanted = NEW_GROUPS;
      else
	  {
		if (PL_strstr(command_specific_data, "?list-pretty"))
		{
			cd->type_wanted = PRETTY_NAMES_WANTED;
			cd->command_specific_data = PL_strdup(command_specific_data);
		}
		else if (PL_strstr(command_specific_data, "?profile"))
		{
			cd->type_wanted = PROFILE_WANTED;
			cd->command_specific_data = PL_strdup(command_specific_data);
		}
		else if (PL_strstr(command_specific_data, "?list-ids"))
		{
			cd->type_wanted = IDS_WANTED;
			cd->command_specific_data = PL_strdup(command_specific_data);
		}
		else
		{
			cd->type_wanted = SEARCH_WANTED;
			cd->command_specific_data = PL_strdup(command_specific_data);
			cd->current_search = cd->command_specific_data;
		}
	  }
	}
  else if (group)
	{
	  /* news:GROUP
		 news:/GROUP
		 news://HOST/GROUP
	   */
	  if (ce->window_id->type != MWContextNews && ce->window_id->type != MWContextNewsMsg 
		  && ce->window_id->type != MWContextMail && ce->window_id->type != MWContextMailNewsProgress)
		{
		  status = -1;
		  goto FAIL;
		}
	  if (PL_strchr (group, '*'))
		cd->type_wanted = LIST_WANTED;
	  else
		cd->type_wanted = GROUP_WANTED;
	}
  else
	{
	  /* news:
	     news://HOST
	   */
	  cd->type_wanted = READ_NEWS_RC;
	}


  /* At this point, we're all done parsing the URL, and know exactly
	 what we want to do with it.
   */

#ifndef NO_ARTICLE_CACHEING
  /* Turn off caching on all news entities, except articles. */
  /* It's very important that this be turned off for CANCEL_WANTED;
	 for the others, I don't know what cacheing would cause, but
	 it could only do harm, not good. */
  if(cd->type_wanted != ARTICLE_WANTED)
#endif
  	ce->format_out = CLEAR_CACHE_BIT (ce->format_out);

  if (cd->type_wanted == ARTICLE_WANTED)
  {
		const char *group = 0;
		uint32 number = 0;

		if (MSG_IsOfflineArticle (cd->pane, cd->path,
									&group, &number))
		{
			ce->local_file = TRUE;
			ce->socket = NULL;
			NET_SetCallNetlibAllTheTime(ce->window_id, "mknews");
			MSG_StartOfflineRetrieval(cd->pane, group, number, &cd->offlineState);
		}
  }
  if (NET_IsOffline() && !ce->local_file)
  {
	  status = MK_OFFLINE;	
	  goto FAIL;
  }
  if (cd->offlineState)
	  goto FAIL;	/* we don't need to do any of this connection stuff */

  /* check for established connection and use it if available
   */
  if(nntp_connection_list)
	{
	  NNTPConnection * tmp_con;
	  XP_List * list_entry = nntp_connection_list;

	  while((tmp_con = (NNTPConnection *)XP_ListNextObject(list_entry))
			!= NULL)
		{
		  /* if the hostnames match up exactly and the connection
		   * is not busy at the moment then reuse this connection.
		   */
		  if(!PL_strcmp(tmp_con->hostname, host_and_port)
			 && secure_p == tmp_con->secure
			 &&!tmp_con->busy)
			{
			  cd->control_con = tmp_con;

			  NNTP_LOG_NOTE(("Reusing control_con %08lX for %s", tmp_con, url));

			  /* set select on the control socket */
			  ce->socket = cd->control_con->csock;
			  NET_SetReadSelect(ce->window_id, cd->control_con->csock);
			  cd->control_con->prev_cache = TRUE;  /* this was from the cache */
			  break;
			}
		}
	}
  else
	{
	  /* initialize the connection list
	   */
	  nntp_connection_list = XP_ListNew();
	}


  if(cd->control_con)
	{
	  cd->next_state = SEND_FIRST_NNTP_COMMAND;
	  /* set the connection busy
	   */
	  cd->control_con->busy = TRUE;
	  NET_TotalNumberOfOpenConnections++;
	}
  else
	{
	  /* build a control connection structure so we
	   * can store the data as we go along
	   */
	  cd->control_con = PR_NEW(NNTPConnection);
	  if(!cd->control_con)
		{
		  status = MK_OUT_OF_MEMORY;
		  goto FAIL;
		}
	  NNTP_LOG_NOTE(("Created control_con %08lX for %s", cd->control_con, ce->URL_s->address));
	  memset(cd->control_con, 0, sizeof(NNTPConnection));

	  cd->control_con->default_host = default_host;
	  StrAllocCopy(cd->control_con->hostname, host_and_port);
	  NNTP_LOG_NOTE(("Set host string: %s", cd->control_con->hostname));

	  cd->control_con->secure = secure_p;

	  cd->control_con->prev_cache = FALSE;  /* this wasn't from the cache */

	  cd->control_con->csock = NULL;

	  /* define this to test support for older NNTP servers
	   * that don't support the XOVER command
	   */
#ifdef TEST_NO_XOVER_SUPPORT
	  cd->control_con->no_xover = TRUE;
#endif /* TEST_NO_XOVER_SUPPORT */

	  /* add this structure to the connection list even
	   * though it's not really valid yet.
	   * we will fill it in as we go and if
	   * an error occurs will will remove it from the
	   * list.  No one else will be able to use it since
	   * we will mark it busy.
	   */
	  XP_ListAddObject(nntp_connection_list, cd->control_con);

	  /* set the connection busy
	   */
	  cd->control_con->busy = TRUE;

	  /* gate the maximum number of cached connections
	   * but don't delete busy ones.
	   */
	  if(XP_ListCount(nntp_connection_list) > kMaxCachedConnections)
		{
		  XP_List * list_ptr = nntp_connection_list->next;
		  NNTPConnection * con;

		  while(list_ptr)
			{
			  con = (NNTPConnection *) list_ptr->object;
			  list_ptr = list_ptr->next;
			  if(!con->busy)
				{
				  XP_ListRemoveObject(nntp_connection_list, con);
				  net_nntp_close (con->csock, status);
				  FREEIF(con->current_group);
				  FREEIF(con->hostname);
				  PR_Free(con);
				  break;
				}
			}
		}

	  cd->next_state = NNTP_CONNECT;
	}

 FAIL:

  FREEIF (host_and_port);
  FREEIF (group);
  FREEIF (message_id);
  FREEIF (command_specific_data);

  if (status < 0)
  {
	  FREEIF (cd->output_buffer);
	  FREEIF (cd);
	  ce->URL_s->error_msg = NET_ExplainErrorDetails(status);
	  return status;
  }
  else 
  {
	  return (net_ProcessNews (ce));
  }
}
/* process the offline news state machine, such as it is. */

PRIVATE int
NET_ProcessOfflineNews(ActiveEntry *ce, NewsConData *cd)
{
	int32 read_size = 0;
	int status;

    cd->pause_for_read  = TRUE;

	if (cd->stream)
		read_size = (*cd->stream->is_write_ready)
								(cd->stream);
	else
		ce->status = net_begin_article(ce);

	if(!read_size)
		return(0);  /* wait until we are ready to write */
	else
		read_size = MIN(read_size, NET_Socket_Buffer_Size);

	status = MSG_ProcessOfflineNews(cd->offlineState, NET_Socket_Buffer, read_size);
	if(status > 0)
	{
		ce->bytes_received += status;
		PUT_STREAM(NET_Socket_Buffer, status);	/* blow off error? ### dmb */
	}

	if (status == 0)
	{
 	  if (cd->stream)
		COMPLETE_STREAM;
       	NET_ClearCallNetlibAllTheTime(ce->window_id, "mknews");

		if(cd->destroy_graph_progress)
		  FE_GraphProgressDestroy(ce->window_id, 
								  ce->URL_s, 
								  cd->original_content_length,
								  ce->bytes_received);
		status = -1;

	}
	return status;
}

/* process the state machine
 *
 * returns negative when completely done
 */
PRIVATE int32
net_ProcessNews (ActiveEntry *ce)
{
    NewsConData * cd = (NewsConData *)ce->con_data;

	if (cd->offlineState != NULL)
	{
		return NET_ProcessOfflineNews(ce, cd);
	}
    cd->pause_for_read = FALSE;

    while(!cd->pause_for_read)
      {

#if DEBUG
        NNTP_LOG_NOTE(("Next state: %s",stateLabels[cd->next_state])); 
#endif

        switch(cd->next_state)
          {
            case NNTP_RESPONSE:
                ce->status = net_news_response(ce);
                break;

#ifdef BLOCK_UNTIL_AVAILABLE_CONNECTION
			case NNTP_BLOCK_UNTIL_CONNECTIONS_ARE_AVAILABLE:
				if (nntp_are_connections_available(cd->control_con))
				{
					/* haven't reached connection ceiling on this host; go to connect */
					cd->next_state = NNTP_CONNECTIONS_ARE_AVAILABLE;
					cd->pause_for_read = FALSE;
				}
				else
				{
					NET_SetReadSelect(ce->window_id, ce->socket);
					cd->pause_for_read = TRUE;
				}
				break;
			case NNTP_CONNECTIONS_ARE_AVAILABLE:
				/* release our hacky little lock and move on to connection */
				NET_ClearCallNetlibAllTheTime(ce->window_id, "mknews");
				
				cd->next_state = NNTP_CONNECT;
				cd->pause_for_read = FALSE;
				break;
#endif

            case NNTP_CONNECT:
				if (nntp_are_connections_available(cd->control_con))
				{
					ce->status = NET_BeginConnect(
						(cd->ssl_proxy_server && cd->control_con->secure ?
						cd->ssl_proxy_server :cd->control_con->hostname), 
						NULL,
						"NNTP", 
						(cd->control_con->secure ? SECURE_NEWS_PORT : NEWS_PORT), 
						&ce->socket, 
						(cd->ssl_proxy_server ? FALSE : cd->control_con->secure), 
						&cd->tcp_con_data, 
						ce->window_id,
						&ce->URL_s->error_msg,
						ce->socks_host,
						ce->socks_port);

					if(ce->socket != NULL)
						NET_TotalNumberOfOpenConnections++;

					cd->pause_for_read = TRUE;
					if(ce->status == MK_CONNECTED)
					{
HG33086
						{
							cd->next_state = NNTP_RESPONSE;
							cd->next_state_after_response = NNTP_LOGIN_RESPONSE;
						}
						NET_SetReadSelect(ce->window_id, ce->socket);
						cd->control_con->csock = ce->socket;
					}
					else if(ce->status > -1)
					{
						ce->con_sock = ce->socket;  /* set con_sock so we can select on it */
						cd->control_con->csock = ce->socket;
						NET_SetConnectSelect(ce->window_id, ce->con_sock);
						cd->next_state = NNTP_CONNECT_WAIT;
					}
				}
				else
				{
#ifdef BLOCK_UNTIL_AVAILABLE_CONNECTION
					/* ###phil this doesn't work yet, but the idea is that we've
					 * maxed out connections on this host; stay in this state 
					 */
					ce->con_sock = ce->socket;  /* set con_sock so we can select on it */
					cd->control_con->csock = ce->socket;
					NET_SetConnectSelect(ce->window_id, ce->con_sock);
					cd->next_state = NNTP_BLOCK_UNTIL_CONNECTIONS_ARE_AVAILABLE;
#else
					ce->status = -1;
					cd->next_state = NNTP_ERROR;
					ce->socket = cd->control_con->csock = NULL;
#endif
				}
				break;

            case NNTP_CONNECT_WAIT:
                ce->status = NET_FinishConnect(
							  (cd->ssl_proxy_server && cd->control_con->secure ?
								cd->ssl_proxy_server : cd->control_con->hostname),
							  "NNTP", 
							  (cd->control_con->secure ? SECURE_NEWS_PORT : NEWS_PORT), 
							  &ce->socket, 
							  &cd->tcp_con_data, 
							  ce->window_id,
							  &ce->URL_s->error_msg);
  
                cd->pause_for_read = TRUE;
                if(ce->status == MK_CONNECTED)
                  {
				    cd->control_con->csock = ce->socket;
HG17928
                      {
                        cd->next_state = NNTP_RESPONSE;
                        cd->next_state_after_response = NNTP_LOGIN_RESPONSE;
                      }

					NET_ClearConnectSelect(ce->window_id, ce->con_sock);
                    ce->con_sock = NULL;  /* reset con_sock so we don't select on it */
					NET_SetReadSelect(ce->window_id, ce->socket);
                  }
				else if(ce->status < 0)
                  {
                    	NET_ClearConnectSelect(ce->window_id, ce->con_sock);
                  }
				else
                  {
					/* the not yet connected state */

        			/* unregister the old ce->socket from the select list
         			 * and register the new value in the case that it changes
         			 */
        			if(ce->con_sock != ce->socket)
          			  {
            			NET_ClearConnectSelect(ce->window_id, ce->con_sock);
            			ce->con_sock = ce->socket;
            			NET_SetConnectSelect(ce->window_id, ce->con_sock);
          			  }
                  }
                break;
HG68092
            case NNTP_LOGIN_RESPONSE:
                ce->status = net_nntp_login_response(ce);
                break;

			case NNTP_SEND_MODE_READER:
                ce->status = net_nntp_send_mode_reader(ce);
                break;

			case NNTP_SEND_MODE_READER_RESPONSE:
                ce->status = net_nntp_send_mode_reader_response(ce);
                break;

			case SEND_LIST_EXTENSIONS:
				ce->status = net_nntp_send_list_extensions(ce);
				break;
			case SEND_LIST_EXTENSIONS_RESPONSE:
				ce->status = net_nntp_send_list_extensions_response(ce);
				break;
			case SEND_LIST_SEARCHES:
				ce->status = net_nntp_send_list_searches(ce);
				break;
			case SEND_LIST_SEARCHES_RESPONSE:
				ce->status = net_nntp_send_list_searches_response(ce);
				break;
			case NNTP_LIST_SEARCH_HEADERS:
				ce->status = net_send_list_search_headers(ce);
				break;
			case NNTP_LIST_SEARCH_HEADERS_RESPONSE:
				ce->status = net_send_list_search_headers_response(ce);
				break;
			case NNTP_GET_PROPERTIES:
				ce->status = nntp_get_properties(ce);
				break;
			case NNTP_GET_PROPERTIES_RESPONSE:
				ce->status = nntp_get_properties_response(ce);
				break;
				
			case SEND_LIST_SUBSCRIPTIONS:
				ce->status = net_send_list_subscriptions(ce);
				break;
			case SEND_LIST_SUBSCRIPTIONS_RESPONSE:
				ce->status = net_send_list_subscriptions_response(ce);
				break;

            case SEND_FIRST_NNTP_COMMAND:
                ce->status = net_send_first_nntp_command(ce);
                break;

            case SEND_FIRST_NNTP_COMMAND_RESPONSE:
                ce->status = net_send_first_nntp_command_response(ce);
                break;

            case NNTP_SEND_GROUP_FOR_ARTICLE:
                ce->status = net_send_group_for_article(ce);
                break;
            case NNTP_SEND_GROUP_FOR_ARTICLE_RESPONSE:
                ce->status = net_send_group_for_article_response(ce);
                break;
            case NNTP_SEND_ARTICLE_NUMBER:
                ce->status = net_send_article_number(ce);
                break;

            case SETUP_NEWS_STREAM:
                ce->status = net_setup_news_stream(ce);
                break;

			case NNTP_BEGIN_AUTHORIZE:
				ce->status = net_news_begin_authorize(ce);
                break;

			case NNTP_AUTHORIZE_RESPONSE:
				ce->status = net_news_authorize_response(ce);
                break;

			case NNTP_PASSWORD_RESPONSE:
				ce->status = net_news_password_response(ce);
                break;
    
            case NNTP_READ_LIST_BEGIN:
                ce->status = net_read_news_list_begin(ce);
                break;

            case NNTP_READ_LIST:
                ce->status = net_read_news_list(ce);
                break;

			case DISPLAY_NEWSGROUPS:
				ce->status = net_display_newsgroups(ce);
                break;

			case NNTP_NEWGROUPS_BEGIN:
				ce->status = net_newgroups_begin(ce);
                break;
    
			case NNTP_NEWGROUPS:
				ce->status = net_process_newgroups(ce);
                break;
    
            case NNTP_BEGIN_ARTICLE:
				ce->status = net_begin_article(ce);
                break;

            case NNTP_READ_ARTICLE:
			  {
				char *line;
				NewsConData * cd =
				  (NewsConData *)ce->con_data;

				ce->status = NET_BufferedReadLine(ce->socket, &line,
												 &cd->data_buf, 
												 &cd->data_buf_size,
												 (Bool*)&cd->pause_for_read);

				if(ce->status == 0)
				  {
					cd->next_state = NNTP_ERROR;
					cd->pause_for_read = FALSE;
					ce->URL_s->error_msg =
					  NET_ExplainErrorDetails(MK_NNTP_SERVER_ERROR);
					return(MK_NNTP_SERVER_ERROR);
				  }

				if(ce->status > 1)
				  {
					ce->bytes_received += ce->status;
					/* We shouldn't graph progress if going offline - it messes
					up our go offline status msgs. ### dmb
					*/
					FE_GraphProgress(ce->window_id, ce->URL_s,
									 ce->bytes_received, ce->status,
									 ce->URL_s->content_length);

				  }

				if(!line)
				  return(ce->status);  /* no line yet or error */

				if (cd->type_wanted == CANCEL_WANTED &&
					cd->response_code != 221)
				  {
					/* HEAD command failed. */
					return MK_NNTP_CANCEL_ERROR;
				  }

				if (line[0] == '.' && line[1] == 0)
				  {
					if (cd->type_wanted == CANCEL_WANTED)
					  cd->next_state = NEWS_START_CANCEL;
					else
					  cd->next_state = NEWS_DONE;

					cd->pause_for_read = FALSE;
				  }
				else
				  {
					if (line[0] == '.')
					  PL_strcpy (cd->output_buffer, line + 1);
					else
					  PL_strcpy (cd->output_buffer, line);

					/* When we're sending this line to a converter (ie,
					   it's a message/rfc822) use the local line termination
					   convention, not CRLF.  This makes text articles get
					   saved with the local line terminators.  Since SMTP
					   and NNTP mandate the use of CRLF, it is expected that
					   the local system will convert that to the local line
					   terminator as it is read.
					 */
					PL_strcat (cd->output_buffer, LINEBREAK);
					/* Don't send content-type to mime parser if we're doing a cancel
					  because it confuses mime parser into not parsing.
					  */
					if (cd->type_wanted != CANCEL_WANTED || PL_strncmp(cd->output_buffer, "Content-Type:", 13))
						ce->status = PUTSTRING(cd->output_buffer);
				  }
				break;
			  }

            case NNTP_XOVER_BEGIN:
			    NET_Progress(ce->window_id, XP_GetString(XP_PROGRESS_READ_NEWSGROUPINFO));
			    ce->status = net_read_xover_begin(ce);
                break;

		  case NNTP_FIGURE_NEXT_CHUNK:
				ce->status = net_figure_next_chunk(ce);
				break;

			case NNTP_XOVER_SEND:
				ce->status = net_xover_send(ce);
				break;
    
            case NNTP_XOVER:
                ce->status = net_read_xover(ce); 
                break;

            case NNTP_XOVER_RESPONSE:
                ce->status = net_read_xover_response(ce); 
                break;

            case NEWS_PROCESS_XOVER:
		    case NEWS_PROCESS_BODIES:
			    if (cd->group_name && PL_strstr (cd->group_name, ".emacs"))
				  /* This is a joke!  Don't internationalize it... */
			      NET_Progress(ce->window_id, "Garbage collecting...");
			    else
			      NET_Progress(ce->window_id, XP_GetString(XP_PROGRESS_SORT_ARTICLES));
				  ce->status = net_process_xover(ce);
                break;

			case NNTP_PROFILE_ADD:
				ce->status = net_profile_add (ce);
				break;
			case NNTP_PROFILE_ADD_RESPONSE:
				ce->status = net_profile_add_response(ce);
				break;
			case NNTP_PROFILE_DELETE:
				ce->status = net_profile_delete (ce);
				break;
			case NNTP_PROFILE_DELETE_RESPONSE:
				ce->status = net_profile_delete_response(ce);
				break;

            case NNTP_READ_GROUP:
                ce->status = net_read_news_group(ce);
                break;
    
            case NNTP_READ_GROUP_RESPONSE:
                ce->status = net_read_news_group_response(ce);
                break;

            case NNTP_READ_GROUP_BODY:
                ce->status = net_read_news_group_body(ce);
                break;

	        case NNTP_SEND_POST_DATA:
	            ce->status = net_send_news_post_data(ce);
	            break;

	        case NNTP_SEND_POST_DATA_RESPONSE:
	            ce->status = net_send_news_post_data_response(ce);
	            break;

			case NNTP_CHECK_FOR_MESSAGE:
				ce->status = net_check_for_message(ce);
				break;

			case NEWS_NEWS_RC_POST:
		        ce->status = net_NewsRCProcessPost(ce);
		        break;

            case NEWS_DISPLAY_NEWS_RC:
		        ce->status = net_DisplayNewsRC(ce);
		        break;

            case NEWS_DISPLAY_NEWS_RC_RESPONSE:
		        ce->status = net_DisplayNewsRCResponse(ce);
		        break;

            case NEWS_START_CANCEL:
		        ce->status = net_start_cancel(ce);
		        break;

            case NEWS_DO_CANCEL:
		        ce->status = net_do_cancel(ce);
		        break;

			case NNTP_XPAT_SEND:
				ce->status = net_xpat_send(ce);
				break;
			case NNTP_XPAT_RESPONSE:
				ce->status = net_xpat_response(ce);
				break;
			case NNTP_SEARCH:
				ce->status = net_nntp_search(ce);
				break;
			case NNTP_SEARCH_RESPONSE:
				ce->status = net_nntp_search_response(ce);
				break;
			case NNTP_SEARCH_RESULTS:
				ce->status = net_nntp_search_results(ce);
				break;
			case NNTP_LIST_PRETTY_NAMES:
				ce->status = net_list_pretty_names(ce);
				break;
			case NNTP_LIST_PRETTY_NAMES_RESPONSE:
				ce->status = net_list_pretty_names_response(ce);
				break;
			case NNTP_LIST_XACTIVE:
				ce->status = net_list_xactive(ce);
				break;
			case NNTP_LIST_XACTIVE_RESPONSE:
				ce->status = net_list_xactive_response(ce);
				break;
			case NNTP_LIST_GROUP:
				ce->status = net_list_group(ce);
				break;
			case NNTP_LIST_GROUP_RESPONSE:
				ce->status = net_list_group_response(ce);
				break;
	        case NEWS_DONE:
			  /* call into libmsg and see if the article counts
			   * are up to date.  If they are not then we
			   * want to do a "news://host/group" URL so that we
			   * can finish up the article counts.
			   */
			  if (cd->stream)
				COMPLETE_STREAM;

	            cd->next_state = NEWS_FREE;
                /* set the connection unbusy
     	         */
    		    cd->control_con->busy = FALSE;
                NET_TotalNumberOfOpenConnections--;

				NET_ClearReadSelect(ce->window_id, cd->control_con->csock);
				NET_RefreshCacheFileExpiration(ce->URL_s);
	            break;

	        case NEWS_ERROR:
	            if(cd->stream)
		             ABORT_STREAM(ce->status);
	            cd->next_state = NEWS_FREE;
    	        /* set the connection unbusy
     	         */
    		    cd->control_con->busy = FALSE;
                NET_TotalNumberOfOpenConnections--;

				if(cd->control_con->csock != NULL)
				  {
					NET_ClearReadSelect(ce->window_id, cd->control_con->csock);
				  }
	            break;

	        case NNTP_ERROR:
	            if(cd->stream)
				  {
		            ABORT_STREAM(ce->status);
					cd->stream=0;
				  }
    
				if(cd->control_con && cd->control_con->csock != NULL)
				  {
					NNTP_LOG_NOTE(("Clearing read and connect select on socket %d",
															cd->control_con->csock));
					NET_ClearConnectSelect(ce->window_id, cd->control_con->csock);
					NET_ClearReadSelect(ce->window_id, cd->control_con->csock);
#ifdef XP_WIN
					if(cd->calling_netlib_all_the_time)
					{
						cd->calling_netlib_all_the_time = FALSE;
						NET_ClearCallNetlibAllTheTime(ce->window_id, "mknews");
					}
#endif /* XP_WIN */
                    NET_ClearDNSSelect(ce->window_id, cd->control_con->csock);
				    net_nntp_close (cd->control_con->csock, ce->status);  /* close the socket */
					FREEIF(cd->control_con->current_group);
					NET_TotalNumberOfOpenConnections--;
					ce->socket = NULL;
				  }

                /* check if this connection came from the cache or if it was
                 * a new connection.  If it was not new lets start it over
				 * again.  But only if we didn't have any successful protocol
				 * dialog at all.
                 */
                if(cd->control_con && cd->control_con->prev_cache &&
				   !cd->some_protocol_succeeded)
                  {
                     cd->control_con->prev_cache = FALSE;
                     cd->next_state = NNTP_CONNECT;
                     ce->status = 0;  /* keep going */
                  }
                else
                  {
                    cd->next_state = NEWS_FREE;
            
                    /* remove the connection from the cache list
                     * and free the data
                     */
                    XP_ListRemoveObject(nntp_connection_list, cd->control_con);
                    if(cd->control_con)
                      {
                        FREEIF(cd->control_con->hostname);
                        FREE(cd->control_con);
                      }
                  }
                break;
    
            case NEWS_FREE:

				  /* do we need to know if we're parsing xover to call finish xover? */
					/* If we've gotten to NEWS_FREE and there is still XOVER
					   data, there was an error or we were interrupted or
					   something.  So, tell libmsg there was an abnormal
					   exit so that it can free its data. */
/*				if (cd->xover_parse_state != NULL)*/
				{
					int status;
/*					PR_ASSERT (ce->status < 0);*/
					status = MSG_FinishXOVER (cd->pane,
											  &cd->xover_parse_state,
											  ce->status);
					PR_ASSERT (!cd->xover_parse_state);
					if (ce->status >= 0 && status < 0)
					  ce->status = status;
				}
				if (cd->newsgroup_parse_state)
					MSG_FinishAddArticleKeyToGroup(cd->pane, &cd->newsgroup_parse_state);

                FREEIF(cd->path);
                FREEIF(cd->response_txt);
                FREEIF(cd->data_buf);

				FREEIF(cd->output_buffer);
				FREEIF(cd->ssl_proxy_server);
                FREEIF(cd->stream);  /* don't forget the stream */
            	if(cd->tcp_con_data)
                	NET_FreeTCPConData(cd->tcp_con_data);

                FREEIF(cd->group_name);

				FREEIF (cd->cancel_id);
				FREEIF (cd->cancel_from);
				FREEIF (cd->cancel_newsgroups);
				FREEIF (cd->cancel_distribution);

                if(cd->destroy_graph_progress)
                    FE_GraphProgressDestroy(ce->window_id, 
                                            ce->URL_s, 
                                            cd->original_content_length,
											ce->bytes_received);
                PR_Free(cd);

        		/* gate the maximum number of cached connections
				 * but don't delete busy ones.
                 */
                if(XP_ListCount(nntp_connection_list) > kMaxCachedConnections)
                  {
                    XP_List * list_ptr = nntp_connection_list->next;
                    NNTPConnection * con;

					NNTP_LOG_NOTE(("killing cached news connection"));

                    while(list_ptr)
                      {
                        con = (NNTPConnection *) list_ptr->object;
                        list_ptr = list_ptr->next;
                        if(!con->busy)
                          {
                            XP_ListRemoveObject(nntp_connection_list, con);
                            net_nntp_close(con->csock, ce->status);
							FREEIF(con->current_group);
                            FREEIF(con->hostname);
                            PR_Free(con);
                            break;
                          }
                      }
				  }
                return(-1); /* all done */

            default:
                /* big error */
                return(-1);
          }

        if(ce->status < 0 && cd->next_state != NEWS_ERROR &&
                    cd->next_state != NNTP_ERROR && cd->next_state != NEWS_FREE)
          {
            cd->next_state = NNTP_ERROR;
            cd->pause_for_read = FALSE;
          }
      } /* end big while */

      return(0); /* keep going */
}

/* abort the connection in progress
 */
PRIVATE int32
net_InterruptNews(ActiveEntry * ce)
{
    NewsConData * cd = (NewsConData *)ce->con_data;

	if (cd->offlineState != NULL)
		return MSG_InterruptOfflineNews(cd->offlineState);

    cd->next_state = NNTP_ERROR;
    ce->status = MK_INTERRUPTED;

	if (cd->control_con)
		cd->control_con->prev_cache = FALSE;   /* to keep if from reconnecting */
 
    return(net_ProcessNews(ce));
}

/* Free any memory used up
 * close cached connections and
 * free newsgroup listings
 */
PRIVATE void
net_CleanupNews(void)
{
	NNTP_LOG_NOTE(("NET_CleanupNews called"));

	/* this is a small leak but since I don't have another function to
	 * clear my connections I need to call this one alot and I don't
	 * want to free the newshost
	 *
     * FREE_AND_CLEAR(NET_NewsHost);
	 */

    if(nntp_connection_list)
      {
        NNTPConnection * tmp_con;

        while((tmp_con = (NNTPConnection *)XP_ListRemoveTopObject(nntp_connection_list)) != NULL)
          {
			if(!tmp_con->busy)
			  {
			    FREE(tmp_con->hostname);
			    FREEIF(tmp_con->current_group);
			    if(tmp_con->csock != NULL)
			      {
			        net_nntp_close(tmp_con->csock, 0 /* status? */);
			      }
			    FREE(tmp_con);
			  }
          }

		if(XP_ListIsEmpty(nntp_connection_list))
		  {
			XP_ListDestroy(nntp_connection_list);
			nntp_connection_list = 0;
		  }
      }

    return;
}

MODULE_PRIVATE void
NET_InitNewsProtocol(void)
{
    static NET_ProtoImpl news_proto_impl;

    news_proto_impl.init = net_NewsLoad;
    news_proto_impl.process = net_ProcessNews;
    news_proto_impl.interrupt = net_InterruptNews;
    news_proto_impl.cleanup = net_CleanupNews;

    NET_RegisterProtocolImplementation(&news_proto_impl, NEWS_TYPE_URL);
    NET_RegisterProtocolImplementation(&news_proto_impl, INTERNAL_NEWS_TYPE_URL);
}

#ifdef PROFILE
#pragma profile off
#endif

#endif /* MOZILLA_CLIENT */

