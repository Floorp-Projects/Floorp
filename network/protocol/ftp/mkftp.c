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
/* state machine to read FTP sites
 *
 * Designed and originally implemented by Lou Montulli '94
 *
 * State machine to implement File Transfer Protocol,
 * all the protocol state machines work basically the same way.
 */

#include "rosetta.h"
#include "xp.h"
#include "netutils.h"
#include "mkselect.h"
#include "mktcp.h"
#include "mkgeturl.h"
#include "net_xp_file.h"
#include "prerror.h"
#include "prmem.h"
#include "plstr.h"

#define FTP_PORT 21

#define FTP_MODE_UNKNOWN 0
#define FTP_MODE_ASCII   1
#define FTP_MODE_BINARY  2

#include "ftpurl.h"  
#include "glhist.h"
#include "mkgeturl.h"
#include "mkparse.h"
#include "mktcp.h"
#include "mksort.h"
#include "mkfsort.h"   /* file sorting */
#include "mkformat.h"
#include "mkfsort.h"  
#include "merrors.h"
#include "shist.h"
#include HG27229
#include "prtime.h"
#include "pwcacapi.h"
#include "secnav.h"

#if defined(SMOOTH_PROGRESS)
#include "progress.h"
#endif

#ifdef XP_OS2 /* DSR072196 */
#include "os2sock.h"
#endif /* XP_OS2 */

#ifdef XP_UNIX
#if defined(HAVE_SYS_FILIO_H)
#include <sys/filio.h>
#endif
#endif /* XP_UNIX */ 

#include "prefapi.h"

/* for XP_GetString() */
#include "xpgetstr.h"
extern int XP_HTML_FTPERROR_NOLOGIN;
extern int XP_HTML_FTPERROR_TRANSFER;
extern int XP_PROGRESS_RECEIVE_FTPDIR;
extern int XP_PROGRESS_RECEIVE_FTPFILE;
extern int XP_PROGRESS_READFILE;
extern int XP_PROGRESS_FILEDONE;
extern int XP_PROMPT_ENTER_PASSWORD;
extern int MK_COULD_NOT_PUT_FILE;
extern int MK_TCP_READ_ERROR;
extern int MK_TCP_WRITE_ERROR;
extern int MK_UNABLE_TO_ACCEPT_SOCKET;
extern int MK_UNABLE_TO_CHANGE_FTP_MODE;
extern int MK_UNABLE_TO_CREATE_SOCKET;
extern int MK_UNABLE_TO_LOCATE_FILE;
extern int MK_UNABLE_TO_OPEN_FILE;
extern int MK_UNABLE_TO_SEND_PORT_COMMAND;
extern int MK_UNABLE_TO_DELETE_DIRECTORY;
extern int MK_COULD_NOT_CREATE_DIRECTORY;
extern int XP_ERRNO_EWOULDBLOCK;
extern int MK_OUT_OF_MEMORY;
extern int MK_DISK_FULL;
extern int XP_COULD_NOT_LOGIN_TO_FTP_SERVER;
extern int XP_ERROR_COULD_NOT_MAKE_CONNECTION_NON_BLOCKING;
extern int XP_POSTING_FILE;
extern int XP_TITLE_DIRECTORY_OF_ETC;
extern int XP_UPTO_HIGHER_LEVEL_DIRECTORY ;


#define PUTSTRING(s)           (*cd->stream->put_block) \
                                 (cd->stream, s, PL_strlen(s))
#define PUTBLOCK(b,l)         (*cd->stream->put_block) \
                                (cd->stream, b, l)
#define COMPLETE_STREAM   (*cd->stream->complete) \
                                 (cd->stream)
#define ABORT_STREAM(s)   (*cd->stream->abort) \
                                 (cd->stream, s)


#define FTP_GENERIC_TYPE     0
#define FTP_UNIX_TYPE        1
#define FTP_DCTS_TYPE        2
#define FTP_NCSA_TYPE        3
#define FTP_PETER_LEWIS_TYPE 4
#define FTP_MACHTEN_TYPE     5
#define FTP_CMS_TYPE         6
#define FTP_TCPC_TYPE        7
#define FTP_VMS_TYPE         8
#define FTP_NT_TYPE          9
/* bug # 309043 */
#define FTP_WEBSTAR_TYPE     10


#define OUTPUT_BUFFER_SIZE 2048

PRIVATE XP_List * connection_list=0;  /* a pointer to a list of open connections */

PRIVATE Bool   	net_use_pasv=TRUE;
PRIVATE char *	ftp_last_password=0;
PRIVATE char *	ftp_last_password_host=0;
PRIVATE PRBool	net_send_email_address_as_password=FALSE;

#define PC_FTP_PASSWORD_KEY "pass"
#define PC_FTP_MODULE_KEY   "ftp"

typedef struct _FTPConnection {
    char   *hostname;       /* hostname string (may contain :port) */
    PRFileDesc *csock;          /* control socket */
    int     server_type;    /* type of server */
    int     cur_mode;       /* ascii, binary, unknown */
    Bool use_list;       /* use LIST? (or NLST) */
    Bool busy;           /* is the connection in use already? */
    Bool no_pasv;        /* should we NOT use pasv mode since is doesn't work? */
    Bool prev_cache;     /* did this connection come from the cache? */
    Bool no_restart;     /* do we know that restarts don't work? */
} FTPConnection;

/* define the states of the machine 
 */
typedef enum _FTPStates {
FTP_WAIT_FOR_RESPONSE,
FTP_WAIT_FOR_AUTH,
FTP_CONTROL_CONNECT,
FTP_CONTROL_CONNECT_WAIT,
FTP_SEND_USERNAME,
FTP_SEND_USERNAME_RESPONSE,
FTP_GET_PASSWORD,
FTP_GET_PASSWORD_RESPONSE,
FTP_SEND_PASSWORD,
FTP_SEND_PASSWORD_RESPONSE,
FTP_SEND_ACCT,
FTP_SEND_ACCT_RESPONSE,
FTP_SEND_REST_ZERO,
FTP_SEND_REST_ZERO_RESPONSE,
FTP_SEND_SYST,
FTP_SEND_SYST_RESPONSE,
FTP_SEND_MAC_BIN,
FTP_SEND_MAC_BIN_RESPONSE,
FTP_SEND_PWD,
FTP_SEND_PWD_RESPONSE,
FTP_SEND_PASV,
FTP_SEND_PASV_RESPONSE,
FTP_PASV_DATA_CONNECT,
FTP_PASV_DATA_CONNECT_WAIT,
FTP_FIGURE_OUT_WHAT_TO_DO,
FTP_SEND_DELETE_FILE,
FTP_SEND_DELETE_FILE_RESPONSE,
FTP_SEND_DELETE_DIR,
FTP_SEND_DELETE_DIR_RESPONSE,
FTP_SEND_MKDIR,
FTP_SEND_MKDIR_RESPONSE,
FTP_SEND_PORT,
FTP_SEND_PORT_RESPONSE,
FTP_GET_FILE_TYPE,
FTP_SEND_BIN,
FTP_SEND_BIN_RESPONSE,
FTP_SEND_ASCII,
FTP_SEND_ASCII_RESPONSE,
FTP_GET_FILE_SIZE,
FTP_GET_FILE_SIZE_RESPONSE,
FTP_GET_FILE_MDTM,
FTP_GET_FILE_MDTM_RESPONSE,
FTP_GET_FILE,
FTP_BEGIN_PUT,
FTP_BEGIN_PUT_RESPONSE,
FTP_DO_PUT,
FTP_SEND_REST,
FTP_SEND_REST_RESPONSE,
FTP_SEND_RETR,
FTP_SEND_RETR_RESPONSE,
FTP_SEND_CWD,
FTP_SEND_CWD_RESPONSE,
FTP_SEND_LIST_OR_NLST,
FTP_SEND_LIST_OR_NLST_RESPONSE,
FTP_SETUP_STREAM,
FTP_WAIT_FOR_ACCEPT,
FTP_START_READ_FILE,
FTP_READ_CACHE,
FTP_READ_FILE,
FTP_START_READ_DIRECTORY,
FTP_READ_DIRECTORY,
FTP_PRINT_DIR,
FTP_DONE,
FTP_ERROR_DONE,
FTP_FREE_DATA
} FTPStates;

typedef struct _FTPConData {
    FTPStates     next_state;        /* the next state of the machine */
    Bool          pause_for_read;    /* should we pause (return) for read? */

    NET_StreamClass *stream;         /* the outgoing data stream */

    FTPConnection   *cc;  /* information about the control connection */

    PRFileDesc *dsock;                /* data socket */
    PRFileDesc *listen_sock;          /* socket to listen on */
    Bool    restart;              /* is this a restarted fetch? */
    Bool    use_list;             /* use LIST or NLST */
    Bool    pasv_mode;            /* using pasv mode? */
    Bool    is_directory;         /* is the url a directory? */
    Bool    retr_already_failed;  /* did RETR fail? */
	Bool	store_password;		  /* cache password? */
    char   *data_buf;             /* unprocessed data from the socket */
    int32   data_buf_size;        /* size of the unprocesed data */

	Bool use_default_path;        /* set if we should use PWD to figure out
							       * current path and use that 
							       */

    /* used by response */
    FTPStates   next_state_after_response;  /* the next state after 
											 * full response recieved */
    int         response_code;              /* code returned in response */
    char       *return_msg;                 /* message from response */
    int         cont_response;              /* a continuation code */

    char   *data_con_address;     /* address and port for PASV data connection*/
    char   *filename;             /* name of the file we are getting */ 
    char   *file_to_post;         /* local filename to put */
    XP_File file_to_post_fp;      /* filepointer */
    char   *post_file_to;         /* If non-NULL, specifies the destination of
                                     the source file, file_to_post.  If NULL, 
                                     generate the destination name automatically
                                     from file_to_post. */
	int32   total_size_of_files_to_post;   /* total bytes */
    char   *path;                 /* path as returned by MKParse */
    int     type;                 /* a type indicator for the file */
    char   *username;             /* the username if not anonymous */
    char   *password;             /* the password if not anonymous */

    char   *cwd_message_buffer;   /* used to print out server help messages */
    char   *login_message_buffer; /* used to print out server help messages */

	char   *output_buffer;		  /* buffer before sending to PUTSTRING */

    SortStruct *sort_base;        /* base pointer to Qsorting list */

    Bool destroy_graph_progress;  /* do we need to destroy graph progress? */    
    Bool destroy_file_upload_progress_dialog;  /* do we need to destroy the dialog? */
	Bool calling_netlib_all_the_time;  /* is SetCallNetlibAllTheTime set? */
	int32   original_content_length; /* the content length at the time of
                                      * calling graph progress
                                      */

    TCP_ConData *tcp_con_data;     /* Data pointer for tcp connect 
									* state machine */

	int32   buff_size;             /* used by ftp_send_file */
	int32   amt_of_buff_wrt;       /* amount of data already written
									* used by ftp_send_file()
									*/
	void   *write_post_data_data;
    XP_File partial_cache_fp;      /* pointer to partial cache file */
} FTPConData;

/* forward decls */
static const char *pref_email_as_ftp_password = "security.email_as_ftp_password";
PRIVATE int32 net_ProcessFTP(ActiveEntry * ce);
PUBLIC PRBool stub_PromptPassword(MWContext *context,
                                  char *prompt,
                                  XP_Bool *remember,
                                  XP_Bool is_secure,
                                  void *closure);

/* function prototypes
 */
PRIVATE NET_FileEntryInfo * net_parse_dir_entry (char *entry, int server_type);

/* call this to enable/disable FTP PASV mode.
 * Note: PASV mode is on by default
 */
PUBLIC void
NET_UsePASV(Bool do_it)
{
	net_use_pasv = do_it;
}

/* callback routine invoked by prefapi when the pref value changes */
/* fix Mac missing prototype warnings */
MODULE_PRIVATE int PR_CALLBACK
net_ftp_pref_changed(const char * newpref, void * data);

MODULE_PRIVATE int PR_CALLBACK
net_ftp_pref_changed(const char * newpref, void * data)
{
	PREF_GetBoolPref(pref_email_as_ftp_password, &net_send_email_address_as_password);
	return PREF_NOERROR;
}

PRIVATE XP_Bool
net_get_send_email_address_as_password(void)
{
#ifdef XP_WIN
	static XP_Bool	net_read_FTP_password_pref = FALSE;

	if (!net_read_FTP_password_pref) {
        if ( (PREF_OK != PREF_GetBoolPref(pref_email_as_ftp_password, &net_send_email_address_as_password)) ) {
            net_send_email_address_as_password = FALSE;
        }
		PREF_RegisterCallback(pref_email_as_ftp_password, net_ftp_pref_changed, NULL);
		net_read_FTP_password_pref = TRUE;
	}
#endif

	return net_send_email_address_as_password;
}

#if defined(XP_MAC) || defined(XP_UNIX)
/* call this with TRUE to enable the sending of the email
 * address as the default user "anonymous" password.
 * The default is OFF.  
 */
PUBLIC void
NET_SendEmailAddressAsFTPPassword(Bool do_it)
{
	net_send_email_address_as_password = (XP_Bool)do_it;
}
#endif

PRIVATE NET_StreamClass *
net_ftp_make_stream(FO_Present_Types format_out, 
					URL_Struct *URL_s, 
					MWContext *window_id)
{
	MWContext *stream_context;

#ifdef MOZILLA_CLIENT
	/* if the context can't handle HTML then we
	 * need to generate an HTML dialog to handle
	 * the message
	 */
	if(URL_s->files_to_post && EDT_IS_EDITOR(window_id))
	  {
		Chrome chrome_struct;
		memset(&chrome_struct, 0, sizeof(Chrome));
		chrome_struct.is_modal = TRUE;
		chrome_struct.allow_close = TRUE;
		chrome_struct.allow_resize = TRUE;
		chrome_struct.show_scrollbar = TRUE;
		chrome_struct.w_hint = 400;
		chrome_struct.h_hint = 300;
#ifdef XP_MAC
		/* on Mac, topmost windows are floating windows not dialogs */
		chrome_struct.topmost = FALSE;
		/* disable commands to change to minimal menu bar; */
		/* avoids confusion about which commands are present */
		chrome_struct.disable_commands = TRUE;
#else
		chrome_struct.topmost = TRUE;
#endif

		stream_context = FE_MakeNewWindow(window_id,
		NULL,
		NULL,
		&chrome_struct);
		if(!stream_context)
			return (NULL);
	  }
	else
#endif /* MOZILLA_CLIENT */
	  {
		stream_context = window_id;
	  }

	return(NET_StreamBuilder(format_out, URL_s, stream_context));

}

PRIVATE int
net_ftp_show_error(ActiveEntry *ce, char *line)
{
    FTPConData * cd = (FTPConData *)ce->con_data;

	/* show the error
	 */
	StrAllocCopy(ce->URL_s->content_type, TEXT_HTML);
	ce->URL_s->expires = 1; /* immediate expire date */
	cd->stream = net_ftp_make_stream(ce->format_out, ce->URL_s, ce->window_id);

	if(cd->stream)
	  {
		PL_strcpy(cd->output_buffer, XP_GetString(XP_HTML_FTPERROR_TRANSFER));
		if(ce->status > -1)
			ce->status = PUTSTRING(cd->output_buffer);
		if(line && ce->status > -1)
			ce->status = PUTSTRING(cd->return_msg);
		PL_strcpy(cd->output_buffer, "</PRE>");
		if(ce->status > -1)
			ce->status = PUTSTRING(cd->output_buffer);
	  }
	  
	cd->next_state = FTP_ERROR_DONE;
	return MK_INTERRUPTED;
}

/* get a reply from the server
 */
PRIVATE int 
net_ftp_response (ActiveEntry *ce)
{
    FTPConData * cd = (FTPConData *)ce->con_data;
    int result_code;        /* numerical result code from server */
    int status;
    char cont_char;
    char * line;

    status = NET_BufferedReadLine(cd->cc->csock, &line, &cd->data_buf, 
                        &cd->data_buf_size, &cd->pause_for_read);

    if (status > 0 && !line && cd->next_state_after_response == FTP_DONE)
    {
      /* This is wierd. I have been seeing this consistently when the ftp directory
       * listing has ended. The condition is that there is no data on the socket,
       * and we are waiting to gobble the final bytes from the ftp server for
       * completing a ftp directory listing. NET_BufferedReadLine() seems to indicate
       * that the call would block and no data is available now by returning a
       * status of 1 and line set to NULL.
       *
       * For the end of ftp directory, next_state_after_response will be FTP_DONE.
       * We will use that to fix this. How we are fixing it is by turning status to 0.
       * This will make it adopt the following if condition of status==0 and set next_state to
       * next_state_after_response which would be done.
       *
       * If we dont do this, ftp directory doesnt end and we have our ftp waiting indefinitely
       * for some thing from the ftp server while there aint any more data showing up.
       *
       * XXX The right thing however would be to see why we are getting a non-zero status
       * XXX but no data from NET_BufferedReadLine(). The smarted question to ask would
       * XXX be "if no data is available, then why was this socket selected for read again ?"
       */
      TRACEMSG(("Wierd state. status > 0 but no data and next_state_after_response = %d. Fixing up...\n", cd->next_state_after_response));
      status = 0;
    }

	if(status == 0)
	  {
		/* this shouldn't really happen, but...
		 */
     	cd->next_state = cd->next_state_after_response;
     	cd->pause_for_read = FALSE; /* don't pause */
		return(0);
	  }
    else if(status < 0)
      {
		int rv = PR_GetError();

        if (rv == PR_WOULD_BLOCK_ERROR)
          {
            cd->pause_for_read = TRUE;
            return(0);
          }

		ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_TCP_READ_ERROR, rv);

    	/* return TCP error
         */
		return MK_TCP_READ_ERROR;
      }
	else if(!line)
	  {
         return status; /* wait for line */
	  }

    TRACEMSG(("    Rx: %s", line));

	/* Login messages or CWD messages 
     * Their so handy, those help messages are :)
     */
    if(cd->cc->server_type == FTP_UNIX_TYPE && !PL_strncmp(line, "250-",4))
      {
        StrAllocCat(cd->cwd_message_buffer, line+4);
        StrAllocCat(cd->cwd_message_buffer, "\n");  /* nasty */
      }
    else if(!PL_strncmp(line,"230-",4) || !PL_strncmp(line,"220-",4))
      {
        StrAllocCat(cd->login_message_buffer, line+4);
        StrAllocCat(cd->login_message_buffer, "\n");  /* nasty */
      }
    else if(cd->cont_response == 230)
      {
	 	/* this does the same thing as the one directly above it
		 * but it checks for 230 response code instead
	  	 * this is to fix buggy ftp servers that don't follow specs
		 */
		if(PL_strncmp(line, "230", 3))
		  {
			/* make sure it's not  "230 Guest login ok...
			 */
            StrAllocCat(cd->login_message_buffer, line);
            StrAllocCat(cd->login_message_buffer, "\n");  /* nasty */
		  }
      }

	 result_code = 0;          /* defualts */
	 cont_char = ' ';  /* defualts */
     sscanf(line, "%d%c", &result_code, &cont_char);

     if(cd->cont_response == -1) 
       {
         if (cont_char == '-')  /* start continuation */
             cd->cont_response = result_code;

         StrAllocCopy(cd->return_msg, line+4);
       } 
     else 
       {    /* continuing */
      	 if (cd->cont_response == result_code && cont_char == ' ')
             cd->cont_response = -1;    /* ended */

         StrAllocCat(cd->return_msg, "\n");
		 if(PL_strlen(line) > 3)
		   {
			 if(isdigit(line[0]))
                 StrAllocCat(cd->return_msg, line+4);
			 else
                 StrAllocCat(cd->return_msg, line);
		   }
		 else
		   {
             StrAllocCat(cd->return_msg, line);
		   }
       }    

     if(cd->cont_response == -1)  /* all done with this response? */
       {
     	 cd->next_state = cd->next_state_after_response;
     	 cd->pause_for_read = FALSE; /* don't pause */
       }
	 if(result_code == 551)
	   {
		 return net_ftp_show_error(ce, line + 4);
	   }
        
    cd->response_code = result_code/100;

    return(1);  /* everything ok */
}

PRIVATE int
net_send_username(FTPConData * cd)
{

	/* look for the word UNIX in the hello message
	 * if it is there assume a unix server that can
	 * use the LIST command
	 */	
	if(cd->return_msg && PL_strcasestr(cd->return_msg, "UNIX"))
	  {
        cd->cc->server_type = FTP_UNIX_TYPE;
		cd->cc->use_list = TRUE;
	  }

    if (cd->username)
      {
        PR_snprintf(cd->output_buffer, OUTPUT_BUFFER_SIZE, "USER %.256s%c%c", cd->username, CR, LF);
      }
    else
      {
        PR_snprintf(cd->output_buffer, OUTPUT_BUFFER_SIZE, "USER anonymous%c%c", CR, LF);
      }
	
	TRACEMSG(("FTP Rx: %.256s", cd->output_buffer));

    cd->next_state = FTP_WAIT_FOR_RESPONSE;
    cd->next_state_after_response = FTP_SEND_USERNAME_RESPONSE;
    cd->pause_for_read = TRUE;

    return((int) NET_BlockingWrite(cd->cc->csock, 
							 cd->output_buffer, 
							 PL_strlen(cd->output_buffer)));
}

PRIVATE int
net_send_username_response(ActiveEntry * ce)
{
    FTPConData * cd = (FTPConData *)ce->con_data;

    if (cd->response_code == 3)  /* Send password */
      {
		if(!cd->password)
            cd->next_state = FTP_GET_PASSWORD;
		else 
			cd->next_state = FTP_SEND_PASSWORD;
      }
    else if (cd->response_code == 2) /* Logged in */
      {
          if( cd->cc->no_restart )
              cd->next_state = FTP_SEND_SYST;
          else
              cd->next_state = FTP_SEND_REST_ZERO;
      }
    else
      {
		/* show the error
		 */
	 	StrAllocCopy(ce->URL_s->content_type, TEXT_HTML);
		ce->URL_s->expires = 1; /* immediate expire date */
        cd->stream = net_ftp_make_stream(ce->format_out, ce->URL_s, ce->window_id);

		if(cd->stream)
		  {
			PR_snprintf(cd->output_buffer, OUTPUT_BUFFER_SIZE, XP_GetString(XP_HTML_FTPERROR_NOLOGIN)) ;
			if(ce->status > -1)
    			ce->status = PUTSTRING(cd->output_buffer);
			if(cd->return_msg && ce->status > -1)
    		    ce->status = PUTSTRING(cd->return_msg);
			PR_snprintf(cd->output_buffer, OUTPUT_BUFFER_SIZE, "</PRE>");
			if(ce->status > -1)
    			ce->status = PUTSTRING(cd->output_buffer);

			if(ce->status > -1)
           		COMPLETE_STREAM;
			else
                ABORT_STREAM(ce->status);

			PR_Free(cd->stream);
			cd->stream = NULL;
		  }
		
        cd->next_state = FTP_ERROR_DONE;

		/* make sure error_msg is empty */
		PR_FREEIF(ce->URL_s->error_msg);
		ce->URL_s->error_msg = NULL;

        return MK_UNABLE_TO_LOGIN;

      }

    return(0); /* good */
}


PRIVATE int
net_send_password(FTPConData * cd)
{

    PR_snprintf(cd->output_buffer, OUTPUT_BUFFER_SIZE, 
				"PASS %.256s%c%c", 
				NET_UnEscape(cd->password), 
				CR, LF);

    cd->next_state = FTP_WAIT_FOR_RESPONSE;
    cd->next_state_after_response = FTP_SEND_PASSWORD_RESPONSE;
    cd->pause_for_read = TRUE;

	TRACEMSG(("FTP Tx: %s", cd->output_buffer));

    return((int) NET_BlockingWrite(cd->cc->csock, 
							 cd->output_buffer, 
							 PL_strlen(cd->output_buffer)));
}

PRIVATE char *
gen_ftp_password_key(char *address, char *username)
{
	char *rv=NULL;

	if(!address || !username)
		return NULL;

	StrAllocCopy(rv, address);
	StrAllocCat(rv, "\t");
	StrAllocCat(rv, username);

	return rv;
}

PRIVATE int
net_send_password_response(ActiveEntry * ce)
{
    FTPConData * cd = (FTPConData *)ce->con_data;

    if (cd->response_code == 3)
      {
        cd->next_state = FTP_SEND_ACCT;
      }
    else if (cd->response_code == 2) /* logged in */
      {
		  if(cd->store_password && cd->username)
		  {
			/* store the password in the cache */
			char *key = gen_ftp_password_key(ce->URL_s->address, cd->username);	

			if(key)
			{
				PCNameValueArray * array = PC_NewNameValueArray();
		
				PC_AddToNameValueArray(array, PC_FTP_PASSWORD_KEY, cd->password);
				
				PC_StorePasswordNameValueArray(PC_FTP_MODULE_KEY, key, array);

				PR_Free(key);

				PC_FreeNameValueArray(array);
			}
		  }

          if( cd->cc->no_restart )
              cd->next_state = FTP_SEND_SYST;
          else
              cd->next_state = FTP_SEND_REST_ZERO;
      }
    else
      {
		PR_FREEIF(ftp_last_password_host);
		PR_FREEIF(ftp_last_password);

        cd->next_state = FTP_ERROR_DONE;
#if defined(SingleSignon)
	if (cd->cc) { /* just for safety, probably cd->cc can never be null */
	    SI_RemoveUser(cd->cc->hostname, cd->username, TRUE);
	}
#endif
        FE_Alert(ce->window_id, cd->return_msg ? cd->return_msg :
								 XP_GetString( XP_COULD_NOT_LOGIN_TO_FTP_SERVER ) );
		/* no error status message in this case 
		 */
        return MK_UNABLE_TO_LOGIN;  /* negative return code */
      }

    return(0); /* good */
}
    

PRIVATE int
net_send_acct(FTPConData * cd)
{

    PR_snprintf(cd->output_buffer, OUTPUT_BUFFER_SIZE, "ACCT noaccount%c%c", CR, LF);

    cd->next_state = FTP_WAIT_FOR_RESPONSE;
    cd->next_state_after_response = FTP_SEND_ACCT_RESPONSE;
    cd->pause_for_read = TRUE;

	TRACEMSG(("FTP Tx: %s", cd->output_buffer));

    return((int) NET_BlockingWrite(cd->cc->csock, 
							 cd->output_buffer, 
							 PL_strlen(cd->output_buffer)));
}

PRIVATE int
net_send_acct_response(ActiveEntry * ce)
{
    FTPConData * cd = (FTPConData *)ce->con_data;

    if (cd->response_code == 2)
      {
          if( cd->cc->no_restart )
              cd->next_state = FTP_SEND_SYST;
          else
              cd->next_state = FTP_SEND_REST_ZERO;
      }
    else
      {
        cd->next_state = FTP_ERROR_DONE;
        FE_Alert(ce->window_id, cd->return_msg ? cd->return_msg : 
								 XP_GetString( XP_COULD_NOT_LOGIN_TO_FTP_SERVER ) );
		/* no error status message in this case 
		 */
        return MK_UNABLE_TO_LOGIN;  /* negative return code */
      }

    return(0); /* good */
}

PRIVATE int
net_send_rest_zero(ActiveEntry * ce)
{
    FTPConData * cd = (FTPConData *)ce->con_data;
    
    PR_snprintf(cd->output_buffer, OUTPUT_BUFFER_SIZE, "REST 0%c%c", CR, LF);

    cd->next_state = FTP_WAIT_FOR_RESPONSE;
    cd->next_state_after_response = FTP_SEND_REST_ZERO_RESPONSE;
    cd->pause_for_read = TRUE;

	TRACEMSG(("FTP Tx: %s", cd->output_buffer));

    return((int) NET_BlockingWrite(cd->cc->csock, 
							 cd->output_buffer, 
							 PL_strlen(cd->output_buffer)));
}

PRIVATE int
net_send_rest_zero_response(ActiveEntry * ce)
{
    FTPConData * cd = (FTPConData *)ce->con_data;

    if( cd->response_code == 3 )
    {
        TRACEMSG(("Server can do restarts!"));
        ce->URL_s->server_can_do_restart = TRUE;
        cd->cc->no_restart = FALSE;
    }
    else
    {
        TRACEMSG(("Server can NOT do restarts!"));
        ce->URL_s->server_can_do_restart = FALSE;
        cd->cc->no_restart = TRUE;
    }

    cd->next_state = FTP_SEND_SYST;
    return 0;
}

PRIVATE int
net_send_syst(FTPConData * cd)
{
    

    PR_snprintf(cd->output_buffer, OUTPUT_BUFFER_SIZE, "SYST%c%c", CR, LF);

    cd->next_state = FTP_WAIT_FOR_RESPONSE;
    cd->next_state_after_response = FTP_SEND_SYST_RESPONSE;
    cd->pause_for_read = TRUE;

	TRACEMSG(("FTP Tx: %s", cd->output_buffer));

    return((int) NET_BlockingWrite(cd->cc->csock, 
							 cd->output_buffer, 
							 PL_strlen(cd->output_buffer)));
}


PRIVATE int
net_send_syst_response(FTPConData * cd)
{
    if (cd->response_code == 2)
      {
        TRACEMSG(("SYST response: %s",cd->return_msg));

        /* default next state will be to try PASV mode
		 *
		 * TEST: set this default next state to FTP_SEND_PORT
		 * to test the non-pasv mode ftp
         */
		if(cd->use_default_path)
			cd->next_state = FTP_SEND_PWD;
    	else
        	cd->next_state = FTP_FIGURE_OUT_WHAT_TO_DO;

                /* we got a line -- what kind of server are we talking to? */
        if (!PL_strncmp(cd->return_msg, "UNIX Type: L8 MAC-OS MachTen", 28))
          {
            cd->cc->server_type = FTP_MACHTEN_TYPE;
            cd->cc->use_list = TRUE;
          }
        else if (PL_strstr(cd->return_msg, "UNIX") != NULL)
          {
             cd->cc->server_type = FTP_UNIX_TYPE;
             cd->cc->use_list = TRUE;
          }
        else if (PL_strstr(cd->return_msg, "Windows_NT") != NULL)
          {
             cd->cc->server_type = FTP_NT_TYPE;
             cd->cc->use_list = TRUE;
          }
        else if (PL_strncmp(cd->return_msg, "VMS", 3) == 0)
          {
             cd->cc->server_type = FTP_VMS_TYPE;
             cd->cc->use_list = TRUE;
          }
        else if ((PL_strncmp(cd->return_msg, "VM/CMS", 6) == 0)
                                 || (PL_strncmp(cd->return_msg, "VM ", 3) == 0))
          {
             cd->cc->server_type = FTP_CMS_TYPE;
          }
        else if (PL_strncmp(cd->return_msg, "DCTS", 4) == 0)
          {
             cd->cc->server_type = FTP_DCTS_TYPE;
          }
        else if (PL_strstr(cd->return_msg, "MAC-OS TCP/Connect II") != NULL)
          {
             cd->cc->server_type = FTP_TCPC_TYPE;
         	 cd->next_state = FTP_SEND_PWD;
          }
        else if (PL_strncmp(cd->return_msg, "MACOS Peter's Server", 20) == 0)
          {
             cd->cc->server_type = FTP_PETER_LEWIS_TYPE;
             cd->cc->use_list = TRUE;
             cd->next_state = FTP_SEND_MAC_BIN; 
          }
        else if (PL_strncmp(cd->return_msg, "MACOS WebSTAR FTP", 17) == 0)
          {
             cd->cc->server_type = FTP_WEBSTAR_TYPE;
             cd->cc->use_list = TRUE;
             cd->next_state = FTP_SEND_MAC_BIN; 
          }
        else if(cd->cc->server_type == FTP_GENERIC_TYPE)
          {
              cd->next_state = FTP_SEND_PWD; 
          }
      }
    else
      {
        cd->next_state = FTP_SEND_PWD;
      }

    TRACEMSG(("Set server type to: %d\n",cd->cc->server_type));

    return(0);  /* good */

} 

PRIVATE int
net_send_mac_bin(FTPConData * cd)
{
    /* Check bug # 323918 for MACB details */
    PR_snprintf(cd->output_buffer, OUTPUT_BUFFER_SIZE, "MACB ENABLE%c%c", CR, LF);

    cd->next_state = FTP_WAIT_FOR_RESPONSE;
    cd->next_state_after_response = FTP_SEND_MAC_BIN_RESPONSE;
    cd->pause_for_read = TRUE;

	TRACEMSG(("FTP Tx: %s", cd->output_buffer));

    return((int) NET_BlockingWrite(cd->cc->csock, 
							 cd->output_buffer, 
							 PL_strlen(cd->output_buffer)));
}


PRIVATE int
net_send_mac_bin_response(FTPConData * cd)
{
    if(cd->response_code == 2)
      {  /* succesfully set mac binary */
        if(cd->cc->server_type == FTP_UNIX_TYPE)
            cd->cc->server_type = FTP_NCSA_TYPE;  /* we were unsure here */
      }

	cd->next_state = FTP_FIGURE_OUT_WHAT_TO_DO;
    
    return(0); /* continue */
}

PRIVATE
int
net_figure_out_what_to_do(ActiveEntry *ce)
{
    FTPConData * cd = (FTPConData *)ce->con_data;

	switch(ce->URL_s->method)
	  {
		case URL_MKDIR_METHOD:
			cd->next_state = FTP_SEND_MKDIR;
			break;

		case URL_DELETE_METHOD:
			cd->next_state = FTP_SEND_DELETE_FILE;
			break;

		case URL_PUT_METHOD:
			/* trim the path to the last slash */
			{
				char * slash = PL_strrchr(cd->path, '/');
				if(slash)
					*slash = '\0';
			}
			/* fall through */
			goto get_method_case;
					
		case URL_POST_METHOD:
			/* we support POST if files_to_post
			 * is filed in
			 */
			PR_ASSERT(ce->URL_s->files_to_post);
			/* fall through */

		case URL_GET_METHOD:
get_method_case:  /* ack! goto alert */

			/* don't send PASV if the server cant do it */
			if(cd->cc->no_pasv || !net_use_pasv)
				cd->next_state = FTP_SEND_PORT;
			else
				cd->next_state = FTP_SEND_PASV;
			break;

		default:
			PR_ASSERT(0);
	  }

	return(0);
}

PRIVATE int
net_send_mkdir(ActiveEntry *ce)
{
    FTPConData * cd = (FTPConData *)ce->con_data;

    PR_snprintf(cd->output_buffer, 
				OUTPUT_BUFFER_SIZE, 
				"MKD %s"CRLF, 
				cd->path);

    cd->next_state = FTP_WAIT_FOR_RESPONSE;
    cd->next_state_after_response = FTP_SEND_MKDIR_RESPONSE;
    cd->pause_for_read = TRUE;

    TRACEMSG(("FTP Rx: %s", cd->output_buffer));

    return((int) NET_BlockingWrite(cd->cc->csock,
                             cd->output_buffer,
                             PL_strlen(cd->output_buffer)));
}

PRIVATE int
net_send_mkdir_response(ActiveEntry *ce)
{
	FTPConData * cd = (FTPConData *)ce->con_data;

	if (cd->response_code !=2) 
      {
		/* error */
		ce->URL_s->error_msg = NET_ExplainErrorDetails(
												MK_COULD_NOT_CREATE_DIRECTORY,
												cd->path);
		return(MK_COULD_NOT_CREATE_DIRECTORY);
	  }
	
	/* success */

	cd->next_state = FTP_DONE;

	return(0);
}


PRIVATE int
net_send_delete_file(ActiveEntry *ce)
{
    FTPConData * cd = (FTPConData *)ce->con_data;

    PR_snprintf(cd->output_buffer, 
				OUTPUT_BUFFER_SIZE, 
				"DELE %s"CRLF, 
				cd->path);

    cd->next_state = FTP_WAIT_FOR_RESPONSE;
    cd->next_state_after_response = FTP_SEND_DELETE_FILE_RESPONSE;
    cd->pause_for_read = TRUE;

    TRACEMSG(("FTP Rx: %s", cd->output_buffer));

    return((int) NET_BlockingWrite(cd->cc->csock,
                             cd->output_buffer,
                             PL_strlen(cd->output_buffer)));
}

PRIVATE int
net_send_delete_file_response(ActiveEntry *ce)
{
	FTPConData * cd = (FTPConData *)ce->con_data;

	if (cd->response_code !=2) 
      {
		/* error */
		/* try and delete it as a file */
		cd->next_state = FTP_SEND_DELETE_DIR;
		return(0);
	  }
	
	/* success */

	cd->next_state = FTP_DONE;

	return(0);
}

PRIVATE int
net_send_delete_dir(ActiveEntry *ce)
{
    FTPConData * cd = (FTPConData *)ce->con_data;

    PR_snprintf(cd->output_buffer, 
				OUTPUT_BUFFER_SIZE, 
				"RMD %s"CRLF, 
				cd->path);

    cd->next_state = FTP_WAIT_FOR_RESPONSE;
    cd->next_state_after_response = FTP_SEND_DELETE_DIR_RESPONSE;
    cd->pause_for_read = TRUE;

    TRACEMSG(("FTP Rx: %s", cd->output_buffer));

    return((int) NET_BlockingWrite(cd->cc->csock,
                             cd->output_buffer,
                             PL_strlen(cd->output_buffer)));
}

PRIVATE int
net_send_delete_dir_response(ActiveEntry *ce)
{
	FTPConData * cd = (FTPConData *)ce->con_data;

	if (cd->response_code !=2) 
      {
		/* error */
		ce->URL_s->error_msg = NET_ExplainErrorDetails(
												MK_UNABLE_TO_DELETE_DIRECTORY,
												cd->path);
		return(MK_UNABLE_TO_DELETE_DIRECTORY);
	  }
	
	/* success */

	cd->next_state = FTP_DONE;

	return(0);
}

PRIVATE int
net_send_pwd(FTPConData * cd)
{
    PR_snprintf(cd->output_buffer, OUTPUT_BUFFER_SIZE, "PWD%c%c", CR, LF);

    cd->next_state = FTP_WAIT_FOR_RESPONSE;
    cd->next_state_after_response = FTP_SEND_PWD_RESPONSE;
    cd->pause_for_read = TRUE;

	TRACEMSG(("FTP Rx: %s", cd->output_buffer));

    return((int) NET_BlockingWrite(cd->cc->csock, 
							 cd->output_buffer, 
							 PL_strlen(cd->output_buffer)));
}

PRIVATE int
net_send_pwd_response(ActiveEntry *ce)
{
    FTPConData * cd = (FTPConData *)ce->con_data;
    char *cp = PL_strchr(cd->return_msg+5,'"');

    if(cp) 
        *cp = '\0';

    /* default next state */
    /* don't send PASV if the server cant do it */
    cd->next_state = FTP_FIGURE_OUT_WHAT_TO_DO;

    if (cd->cc->server_type == FTP_TCPC_TYPE)
      {
        cd->cc->server_type = (cd->return_msg[1] == '/' ? 
								FTP_NCSA_TYPE : FTP_TCPC_TYPE);
      }
	else if(cd->cc->server_type == FTP_GENERIC_TYPE)
	  {
    	if (cd->return_msg[1] == '/')
          {
            /* path names beginning with / imply Unix,
             * right?
             */
              cd->cc->server_type = FTP_UNIX_TYPE;
              cd->cc->use_list = TRUE;
              cd->next_state = FTP_SEND_MAC_BIN;  /* see if it's a mac */
           }
         else if (cd->return_msg[PL_strlen(cd->return_msg)-1] == ']')
           {
             /* path names ending with ] imply VMS, right? */
             cd->cc->server_type = FTP_VMS_TYPE;
             cd->cc->use_list = TRUE;
           }
	  }

	/* use the path returned by PWD if use_default_path is set
	 * don't do this for vms hosts
	 */
	if(cd->use_default_path && cd->cc->server_type != FTP_VMS_TYPE)
	  {
		char *path = strtok(&cd->return_msg[1], "\"");
		char *ptr;
		char *existing_path = cd->path;

		/* NT can return a drive letter.  We can't
		 * deal with a drive letter right now
		 * so we need to just strip the drive letter
		 */
		if(*path != '/') {
		  ptr = PL_strchr(path, '/');
          /* If that didn't work, look for a backslash (e.g., OS/2) */
          if( ptr == (char *)0 ) {
              ptr = PL_strchr(path, '\\');
              /* If that worked, then go flip the slashes so the URL is legal */
              if( ptr != (char *)0 ) {
                  char *p;
                  for( p = ptr; *p != '\0'; p++ ){
                      if( *p == '\\' ) {
                          *p = '/';
                      }
                  }
              }
          }
        }
		else
		  ptr = path;

		if(ptr)
		  {
			char * new_url;

			cd->path = NULL;
			StrAllocCopy(cd->path, ptr);

			if(existing_path && existing_path[0] != '\0')
		 	  {
			  	/* if the new path has a / on the
				 * end remove it since the
				 * existing path already has
				 * a slash at the beginning
				 */
				if(cd->path[0] != '\0')
				  {
					char *end = &cd->path[PL_strlen(cd->path)-1];
					if(*end == '/')
						*end = '\0';
				  }
				StrAllocCat(cd->path, existing_path);
		 	  }

			/* strip the path part from the URL
			 * and add the new path
			 */
			new_url = NET_ParseURL(ce->URL_s->address, 
								   GET_PROTOCOL_PART | GET_USERNAME_PART | GET_HOST_PART);

			StrAllocCat(new_url, cd->path);
			
			PR_Free(ce->URL_s->address);

			ce->URL_s->address = new_url;
		  }


		/* make sure we don't do this again for some reason */
		cd->use_default_path = FALSE;
	  }

     if (cd->cc->server_type == FTP_NCSA_TYPE ||
                        cd->cc->server_type == FTP_TCPC_TYPE ||
                        cd->cc->server_type == FTP_WEBSTAR_TYPE ||
                        cd->cc->server_type == FTP_PETER_LEWIS_TYPE)
         cd->next_state = FTP_SEND_MAC_BIN;

     return(0); /* good */
}


PRIVATE int
net_send_pasv(FTPConData * cd)
{

    PR_snprintf(cd->output_buffer, OUTPUT_BUFFER_SIZE, "PASV%c%c", CR, LF);

    cd->next_state = FTP_WAIT_FOR_RESPONSE;
    cd->next_state_after_response = FTP_SEND_PASV_RESPONSE;

	TRACEMSG(("FTP Rx: %s", cd->output_buffer));

    return((int) NET_BlockingWrite(cd->cc->csock, 
							 cd->output_buffer, 
							 PL_strlen(cd->output_buffer)));
}


PRIVATE int
net_send_pasv_response(FTPConData * cd)
{
    int    h0, h1, h2, h3; 
    int32  p0, p1;
    int32  pasv_port;
    char  *cp;
    int    status;

    if (cd->response_code !=2) 
      {
        cd->next_state = FTP_SEND_PORT; /* default next state */
        cd->pasv_mode = FALSE;
        cd->cc->no_pasv = TRUE;  /* pasv doesn't work don't try it again */
        return(0); /* continue */
      }

    cd->cc->no_pasv = FALSE;  /* pasv does work */

    TRACEMSG(("PASV response: %s\n",cd->return_msg));

    /* find first comma or end of line */
    for(cp=cd->return_msg; *cp && *cp != ','; cp++)
        ; /* null body */

    /* find the begining of digits */
    while (--cp > cd->return_msg && '0' <= *cp && *cp <= '9')
        ; /* null body */   

	/* go past the '(' when the return is of the
	 * form (128,234,563,356,356)
	 */
	if(*cp < '0' || *cp > '9')
		cp++;

    status = sscanf(cp,
#ifdef __alpha
		"%d,%d,%d,%d,%d,%d",
#else
		"%d,%d,%d,%d,%ld,%ld",
#endif
		&h0, &h1, &h2, &h3, &p0, &p1);
    if (status<6)
      {
        TRACEMSG(("FTP: PASV reply has no inet address!\n"));
        cd->next_state = FTP_SEND_PORT; /* default next state */
        cd->pasv_mode = FALSE;
        return(0); /* continue */
      }

    pasv_port = ((int32) (p0<<8)) + p1;
    TRACEMSG(("FTP: Server is listening on port %d\n", pasv_port));

    /* Open connection for data:
     */
    PR_snprintf(cd->output_buffer, OUTPUT_BUFFER_SIZE, "%d.%d.%d.%d:%ld",h0,h1,h2,h3,pasv_port);
    StrAllocCopy(cd->data_con_address, cd->output_buffer);

    cd->next_state = FTP_PASV_DATA_CONNECT;

    return(0); /* OK */
}


PRIVATE int
net_send_port(ActiveEntry * ce)
{
	FTPConData * cd = (FTPConData *) ce->con_data;
    int    status;
	PRNetAddr pr_addr;
    PRSocketOptionData opt;
    PRStatus sock_status;
    
    TRACEMSG(("Entering get_listen_socket with control socket: %d\n", cd->cc->csock));

	cd->pasv_mode = FALSE;  /* not using pasv mode if we get here */

    /*  Create internet socket
    */
    cd->listen_sock = PR_NewTCPSocket();
	
    if(cd->listen_sock == NULL)
	  {
		ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_UNABLE_TO_CREATE_SOCKET);
        return MK_UNABLE_TO_CREATE_SOCKET;
	  }

#if defined(XP_WIN16) || (defined(XP_OS2) && !defined(XP_OS2_DOUGSOCK))
    opt.option = PR_SockOpt_Linger;
    opt.value.linger.polarity = PR_TRUE;
    opt.value.linger.linger = PR_INTERVAL_NO_WAIT;
    PR_SetSocketOption(cd->listen_sock, &opt);
#endif
      /* lets try to set the socket non blocking
       * so that we can do multi threading 
       */
    opt.option = PR_SockOpt_Nonblocking;
    opt.value.non_blocking = PR_TRUE;
    sock_status = PR_SetSocketOption(cd->listen_sock, &opt);
    if (sock_status != PR_SUCCESS)
#if defined(SMOOTH_PROGRESS)
        PM_Status(ce->window_id, ce->URL_s,
                  XP_GetString(XP_ERROR_COULD_NOT_MAKE_CONNECTION_NON_BLOCKING));
#else
        NET_Progress(ce->window_id, 
                     XP_GetString( XP_ERROR_COULD_NOT_MAKE_CONNECTION_NON_BLOCKING ) );
#endif

HG31456

	/* init the addr, leave port zero to let bind allocate one */
	PR_InitializeNetAddr(PR_IpAddrAny, 0, &pr_addr);

    status = PR_GetSockName(cd->cc->csock, &pr_addr);
    if (status != PR_SUCCESS) 
      {
        TRACEMSG(("Failure in getsockname for control connection: %d\n", 
                                cd->cc->csock));
        return -1;
      }
    
    status=PR_Bind(cd->listen_sock, &pr_addr);
    if (status != PR_SUCCESS) 
    {
        TRACEMSG(("Failure in bind for listen socket: %d\n", cd->listen_sock));
       	return -1;
    }

    /* TRACEMSG(("FTP: This host is %s\n", PR_NetAddrToString(&pr_addr))); */
    
	/* get the name again to find out the port */
    status = PR_GetSockName(cd->listen_sock, &pr_addr);
    if (status != PR_SUCCESS)   
        return -1;

    /* TRACEMSG(("FTP: bound to %s\n",PR_NetAddrToString(&pr_addr))); */

    /*  Now we must tell the other side where to connect
     */
    PR_snprintf(cd->output_buffer, OUTPUT_BUFFER_SIZE, "PORT %d,%d,%d,%d,%d,%d%c%c",
            (int)* ((unsigned char*) (&pr_addr.inet.ip)+0),
            (int)* ((unsigned char*) (&pr_addr.inet.ip)+1),
            (int)* ((unsigned char*) (&pr_addr.inet.ip)+2),
            (int)* ((unsigned char*) (&pr_addr.inet.ip)+3),
            (int)* ((unsigned char*) (&pr_addr.inet.port)+0),
            (int)* ((unsigned char*) (&pr_addr.inet.port)+1),
            CR, LF);

    /*  Inform TCP that we will accept connections
     */
    if (PR_Listen(cd->listen_sock, 1) < 0) 
      {
		ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_UNABLE_TO_LISTEN_ON_SOCKET);
        return MK_UNABLE_TO_LISTEN_ON_SOCKET;
      }

    TRACEMSG(("TCP: listen socket(), bind() and listen() all OK.\n"));

    cd->next_state = FTP_WAIT_FOR_RESPONSE;
    cd->next_state_after_response = FTP_SEND_PORT_RESPONSE;
    cd->pause_for_read = TRUE;

	TRACEMSG(("FTP Tx: %s", cd->output_buffer));

    /* Inform the server of the port number we will listen on
     * The port_command came from above
     */
    return((int) NET_BlockingWrite(cd->cc->csock, 
							 cd->output_buffer, 
							 PL_strlen(cd->output_buffer)));

} /* get_listen */

PRIVATE int
net_send_port_response(ActiveEntry * ce)
{
	FTPConData * cd = (FTPConData *) ce->con_data;

    if (cd->response_code !=2)
      {
        TRACEMSG(("FTP: Bad response (port_command: %s)\n",cd->return_msg));
		ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_UNABLE_TO_SEND_PORT_COMMAND);

        return MK_UNABLE_TO_SEND_PORT_COMMAND;
      }

    /* else */

    cd->next_state = FTP_GET_FILE_TYPE;
    return(0); /* good */
}

/* figure out what file transfer type to use
 * and set the next state to set it if it needs
 * set or else begin the transfer if it's already
 * set correctly
 */
PRIVATE int
net_get_ftp_file_type(ActiveEntry *ce)
{
	FTPConData * cd = (FTPConData *)ce->con_data;

    NET_cinfo * cinfo_struct;

	/* figure out if we are puttting a file
	 * and if we are figure out which
	 * file we are doing now 
	 */
	if(ce->URL_s->files_to_post &&
		ce->URL_s->files_to_post[0] != 0)
	  {
		/* files_to_post is an array of filename pointers
		 * find the last one in the list and
		 * remove it once done.
		 */
		char **array = ce->URL_s->files_to_post;
		int i;
		
		if(!cd->total_size_of_files_to_post)
		  {
		  	/* If we don't know the total size
			 * of all the files we are posting
			 * figure it out now
			 */
			XP_StatStruct stat_entry; 
			
			for(i=0; array[i]; i++)
			  {
				if(NET_XP_Stat(array[i], 
						   &stat_entry, xpFileToPost) != -1)
					cd->total_size_of_files_to_post += stat_entry.st_size;
			  }

			/* start the graph progress now since we know the size
		 	 */
			ce->URL_s->content_length = cd->total_size_of_files_to_post; 
#if defined(SMOOTH_PROGRESS)
            PM_Progress(ce->window_id, ce->URL_s, 0, ce->URL_s->content_length);
#else
	    	FE_GraphProgressInit(ce->window_id, ce->URL_s, ce->URL_s->content_length);
			cd->destroy_graph_progress = TRUE;
#endif
			cd->original_content_length = ce->URL_s->content_length;

#ifdef EDITOR
			/* Don't show the dialog if the data is being delivered to a plug-in */
			if (CLEAR_CACHE_BIT(ce->format_out) != FO_PLUGIN)
			{
	            FE_SaveDialogCreate(ce->window_id, i, ED_SAVE_DLG_PUBLISH );
	            cd->destroy_file_upload_progress_dialog = TRUE;
	        }
#endif /* EDITOR */
		  }  
		   		      
		/* posting files is started as a POST METHOD
		 * eventually though, after posting the files
		 * we end up with a directory listing which
		 * should be treated as a GET METHOD.  Change
		 * the method now so that the cache doesn't
		 * get confused about the method as it caches
		 * the directory listing at the end
		 */
		if(ce->URL_s->method == URL_POST_METHOD)
			ce->URL_s->method = URL_GET_METHOD;

		for(i=0; array[i] != 0; i++)
		    ; /* null body */
		
	    if(i < 1)
		  {
			/* no files left to post
			 * quit
			 */
			cd->next_state = FTP_DONE;
			return(0);
		  }

		PR_FREEIF(cd->file_to_post);
		cd->file_to_post = array[i-1];
		array[i-1] = 0;

    /* Use post_to data if specified in the URL struct. */
    if (ce->URL_s->post_to && ce->URL_s->post_to[i-1]) {
      PR_FREEIF(cd->post_file_to);
      cd->post_file_to = ce->URL_s->post_to[i-1];
      ce->URL_s->post_to[i-1] = 0;
    }


        cinfo_struct = NET_cinfo_find_type(cd->file_to_post);

        /* use binary mode always except for "text" types
		 * don't use ASCII for default types
         */
        if(!cinfo_struct->is_default 
			&& !PL_strncasecmp(cinfo_struct->type, "text",4))
          {
            if(cd->cc->cur_mode != FTP_MODE_ASCII)
                cd->next_state = FTP_SEND_ASCII;
            else
                cd->next_state = FTP_SEND_CWD;
          }
        else
          {
            if(cd->cc->cur_mode != FTP_MODE_BINARY)
                cd->next_state = FTP_SEND_BIN;
            else
                cd->next_state = FTP_SEND_CWD;
          }

        return(0);  /* continue */
	  }
	else if(ce->URL_s->method == URL_PUT_METHOD)
	  {
		cd->next_state = FTP_SEND_BIN;
		return(0);
	  }

    if(cd->type == FTP_MODE_UNKNOWN)
      {  /* use extensions to determine the type */

		NET_cinfo * enc = NET_cinfo_find_enc(cd->filename);
        cinfo_struct = NET_cinfo_find_type(cd->filename);

		TRACEMSG(("%s", cinfo_struct->is_default ?
							"Default cinfo type found" : ""));

		TRACEMSG(("Current FTP transfer mode is: %d", cd->cc->cur_mode));

        /* use binary mode always except for "text" types
		 * don't use ASCII for default types
		 *
		 * if it has an encoding value, transfer as binary
         */
        if(!cinfo_struct->is_default 
			&& !PL_strncasecmp(cinfo_struct->type, "text",4)
			&& (!enc || !enc->encoding))
          {
            if(cd->cc->cur_mode != FTP_MODE_ASCII)
                cd->next_state = FTP_SEND_ASCII;
            else
                cd->next_state = FTP_GET_FILE_SIZE;
          }
        else
          {
            if(cd->cc->cur_mode != FTP_MODE_BINARY)
                cd->next_state = FTP_SEND_BIN;
            else
                cd->next_state = FTP_GET_FILE_SIZE;
          }
      }
    else
      {  /* use the specified type */
        if(cd->cc->cur_mode == cd->type)
          {
            cd->next_state = FTP_GET_FILE_SIZE;
          }
        else
          {
            if(cd->type == FTP_MODE_ASCII)
                cd->next_state = FTP_SEND_ASCII;
            else
                cd->next_state = FTP_SEND_BIN;
          }
      }
  

    return(0);  /* continue */
}

PRIVATE int
net_send_bin(FTPConData * cd)
{

    PR_snprintf(cd->output_buffer, OUTPUT_BUFFER_SIZE, "TYPE I%c%c", CR, LF);
    cd->cc->cur_mode = FTP_MODE_BINARY;

    cd->next_state = FTP_WAIT_FOR_RESPONSE;
    cd->next_state_after_response = FTP_SEND_BIN_RESPONSE;
    cd->pause_for_read = TRUE;

	TRACEMSG(("FTP Tx: %s", cd->output_buffer));

    return((int) NET_BlockingWrite(cd->cc->csock, 
							 cd->output_buffer, 
							 PL_strlen(cd->output_buffer)));
}


PRIVATE int
net_send_ascii(FTPConData * cd)
{

    PR_snprintf(cd->output_buffer, OUTPUT_BUFFER_SIZE, "TYPE A%c%c", CR, LF);
    cd->cc->cur_mode = FTP_MODE_ASCII;

    cd->next_state = FTP_WAIT_FOR_RESPONSE;
    cd->next_state_after_response = FTP_SEND_ASCII_RESPONSE;
    cd->pause_for_read = TRUE;

	TRACEMSG(("FTP Rx: %s", cd->output_buffer));

    return((int) NET_BlockingWrite(cd->cc->csock, 
							 cd->output_buffer, 
							 PL_strlen(cd->output_buffer)));
}

PRIVATE int
net_send_bin_or_ascii_response(ActiveEntry * ce)
{
    FTPConData * cd = (FTPConData *) ce->con_data;

    if (cd->response_code != 2)
	  {
		ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_UNABLE_TO_CHANGE_FTP_MODE);
        return MK_UNABLE_TO_CHANGE_FTP_MODE;
	  }

    /* else */
	if(cd->file_to_post || ce->URL_s->method == URL_PUT_METHOD)
    	cd->next_state = FTP_SEND_CWD;
	else
    	cd->next_state = FTP_GET_FILE_SIZE;

    return(0); /* good */
}

PRIVATE int
net_get_ftp_file_size(ActiveEntry * ce)
{
    FTPConData * cd = (FTPConData *) ce->con_data;

    if (cd->cc->server_type == FTP_VMS_TYPE) 
	  {
		/* skip this since I'm not sure how it should work on VMS
		 */
		cd->next_state = FTP_GET_FILE; /* we can skip the mdtm request, too */
		return(0);
	  }

    PR_snprintf(cd->output_buffer, OUTPUT_BUFFER_SIZE, "SIZE %.1024s%c%c", cd->path, CR, LF);

    cd->next_state = FTP_WAIT_FOR_RESPONSE;
    cd->next_state_after_response = FTP_GET_FILE_SIZE_RESPONSE;
    cd->pause_for_read = TRUE;

    TRACEMSG(("FTP Rx: %s", cd->output_buffer));

    return((int) NET_BlockingWrite(cd->cc->csock,
                             cd->output_buffer,
                             PL_strlen(cd->output_buffer)));
}

PRIVATE int
net_get_ftp_file_size_response(ActiveEntry * ce)
{
    FTPConData * cd = (FTPConData *) ce->con_data;

    if(cd->response_code == 2 && cd->return_msg)
	  {
          int32 cl = atol(cd->return_msg);
		/* The response looks like  "160452"
		 * where "160452" is the size in bytes.
		 */

          if( cd->restart )
          {
              TRACEMSG(("Verifying content length: was %ld, is %ld", ce->URL_s->real_content_length, cl));
              if( ce->URL_s->real_content_length != cl )
              {
                  TRACEMSG(("Doesn't match!  Clearing restart flag."));
                  cd->restart = FALSE;
                  ce->URL_s->content_length = cl;
                  ce->URL_s->real_content_length = cl;
              }
              else
              {
                  TRACEMSG(("Length didn't change."));
              }
          }
          else
          {
              ce->URL_s->content_length = cl;
              ce->URL_s->real_content_length = cl;
              TRACEMSG(("Set content length: %ld",  ce->URL_s->content_length));
          }
	  }
	else
	  {
		/* unknown length */
      	ce->URL_s->content_length = 0;
        ce->URL_s->real_content_length = 0;
        cd->restart = FALSE;
	  }

	cd->next_state = FTP_GET_FILE_MDTM;
	
	return(0);
}

PRIVATE int
net_get_ftp_file_mdtm(ActiveEntry * ce)
{
    FTPConData * cd = (FTPConData *) ce->con_data;

    if (cd->cc->server_type == FTP_VMS_TYPE) 
	  {
		/* skip this since I'm not sure how it should work on VMS
		 */
		cd->next_state = FTP_GET_FILE;
		return(0);
	  }

    PR_snprintf(cd->output_buffer, OUTPUT_BUFFER_SIZE, "MDTM %.1024s%c%c", cd->path, CR, LF);

    cd->next_state = FTP_WAIT_FOR_RESPONSE;
    cd->next_state_after_response = FTP_GET_FILE_MDTM_RESPONSE;
    cd->pause_for_read = TRUE;

    TRACEMSG(("FTP Rx: %s", cd->output_buffer));

    return((int) NET_BlockingWrite(cd->cc->csock,
                             cd->output_buffer,
                             PL_strlen(cd->output_buffer)));
}

PRIVATE int
net_get_ftp_file_mdtm_response(ActiveEntry * ce)
{
    FTPConData * cd = (FTPConData *) ce->con_data;

    if(cd->response_code == 2 && cd->return_msg)
	{
        /* 
         * The time is returned in ISO 3307 "Representation
         * of the Time of Day" format. This format is YYYYMMDDHHmmSS or
         * YYYYMMDDHHmmSS.xxx, where
         *         YYYY    is the year
         *         MM      is the month (01-12)
         *         DD      is the day of the month (01-31)
         *         HH      is the hour of the day (00-23)
         *         mm      is the minute of the hour (00-59)
         *         SS      is the second of the hour (00-59)
         *         xxx     if present, is a fractional second and may be any length
         * Time is expressed in UTC (GMT), not local time.
         */

        PRExplodedTime ts;
        int64 s2us, st;
		PRTime t;
        time_t tt;

        TRACEMSG(("Parsing MDTM \"%.1024s\"", cd->return_msg, CR, LF));

        ts.tm_year = (cd->return_msg[0] - '0') * 1000 +
                     (cd->return_msg[1] - '0') *  100 +
                     (cd->return_msg[2] - '0') *   10 +
                     (cd->return_msg[3] - '0');

        ts.tm_month = ((cd->return_msg[4] - '0') * 10 + (cd->return_msg[5] - '0')) - 1;
        ts.tm_mday = (cd->return_msg[6] - '0') * 10 + (cd->return_msg[7] - '0');
        ts.tm_hour = (cd->return_msg[8] - '0') * 10 + (cd->return_msg[9] - '0');
        ts.tm_min  = (cd->return_msg[10] - '0') * 10 + (cd->return_msg[11] - '0');
        ts.tm_sec  = (cd->return_msg[12] - '0') * 10 + (cd->return_msg[13] - '0');
        ts.tm_usec = 0;

        /* 
         * if( '.' == cd->return_msg[14] )
         * {
         *     long int i = 1000000;
         *     char *p = &cd->return_msg[15];
         *     while( isdigit(*p) )
         *     {
         *         ts.tm_usec += (*p - '0') * i;
         *         i /= 10;
         *         p++;
         *     }
         * }
         */

        t = PR_ImplodeTime(&ts);
        LL_I2L(s2us, PR_USEC_PER_SEC);
        LL_DIV(st, t, s2us);
        LL_L2I(tt, st);

#ifdef DEBUG
        {
            char line[256];
            PRExplodedTime et;
			PRTime t2;
            LL_MUL(t2, st, s2us);
            PR_ExplodeTime(t2, PR_GMTParameters, &et);
            PR_FormatTimeUSEnglish(line, sizeof(line), "%Y%m%d%H%M%S", &et);
            TRACEMSG(("Parse check: mdtm is \"%s\"", line));
        }
#endif

        if( cd->restart && (ce->URL_s->last_modified != 0) )
        {
            TRACEMSG(("Verifying last modified date: was %ld, is %ld", 
                      ce->URL_s->last_modified, tt));
            if( ce->URL_s->last_modified != tt )
            {
                TRACEMSG(("Doesn't match!  Clearing restart flag."));
                cd->restart = FALSE;
                ce->URL_s->last_modified = tt;
            }
            else
            {
                TRACEMSG(("Time of last modification didn't change."));
            }
        }
        else
        {
            ce->URL_s->last_modified = tt;
            TRACEMSG(("set last modified: %ld", ce->URL_s->last_modified));
        }
	}

	cd->next_state = FTP_GET_FILE;
	
	return(0);
}

PRIVATE int
net_get_ftp_file(FTPConData * cd)
{
    
    /* if this is a VMS server then we need to 
     * do some special things to map a UNIX syntax
     * ftp url to a VMS type file syntax
     */
    if (cd->cc->server_type == FTP_VMS_TYPE) 
      {
          cd->restart = FALSE;
        /* If we want the VMS account's top directory speed to it 
		 */
        if (cd->path[0] == '\0' || (cd->path[0] == '/' && cd->path[1]== '\0')) 
          {
        	cd->is_directory = YES;
        	cd->next_state = FTP_SEND_LIST_OR_NLST;
          }
		else if(!PL_strchr(cd->path, '/'))
		  {
			/* if we want a file out of the top directory skip the PWD
			 */
			cd->next_state = FTP_SEND_RETR;
		  }
		else
		  {
    	    cd->next_state = FTP_SEND_CWD;
		  }
    
      }
    else
      {  /* non VMS */
		/* if we have already done the RETR or
		 * if the path ends with a slash
		 */
		if(cd->retr_already_failed ||
			'/' == cd->path[PL_strlen(cd->path)-1])
        	cd->next_state = FTP_SEND_CWD;
		else
        {
            if( cd->restart ) cd->next_state = FTP_SEND_REST;
            else              cd->next_state = FTP_SEND_RETR;
        }
      }

    return(0); /* continue */
}

PRIVATE int
net_send_cwd(FTPConData * cd)
{

    if (cd->cc->server_type == FTP_VMS_TYPE) 
      {
        char *cp, *cp1;
        /** Try to go to the appropriate directory and doctor filename **/
        if ((cp=PL_strrchr(cd->path, '/'))!=NULL) 
          {
			*cp = '\0';
        	PR_snprintf(cd->output_buffer, 
					   OUTPUT_BUFFER_SIZE, 
					   "CWD [%.1024s]" CRLF, 
					   cd->path);
			*cp = '/';  /* set it back so it is left unmodified */

			/* turn slashes into '.' */
        	while ((cp1=PL_strchr(cd->output_buffer, '/')) != NULL)
            	*cp1 = '.';
          }
        else
          {
			PR_snprintf(cd->output_buffer, 
					   OUTPUT_BUFFER_SIZE, 
					   "CWD [.%.1024s]" CRLF, 
					   cd->path);
          }
      } /* if FTP_VMS_TYPE */
    else
      {  /* non VMS server */
        PR_snprintf(cd->output_buffer, OUTPUT_BUFFER_SIZE, "CWD %.1024s" CRLF, cd->path);
      }

    cd->next_state = FTP_WAIT_FOR_RESPONSE;
    cd->next_state_after_response = FTP_SEND_CWD_RESPONSE;
    cd->pause_for_read = TRUE;

	TRACEMSG(("FTP Tx: %s", cd->output_buffer));

    return((int) NET_BlockingWrite(cd->cc->csock, 
							 cd->output_buffer, 
							 PL_strlen(cd->output_buffer)));
}

PRIVATE int
net_send_cwd_response(ActiveEntry * ce)
{
    FTPConData * cd = (FTPConData *) ce->con_data;

    if (cd->response_code == 2) /* Successed : let's NAME LIST it */
      {  

		if(cd->file_to_post)
		  {
            cd->next_state = FTP_BEGIN_PUT;
            return(0); /* good */
          }
		else if(ce->URL_s->method == URL_PUT_METHOD)
		  {
            cd->next_state = FTP_BEGIN_PUT;
            return(0); /* good */
		  }

        ce->URL_s->is_directory = TRUE;
        cd->restart = FALSE;

        if (cd->cc->server_type == FTP_VMS_TYPE)
          {
			if(*cd->filename)
                cd->next_state = FTP_SEND_RETR;
			else
			 	cd->next_state = FTP_SEND_LIST_OR_NLST;
            return(0); /* good */
          }

        /* non VMS server */
        /* we are in the correct directory and we
         * already failed on the RETR so lets 
         * LIST or NLST it
         */
        cd->next_state = FTP_SEND_LIST_OR_NLST;
        return(0); /* good */
      }

    /* else */
	ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_UNABLE_TO_LOCATE_FILE, 
													*cd->path ? cd->path : "/");

    return MK_UNABLE_TO_LOCATE_FILE;  /* error */
}

PRIVATE int
net_begin_put(ActiveEntry * ce)
{
    FTPConData * cd = (FTPConData *) ce->con_data;
	char *path, *filename;

	if(cd->file_to_post)
	  {
       /* Use post_file_to as destination filename, if supplied. */
       /* All path info in post_file_to is thrown away.  Assumed to
          be in same directory. */
       if (cd->post_file_to) {
      	  path = PL_strdup(cd->post_file_to);
 		      if(!path)
			      return(MK_OUT_OF_MEMORY);
       	  filename = PL_strrchr(path, '/');
       }
       else {
		    path = PL_strdup(cd->file_to_post);
		    if(!path)
			    return(MK_OUT_OF_MEMORY);
#if defined(XP_WIN) || defined(XP_OS2)               /* IBM - SAH */
		    filename = PL_strrchr(path, '\\');
#else
		    filename = PL_strrchr(path, '/');
#endif
       }
	  }
	else
	  {
		path = NET_ParseURL(ce->URL_s->address, GET_PATH_PART);
		if(!path)
			return(MK_OUT_OF_MEMORY);
		filename = PL_strrchr(path, '/');
	  }

	if(!filename)
		filename = path;
	else
		filename++; /* go past delimiter */

	PR_snprintf(cd->output_buffer, 
			   OUTPUT_BUFFER_SIZE, 
			   "STOR %.1024s" CRLF, 
			   filename);

#ifdef EDITOR
    if(cd->destroy_file_upload_progress_dialog)
    	FE_SaveDialogSetFilename(ce->window_id, filename);
#endif /* EDITOR */

	PR_Free(path);

    cd->next_state = FTP_WAIT_FOR_RESPONSE;
    cd->next_state_after_response = FTP_BEGIN_PUT_RESPONSE;
    cd->pause_for_read = TRUE;

	TRACEMSG(("FTP Tx: %s", cd->output_buffer));

    return((int) NET_BlockingWrite(cd->cc->csock, 
							 cd->output_buffer, 
							 PL_strlen(cd->output_buffer)));
}


PRIVATE int
net_begin_put_response(ActiveEntry * ce)
{
    FTPConData * cd = (FTPConData *) ce->con_data;

    if (cd->response_code != 1) 
	  {
	    /* failed */
		ce->URL_s->error_msg = NET_ExplainErrorDetails(
								MK_COULD_NOT_PUT_FILE, 
								cd->file_to_post ? 
										cd->file_to_post : ce->URL_s->address,
								cd->return_msg ? cd->return_msg : "");
		return(MK_COULD_NOT_PUT_FILE);
	  }

    NET_ClearReadSelect(ce->window_id, ce->socket);
    ce->socket = cd->dsock;
    ce->con_sock = cd->dsock;

#if defined(XP_WIN16) || (defined(XP_OS2) && !defined(XP_OS2_DOUGSOCK)) /* stuff not for windows since setsockopt doesnt work */
	{
	    PRSocketOptionData opt;
		  /* turn the linger off so that we don't
		   * wait but we don't abort either
		   * since aborting will lose quede data
		   */
		opt.option = PR_SockOpt_Linger;
		opt.value.linger.polarity = PR_FALSE;
		opt.value.linger.linger = PR_INTERVAL_NO_WAIT;
		PR_SetSocketOption(cd->listen_sock, &opt);
	}
#endif /* XP_WIN16 */

    /* set connect select because we are writting
     * not reading
     */
	if(cd->pasv_mode)
		NET_SetConnectSelect(ce->window_id, ce->socket);
#ifdef XP_WIN
	cd->calling_netlib_all_the_time = TRUE;
	NET_SetCallNetlibAllTheTime(ce->window_id, "mkftp");
#endif


  /* Maybe should change string if cd->post_file_to */
	PR_snprintf(cd->output_buffer,
			   OUTPUT_BUFFER_SIZE,
				XP_GetString( XP_POSTING_FILE ),
			   cd->file_to_post);

#if defined(SMOOTH_PROGRESS)
    PM_Status(ce->window_id, ce->URL_s, cd->output_buffer);
#else
	NET_Progress(ce->window_id, cd->output_buffer);
#endif
		
    if(cd->pasv_mode)
      {
        cd->next_state = FTP_DO_PUT;
      }
    else
      { /* non PASV */
        cd->next_state = FTP_WAIT_FOR_ACCEPT;
      }

	return(0);

}

/* finally send the file
 */
PRIVATE int
net_do_put(ActiveEntry * ce)
{
    FTPConData * cd = (FTPConData *) ce->con_data;

    /* Default to adding crlf if ftp ASCII mode. */
    XP_Bool add_crlf = (cd->cc->cur_mode == FTP_MODE_ASCII);


	if(!cd->write_post_data_data)
	  {
		/* first time through */

		/* if file_to_post is filled in then
		 * throw the name into the post data
		 * so that we can use the WritePostData
		 * function
		 */
		if(cd->file_to_post)
		  {
			ce->URL_s->post_data = cd->file_to_post;
			ce->URL_s->post_data_is_file = TRUE;
		  }
	  }

    /* If the add_crlf field is set in the URL struct, find the
       entry corresponding to the current file being posted. */
    if (ce->URL_s->files_to_post && ce->URL_s->add_crlf) {
      int n = 0;
      while (ce->URL_s->files_to_post[n]) {
        n++;
      }
      /* n should now point after the last entry in files_to_post,
         which is the current file being uploaded. */      
      add_crlf = ce->URL_s->add_crlf[n];
    }

    /* returns 0 on done and negative on error
     * positive if it needs to continue.
     */
    ce->status = NET_WritePostData(ce->window_id, ce->URL_s,
                                  ce->socket,
                                  &cd->write_post_data_data,
								                  add_crlf);

	cd->pause_for_read = TRUE;

    if(ce->status == 0)
      {
        /* normal done
         */
   		NET_ClearConnectSelect(ce->window_id, ce->socket);
#ifdef XP_WIN
		if(cd->calling_netlib_all_the_time)
		{
			cd->calling_netlib_all_the_time = FALSE;
			NET_ClearCallNetlibAllTheTime(ce->window_id, "mkftp");
		}
#endif
   		ce->socket = cd->cc->csock;
   		ce->con_sock = NULL;
   		NET_SetReadSelect(ce->window_id, ce->socket);

        PR_Close(cd->dsock);
        cd->dsock = NULL;

#if !defined(SMOOTH_PROGRESS)
        if(cd->destroy_graph_progress)
		  {
            FE_GraphProgressDestroy(ce->window_id, 	
									ce->URL_s, 	
									cd->original_content_length,
									ce->bytes_received);
			cd->destroy_graph_progress = FALSE;
		  }
#endif

       	/* go read the response since we need to
       	 * get the "226 sucessful transfer" message
       	 * out of the read queue in order to cache
       	 * connections properly
       	 */
       	cd->next_state = FTP_WAIT_FOR_RESPONSE;

		if(cd->file_to_post)
		  {
			ce->URL_s->post_data = 0;
			ce->URL_s->post_data_is_file = FALSE;
			PR_FREEIF(cd->file_to_post);
			cd->file_to_post = NULL;
		  }

    if (cd->post_file_to)
      {
      PR_FREEIF(cd->post_file_to);
      cd->post_file_to = NULL;
      }

		/* after getting the server response start
		 * over again if there are more
		 * files to send or to do a new directory
		 * listing...
		 */
      	cd->next_state_after_response = FTP_DONE;


		/* if this was the last file, check the history
		 * to see if the window is currently displaying an ftp
		 * listing.  If it is update the listing.  If it isnt
		 * quit.
		 */
		if(ce->URL_s->files_to_post)
		  {
			if(ce->URL_s->files_to_post[0])
			  {
				/* more files, keep going */
				cd->next_state_after_response = FTP_FIGURE_OUT_WHAT_TO_DO;
			  }
      /* Don't want to show a directory when remote editing a FTP page, bug 50942 */
			else if (!EDT_IS_EDITOR(ce->window_id))
		  	  {
				/* no more files, figure out if we should
				 * show an index
				 */
#ifdef MOZILLA_CLIENT
		  		History_entry * his = SHIST_GetCurrent(&ce->window_id->hist);
#else
            	History_entry * his = NULL;
#endif /* MOZILLA_CLIENT */

				if(his && !PL_strncasecmp(his->address, "ftp:", 4))
			  	  {
					/* go get the index of the directory */
					cd->next_state_after_response = FTP_FIGURE_OUT_WHAT_TO_DO;
			  	  }
			  }
		  }
	  }
	else if(ce->status > 0)
	  {
    	ce->bytes_received += ce->status;
#if defined(SMOOTH_PROGRESS)
        PM_Progress(ce->window_id, ce->URL_s, ce->bytes_received, ce->URL_s->content_length);
#else
    	FE_GraphProgress(ce->window_id,
                     	ce->URL_s,
                     	ce->bytes_received,
                     	ce->status,
                     	ce->URL_s->content_length);
    	FE_SetProgressBarPercent(ce->window_id,
                             	(long)(((double)ce->bytes_received*100) / (double)(uint32)ce->URL_s->content_length) );
#endif
	  }

	return(ce->status);
}

PRIVATE int
net_send_rest(ActiveEntry *ce)
{
    FTPConData * cd = (FTPConData *) ce->con_data;

    PR_snprintf(cd->output_buffer, OUTPUT_BUFFER_SIZE, "REST %ld%c%c", 
                ce->URL_s->content_length, CR, LF);

    cd->next_state = FTP_WAIT_FOR_RESPONSE;
    cd->next_state_after_response = FTP_SEND_REST_RESPONSE;
    cd->pause_for_read = TRUE;

    TRACEMSG(("FTP Rx: %s", cd->output_buffer));

    return((int) NET_BlockingWrite(cd->cc->csock,
                             cd->output_buffer,
                             PL_strlen(cd->output_buffer)));
}

PRIVATE int
net_send_rest_response(ActiveEntry *ce)
{
    FTPConData * cd = (FTPConData *) ce->con_data;

    if(cd->response_code == 3 && cd->return_msg)
	{
        TRACEMSG(("Restarting!  Yay!"));
    }
    else
    {
        TRACEMSG(("Didn't understand the REST command."));
        cd->cc->no_restart = TRUE;
        cd->restart = FALSE;
    }

    cd->next_state = FTP_SEND_RETR;
    return(0);
}


PRIVATE int
net_send_retr(FTPConData * cd)
{

    if(cd->cc->server_type == FTP_VMS_TYPE)
        PR_snprintf(cd->output_buffer, 
				   OUTPUT_BUFFER_SIZE,
				   "RETR %.1024s" CRLF, 
				   cd->filename);
    else
        PR_snprintf(cd->output_buffer, 
					OUTPUT_BUFFER_SIZE, 
					"RETR %.1024s" CRLF, 
					cd->path);

    cd->next_state = FTP_WAIT_FOR_RESPONSE;
    cd->next_state_after_response = FTP_SEND_RETR_RESPONSE;
    cd->pause_for_read = TRUE;

	TRACEMSG(("FTP Tx: %s", cd->output_buffer));

    return((int) NET_BlockingWrite(cd->cc->csock, 
							 cd->output_buffer, 
							 PL_strlen(cd->output_buffer)));
}


PRIVATE int
net_send_retr_response(ActiveEntry * ce)
{
    FTPConData * cd = (FTPConData *) ce->con_data;

    if(cd->response_code != 1) 
      {
        if(cd->cc->server_type == FTP_VMS_TYPE) 
          {
	        ce->URL_s->error_msg = NET_ExplainErrorDetails(
												  MK_UNABLE_TO_LOCATE_FILE, 
												  *cd->path ? cd->path : "/");
            return(MK_UNABLE_TO_LOCATE_FILE);
          }
        else
          { /* non VMS */

#ifdef DOUBLE_PASV 
			/* this will use a new PASV connection
			 * for each command even if the one only
			 * failed and didn't transmit
			 */
			/* close the old pasv connection */
			NETCLOSE(cd->dsock);
			/* invalidate the old pasv connection */
			cd->dsock = NULL;

			cd->next_state = FTP_SEND_PASV; /* send it again */
			cd->retr_already_failed = TRUE;
#else
			/* we need to use this to make the mac FTPD work
			 */
			/* old way */
            cd->next_state = FTP_SEND_CWD; 
#endif /* DOUBLE_PASV */
          }
      }
    else
      {   /* successful RETR */
        cd->next_state = FTP_SETUP_STREAM;
      }

    return(0); /* continue */
}

PRIVATE int
net_send_list_or_nlst(ActiveEntry * ce)
{
    FTPConData * cd = (FTPConData *)ce->con_data;

    if(cd->cc->use_list)
        PL_strcpy(cd->output_buffer, "LIST" CRLF);
    else
        PL_strcpy(cd->output_buffer, "NLST" CRLF);

    cd->next_state = FTP_WAIT_FOR_RESPONSE;
    cd->next_state_after_response = FTP_SEND_LIST_OR_NLST_RESPONSE;
    cd->pause_for_read = TRUE;

	TRACEMSG(("FTP Tx: %s", cd->output_buffer));

    return((int) NET_BlockingWrite(cd->cc->csock, 
							 cd->output_buffer, 
							 PL_strlen(cd->output_buffer)));
}

PRIVATE int
net_send_list_or_nlst_response(ActiveEntry * ce)
{
    FTPConData * cd = (FTPConData *)ce->con_data;

    if(cd->response_code == 1)
      {  /* succesful list or nlst */
    	cd->is_directory = TRUE;
        cd->restart = FALSE;
    	cd->next_state = FTP_SETUP_STREAM;

		/* add a slash to the end of the uRL if it doesnt' have one now
		 */
		if(ce->URL_s->address[PL_strlen(ce->URL_s->address)-1] != '/')
		  {
			/* update the global history before modification
			 */
#ifdef MOZILLA_CLIENT
			GH_UpdateGlobalHistory(ce->URL_s);
#endif /* MOZILLA_CLIENT */
			StrAllocCat(ce->URL_s->address, "/");
			ce->URL_s->address_modified = TRUE;
		  }

    	return(0); /* good */
      }
  
	ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_UNABLE_TO_LOCATE_FILE, 
												  *cd->path ? cd->path : "/");
    return(MK_UNABLE_TO_LOCATE_FILE);
}

    
PRIVATE int
net_setup_ftp_stream(ActiveEntry * ce)
{
    FTPConData * cd = (FTPConData *)ce->con_data;

    /* set up the data stream now
     */
    if(cd->is_directory)
      {
 		char * pUrl ;
		int status;
 
 		status = PREF_CopyCharPref("protocol.mimefor.ftp",&pUrl);
 
 		if (status >= 0 && pUrl) {
 			StrAllocCopy(ce->URL_s->content_type, pUrl);
 			PR_Free(pUrl);
 		} else {
 	 		StrAllocCopy(ce->URL_s->content_type, APPLICATION_HTTP_INDEX);
 		}

        cd->stream = net_ftp_make_stream(ce->format_out, ce->URL_s, ce->window_id);
      }
    else
      {
		if(!ce->URL_s->preset_content_type || !ce->URL_s->content_type )
		  {
			NET_cinfo * cinfo_struct = NET_cinfo_find_type(cd->filename);
	 		StrAllocCopy(ce->URL_s->content_type, cinfo_struct->type);

			cinfo_struct = NET_cinfo_find_enc(cd->filename);
	 		StrAllocCopy(ce->URL_s->content_encoding, cinfo_struct->encoding);
		  }

        cd->stream = net_ftp_make_stream(ce->format_out, ce->URL_s, ce->window_id);
      }

    if (!cd->stream)
      {
        cd->next_state = FTP_ERROR_DONE;
	    ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_UNABLE_TO_CONVERT); 
        return(MK_UNABLE_TO_CONVERT);
      }

HG69136

    if(cd->pasv_mode)
      {
        if(cd->is_directory)
           cd->next_state = FTP_START_READ_DIRECTORY;
        else
           cd->next_state = FTP_START_READ_FILE;
      }
    else
      { /* non PASV */
        cd->next_state = FTP_WAIT_FOR_ACCEPT;
      }

	/* start the graph progress indicator
     */
#if defined(SMOOTH_PROGRESS)
    PM_Progress(ce->window_id, ce->URL_s, 0, ce->URL_s->real_content_length);
#else
    FE_GraphProgressInit(ce->window_id, ce->URL_s, ce->URL_s->real_content_length);
	cd->destroy_graph_progress = TRUE;
#endif
	cd->original_content_length = ce->URL_s->real_content_length;

    return(0); /* continue */
}
    
PRIVATE int
net_wait_for_ftp_accept(ActiveEntry * ce)
{
    FTPConData * cd = (FTPConData *)ce->con_data;
	PRNetAddr pr_addr;

/* make this non-blocking sometime */

    /* Wait for the connection
     */
#define ACCEPT_TIMEOUT 20
    cd->dsock = PR_Accept(cd->listen_sock, &pr_addr, ACCEPT_TIMEOUT);
   
    if (cd->dsock == NULL)
      {
		if(PR_GetError() == PR_WOULD_BLOCK_ERROR)
		  {
			cd->pause_for_read = TRUE; /* pause */
    		NET_ClearReadSelect(ce->window_id, ce->socket);
    		ce->socket = cd->listen_sock;
    		NET_SetReadSelect(ce->window_id, ce->socket);
			return(0);
		  }
		else
		  {
	        ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_UNABLE_TO_ACCEPT_SOCKET); 
            return MK_UNABLE_TO_ACCEPT_SOCKET;
		  }
      }

    TRACEMSG(("FTP: Accepted new socket %d\n", cd->dsock));

	if(cd->file_to_post || ce->URL_s->method == URL_POST_METHOD)
	  {
		ce->socket = cd->dsock;
		NET_SetConnectSelect(ce->window_id, ce->socket);
		cd->next_state = FTP_DO_PUT;
	  }
    else if (cd->is_directory)
      {
        cd->next_state = FTP_START_READ_DIRECTORY;
      }
    else
      {
        cd->next_state = FTP_START_READ_FILE;
      }

    return(0);
}

PRIVATE int
net_ftp_push_partial_cache_file(ActiveEntry * ce)
{
    FTPConData * cd = (FTPConData *)ce->con_data;

    int32 write_ready, status;

    write_ready = (*cd->stream->is_write_ready)(cd->stream);

    if( 0 == write_ready )
      {
        cd->pause_for_read = TRUE;
        return(0);  /* wait until we are ready to write */
      }

    write_ready = MIN(write_ready, NET_Socket_Buffer_Size);

    status = NET_XP_FileRead(NET_Socket_Buffer, write_ready, cd->partial_cache_fp);

    if( status < 0 )
    {
        ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_TCP_READ_ERROR,
                                                       PR_GetOSError());
        return MK_TCP_READ_ERROR;
    }
    else if( status == 0 )
    {
        /* All done, switch over to incoming data */
        TRACEMSG(("Closing FTP cache file"));
#if defined(SMOOTH_PROGRESS)
        PM_Status(ce->window_id, ce->URL_s, XP_GetString(XP_PROGRESS_FILEDONE));
#else
        NET_Progress(ce->window_id, XP_GetString(XP_PROGRESS_FILEDONE));
#endif
        NET_ClearFileReadSelect(ce->window_id, XP_Fileno(cd->partial_cache_fp));
        NET_XP_FileClose(cd->partial_cache_fp);
        cd->partial_cache_fp = NULL;
        ce->socket = cd->dsock;
        NET_SetReadSelect(ce->window_id, ce->socket);
        ce->local_file = FALSE;
        cd->next_state = FTP_READ_FILE;
#if defined(SMOOTH_PROGRESS)
        PM_Status(ce->window_id, ce->URL_s, XP_GetString(XP_PROGRESS_RECEIVE_FTPFILE));
#else
        NET_Progress (ce->window_id, XP_GetString(XP_PROGRESS_RECEIVE_FTPFILE));
#endif
        return 0;
    }
    else
    {
        /* write the data */
        TRACEMSG(("Transferring %ld bytes from cache file.", status));
        ce->bytes_received += status;
#if defined(SMOOTH_PROGRESS)
        PM_Progress(ce->window_id, ce->URL_s, ce->bytes_received, ce->URL_s->real_content_length);
#else
        FE_GraphProgress(ce->window_id, ce->URL_s, ce->bytes_received, status, ce->URL_s->real_content_length);
        FE_SetProgressBarPercent(ce->window_id, (long)(((double)ce->bytes_received*100) / (double)(uint32)ce->URL_s->real_content_length) );
#endif
        status = (*cd->stream->put_block)(cd->stream, NET_Socket_Buffer, status);
        cd->pause_for_read = TRUE;
        return status;
    }
}

PRIVATE int
net_start_ftp_read_file(ActiveEntry * ce)
{
    FTPConData * cd = (FTPConData *)ce->con_data;

    if( cd->restart )
    {
        char *cache_file = ce->URL_s->cache_file;
        XP_File fp;

        TRACEMSG(("Opening FTP cache file"));

        if( (NULL == cache_file) || (NULL == (fp = NET_XP_FileOpen(cache_file, xpCache, XP_FILE_READ_BIN))) )
        {
            ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_UNABLE_TO_OPEN_FILE, cache_file);
            return MK_UNABLE_TO_OPEN_FILE;
        }

        NET_ClearReadSelect(ce->window_id, ce->socket);
        /* ce->socket = XP_Fileno(fp); */
		ce->socket = NULL;
        NET_SetFileReadSelect(ce->window_id, XP_Fileno(fp));
        ce->local_file = TRUE;
        cd->next_state = FTP_READ_CACHE;
        cd->partial_cache_fp = fp;
#if defined(SMOOTH_PROGRESS)
        PM_Status(ce->window_id, ce->URL_s, XP_GetString(XP_PROGRESS_READFILE));
#else
        NET_Progress(ce->window_id, XP_GetString(XP_PROGRESS_READFILE));
#endif
        return net_ftp_push_partial_cache_file(ce);
    }

    /* nothing much to be done */
    cd->next_state = FTP_READ_FILE;
    cd->pause_for_read = TRUE;

    NET_ClearReadSelect(ce->window_id, ce->socket);
    ce->socket = cd->dsock;
    NET_SetReadSelect(ce->window_id, ce->socket);

#if defined(SMOOTH_PROGRESS)
    PM_Status(ce->window_id, ce->URL_s, XP_GetString(XP_PROGRESS_RECEIVE_FTPFILE));
#else
    NET_Progress (ce->window_id, XP_GetString(XP_PROGRESS_RECEIVE_FTPFILE));
#endif

    return(0); /* continue */
}

PRIVATE int
net_ftp_read_file(ActiveEntry * ce) 
{ 
	FTPConData * cd = (FTPConData *)ce->con_data; 
	int status; 
    unsigned int write_ready, read_size;

    /* check to see if the stream is ready for writing
     */
    write_ready = (*cd->stream->is_write_ready)(cd->stream);

    if(!write_ready)
      {
        cd->pause_for_read = TRUE;
        return(0);  /* wait until we are ready to write */
      }
    else if(write_ready < (unsigned int) NET_Socket_Buffer_Size)
      {
        read_size = write_ready;
      }
    else
      {
        read_size = NET_Socket_Buffer_Size;
      }

	status = PR_Read(cd->dsock, NET_Socket_Buffer, read_size); 

	if(status > 0) 
	  { 
		ce->bytes_received += status; 
#if defined(SMOOTH_PROGRESS)
        PM_Progress(ce->window_id, ce->URL_s, ce->bytes_received, ce->URL_s->real_content_length);
#else
		FE_GraphProgress(ce->window_id, ce->URL_s, ce->bytes_received, status, ce->URL_s->real_content_length);
		if (ce->URL_s->real_content_length == 0)
			FE_SetProgressBarPercent(ce->window_id, 0);
		else
    		FE_SetProgressBarPercent(ce->window_id, (long)(((double)ce->bytes_received*100) / (double)(uint32)ce->URL_s->real_content_length) );
#endif
        status = PUTBLOCK(NET_Socket_Buffer, status);
        cd->pause_for_read = TRUE;
      }
    else if(status == 0)
      {
        /* go read the response since we need to
         * get the "226 sucessful transfer" message
         * out of the read queue in order to cache
         * connections properly
         */
        cd->next_state = FTP_WAIT_FOR_RESPONSE;
        cd->next_state_after_response = FTP_DONE;

		if(cd->dsock != NULL)
	      {
		    NET_ClearReadSelect(ce->window_id, cd->dsock);
            PR_Close(cd->dsock);
            cd->dsock = NULL;
		  }

		/* set select on the control socket */
    	ce->socket = cd->cc->csock;
    	NET_SetReadSelect(ce->window_id, ce->socket);
        /* try and read the response immediately,
         * so don't pause for read
         *
         * cd->pause_for_read = TRUE;
         */

        return(MK_DATA_LOADED);
      }
	else
	  {
		/* status less than zero
	     */
	      int rv = PR_GetError();

		if (rv == PR_WOULD_BLOCK_ERROR)
		  {
			cd->pause_for_read = TRUE;
			return 0;
		  }
	      status = (rv < 0) ? rv : (-rv);
	  }

   return(status);
}

PRIVATE int
net_start_ftp_read_dir(ActiveEntry * ce)
{
    FTPConData * cd = (FTPConData *)ce->con_data;
	char buf[512];
	char *line;

    cd->sort_base = NET_SortInit();

#if defined(SMOOTH_PROGRESS)
    PM_Status(ce->window_id, ce->URL_s, XP_GetString(XP_PROGRESS_RECEIVE_FTPDIR));
#else
    NET_Progress (ce->window_id, XP_GetString(XP_PROGRESS_RECEIVE_FTPDIR));
#endif

    if (*cd->path != '\0')  /* Not Empty path: */
      {
        int end;

        end = PL_strlen(cd->path)-1;
        /* if the path ends with a slash kill it.
         * that includes the path "/"
         */
        if(cd->path[end] == '/')
          {
             cd->path[end] = 0; /* kill the last slash */
          }
      }

    if(cd->login_message_buffer) /* Don't check !(*cd->path) here or this
									message never shows up.  It's supposed to
									be shown the first time you connect to
									this server, whether you started at the
									root or not. */
      {
	
		/* print it out like so:  101: message line 1
		 *                        101: message line 2, etc...
	 	 */
		line = strtok(cd->login_message_buffer, "\n");
		while(ce->status >= 0 && line)
		{
			PL_strcpy(buf, "101: ");
			PL_strcatn (buf, sizeof(buf)-8, line);
			PL_strcat (buf, CRLF);

			ce->status = PUTSTRING(buf);

			line = strtok(NULL, "\n");
		}

		/* if there is also a cwd message add a blank line */
    	if(ce->status >= 0 && cd->cwd_message_buffer)
		{
			PL_strcpy(buf, "101: "CRLF);
			ce->status = PUTSTRING(buf);
		}
	}

    if(cd->cwd_message_buffer)
    {
		/* print it out like so:  101: message line 1
		 *                        101: message line 2, etc...
	 	 */
		char *end_line = PL_strchr(cd->cwd_message_buffer, '\n');
		if(end_line)
			*end_line = '\0';
		line = cd->cwd_message_buffer;

		while(ce->status >= 0 && line)
		{
			PL_strcpy(buf, "101: ");
			PL_strcatn (buf, sizeof(buf)-8, line);
			PL_strcat (buf, CRLF);

			ce->status = PUTSTRING(buf);

			if(end_line)
			{
				line = end_line+1;
				end_line = PL_strchr(line, '\n');
				if(end_line)
					*end_line = '\0';
			}
			else
			{
				line = NULL;
			}
		} 
    }

    /* clear the buffer so we can use it on the data sock */
    PR_FREEIF(cd->data_buf);
    cd->data_buf = NULL;
    cd->data_buf_size = 0;

    cd->next_state = FTP_READ_DIRECTORY;

    /* clear the select on the control socket
     */
    NET_ClearReadSelect(ce->window_id, ce->socket);
    ce->socket = cd->dsock;
    NET_SetReadSelect(ce->window_id, ce->socket);

    return(0); /* continue */
}

PRIVATE int
net_ftp_read_dir(ActiveEntry * ce)
{
   
    char * line;
    NET_FileEntryInfo * entry_info;
    FTPConData * cd = (FTPConData *)ce->con_data;

    ce->status = NET_BufferedReadLine(cd->dsock, &line, &cd->data_buf, 
                    &cd->data_buf_size, &cd->pause_for_read);

    if(ce->status == 0)
      {
		cd->next_state = FTP_PRINT_DIR;
#ifdef	XP_MAC
                cd->pause_for_read = FALSE;		/* rjc: fix FTP for Mac */
#endif
        return(ce->status);
      }
    else if(ce->status < 0)
	  {
		if(cd->stream && (*cd->stream->is_write_ready)(cd->stream))
			NET_PrintDirectory(&cd->sort_base, cd->stream, cd->path, ce->URL_s);

		ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_TCP_READ_ERROR, ce->status);

        /* return TCP error
         */
        return MK_TCP_READ_ERROR;
	  }

    /* not exactly right but really close
	 */
    if(ce->status > 1)
      {
    	ce->bytes_received += ce->status;
#if defined(SMOOTH_PROGRESS)
        PM_Progress(ce->window_id, ce->URL_s, ce->bytes_received, ce->URL_s->content_length);
#else
		FE_GraphProgress(ce->window_id, ce->URL_s, ce->bytes_received, ce->status, ce->URL_s->content_length);
#endif
	  }

    if(!line)
        return(0); /* no line ready */

    TRACEMSG(("MKFTP: Line in %s is %s\n", cd->path, line));

    entry_info = net_parse_dir_entry(line, cd->cc->server_type);

    if(!entry_info)
	  {
	    ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_OUT_OF_MEMORY); 
    	return(MK_OUT_OF_MEMORY);
	  }

    if(entry_info->display)
      {
        TRACEMSG(("Adding file to sort list: %s\n", entry_info->filename));
        NET_SortAdd(cd->sort_base, (void *)entry_info);
      }
    else
      {
        NET_FreeEntryInfoStruct(entry_info);
      }

    return(ce->status);
}

PRIVATE int
net_ftp_print_dir(ActiveEntry * ce)
{
    FTPConData * cd = (FTPConData *)ce->con_data;

    if(!cd->calling_netlib_all_the_time)
    {

        cd->calling_netlib_all_the_time = TRUE;
	    NET_SetCallNetlibAllTheTime(ce->window_id, "mkftp");
    }

	if(cd->stream 
	   && (ce->status = (*cd->stream->is_write_ready)(cd->stream)) != 0)
	{
    	ce->status = NET_PrintDirectory(&cd->sort_base, cd->stream, cd->path, ce->URL_s);

        /* go read the response since we need to
         * get the "226 sucessful transfer" message
         * out of the read queue in order to cache
         * connections properly
         */
        cd->next_state = FTP_WAIT_FOR_RESPONSE;
        cd->next_state_after_response = FTP_DONE;
          
        /* clear select on the data sock and set select on the control sock
         */
        if(cd->dsock != NULL)
          {
            NET_ClearReadSelect(ce->window_id, cd->dsock);
            PR_Close(cd->dsock);
            cd->dsock = NULL;
          }
        
        ce->socket = cd->cc->csock;
        NET_SetReadSelect(ce->window_id, cd->cc->csock);

		return(ce->status);
	  }
	else
	  {
		/* come back into this state and try again later */
		/* @@@ busy wait */
		cd->pause_for_read = TRUE;
		return(0);
	  }
}

/*
 * is_ls_date() --
 *      Return 1 if the ptr points to a string of the form:
 *              "Sep  1  1990 " or
 *              "Sep 11 11:59 " or
 *              "Dec 12 1989  " or
 *              "FCv 23 1990  " ...
 */
PRIVATE Bool 
net_is_ls_date (char *s)
{
    /* must start with three alpha characters */
    if (!isalpha(*s++) || !isalpha(*s++) || !isalpha(*s++))
        return FALSE;

    /* space */
    if (*s++ != ' ')
        return FALSE;

    /* space or digit */
    if ((*s != ' ') && !isdigit(*s))
        return FALSE;
    s++;

    /* digit */
    if (!isdigit(*s++))
        return FALSE;

    /* space */
    if (*s++ != ' ')
        return FALSE;

    /* space or digit */
    if ((*s != ' ') && !isdigit(*s))
        return FALSE;
    s++;

    /* digit */
    if (!isdigit(*s++))
        return FALSE;

    /* colon or digit */
    if ((*s != ':') && !isdigit(*s))
        return FALSE;
    s++;

    /* digit */
    if (!isdigit(*s++))
        return FALSE;

    /* space or digit */
    if ((*s != ' ') && !isdigit(*s))
        return FALSE;
    s++;

    /* space */
    if (*s++ != ' ')
        return FALSE;

    return TRUE;
} /* is_ls_date() */

/*       
 *  Converts a date string from 'ls -l' to a time_t number
 *
 *              "Sep  1  1990 " or
 *              "Sep 11 11:59 " or
 *              "Dec 12 1989  " or
 *              "FCv 23 1990  " ...
 *
 *  Returns 0 on error.
 */
PRIVATE time_t net_convert_unix_date (char * datestr)
{
    struct tm *time_info;         /* Points to static tm structure */
    char *bcol = datestr;         /* Column begin */
    char *ecol;                   /* Column end */
    long tval;
    int cnt;
    time_t curtime = time(NULL);

    if ((time_info = gmtime(&curtime)) == NULL) 
      {
    	return (time_t) 0;
      }

    time_info->tm_isdst = -1;                 /* Disable summer time */
    for (cnt=0; cnt<3; cnt++)                 /* Month */
    	*bcol++ = toupper(*bcol);

    if ((time_info->tm_mon = NET_MonthNo(datestr)) < 0)
    	return (time_t) 0;

    ecol = bcol;                        /* Day */
    while (*ecol++ == ' ') ;            /* Spool to other side of day */
    while (*ecol++ != ' ') ;
    *--ecol = '\0';

    time_info->tm_mday = atoi(bcol);
    time_info->tm_wday = 0;
    time_info->tm_yday = 0;
    bcol = ++ecol;                                   /* Year */
    if ((ecol = PL_strchr(bcol, ':')) == NULL) 
      {
    	time_info->tm_year = atoi(bcol)-1900;
    	time_info->tm_sec = 0;
    	time_info->tm_min = 0;
    	time_info->tm_hour = 0;
      } 
	else 
      {                                 /* Time */
    	/* If the time is given as hh:mm, then the file is less than 1 year
         *	old, but we might shift calandar year. This is avoided by checking
         *	if the date parsed is future or not. 
		 */
    	*ecol = '\0';
    	time_info->tm_sec = 0;
    	time_info->tm_min = atoi(++ecol);           /* Right side of ':' */
    	time_info->tm_hour = atoi(bcol);         /* Left side of ':' */
    	if (mktime(time_info) > curtime)
        	--time_info->tm_year;
      }
    return ((tval = mktime(time_info)) == -1 ? (time_t) 0 : tval);
}

/* a dos date/time string looks like this
 * 04-06-95  02:03PM 
 * 07-13-95  11:39AM 
 */
PRIVATE time_t 
net_parse_dos_date_time (char * datestr)
{
    struct tm *time_info;         /* Points to static tm structure */
    long tval;
    time_t curtime = time(NULL);

    if ((time_info = gmtime(&curtime)) == NULL) 
      {
    	return (time_t) 0;
      }

    time_info->tm_isdst = -1;                 /* Disable summer time */

    time_info->tm_mon = (datestr[1]-'0')-1;

    time_info->tm_mday = (((datestr[3]-'0')*10) + datestr[4]-'0');
    time_info->tm_year = (((datestr[6]-'0')*10) + datestr[7]-'0');
    time_info->tm_hour = (((datestr[10]-'0')*10) + datestr[11]-'0');
	if(datestr[15] == 'P')
    	time_info->tm_hour += 12;
    time_info->tm_min = (((datestr[13]-'0')*10) + datestr[14]-'0');

    time_info->tm_wday = 0;
    time_info->tm_yday = 0;
    time_info->tm_sec = 0;

    return ((tval = mktime(time_info)) == -1 ? (time_t) 0 : tval);
}

/*         
 *  Converts a date string from vms to a time_t number
 *  This is needed in order to put out the date using the same format
 *  for all directory listings.
 *
 *  Returns 0 on error
 */
PRIVATE time_t net_convert_vms_date (char * datestr)
{
    struct tm *time_info;           /* Points to static tm structure */
    char *col;
    long tval;
    time_t curtime = time(NULL);

    if ((time_info = gmtime(&curtime)) == NULL)
    	return (time_t) 0;

    time_info->tm_isdst = -1;                 /* Disable summer time */

    if ((col = strtok(datestr, "-")) == NULL)
    	return (time_t) 0;

    time_info->tm_mday = atoi(col);                   /* Day */
    time_info->tm_wday = 0;
    time_info->tm_yday = 0;

    if ((col = strtok(NULL, "-")) == NULL || (time_info->tm_mon = NET_MonthNo(col)) < 0)
    	return (time_t) 0;

    if ((col = strtok(NULL, " ")) == NULL)               /* Year */
    	return (time_t) 0;

    time_info->tm_year = atoi(col)-1900;

    if ((col = strtok(NULL, ":")) == NULL)               /* Hour */
    	return (time_t) 0;

    time_info->tm_hour = atoi(col);

    if ((col = strtok(NULL, " ")) == NULL)               /* Mins */
    	return (time_t) 0;

    time_info->tm_min = atoi(col);
    time_info->tm_sec = 0;

    return ((tval = mktime(time_info)) < 0 ? (time_t) 0 : tval);
}


/*
 * parse_ls_line() --
 *      Extract the name, size, and date from an ls -l line.
 *
 * ls -l listing
 *
 * drwxr-xr-x    2 montulli eng          512 Nov  8 23:23 CVS
 * -rw-r--r--    1 montulli eng         2244 Nov  8 23:23 Imakefile
 * -rw-r--r--    1 montulli eng        14615 Nov  9 17:03 Makefile
 *
 * or a dl listing
 *
 * cs.news/              =  ICSI Computing Systems Newsletter
 * dash/                 -  DASH group documents and software
 * elisp/                -  Emacs lisp code
 * 
 */
PRIVATE void 
net_parse_ls_line (char *line, NET_FileEntryInfo *entry_info)
{
    int32   base=1;
	int32   size_num=0;
	char    save_char;
	char   *ptr;

    for (ptr = &line[PL_strlen(line) - 1];
            (ptr > line+13) && (!NET_IS_SPACE(*ptr) || !net_is_ls_date(ptr-12)); ptr--)
                ; /* null body */
	save_char = *ptr;
    *ptr = '\0';
    if (ptr > line+13) 
	  {
        entry_info->date = net_convert_unix_date(ptr-12);
	  }
	else
	  {
	    /* must be a dl listing
	     */
		/* unterminate the line */
		*ptr = save_char;
		/* find the first whitespace and  terminate
		 */
		for(ptr=line; *ptr != '\0'; ptr++)
			if(NET_IS_SPACE(*ptr))
			  {
				*ptr = '\0';
				break;
			  }
        entry_info->filename = NET_Escape(line, URL_PATH);
	
		return;
	  }

    /* escape and copy
     */
    entry_info->filename = NET_Escape(ptr+1, URL_PATH);

	/* parse size
	 */
	if(ptr > line+15)
	  {
        ptr -= 14;
        while (isdigit(*ptr))
          {
            size_num += ((int32) (*ptr - '0')) * base;
            base *= 10;
            ptr--;
          }
    
        entry_info->size = size_num;
	  }
    
} /* parse_ls_line() */

/*
 * net_parse_vms_dir_entry()
 *      Format the name, date, and size from a VMS LIST line
 *      into the EntryInfo structure
 */
PRIVATE void 
net_parse_vms_dir_entry (char *line, NET_FileEntryInfo *entry_info)
{
        int i, j;
        int32 ialloc;
        char *cp, *cpd, *cps, date[64], *sp = " ";
        time_t NowTime;
        static char ThisYear[32];
        static Bool HaveYear = FALSE; 

        /**  Get rid of blank lines, and information lines.  **/
        /**  Valid lines have the ';' version number token.  **/
        if (!PL_strlen(line) || (cp=PL_strchr(line, ';')) == NULL) 
		  {
            entry_info->display = FALSE;
            return;
          }

        /** Cut out file or directory name at VMS version number. **/
    	*cp++ ='\0';
		/* escape and copy
	 	 */
		entry_info->filename = NET_Escape(line, URL_PATH);

        /** Cast VMS file and directory names to lowercase. **/
    	for (i=0; entry_info->filename[i]; i++)
            entry_info->filename[i] = tolower(entry_info->filename[i]);

        /** Uppercase terminal .z's or _z's. **/
    	if ((--i > 2) && entry_info->filename[i] == 'z' &&
             	(entry_info->filename[i-1] == '.' || entry_info->filename[i-1] == '_'))
            entry_info->filename[i] = 'Z';

        /** Convert any tabs in rest of line to spaces. **/
    	cps = cp-1;
        while ((cps=PL_strchr(cps+1, '\t')) != NULL)
            *cps = ' ';

        /** Collapse serial spaces. **/
        i = 0; j = 1;
    	cps = cp;
        while (cps[j] != '\0') 
		  {
            if (cps[i] == ' ' && cps[j] == ' ')
                j++;
            else
                cps[++i] = cps[j++];
		  }
        cps[++i] = '\0';

        /* Save the current year.       
    	 * It could be wrong on New Year's Eve.
		 */
    	if (!HaveYear) 
	 	  {
        	NowTime = time(NULL);
        	PL_strcpy(ThisYear, (char *)ctime(&NowTime)+20);
        	ThisYear[4] = '\0';
        	HaveYear = TRUE;
    	  }

        /* get the date. 
		 */
        if ((cpd=PL_strchr(cp, '-')) != NULL &&
                PL_strlen(cpd) > 9 && isdigit(*(cpd-1)) &&
                isalpha(*(cpd+1)) && *(cpd+4) == '-') 
		  {

            /* Month 
			 */
            *(cpd+4) = '\0';
            *(cpd+2) = tolower(*(cpd+2));
            *(cpd+3) = tolower(*(cpd+3));
            sprintf(date, "%.32s ", cpd+1);
            *(cpd+4) = '-';

            /** Day **/
            *cpd = '\0';
            if (isdigit(*(cpd-2)))
                sprintf(date+4, "%.32s ", cpd-2);
            else
                sprintf(date+4, " %.32s ", cpd-1);
            *cpd = '-';

            /** Time or Year **/
            if (!PL_strncmp(ThisYear, cpd+5, 4) && PL_strlen(cpd) > 15 && *(cpd+12) == ':')
			  {
                *(cpd+15) = '\0';
                sprintf(date+7, "%.32s", cpd+10);
                *(cpd+15) = ' ';
              } 
			else 
			  {
                *(cpd+9) = '\0';
                sprintf(date+7, " %.32s", cpd+5);
                *(cpd+9) = ' ';
              }

            entry_info->date = net_convert_vms_date(date);
          }

        /* get the size 
		 */
        if ((cpd=PL_strchr(cp, '/')) != NULL) 
		  {
            /* Appears be in used/allocated format */
            cps = cpd;
            while (isdigit(*(cps-1)))
                cps--;
            if (cps < cpd)
                *cpd = '\0';
            entry_info->size = atol(cps);
            cps = cpd+1;
            while (isdigit(*cps))
                cps++;
            *cps = '\0';
            ialloc = atoi(cpd+1);
            /* Check if used is in blocks or bytes */
            if (entry_info->size <= ialloc)
                entry_info->size *= 512;
          }
        else if ((cps=strtok(cp, sp)) != NULL) 
		  {
            /* We just initialized on the version number 
             * Now let's find a lone, size number   
			 */
            while ((cps=strtok(NULL, sp)) != NULL) 
			  {
                 cpd = cps;
                 while (isdigit(*cpd))
                     cpd++;
                 if (*cpd == '\0') 
				   {
                     /* Assume it's blocks */
                     entry_info->size = atol(cps) * 512;
                     break;
                   }
               }
          }

        /** Wrap it up **/
        TRACEMSG(("MKFTP: VMS filename: %s  date: %d  size: %ld\n",
                         entry_info->filename, entry_info->date, entry_info->size));
        return;

} /* net_parse_vms_dir_entry() */

/*
 *     parse_dir_entry() 
 *      Given a line of LIST/NLST output in entry, return results 
 *      and a file/dir name in entry_info struct
 *
 */
PRIVATE NET_FileEntryInfo * 
net_parse_dir_entry (char *entry, int server_type)
{
        NET_FileEntryInfo *entry_info;
        int  i;
        int  len;
        Bool remove_size=FALSE;

        entry_info = NET_CreateFileEntryInfoStruct();
        if(!entry_info)
            return(NULL);

        entry_info->display = TRUE;

		/* do special case for ambiguous NT servers
		 *
		 * ftp.microsoft.com is an NT server with UNIX ls -l
		 * syntax.  But most NT servers use the DOS dir syntax.
		 * If there is a space at position 8 then it's a DOS dir syntax
		 */
		if(server_type == FTP_NT_TYPE && !NET_IS_SPACE(entry[8]))
			server_type = FTP_UNIX_TYPE;

        switch (server_type)
        {
            case FTP_UNIX_TYPE:
            case FTP_PETER_LEWIS_TYPE:
            case FTP_WEBSTAR_TYPE:
            case FTP_MACHTEN_TYPE:
				/* interpret and edit LIST output from Unix server */

                if(!PL_strncmp(entry, "total ", 6)  
						|| (PL_strstr(entry, "Permission denied") != NULL)
						    || 	(PL_strstr(entry, "not available") != NULL))
                  {
                    entry_info->display=FALSE;
                    return(entry_info);
                  }
 
                len = PL_strlen(entry);

                if( '+' == entry[0] )
                {
                    /*
                     * EPLF: see http://pobox.com/~djb/proto/eplf.txt
                     * "+i8388621.29609,m824255902,/,\tdev"
                     * "+i8388621.44468,m839956783,r,s10376,\tRFCEPLF"
                     *
                     * A plus, a bunch of comma-separated facts, a tab, 
                     * and then the name.  Facts include m for mdtm (as
                     * seconds since the Unix epoch), s for size, r for
                     * (readable) file, and / for (listable) directory.
                     */
                    char *tab = PL_strchr(entry, '\t');
                    char *begin, *end;
                    remove_size = TRUE; /* It'll be put back if we have data */
                    if( (char *)0 == tab ) break;
                    for( begin = &entry[1]; begin < tab; begin = &end[1])
                    {
                        end = PL_strchr(begin, ',');
                        if( (char *)0 == end ) break;
                        switch( *begin )
                        {
                            case 'm':
                                *end = '\0';
                                entry_info->date = XP_ATOI(&begin[1]);
                                *end = ',';
                                break;
                            case 's':
                                *end = '\0';
                                entry_info->size = XP_ATOI(&begin[1]);
                                *end = ',';
                                remove_size = FALSE;
                                break;
                            case 'r':
                                entry_info->special_type = NET_FILE_TYPE;
                                break;
                            case '/':
                                entry_info->special_type = NET_DIRECTORY;
                                break;
                            default:
                                break;
                        }
                    }

                    entry_info->filename = NET_Escape(&tab[1], URL_PATH);
                    break;
                }
                else 
                /* check first character of ls -l output */
                if (toupper(entry[0]) == 'D') 
                  {
                    /* it's a directory */
				    entry_info->special_type = NET_DIRECTORY;
                    remove_size=TRUE; /* size is not useful */
                  }
                else if (entry[0] == 'l')
                  {
                    /* it's a symbolic link, does the user care about
                     * knowing if it is symbolic?  I think so since
                     * it might be a directory
                     */
					entry_info->special_type = NET_SYM_LINK;
                    remove_size=TRUE; /* size is not useful */

                    /* strip off " -> pathname" */
                    for (i = len - 1; (i > 3) && (!NET_IS_SPACE(entry[i])
                    || (entry[i-1] != '>') 
                    || (entry[i-2] != '-')
                    || (entry[i-3] != ' ')); i--)
                             ; /* null body */
                    if (i > 3)
                      {
                        entry[i-3] = '\0';
                        len = i - 3;
                      }
                  } /* link */

                net_parse_ls_line(entry, entry_info); 

                if(!PL_strcmp(entry_info->filename,"..") || !PL_strcmp(entry_info->filename,"."))
            		entry_info->display=FALSE;

				/* add a trailing slash to all directories */
				if(entry_info->special_type == NET_DIRECTORY)
				    StrAllocCat(entry_info->filename, "/");

                /* goto the bottom and get real type */
                break;

            case FTP_VMS_TYPE:
                /* Interpret and edit LIST output from VMS server */
                /* and convert information lines to zero length.  */
                net_parse_vms_dir_entry(entry, entry_info);

                /* Get rid of any junk lines */
                if(!entry_info->display)
                    return(entry_info);

                /** Trim off VMS directory extensions **/
                len = PL_strlen(entry_info->filename);
                if ((len > 4) && !PL_strcmp(&entry_info->filename[len-4], ".dir"))
                  {
                    entry_info->filename[len-4] = '\0';
				    entry_info->special_type = NET_DIRECTORY;
                    remove_size=TRUE; /* size is not useful */
					/* add trailing slash to directories
					 */
					StrAllocCat(entry_info->filename, "/");
                  }
                /* goto the bottom and get real type */
                break;

            case FTP_CMS_TYPE:
                /* can't be directory... */
                /*
                 * "entry" already equals the correct filename
                 */
		        /* escape and copy
	 	         */
		        entry_info->filename = NET_Escape(entry, URL_PATH);
                /* goto the bottom and get real type */
                break;

            case FTP_NCSA_TYPE:
            case FTP_TCPC_TYPE:
                /* directories identified by trailing "/" characters */
		        /* escape and copy
	 	         */
		        entry_info->filename = NET_Escape(entry, URL_PATH);
                len = PL_strlen(entry);
                if (entry[len-1] == '/')
                  {
				    entry_info->special_type = NET_DIRECTORY;
                    remove_size=TRUE; /* size is not useful */
                  }
                  /* goto the bottom and get real type */
                break;

			case FTP_NT_TYPE:
				/* windows NT DOS dir syntax.
				 * looks like:  
				 *            1         2         3         4         5
				 *  012345678901234567890123456789012345678901234567890
				 *  06-29-95  03:05PM       <DIR>          muntemp
				 *  05-02-95  10:03AM               961590 naxp11e.zip
				 *
				 *  The date time directory indicator and filename
				 *  are always in a fixed position.  The file
			 	 *  size always ends at position 37.
				 */
				  {
					char *date, *size_s, *name;
		
					if(PL_strlen(entry) > 37)
					  {
					    date = entry;
					    entry[17] = '\0';
					    size_s = &entry[18];
					    entry[38] = '\0';
					    name = &entry[39];
    
					    if(PL_strstr(size_s, "<DIR>"))
						    entry_info->special_type = NET_DIRECTORY;
					    else
						    entry_info->size = atol(XP_StripLine(size_s));
    
					    entry_info->date = net_parse_dos_date_time(date);

					    StrAllocCopy(entry_info->filename, name);
					  }
					else
					  {
					    StrAllocCopy(entry_info->filename, entry);
					  }
				  }
				break;
    
            default:
                /* we cant tell if it is a directory since we only
                 * did an NLST   
                 */
		        /* escape and copy
	 	         */
		        entry_info->filename = NET_Escape(entry, URL_PATH);
                return(entry_info); /* mostly empty info */
                /* break; Not needed */

        } /* switch (server_type) */


    if(remove_size && entry_info->size)
      {
        entry_info->size = 0;
      }

    /* get real types eventually */
    if(!entry_info->special_type) 
      {
        entry_info->cinfo = NET_cinfo_find_type(entry_info->filename);
      }

    return(entry_info);

} /* net_parse_dir_entry */


PRIVATE int
net_get_ftp_password(ActiveEntry *ce)
{
    FTPConData *cd = (FTPConData *) ce->con_data;

	if(!cd->password)
	  {
        char * host_string = NET_ParseURL(ce->URL_s->address, (GET_USERNAME_PART | GET_HOST_PART) );

#ifndef MCC_PROXY
		if(ftp_last_password 
				&& ftp_last_password_host 
					&& !PL_strcmp(ftp_last_password_host, host_string)) 
		  {
			StrAllocCopy(cd->password, ftp_last_password);
		  }
		else
#endif
	      {
			/* check for cached password */
			char *key = gen_ftp_password_key(ce->URL_s->address, cd->username);

			if(key)
			{
				PCNameValueArray * array = PC_CheckForStoredPasswordArray(PC_FTP_MODULE_KEY, key);

				if(array)
					cd->password = PC_FindInNameValueArray(array, PC_FTP_PASSWORD_KEY);

				PC_FreeNameValueArray(array);

				PR_Free(key);
			}

			if(!cd->password)
			{
                PRBool status;
                NET_AuthClosure * auth_closure = PR_NEWZAP (NET_AuthClosure);

            	PR_snprintf(cd->output_buffer, OUTPUT_BUFFER_SIZE,
							XP_GetString(XP_PROMPT_ENTER_PASSWORD),
							host_string);

                /* the new multi-threaded case -- we return asynchronously */
                if (!auth_closure) {
                  return(MK_INTERRUPTED);
                }
        
                auth_closure->_private = (void *) ce;
                auth_closure->user = NULL;
                auth_closure->pass = NULL;
                auth_closure->msg = PL_strdup (cd->output_buffer);

            	status = stub_PromptPassword(ce->window_id,
                                             cd->output_buffer, 
                                             &cd->store_password,
                                             FALSE /* not secure */,
                                             (void *) auth_closure);

                PR_Free(host_string);
                /* this no longer a user cancel - password prompting is async */
                return(FTP_WAIT_FOR_AUTH);
			}
		  }
	  }

    cd->next_state = FTP_SEND_PASSWORD;

    return 0;
}


PRIVATE int
net_get_ftp_password_response(FTPConData *cd)
{
	/* not used yet
	 */
    return 0; /* #### what should return value be? */
}

/* the load initializeation routine for FTP.
 *
 * this sets up all the data structures and begin's the
 * connection by calling processFTP
 */
PRIVATE int32
net_FTPLoad (ActiveEntry * ce)
{
	char * host = NET_ParseURL(ce->URL_s->address, GET_HOST_PART);
	char * unamePwd = NET_ParseURL(ce->URL_s->address, GET_USERNAME_PART | GET_PASSWORD_PART);
	char *username,*colon,*password=NULL;
    char * filename;
    char * semi_colon;
    FTPConData * cd = PR_NEW(FTPConData);

    if(!cd)
	  {
	    ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_OUT_OF_MEMORY); 
		ce->status = MK_OUT_OF_MEMORY;
        return(MK_OUT_OF_MEMORY);  
	  }

	/* get the username & password out of the combo string */
	if( (colon = PL_strchr(unamePwd, ':')) != NULL ) {
		*colon='\0';
		username=PL_strdup(unamePwd);
		password=PL_strdup(colon+1);
		*colon=':';
		PR_Free(unamePwd);
	} else {
		username=unamePwd;
	}
	
    ce->con_data = (FTPConData *)cd;

    /* init the connection data struct 
     */
    memset(cd, 0, sizeof(FTPConData));
    cd->dsock         = NULL;
    cd->listen_sock   = NULL;
    cd->pasv_mode     = TRUE;
    cd->is_directory  = FALSE;
    cd->next_state_after_response = FTP_ERROR_DONE;
    cd->cont_response     = -1;

	/* @@@@ hope we never need a buffer larger than this 
	 */
	cd->output_buffer = (char *) PR_Malloc(OUTPUT_BUFFER_SIZE);
	if(!cd->output_buffer)
	  {
		PR_Free(cd);
	    ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_OUT_OF_MEMORY); 
		ce->status = MK_OUT_OF_MEMORY;
        return(MK_OUT_OF_MEMORY);  
	  }

	if (username && *username) {
		StrAllocCopy(cd->username, username);
		if (password && *password)
			StrAllocCopy(cd->password, password);
	} else {
		const char * user = NULL;
		PR_FREEIF(username);
		username = NULL;
		PR_FREEIF(password);
		password = NULL;
		
#ifdef OLD_MOZ_MAIL_NEWS
#ifdef MOZILLA_CLIENT
		if(net_get_send_email_address_as_password())
			user = FE_UsersMailAddress();
#endif
#endif /* OLD_MOZ_MAIL_NEWS */

		if(user && *user)
		  {
		    StrAllocCopy(cd->password, user);

			/* make sure it has an @ sign in it or else the ftp
			 * server won't like it
			 */
			if(!PL_strchr(cd->password, '@'))
				StrAllocCat(cd->password, "@");
		  }
		else
	      {
			StrAllocCopy(cd->password, "mozilla@");
		  }
	}

    /* get the path */
    cd->path = NET_ParseURL(ce->URL_s->address, GET_PATH_PART);

    if(!cd->path)
      {
        PR_Free(ce->con_data);
        PR_Free(cd->output_buffer);
	    ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_OUT_OF_MEMORY); 
		ce->status = MK_OUT_OF_MEMORY;
        return(MK_OUT_OF_MEMORY);
      }

	NET_UnEscape(cd->path);

	if(*cd->path == '\0')
	  {
		TRACEMSG(("Found ftp url with NO PATH"));
		
		cd->use_default_path = TRUE;
	  }
	else
	  {
		/* terminate at an "\r" or "\n" in the string
		 * this prevents URL's of the form "foo\nDELE foo"
		 */
		strtok(cd->path, "\r");
		strtok(cd->path, "\n");
	  }

	/* if the path begins with /./ 
	 * use pwd to get the sub path
	 * and append the end to it
	 */
	if(!PL_strncmp(cd->path, "/./", 3) || !PL_strcmp(cd->path, "/."))
	  {
		char * tmp = cd->path;

		cd->use_default_path = TRUE;

		/* skip the "/." and leave a slash at the beginning */
		cd->path = PL_strdup(cd->path+2);
		PR_Free(tmp);

		if(!cd->path)
		  {
			PR_Free(ce->con_data);
        	PR_Free(cd->output_buffer);
	    	ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_OUT_OF_MEMORY); 
			ce->status = MK_OUT_OF_MEMORY;
			return(MK_OUT_OF_MEMORY);
		  }
	  }

    /* set the default type for use later
     */
    cd->type = FTP_MODE_UNKNOWN;

    TRACEMSG(("content length: %d, real content length: %d",
           ce->URL_s->content_length, ce->URL_s->real_content_length));
    if( (ce->URL_s->real_content_length > ce->URL_s->content_length) &&
        (NULL != ce->URL_s->cache_file) )
    {
        cd->restart = TRUE;
        TRACEMSG(("Considering a restart."));
    }

    /* look for the extra type information at the end per
     * the RFC 1630.  It will be delimited by a ';'
     */
    if((semi_colon = PL_strrchr(cd->path, ';')) != NULL)
      {
        /* just switch on the character at the end of this whole
         * thing since it must be the type to use
         */
        switch(semi_colon[PL_strlen(semi_colon)-1])
          {
            case 'a':
            case 'A':
                cd->type = FTP_MODE_ASCII;
				TRACEMSG(("Explicitly setting type ASCII"));
                break;

            case 'i':
            case 'I':
                cd->type = FTP_MODE_BINARY;
				TRACEMSG(("Explicitly setting type BINARY"));
                break;
          }

        /* chop off the bits after the semi colon
         */
        *semi_colon = '\0';

		/* also chop of the semi colon from the real URL */
		strtok(ce->URL_s->address, ";");
      }

    /* find the filename in the path */
    filename = PL_strrchr(cd->path, '/');
    if(!filename)
        filename = cd->path;
	else
		filename++;  /* go past slash */

    cd->filename = 0;
    StrAllocCopy(cd->filename, filename);

    /* Try and find an open connection to this 
     * server that is not currently being used
     */
    if(connection_list)
      {
        FTPConnection * tmp_con;
        XP_List * list_entry = connection_list;

        while((tmp_con = (FTPConnection *)XP_ListNextObject(list_entry)) != NULL)
          {
            /* if the hostnames match up exactly and the connection
             * is not busy at the moment then reuse this connection.
             */
            if(!tmp_con->busy && !PL_strcmp(tmp_con->hostname, host))
              {
                cd->cc = tmp_con;
                ce->socket = cd->cc->csock;  /* set select on the control socket */
    			NET_SetReadSelect(ce->window_id, cd->cc->csock);
                cd->cc->prev_cache = TRUE;  /* this was from the cache */
                break;
              }
          }
      }
    else
      {
    /* initialize the connection list 
     */
        connection_list = XP_ListNew();
      }
    
    if(!cd->cc)
      {

		TRACEMSG(("cached connection not found making new FTP connection"));

        /* build a control connection structure so we
         * can store the data as we go along
         */
        cd->cc = PR_NEW(FTPConnection);
        if(!cd->cc)
          {
            PR_Free(host);  /* free from way up above */
			PR_FREEIF(username);
			PR_FREEIF(password);
	        ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_OUT_OF_MEMORY); 
            return(MK_OUT_OF_MEMORY);
          }

        cd->cc->hostname = 0;
        StrAllocCopy(cd->cc->hostname, host);
        /* set the mode of the control connection unknown
         */
        cd->cc->cur_mode = FTP_MODE_UNKNOWN;
		cd->cc->csock = NULL;
		cd->cc->server_type = FTP_GENERIC_TYPE;
		cd->cc->use_list = FALSE;
		cd->cc->no_pasv  = FALSE;

        cd->cc->prev_cache = FALSE;  /* this wasn't from the cache */
        cd->cc->no_restart = FALSE; /* as far as we know so far */

        /* add this structure to the connection list even
         * though it's not really valid yet.
         * we will fill it in as we go and if
         * an error occurs will will remove it from the
         * list.  No one else will be able to use it since
         * we will mark it busy.
         */
        XP_ListAddObjectToEnd(connection_list, cd->cc);

        /* set this connection busy so no one else can use it
         */
        cd->cc->busy = TRUE;

		/* gate the maximum number of cached connections
		 * to one
		 */
		if(XP_ListCount(connection_list) > 1)
		  {
            XP_List * list_ptr = connection_list->next;
            FTPConnection * con;

            while(list_ptr)
              {
                con = (FTPConnection *) list_ptr->object;
                list_ptr = list_ptr->next;
                if(!con->busy)
                  {
                    XP_ListRemoveObject(connection_list, con);
                    PR_Close(con->csock);
					/* dont reduce the number of
					 * open connections since cached ones
					 * dont count as active
					 */
                    PR_FREEIF(con->hostname);
                    PR_Free(con);
                    break;
                  }
              }
		  }

        cd->next_state = FTP_CONTROL_CONNECT;
      }
    else
      {
		TRACEMSG(("Found cached FTP connection! YES!!! "));

    	/* don't send PASV if the server cant do it */
    	if(cd->cc->no_pasv || !net_use_pasv)
            cd->next_state = FTP_SEND_PORT;
        else
            cd->next_state = FTP_SEND_PASV;

        if( cd->cc->no_restart )
            ce->URL_s->server_can_do_restart = FALSE;
        else
            ce->URL_s->server_can_do_restart = TRUE;

        /* set this connection busy so no one else can use it
         */
        cd->cc->busy = TRUE;
		/* add this connection to the number that is open
		 * since it is now active
		 */
		NET_TotalNumberOfOpenConnections++;
      }

    PR_Free(host);  /* free from way up above */
	PR_FREEIF(username);
	PR_FREEIF(password);
    TRACEMSG(("Server can do restarts: %s", ce->URL_s->server_can_do_restart ? 
              "TRUE" : "FALSE"));
    return(net_ProcessFTP(ce));
} 


PUBLIC void
net_ResumeFTP(ActiveEntry *ce, NET_AuthClosure *auth_closure, PRBool resume)
{
  FTPConData * cd = (FTPConData *) ce->con_data;
  
  TRACEMSG (("net_ResumeFTP: %s", ce->URL_s->address));

  if (resume) {
    char * host_string = NET_ParseURL(ce->URL_s->address, (GET_USERNAME_PART | GET_HOST_PART) );

    cd->password = PL_strdup (auth_closure->pass);
    cd->next_state = FTP_SEND_PASSWORD;

    StrAllocCopy(ftp_last_password_host, host_string);
    StrAllocCopy(ftp_last_password, cd->password);

    PR_Free (host_string);
  } else {
    cd->next_state = FTP_DONE;
  }
      
  return;
}

/* the main state machine control routine.  Calls
 * the individual state processors
 * 
 * returns -1 when all done or 0 or positive all other times
 */
PRIVATE int32
net_ProcessFTP(ActiveEntry * ce)
{
    FTPConData * cd = (FTPConData *)ce->con_data;

    cd->pause_for_read = FALSE;

    while(!cd->pause_for_read)
      {
        TRACEMSG(("In ProcessFTP switch loop with state: %d \n", cd->next_state));

        switch(cd->next_state) {

          case FTP_WAIT_FOR_AUTH:
            printf ("ProcessFTP: waiting for password\n");
            break;

          case FTP_WAIT_FOR_RESPONSE:
            ce->status = net_ftp_response(ce);
            break;
    
          /* begin login states */
          case FTP_CONTROL_CONNECT:
            ce->status = NET_BeginConnect(ce->URL_s->address,
										  ce->URL_s->IPAddressString,
										 "FTP",
                               			 FTP_PORT,
										 &cd->cc->csock, 
										 HG27230 
										 &cd->tcp_con_data, 
										 ce->window_id,
										 &ce->URL_s->error_msg,
										  ce->socks_host,
										  ce->socks_port,
                                          ce->URL_s->localIP);
            ce->socket = cd->cc->csock;

			if(cd->cc->csock != NULL)
				NET_TotalNumberOfOpenConnections++;
    
            cd->pause_for_read = TRUE;
    
            if(ce->status == MK_CONNECTED)
              {
                cd->next_state = FTP_WAIT_FOR_RESPONSE;
                cd->next_state_after_response = FTP_SEND_USERNAME;
			    NET_SetReadSelect(ce->window_id, cd->cc->csock);
              }
            else if(ce->status > -1)
              {
                cd->next_state = FTP_CONTROL_CONNECT_WAIT;
                ce->con_sock = cd->cc->csock;  /* set con sock so we can select on it */
                NET_SetConnectSelect(ce->window_id, ce->con_sock);
    
              }
            break;
    
          case FTP_CONTROL_CONNECT_WAIT:
            ce->status = NET_FinishConnect(ce->URL_s->address,
										  "FTP",
                            	  		  FTP_PORT,
										  &cd->cc->csock, 
										  &cd->tcp_con_data, 
										  ce->window_id,
										  &ce->URL_s->error_msg,
                                          ce->URL_s->localIP);
    
            cd->pause_for_read = TRUE;
    
            if(ce->status == MK_CONNECTED)
              {
                cd->next_state = FTP_WAIT_FOR_RESPONSE;
                cd->next_state_after_response = FTP_SEND_USERNAME;
                NET_ClearConnectSelect(ce->window_id, ce->con_sock);
                ce->con_sock = NULL;  /* reset con sock so we don't select on it */
			    NET_SetReadSelect(ce->window_id, ce->socket);
              }
			else
			  {
				/* the not yet connected state */

        		/* unregister the old CE_SOCK from the select list
         		 * and register the new value in the case that it changes
         		 */
				if(ce->con_sock != cd->cc->csock)
				  {
            		NET_ClearConnectSelect(ce->window_id, ce->con_sock);
					ce->con_sock = cd->cc->csock;
            		NET_SetConnectSelect(ce->window_id, ce->con_sock);
				  }
			  }
            break;

          case FTP_SEND_USERNAME:
            ce->status = net_send_username((FTPConData *)ce->con_data);
            break;
    
          case FTP_SEND_USERNAME_RESPONSE:
            ce->status = net_send_username_response(ce);
            break;
    
		  case FTP_GET_PASSWORD:
            ce->status = net_get_ftp_password(ce);
            break;

		  case FTP_GET_PASSWORD_RESPONSE:
			/* currently skipped 
			 */
            ce->status = net_get_ftp_password_response((FTPConData *)ce->con_data);
            break;
    
          case FTP_SEND_PASSWORD:
            ce->status = net_send_password((FTPConData *)ce->con_data);
            break;
    
          case FTP_SEND_PASSWORD_RESPONSE:
            ce->status = net_send_password_response(ce);
            break;
        
          case FTP_SEND_ACCT:
            ce->status = net_send_acct((FTPConData *)ce->con_data);
            break;
    
          case FTP_SEND_ACCT_RESPONSE:
            ce->status = net_send_acct_response(ce);
            break;
    
          case FTP_SEND_REST_ZERO:
            ce->status = net_send_rest_zero(ce);
            break;
    
          case FTP_SEND_REST_ZERO_RESPONSE:
            ce->status = net_send_rest_zero_response(ce);
            break;
    
          case FTP_SEND_SYST:
            ce->status = net_send_syst((FTPConData *)ce->con_data);
            break;
    
          case FTP_SEND_SYST_RESPONSE:
            ce->status = net_send_syst_response((FTPConData *)ce->con_data);
            break;
    
          case FTP_SEND_MAC_BIN:
            ce->status = net_send_mac_bin((FTPConData *)ce->con_data);
            break;
    
          case FTP_SEND_MAC_BIN_RESPONSE:
            ce->status = net_send_mac_bin_response((FTPConData *)ce->con_data);
            break;
    
          case FTP_SEND_PWD:
            ce->status = net_send_pwd((FTPConData *)ce->con_data);
            break;
    
          case FTP_SEND_PWD_RESPONSE:
            ce->status = net_send_pwd_response(ce);
            break;
        /* end login states */

		  case FTP_FIGURE_OUT_WHAT_TO_DO:
            ce->status = net_figure_out_what_to_do(ce);
			break;

		  case FTP_SEND_MKDIR:
            ce->status = net_send_mkdir(ce);
			break;

		  case FTP_SEND_MKDIR_RESPONSE:
            ce->status = net_send_mkdir_response(ce);
			break;
    
		  case FTP_SEND_DELETE_FILE:
            ce->status = net_send_delete_file(ce);
			break;

		  case FTP_SEND_DELETE_FILE_RESPONSE:
            ce->status = net_send_delete_file_response(ce);
			break;
    
		  case FTP_SEND_DELETE_DIR:
            ce->status = net_send_delete_dir(ce);
			break;

		  case FTP_SEND_DELETE_DIR_RESPONSE:
            ce->status = net_send_delete_dir_response(ce);
			break;

          case FTP_SEND_PASV:
            ce->status = net_send_pasv((FTPConData *)ce->con_data);
            break;
    
          case FTP_SEND_PASV_RESPONSE:
            ce->status = net_send_pasv_response((FTPConData *)ce->con_data);
            break;
    
          case FTP_PASV_DATA_CONNECT:
            ce->status = NET_BeginConnect(cd->data_con_address, 
										  NULL,
										 "FTP Data Connection",
                       					FTP_PORT, 
										 &cd->dsock, 
										 HG27230 
										 &cd->tcp_con_data, 
										 ce->window_id,
										 &ce->URL_s->error_msg,
										  ce->socks_host,
										  ce->socks_port,
                                          ce->URL_s->localIP);
    
            if(ce->status == MK_CONNECTED)
              {
                cd->next_state = FTP_GET_FILE_TYPE;
              }
            else if(ce->status > -1)
              {
                cd->pause_for_read = TRUE;
                cd->next_state = FTP_PASV_DATA_CONNECT_WAIT;
                ce->con_sock = cd->dsock;  /* set con sock so we can select on it */
                NET_SetConnectSelect(ce->window_id, ce->con_sock);
              }
            break;

          case FTP_PASV_DATA_CONNECT_WAIT:
                ce->status = NET_FinishConnect(cd->data_con_address, 
											  "FTP Data Connection",
                                			  FTP_PORT,
											  &cd->dsock,
											  &cd->tcp_con_data,
											  ce->window_id,
											  &ce->URL_s->error_msg,
                                              ce->URL_s->localIP);
                TRACEMSG(("FTP: got pasv data connection on port #%d\n",cd->cc->csock));
    
                if(ce->status == MK_CONNECTED)
                  {
                    cd->next_state = FTP_GET_FILE_TYPE;
                    NET_ClearConnectSelect(ce->window_id, ce->con_sock);
                    ce->con_sock = NULL;  /* reset con sock so we don't select on it */
                  }
                else
                  {
					ce->con_sock = cd->dsock; /* it might change */
                    cd->pause_for_read = TRUE;
                  }
            break;
    
          case FTP_SEND_PORT:
            ce->status = net_send_port(ce);
            break;
    
          case FTP_SEND_PORT_RESPONSE:
            ce->status = net_send_port_response(ce);
            break;
    
		  /* This is the state that gets called after
		   * login and port/pasv are done
		   */
          case FTP_GET_FILE_TYPE:
            ce->status = net_get_ftp_file_type(ce);
            break;
    
          case FTP_SEND_BIN:
            ce->status = net_send_bin((FTPConData *)ce->con_data);
            break;
    
          case FTP_SEND_ASCII:
            ce->status = net_send_ascii((FTPConData *)ce->con_data);
            break;
    
          case FTP_SEND_BIN_RESPONSE:
          case FTP_SEND_ASCII_RESPONSE:
            ce->status = net_send_bin_or_ascii_response(ce);
            break;

		  case FTP_GET_FILE_SIZE:
            ce->status = net_get_ftp_file_size(ce);
            break;

		  case FTP_GET_FILE_SIZE_RESPONSE:
            ce->status = net_get_ftp_file_size_response(ce);
            break;
    
		  case FTP_GET_FILE_MDTM:
            ce->status = net_get_ftp_file_mdtm(ce);
            break;

		  case FTP_GET_FILE_MDTM_RESPONSE:
            ce->status = net_get_ftp_file_mdtm_response(ce);
            break;
    
          case FTP_GET_FILE:
            ce->status = net_get_ftp_file((FTPConData *)ce->con_data);
            break;
    
          case FTP_SEND_REST:
            ce->status = net_send_rest(ce);
            break;

          case FTP_SEND_REST_RESPONSE:
            ce->status = net_send_rest_response(ce);
            break;

          case FTP_SEND_RETR:
            ce->status = net_send_retr((FTPConData *)ce->con_data);
            break;
    
          case FTP_SEND_RETR_RESPONSE:
            ce->status = net_send_retr_response(ce);
            break;
    
          case FTP_SEND_CWD:
            ce->status = net_send_cwd((FTPConData *)ce->con_data);
            break;
    
          case FTP_SEND_CWD_RESPONSE:
            ce->status = net_send_cwd_response(ce);
            break;
    
          case FTP_BEGIN_PUT:
            ce->status = net_begin_put(ce);
            break;
    
          case FTP_BEGIN_PUT_RESPONSE:
            ce->status = net_begin_put_response(ce);
            break;
    
          case FTP_DO_PUT:
            ce->status = net_do_put(ce);
            break;
    
          case FTP_SEND_LIST_OR_NLST:
            ce->status = net_send_list_or_nlst(ce);
            break;
    
          case FTP_SEND_LIST_OR_NLST_RESPONSE:
            ce->status = net_send_list_or_nlst_response(ce);
            break;

          case FTP_SETUP_STREAM:
            ce->status = net_setup_ftp_stream(ce);
            break;
    
          case FTP_WAIT_FOR_ACCEPT:
            ce->status = net_wait_for_ftp_accept(ce);
            break;
    
          case FTP_READ_CACHE:
            ce->status = net_ftp_push_partial_cache_file(ce);
            break;

          case FTP_START_READ_FILE:
            ce->status = net_start_ftp_read_file(ce);
            break;
    
          case FTP_READ_FILE:
            ce->status = net_ftp_read_file(ce);
            break;
    
          case FTP_START_READ_DIRECTORY:
            ce->status = net_start_ftp_read_dir(ce);
            break;
    
          case FTP_READ_DIRECTORY:
    
            ce->status = net_ftp_read_dir(ce);
            break;

	  case FTP_PRINT_DIR:
            ce->status = net_ftp_print_dir(ce);
            break;

          case FTP_DONE:
			if(cd->stream)
            	COMPLETE_STREAM;

            /* Make sure that any previous cache entry is not locked anymore */
#ifndef NU_CACHE
            NET_ChangeCacheFileLock(ce->URL_s, FALSE);
#endif /* NU_CACHE */

    	    /* don't close the control sock since we need 
			 * it for connection caching
			 */
            if(cd->dsock != NULL)
			  {
			    NET_ClearReadSelect(ce->window_id, cd->dsock);
				TRACEMSG(("Closing and clearing sock cd->dsock: %d", cd->dsock));
				PR_Close(cd->dsock);
				cd->dsock = NULL;
			  }

            if(cd->listen_sock != NULL)
              {
				TRACEMSG(("Closing and clearing sock cd->listen_sock: %d", cd->listen_sock));
			    NET_ClearReadSelect(ce->window_id, cd->listen_sock);
				PR_Close(cd->listen_sock);
				/* not counted as a connection */
			  }
				
            cd->next_state = FTP_FREE_DATA;
    
            cd->cc->busy = FALSE;  /* we are no longer using the connection */
			NET_ClearReadSelect(ce->window_id, cd->cc->csock);

			/* we can't reuse VMS control connections since they are screwed up
		   	 */
			if(cd->cc->server_type == FTP_VMS_TYPE)
			  {
	            /* remove the connection from the cache list
                 * and free the data
                 */
                XP_ListRemoveObject(connection_list, cd->cc);
                PR_FREEIF(cd->cc->hostname);
                if(cd->cc->csock != NULL)
                  {
				    TRACEMSG(("Closing and clearing sock cd->cc->csock: %d", cd->cc->csock));
                    PR_Close(cd->cc->csock);
					cd->cc->csock = NULL;
                  }
                PR_Free(cd->cc);
                cd->cc = 0;
			  }

			/* remove the cached connection from the total number
			 * counted, since it is no longer active
			 */
			NET_TotalNumberOfOpenConnections--;

            break;
    
          case FTP_ERROR_DONE:
            if(cd->sort_base)
              {
                 /* we have to do this to get it free'd
                  */
		 if(cd->stream && (*cd->stream->is_write_ready)(cd->stream))
                 	NET_PrintDirectory(&cd->sort_base, cd->stream, cd->path, ce->URL_s);
              }

            if(cd->stream && cd->stream->data_object)
            {
                ABORT_STREAM(ce->status);
                /* (ABORT_STREAM frees cd->stream->data_object:) */
                cd->stream->data_object = NULL;
            }

            /* lock the object in cache if we can restart it */
            /* This way even if it's huge, we can restart. */
            TRACEMSG(("FTP aborting!  content_length = %ld, real_content_length = %ld\n", 
                      ce->URL_s->content_length, ce->URL_s->real_content_length));
            TRACEMSG(("  bytes_received = %ld\n", ce->bytes_received));
#ifndef NU_CACHE
            if(ce->URL_s->server_can_do_restart)
                NET_ChangeCacheFileLock(ce->URL_s, TRUE);
#endif /* NU_CACHE */

            if(cd->dsock != NULL)
              {
			    NET_ClearConnectSelect(ce->window_id, cd->dsock);
			    NET_ClearReadSelect(ce->window_id, cd->dsock);
                NET_ClearDNSSelect(ce->window_id, cd->dsock);

#ifdef XP_WIN
				if(cd->calling_netlib_all_the_time)
				{
					cd->calling_netlib_all_the_time = FALSE;
					NET_ClearCallNetlibAllTheTime(ce->window_id, "mkftp");
				}
#endif /* XP_WIN */

				TRACEMSG(("Closing and clearing sock cd->dsock: %d", cd->dsock));
                PR_Close(cd->dsock);
                cd->dsock = NULL;
              }

			if(cd->file_to_post_fp)
			  {
				NET_XP_FileClose(cd->file_to_post_fp);
				cd->file_to_post_fp = 0;
			  }
    
			if(cd->cc->csock != NULL)
			  {
			    NET_ClearConnectSelect(ce->window_id, cd->cc->csock);
			    NET_ClearReadSelect(ce->window_id, cd->cc->csock);
                NET_ClearDNSSelect(ce->window_id, cd->cc->csock);
				TRACEMSG(("Closing and clearing sock cd->cc->csock: %d", cd->cc->csock));
                PR_Close(cd->cc->csock);
				cd->cc->csock = NULL;
				NET_TotalNumberOfOpenConnections--;
			  }

            if(cd->listen_sock != NULL)
              {
				TRACEMSG(("Closing and clearing sock cd->listen_sock: %d", cd->listen_sock));
				NET_ClearReadSelect(ce->window_id, cd->listen_sock);
				PR_Close(cd->listen_sock);
				/* not counted as a connection */
			  }


            /* check if this connection came from the cache or if it was
             * a new connection.  If it was new lets start it over again
             */
            if(cd->cc->prev_cache 
				&& ce->status != MK_INTERRUPTED
				  && ce->status != MK_UNABLE_TO_CONVERT
					&& ce->status != MK_OUT_OF_MEMORY
					&& ce->status != MK_DISK_FULL)
              {
                cd->cc->prev_cache = NO;
                cd->next_state = FTP_CONTROL_CONNECT;
				cd->pasv_mode = TRUE;  /* until we learn otherwise */
            	cd->pause_for_read = FALSE;
                ce->status = 0;  /* keep going */
            	cd->cc->cur_mode = FTP_MODE_UNKNOWN;
              }
            else
              {
                cd->next_state = FTP_FREE_DATA;
        
                /* remove the connection from the cache list
                 * and free the data
                 */
                XP_ListRemoveObject(connection_list, cd->cc);
                PR_FREEIF(cd->cc->hostname);
                PR_Free(cd->cc);
              }
    
            break;
    
          case FTP_FREE_DATA:
				if(cd->calling_netlib_all_the_time)
				  {
					NET_ClearCallNetlibAllTheTime(ce->window_id, "mkftp");
                    cd->calling_netlib_all_the_time = FALSE;
				  }

#if !defined(SMOOTH_PROGRESS)
                if(cd->destroy_graph_progress)
                    FE_GraphProgressDestroy(ce->window_id, 	
											ce->URL_s, 	
											cd->original_content_length,
											ce->bytes_received);
#endif

#ifdef EDITOR
                if(cd->destroy_file_upload_progress_dialog) {
                    /* Convert from netlib errors to editor errors. */
                    ED_FileError error = ED_ERROR_NONE;
                    if ( ce->status < 0 )
                        error = ED_ERROR_PUBLISHING;
                    FE_SaveDialogDestroy(ce->window_id, error, ce->URL_s->post_data);
                    /*  FE_SaveDialogDestroy(ce->window_id, ce->status, cd->file_to_post); */
                }
#endif /* EDITOR */
                PR_FREEIF(cd->cwd_message_buffer);
                PR_FREEIF(cd->login_message_buffer);
                PR_FREEIF(cd->data_buf);
                PR_FREEIF(cd->return_msg);
                PR_FREEIF(cd->data_con_address);
                PR_FREEIF(cd->filename);
                PR_FREEIF(cd->path);
                PR_FREEIF(cd->username);
                PR_FREEIF(cd->password);
                PR_FREEIF(cd->stream);  /* don't forget the stream */
				PR_FREEIF(cd->output_buffer);
            	if(cd->tcp_con_data)
                	NET_FreeTCPConData(cd->tcp_con_data);
				

                PR_Free(cd);

                /* gate the maximum number of cached connections
			 	 * to one
                 */
                if(XP_ListCount(connection_list) > 1)
                  {
                    XP_List * list_ptr = connection_list->next;
                    FTPConnection * con;
        
                    while(list_ptr)
                      {
						con = (FTPConnection *) list_ptr->object;
						list_ptr = list_ptr->next;
                        if(!con->busy)
                          {
                            XP_ListRemoveObject(connection_list, con);

							if(con->csock != NULL)
							  {
                			    PR_Close(con->csock);
								/* dont reduce the number of
								 * open connections since cached ones
							 	 * dont count as active
								 */
							  }
                            PR_FREEIF(con->hostname);
                            PR_Free(con);
                            break;
                          }
                      }
                  }

                return(-1);
    
              default:
                TRACEMSG(("Bad state in ProcessFTP\n"));
                return(-1);
         } /* end switch */
    
       if(ce->status < 0 && cd->next_state != FTP_FREE_DATA)
          {
            cd->pause_for_read = FALSE;
            cd->next_state = FTP_ERROR_DONE;
          }
    } /* end while */

    return(ce->status);
}
    
/* abort the connection in progress
 */
PRIVATE int32
net_InterruptFTP(ActiveEntry * ce)
{
    FTPConData * cd = (FTPConData *)ce->con_data;

    cd->next_state = FTP_ERROR_DONE;
    ce->status = MK_INTERRUPTED;
	cd->cc->prev_cache = FALSE;  /* to keep it from reconnecting */
 
    return(net_ProcessFTP(ce));
}

/* Free any connections or memory that are cached
 */
PRIVATE void
net_CleanupFTP(void)
{
    if(connection_list)
      {
        FTPConnection * tmp_con;

        while((tmp_con = (FTPConnection *)XP_ListRemoveTopObject(connection_list)) != NULL)
          {
			if(!tmp_con->busy)
			  {
            	PR_Free(tmp_con->hostname);       /* hostname string */
				if(tmp_con->csock != NULL)
			      {
            	    PR_Close(tmp_con->csock);      /* control socket */
            	    tmp_con->csock = NULL;
					/* don't count cached connections as open ones
					 */
			      }
		        PR_Free(tmp_con);
              }
		  }
      }

    return;
}


MODULE_PRIVATE void
NET_InitFTPProtocol(void)
{
	static NET_ProtoImpl ftp_proto_impl;

	ftp_proto_impl.init = net_FTPLoad;
	ftp_proto_impl.process = net_ProcessFTP;
	ftp_proto_impl.interrupt = net_InterruptFTP;
	ftp_proto_impl.resume = net_ResumeFTP;
	ftp_proto_impl.cleanup = net_CleanupFTP;

	NET_RegisterProtocolImplementation(&ftp_proto_impl, FTP_TYPE_URL);
}
