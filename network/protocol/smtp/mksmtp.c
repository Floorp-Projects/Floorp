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
 * state machine to speak SMTP
 */

/* Please leave outside of ifdef for windows precompiled headers */
#include "rosetta.h"
#include "mkutils.h"
#include "netutils.h"

#if defined(MOZILLA_CLIENT) || defined(LIBNET_SMTP)
#if defined(MOZ_MAIL_NEWS) || defined(MOZ_MAIL_COMPOSE)
#include "mkgeturl.h"
#include "mksmtp.h"
#include "mime.h"
#include "glhist.h"
#include "mktcp.h"
#include "mkparse.h"
#include "msgcom.h"
#include "msgnet.h"
#include "xp_time.h"
#include "xp_thrmo.h"
#include "merrors.h"
#include "ssl.h"
#include "imap.h"

#include "xp_error.h"
#include "prefapi.h"
#include "libi18n.h"

#ifdef AUTH_SKEY_DEFINED
extern int btoa8(char *out, char*in);
#endif

extern void NET_SetPopPassword2(const char *password);
extern void net_free_write_post_data_object(struct WritePostDataData *obj);
MODULE_PRIVATE char* NET_MailRelayHost(MWContext *context);

/* for XP_GetString() */
#include "xpgetstr.h"
extern int XP_PROGRESS_MAILSENT;
extern int MK_COULD_NOT_GET_USERS_MAIL_ADDRESS;
extern int MK_COULD_NOT_LOGIN_TO_SMTP_SERVER;
extern int MK_ERROR_SENDING_DATA_COMMAND;
extern int MK_ERROR_SENDING_FROM_COMMAND;
extern int MK_ERROR_SENDING_MESSAGE;
extern int MK_ERROR_SENDING_RCPT_COMMAND;
extern int MK_OUT_OF_MEMORY;
extern int MK_SMTP_SERVER_ERROR;
extern int MK_TCP_READ_ERROR;
extern int XP_MESSAGE_SENT_WAITING_MAIL_REPLY;
extern int MK_MSG_DELIV_MAIL;
extern int MK_MSG_NO_SMTP_HOST;
extern int MK_MIME_NO_RECIPIENTS;

extern int MK_POP3_USERNAME_UNDEFINED;
extern int MK_POP3_PASSWORD_UNDEFINED;
extern int XP_PASSWORD_FOR_POP3_USER;
extern int XP_RETURN_RECEIPT_NOT_SUPPORT;
extern int MK_SENDMAIL_BAD_TLS;

#define SMTP_PORT 25

/* definitions of state for the state machine design
 */
#define SMTP_RESPONSE               0
#define SMTP_START_CONNECT          1
#define SMTP_FINISH_CONNECT         2
#define SMTP_LOGIN_RESPONSE         3
#define SMTP_SEND_HELO_RESPONSE     4
#define SMTP_SEND_VRFY_RESPONSE     5
#define SMTP_SEND_MAIL_RESPONSE     6
#define SMTP_SEND_RCPT_RESPONSE     7
#define SMTP_SEND_DATA_RESPONSE	    8
#define SMTP_SEND_POST_DATA	    	9
#define SMTP_SEND_MESSAGE_RESPONSE  10
#define SMTP_DONE                   11
#define SMTP_ERROR_DONE             12
#define SMTP_FREE                   13
#define SMTP_EXTN_LOGIN_RESPONSE	14
#define SMTP_SEND_EHLO_RESPONSE		15

#define SMTP_SEND_AUTH_LOGIN_USERNAME 16
#define SMTP_SEND_AUTH_LOGIN_PASSWORD 17
#define SMTP_AUTH_LOGIN_RESPONSE      18
#define SMTP_AUTH_RESPONSE			19

HG08747

/* structure to hold data pertaining to the active state of
 * a transfer in progress.
 *
 */
typedef struct _SMTPConData {
    int     next_state;       			/* the next state or action to be taken */
	int     next_state_after_response;  /* the state after the response one */
    Bool    pause_for_read;   			/* Pause now for next read? */
#ifdef XP_WIN
	Bool    calling_netlib_all_the_time;
#endif
	char   *response_text;
	int     response_code;
    char   *data_buffer;
    int32  data_buffer_size;
	char   *address_copy;
	char   *mail_to_address_ptr;
    int     mail_to_addresses_left;
	TCP_ConData * tcp_con_data;
	int     continuation_response;
	char   *hostname;
	char   *verify_address;
	void   *write_post_data_data;      /* a data object for the 
										* WritePostData function
									 	*/
	int32   total_amt_written;
	uint32  total_message_size;
	unsigned long last_time;
	XP_Bool	ehlo_dsn_enabled;
    XP_Bool auth_login_enabled;
HG60917
} SMTPConData;

/* macro's to simplify variable names */
#define CD_NEXT_STATE        	      cd->next_state
#define CD_NEXT_STATE_AFTER_RESPONSE  cd->next_state_after_response
#define CD_PAUSE_FOR_READ    	      cd->pause_for_read
#define CD_RESPONSE_TXT			      cd->response_text
#define CD_RESPONSE_CODE              cd->response_code
#define CD_DATA_BUFFER				  cd->data_buffer
#define CD_DATA_BUFFER_SIZE			  cd->data_buffer_size
#define CD_ADDRESS_COPY			      cd->address_copy
#define CD_MAIL_TO_ADDRESS_PTR        cd->mail_to_address_ptr
#define CD_MAIL_TO_ADDRESSES_LEFT     cd->mail_to_addresses_left
#define CD_TCP_CON_DATA				  cd->tcp_con_data
#define CD_CONTINUATION_RESPONSE	  cd->continuation_response
#define CD_HOSTNAME	  				  cd->hostname
#define CD_VERIFY_ADDRESS			  cd->verify_address
#define CD_TOTAL_AMT_WRITTEN          cd->total_amt_written
#define CD_TOTAL_MESSAGE_SIZE         cd->total_message_size
#define CD_EHLO_DSN_ENABLED			  cd->ehlo_dsn_enabled

#define CD_AUTH_LOGIN_ENABLED         cd->auth_login_enabled
HG82493

#define CE_URL_S            cur_entry->URL_s
#define CE_SOCK             cur_entry->socket
#define CE_CON_SOCK         cur_entry->con_sock
#define CE_STATUS           cur_entry->status
#define CE_WINDOW_ID        cur_entry->window_id
#define CE_BYTES_RECEIVED   cur_entry->bytes_received
#define CE_FORMAT_OUT       cur_entry->format_out

PRIVATE char *net_smtp_relay_host=0;
PRIVATE char *net_smtp_password=0;

/* forward decl */
PRIVATE int32 net_ProcessMailto (ActiveEntry *cur_entry);

/* fix Mac warning of missing prototype */
MODULE_PRIVATE char *
NET_MailRelayHost(MWContext *context);

MODULE_PRIVATE char *
NET_MailRelayHost(MWContext *context)
{
    if (net_smtp_relay_host)
		return net_smtp_relay_host;
	else
		return "";	/* caller now checks for empty string and returns MK_MSG_NO_SMTP_HOST */
}

PUBLIC void
NET_SetMailRelayHost(char * host)
{
	char * at = NULL;

	/*
	** If we are called with data like "fred@bedrock.com", then we will
	** help the user by ignoring the stuff before the "@".  People with
	** @ signs in their host names will be hosed.  They also can't possibly
	** be current happy internet users.
	*/
	if (host) at = XP_STRCHR(host, '@');
	if (at != NULL) host = at + 1;
	StrAllocCopy(net_smtp_relay_host, host);
}

/*
 * gets user domian name out from FE_UsersMailAddress()
 */
PRIVATE const char *
net_smtp_get_user_domain_name()
{
	const char *mail_addr, *at_sign = NULL;
	mail_addr = FE_UsersMailAddress();
	at_sign = XP_STRCHR(mail_addr, '@');
	return (at_sign ? at_sign+1 : mail_addr);
}

/* RFC 1891 -- extended smtp value encoding scheme

  5. Additional parameters for RCPT and MAIL commands

     The extended RCPT and MAIL commands are issued by a client when it wishes to request a DSN from the
     server, under certain conditions, for a particular recipient. The extended RCPT and MAIL commands are
     identical to the RCPT and MAIL commands defined in [1], except that one or more of the following parameters
     appear after the sender or recipient address, respectively. The general syntax for extended SMTP commands is
     defined in [4]. 

     NOTE: Although RFC 822 ABNF is used to describe the syntax of these parameters, they are not, in the
     language of that document, "structured field bodies". Therefore, while parentheses MAY appear within an
     emstp-value, they are not recognized as comment delimiters. 

     The syntax for "esmtp-value" in [4] does not allow SP, "=", control characters, or characters outside the
     traditional ASCII range of 1- 127 decimal to be transmitted in an esmtp-value. Because the ENVID and
     ORCPT parameters may need to convey values outside this range, the esmtp-values for these parameters are
     encoded as "xtext". "xtext" is formally defined as follows: 

     xtext = *( xchar / hexchar ) 

     xchar = any ASCII CHAR between "!" (33) and "~" (126) inclusive, except for "+" and "=". 

	; "hexchar"s are intended to encode octets that cannot appear
	; as ASCII characters within an esmtp-value.

		 hexchar = ASCII "+" immediately followed by two upper case hexadecimal digits 

	When encoding an octet sequence as xtext:

	+ Any ASCII CHAR between "!" and "~" inclusive, except for "+" and "=",
		 MAY be encoded as itself. (A CHAR in this range MAY instead be encoded as a "hexchar", at the
		 implementor's discretion.) 

	+ ASCII CHARs that fall outside the range above must be encoded as
		 "hexchar". 

 */
/* caller must free the return buffer */
PRIVATE char *
esmtp_value_encode(char *addr)
{
	char *buffer = XP_ALLOC(512); /* esmpt ORCPT allow up to 500 chars encoded addresses */
	char *bp = buffer, *bpEnd = buffer+500;
	int len, i;

	if (!buffer) return NULL;

	*bp=0;
	if (! addr || *addr == 0) /* this will never happen */
		return buffer;

	for (i=0, len=XP_STRLEN(addr); i < len && bp < bpEnd; i++)
	{
		if (*addr >= 0x21 && 
			*addr <= 0x7E &&
			*addr != '+' &&
			*addr != '=')
		{
			*bp++ = *addr++;
		}
		else
		{
			PR_snprintf(bp, bpEnd-bp, "+%.2X", ((int)*addr++));
			bp += XP_STRLEN(bp);
		}
	}
	*bp=0;
	return buffer;
}

/*
 * gets the response code from the nntp server and the
 * response line
 *
 * returns the TCP return code from the read
 */
PRIVATE int
net_smtp_response (ActiveEntry * cur_entry)
{
    char * line;
	char cont_char;
    SMTPConData * cd = (SMTPConData *)cur_entry->con_data;
	int err = 0;

    CE_STATUS = NET_BufferedReadLine(CE_SOCK, &line, &CD_DATA_BUFFER,
                    						&CD_DATA_BUFFER_SIZE, &CD_PAUSE_FOR_READ);

    if(CE_STATUS == 0)
      {
        CD_NEXT_STATE = SMTP_ERROR_DONE;
        CD_PAUSE_FOR_READ = FALSE;
		CE_URL_S->error_msg = NET_ExplainErrorDetails(MK_SMTP_SERVER_ERROR,
													  CD_RESPONSE_TXT);
		CE_STATUS = MK_SMTP_SERVER_ERROR;
        return(MK_SMTP_SERVER_ERROR);
      }

    /* if TCP error of if there is not a full line yet return
     */
    if(CE_STATUS < 0)
	  {
		HG22864
		CE_URL_S->error_msg =
		  NET_ExplainErrorDetails(MK_TCP_READ_ERROR, SOCKET_ERRNO);

        /* return TCP error
         */
        return MK_TCP_READ_ERROR;
	  }
	else if(!line)
	  {
         return CE_STATUS;
	  }

    TRACEMSG(("SMTP Rx: %s\n", line));

	cont_char = ' '; /* default */
    sscanf(line, "%d%c", &CD_RESPONSE_CODE, &cont_char);

     if(CD_CONTINUATION_RESPONSE == -1)
       {
         if (cont_char == '-')  /* begin continuation */
             CD_CONTINUATION_RESPONSE = CD_RESPONSE_CODE;

         if(XP_STRLEN(line) > 3)
         	StrAllocCopy(CD_RESPONSE_TXT, line+4);
       }
     else
       {    /* have to continue */
         if (CD_CONTINUATION_RESPONSE == CD_RESPONSE_CODE && cont_char == ' ')
             CD_CONTINUATION_RESPONSE = -1;    /* ended */

         StrAllocCat(CD_RESPONSE_TXT, "\n");
         if(XP_STRLEN(line) > 3)
             StrAllocCat(CD_RESPONSE_TXT, line+4);
       }

     if(CD_CONTINUATION_RESPONSE == -1)  /* all done with this response? */
       {
         CD_NEXT_STATE = CD_NEXT_STATE_AFTER_RESPONSE;
         CD_PAUSE_FOR_READ = FALSE; /* don't pause */
       }

    return(0);  /* everything ok */
}

PRIVATE int
net_smtp_login_response(ActiveEntry *cur_entry)
{
    SMTPConData * cd = (SMTPConData *)cur_entry->con_data;
	char buffer[356];

    if(CD_RESPONSE_CODE != 220)
	  {
		CE_URL_S->error_msg = NET_ExplainErrorDetails(MK_COULD_NOT_LOGIN_TO_SMTP_SERVER);
		return(MK_COULD_NOT_LOGIN_TO_SMTP_SERVER);
	  }


	PR_snprintf(buffer, sizeof(buffer), "HELO %.256s" CRLF, 
				net_smtp_get_user_domain_name());
	 
	TRACEMSG(("Tx: %s", buffer));

    CE_STATUS = (int) NET_BlockingWrite(CE_SOCK, buffer, XP_STRLEN(buffer));

    CD_NEXT_STATE = SMTP_RESPONSE;
    CD_NEXT_STATE_AFTER_RESPONSE = SMTP_SEND_HELO_RESPONSE;
    CD_PAUSE_FOR_READ = TRUE;

    return(CE_STATUS);
}
    

PRIVATE int
net_smtp_extension_login_response(ActiveEntry *cur_entry)
{
    SMTPConData * cd = (SMTPConData *)cur_entry->con_data;
	char buffer[356];

    if(CD_RESPONSE_CODE != 220)
	  {
		CE_URL_S->error_msg = NET_ExplainErrorDetails(MK_COULD_NOT_LOGIN_TO_SMTP_SERVER);
		return(MK_COULD_NOT_LOGIN_TO_SMTP_SERVER);
	  }

	PR_snprintf(buffer, sizeof(buffer), "EHLO %.256s" CRLF, 
				net_smtp_get_user_domain_name());

	TRACEMSG(("Tx: %s", buffer));

    CE_STATUS = (int) NET_BlockingWrite(CE_SOCK, buffer, XP_STRLEN(buffer));

    CD_NEXT_STATE = SMTP_RESPONSE;
    CD_NEXT_STATE_AFTER_RESPONSE = SMTP_SEND_EHLO_RESPONSE;
    CD_PAUSE_FOR_READ = TRUE;

    return(CE_STATUS);
}
    

PRIVATE int
net_smtp_send_helo_response(ActiveEntry *cur_entry)
{
    SMTPConData * cd = (SMTPConData *)cur_entry->con_data;
	char buffer[620];
	const char *mail_add = FE_UsersMailAddress();

	/* don't check for a HELO response because it can be bogus and
	 * we don't care
	 */

	if(!mail_add)
	  {
		CE_URL_S->error_msg = NET_ExplainErrorDetails(MK_COULD_NOT_GET_USERS_MAIL_ADDRESS);
		return(MK_COULD_NOT_GET_USERS_MAIL_ADDRESS);
	  }

	if(CD_VERIFY_ADDRESS)
	  {
		PR_snprintf(buffer, sizeof(buffer), "VRFY %.256s" CRLF, CD_VERIFY_ADDRESS);
	  }
	else
	  {
		/* else send the MAIL FROM: command */
		char *s = MSG_MakeFullAddress (NULL, mail_add);
		if (!s)
		  {
			CE_URL_S->error_msg = NET_ExplainErrorDetails(MK_OUT_OF_MEMORY);
			return(MK_OUT_OF_MEMORY);
		  }
		if (CE_URL_S->msg_pane) {
			if (MSG_RequestForReturnReceipt(CE_URL_S->msg_pane)) {
				if (CD_EHLO_DSN_ENABLED) {
					PR_snprintf(buffer, sizeof(buffer), 
								"MAIL FROM:<%.256s> RET=FULL ENVID=NS40112696JT" CRLF,
								s);
				}
				else {
					FE_Alert (CE_WINDOW_ID, XP_GetString(XP_RETURN_RECEIPT_NOT_SUPPORT));
					PR_snprintf(buffer, sizeof(buffer), "MAIL FROM:<%.256s>" CRLF, s);
				}
			}
			else if (MSG_SendingMDNInProgress(CE_URL_S->msg_pane)) {
				PR_snprintf(buffer, sizeof(buffer), "MAIL FROM:<%.256s>" CRLF, "");
			}
			else {
				PR_snprintf(buffer, sizeof(buffer), "MAIL FROM:<%.256s>" CRLF, s);
			}
		}
		else {
			PR_snprintf(buffer, sizeof(buffer), "MAIL FROM:<%.256s>" CRLF, s);
		}
		XP_FREE (s);
	  }

	TRACEMSG(("Tx: %s", buffer));
    CE_STATUS = (int) NET_BlockingWrite(CE_SOCK, buffer, XP_STRLEN(buffer));

    CD_NEXT_STATE = SMTP_RESPONSE;

	if(CD_VERIFY_ADDRESS)
    	CD_NEXT_STATE_AFTER_RESPONSE = SMTP_SEND_VRFY_RESPONSE;
	else
    	CD_NEXT_STATE_AFTER_RESPONSE = SMTP_SEND_MAIL_RESPONSE;
    CD_PAUSE_FOR_READ = TRUE;

    return(CE_STATUS);
}


PRIVATE int
net_smtp_send_ehlo_response(ActiveEntry *cur_entry)
{
  SMTPConData * cd = (SMTPConData *)cur_entry->con_data;

  if (CD_RESPONSE_CODE != 250) {
	/* EHLO not implemented */
	char buffer[384];

HG85890

	PR_snprintf(buffer, sizeof(buffer), "HELO %.256s" CRLF, 
				net_smtp_get_user_domain_name());

	TRACEMSG(("Tx: %s", buffer));

    CE_STATUS = (int) NET_BlockingWrite(CE_SOCK, buffer, XP_STRLEN(buffer));

    CD_NEXT_STATE = SMTP_RESPONSE;
    CD_NEXT_STATE_AFTER_RESPONSE = SMTP_SEND_HELO_RESPONSE;
    CD_PAUSE_FOR_READ = TRUE;
	return (CE_STATUS);
  }
  else {
	char *ptr = NULL;
HG09714

	ptr = strcasestr(CD_RESPONSE_TXT, "DSN");
	CD_EHLO_DSN_ENABLED = (ptr && XP_TO_UPPER(*(ptr-1)) != 'X');
	/* should we use auth login */
	PREF_GetBoolPref("mail.auth_login", &(CD_AUTH_LOGIN_ENABLED));
	if (CD_AUTH_LOGIN_ENABLED) {
	  /* okay user has set to use skey
		 let's see does the server have the capability */
	  CD_AUTH_LOGIN_ENABLED = (NULL != strcasestr(CD_RESPONSE_TXT, "AUTH LOGIN"));
	  if (!CD_AUTH_LOGIN_ENABLED)
		  CD_AUTH_LOGIN_ENABLED = (NULL != strcasestr(CD_RESPONSE_TXT, "AUTH=LOGIN"));	/* check old style */
	}

	HG90967
	
	CD_NEXT_STATE = CD_NEXT_STATE_AFTER_RESPONSE = SMTP_AUTH_RESPONSE;
	
	HG59852 
	return (CE_STATUS);
  }
}


PRIVATE int
net_smtp_auth_login_response(ActiveEntry *cur_entry)
{
  SMTPConData * cd = (SMTPConData *)cur_entry->con_data;

  switch (CD_RESPONSE_CODE/100) {
  case 2:
	  {
#ifdef MOZ_MAIL_NEWS
		  char *pop_password = (char *)NET_GetPopPassword();
		  CD_NEXT_STATE = SMTP_SEND_HELO_RESPONSE;
		  if (pop_password == NULL)
			NET_SetPopPassword2(net_smtp_password);
		  if ( IMAP_GetPassword() == NULL )
			  IMAP_SetPassword(net_smtp_password);
		  XP_FREEIF(pop_password);
#else /* MOZ_MAIL_NEWS */
          XP_ASSERT(0);
#endif /* MOZ_MAIL_NEWS */
	  }
	break;
  case 3:
	CD_NEXT_STATE = SMTP_SEND_AUTH_LOGIN_PASSWORD;
	break;
  case 5:
  default:
	  {
		char* pop_username = (char *) NET_GetPopUsername();
#if defined(SingleSignon)
		char *username = 0;
		char host[256];
		int len = 256;
#endif
		/* NET_GetPopUsername () returns pointer to the cached
		 * username. It did *NOT* alloc a new string
		 */
		XP_FREEIF(net_smtp_password);
#if defined(SingleSignon)
		/*
		 * need to alloc a new string for username because call to
		 * SI_PromptUsernameAndPassword below will attempt to free
		 * it if it is not null
		 */
		StrAllocCopy(username, pop_username); /* alloc a new string */
                PREF_GetCharPref("network.hosts.smtp_server", host, &len);
		SI_RemoveUser(host, pop_username, TRUE);
		/* prefill prompt with previous username/passwords if any */
		if (SI_PromptUsernameAndPassword
			(cur_entry->window_id, " ",
			&username, &net_smtp_password, host)) {
#else
		if (FE_PromptUsernameAndPassword(cur_entry->window_id,
			NULL, &pop_username, &net_smtp_password)) {
#endif
            CD_NEXT_STATE = SMTP_SEND_AUTH_LOGIN_USERNAME;
#if defined(SingleSignon)
			XP_FREEIF(username);
#else
			/* FE_PromptUsernameAndPassword() always alloc a new string
			 * for pop_username. The caller has to free it.
			 */
			XP_FREEIF(pop_username);
#endif
        }
        else {
			/* User hit cancel, but since the client and server both say 
			 * they want auth login we're just going to return an error 
			 * and not let the msg be sent to the server
			 */
			CE_STATUS = MK_POP3_PASSWORD_UNDEFINED;
        }
	  }
	break;
  }
  
  return (CE_STATUS);
}

PRIVATE int
net_smtp_auth_login_username(ActiveEntry *cur_entry)
{
  SMTPConData * cd = (SMTPConData *)cur_entry->con_data;
  char buffer[512];
  char *pop_username = (char *) NET_GetPopUsername();
  char *base64Str = NULL;

  if (!pop_username || !*pop_username)
	return (MK_POP3_USERNAME_UNDEFINED);

#ifdef MOZ_MAIL_NEWS
  base64Str = NET_Base64Encode(pop_username,
							   XP_STRLEN(pop_username));
  if (base64Str) {
	PR_snprintf(buffer, sizeof(buffer), "AUTH LOGIN %.256s" CRLF, base64Str);
	TRACEMSG(("Tx: %s", buffer));

	CE_STATUS = (int) NET_BlockingWrite(CE_SOCK, buffer, XP_STRLEN(buffer));
	CD_NEXT_STATE = SMTP_RESPONSE;
	CD_NEXT_STATE_AFTER_RESPONSE = SMTP_AUTH_LOGIN_RESPONSE;
	CD_PAUSE_FOR_READ = TRUE;
	XP_FREEIF(base64Str);
	
	return (CE_STATUS);
  }
#endif /* MOZ_MAIL_NEWS */

  return -1;
}

PRIVATE int
net_smtp_auth_login_password(ActiveEntry *cur_entry)
{
  SMTPConData * cd = (SMTPConData *)cur_entry->con_data;
  char buffer[1024];

  /* use cached smtp password first
   * if not then use cached pop password
   * if pop password undefined 
   * sync with smtp password
   */
  
  if (!net_smtp_password || !*net_smtp_password) {
	  XP_FREEIF(net_smtp_password); /* in case its an empty string */
#ifdef MOZ_MAIL_NEWS
	  net_smtp_password = (char *) NET_GetPopPassword();
#endif /* MOZ_MAIL_NEWS */
  }

  if (!net_smtp_password || !*net_smtp_password) {
	char *fmt = XP_GetString (XP_PASSWORD_FOR_POP3_USER);
	char host[256];
	int len = 256;
#if defined(SingleSignon)
	char *usernameAndHost=0;
#endif
	
	XP_MEMSET(host, 0, 256);
	PREF_GetCharPref("network.hosts.smtp_server", host, &len);
	
	PR_snprintf(buffer, sizeof (buffer), 
				fmt, NET_GetPopUsername(), host);
	XP_FREEIF(net_smtp_password);
#if defined(SingleSignon)
	StrAllocCopy(usernameAndHost, NET_GetPopUsername());
        StrAllocCat(usernameAndHost, "@");
	StrAllocCat(usernameAndHost, host);

	net_smtp_password = SI_PromptPassword(cur_entry->window_id, buffer,
					     usernameAndHost, FALSE);
	XP_FREE(usernameAndHost);
#else
	net_smtp_password = FE_PromptPassword(cur_entry->window_id, buffer);
#endif
	if (!net_smtp_password)
	  return MK_POP3_PASSWORD_UNDEFINED;
  }

  XP_ASSERT(net_smtp_password);
  
  if (net_smtp_password) {
	char *base64Str = NULL;
	
#ifdef MOZ_MAIL_NEWS
	base64Str = NET_Base64Encode(net_smtp_password, XP_STRLEN(net_smtp_password));
#endif /* MOZ_MAIL_NEWS */

	if (base64Str) {
	  PR_snprintf(buffer, sizeof(buffer), "%.256s" CRLF, base64Str);
	  TRACEMSG(("Tx: %s", buffer));

	  CE_STATUS = (int) NET_BlockingWrite(CE_SOCK, buffer, XP_STRLEN(buffer));
	  CD_NEXT_STATE = SMTP_RESPONSE;
	  CD_NEXT_STATE_AFTER_RESPONSE = SMTP_AUTH_LOGIN_RESPONSE;
	  CD_PAUSE_FOR_READ = TRUE;
	  XP_FREEIF(base64Str);

	  return (CE_STATUS);
	}
  }

  return -1;
}

PRIVATE int
net_smtp_send_vrfy_response(ActiveEntry *cur_entry)
{
#if 0
	SMTPConData * cd = (SMTPConData *)cur_entry->con_data;
    char buffer[512];

    if(CD_RESPONSE_CODE == 250 || CD_RESPONSE_CODE == 251)
		return(MK_USER_VERIFIED_BY_SMTP);
	else
		return(MK_USER_NOT_VERIFIED_BY_SMTP);
#else	
	XP_ASSERT(0);
	return(-1);
#endif
}

PRIVATE int
net_smtp_send_mail_response(ActiveEntry *cur_entry)
{
	SMTPConData * cd = (SMTPConData *)cur_entry->con_data;
    char buffer[1024];

    if(CD_RESPONSE_CODE != 250)
	  {
		CE_URL_S->error_msg = 
		  NET_ExplainErrorDetails(MK_ERROR_SENDING_FROM_COMMAND,
								  CD_RESPONSE_TXT);
		return(MK_ERROR_SENDING_FROM_COMMAND);
	  }

    /* Send the RCPT TO: command */
	if (CD_EHLO_DSN_ENABLED &&
		(CE_URL_S->msg_pane && 
		 MSG_RequestForReturnReceipt(CE_URL_S->msg_pane)))
	{
		char *encodedAddress = esmtp_value_encode(CD_MAIL_TO_ADDRESS_PTR);

		if (encodedAddress) {
			PR_snprintf(buffer, sizeof(buffer), 
			"RCPT TO:<%.256s> NOTIFY=SUCCESS,FAILURE ORCPT=rfc822;%.500s" CRLF, 
			CD_MAIL_TO_ADDRESS_PTR, encodedAddress);
			XP_FREEIF(encodedAddress);
		}
		else {
			CE_STATUS = MK_OUT_OF_MEMORY;
			return (CE_STATUS);
		}
	}
	else
	{
	    PR_snprintf(buffer, sizeof(buffer), "RCPT TO:<%.256s>" CRLF, CD_MAIL_TO_ADDRESS_PTR);
	}
	/* take the address we sent off the list (move the pointer to just
	   past the terminating null.) */
	CD_MAIL_TO_ADDRESS_PTR += XP_STRLEN (CD_MAIL_TO_ADDRESS_PTR) + 1;
	CD_MAIL_TO_ADDRESSES_LEFT--;

	TRACEMSG(("Tx: %s", buffer));
    
    CE_STATUS = (int) NET_BlockingWrite(CE_SOCK, buffer, XP_STRLEN(buffer));

    CD_NEXT_STATE = SMTP_RESPONSE;
    CD_NEXT_STATE_AFTER_RESPONSE = SMTP_SEND_RCPT_RESPONSE;
    CD_PAUSE_FOR_READ = TRUE;

    return(CE_STATUS);
}

PRIVATE int
net_smtp_send_rcpt_response(ActiveEntry *cur_entry)
{
    SMTPConData * cd = (SMTPConData *)cur_entry->con_data;
    char buffer[16];

	if(CD_RESPONSE_CODE != 250 && CD_RESPONSE_CODE != 251)
	  {
		CE_URL_S->error_msg =
		  NET_ExplainErrorDetails(MK_ERROR_SENDING_RCPT_COMMAND,
								  CD_RESPONSE_TXT);
        return(MK_ERROR_SENDING_RCPT_COMMAND);
	  }

	if(CD_MAIL_TO_ADDRESSES_LEFT > 0)
	  {
		/* more senders to RCPT to 
		 */
        CD_NEXT_STATE = SMTP_SEND_MAIL_RESPONSE; 
		return(0);
	  }

    /* else send the RCPT TO: command */
    XP_STRCPY(buffer, "DATA" CRLF);

	TRACEMSG(("Tx: %s", buffer));
        
    CE_STATUS = (int) NET_BlockingWrite(CE_SOCK, buffer, XP_STRLEN(buffer));   

    CD_NEXT_STATE = SMTP_RESPONSE;  
    CD_NEXT_STATE_AFTER_RESPONSE = SMTP_SEND_DATA_RESPONSE; 
    CD_PAUSE_FOR_READ = TRUE;   

    return(CE_STATUS);  
}

PRIVATE int
net_smtp_send_data_response(ActiveEntry *cur_entry)
{
    SMTPConData * cd = (SMTPConData *)cur_entry->con_data;
    char * command=0;   

    if(CD_RESPONSE_CODE != 354)
	  {
		CE_URL_S->error_msg = NET_ExplainErrorDetails(
									MK_ERROR_SENDING_DATA_COMMAND,
                        			CD_RESPONSE_TXT ? CD_RESPONSE_TXT : "");
        return(MK_ERROR_SENDING_DATA_COMMAND);
	  }

#ifdef XP_UNIX
	{
	  const char * FE_UsersRealMailAddress(void); /* definition */
	  const char *real_name;
	  char *s = 0;
	  XP_Bool suppress_sender_header = FALSE;

	  PREF_GetBoolPref ("mail.suppress_sender_header", &suppress_sender_header);
	  if (!suppress_sender_header)
	    {
	      real_name =  FE_UsersRealMailAddress();
	      s = (real_name ? MSG_MakeFullAddress (NULL, real_name) : 0);
	      if (real_name && !s)
		{
		  CE_URL_S->error_msg = NET_ExplainErrorDetails(MK_OUT_OF_MEMORY);
		  return(MK_OUT_OF_MEMORY);
		}
	      if(real_name)
		{
		  char buffer[512];
		  PR_snprintf(buffer, sizeof(buffer), "Sender: %.256s" CRLF, real_name);
		  StrAllocCat(command, buffer);
		  if(!command)
		    {
		      CE_URL_S->error_msg = NET_ExplainErrorDetails(MK_OUT_OF_MEMORY);
		      return(MK_OUT_OF_MEMORY);
		    }
		}
	      
	      TRACEMSG(("sending extra unix header: %s", command));
	      
	      CE_STATUS = (int) NET_BlockingWrite(CE_SOCK, command, XP_STRLEN(command));   
	      if(CE_STATUS < 0)
		{
		  TRACEMSG(("Error sending message"));
		}
	    }
	}
#endif /* XP_UNIX */

	/* set connect select since we need to select on
	 * writes
	 */
	NET_ClearReadSelect(CE_WINDOW_ID, CE_SOCK);
	NET_SetConnectSelect(CE_WINDOW_ID, CE_SOCK);
#ifdef XP_WIN
	cd->calling_netlib_all_the_time = TRUE;
	NET_SetCallNetlibAllTheTime(CE_WINDOW_ID, "mksmtp");
#endif
    CE_CON_SOCK = CE_SOCK;

	FREE(command);

    CD_NEXT_STATE = SMTP_SEND_POST_DATA;
    CD_PAUSE_FOR_READ = FALSE;   /* send data directly */

	NET_Progress(CE_WINDOW_ID, XP_GetString(MK_MSG_DELIV_MAIL));

	/* get the size of the message */
	if(CE_URL_S->post_data_is_file)
	  {
		XP_StatStruct stat_entry;

		if(-1 != XP_Stat(CE_URL_S->post_data,
                         &stat_entry,
                         xpFileToPost))
			CD_TOTAL_MESSAGE_SIZE = stat_entry.st_size;
	  }
	else
	  {
			CD_TOTAL_MESSAGE_SIZE = CE_URL_S->post_data_size;
	  }


    return(CE_STATUS);  
}

PRIVATE int
net_smtp_send_post_data(ActiveEntry *cur_entry)
{
    SMTPConData * cd = (SMTPConData *)cur_entry->con_data;
	unsigned long curtime;

	/* returns 0 on done and negative on error
	 * positive if it needs to continue.
	 */
	CE_STATUS = NET_WritePostData(CE_WINDOW_ID, CE_URL_S,
								  CE_SOCK,
								  &cd->write_post_data_data,
								  TRUE);
								  
	cd->pause_for_read = TRUE;

	if(CE_STATUS == 0)
	  {
		/* normal done
		 */
        XP_STRCPY(cd->data_buffer, CRLF "." CRLF);
        TRACEMSG(("sending %s", cd->data_buffer));
        CE_STATUS = (int) NET_BlockingWrite(CE_SOCK,
                                            cd->data_buffer,
                                            XP_STRLEN(cd->data_buffer));

		NET_Progress(CE_WINDOW_ID,
					XP_GetString(XP_MESSAGE_SENT_WAITING_MAIL_REPLY));

        NET_ClearConnectSelect(CE_WINDOW_ID, CE_SOCK);
#ifdef XP_WIN
		if(cd->calling_netlib_all_the_time)
		{
			cd->calling_netlib_all_the_time = FALSE;
			NET_ClearCallNetlibAllTheTime(CE_WINDOW_ID, "mksmtp");
		}
#endif
        NET_SetReadSelect(CE_WINDOW_ID, CE_SOCK);
		CE_CON_SOCK = 0;

        CD_NEXT_STATE = SMTP_RESPONSE;
        CD_NEXT_STATE_AFTER_RESPONSE = SMTP_SEND_MESSAGE_RESPONSE;
        return(0);
	  }

	CD_TOTAL_AMT_WRITTEN += CE_STATUS;

	/* Update the thermo and the status bar.  This is done by hand, rather
	   than using the FE_GraphProgress* functions, because there seems to be
	   no way to make FE_GraphProgress shut up and not display anything more
	   when all the data has arrived.  At the end, we want to show the
	   "message sent; waiting for reply" status; FE_GraphProgress gets in
	   the way of that.  See bug #23414. */

	curtime = XP_TIME();
	if (curtime != cd->last_time) {
		FE_Progress(CE_WINDOW_ID, XP_ProgressText(CD_TOTAL_MESSAGE_SIZE,
												  CD_TOTAL_AMT_WRITTEN,
												  0, 0));
		cd->last_time = curtime;
	}

	if(CD_TOTAL_MESSAGE_SIZE)
		FE_SetProgressBarPercent(CE_WINDOW_ID,
						  	 CD_TOTAL_AMT_WRITTEN*100/CD_TOTAL_MESSAGE_SIZE);

    return(CE_STATUS);
}



PRIVATE int
net_smtp_send_message_response(ActiveEntry *cur_entry)
{
    SMTPConData * cd = (SMTPConData *)cur_entry->con_data;

    if(CD_RESPONSE_CODE != 250)
	  {
		CE_URL_S->error_msg = NET_ExplainErrorDetails(MK_ERROR_SENDING_MESSAGE,
													  CD_RESPONSE_TXT);
        return(MK_ERROR_SENDING_MESSAGE);
	  }

	NET_Progress(CE_WINDOW_ID, XP_GetString(XP_PROGRESS_MAILSENT));

	/* else */
	CD_NEXT_STATE = SMTP_DONE;
	return(MK_NO_DATA);
}


PRIVATE int32
net_MailtoLoad (ActiveEntry * cur_entry)
{
    /* get memory for Connection Data */
    SMTPConData * cd = XP_NEW(SMTPConData);
	int32 pref = 0;

    cur_entry->con_data = cd;
    if(!cur_entry->con_data)
      {
		CE_URL_S->error_msg = NET_ExplainErrorDetails(MK_OUT_OF_MEMORY);
        CE_STATUS = MK_OUT_OF_MEMORY;
        return (CE_STATUS);
      }

/*	GH_UpdateGlobalHistory(cur_entry->URL_s); */

    /* init */
    XP_MEMSET(cd, 0, sizeof(SMTPConData));

	CD_CONTINUATION_RESPONSE = -1;  /* init */
  
    CE_SOCK = NULL;
HG61365

	
	/* make a copy of the address
	 */
	if(CE_URL_S->method == URL_POST_METHOD)
	  {
		int status=0;
		char *addrs1 = 0;
		char *addrs2 = 0;
    	CD_NEXT_STATE = SMTP_START_CONNECT;

		/* Remove duplicates from the list, to prevent people from getting
		   more than one copy (the SMTP host may do this too, or it may not.)
		   This causes the address list to be parsed twice; this probably
		   doesn't matter.
		 */
		addrs1 = MSG_RemoveDuplicateAddresses (CE_URL_S->address+7, 0, FALSE /*removeAliasesToMe*/);

		/* Extract just the mailboxes from the full RFC822 address list.
		   This means that people can post to mailto: URLs which contain
		   full RFC822 address specs, and we will still send the right
		   thing in the SMTP RCPT command.
		 */
		if (addrs1 && *addrs1)
		{
			status = MSG_ParseRFC822Addresses (addrs1, 0, &addrs2);
			FREEIF (addrs1);
		}

		if (status < 0) return status;

		if (status == 0 || addrs2 == 0)
		  {
			CD_NEXT_STATE = SMTP_ERROR_DONE;
			CD_PAUSE_FOR_READ = FALSE;
			CE_STATUS = MK_MIME_NO_RECIPIENTS;
			CE_URL_S->error_msg = NET_ExplainErrorDetails(CE_STATUS);
			return CE_STATUS;
		  }

		CD_ADDRESS_COPY = addrs2;
		CD_MAIL_TO_ADDRESS_PTR = CD_ADDRESS_COPY;
		CD_MAIL_TO_ADDRESSES_LEFT = status;
        return(net_ProcessMailto(cur_entry));
	  }
	else
	  {
		/* parse special headers and stuff from the search data in the
		   URL address.  This data is of the form

		    mailto:TO_FIELD?FIELD1=VALUE1&FIELD2=VALUE2

		   where TO_FIELD may be empty, VALUEn may (for now) only be
		   one of "cc", "bcc", "subject", "newsgroups", "references",
		   and "attachment".

		   "to" is allowed as a field/value pair as well, for consistency.

		   One additional parameter is allowed, which does not correspond
		   to a visible field: "newshost".  This is the NNTP host (and port)
		   to connect to if newsgroups are specified.  

		   Each parameter may appear only once, but the order doesn't
		   matter.  All values must be URL-encoded.
		 */
		HG27655

		char *parms = NET_ParseURL (CE_URL_S->address, GET_SEARCH_PART);
		char *rest = parms;
		char *from = 0;						/* internal only */
		char *reply_to = 0;					/* internal only */
		char *to = 0;
		char *cc = 0;
		char *bcc = 0;
		char *fcc = 0;						/* internal only */
		char *newsgroups = 0;
		char *followup_to = 0;
		char *html_part = 0;				/* internal only */
		char *organization = 0;				/* internal only */
		char *subject = 0;
		char *references = 0;
		char *attachment = 0;				/* internal only */
		char *body = 0;
		char *other_random_headers = 0;		/* unused (for now) */
		char *priority = 0;
		char *newshost = 0;					/* internal only */
		XP_Bool encrypt_p = FALSE;
		XP_Bool sign_p = FALSE;				/* internal only */

		char *newspost_url = 0;
		XP_Bool force_plain_text = FALSE;
		MSG_Pane *cpane = 0;

		to = NET_ParseURL (CE_URL_S->address, GET_PATH_PART);

		if (rest && *rest == '?')
		  {
 			/* start past the '?' */
			rest++;
			rest = XP_STRTOK (rest, "&");
			while (rest && *rest)
			  {
				char *token = rest;
				char *value = 0;
				char *eq = XP_STRCHR (token, '=');
				if (eq)
				  {
					value = eq+1;
					*eq = 0;
				  }
				switch (*token)
				  {
				  case 'A': case 'a':
					if (!strcasecomp (token, "attachment") &&
						CE_URL_S->internal_url)
					  StrAllocCopy (attachment, value);
					break;
				  case 'B': case 'b':
					if (!strcasecomp (token, "bcc"))
					  {
						if (bcc && *bcc)
						  {
							StrAllocCat (bcc, ", ");
							StrAllocCat (bcc, value);
						  }
						else
						  {
							StrAllocCopy (bcc, value);
						  }
					  }
					else if (!strcasecomp (token, "body"))
					  {
						if (body && *body)
						  {
							StrAllocCat (body, "\n");
							StrAllocCat (body, value);
						  }
						else
						  {
							StrAllocCopy (body, value);
						  }
					  }
					break;
				  case 'C': case 'c':
					if (!strcasecomp (token, "cc"))
					  {
						if (cc && *cc)
						  {
							StrAllocCat (cc, ", ");
							StrAllocCat (cc, value);
						  }
						else
						  {
							StrAllocCopy (cc, value);
						  }
					  }
					break;
				  case 'E': case 'e':
					if (!strcasecomp (token, "encrypt") ||
						!strcasecomp (token, "encrypted"))
					  encrypt_p = (!strcasecomp(value, "true") ||
								   !strcasecomp(value, "yes"));
					break;
				  case 'F': case 'f':
					if (!strcasecomp (token, "followup-to"))
					  StrAllocCopy (followup_to, value);
					else if (!strcasecomp (token, "from") &&
							 CE_URL_S->internal_url)
					  StrAllocCopy (from, value);
					else if (!strcasecomp (token, "force-plain-text") &&
							 CE_URL_S->internal_url)
						force_plain_text = TRUE;
					break;
				  case 'H': case 'h':
					  if (!strcasecomp(token, "html-part") &&
						  CE_URL_S->internal_url) {
						StrAllocCopy(html_part, value);
					  }
				  case 'N': case 'n':
					if (!strcasecomp (token, "newsgroups"))
					  StrAllocCopy (newsgroups, value);
					else if (!strcasecomp (token, "newshost") &&
							 CE_URL_S->internal_url)
					  StrAllocCopy (newshost, value);
					break;
				  case 'O': case 'o':
					if (!strcasecomp (token, "organization") &&
						CE_URL_S->internal_url)
					  StrAllocCopy (organization, value);
					break;
				  case 'R': case 'r':
					if (!strcasecomp (token, "references"))
					  StrAllocCopy (references, value);
					else if (!strcasecomp (token, "reply-to") &&
							 CE_URL_S->internal_url)
					  StrAllocCopy (reply_to, value);
					break;
				  case 'S': case 's':
					if(!strcasecomp (token, "subject"))
					  StrAllocCopy (subject, value);
					else if ((!strcasecomp (token, "sign") ||
							  !strcasecomp (token, "signed")) &&
							 CE_URL_S->internal_url)
					  sign_p = (!strcasecomp(value, "true") ||
								!strcasecomp(value, "yes"));
					break;
				  case 'P': case 'p':
					if (!strcasecomp (token, "priority"))
					  StrAllocCopy (priority, value);
					break;
				  case 'T': case 't':
					if (!strcasecomp (token, "to"))
					  {
						if (to && *to)
						  {
							StrAllocCat (to, ", ");
							StrAllocCat (to, value);
						  }
						else
						  {
							StrAllocCopy (to, value);
						  }
					  }
					break;
				  }
				if (eq)
				  *eq = '='; /* put it back */
				rest = XP_STRTOK (0, "&");
			  }
		  }

		FREEIF (parms);
		if (to)
		  NET_UnEscape (to);
		if (cc)
		  NET_UnEscape (cc);
		if (subject)
		  NET_UnEscape (subject);
		if (newsgroups)
		  NET_UnEscape (newsgroups);
		if (references)
		  NET_UnEscape (references);
		if (attachment)
		  NET_UnEscape (attachment);
		if (body)
		  NET_UnEscape (body);
		if (newshost)
		  NET_UnEscape (newshost);

		if(newshost)
		  {
			char *prefix = "news://";
			char *slash = XP_STRRCHR (newshost, '/');
			HG83763
			newspost_url = (char *) XP_ALLOC (XP_STRLEN (prefix) +
											  XP_STRLEN (newshost) + 10);
			if (newspost_url)
			  {
				XP_STRCPY (newspost_url, prefix);
				XP_STRCAT (newspost_url, newshost);
				XP_STRCAT (newspost_url, "/");
			  }
		  }
		else
		  {
			HG35353
				newspost_url = XP_STRDUP ("news:");
		  }

		/* Tell the message library and front end to pop up an edit window.
		 */
#ifdef XP_UNIX
        cpane = MSG_ComposeMessage (CE_WINDOW_ID,
									from, reply_to, to, cc, bcc, fcc,
									newsgroups, followup_to, organization,
									subject, references, other_random_headers,
									priority, attachment, newspost_url, body,
									force_plain_text,
									html_part);
#endif //XP_UNIX

#ifdef MOZ_MAIL_NEWS
		if (cpane && CE_URL_S->fe_data) {
			/* Tell libmsg what to do after deliver the message */
			MSG_SetPostDeliveryActionInfo (cpane, CE_URL_S->fe_data);
		}
#endif /* MOZ_MAIL_NEWS */

		FREEIF(from);
		FREEIF(reply_to);
		FREEIF(to);
		FREEIF(cc);
		FREEIF(bcc);
		FREEIF(fcc);
		FREEIF(newsgroups);
		FREEIF(followup_to);
		FREEIF(html_part);
		FREEIF(organization);
		FREEIF(subject);
		FREEIF(references);
		FREEIF(attachment);
		FREEIF(body);
		FREEIF(other_random_headers);
		FREEIF(newshost);
		FREEIF(priority);
		FREEIF(newspost_url);

		CE_STATUS = MK_NO_DATA;
		XP_FREE(cd);	/* no one else is gonna do it! */
		return(-1);
	  }
}




/*
	We have connected to the mail relay and the type of authorization/login required
	has been established. Before we actually send our name and password check
	and see if we should or must turn the connection to safer mode.
*/

PRIVATE int
NET_CheckAuthResponse (ActiveEntry *cur_entry)
{
    SMTPConData * cd = (SMTPConData *)cur_entry->con_data;
	int err = 0;
	HG54978

	if (CD_AUTH_LOGIN_ENABLED)
	{
		CD_NEXT_STATE_AFTER_RESPONSE = SMTP_AUTH_LOGIN_RESPONSE;
		CD_NEXT_STATE = SMTP_SEND_AUTH_LOGIN_USERNAME;
		return (CE_STATUS);
	}
	CD_NEXT_STATE = SMTP_SEND_HELO_RESPONSE;
	return (CE_STATUS);
}




/*
 * returns negative if the transfer is finished or error'd out
 *
 * returns zero or more if the transfer needs to be continued.
 */
PRIVATE int32
net_ProcessMailto (ActiveEntry *cur_entry)
{
    SMTPConData * cd = (SMTPConData *)cur_entry->con_data;
	char	*mail_relay_host;

    TRACEMSG(("Entering NET_ProcessSMTP"));

    CD_PAUSE_FOR_READ = FALSE; /* already paused; reset */

    while(!CD_PAUSE_FOR_READ)
      {

		TRACEMSG(("In NET_ProcessSMTP with state: %d", CD_NEXT_STATE));

        switch(CD_NEXT_STATE) {

		case SMTP_RESPONSE:
			net_smtp_response (cur_entry);
			break;

        case SMTP_START_CONNECT:
			mail_relay_host = NET_MailRelayHost(CE_WINDOW_ID);
			if (XP_STRLEN(mail_relay_host) == 0)
			{
				CE_STATUS = MK_MSG_NO_SMTP_HOST;
				break;
			}
            CE_STATUS = NET_BeginConnect(NET_MailRelayHost(CE_WINDOW_ID), 
										NULL,
										"SMTP",
										SMTP_PORT, 
										&CE_SOCK, 
										FALSE, 
										&CD_TCP_CON_DATA, 
										CE_WINDOW_ID,
										&CE_URL_S->error_msg,
										 cur_entry->socks_host,
										 cur_entry->socks_port,
                                         CE_URL_S->localIP);
            CD_PAUSE_FOR_READ = TRUE;
            if(CE_STATUS == MK_CONNECTED)
              {
                CD_NEXT_STATE = SMTP_RESPONSE;
                CD_NEXT_STATE_AFTER_RESPONSE = SMTP_EXTN_LOGIN_RESPONSE;
                NET_SetReadSelect(CE_WINDOW_ID, CE_SOCK);
              }
            else if(CE_STATUS > -1)
              {
                CE_CON_SOCK = CE_SOCK;  /* set con_sock so we can select on it */
                NET_SetConnectSelect(CE_WINDOW_ID, CE_CON_SOCK);
                CD_NEXT_STATE = SMTP_FINISH_CONNECT;
              }
            break;

		case SMTP_FINISH_CONNECT:
            CE_STATUS = NET_FinishConnect(NET_MailRelayHost(CE_WINDOW_ID), 
										  "SMTP", 
										  SMTP_PORT, 
										  &CE_SOCK, 
										  &CD_TCP_CON_DATA, 
										  CE_WINDOW_ID,
										  &CE_URL_S->error_msg,
                                          CE_URL_S->localIP);

            CD_PAUSE_FOR_READ = TRUE;
HG18931
            if(CE_STATUS == MK_CONNECTED)
              {
                CD_NEXT_STATE = SMTP_RESPONSE;
                CD_NEXT_STATE_AFTER_RESPONSE = SMTP_EXTN_LOGIN_RESPONSE;
                NET_ClearConnectSelect(CE_WINDOW_ID, CE_CON_SOCK);
                CE_CON_SOCK = NULL;  /* reset con_sock so we don't select on it */
                NET_SetReadSelect(CE_WINDOW_ID, CE_SOCK);
              }
			else
              {
                /* unregister the old CE_SOCK from the select list
                 * and register the new value in the case that it changes
                 */
                if(CE_CON_SOCK != CE_SOCK)
                  {
                    NET_ClearConnectSelect(CE_WINDOW_ID, CE_CON_SOCK);
                    CE_CON_SOCK = CE_SOCK;
                    NET_SetConnectSelect(CE_WINDOW_ID, CE_CON_SOCK);
                  }
              }
            break;

       case SMTP_AUTH_RESPONSE:
			CE_STATUS = NET_CheckAuthResponse(cur_entry);
			break;

       case SMTP_LOGIN_RESPONSE:
            CE_STATUS = net_smtp_login_response(cur_entry);
            break;

       case SMTP_EXTN_LOGIN_RESPONSE:
            CE_STATUS = net_smtp_extension_login_response(cur_entry);
            break;

	   case SMTP_SEND_HELO_RESPONSE:
		    CE_STATUS = net_smtp_send_helo_response(cur_entry);
            break;

	   case SMTP_SEND_EHLO_RESPONSE:
		    CE_STATUS = net_smtp_send_ehlo_response(cur_entry);
			break;

	   case SMTP_AUTH_LOGIN_RESPONSE:
		    CE_STATUS = net_smtp_auth_login_response(cur_entry);
			break;
			
	   case SMTP_SEND_AUTH_LOGIN_USERNAME:
		    CE_STATUS = net_smtp_auth_login_username(cur_entry);
			break;

	   case SMTP_SEND_AUTH_LOGIN_PASSWORD:
		    CE_STATUS = net_smtp_auth_login_password(cur_entry);
			break;
			
	   case SMTP_SEND_VRFY_RESPONSE:
			CE_STATUS = net_smtp_send_vrfy_response(cur_entry);
            break;
			
	   case SMTP_SEND_MAIL_RESPONSE:
            CE_STATUS = net_smtp_send_mail_response(cur_entry);
            break;
			
	   case SMTP_SEND_RCPT_RESPONSE:
            CE_STATUS = net_smtp_send_rcpt_response(cur_entry);
            break;
			
	   case SMTP_SEND_DATA_RESPONSE:
            CE_STATUS = net_smtp_send_data_response(cur_entry);
            break;
			
	   case SMTP_SEND_POST_DATA:
            CE_STATUS = net_smtp_send_post_data(cur_entry);
            break;
			
	   case SMTP_SEND_MESSAGE_RESPONSE:
            CE_STATUS = net_smtp_send_message_response(cur_entry);
            break;
        
        case SMTP_DONE:
			NET_BlockingWrite(CE_SOCK, "QUIT", 4);
            NET_ClearReadSelect(CE_WINDOW_ID, CE_SOCK);
            PR_Close(CE_SOCK);
            CD_NEXT_STATE = SMTP_FREE;
            break;
        
        case SMTP_ERROR_DONE:
            if(CE_SOCK != NULL)
              {
				/* we only send out quit if user interrupt 
				 * else must be server error which may not be
				 * able to blocking write command
				 */
				if (CE_STATUS == MK_INTERRUPTED)
					NET_BlockingWrite(CE_SOCK, "QUIT", 4);
                NET_ClearReadSelect(CE_WINDOW_ID, CE_SOCK);
                NET_ClearConnectSelect(CE_WINDOW_ID, CE_SOCK);
#ifdef XP_WIN
				if(cd->calling_netlib_all_the_time)
				{
					cd->calling_netlib_all_the_time = FALSE;
					NET_ClearCallNetlibAllTheTime(CE_WINDOW_ID, "mksmtp");
				}
#endif /* XP_WIN */
                NET_ClearDNSSelect(CE_WINDOW_ID, CE_SOCK);
                PR_Close(CE_SOCK);
              }
            CD_NEXT_STATE = SMTP_FREE;
            break;
        
        case SMTP_FREE:
            FREEIF(CD_DATA_BUFFER);
            FREEIF(CD_ADDRESS_COPY);
			FREEIF(CD_RESPONSE_TXT);
            if(CD_TCP_CON_DATA)
                NET_FreeTCPConData(CD_TCP_CON_DATA);
			if (cd->write_post_data_data)
				NET_free_write_post_data_object((struct WritePostDataData *) 
												cd->write_post_data_data);
            FREE(cd);

            return(-1); /* final end */
        
        default: /* should never happen !!! */
            TRACEMSG(("SMTP: BAD STATE!"));
            CD_NEXT_STATE = SMTP_ERROR_DONE;
            break;
        }

        /* check for errors during load and call error 
         * state if found
         */
        if(CE_STATUS < 0 && CD_NEXT_STATE != SMTP_FREE)
          {
            CD_NEXT_STATE = SMTP_ERROR_DONE;
            /* don't exit! loop around again and do the free case */
            CD_PAUSE_FOR_READ = FALSE;
          }
      } /* while(!CD_PAUSE_FOR_READ) */
    
    return(CE_STATUS);
}

/* abort the connection in progress
 */
PRIVATE int32
net_InterruptMailto(ActiveEntry * cur_entry)
{
    SMTPConData * cd = (SMTPConData *)cur_entry->con_data;

    CD_NEXT_STATE = SMTP_ERROR_DONE;
	CE_STATUS = MK_INTERRUPTED;
 
    return(net_ProcessMailto(cur_entry));
}

/* Free any memory that might be used in caching etc.
 */
PRIVATE void
net_CleanupMailto(void)
{
    /* nothing so far needs freeing */
    return;
}

MODULE_PRIVATE void
NET_InitMailtoProtocol(void)
{
        static NET_ProtoImpl mailto_proto_impl;

        mailto_proto_impl.init = net_MailtoLoad;
        mailto_proto_impl.process = net_ProcessMailto;
        mailto_proto_impl.interrupt = net_InterruptMailto;
        mailto_proto_impl.resume = NULL;
        mailto_proto_impl.cleanup = net_CleanupMailto;

        NET_RegisterProtocolImplementation(&mailto_proto_impl, MAILTO_TYPE_URL);
}

#endif /* defined(MOZ_MAIL_NEWS) || defined(MOZ_MAIL_COMPOSE) */

static void
MessageSendingDone(URL_Struct* url, int status, MWContext* context)
{
    if (status < 0) {
		char *error_msg = NET_ExplainErrorDetails(status, 0, 0, 0, 0);
		if (error_msg) {
			FE_Alert(context, error_msg);
		}
		XP_FREEIF(error_msg);
    }
    if (url->post_data) {
		XP_FREE(url->post_data);
		url->post_data = NULL;
    }
    if (url->post_headers) {
		XP_FREE(url->post_headers);
		url->post_headers = NULL;
    }
    NET_FreeURLStruct(url);
}



int
NET_SendMessageUnattended(MWContext* context, char* to, char* subject,
						  char* otherheaders, char* body)
{
    char* urlstring;
    URL_Struct* url;
    int16 win_csid;
    int16 csid;
    char* convto;
    char* convsub;

    win_csid = INTL_DefaultWinCharSetID(context);
    csid = INTL_DefaultMailCharSetID(win_csid);
    convto = IntlEncodeMimePartIIStr(to, csid, TRUE);
    convsub = IntlEncodeMimePartIIStr(subject, csid, TRUE);

    urlstring = PR_smprintf("mailto:%s", convto ? convto : to);

    if (!urlstring) return MK_OUT_OF_MEMORY;
    url = NET_CreateURLStruct(urlstring, NET_DONT_RELOAD);
    XP_FREE(urlstring);
    if (!url) return MK_OUT_OF_MEMORY;

    

    url->post_headers = PR_smprintf("To: %s\n\
Subject: %s\n\
%s\n",
								 convto ? convto : to,
								 convsub ? convsub : subject,
								 otherheaders);
    if (convto) XP_FREE(convto);
    if (convsub) XP_FREE(convsub);
    if (!url->post_headers) return MK_OUT_OF_MEMORY;
	url->post_data = XP_STRDUP(body);
	if (!url->post_data) return MK_OUT_OF_MEMORY;
    url->post_data_size = XP_STRLEN(url->post_data);
    url->post_data_is_file = FALSE;
    url->method = URL_POST_METHOD;
    url->internal_url = TRUE;
    return NET_GetURL(url, FO_PRESENT, context, MessageSendingDone);
    
}

#if !defined(MOZ_MAIL_NEWS) && !defined(SMART_MAIL)
PUBLIC const char*
NET_GetPopUsername ()
{
  return 0;
}
#endif /* MOZ_MAIL_NEWS */


#endif /* defined(MOZILLA_CLIENT) || defined(LIBNET_SMTP) */
