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
 * state machine to speak pop3
 *
 * Originally designed and implemented by Lou Montulli '95
 * Extremely heavily modified by Terry Weissman and Jamie Zawinski '95 '96
 */

/* Please leave outside of ifdef for windows precompiled headers */
#include "rosetta.h"
#include "mkutils.h"
#include "mktcp.h"
#include "netutils.h"

/* A more guaranteed way of making sure that we never get duplicate messages
is to always get each message's UIDL (if the server supports it)
and use these for storing up deletes which were not committed on the
server.  Based on our experience, it looks like we do NOT need to
do this (it has performance tradeoffs, etc.).  To turn it on, three
things need to happen: #define POP_ALWAYS_USE_UIDL_FOR_DUPLICATES, verify that
the uidl's are correctly getting added when the delete response is received,
and change the POP3_QUIT_RESPONSE state to flush the newly committed deletes. */

/* 
 * Cannot have the following line uncommented. It is defined.
 * #ifdef POP_ALWAYS_USE_UIDL_FOR_DUPLICATES will always be evaluated
 * as TRUE.
 *
#define POP_ALWAYS_USE_UIDL_FOR_DUPLICATES 0
 *
 */

#ifdef MOZILLA_CLIENT

#include "mkgeturl.h"
#include "mkpop3.h"
#include "xp_hash.h"
#include "merrors.h"
#include "secnav.h"
#include  "ssl.h"
#include "prio.h"
#include "xp_error.h"
#include "xpgetstr.h"
#include "prerror.h"
#include "prefapi.h"

extern int MK_OUT_OF_MEMORY;
extern int MK_POP3_DELE_FAILURE;
extern int MK_POP3_LIST_FAILURE;
extern int MK_POP3_MESSAGE_WRITE_ERROR;
extern int MK_POP3_NO_MESSAGES;
extern int MK_POP3_OUT_OF_DISK_SPACE;
extern int MK_POP3_PASSWORD_FAILURE;
extern int MK_POP3_PASSWORD_UNDEFINED;
extern int MK_POP3_RETR_FAILURE;
extern int MK_POP3_SERVER_ERROR;
extern int MK_POP3_USERNAME_FAILURE;
extern int MK_POP3_USERNAME_UNDEFINED;
extern int MK_TCP_READ_ERROR;
extern int MK_TCP_WRITE_ERROR;
extern int XP_ERRNO_EWOULDBLOCK;
extern int XP_NO_ANSWER;
extern int XP_THE_POP3_SERVER_DOES_NOT_SUPPORT_UIDL_ETC;
extern int XP_RECEIVING_MESSAGE_OF;
extern int XP_THE_POP3_SERVER_DOES_NOT_SUPPORT_THE_TOP_COMMAND;
extern int XP_THE_PREVIOUSLY_ENTERED_PASSWORD_IS_INVALID_ETC;
extern int XP_CONNECT_HOST_CONTACTED_SENDING_LOGIN_INFORMATION;
extern int XP_PASSWORD_FOR_POP3_USER;
extern int MK_MSG_DOWNLOAD_COUNT;
extern int MK_UNABLE_TO_CONNECT;
extern int MK_CONNECTION_REFUSED;
extern int MK_CONNECTION_TIMED_OUT;

extern void net_graceful_shutdown(PRFileDesc * sock, XP_Bool isSecure);

#define POP3_PORT 110  /* the iana port for pop3 */
#define OUTPUT_BUFFER_SIZE 512  /* max size of command string */

/* Globals */
  PRIVATE char *net_pop3_password=0; /* obfuscated pop3 password */
PRIVATE XP_Bool net_pop3_password_pending=FALSE;
PRIVATE XP_Bool net_pop3_block = FALSE;

/* We set this if we find that the UIDL command doesn't work.
   It is reset to FALSE if the user name is re-set (a cheap way
   of seeing if the server has changed...)
 */
PRIVATE XP_Bool net_uidl_command_unimplemented = FALSE;
PRIVATE XP_Bool net_xtnd_xlst_command_unimplemented = FALSE;
PRIVATE XP_Bool net_top_command_unimplemented = FALSE;

/* definitions of extended POP3 capabilities
 */
typedef enum {
	POP3_CAPABILITY_UNDEFINED = 0x00000000,
	POP3_AUTH_LOGIN_UNDEFINED = 0x00000001,
	POP3_HAS_AUTH_LOGIN		  = 0x00000002,
	POP3_XSENDER_UNDEFINED    = 0x00000004,
	POP3_HAS_XSENDER		  = 0x00000008,
	POP3_GURL_UNDEFINED		  = 0x00000010,
	POP3_HAS_GURL			  = 0x00000020
} Pop3CapabilityEnum;

PRIVATE uint32 pop3CapabilityFlags = POP3_AUTH_LOGIN_UNDEFINED |
									 POP3_XSENDER_UNDEFINED |
									 POP3_GURL_UNDEFINED;


/* definitions of state for the state machine design
 */
typedef enum {
    POP3_READ_PASSWORD,
    POP3_START_CONNECT,
    POP3_FINISH_CONNECT,
	POP3_WAIT_FOR_RESPONSE,
	POP3_WAIT_FOR_START_OF_CONNECTION_RESPONSE,
	POP3_SEND_USERNAME,
	POP3_SEND_PASSWORD,
	POP3_SEND_STAT,
	POP3_GET_STAT,
	POP3_SEND_LIST,
	POP3_GET_LIST,
	POP3_SEND_UIDL_LIST,
	POP3_GET_UIDL_LIST,
	POP3_SEND_XTND_XLST_MSGID,
	POP3_GET_XTND_XLST_MSGID,
	POP3_GET_MSG,
	POP3_SEND_TOP,
	POP3_TOP_RESPONSE,
	POP3_SEND_RETR,
	POP3_RETR_RESPONSE,
	POP3_SEND_DELE,
	POP3_DELE_RESPONSE,
	POP3_SEND_QUIT,
    POP3_DONE,
    POP3_ERROR_DONE,
    POP3_FREE,
    /* The following 3 states support the use of the 'TOP' command instead of UIDL
       for leaving mail on the pop server -km */
    POP3_START_USE_TOP_FOR_FAKE_UIDL, 
    POP3_SEND_FAKE_UIDL_TOP, 
    POP3_GET_FAKE_UIDL_TOP,
	POP3_SEND_AUTH,
	POP3_AUTH_RESPONSE,
	POP3_AUTH_LOGIN,
	POP3_AUTH_LOGIN_RESPONSE,
	POP3_SEND_XSENDER,
	POP3_XSENDER_RESPONSE,
	POP3_SEND_GURL,
	POP3_GURL_RESPONSE,
#ifdef POP_ALWAYS_USE_UIDL_FOR_DUPLICATES
	POP3_QUIT_RESPONSE,
#endif
	POP3_INTERRUPTED
} Pop3StatesEnum;

/* structure to hold data pertaining to the active state of
 * a transfer in progress.
 *
 */



#define KEEP		'k'			/* If we want to keep this item on server. */
#define DELETE_CHAR	'd'			/* If we want to delete this item. */
#define TOO_BIG		'b'			/* item left on server because it was too big */

typedef struct Pop3AllocedString { /* Need this silliness as a place to
									  keep the strings that are allocated
									  for the keys in the hash table. ### */
  char* str;
  struct Pop3AllocedString* next;
} Pop3AllocedString;

typedef struct Pop3UidlHost {
  char* host;
  char* user;
  XP_HashTable hash;
  Pop3AllocedString* strings;
  struct Pop3UidlHost* next;
} Pop3UidlHost;


typedef struct Pop3MsgInfo {
  int32 size;
  char* uidl;
} Pop3MsgInfo;

typedef struct _Pop3ConData {
	MSG_Pane* pane;				/* msglib pane object. */
	char* host;
	char* username;
    char* password;
    char* accountdir;

	XP_Bool leave_on_server;	/* Whether we're supposed to leave messages
								   on server. */
	int32 size_limit;			/* Leave messages bigger than this on the
								   server and only download a partial
								   message. */

    Pop3StatesEnum  next_state;       		/* the next state or action to be taken */
    Pop3StatesEnum  next_state_after_response;  
    Bool     	pause_for_read;   		/* Pause now for next read? */
	
	XP_Bool         command_succeeded;      /* did the last command succeed? */
	char           *command_response;		/* text of last response */
	int32           first_msg;
	TCP_ConData *tcp_con_data;  /* Data pointer for tcp connect state machine */
	char  		   *data_buffer;
	int32		    data_buffer_size;

    char *obuffer;		/* line buffer for output to msglib */
    uint32 obuffer_size;
    uint32 obuffer_fp;

	int32           number_of_messages;
	Pop3MsgInfo	   *msg_info;	/* Message sizes and uidls (used only if we
								   are playing games that involve leaving
								   messages on the server). */
	int32           last_accessed_msg;
	int32           cur_msg_size;
	char           *output_buffer;
	XP_Bool         truncating_cur_msg;     /* are we using top and uidl? */
	XP_Bool         msg_del_started;        /* True if MSG_BeginMailDel...
											 * called
											 */
	XP_Bool         only_check_for_new_mail;
  /* MSG_BIFF_STATE	biffstate;	 If just checking for, what the answer is. */

	XP_Bool         password_failed;        /* flag for password querying */
	void 		   *msg_closure;
	int32			bytes_received_in_message; 
	int32			total_folder_size;

  	int32			total_download_size; /* Number of bytes we're going to
											download.  Might be much less
											than the total_folder_size. */

    XP_Bool  graph_progress_bytes_p;	/* whether we should display info about
										   the bytes transferred (usually we
										   display info about the number of
										   messages instead.) */

    Pop3UidlHost   *uidlinfo;
    XP_HashTable	newuidl;
	char           *only_uidl;	/* If non-NULL, then load only this UIDL. */
	
								/* the following three fields support the 
								   use of the 'TOP' command instead of UIDL
       							   for leaving mail on the pop server -km */
       
								/* the current message that we are retrieving 
								   with the TOP command */
   int32 current_msg_to_top;	
   	
   												/* we will download this many in 
   												   POP3_GET_MSG */							   
   int32 number_of_messages_not_seen_before;
    											/* reached when we have TOPped 
    											   all of the new messages */
   XP_Bool found_new_message_boundary; 
   
   												/* if this is true then we don't stop 
   												   at new message boundary, we have to 
   												   TOP em all to delete some */
   XP_Bool delete_server_message_during_top_traversal;
   XP_Bool get_url;
   XP_Bool seenFromHeader;
   char *sender_info;
   NET_StreamClass *teststream;  
   URL_Struct *testurls;
   void *rdf;
   char* current_from;
   char* current_to;
   char* current_date;
   char* current_subject;
} Pop3ConData;

void *MSG_IncorporateBegin (ActiveEntry *ce) ;
int MSG_IncorporateAbort (Pop3ConData *cd)  ;
int MSG_IncorporateWrite (Pop3ConData *cd,   const char *block, int32 length) ;
int MSG_IncorporateComplete(Pop3ConData *cd) ;
 
char * POP_Base64Encode (char *src, int32 srclen);

PUBLIC void
NET_LeavePopMailOnServer(Bool do_it)
{
	/* XP_ASSERT(0);*/				/* This routine is obsolete. */
}

/* Well, someone finally found a legitimate reason to put an @ in the
 * mail server user name. They're trying to use the user name as a UID
 * in the LDAP directory, and the UIDs happen to have the format
 * foo@bar.com. We don't change our default behavior, but we let admins
 * turn it off if they want
 */
PRIVATE XP_Bool net_allow_at_sign_in_mail_user_name = FALSE;

MODULE_PRIVATE XP_Bool NET_GetAllowAtSignInMailUserName()
{
	return net_allow_at_sign_in_mail_user_name;
}

MODULE_PRIVATE void NET_SetAllowAtSignInMailUserName(XP_Bool allow)
{
	net_allow_at_sign_in_mail_user_name = allow;
}

PUBLIC void
NET_SetPopUsername(const char *username)
{
}

PUBLIC const char*
NET_GetPopUsername(void)
{
  return NULL;
}

PRIVATE void
net_set_pop3_password(const char *password)
{
     
}

PRIVATE char *
net_get_pop3_password(void)
{
	return NULL;
}

PUBLIC void
NET_SetPopPassword(const char *password)
{ 
}

PUBLIC const char *
NET_GetPopPassword(void)
{return NULL ;  
}

/* fix Mac warning of missing prototype */
PUBLIC void
NET_SetPopPassword2(const char *password);

PUBLIC void
NET_SetPopPassword2(const char *password)
{
	net_set_pop3_password(password);
}

/* sets the size limit for pop3 messages
 *
 * set the size negative to make it infinite length
 */
PUBLIC void
NET_SetPopMsgSizeLimit(int32 size)
{
	XP_ASSERT(0);				/* This routine is obsolete */
}

PUBLIC int32
NET_GetPopMsgSizeLimit(void)
{
	XP_ASSERT(0);				/* This routine is obsolete */
	return -1;
}


static int
uidl_cmp (const void *obj1, const void *obj2)
{
  XP_ASSERT (obj1 && obj2);
  return XP_STRCMP ((char *) obj1, (char *) obj2);
}


static void
put_hash(Pop3UidlHost* host, XP_HashTable table, const char* key, char value)
{
  Pop3AllocedString* tmp;
  int v = value;

  tmp = XP_NEW_ZAP(Pop3AllocedString);
  if (tmp) {
	tmp->str = XP_STRDUP(key);
	if (tmp->str) {
	  tmp->next = host->strings;
	  host->strings = tmp;
	  XP_Puthash(table, tmp->str, (void*) v);
	} else {
	  XP_FREE(tmp);
	}
  }
}

PRIVATE Pop3UidlHost* net_pop3_load_state(Pop3ConData *cd) 
{
   
  XP_File file;
  char* buf;
  char* host;
  char* user;
  char* uidl;
  char* flags;
  Pop3UidlHost* result = NULL;
  Pop3UidlHost* current = NULL;
  Pop3UidlHost* tmp; 
  char* searchhost = cd->host;
  char* searchuser = cd->username;

  result = XP_NEW_ZAP(Pop3UidlHost);
  if (!result) return NULL;
  result->host = XP_STRDUP(searchhost);
  result->user = XP_STRDUP(searchuser);
  result->hash = XP_HashTableNew(20, XP_StringHash, uidl_cmp);
  if (!result->host || !result->user || !result->hash) {
    FREEIF(result->host);
	FREEIF(result->user);
	if (result->hash) XP_HashTableDestroy(result->hash);
	XP_FREE(result);
	return NULL;
  }
  
  file = XP_FileOpen(cd->accountdir, xpMailPopState, XP_FILE_READ);
  
  if (!file) return result;
  buf = (char*)XP_ALLOC(512);
  if (buf) {
	while (XP_FileReadLine(buf, 512, file)) {
	  if (*buf == '#' || *buf == CR || *buf == LF || *buf == 0)
		continue;
	  if (buf[0] == '*') {
		/* It's a host&user line. */
		current = NULL;
		host = XP_STRTOK(buf + 1, " \t" LINEBREAK);
		user = XP_STRTOK(NULL, " \t" LINEBREAK);
		if (host == NULL || user == NULL) continue;
		for (tmp = result ; tmp ; tmp = tmp->next) {
		  if (XP_STRCMP(host, tmp->host) == 0 &&
			  XP_STRCMP(user, tmp->user) == 0) {
			current = tmp;
			break;
		  }
		}
		if (!current) {
		  current = XP_NEW_ZAP(Pop3UidlHost);
		  if (current) {
			current->host = XP_STRDUP(host);
			current->user = XP_STRDUP(user);
			current->hash = XP_HashTableNew(20, XP_StringHash, uidl_cmp);
			if (!current->host || !current->user || !current->hash) {
			  FREEIF(current->host);
			  FREEIF(current->user);
			  if (current->hash) XP_HashTableDestroy(current->hash);
			  XP_FREE(current);
			} else {
			  current->next = result->next;
			  result->next = current;
			}
		  }
		}
	  } else {
		/* It's a line with a UIDL on it. */
		if (current) {
		  flags = XP_STRTOK(buf, " \t" LINEBREAK);
		  uidl = XP_STRTOK(NULL, " \t" LINEBREAK);
		  if (flags && uidl) {
			XP_ASSERT((flags[0] == KEEP) || (flags[0] == DELETE_CHAR) || (flags[0] == TOO_BIG));
			if ((flags[0] == KEEP) || (flags[0] == DELETE_CHAR) || (flags[0] == TOO_BIG)) {
			  put_hash(current, current->hash, uidl, flags[0]);
			}
		  }
		}
	  }
	}
	XP_FREE(buf);
  }
  XP_FileClose(file);
  return result;
}



static XP_Bool
hash_empty_mapper(XP_HashTable hash, const void* key, void* value,
				  void* closure)
{
  *((XP_Bool*) closure) = FALSE;
  return FALSE;
}

static XP_Bool
hash_empty(XP_HashTable hash)
{
  XP_Bool result = TRUE;
  XP_Maphash(hash, hash_empty_mapper, &result);
  return result;
}


PRIVATE XP_Bool
net_pop3_write_mapper(XP_HashTable hash, const void* key, void* value,
					  void* closure)
{
  XP_File file = (XP_File) closure;
  XP_ASSERT((value == ((void *) (int) KEEP)) ||
			(value == ((void *) (int) DELETE_CHAR)) ||
			(value == ((void *) (int) TOO_BIG)));
  XP_FilePrintf(file, "%c %s" LINEBREAK, (char)(long)value, (char*) key);
  return TRUE;
}


PRIVATE void
net_pop3_write_state(Pop3ConData *cd)
{
  XP_File file;
  int32 len = 0;
  Pop3UidlHost* host = cd->uidlinfo;

  file = XP_FileOpen(cd->accountdir, xpMailPopState, XP_FILE_WRITE_BIN);

  if (!file) return;
  len = XP_FileWrite("# Netscape POP3 State File" LINEBREAK
			   "# This is a generated file!  Do not edit." LINEBREAK LINEBREAK,
			   -1, file);
  for (; host && (len >= 0); host = host->next)
  {
	  if (!hash_empty(host->hash))
	  {
		XP_FileWrite("*", 1, file);
		XP_FileWrite(host->host, -1, file);
		XP_FileWrite(" ", 1, file);
		XP_FileWrite(host->user, -1, file);
		len = XP_FileWrite(LINEBREAK, LINEBREAK_LEN, file);
		XP_Maphash(host->hash, net_pop3_write_mapper, file);
	}
  }
  XP_FileClose(file);
}


/*
Wrapper routines for POP data. The following routines are used from MSG_FolderInfoMail to
allow deleting of messages that have been kept on a POP3 server due to their size or
a preference to keep the messages on the server. When "deleting" messages we load
our state file, mark any messages we have for deletion and then re-save the state file.
*/
extern char* ReadPopData(char *name);
extern void SavePopData(char *data);
extern void net_pop3_delete_if_in_server(char *data, char *uidl, XP_Bool *changed);
extern void KillPopData(char* data);

/*
char* ReadPopData(char *name)
{
	Pop3UidlHost *uidlHost = NULL;
	
	if(!net_pop3_username || !*net_pop3_username)
		return (char*) uidlHost;
	
	uidlHost = net_pop3_load_state(name, net_pop3_username);
	return (char*) uidlHost; 
	return NULL;
}

void SavePopData(ActiveEntry *ce)
{
	Pop3UidlHost *host = (Pop3UidlHost*) data;

	if (!host)
		return;
	net_pop3_write_state(host); 
}


/*
Look for a specific UIDL string in our hash tables, if we have it then we need
to mark the message for deletion so that it can be deleted later. If the uidl of the
message is not found, then the message was downloaded completly and already deleted
from the server. So this only applies to messages kept on the server or too big
for download. */

void net_pop3_delete_if_in_server(char *data, char *uidl, XP_Bool *changed)
{
	Pop3UidlHost *host = (Pop3UidlHost*) data;
	
	if (!host)
		return;
	if (XP_Gethash (host->hash, (const void*) uidl, 0))
	{
		XP_Puthash(host->hash, uidl, (void*) DELETE_CHAR);
		*changed = TRUE;
	}
}




PRIVATE void
net_pop3_free_state(Pop3UidlHost* host) 
{
  Pop3UidlHost* h;
  Pop3AllocedString* tmp;
  Pop3AllocedString* next;
  while (host) {
	h = host->next;
	XP_FREE(host->host);
	XP_FREE(host->user);
	XP_HashTableDestroy(host->hash);
	tmp = host->strings;
	while (tmp) {
	  next = tmp->next;
	  XP_FREE(tmp->str);
	  XP_FREE(tmp);
	  tmp = next;
	}
	XP_FREE(host);
	host = h;
  }
}


void KillPopData(char* data)
{
	if (!data)
		return;
	net_pop3_free_state((Pop3UidlHost*) data);
}

PRIVATE void
net_pop3_free_msg_info(ActiveEntry* ce)
{
  Pop3ConData * cd = (Pop3ConData *)ce->con_data;
  int i;
  if (cd->msg_info) {
	for (i=0 ; i<cd->number_of_messages ; i++) {
	  if (cd->msg_info[i].uidl)
		  XP_FREE(cd->msg_info[i].uidl);
	  cd->msg_info[i].uidl = NULL;
	}
	XP_FREE(cd->msg_info);
	cd->msg_info = NULL;
  }
}	

/* km
	This function will read one line and only go on to the next state
	if the first character of that line is a '+' character.  This will
	accomodate pop mail servers that print a banner when the client 
	initiates a connection.  Bug #7165
*/
PRIVATE int
net_pop3_wait_for_start_of_connection_response(ActiveEntry * ce)
{
	Pop3ConData * cd = (Pop3ConData *)ce->con_data;
    char * line;

    ce->status = NET_BufferedReadLine(ce->socket, &line, &cd->data_buffer,
                        &cd->data_buffer_size, &cd->pause_for_read);

    if(ce->status == 0)
      {
        /* this shouldn't really happen, but...
         */
        cd->next_state = cd->next_state_after_response;
        cd->pause_for_read = FALSE; /* don't pause */
        return(0);
      }
    else if(ce->status < 0)
      {
        int rv = PR_GetError();

        if (rv == PR_WOULD_BLOCK_ERROR)
          {
            cd->pause_for_read = TRUE;
            return(0);
          }

        TRACEMSG(("TCP Error: %d", rv));

        ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_TCP_READ_ERROR, rv);

        /* return TCP error
         */
        return MK_TCP_READ_ERROR;
      }
    else if(!line)
      {
         return ce->status; /* wait for line */
      }

    TRACEMSG(("    Rx: %s", line));

	if(*line == '+')
	  {
		cd->command_succeeded = TRUE;
		if(XP_STRLEN(line) > 4)
			StrAllocCopy(cd->command_response, line+4);
		else
			StrAllocCopy(cd->command_response, line);
			
    	cd->next_state = cd->next_state_after_response;
   		cd->pause_for_read = FALSE; /* don't pause */
	  }
	 

    return(1);  /* everything ok */
}

PRIVATE int
net_pop3_wait_for_response(ActiveEntry * ce)
{
	Pop3ConData * cd = (Pop3ConData *)ce->con_data;
    char * line;

    ce->status = NET_BufferedReadLine(ce->socket, &line, &cd->data_buffer,
                        &cd->data_buffer_size, &cd->pause_for_read);

    if(ce->status == 0)
      {
        /* this shouldn't really happen, but...
         */
        cd->next_state = cd->next_state_after_response;
        cd->pause_for_read = FALSE; /* don't pause */
        return(0);
      }
    else if(ce->status < 0)
      {
        int rv = PR_GetError();

        if (rv == PR_WOULD_BLOCK_ERROR)
          {
            cd->pause_for_read = TRUE;
            return(0);
          }

        TRACEMSG(("TCP Error: %d", rv));

        ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_TCP_READ_ERROR, rv);

        /* return TCP error
         */
        return MK_TCP_READ_ERROR;
      }
    else if(!line)
      {
         return ce->status; /* wait for line */
      }

    TRACEMSG(("    Rx: %s", line));

	if(*line == '+')
	  {
		cd->command_succeeded = TRUE;
		if(XP_STRLEN(line) > 4)
			StrAllocCopy(cd->command_response, line+4);
		else
			StrAllocCopy(cd->command_response, line);
	  }
	else
	  {
		cd->command_succeeded = FALSE;
		if(XP_STRLEN(line) > 5)
			StrAllocCopy(cd->command_response, line+5);
		else
			StrAllocCopy(cd->command_response, line);
	  }

    cd->next_state = cd->next_state_after_response;
    cd->pause_for_read = FALSE; /* don't pause */

    return(1);  /* everything ok */
}

PRIVATE int
net_pop3_error(ActiveEntry *ce, int err_code)
{
    Pop3ConData * cd = (Pop3ConData *)ce->con_data;

	ce->URL_s->error_msg = NET_ExplainErrorDetails(err_code, 
							cd->command_response ? cd->command_response :
							XP_GetString( XP_NO_ANSWER ) );

	cd->next_state = POP3_ERROR_DONE;
	cd->pause_for_read = FALSE;

	return(err_code);
}

PRIVATE int
net_pop3_send_command(ActiveEntry *ce, const char * command)
{
    Pop3ConData * cd = (Pop3ConData *)ce->con_data;
	int status;

    status = (int) NET_BlockingWrite(ce->socket,
										command,
										XP_STRLEN(command));

    TRACEMSG(("Pop3 Tx: %s", cd->output_buffer));

	if(status < 0)
	  {
		ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_TCP_WRITE_ERROR, SOCKET_ERRNO);
		cd->next_state = POP3_ERROR_DONE;
		return(MK_TCP_WRITE_ERROR);
	  }

	cd->pause_for_read = TRUE;
	cd->next_state = POP3_WAIT_FOR_RESPONSE;

    return(status);
}

/*
 * POP3 AUTH LOGIN extention
 */

PRIVATE int
net_pop3_send_auth(ActiveEntry *ce)
{
    Pop3ConData * cd = (Pop3ConData *)ce->con_data;

	/* check login response */
	if(!cd->command_succeeded)
		return(net_pop3_error(ce, MK_POP3_SERVER_ERROR));

	PR_snprintf(cd->output_buffer, OUTPUT_BUFFER_SIZE, "AUTH" CRLF);

	cd->next_state_after_response = POP3_AUTH_RESPONSE;

	return net_pop3_send_command(ce, cd->output_buffer);
}

PRIVATE	int
net_pop3_auth_response(ActiveEntry *ce)
{
    Pop3ConData * cd = (Pop3ConData *)ce->con_data;
    char * line;

	if (POP3_AUTH_LOGIN_UNDEFINED & pop3CapabilityFlags)
		pop3CapabilityFlags &= ~POP3_AUTH_LOGIN_UNDEFINED;

	if (!cd->command_succeeded) 
	{
		/* AUTH command not implemented 
		 * no base64 encoded username/password
		 */
		cd->command_succeeded = TRUE;
		pop3CapabilityFlags &= ~POP3_HAS_AUTH_LOGIN;
		cd->next_state = POP3_SEND_USERNAME;
		return 0;
	}

	ce->status = NET_BufferedReadLine(ce->socket, &line, &cd->data_buffer,
                    &cd->data_buffer_size, &cd->pause_for_read);

	if(ce->status == 0) 
	{
		/* this shouldn't really happen, but...
		 */
		cd->next_state = cd->next_state_after_response;
		cd->pause_for_read = FALSE; /* don't pause */
		return(0);
    }
    else if (ce->status < 0) 
	{
        int rv = SOCKET_ERRNO;

		TRACEMSG(("TCP Error: %d", rv));

		ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_TCP_READ_ERROR, rv);

		/* return TCP error
		 */
		return MK_TCP_READ_ERROR;
    } 
	else if (!line) 
		 return ce->status; /* wait for line */

    TRACEMSG(("    Rx: %s", line));

    if (!XP_STRCMP(line, ".")) 
	{
		/* now that we've read all the AUTH responses, decide which 
		 * state to go to next 
		 */
		if (pop3CapabilityFlags & POP3_HAS_AUTH_LOGIN)
			cd->next_state = POP3_AUTH_LOGIN;
		else
			cd->next_state = POP3_SEND_USERNAME;
        cd->pause_for_read = FALSE; /* don't pause */
	}
    else if (!XP_STRCASECMP (line, "LOGIN")) 
		pop3CapabilityFlags |= POP3_HAS_AUTH_LOGIN;

	return 0;
}

PRIVATE int
net_pop3_auth_login(ActiveEntry *ce)
{
    Pop3ConData * cd = (Pop3ConData *)ce->con_data;

	/* check login response */
	if(!cd->command_succeeded) {
		pop3CapabilityFlags &= ~POP3_HAS_AUTH_LOGIN;
		return(net_pop3_error(ce, MK_POP3_SERVER_ERROR));
	}

	PR_snprintf(cd->output_buffer, OUTPUT_BUFFER_SIZE, "AUTH LOGIN" CRLF);

	cd->next_state_after_response = POP3_AUTH_LOGIN_RESPONSE;

	return net_pop3_send_command(ce, cd->output_buffer);

}

PRIVATE int
net_pop3_auth_login_response(ActiveEntry *ce)
{
  Pop3ConData * cd = (Pop3ConData *)ce->con_data;

  if (!cd->command_succeeded) 
	{
	  /* sounds like server does not support auth-skey extension
		 resume regular logon process */
	  /* reset command_succeeded to true */
	  cd->command_succeeded = TRUE;
	  /* reset auth login state */
		pop3CapabilityFlags &= ~POP3_HAS_AUTH_LOGIN;
	}
  else
	{
	  pop3CapabilityFlags |= POP3_HAS_AUTH_LOGIN;
	}
  cd->next_state = POP3_SEND_USERNAME;
  return 0;
}


PRIVATE int
net_pop3_send_username(ActiveEntry *ce)
{
    Pop3ConData * cd = (Pop3ConData *)ce->con_data;

	/* check login response */
	if(!cd->command_succeeded)
		return(net_pop3_error(ce, MK_POP3_SERVER_ERROR));

	if(!cd->username)
		return(net_pop3_error(ce, MK_POP3_USERNAME_UNDEFINED));

	if (POP3_HAS_AUTH_LOGIN & pop3CapabilityFlags) {
		char * str = 
		  POP_Base64Encode(cd->username, 
						   XP_STRLEN(cd->username));
		if (str)
		  {
			PR_snprintf(cd->output_buffer, OUTPUT_BUFFER_SIZE, "%.256s" CRLF,
						str);
			 
		  }
		else
		  {
			return (net_pop3_error(ce, MK_POP3_USERNAME_UNDEFINED));
		  }
	}
	else {
		PR_snprintf(cd->output_buffer, 
					OUTPUT_BUFFER_SIZE, 
					"USER %.256s" CRLF, cd->username);
	}

	cd->next_state_after_response = POP3_SEND_PASSWORD;

	return(net_pop3_send_command(ce, cd->output_buffer));
}

PRIVATE int
net_pop3_send_password(ActiveEntry *ce)
{
	Pop3ConData * cd = (Pop3ConData *)ce->con_data;
	char *password;

	/* check username response */
	if (!cd->command_succeeded)
		return(net_pop3_error(ce, MK_POP3_USERNAME_FAILURE));

	password = cd->password;

	if (password == NULL)
		return(net_pop3_error(ce, MK_POP3_PASSWORD_UNDEFINED));

	if (POP3_HAS_AUTH_LOGIN & pop3CapabilityFlags) {
		char * str = 
		  POP_Base64Encode(password, XP_STRLEN(password));
		if (str)
		  {
			PR_snprintf(cd->output_buffer, 
						OUTPUT_BUFFER_SIZE, "%.256s" CRLF, str);
			
		  }
		else
		  {
			return (net_pop3_error(ce, MK_POP3_PASSWORD_UNDEFINED));
		  }
	}
	else {
		PR_snprintf(cd->output_buffer, 
					OUTPUT_BUFFER_SIZE, "PASS %.256s" CRLF, password);
	}


	if (cd->get_url)
		cd->next_state_after_response = POP3_SEND_GURL;
	else
		cd->next_state_after_response = POP3_SEND_STAT;

	return(net_pop3_send_command(ce, cd->output_buffer));
}

PRIVATE int
net_pop3_send_stat_or_gurl(ActiveEntry *ce, XP_Bool sendStat)
{
    Pop3ConData * cd = (Pop3ConData *)ce->con_data;

	/* check password response */
	if(!cd->command_succeeded)
	  {
		/* The password failed.

		   Sever the connection and go back to the `read password' state,
		   which, upon success, will re-open the connection.  Set a flag
		   which causes the prompt to be different that time (to indicate
		   that the old password was bogus.)

		   But if we're just checking for new mail (biff) then don't bother
		   prompting the user for a password: just fail silently. */
		if (cd->only_check_for_new_mail)
		  return MK_POP3_PASSWORD_UNDEFINED;

		cd->password_failed = TRUE;
		cd->next_state = POP3_ERROR_DONE;	/* close */
		cd->pause_for_read = FALSE;		   /* try again right away */
		pop3CapabilityFlags = POP3_AUTH_LOGIN_UNDEFINED | POP3_XSENDER_UNDEFINED |
							  POP3_GURL_UNDEFINED;
		/*if (cd->pane) {
			MSG_SetUserAuthenticated(MSG_GetMaster(cd->pane), FALSE);
			MSG_SetMailAccountURL(MSG_GetMaster(cd->pane), NULL);
		} */
		/* clear the bogus password in case 
		 * we need to sync with auth smtp password 
		 */
	 
		return 0;
	  }
    else if (net_pop3_password_pending)
      {
		/*
		 * First time with this password.  Record it as a keeper.
		 * (The user's preference might say to save it.)
		  
		FE_RememberPopPassword(ce->window_id, net_pop3_password); */
		net_pop3_password_pending = FALSE;
        /*		if (cd->pane)
			MSG_SetUserAuthenticated(MSG_GetMaster(cd->pane), TRUE); */
      }

	if (sendStat) {
	    XP_STRCPY(cd->output_buffer, "STAT" CRLF);
		cd->next_state_after_response = POP3_GET_STAT;
	}
	else {
	    XP_STRCPY(cd->output_buffer, "GURL" CRLF);
		cd->next_state_after_response = POP3_GURL_RESPONSE;
	}

	return(net_pop3_send_command(ce, cd->output_buffer));
}


PRIVATE int
net_pop3_send_stat(ActiveEntry *ce)
{
	return net_pop3_send_stat_or_gurl(ce, TRUE);
}


PRIVATE int
net_pop3_get_stat(ActiveEntry *ce)
{
	Pop3ConData * cd = (Pop3ConData *)ce->con_data;

	char *num;

	/* check stat response */
	if(!cd->command_succeeded)
		return(net_pop3_error(ce, MK_POP3_PASSWORD_FAILURE));

	/* stat response looks like:  %d %d
	 * The first number is the number of articles
	 * The second number is the number of bytes
	 *
	 *  grab the first and second arg of stat response
	 */
	num = XP_STRTOK(cd->command_response, " ");

	cd->number_of_messages = atol(num);

	num = XP_STRTOK(NULL, " ");

	cd->total_folder_size = (int32) atol(num);

	cd->total_download_size = -1; /* Means we need to calculate it, later. */

	if(cd->number_of_messages <= 0) {
	  /* We're all done.  We know we have no mail. */
	  cd->next_state = POP3_SEND_QUIT;
	  XP_Clrhash(cd->uidlinfo->hash);
	  return(0);
	}

	if (cd->only_check_for_new_mail && !cd->leave_on_server &&
		cd->size_limit < 0) {
	  /* We're just checking for new mail, and we're not playing any games that
		 involve keeping messages on the server.  Therefore, we now know enough
		 to finish up.  If we had no messages, that would have been handled
		 above; therefore, we know we have some new messages. */
	  /* cd->biffstate = MSG_BIFF_NewMail; */
	  cd->next_state = POP3_SEND_QUIT;
	  return(0);
	}


	if (!cd->only_check_for_new_mail) {
      /*	  cd->msg_del_started = MSG_BeginMailDelivery(cd->pane); */
      cd->msg_del_started = 1;
	  if(!cd->msg_del_started)
		{
		  return(net_pop3_error(ce, MK_POP3_MESSAGE_WRITE_ERROR));
		}
	}

	/*
#ifdef POP_ALWAYS_USE_UIDL_FOR_DUPLICATES
	if (net_uidl_command_unimplemented && net_xtnd_xlst_command_unimplemented && net_top_command_unimplemented) 
#else
	if ((net_uidl_command_unimplemented && net_xtnd_xlst_command_unimplemented && net_top_command_unimplemented) ||
		(!cd->only_uidl && !cd->leave_on_server &&
		 (cd->size_limit < 0 || net_top_command_unimplemented)))
#endif */
		 /* We don't need message size or uidl info; go directly to getting
		 messages. */
	/*  cd->next_state = POP3_GET_MSG;
	else */  /* Fix bug 51262 where pop messages kept on server are re-downloaded after unchecking keep on server */
	
	cd->next_state = POP3_SEND_LIST;
	return 0;
}



PRIVATE int
net_pop3_send_gurl(ActiveEntry *ce)
{
	if (pop3CapabilityFlags == POP3_CAPABILITY_UNDEFINED ||
		pop3CapabilityFlags & POP3_GURL_UNDEFINED ||
		pop3CapabilityFlags & POP3_HAS_GURL)
		return net_pop3_send_stat_or_gurl(ce, FALSE);
	else 
		return -1;
}


PRIVATE int
net_pop3_gurl_response(ActiveEntry *ce)
{
    Pop3ConData * cd = (Pop3ConData *)ce->con_data;
	
	if (POP3_GURL_UNDEFINED & pop3CapabilityFlags)
		pop3CapabilityFlags &= ~POP3_GURL_UNDEFINED;

	if (cd->command_succeeded) {
		pop3CapabilityFlags |= POP3_HAS_GURL;
        /*		if (cd->pane)
			MSG_SetMailAccountURL(MSG_GetMaster(cd->pane), cd->command_response); */
	}
	else {
		pop3CapabilityFlags &= ~POP3_HAS_GURL;
	}
	cd->next_state = POP3_SEND_QUIT;
	return 0;
}

PRIVATE int
net_pop3_send_list(ActiveEntry *ce)
{
    Pop3ConData * cd = (Pop3ConData *)ce->con_data;
	cd->msg_info = (Pop3MsgInfo *) XP_ALLOC(sizeof(Pop3MsgInfo) *
											cd->number_of_messages);
	if (!cd->msg_info)
		return(MK_OUT_OF_MEMORY);
	XP_MEMSET(cd->msg_info, 0, sizeof(Pop3MsgInfo) * cd->number_of_messages);
    XP_STRCPY(cd->output_buffer, "LIST" CRLF);
	cd->next_state_after_response = POP3_GET_LIST;
	return(net_pop3_send_command(ce, cd->output_buffer));
}



PRIVATE int
net_pop3_get_list(ActiveEntry *ce)
{
    Pop3ConData * cd = (Pop3ConData *)ce->con_data;
    char * line;
	int32 msg_num;

	/* check list response 
	 * This will get called multiple times
	 * but it's alright since command_succeeded
	 * will remain constant
	 */
	if(!cd->command_succeeded)
		return(net_pop3_error(ce, MK_POP3_LIST_FAILURE));


    ce->status = NET_BufferedReadLine(ce->socket, &line, &cd->data_buffer,
                        &cd->data_buffer_size, &cd->pause_for_read);

    if(ce->status == 0)
      {
        /* this shouldn't really happen, but...
         */
		return(net_pop3_error(ce, MK_POP3_SERVER_ERROR));
      }
    else if(ce->status < 0)
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
         return ce->status; /* wait for line */
      }

    TRACEMSG(("    Rx: %s", line));

	/* remove CRLF */
	XP_StripLine(line);

	/* parse the line returned from the list command 
	 * it looks like
	 * #msg_number #bytes
	 *
	 * list data is terminated by a ".CRLF" line
	 */
	if(!XP_STRCMP(line, "."))
	  {
		cd->next_state = POP3_SEND_UIDL_LIST;
		cd->pause_for_read = FALSE;
		return(0);
	  }

	msg_num = atol(XP_STRTOK(line, " "));

	if(msg_num <= cd->number_of_messages && msg_num > 0)
		cd->msg_info[msg_num-1].size = atol(XP_STRTOK(NULL, " "));

	return(0);
}
/* km
 *
 *
 *  Process state: POP3_START_USE_TOP_FOR_FAKE_UIDL
 *
 *	If we get here then UIDL and XTND are not supported by the mail server.
 * 
 *	Now we will walk backwards, from higher message numbers to lower, 
 *  using the TOP command. Along the way, keep track of messages to down load
 *  by POP3_GET_MSG.
 *
 *  Stop when we reach a msg that we knew about before or we run out of
 *  messages.
 *
 *	There is also conditional code to handle:
 *		BIFF
 *		Pop3ConData->only_uidl == true (fetching one message only)
 *      emptying message hash table if all messages are new
 *		Deleting messages that are marked deleted.
 *
 *
*/

/* this function gets called for each hash table entry.  If it finds a message
   marked delete, then we will have to traverse all of the server messages
   with TOP in order to delete those that are marked delete.


   If UIDL or XTND XLST are supported then this function will never get called.
   
   A pointer to this function is passed to XP_Maphash     -km */

PRIVATE XP_Bool
net_pop3_check_for_hash_messages_marked_delete(XP_HashTable table,
				       						   const void *key, 
				       						   void *value, 
				       						   void *closure)
{
	char valueChar = (char) (int) value;
	if (valueChar == DELETE_CHAR)
	{
		((Pop3ConData *) closure)->delete_server_message_during_top_traversal = TRUE;
		return FALSE;	/* XP_Maphash will stop traversing hash table now */
	}
	
	return TRUE;		/* XP_Maphash will continue traversing the hash */
}

PRIVATE int
send_fake_uidl_top(ActiveEntry *ce)
{
	Pop3ConData * cd = (Pop3ConData *)ce->con_data;

	PR_snprintf(cd->output_buffer, 
		        OUTPUT_BUFFER_SIZE,  
		       "TOP %ld 1" CRLF,
		        cd->current_msg_to_top);

	cd->next_state_after_response = POP3_GET_FAKE_UIDL_TOP;
	cd->pause_for_read = TRUE;
	return(net_pop3_send_command(ce, cd->output_buffer));
}

PRIVATE int
start_use_top_for_fake_uidl(ActiveEntry *ce)
{
	Pop3ConData * cd = (Pop3ConData *)ce->con_data;

	cd->current_msg_to_top = cd->number_of_messages;
	cd->number_of_messages_not_seen_before = 0;
	cd->found_new_message_boundary = FALSE;
	cd->delete_server_message_during_top_traversal = FALSE;
	
	/* may set delete_server_message_during_top_traversal to true */
	XP_Maphash(cd->uidlinfo->hash,
			   net_pop3_check_for_hash_messages_marked_delete, cd);
	
	return (send_fake_uidl_top(ce));
}


PRIVATE int
get_fake_uidl_top(ActiveEntry *ce)
{
    Pop3ConData * cd = (Pop3ConData *)ce->con_data;
    char * line;

	/* check list response 
	 * This will get called multiple times
	 * but it's alright since command_succeeded
	 * will remain constant
	 */
	if(!cd->command_succeeded) {

	  /* UIDL, XTND and TOP are all unsupported for this mail server.
		 Tell the user to join the 20th century.

		 Tell the user this, and refuse to download any messages until they've
		 gone into preferences and turned off the `Keep Mail on Server' and
		 `Maximum Message Size' prefs.  Some people really get their panties
		 in a bunch if we download their mail anyway. (bug 11561)
	   */
	  char *fmt = XP_GetString(XP_THE_POP3_SERVER_DOES_NOT_SUPPORT_UIDL_ETC);
	  char *host = NET_ParseURL(ce->URL_s->address, GET_HOST_PART);
	  ce->URL_s->error_msg = PR_smprintf (fmt, (host ? host : "(null)"));
	  FREEIF(host);
	  cd->next_state = POP3_ERROR_DONE;
	  cd->pause_for_read = FALSE;
	  return -1;

	}


    ce->status = NET_BufferedReadLine(ce->socket, &line, &cd->data_buffer,
                        &cd->data_buffer_size, &cd->pause_for_read);

    if(ce->status == 0)
      {
        /* this shouldn't really happen, but...
         */
		return(net_pop3_error(ce, MK_POP3_SERVER_ERROR));
      }
    else if(ce->status < 0)
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
         return ce->status; /* wait for line */
      }

   	TRACEMSG(("    Rx: %s", line));

	/* remove CRLF */
	XP_StripLine(line);

	if(!XP_STRCMP(line, "."))
	{
		cd->current_msg_to_top--;
		if (!cd->current_msg_to_top || 
		    (cd->found_new_message_boundary &&
			 !cd->delete_server_message_during_top_traversal))
		{
			/* we either ran out of messages or reached the edge of new
			   messages and no messages are marked dele */
			cd->next_state = POP3_GET_MSG;
			cd->pause_for_read = FALSE;
			
			/* if all of the messages are new, toss all hash table entries */
			if (!cd->current_msg_to_top && !cd->found_new_message_boundary)
				XP_Clrhash(cd->uidlinfo->hash);
		}
		else
		{
			/* this message is done, go to the next */
			cd->next_state = POP3_SEND_FAKE_UIDL_TOP;
			cd->pause_for_read = FALSE;
		}
	}
	else
	{
		/* we are looking for a string of the form
		   Message-Id: <199602071806.KAA14787@neon.netscape.com> */
		char *firstToken = XP_STRTOK(line, " ");
		int state = 0;

		if (firstToken && !XP_STRCASECMP(firstToken, "MESSAGE-ID:") )
		{
			char *message_id_token = XP_STRTOK(NULL, " ");
			state = (int) XP_Gethash(cd->uidlinfo->hash, message_id_token, 0);

			if (!cd->only_uidl && message_id_token && (state == 0))
			{	/* we have not seen this message before */
				
				/* if we are only doing a biff, stop here */
				if (cd->only_check_for_new_mail)
				{
                  /* cd->biffstate = MSG_BIFF_NewMail; */
	  				cd->next_state = POP3_SEND_QUIT;
					cd->pause_for_read = FALSE;
				}
				else	/* we will retrieve it and cache it in GET_MSG */
				{
					cd->number_of_messages_not_seen_before++;
					cd->msg_info[cd->current_msg_to_top-1].uidl = XP_STRDUP(message_id_token);
	  				if (!cd->msg_info[cd->current_msg_to_top-1].uidl)
						return MK_OUT_OF_MEMORY;
				}
			}
			else if (cd->only_uidl && message_id_token &&
					 !XP_STRCMP(cd->only_uidl, message_id_token))
			{
					cd->last_accessed_msg = cd->current_msg_to_top - 1;
					cd->found_new_message_boundary = TRUE;
					cd->msg_info[cd->current_msg_to_top-1].uidl = XP_STRDUP(message_id_token);
	  				if (!cd->msg_info[cd->current_msg_to_top-1].uidl)
						return MK_OUT_OF_MEMORY;
			}
			else if (!cd->only_uidl)
			{	/* we have seen this message and we care about the edge,
				   stop looking for new ones */
				if (cd->number_of_messages_not_seen_before != 0)
				{
					cd->last_accessed_msg = cd->current_msg_to_top;	/* -1 ? */
					cd->found_new_message_boundary = TRUE;
					/* we stay in this state so we can process the rest of the
					   lines in the top message */
				}
				else
				{
	  				cd->next_state = POP3_SEND_QUIT;
					cd->pause_for_read = FALSE;
				}
			}
		}
	}
	return 0;
}


/* km
 *
 *	net_pop3_send_xtnd_xlst_msgid
 *
 *  Process state: POP3_SEND_XTND_XLST_MSGID
 *
 *	If we get here then UIDL is not supported by the mail server.
 *  Some mail servers support a similar command:
 *
 *		XTND XLST Message-Id
 *
 * 	Here is a sample transaction from a QUALCOMM server
 
 >>XTND XLST Message-Id
 <<+OK xlst command accepted; headers coming.
 <<1 Message-ID: <3117E4DC.2699@netscape.com>
 <<2 Message-Id: <199602062335.PAA19215@lemon.mcom.com>
 
 * This function will send the xtnd command and put us into the
 * POP3_GET_XTND_XLST_MSGID state
 *
*/
PRIVATE int
net_pop3_send_xtnd_xlst_msgid(ActiveEntry *ce)
{
  Pop3ConData * cd = (Pop3ConData *)ce->con_data;

  if (net_xtnd_xlst_command_unimplemented)
  	return(start_use_top_for_fake_uidl(ce));


  XP_STRCPY(cd->output_buffer, "XTND XLST Message-Id" CRLF);
  cd->next_state_after_response = POP3_GET_XTND_XLST_MSGID;
  cd->pause_for_read = TRUE;
  return(net_pop3_send_command(ce, cd->output_buffer));
}


/* km
 *
 *	net_pop3_get_xtnd_xlst_msgid
 *
 *  This code was created from the net_pop3_get_uidl_list boiler plate.
 *	The difference is that the XTND reply strings have one more token per
 *  string than the UIDL reply strings do.
 *
 */

PRIVATE int
net_pop3_get_xtnd_xlst_msgid(ActiveEntry *ce)
{
    Pop3ConData * cd = (Pop3ConData *)ce->con_data;
    char * line;
	int32 msg_num;

	/* check list response 
	 * This will get called multiple times
	 * but it's alright since command_succeeded
	 * will remain constant
	 */
	if(!cd->command_succeeded) {
	    net_xtnd_xlst_command_unimplemented = TRUE;
		cd->next_state = POP3_START_USE_TOP_FOR_FAKE_UIDL;
		cd->pause_for_read = FALSE;
		return(0);
	}


    ce->status = NET_BufferedReadLine(ce->socket, &line, &cd->data_buffer,
                        &cd->data_buffer_size, &cd->pause_for_read);

    if(ce->status == 0)
      {
        /* this shouldn't really happen, but...
         */
		return(net_pop3_error(ce, MK_POP3_SERVER_ERROR));
      }
    else if(ce->status < 0)
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
         return ce->status; /* wait for line */
      }

    TRACEMSG(("    Rx: %s", line));

	/* remove CRLF */
	XP_StripLine(line);


	/* parse the line returned from the list command 
	 * it looks like
	 * 1 Message-ID: <3117E4DC.2699@netscape.com>
	 *
	 * list data is terminated by a ".CRLF" line
	 */
	if(!XP_STRCMP(line, "."))
	  {
		cd->next_state = POP3_GET_MSG;
		cd->pause_for_read = FALSE;
		return(0);
	  }

	msg_num = atol(XP_STRTOK(line, " "));

	if(msg_num <= cd->number_of_messages && msg_num > 0) {
/*	  char *eatMessageIdToken = XP_STRTOK(NULL, " ");	*/
	  char *uidl = XP_STRTOK(NULL, " ");	/* not really a uidl but a unique token -km */

	  if (!uidl)
		/* This is bad.  The server didn't give us a UIDL for this message.
		   I've seen this happen when somehow the mail spool has a message
		   that contains a header that reads "X-UIDL: \n".  But how that got
		   there, I have no idea; must be a server bug.  Or something. */
		uidl = "";

	  cd->msg_info[msg_num-1].uidl = XP_STRDUP(uidl);
	  if (!cd->msg_info[msg_num-1].uidl)
		  return MK_OUT_OF_MEMORY;
	}

	return(0);
}


PRIVATE int
net_pop3_send_uidl_list(ActiveEntry *ce)
{
  Pop3ConData * cd = (Pop3ConData *)ce->con_data;

  if (net_uidl_command_unimplemented)
  	return(net_pop3_send_xtnd_xlst_msgid(ce));


  XP_STRCPY(cd->output_buffer, "UIDL" CRLF);
  cd->next_state_after_response = POP3_GET_UIDL_LIST;
  cd->pause_for_read = TRUE;
  return(net_pop3_send_command(ce, cd->output_buffer));
}


PRIVATE int
net_pop3_get_uidl_list(ActiveEntry *ce)
{
    Pop3ConData * cd = (Pop3ConData *)ce->con_data;
    char * line;
	int32 msg_num;

	/* check list response 
	 * This will get called multiple times
	 * but it's alright since command_succeeded
	 * will remain constant
	 */
	if(!cd->command_succeeded) {
		cd->next_state = POP3_SEND_XTND_XLST_MSGID;
		cd->pause_for_read = FALSE;
		net_uidl_command_unimplemented = TRUE;
		return(0);

#if 0  /* this if block shows what UIDL used to do in this case */
	  /* UIDL doesn't work so we can't retrieve the message later, and we
	   * can't play games notating how to keep it on the server.  Therefore
	   * just go download the whole thing, and warn the user.
	   */
	  char *host, *fmt, *buf;

	  net_uidl_command_unimplemented = TRUE;

	  fmt = XP_GetString( XP_THE_POP3_SERVER_DOES_NOT_SUPPORT_UIDL_ETC );

	  host = NET_ParseURL(ce->URL_s->address, GET_HOST_PART);
	  XP_ASSERT(host);
	  if (!host)
		  host = "(null)";
	  buf = PR_smprintf (fmt, host);
	  if (!buf)
		  return MK_OUT_OF_MEMORY;
	  FE_Alert (ce->window_id, buf);
	  XP_FREE (buf);

	  /* Free up the msg_info structure, as we use its presence later to
		 decide if we can do UIDL-based games. */
	  net_pop3_free_msg_info(ce);

	  cd->next_state = POP3_GET_MSG;
	  return(0);

#endif /* 0 */
	}


    ce->status = NET_BufferedReadLine(ce->socket, &line, &cd->data_buffer,
                        &cd->data_buffer_size, &cd->pause_for_read);

    if(ce->status == 0)
      {
        /* this shouldn't really happen, but...
         */
		return(net_pop3_error(ce, MK_POP3_SERVER_ERROR));
      }
    else if(ce->status < 0)
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
         return ce->status; /* wait for line */
      }

    TRACEMSG(("    Rx: %s", line));

	/* remove CRLF */
	XP_StripLine(line);

	/* parse the line returned from the list command 
	 * it looks like
	 * #msg_number uidl
	 *
	 * list data is terminated by a ".CRLF" line
	 */
	if(!XP_STRCMP(line, "."))
	  {
		cd->next_state = POP3_GET_MSG;
		cd->pause_for_read = FALSE;
		return(0);
	  }

	msg_num = atol(XP_STRTOK(line, " "));

	if(msg_num <= cd->number_of_messages && msg_num > 0) {
	  char *uidl = XP_STRTOK(NULL, " ");

	  if (!uidl)
		/* This is bad.  The server didn't give us a UIDL for this message.
		   I've seen this happen when somehow the mail spool has a message
		   that contains a header that reads "X-UIDL: \n".  But how that got
		   there, I have no idea; must be a server bug.  Or something. */
		uidl = "";

	  cd->msg_info[msg_num-1].uidl = XP_STRDUP(uidl);
	  if (!cd->msg_info[msg_num-1].uidl)
		  return MK_OUT_OF_MEMORY;
	}

	return(0);
}




/* this function decides if we are going to do a
 * normal RETR or a TOP.  The first time, it also decides the total number
 * of bytes we're probably going to get.
 */
PRIVATE int 
net_pop3_get_msg(ActiveEntry *ce)
{
    Pop3ConData * cd = (Pop3ConData *)ce->con_data;
	char c;
	int i;
	XP_Bool prefBool = FALSE;

	if(cd->last_accessed_msg >= cd->number_of_messages) {
	  /* Oh, gee, we're all done. */
	  cd->next_state = POP3_SEND_QUIT;
	  return 0;
	}

	if (cd->total_download_size < 0) {
	  /* First time.  Figure out how many bytes we're about to get.
		 If we didn't get any message info, then we are going to get
		 everything, and it's easy.  Otherwise, if we only want one
		 uidl, than that's the only one we'll get.  Otherwise, go
		 through each message info, decide if we're going to get that
		 message, and add the number of bytes for it. When a message is too
		 large (per user's preferences) only add the size we are supposed
		 to get. */
	  if (cd->msg_info) {
		cd->total_download_size = 0;
		for (i=0 ; i < cd->number_of_messages ; i++) {
		  c = 0;
		  if (cd->only_uidl) {
			if (cd->msg_info[i].uidl && XP_STRCMP(cd->msg_info[i].uidl,
												  cd->only_uidl) == 0) {
			  /*if (cd->msg_info[i].size > cd->size_limit)
				  cd->total_download_size = cd->size_limit;	*/	/* if more than max, only count max */
			  /*else*/
				  cd->total_download_size = cd->msg_info[i].size;
			  break;
			}
			continue;
		  }
		  if (cd->msg_info[i].uidl)
			c = (char) (int) XP_Gethash(cd->uidlinfo->hash, cd->msg_info[i].uidl, 0);
		  if ((c == KEEP) && !cd->leave_on_server)
		  {		/* This message has been downloaded but kept on server, we no longer want to keep it there */
			if (cd->newuidl == NULL)
			{
				cd->newuidl = XP_HashTableNew(20, XP_StringHash, uidl_cmp);
				if (!cd->newuidl)
					return MK_OUT_OF_MEMORY;
			}
			c = DELETE_CHAR;
			put_hash(cd->uidlinfo, cd->newuidl, cd->msg_info[i].uidl, DELETE_CHAR); /* Mark message to be deleted in new table */
			put_hash(cd->uidlinfo, cd->uidlinfo->hash, cd->msg_info[i].uidl, DELETE_CHAR); /* and old one too */
		  }
		  if ((c != KEEP) && (c != DELETE_CHAR) && (c != TOO_BIG)) {	/* mesage left on server */
			/*if (cd->msg_info[i].size > cd->size_limit)
				cd->total_download_size += cd->size_limit;	*/	/* if more than max, only count max */
			/*else*/
				cd->total_download_size += cd->msg_info[i].size;
		  }
		}
	  } else {
		cd->total_download_size = cd->total_folder_size;
	  }
	 /* cd->only_check_for_new_mail = 0; */
	  if (cd->only_check_for_new_mail) {
        /*		if (cd->total_download_size > 0) cd->biffstate = MSG_BIFF_NewMail;*/
		cd->next_state = POP3_SEND_QUIT;
		return(0);
	  }
	  /* get the amount of available space on the drive
	   * and make sure there is enough
	   */	

	  {
		/* const char* dir = MSG_GetFolderDirectory(MSG_GetPrefs(cd->pane));

		 When checking for disk space available, take in consideration possible database
		changes, therefore ask for a little more than what the message size is.
		Also, due to disk sector sizes, allocation blocks, etc. The space "available" may be greater
		than the actual space usable. 
		if((cd->total_download_size > 0)
            && ((uint32)cd->total_download_size + (uint32) 3096) > FE_DiskSpaceAvailable(ce->window_id, dir))
		  {
			return(net_pop3_error(ce, MK_POP3_OUT_OF_DISK_SPACE));
		  }
*/
	  }
	}


	/* Look at this message, and decide whether to ignore it, get it, just get
	   the TOP of it, or delete it. */

	PREF_GetBoolPref("mail.auth_login", &prefBool);
	if (prefBool && (pop3CapabilityFlags & POP3_HAS_XSENDER ||
					 pop3CapabilityFlags & POP3_XSENDER_UNDEFINED))
		cd->next_state = POP3_SEND_XSENDER;
	else
		cd->next_state = POP3_SEND_RETR;
	cd->truncating_cur_msg = FALSE;
	cd->pause_for_read = FALSE;
	if (cd->msg_info) {
	  Pop3MsgInfo* info = cd->msg_info + cd->last_accessed_msg;
	  if (cd->only_uidl) {
		if (info->uidl == NULL || XP_STRCMP(info->uidl, cd->only_uidl))
			cd->next_state = POP3_GET_MSG;
		else
			cd->next_state = POP3_SEND_RETR;
	  } else {
		c = 0;
		if (cd->newuidl == NULL) {
			cd->newuidl = XP_HashTableNew(20, XP_StringHash, uidl_cmp);
			if (!cd->newuidl)
				return MK_OUT_OF_MEMORY;
		}
		if (info->uidl) {
		  c = (char) (int) XP_Gethash(cd->uidlinfo->hash, info->uidl, 0);
		}
		cd->truncating_cur_msg = FALSE;
		if (c == DELETE_CHAR) {
		  cd->next_state = POP3_SEND_DELE;
		} else if (c == KEEP) {
		  cd->next_state = POP3_GET_MSG;
		} else if ((c != TOO_BIG) && (cd->size_limit > 0) && (info->size > cd->size_limit) &&
				   !net_top_command_unimplemented && (cd->only_uidl == NULL)) {
			/* message is too big */
			cd->truncating_cur_msg = TRUE;
			cd->next_state = POP3_SEND_TOP;
			put_hash(cd->uidlinfo, cd->newuidl, info->uidl, TOO_BIG);
		} else if (c == TOO_BIG) {	/* message previously left on server, see if the
									   max download size has changed, because we may
									   want to download the message this time around.
									   Otherwise ignore the message, we have the header. */
			if ((cd->size_limit > 0) && (info->size <= cd->size_limit))
				XP_Remhash (cd->uidlinfo->hash, (void*) info->uidl);	/* remove from our table, and download */
			else
			{
				cd->truncating_cur_msg = TRUE;
				cd->next_state = POP3_GET_MSG;	/* ignore this message and get next one */
				put_hash(cd->uidlinfo, cd->newuidl, info->uidl, TOO_BIG);
			}
		}
	  }
	  if ((cd->leave_on_server && cd->next_state != POP3_SEND_DELE) ||
		  cd->next_state == POP3_GET_MSG || cd->next_state == POP3_SEND_TOP) {

		/* This is a message we have decided to keep on the server.  Notate
		   that now for the future.  (Don't change the popstate file at all
		   if only_uidl is set; in that case, there might be brand new messages
		   on the server that we *don't* want to mark KEEP; we just want to
		   leave them around until the user next does a GetNewMail.) */

		if (info->uidl && cd->only_uidl == NULL) {
		  if (!cd->truncating_cur_msg)	/* message already marked as too_big */
			put_hash(cd->uidlinfo, cd->newuidl, info->uidl, KEEP);
		}
	  }
	  if (cd->next_state == POP3_GET_MSG) {
		cd->last_accessed_msg++; /* Make sure we check the next message next
									time! */
	  }
	}
	return 0;
}


/* start retreiving just the first 20 lines
 */
PRIVATE int
net_pop3_send_top(ActiveEntry *ce)
{
    Pop3ConData * cd = (Pop3ConData *)ce->con_data;

	XP_ASSERT(!(net_uidl_command_unimplemented && 
	            net_xtnd_xlst_command_unimplemented &&
	            net_top_command_unimplemented) );
	XP_ASSERT(!net_top_command_unimplemented);

    PR_snprintf(cd->output_buffer, 
			   OUTPUT_BUFFER_SIZE,  
			   "TOP %ld 20" CRLF,
			   cd->last_accessed_msg+1);

	cd->next_state_after_response = POP3_TOP_RESPONSE;

	cd->cur_msg_size = -1;

	/* zero the bytes received in message in preparation for
	 * the next
	 */
	cd->bytes_received_in_message = 0;

	return(net_pop3_send_command(ce, cd->output_buffer));
}

/* send the xsender command
 */
PRIVATE int
net_pop3_send_xsender(ActiveEntry *ce)
{
  Pop3ConData * cd = (Pop3ConData *) ce->con_data;
  
  PR_snprintf(cd->output_buffer,
			  OUTPUT_BUFFER_SIZE,
			  "XSENDER %ld" CRLF,
			  cd->last_accessed_msg+1);

  cd->next_state_after_response = POP3_XSENDER_RESPONSE;
  return net_pop3_send_command(ce, cd->output_buffer);
}

PRIVATE int
net_pop3_xsender_response(ActiveEntry *ce)
{
  Pop3ConData * cd = (Pop3ConData *) ce->con_data;

  cd->seenFromHeader = FALSE;
  FREEIF(cd->sender_info);

  if (POP3_XSENDER_UNDEFINED & pop3CapabilityFlags)
	  pop3CapabilityFlags &= ~POP3_XSENDER_UNDEFINED;

  if (cd->command_succeeded) {
	  if (XP_STRLEN (cd->command_response) > 4)
	  {
		  StrAllocCopy(cd->sender_info, cd->command_response);
	  }
	  if (! (POP3_HAS_XSENDER & pop3CapabilityFlags))
		  pop3CapabilityFlags |= POP3_HAS_XSENDER;
  }
  else {
	  pop3CapabilityFlags &= ~POP3_HAS_XSENDER;
  }
  if (cd->truncating_cur_msg)
	  cd->next_state = POP3_SEND_TOP;
  else
	cd->next_state = POP3_SEND_RETR;
  return 0;
}

/* retreive the whole message
 */
PRIVATE int
net_pop3_send_retr(ActiveEntry *ce)
{
    Pop3ConData * cd = (Pop3ConData *)ce->con_data;
	char buf[OUTPUT_BUFFER_SIZE];

    PR_snprintf(cd->output_buffer, 
			   OUTPUT_BUFFER_SIZE,  
			   "RETR %ld" CRLF,
			   cd->last_accessed_msg+1);

	cd->next_state_after_response = POP3_RETR_RESPONSE;

	cd->cur_msg_size = -1;

	/* zero the bytes received in message in preparation for
	 * the next
	 */
	cd->bytes_received_in_message = 0;

	if (cd->only_uidl)
	  {
		/* Display bytes if we're only downloading one message. */
		XP_ASSERT(!cd->graph_progress_bytes_p);
		if (!cd->graph_progress_bytes_p)
		  FE_GraphProgressInit(ce->window_id, ce->URL_s,
							   cd->total_download_size);
		cd->graph_progress_bytes_p = TRUE;
	  }
	else
	  {
		PR_snprintf(buf, OUTPUT_BUFFER_SIZE,
		XP_GetString( XP_RECEIVING_MESSAGE_OF ),
					cd->last_accessed_msg+1,
					cd->number_of_messages);
		NET_Progress(ce->window_id, buf);
	  }

	return(net_pop3_send_command(ce, cd->output_buffer));
}

/* #### should include msgutils.h instead... */
int msg_LineBuffer (const char *net_buffer, int32 net_buffer_size,
						   char **bufferP, uint32 *buffer_sizeP,
						   uint32 *buffer_fpP,
						   XP_Bool convert_newlines_p,
						   int32 (*per_line_fn) (char *line, uint32
												 line_length, void *closure),
						   void *closure);


PRIVATE int32 net_pop3_retr_handle_line(char *line, uint32 line_length,
										void *closure);

static int32 gPOP3parsed_bytes, gPOP3size;
static XP_Bool gPOP3dotFix, gPOP3AssumedEnd;


/*
	To fix a bug where we think the message is over, check the alleged size of the message
	before we assume that CRLF.CRLF is the end.
	return TRUE if end of message is unlikely. parsed bytes is not accurate since we add
	bytes every now and then.
*/

XP_Bool NET_POP3TooEarlyForEnd(int32 len)
{
	if (!gPOP3dotFix)
		return FALSE;	/* fix turned off */
	gPOP3parsed_bytes += len;
	if (gPOP3parsed_bytes >= (gPOP3size - 3))
		return FALSE;
	return TRUE;
}



/* digest the message
 */
PRIVATE int
net_pop3_retr_response(ActiveEntry *ce)
{
    Pop3ConData * cd = (Pop3ConData *)ce->con_data;
	char *buffer;
	int32 buffer_size;
	int32 flags = 0;
	char *uidl = NULL;
	int32 old_bytes_received = ce->bytes_received;
	XP_Bool fix = FALSE;
	
	if(cd->cur_msg_size == -1)
	{
		/* this is the beginning of a message
		 * get the response code and byte size
		 */
		if(!cd->command_succeeded)
			return net_pop3_error(ce, MK_POP3_RETR_FAILURE);

		/* a successful RETR response looks like: #num_bytes Junk
		   from TOP we only get the +OK and data
		 */
		if (cd->truncating_cur_msg)
		{ /* TOP, truncated message */
			cd->cur_msg_size = cd->size_limit;
			/* xxx flags |= MSG_FLAG_PARTIAL; */
		}
		else
			cd->cur_msg_size = atol(XP_STRTOK(cd->command_response, " "));  /* RETR complete message */

        /*	xxx	if (cd->sender_info)
			flags |= MSG_FLAG_SENDER_AUTHED; */

		if(cd->cur_msg_size < 0)
			cd->cur_msg_size = 0;

		if (cd->msg_info && cd->msg_info[cd->last_accessed_msg].uidl)
			uidl = cd->msg_info[cd->last_accessed_msg].uidl;

		gPOP3parsed_bytes = 0;
		gPOP3size = cd->cur_msg_size;
		gPOP3AssumedEnd = FALSE;
		PREF_GetBoolPref("mail.dot_fix", &fix);
		gPOP3dotFix = fix;
		

		TRACEMSG(("Opening message stream: MSG_IncorporateBegin"));
		/* open the message stream so we have someplace
		 * to put the data
		 */
		cd->msg_closure = MSG_IncorporateBegin (ce);

		TRACEMSG(("Done opening message stream!"));
													
		/* if(!cd->msg_closure)
		    return(net_pop3_error(ce, MK_POP3_MESSAGE_WRITE_ERROR)); */
	}

	if (cd->data_buffer_size > 0)
	{
		XP_ASSERT(cd->obuffer_fp == 0); /* must be the first time */
		buffer = cd->data_buffer;
		buffer_size = cd->data_buffer_size;
		cd->data_buffer_size = 0;
	}
	else
	  {
        ce->status = PR_Read(ce->socket, 
							 NET_Socket_Buffer, 
							 NET_Socket_Buffer_Size);
		buffer = NET_Socket_Buffer;
		buffer_size = ce->status;

		cd->pause_for_read = TRUE;

	    if(ce->status == 0)
		{
		    /* should never happen */
			return(net_pop3_error(ce, MK_POP3_SERVER_ERROR));
		}
        else if(ce->status < 0) /* error */
          {
            int err = PR_GetError();
        
            if (err == PR_WOULD_BLOCK_ERROR)
			{
				/*
				(rb) If we have the dot fix, and the server reported the wrong number of bytes there may
				be nothing else to read, so now go ahead and close the message if we saw something
				resembling the end of the message.
				*/
				if (gPOP3dotFix && gPOP3AssumedEnd)
				{
					ce->status = MSG_IncorporateComplete(cd);
					cd->msg_closure = 0;
					buffer_size = 0;
				}
				else
				{
					cd->pause_for_read = TRUE;
					return (0);
				}
			}
			else
			{
				TRACEMSG(("TCP Error: %d", err));

				ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_TCP_READ_ERROR, err);
    
				/* return TCP error
				 */
				return MK_TCP_READ_ERROR;
			}
		}
	}

	if (cd->msg_closure)	/* not done yet */
		ce->status = msg_LineBuffer(buffer, buffer_size,
								&cd->obuffer, &cd->obuffer_size,
								&cd->obuffer_fp, FALSE,
								net_pop3_retr_handle_line, (void *) ce);

	if (ce->status < 0)
	{
		if (cd->msg_closure)
		{
			MSG_IncorporateAbort(cd);
			cd->msg_closure = NULL;
		}
        /*		MSG_AbortMailDelivery(cd->pane); */
		return(net_pop3_error(ce, MK_POP3_MESSAGE_WRITE_ERROR));
	}

	/* normal read. Yay! */
	if (cd->bytes_received_in_message + buffer_size > cd->cur_msg_size)
		buffer_size = cd->cur_msg_size -  cd->bytes_received_in_message;

    cd->bytes_received_in_message += buffer_size;
    ce->bytes_received            += buffer_size;

	if (!cd->msg_closure) /* meaning _handle_line read ".\r\n" at end-of-msg */
	{
		cd->pause_for_read = FALSE;
		if (cd->truncating_cur_msg ||
			(cd->leave_on_server && !(net_uidl_command_unimplemented &&
									  net_xtnd_xlst_command_unimplemented &&
									       net_top_command_unimplemented) ))
		{
		  /* We've retrieved all or part of this message, but we want to
			 keep it on the server.  Go on to the next message. */
			cd->last_accessed_msg++;
			cd->next_state = POP3_GET_MSG;
		} else
		{
			cd->next_state = POP3_SEND_DELE;
		}

		/* if we didn't get the whole message add the bytes that we didn't get
		   to the bytes received part so that the progress percent stays sane.
		 */
		if(cd->bytes_received_in_message < cd->cur_msg_size)
			ce->bytes_received += (cd->cur_msg_size - cd->bytes_received_in_message);
	}

	if (cd->graph_progress_bytes_p)
	  FE_GraphProgress(ce->window_id, ce->URL_s,
					   ce->bytes_received,
					   ce->bytes_received - old_bytes_received,
					   cd->cur_msg_size);

	/* set percent done to portion of total bytes of all messages
	   that we're going to download. */
	if (cd->total_download_size)
		FE_SetProgressBarPercent(ce->window_id, ((ce->bytes_received*100) / cd->total_download_size));

	return(0);
}


PRIVATE int
net_pop3_top_response(ActiveEntry *ce)
{
  Pop3ConData * cd = (Pop3ConData *)ce->con_data;
  if(cd->cur_msg_size == -1 &&  /* first line after TOP command sent */
	 !cd->command_succeeded)	/* and TOP command failed */
	{
	  /* TOP doesn't work so we can't retrieve the first part of this msg.
		 So just go download the whole thing, and warn the user.

		 Note that the progress bar will not be accurate in this case.
		 Oops. #### */
	  char *host, *fmt, *buf;
	  int size;
	  XP_Bool prefBool = FALSE;

	  net_top_command_unimplemented = TRUE;
	  cd->truncating_cur_msg = FALSE;

	  fmt = XP_GetString( XP_THE_POP3_SERVER_DOES_NOT_SUPPORT_THE_TOP_COMMAND );

	  host = NET_ParseURL(ce->URL_s->address, GET_HOST_PART);
	  size = XP_STRLEN(fmt) + XP_STRLEN(host ? host : "(null)") + 100;
	  buf = (char *) XP_ALLOC (size);
	  if (!buf) {
		  FREEIF(host);
		  return MK_OUT_OF_MEMORY;
	  }
	  PR_snprintf (buf, size, fmt, host ? host : "(null)");
	  FE_Alert (ce->window_id, buf);
	  XP_FREE (buf);
	  FREEIF(host);

	  PREF_GetBoolPref ("mail.auth_login", &prefBool);
	  if (prefBool && (POP3_XSENDER_UNDEFINED & pop3CapabilityFlags ||
					   POP3_HAS_XSENDER & pop3CapabilityFlags))
		  cd->next_state = POP3_SEND_XSENDER;
	  else
		  cd->next_state = POP3_SEND_RETR;
	  return(0);
	}

  /* If TOP works, we handle it in the same way as RETR. */
  return net_pop3_retr_response(ce);
}



PRIVATE int32
net_pop3_retr_handle_line(char *line, uint32 line_length, void *closure)
{
  ActiveEntry *ce = (ActiveEntry *) closure;
  Pop3ConData * cd = (Pop3ConData *)ce->con_data;
  int status;

  XP_ASSERT(cd->msg_closure);
  if (!cd->msg_closure)
	  return -1;

  if (cd->sender_info && !cd->seenFromHeader)
  {
	  if (line_length > 6 && !XP_MEMCMP("From: ", line, 6))
	  {
		  /* Zzzzz XP_STRSTR only works with NULL terminated string. Since,
		   * the last character of a line is either a carriage return
		   * or a linefeed. Temporary setting the last character of the
		   * line to NULL and later setting it back should be the right 
		   * thing to do. 
		   */
		  char ch = line[line_length-1];
		  line[line_length-1] = 0;
		  cd->seenFromHeader = TRUE;
          /* 		  if (XP_STRSTR(line, cd->sender_info) == NULL)
			  MSG_ClearSenderAuthedFlag(cd->pane, cd->msg_closure); */
		  line[line_length-1] = ch;
	  }
  }

  
  status = MSG_IncorporateWrite(cd, line, line_length);

  if ((status >= 0) &&
	  (line[0] == '.') &&
	  ((line[1] == CR) || (line[1] == LF)))
  {
	  gPOP3AssumedEnd = TRUE;	/* in case byte count from server is wrong, mark we may have had the end */
	  if (!gPOP3dotFix || (gPOP3parsed_bytes >= (gPOP3size -3)))
	  {
		status = MSG_IncorporateComplete(cd);
		cd->msg_closure = 0;
	  }
  }

  return status;
}


PRIVATE int
net_pop3_send_dele(ActiveEntry *ce)
{
    Pop3ConData * cd = (Pop3ConData *)ce->con_data;

	/* increment the last accessed message since we have now read it
	 */
	cd->last_accessed_msg++;

	PR_snprintf(cd->output_buffer, 
				OUTPUT_BUFFER_SIZE,  
				"DELE %ld" CRLF, 
				cd->last_accessed_msg);
	
	cd->next_state_after_response = POP3_DELE_RESPONSE;
	
	return(net_pop3_send_command(ce, cd->output_buffer));
}

PRIVATE int
net_pop3_dele_response(ActiveEntry *ce)
{
    Pop3ConData * cd = (Pop3ConData *)ce->con_data;
    Pop3UidlHost *host = NULL;
	
	if (cd)
		host = cd->uidlinfo;

	/* the return from the delete will come here
	 */
	if(!cd->command_succeeded)
		return(net_pop3_error(ce, MK_POP3_DELE_FAILURE));


	/*	###chrisf
	the delete succeeded.  Write out state so that we
	keep track of all the deletes which have not yet been
	committed on the server.  Flush this state upon successful
	QUIT.
	
	We will do this by adding each successfully deleted message id
	to a list which we will write out to popstate.dat in 
	net_pop3_write_state(cd).
	*/
	if (host)
	{
		if (cd->msg_info && cd->msg_info[cd->last_accessed_msg-1].uidl) {
			if (cd->newuidl)
				XP_Remhash(cd->newuidl, (void*) cd->msg_info[cd->last_accessed_msg-1].uidl); /* kill message in new hash table */
			else
				XP_Remhash(host->hash, (void*) cd->msg_info[cd->last_accessed_msg-1].uidl);
		}
	}

	cd->next_state = POP3_GET_MSG;

	cd->pause_for_read = FALSE;

	return(0);
}


PRIVATE int
net_pop3_commit_state(ActiveEntry *ce, XP_Bool remove_last_entry)
{
	Pop3ConData * cd = (Pop3ConData *)ce->con_data;
	
	if (remove_last_entry)
	{
		/* now, if we are leaving messages on the server, pull out the last
		   uidl from the hash, because it might have been put in there before
		   we got it into the database. */
		if (cd->msg_info) {
			Pop3MsgInfo* info = cd->msg_info + cd->last_accessed_msg;
			if (info && info->uidl && (cd->only_uidl == NULL) && cd->newuidl) {
				XP_Bool val = XP_Remhash (cd->newuidl, info->uidl);
				XP_ASSERT(val);
			}
		}
	}

	if (cd->newuidl) {
		XP_HashTableDestroy(cd->uidlinfo->hash);
		cd->uidlinfo->hash = cd->newuidl;
		cd->newuidl = NULL;
	}
	if (!cd->only_check_for_new_mail) {
		net_pop3_write_state(cd);
	}
	return 0;
}


/* start a pop3 load
 */
PRIVATE int32
net_Pop3Load (ActiveEntry * ce)
{
	Pop3ConData * cd = XP_NEW(Pop3ConData);
	char* uidl;
	int err = 0;
    int dirlen; 

	XP_MEMSET(cd, 0, sizeof(Pop3ConData));
 	cd->host = NET_ParseURL(ce->URL_s->address, GET_HOST_PART);
	cd->username = NET_ParseURL(ce->URL_s->address, GET_USERNAME_PART | GET_PASSWORD_PART); 
    dirlen = strlen(cd->host) + strlen(cd->username) + 1;
    cd->accountdir = XP_ALLOC(dirlen);
    sprintf(cd->accountdir, "%s@%s", cd->username, cd->host);
    cd->rdf = ce->URL_s->fe_data; 


	if (net_pop3_block)		/* we already have a connection going */
	{
      FREEIF(cd->host);
		FREE(cd);
		return -1;	/* avoid looping back in */
	}
	if(!cd || !cd->host /* guha || !ce->URL_s->internal_url */) {
		FREEIF(cd->host);
		FREEIF(cd);
		return(MK_OUT_OF_MEMORY);
	}

 
     
 
    if (cd->username && strchr(cd->username, ':')) {
      char* temp = cd->username;
      size_t len = strlen(temp);
      char* pw  = strchr(temp, ':');
	  size_t l1 = len - strlen(pw);
      cd->password = XP_STRDUP(&temp[l1+1]);
      temp[l1] = '\0';
      cd->username = XP_STRDUP(temp);
      FREE(temp);
    } else cd->password = NULL;
	if(!cd->username)
	  {
		FREEIF(cd->host);
		FREE(cd);

		ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_POP3_USERNAME_UNDEFINED);
		ce->status = MK_POP3_USERNAME_UNDEFINED;
#ifdef XP_MAC
		FE_Alert(ce->window_id, ce->URL_s->error_msg); /* BEFORE going to the prefs window */
		FE_EditPreference(PREF_Pop3ID);
#endif
		return(MK_POP3_USERNAME_UNDEFINED);
	  }

	/* acquire the semaphore which prevents POP3 from making more connections */
	net_pop3_block = TRUE;

	if(strcasestr(ce->URL_s->address, "?check"))
       cd->only_check_for_new_mail = TRUE;
	
	if(strcasestr(ce->URL_s->address, "?gurl"))
		cd->get_url = TRUE;

	if (!cd->only_check_for_new_mail) {
	  XP_Bool tmp = FALSE;
   /* guha   cd->pane = ce->URL_s->msg_pane;
	  if (!cd->pane)
      {
#ifdef DEBUG_phil
        XP_Trace ("NET_Pop3Load: url->msg_pane NULL for URL: %s\n", ce->URL_s->address);
#endif
	    cd->pane = MSG_FindPane(ce->window_id, MSG_FOLDERPANE); 
      }
	  XP_ASSERT(cd->pane);
	  if (cd->pane == NULL)
	  {
		  net_pop3_block = FALSE;
		  return -1; 
	  }
*/
	  PREF_GetBoolPref("mail.leave_on_server", &(cd->leave_on_server));
      cd->leave_on_server = 1;
	  PREF_GetBoolPref("mail.limit_message_size", &tmp);
	  if (tmp) {
		PREF_GetIntPref("mail.max_size", &(cd->size_limit));
		cd->size_limit *= 1024;
	  } else {
		cd->size_limit = -1;
	  }
	}

	cd->uidlinfo = net_pop3_load_state(cd);

	cd->output_buffer = (char*)XP_ALLOC(OUTPUT_BUFFER_SIZE);
	if(!cd->uidlinfo || !cd->output_buffer)
		goto FAIL;


	ce->con_data = cd;

	/* cd->biffstate = MSG_BIFF_NoMail;	 Return "no mail" unless proven
										   otherwise. */

	uidl = strcasestr(ce->URL_s->address, "?uidl=");
	if (uidl) {
	  uidl += 6;
	  cd->only_uidl = XP_STRDUP(uidl);
	  if (!cd->only_uidl)
		  goto FAIL;
	}

	ce->socket = NULL;

	cd->next_state = POP3_READ_PASSWORD;

    /* acquire the semaphore which prevents biff from interrupting connections */
	net_pop3_block = TRUE;

	err = net_ProcessPop3(ce);
	if (err < 0)
		net_pop3_block = FALSE;
	return (err);

FAIL:
	/* release the semaphore which prevents POP3 from making more connections */
	net_pop3_block = FALSE;
	if (cd->uidlinfo)
		net_pop3_free_state(cd->uidlinfo);
	if (cd->output_buffer)
		XP_FREE(cd->output_buffer);
	FREE(cd);
	return(MK_OUT_OF_MEMORY);
}

/* NET_process_Pop3  will control the state machine that
 * loads messages from a pop3 server
 *
 * returns negative if the transfer is finished or error'd out
 *
 * returns zero or more if the transfer needs to be continued.
 */
PRIVATE int32
net_ProcessPop3 (ActiveEntry *ce)
{

   Pop3ConData * cd = (Pop3ConData *)ce->con_data;
   int oldStatus = 0;

    TRACEMSG(("Entering net_ProcessPop3"));

    cd->pause_for_read = FALSE; /* already paused; reset */

	if(!cd->username)
	  return(net_pop3_error(ce, MK_POP3_USERNAME_UNDEFINED));

    while(!cd->pause_for_read)
      {

		TRACEMSG(("POP3: Entering state: %d", cd->next_state));

        switch(cd->next_state)
          {
		    case POP3_READ_PASSWORD:
			  /* This is a seperate state so that we're waiting for the
				 user to type in a password while we don't actually have
				 a connection to the pop server open; this saves us from
				 having to worry about the server timing out on us while
				 we wait for user input. */
			  {
				char *fmt1 = 0, *fmt2 = 0;

			    /* If we're just checking for new mail (biff) then don't
				   prompt the user for a password; just tell him we don't
				   know whether he has new mail. */
                 
			    if ((cd->only_check_for_new_mail) &&
					(!cd->username))
				  {
					ce->status = MK_POP3_PASSWORD_UNDEFINED;
                    /*	cd->biffstate = MSG_BIFF_Unknown; 
					MSG_SetBiffStateAndUpdateFE(cd->biffstate);	 update old style biff */
					cd->next_state = POP3_FREE;
					cd->pause_for_read = FALSE;
					break;
				  }

				XP_ASSERT(cd->username);

				if (cd->password_failed)
				  fmt2 =
				  XP_GetString( XP_THE_PREVIOUSLY_ENTERED_PASSWORD_IS_INVALID_ETC );
				else if (!cd->password)
				  fmt1 = 
				   XP_GetString( XP_PASSWORD_FOR_POP3_USER );

				if (!cd->password && (fmt1 || fmt2))	/* We need to read a password. */
				  {
					char *password;
					char *host = cd->host;
					size_t len = (XP_STRLEN(fmt1 ? fmt1 : fmt2) +
                                 (host ? XP_STRLEN(host) : 0) + 300) 
								 * sizeof(char);
					char *prompt = (char *) XP_ALLOC (len);

					if (!prompt) {
						FREEIF(host);
						net_pop3_block = FALSE;
						return MK_OUT_OF_MEMORY;
					}
					if (fmt1)
					  PR_snprintf (prompt, len, fmt1, cd->username, cd->host);
					else
					  PR_snprintf (prompt, len, fmt2,
								   (cd->command_response
									? cd->command_response
									: XP_GetString(XP_NO_ANSWER)),
								   cd->username, cd->host);
 
					
					cd->password_failed = FALSE;
					password = FE_PromptPassword
					    (ce->window_id, prompt);
 
					XP_FREE(prompt);

					if (password == NULL)
					{
						net_pop3_block = FALSE;
						return MK_POP3_PASSWORD_UNDEFINED;
					}

					cd->password = XP_STRDUP(password);  

					net_pop3_password_pending = TRUE;
				  }

				XP_ASSERT (cd->username && cd->password);
				if (!cd->username)
				{
					net_pop3_block = FALSE;
					return -1;
				}

				cd->next_state = POP3_START_CONNECT;
				cd->pause_for_read = FALSE;
				break;
			  }

        	case POP3_START_CONNECT:

			    /* Start the thermometer (in cylon mode.) */
			    FE_SetProgressBarPercent(ce->window_id, -1);

            	ce->status = NET_BeginConnect(ce->URL_s->address,
											ce->URL_s->IPAddressString,
                                         	"POP3",
                                         	POP3_PORT,
                                         	&ce->socket,
                                         	HG09439
                                         	&cd->tcp_con_data,
                                         	ce->window_id,
                                         	&ce->URL_s->error_msg,
											 ce->socks_host,
											 ce->socks_port,
                                             ce->URL_s->localIP);
	
				if ((ce->status == MK_UNABLE_TO_CONNECT) ||
					(ce->status == MK_CONNECTION_TIMED_OUT) ||
					(ce->status == MK_CONNECTION_REFUSED))
            	if(ce->socket != NULL)
                	NET_TotalNumberOfOpenConnections++;
   
            	cd->pause_for_read = TRUE;
   
            	if(ce->status == MK_CONNECTED)
                  {
					XP_Bool prefBool = FALSE;

                	cd->next_state = POP3_WAIT_FOR_START_OF_CONNECTION_RESPONSE;
                	
                	/* cd->next_state_after_response = POP3_SEND_USERNAME; */
					PREF_GetBoolPref ("mail.auth_login", &prefBool);
					
					if (prefBool) {
						if (pop3CapabilityFlags & POP3_AUTH_LOGIN_UNDEFINED)
							cd->next_state_after_response = POP3_SEND_AUTH;
						else if (pop3CapabilityFlags & POP3_HAS_AUTH_LOGIN)
							cd->next_state_after_response = POP3_AUTH_LOGIN;
						else
							cd->next_state_after_response = POP3_SEND_USERNAME;
					}
					else
                		cd->next_state_after_response = POP3_SEND_USERNAME;

                	NET_SetReadSelect(ce->window_id, ce->socket);
/*DSR112096 - serious thrashing getting mail by modem if this guy isn't reset properly*/
/*see defect 19736 for more info. I suspect it is a general defect in the backend.    */
#ifdef XP_OS2_FIX
                	ce->con_sock = NULL;  /* set con sock so we can select on it */
#endif
                  }
           	else if(ce->status > -1)
                  {
                	cd->next_state = POP3_FINISH_CONNECT;
                	ce->con_sock = ce->socket;  /* set con sock so we can select on it */
                	NET_SetConnectSelect(ce->window_id, ce->con_sock);
                  }
				else if(ce->status < 0)
				  {
					/* close and clear the socket here
					 * so that we don't try and send a RSET
					 */
            		if(ce->socket != NULL)
					  {
                		if (NET_TotalNumberOfOpenConnections > 0) NET_TotalNumberOfOpenConnections--;
                		NET_ClearConnectSelect(ce->window_id, ce->socket);
                		TRACEMSG(("Closing and clearing socket ce->socket: %d", ce->socket));
                		PR_Close(ce->socket);
						ce->socket = NULL;
						cd->next_state = POP3_ERROR_DONE;
					  }
				  }
            	break;

          case POP3_FINISH_CONNECT:
            	ce->status = NET_FinishConnect(ce->URL_s->address,
                                          	"POP3",
                                          	POP3_PORT,
                                          	&ce->socket,
                                          	&cd->tcp_con_data,
                                          	ce->window_id,
                                          	&ce->URL_s->error_msg,
                                            ce->URL_s->localIP);

            	cd->pause_for_read = TRUE;

            	if(ce->status == MK_CONNECTED)
                  {
					XP_Bool prefBool = FALSE;

					cd->next_state = POP3_WAIT_FOR_START_OF_CONNECTION_RESPONSE;
					PREF_GetBoolPref ("mail.auth_login", &prefBool);
					if (prefBool) {
						if (pop3CapabilityFlags & POP3_AUTH_LOGIN_UNDEFINED)
							cd->next_state_after_response = POP3_SEND_AUTH;
						else if (pop3CapabilityFlags & POP3_HAS_AUTH_LOGIN)
							cd->next_state_after_response = POP3_AUTH_LOGIN;
						else
							cd->next_state_after_response = POP3_SEND_USERNAME;
					}
					else
                		cd->next_state_after_response = POP3_SEND_USERNAME;
                	NET_ClearConnectSelect(ce->window_id, ce->con_sock);
                	NET_SetReadSelect(ce->window_id, ce->socket);
                	ce->con_sock = NULL;  /* set con sock so we can select on it */
                  }
            	else if(ce->status > -1)
                  {

                	/* unregister the old CE_SOCK from the select list
                 	 * and register the new value in the case that it changes
                 	 */
                	if(ce->con_sock != ce->socket)
                  	  {
                    	NET_ClearConnectSelect(ce->window_id, ce->con_sock);
                    	ce->con_sock = ce->socket;
                    	NET_SetConnectSelect(ce->window_id, ce->con_sock);
                  	  }
                  }
				else if(ce->status < 0)
				  {
					/* close and clear the socket here
					 * so that we don't try and send a RSET
					 */
                	if (NET_TotalNumberOfOpenConnections > 0) NET_TotalNumberOfOpenConnections--;
                	NET_ClearConnectSelect(ce->window_id, ce->socket);
                	TRACEMSG(("Closing and clearing socket ce->socket: %d", ce->socket));
                	PR_Close(ce->socket);
					ce->socket = NULL;
					cd->next_state = POP3_ERROR_DONE;
				  }
            	break;

			case POP3_WAIT_FOR_RESPONSE:
				ce->status = net_pop3_wait_for_response(ce);
				break;
				
			case POP3_WAIT_FOR_START_OF_CONNECTION_RESPONSE:
				net_pop3_wait_for_start_of_connection_response(ce);
				break;

			case POP3_SEND_AUTH:
				ce->status = net_pop3_send_auth(ce);
				break;

			case POP3_AUTH_RESPONSE:
				ce->status = net_pop3_auth_response(ce);
				break;

			case POP3_AUTH_LOGIN:
				ce->status = net_pop3_auth_login(ce);
				break;

			case POP3_AUTH_LOGIN_RESPONSE:
				ce->status = net_pop3_auth_login_response(ce);
				break;

			case POP3_SEND_USERNAME:
				NET_Progress(ce->window_id,
				XP_GetString(XP_CONNECT_HOST_CONTACTED_SENDING_LOGIN_INFORMATION) );
				ce->status = net_pop3_send_username(ce);
				break;

			case POP3_SEND_PASSWORD:
				ce->status = net_pop3_send_password(ce);
				break;

			case POP3_SEND_GURL:
				ce->status = net_pop3_send_gurl(ce);
				break;

			case POP3_GURL_RESPONSE:
				ce->status = net_pop3_gurl_response(ce);
				break;

			case POP3_SEND_STAT:
				ce->status = net_pop3_send_stat(ce);
				break;

			case POP3_GET_STAT:
				ce->status = net_pop3_get_stat(ce);
				break;

			case POP3_SEND_LIST:
				ce->status = net_pop3_send_list(ce);
				break;

			case POP3_GET_LIST:
				ce->status = net_pop3_get_list(ce);
				break;

			case POP3_SEND_UIDL_LIST:
				ce->status = net_pop3_send_uidl_list(ce);
				break;

			case POP3_GET_UIDL_LIST:
				ce->status = net_pop3_get_uidl_list(ce);
				break;
				
			case POP3_SEND_XTND_XLST_MSGID:
				ce->status = net_pop3_send_xtnd_xlst_msgid(ce);
				break;

			case POP3_GET_XTND_XLST_MSGID:
				ce->status = net_pop3_get_xtnd_xlst_msgid(ce);
				break;
			
			case POP3_START_USE_TOP_FOR_FAKE_UIDL:
				ce->status = start_use_top_for_fake_uidl(ce);
				break;

    		case POP3_SEND_FAKE_UIDL_TOP:
    			ce->status = send_fake_uidl_top(ce);
    			break;
    			
    		case POP3_GET_FAKE_UIDL_TOP:
    			ce->status = get_fake_uidl_top(ce);
    			break;
    
			case POP3_GET_MSG:
				ce->status = net_pop3_get_msg(ce);
				break;

			case POP3_SEND_TOP:
				ce->status = net_pop3_send_top(ce);
				break;

			case POP3_TOP_RESPONSE:
				ce->status = net_pop3_top_response(ce);
				break;

			case POP3_SEND_XSENDER:
				ce->status = net_pop3_send_xsender(ce);
				break;

			case POP3_XSENDER_RESPONSE:
				ce->status = net_pop3_xsender_response(ce);
				break;

			case POP3_SEND_RETR:
				ce->status = net_pop3_send_retr(ce);
				break;

			case POP3_RETR_RESPONSE:
				ce->status = net_pop3_retr_response(ce);
				break;

			case POP3_SEND_DELE:
				ce->status = net_pop3_send_dele(ce);
				break;

			case POP3_DELE_RESPONSE:
				ce->status = net_pop3_dele_response(ce);
				break;

			case POP3_SEND_QUIT:
				/* attempt to send a server quit command.  Since this means
				   everything went well, this is a good time to update the
				   status file and the FE's biff state.
				 */
				if (!cd->only_uidl) {
                  if (cd->only_check_for_new_mail) {
					/*  MSG_SetBiffStateAndUpdateFE(cd->biffstate);	 update old style biff 
				 } else {
					/* We don't want to pop up a warning message any more (see bug 54116),
					   so instead we put the "no new messages" or "retrieved x new messages"
					   in the status line.  Unfortunately, this tends to be running
					   in a progress pane, so we try to get the real pane and
					   show the message there. */
					  MWContext* context = ce->window_id;
                      /*					  if (cd->pane) {
						  MSG_Pane* parentpane = MSG_GetParentPane(cd->pane);
						  if (parentpane)
							  context = MSG_GetContext(parentpane);
					  } */
					  if (cd->total_download_size <= 0) {
						  /* There are no new messages.  */
						  if (context)
							  NET_Progress(context, XP_GetString(MK_POP3_NO_MESSAGES));
					  }
					  else {
						  /* at least 1 message was queued to download */
						  char *statusTemplate = XP_GetString (MK_MSG_DOWNLOAD_COUNT);
						  char *statusString = PR_smprintf (statusTemplate,  cd->last_accessed_msg, cd->number_of_messages);
						  if (context)
							  NET_Progress(context, statusString);
						  FREEIF(statusString);
						  /* MSG_SetBiffStateAndUpdateFE(MSG_BIFF_NewMail); */
					  }
				  }
				}
    			XP_STRCPY(cd->output_buffer, "QUIT" CRLF);
				oldStatus = ce->status;
				ce->status = net_pop3_send_command(ce, cd->output_buffer);
				if (oldStatus == MK_INTERRUPTED)
					cd->pause_for_read = FALSE;	/* aborting connection, finish clean */
				cd->next_state = POP3_WAIT_FOR_RESPONSE;
#ifdef POP_ALWAYS_USE_UIDL_FOR_DUPLICATES
				cd->next_state_after_response = POP3_QUIT_RESPONSE;
#else
				cd->next_state_after_response = POP3_DONE;
#endif
				break;

#ifdef POP_ALWAYS_USE_UIDL_FOR_DUPLICATES
			case POP3_QUIT_RESPONSE:
				if(cd->command_succeeded)
				{
					/*	the QUIT succeeded.  We can now flush the state in popstate.dat which
						keeps track of any uncommitted DELE's */
					
					/* here we need to clear the hash of all our 
					   uncommitted deletes */
				    /*
					if (cd->uidlinfo && cd->uidlinfo->uncommitted_deletes)
						XP_Clrhash (cd->uidlinfo->uncommitted_deletes);*/

					cd->next_state = POP3_DONE;

				}
				else
				{
					cd->next_state = POP3_ERROR_DONE;
				}
				break;
#endif

			case POP3_DONE:
				net_pop3_commit_state(ce, FALSE);
				if(ce->socket != NULL)
                  {
                	NET_ClearReadSelect(ce->window_id, ce->socket);
                	TRACEMSG(("Closing and clearing sock ce->socket: %d", 
							  ce->socket));
                	PR_Close(ce->socket);
                	if (NET_TotalNumberOfOpenConnections > 0) NET_TotalNumberOfOpenConnections--;
                	ce->socket = NULL;
                  }

                /*				if(cd->msg_del_started)
					MSG_EndMailDelivery(cd->pane);
				else if (cd->pane)
					MSG_GetNextURL(cd->pane); */

				cd->next_state = POP3_FREE;
				break;

			case POP3_INTERRUPTED:
				{
    				XP_STRCPY(cd->output_buffer, "QUIT" CRLF);
					NET_BlockingWrite(ce->socket, cd->output_buffer,
									  XP_STRLEN(cd->output_buffer));
					PR_Shutdown(ce->socket, PR_SHUTDOWN_SEND); /* make sure QUIT get send
														       * before closing down the socket
													      	   */
					cd->pause_for_read = FALSE;
					cd->next_state = POP3_ERROR_DONE;
					net_pop3_block = FALSE;
				}
				break;

			case POP3_ERROR_DONE:

				/*  write out the state */
				net_pop3_commit_state(ce, TRUE);
				
            	if(ce->socket != NULL)
                  {
#if 0
				    /* attempt to send a server reset command
				     * so that messages will not be marked as read

					 #### turns out we never need to do this -- this causes
					 our DELE commands to not be honored, but that turns out
					 to never be the right thing, since we never issue a
					 DELE command until the messages are known to be safely
					 on disk.
				     */
					XP_STRCPY(cd->output_buffer, "RSET" CRLF);
					net_pop3_send_command(ce, cd->output_buffer);
#endif /* 0 */

                	NET_ClearReadSelect(ce->window_id, ce->socket);
                	NET_ClearConnectSelect(ce->window_id, ce->socket);
                	TRACEMSG(("Closing and clearing socket ce->socket: %d",
							  ce->socket));
                	PR_Close(ce->socket);
                	if (NET_TotalNumberOfOpenConnections > 0) NET_TotalNumberOfOpenConnections--;
                	ce->socket = NULL;
                  }

				if(cd->msg_closure)
				  {
					MSG_IncorporateAbort(cd);
					cd->msg_closure = NULL;
                    /* MSG_AbortMailDelivery(cd->pane); */
			 	  }

				if(cd->msg_del_started)
				  {
					char *statusTemplate = XP_GetString (MK_MSG_DOWNLOAD_COUNT);
					char *statusString = PR_smprintf (statusTemplate,  cd->last_accessed_msg, cd->number_of_messages);
					MWContext* context = ce->window_id;
                    /*					if (cd->pane) {
					  MSG_Pane* parentpane = MSG_GetParentPane(cd->pane);
					  if (parentpane) {
						context = MSG_GetContext(parentpane);
					   } 
					} */
					XP_ASSERT (!cd->password_failed);
                    /*					MSG_AbortMailDelivery(cd->pane); 
					if (context)
						NET_Progress(context, statusString); */
					FREEIF(statusString);
				  }

				if (cd->password_failed)
				  /* We got here because the password was wrong, so go
					 read a new one and re-open the connection. */
				  cd->next_state = POP3_READ_PASSWORD;
				else
				  /* Else we got a "real" error, so finish up. */
				  cd->next_state = POP3_FREE;

				cd->pause_for_read = FALSE;
				break;

			case POP3_FREE:
			    if (cd->newuidl) XP_HashTableDestroy(cd->newuidl);
			    net_pop3_free_state(cd->uidlinfo);
				if (cd->graph_progress_bytes_p) {
				  /* Only destroy it if we have initialized it. */
				  FE_GraphProgressDestroy(ce->window_id, ce->URL_s,
										  cd->cur_msg_size,
										  ce->bytes_received);
				}
				net_pop3_free_msg_info(ce);
				FREEIF(cd->only_uidl);
				FREEIF(cd->output_buffer);
				FREEIF(cd->obuffer);
				FREEIF(cd->data_buffer);
				FREEIF(cd->command_response);
				FREEIF(cd->sender_info);
				FREE(ce->con_data);

				/* release the semaphore which prevents POP3 from creating more connections */
				net_pop3_block = FALSE;

				if (oldStatus == MK_INTERRUPTED)
					return MK_INTERRUPTED;	/* Make sure everyone knows we got canceled */
				return(-1);
				break;

			default:
				XP_ASSERT(0);

	      }  /* end switch */

       if((ce->status < 0) && (cd->next_state != POP3_FREE))
          {
            cd->pause_for_read = FALSE;
            cd->next_state = POP3_ERROR_DONE;
          }
 
	  }  /* end while */
	  if (oldStatus == MK_INTERRUPTED)
		  ce->status = MK_INTERRUPTED;	/* Make sure everyone knows we got canceled */

	return(ce->status);

}

/* abort the connection in progress
 */
PRIVATE int32
net_InterruptPop3(ActiveEntry * ce)
{
    Pop3ConData * cd = (Pop3ConData *)ce->con_data;

	TRACEMSG(("net_InterruptPop3 called"));

	cd->next_state = POP3_SEND_QUIT; /* interrupt does not give enough time for the quit
										command to be executed on the server, leaving us
										with a bunch of messages to be re-downloaded.
										Make a graceful quit instead.
										POP3_INTERRUPTED; */

	ce->status = MK_INTERRUPTED;

	return(net_ProcessPop3(ce));
}

PRIVATE void
net_CleanupPop3(void)
{
}

MODULE_PRIVATE void
NET_InitPop3Protocol(void)
{
    static NET_ProtoImpl pop3_proto_impl;
    pop3_proto_impl.init = net_Pop3Load;
    pop3_proto_impl.process = net_ProcessPop3;
    pop3_proto_impl.interrupt = net_InterruptPop3;
    pop3_proto_impl.resume = NULL;
    pop3_proto_impl.cleanup = net_CleanupPop3;
    NET_RegisterProtocolImplementation(&pop3_proto_impl, POP3_TYPE_URL);
	NET_InitMailboxProtocol();
}

extern void RDF_StartMessageDelivery (void* rdf) ;
extern void RDF_AddMessageLine(void* rdf, const char* line, int32 length);
extern void RDF_FinishMessageDelivery (void* rdf) ;

void *MSG_IncorporateBegin (ActiveEntry *ce) {
  Pop3ConData * cd = (Pop3ConData *)ce->con_data;
  void* rdf = cd->rdf;
  RDF_StartMessageDelivery(rdf);  
  return (void*)1;

}

int MSG_IncorporateWrite (Pop3ConData *cd,   const char *block, int32 length) {  
  void* rdf = cd->rdf;
  RDF_AddMessageLine(rdf, block, length);
  return 1;
}



int MSG_IncorporateComplete(Pop3ConData *cd) {
  void* rdf = cd->rdf;
  RDF_FinishMessageDelivery(rdf);
  return 1;
}

int MSG_IncorporateAbort (Pop3ConData *cd)  {
  return 0;
}

int
msg_GrowBuffer (uint32 desired_size, uint32 element_size, uint32 quantum,
				char **buffer, uint32 *size)
{
  if (*size <= desired_size)
	{
	  char *new_buf;
	  uint32 increment = desired_size - *size;
	  if (increment < quantum) /* always grow by a minimum of N bytes */
		increment = quantum;

#ifdef TESTFORWIN16
	  if (((*size + increment) * (element_size / sizeof(char))) >= 64000)
		{
		  /* Make sure we don't choke on WIN16 */
		  XP_ASSERT(0);
		  return MK_OUT_OF_MEMORY;
		}
#endif /* DEBUG */

	  new_buf = (*buffer
				 ? (char *) XP_REALLOC (*buffer, (*size + increment)
										* (element_size / sizeof(char)))
				 : (char *) XP_ALLOC ((*size + increment)
									  * (element_size / sizeof(char))));
	  if (! new_buf)
		return MK_OUT_OF_MEMORY;
	  *buffer = new_buf;
	  *size += increment;
	}
  return 0;
}

/* Take the given buffer, tweak the newlines at the end if necessary, and
   send it off to the given routine.  We are guaranteed that the given
   buffer has allocated space for at least one more character at the end. */
static int
msg_convert_and_send_buffer(char* buf, uint32 length, XP_Bool convert_newlines_p,
							int32 (*per_line_fn) (char *line,
												  uint32 line_length,
												  void *closure),
							void *closure)
{
  /* Convert the line terminator to the native form.
   */
  char* newline;

  XP_ASSERT(buf && length > 0);
  if (!buf || length <= 0) return -1;
  newline = buf + length;
  XP_ASSERT(newline[-1] == CR || newline[-1] == LF);
  if (newline[-1] != CR && newline[-1] != LF) return -1;

  if (!convert_newlines_p)
	{
	}
#if (LINEBREAK_LEN == 1)
  else if ((newline - buf) >= 2 &&
		   newline[-2] == CR &&
		   newline[-1] == LF)
	{
	  /* CRLF -> CR or LF */
	  buf [length - 2] = LINEBREAK[0];
	  length--;
	}
  else if (newline > buf + 1 &&
		   newline[-1] != LINEBREAK[0])
	{
	  /* CR -> LF or LF -> CR */
	  buf [length - 1] = LINEBREAK[0];
	}
#else
  else if (((newline - buf) >= 2 && newline[-2] != CR) ||
		   ((newline - buf) >= 1 && newline[-1] != LF))
	{
	  /* LF -> CRLF or CR -> CRLF */
	  length++;
	  buf[length - 2] = LINEBREAK[0];
	  buf[length - 1] = LINEBREAK[1];
	}
#endif

  return (*per_line_fn)(buf, length, closure);
}

int
msg_LineBuffer (const char *net_buffer, int32 net_buffer_size,
				char **bufferP, uint32 *buffer_sizeP, uint32 *buffer_fpP,
				XP_Bool convert_newlines_p,
				int32 (*per_line_fn) (char *line, uint32 line_length,
									  void *closure),
				void *closure)
{
  int status = 0;
  if (*buffer_fpP > 0 && *bufferP && (*bufferP)[*buffer_fpP - 1] == CR &&
	  net_buffer_size > 0 && net_buffer[0] != LF) {
	/* The last buffer ended with a CR.  The new buffer does not start
	   with a LF.  This old buffer should be shipped out and discarded. */
	XP_ASSERT(*buffer_sizeP > *buffer_fpP);
	if (*buffer_sizeP <= *buffer_fpP) return -1;
	status = msg_convert_and_send_buffer(*bufferP, *buffer_fpP,
										 convert_newlines_p,
										 per_line_fn, closure);
	if (status < 0) return status;
	*buffer_fpP = 0;
  }
  while (net_buffer_size > 0)
	{
	  const char *net_buffer_end = net_buffer + net_buffer_size;
	  const char *newline = 0;
	  const char *s;


	  for (s = net_buffer; s < net_buffer_end; s++)
		{
		  /* Move forward in the buffer until the first newline.
			 Stop when we see CRLF, CR, or LF, or the end of the buffer.
			 *But*, if we see a lone CR at the *very end* of the buffer,
			 treat this as if we had reached the end of the buffer without
			 seeing a line terminator.  This is to catch the case of the
			 buffers splitting a CRLF pair, as in "FOO\r\nBAR\r" "\nBAZ\r\n".
		   */
		  if (*s == CR || *s == LF)
			{
			  newline = s;
			  if (newline[0] == CR)
				{
				  if (s == net_buffer_end - 1)
					{
					  /* CR at end - wait for the next character. */
					  newline = 0;
					  break;
					}
				  else if (newline[1] == LF)
					/* CRLF seen; swallow both. */
					newline++;
				}
			  newline++;
			  break;
			}
		}

	  /* Ensure room in the net_buffer and append some or all of the current
		 chunk of data to it. */
	  {
		const char *end = (newline ? newline : net_buffer_end);
		uint32 desired_size = (end - net_buffer) + (*buffer_fpP) + 1;

		if (desired_size >= (*buffer_sizeP))
		  {
			status = msg_GrowBuffer (desired_size, sizeof(char), 1024,
									 bufferP, buffer_sizeP);
			if (status < 0) return status;
		  }
		XP_MEMCPY ((*bufferP) + (*buffer_fpP), net_buffer, (end - net_buffer));
		(*buffer_fpP) += (end - net_buffer);
	  }

	  /* Now *bufferP contains either a complete line, or as complete
		 a line as we have read so far.

		 If we have a line, process it, and then remove it from `*bufferP'.
		 Then go around the loop again, until we drain the incoming data.
	   */
	  if (!newline)
		return 0;

	  status = msg_convert_and_send_buffer(*bufferP, *buffer_fpP,
										   convert_newlines_p,
										   per_line_fn, closure);
	  if (status < 0) return status;

	  net_buffer_size -= (newline - net_buffer);
	  net_buffer = newline;
	  (*buffer_fpP) = 0;
	}
  return 0;
}


struct MimeEncoderData {

  /* Buffer for the base64 encoder. */
  unsigned char in_buffer[3];
  int32 in_buffer_count;
	
  int32 current_column, line_byte_count;
  
  /* Where to write the encoded data */
  int (*write_buffer) (const char *buf, int32 size, void *closure);
  void *closure;
};

int pop_mime_encode_base64_buffer (MimeEncoderData *data, const char *buffer, int32 size);
MimeEncoderData* pop_mime_encoder_init (int (*output_fn) (const char *, int32, void *),   void *closure);
int PopMimeEncoderDestroy (MimeEncoderData *data, XP_Bool abort_p);
int net_buffer_output_fn ( const char *buf, int32 size, void *closure);


int
PopMimeEncoderDestroy (MimeEncoderData *data, XP_Bool abort_p)
{
  int status = 0;

  /* If we're uuencoding, we have our own finishing routine. */
  
  /* Since Base64 (and uuencode) output needs to do some buffering to get 
	 a multiple of three bytes on each block, there may be a few bytes 
	 left in the buffer after the last block has been written.  We need to
	 flush those out now.
   */


  if (!abort_p &&
	  data->in_buffer_count > 0)
	{
	  char buf2 [6];
	  char *buf = buf2 + 2;
	  char *out = buf;
	  int j;
	  /* fixed bug 55998, 61302, 61866
	   * type casting to uint32 before shifting
	   */
	  uint32 n = ((uint32) data->in_buffer[0]) << 16;
	  if (data->in_buffer_count > 1)
		n = n | (((uint32) data->in_buffer[1]) << 8);

	  buf2[0] = CR;
	  buf2[1] = LF;

	  for (j = 18; j >= 0; j -= 6)
		{
		  unsigned int k = (n >> j) & 0x3F;
		  if (k < 26)       *out++ = k      + 'A';
		  else if (k < 52)  *out++ = k - 26 + 'a';
		  else if (k < 62)  *out++ = k - 52 + '0';
		  else if (k == 62) *out++ = '+';
		  else if (k == 63) *out++ = '/';
		  else abort ();
		}

	  /* Pad with equal-signs. */
	  if (data->in_buffer_count == 1)
		buf[2] = '=';
	  buf[3] = '=';

	  if (data->current_column >= 72)
		status = data->write_buffer (buf2, 6, data->closure);
	  else
		status = data->write_buffer (buf,  4, data->closure);
	}


  XP_FREE (data);
  return status;
}

MimeEncoderData *
pop_mime_encoder_init (int (*output_fn) (const char *, int32, void *),
				   void *closure)
{
  MimeEncoderData *data = XP_NEW(MimeEncoderData);
  if (!data) return 0;
  XP_MEMSET(data, 0, sizeof(*data));
  data->write_buffer = output_fn;
  data->closure = closure;
  return data;
}

typedef struct {
  char *buffer;
  int32 size;
  int32 pos;
} BufferStruct;


char * POP_Base64Encode (char *src, int32 srclen)
{
  BufferStruct bs;
  MimeEncoderData *encoder_data = NULL;

  XP_ASSERT (src);
  if (!src)
    return NULL;
  else if (srclen == 0)
    return XP_STRDUP("");

  XP_MEMSET (&bs, 0, sizeof (BufferStruct));
  encoder_data = pop_mime_encoder_init(net_buffer_output_fn, (void *) &bs);
  if (!encoder_data)
    return NULL;

  if (pop_mime_encode_base64_buffer(encoder_data, src, srclen) < 0)
    {
      PopMimeEncoderDestroy(encoder_data, FALSE);
      XP_FREEIF(bs.buffer);
      return NULL;
    }
  
  PopMimeEncoderDestroy(encoder_data, FALSE);
  /* caller must free the returned pointer to prevent
   * memory leak problem.
   */
  return bs.buffer;
}

int
pop_mime_encode_base64_buffer (MimeEncoderData *data, const char *buffer, int32 size)
{
  int status = 0;
  const unsigned char *in = (unsigned char *) buffer;
  const unsigned char *end = in + size;
  char out_buffer[80];
  char *out = out_buffer;
  uint32 i = 0, n = 0;
  uint32 off;

  if (size == 0)
	return 0;
  else if (size < 0)
	{
	  XP_ASSERT(0);
	  return -1;
	}


  /* If this input buffer is too small, wait until next time. */
  if (size < (3 - data->in_buffer_count))
	{
	  XP_ASSERT(size < 3 && size > 0);
	  data->in_buffer[data->in_buffer_count++] = buffer[0];
	  if (size > 1)
		data->in_buffer[data->in_buffer_count++] = buffer[1];
	  XP_ASSERT(data->in_buffer_count < 3);
	  return 0;
	}


  /* If there are bytes that were put back last time, take them now.
   */
  i = 0;
  if (data->in_buffer_count > 0) n = data->in_buffer[0];
  if (data->in_buffer_count > 1) n = (n << 8) + data->in_buffer[1];
  i = data->in_buffer_count;
  data->in_buffer_count = 0;

  /* If this buffer is not a multiple of three, put one or two bytes back.
   */
  off = ((size + i) % 3);
  if (off)
	{
	  data->in_buffer[0] = buffer [size - off];
	  if (off > 1)
		data->in_buffer [1] = buffer [size - off + 1];
	  data->in_buffer_count = off;
	  size -= off;
	  XP_ASSERT (! ((size + i) % 3));
	  end = (unsigned char *) (buffer + size);
	}

  /* Populate the out_buffer with base64 data, one line at a time.
   */
  while (in < end)
	{
	  int32 j;

	  while (i < 3)
		{
		  n = (n << 8) | *in++;
		  i++;
		}
	  i = 0;

	  for (j = 18; j >= 0; j -= 6)
		{
		  unsigned int k = (n >> j) & 0x3F;
		  if (k < 26)       *out++ = k      + 'A';
		  else if (k < 52)  *out++ = k - 26 + 'a';
		  else if (k < 62)  *out++ = k - 52 + '0';
		  else if (k == 62) *out++ = '+';
		  else if (k == 63) *out++ = '/';
		  else abort ();
		}

	  data->current_column += 4;
	  if (data->current_column >= 72)
		{
		  /* Do a linebreak before column 76.  Flush out the line buffer. */
		  data->current_column = 0;
		  *out++ = '\015';
		  *out++ = '\012';
		  status = data->write_buffer (out_buffer, (out - out_buffer),
									   data->closure);
		  out = out_buffer;
		  if (status < 0) return status;
		}
	}

  /* Write out the unwritten portion of the last line buffer. */
  if (out > out_buffer)
	{
	  status = data->write_buffer (out_buffer, (out - out_buffer),
								   data->closure);
	  if (status < 0) return status;
	}

  return 0;
}

int
net_buffer_output_fn ( const char *buf, int32 size, void *closure)
{
  BufferStruct *bs = (BufferStruct *) closure;
  /* if the size greater or equal to the available buffer size
   * reallocate the buffer
   */
  PR_ASSERT (buf && bs && size > 0);
  if ( !buf || !bs || size <= 0 )
	return -1;

  if (size >= bs->size - bs->pos)
	{
	  int32 len;
	  char *newBuffer;
	  
	  len = (bs->size << 1) - bs->pos + size + 1; /* null terminated */
	  if (bs->buffer)
		newBuffer = PR_Realloc (bs->buffer, len);
	  else
		newBuffer = PR_Malloc(len);
	  if (!newBuffer)
		return MK_OUT_OF_MEMORY;
	  memset(newBuffer+bs->pos, 0, len-bs->pos);
	  bs->size = len;
	  bs->buffer = newBuffer;
	}
  memcpy (bs->buffer+bs->pos, buf, size);
  bs->pos += size;
  return 0;
}



/* Stick the mailbox stuff here. The mailbox stuff in mkmailbox.c is too ... */


typedef struct _MBoxConData {
  FILE *mbox;
  int32 offset;
  int32 status;
  NET_StreamClass *stream;
  int32 pos;
  char* buff;
} MBoxConData;


extern void* getTranslator (char* url);
extern FILE *getPopMBox(void* db);

int32
net_ProcessMBox (ActiveEntry * ce) {
  MBoxConData *cd = (MBoxConData *)ce->con_data;
  if ((cd->status == 1) && (cd->stream)) { 
	memset(cd->buff, '\0', 100000);
    fseek(cd->mbox, cd->offset, SEEK_SET); 
    while (fgets(cd->buff, 100000, cd->mbox)) {
	  cd->offset = ftell(cd->mbox);
      if (!startsWith("From ", cd->buff)) { 
        (*(cd->stream->put_block))(cd->stream, cd->buff, strlen(cd->buff));  
      } else break;
      memset(cd->buff, '\0', 100000);
	  fseek(cd->mbox, cd->offset, SEEK_SET);
    }
    (*cd->stream->complete)(cd->stream);
    XP_FREE(cd->buff);
    cd->status = 0; 
    XP_FREE(cd);
	XP_FREE(ce->URL_s->content_type);
	ce->URL_s->content_type = NULL;
    ce->con_data = NULL;
  }
  return -1;
}

char* normalizeMTs (char* mt) {
	if (strcmp(mt, "text/html") ==0) {
		return TEXT_HTML;
	} if (strcmp(mt, "text/plain") ==0) {
		return TEXT_PLAIN;
	} if (strcmp(mt, "text") ==0) {
		return TEXT_PLAIN;
	} else return mt;
}

void
ShowMailFolder (NET_StreamClass *stream, char* url) {
  char* buff = XP_ALLOC(2000);
  /* this really sucks. The right way is to have some javascript on the page that
     spits this out. Then it will be really customizable */
  sprintf(buff, "<html>\n<body marginwidth=0 marginheight=0>\n<object  title=\"Summary\" target=\"messages\" width=100%% height=100%% type=builtin/tree data=\"mailbox://%s\">\n<param name=\"title\" value=\"Summary\">\n<param name=\"Column\" value=\"mail:From\">\n<param name=\"Column\" value=\"mail:Subject\">\n<param name=\"Column\" value=\"mail:Date\">\n</object>", &url[17]);
  (*stream->put_block)(stream, buff, strlen(buff));
  sprintf(buff, "</body>\n</html>\n\n");
  (*stream->put_block)(stream, buff, strlen(buff));
  XP_FREE(buff);
    (*stream->complete)(stream);
}

PRIVATE int32
net_MBoxLoad (ActiveEntry * ce)
{
  /* displaying mailbox items */
    void* db;
	char* offset = strchr(ce->URL_s->address, '?') ;
    if (!offset) {
      NET_StreamClass *stream;
      	StrAllocCopy(ce->URL_s->content_type, TEXT_HTML);
        stream = NET_StreamBuilder(1,  ce->URL_s, (MWContext*) ce->window_id); 
        ShowMailFolder(stream, ce->URL_s->address);
        return -1;
    } else {
      MBoxConData  *cd = (MBoxConData *)XP_NEW(MBoxConData);
      char* mbox = XP_ALLOC(100);
      PRBool mimeTypeSet = 0;
      memset(mbox, '\0', 100);
	  offset = offset+1;
      memcpy(mbox, ce->URL_s->address, offset-(ce->URL_s->address+1));
      sscanf(offset, "%d",&cd->offset);
      db = getTranslator(mbox);
      cd->mbox = getPopMBox(db);
      cd->status = 1; 
      cd->buff = XP_ALLOC(100000);
      memset(cd->buff, '\0', 100000);
      fseek(cd->mbox, cd->offset, SEEK_SET); 
      StrAllocCopy(ce->URL_s->content_type, MESSAGE_RFC822);
      cd->stream = NET_StreamBuilder(1,  ce->URL_s, (MWContext*) ce->window_id); 
      ce->con_data = cd;      
      XP_FREE(mbox);
      return net_ProcessMBox(ce);
    }
}


int32
net_InterruptMBox (ActiveEntry *ce) {
  MBoxConData* cd = (MBoxConData*)ce->con_data;
  cd->status = 0;
  return 0;
}

int32 
net_CleanupMBox (void) {
  return 0;
}
  
/* NET_process_Pop3  will control the state machine that
 * loads messages from a pop3 server
 *
 * returns negative if the transfer is finished or error'd out
 *
 * returns zero or more if the transfer needs to be continued.
 */

MODULE_PRIVATE void
NET_InitMailboxProtocol(void)
{
    static NET_ProtoImpl mbox_proto_impl;
    mbox_proto_impl.init = net_MBoxLoad;
    mbox_proto_impl.process = net_ProcessMBox;
    mbox_proto_impl.interrupt = net_InterruptMBox;
    mbox_proto_impl.resume = NULL;
    mbox_proto_impl.cleanup = net_CleanupMBox;
    NET_RegisterProtocolImplementation(&mbox_proto_impl, MAILBOX_TYPE_URL);
}




#endif /* MOZILLA_CLIENT */
