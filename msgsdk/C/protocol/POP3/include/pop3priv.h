/*
 *	 The contents of this file are subject to the Netscape Public
 *	 License Version 1.1 (the "License"); you may not use this file
 *	 except in compliance with the License. You may obtain a copy of
 *	 the License at http://www.mozilla.org/NPL/
 *	
 *	 Software distributed under the License is distributed on an "AS
 *	 IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 *	 implied. See the License for the specific language governing
 *	 rights and limitations under the License.
 *	
 *	 The Original Code is the Netscape Messaging Access SDK Version 3.5 code, 
 *	released on or about June 15, 1998.  *	
 *	 The Initial Developer of the Original Code is Netscape Communications 
 *	 Corporation. Portions created by Netscape are
 *	 Copyright (C) 1998 Netscape Communications Corporation. All
 *	 Rights Reserved.
 *	
 *	 Contributor(s): ______________________________________. 
*/
  
/* 
 * Copyright (c) 1997 and 1998 Netscape Communications Corporation
 * (http://home.netscape.com/misc/trademarks.html) 
 */

#ifndef POP3PRIV_H
#define POP3PRIV_H

/*
 * pop3priv.h
 * @author derekt@netscape.com
 * @version 1.0
 */

#include "nsmail.h"
#include "nsio.h"
#include "linklist.h"

/* Default port number. */
#define DEFAULT_POP3_PORT 110

/* Maximum definitions. */
#define MAX_DOMAIN_LENGTH 64
#define MAX_USERHNAME_LENGTH 64
#define MAX_RCPTBUFFER_LENGTH 100
#define MAX_PATH_LENGTH 256
#define MAX_COMMANDLINE_LENGTH 512
#define MAX_REPLYLINE_LENGTH 512
#define MAX_TEXTLINE_LENGTH 1000
#define MAX_BUFFER_LENGTH 1024

/* Response types for POP3. */
typedef enum pop3_ResponseType
{
    CONN,
    DELE,
	LISTA,
	LIST,
    NOOP,
    PASS,
    QUIT,
    RSET,
    RETR,
    SENDCOMMANDA,
    SENDCOMMAND,
    STAT,
    TOP,
    UIDLA,
    UIDL,
    USER,
    XAUTHLISTA,
    XAUTHLIST,
    XSENDER
} pop3_ResponseType_t;

/* Response variables for POP3. */
const static pop3_ResponseType_t POP3Response_CONN = CONN;
const static pop3_ResponseType_t POP3Response_DELE = DELE;
const static pop3_ResponseType_t POP3Response_LISTA = LISTA;
const static pop3_ResponseType_t POP3Response_LIST = LIST;
const static pop3_ResponseType_t POP3Response_NOOP = NOOP;
const static pop3_ResponseType_t POP3Response_PASS = PASS;
const static pop3_ResponseType_t POP3Response_QUIT = QUIT;
const static pop3_ResponseType_t POP3Response_RSET = RSET;
const static pop3_ResponseType_t POP3Response_RETR = RETR;
const static pop3_ResponseType_t POP3Response_SENDCOMMANDA = SENDCOMMANDA;
const static pop3_ResponseType_t POP3Response_SENDCOMMAND = SENDCOMMAND;
const static pop3_ResponseType_t POP3Response_STAT = STAT;
const static pop3_ResponseType_t POP3Response_TOP = TOP;
const static pop3_ResponseType_t POP3Response_UIDLA = UIDLA;
const static pop3_ResponseType_t POP3Response_UIDL = UIDL;
const static pop3_ResponseType_t POP3Response_USER = USER;
const static pop3_ResponseType_t POP3Response_XAUTHLISTA = XAUTHLISTA;
const static pop3_ResponseType_t POP3Response_XAUTHLIST = XAUTHLIST;
const static pop3_ResponseType_t POP3Response_XSENDER = XSENDER;

/* Constants used in building commands. */
const static char dele[] = "DELE ";
const static char list[] = "LIST ";
const static char noop[] = "NOOP";
const static char pass[] = "PASS ";
const static char quit[] = "QUIT";
const static char rset[] = "RSET";
const static char retr[] = "RETR ";
const static char stat[] = "STAT";
const static char top[] = "TOP ";
const static char uidl[] = "UIDL ";
const static char user[] = "USER ";
const static char xauthlist[] = "XAUTHLIST ";
const static char xsender[] = "XSENDER ";
const static char space[] = " ";
const static char ok[] = "+OK";
const static char err[] = "-ERR";

/*
 * Structure used by a client to communicate to a POP3 server.
 * This structure stores IO_t structure which deals with
 * the specifics of communication.
 */
typedef struct pop3Client
{
	/* Communication structure. */
    IO_t io;

	/* List of command types sent to the server. */
    LinkedList_t * pCommandList;

	/* Flag indicating if a response must be processed. */
    boolean mustProcess;

	/* Flag indicating we are in the middle of a multiline response. */
    boolean multiLineState;

    /* Get/Set variables */
    int chunkSize;
    double timeout;
    pop3Sink_t * pop3Sink;

    /* Re-used variables */
    char commandBuffer[MAX_COMMANDLINE_LENGTH];
    char responseLine[MAX_TEXTLINE_LENGTH];
    char * messageData;
    int messageDataSize;
    int messageNumber;
} pop3Client_i_t;

/* Function pointer for responses. */
typedef void (*sinkMethod_t)( pop3SinkPtr_t, const char * );

/* Internal parsing functions. */
static int setStatusInfo( const char * in_responseLine, boolean * out_status );
static int parseSingleLine( pop3Client_t *, sinkMethod_t );
static int parseConnect( pop3Client_t * in_pPOP3 );
static int parseDelete( pop3Client_t * in_pPOP3 );
static int parseListA( pop3Client_t * in_pPOP3 );
static int parseList( pop3Client_t * in_pPOP3 );
static int parseNoop( pop3Client_t * in_pPOP3 );
static int parsePass( pop3Client_t * in_pPOP3 );
static int parseQuit( pop3Client_t * in_pPOP3 );
static int parseRset( pop3Client_t * in_pPOP3 );
static int parseRetr( pop3Client_t * in_pPOP3 );
static int parseSendCommandA( pop3Client_t * in_pPOP3 );
static int parseSendCommand( pop3Client_t * in_pPOP3 );
static int parseStat( pop3Client_t * in_pPOP3 );
static int parseTop( pop3Client_t * in_pPOP3 );
static int parseUidlA( pop3Client_t * in_pPOP3 );
static int parseUidl( pop3Client_t * in_pPOP3 );
static int parseUser( pop3Client_t * in_pPOP3 );
static int parseXAuthListA( pop3Client_t * in_pPOP3 );
static int parseXAuthList( pop3Client_t * in_pPOP3 );
static int parseXSender( pop3Client_t * in_pPOP3 );

/* Internal sink functions used to check for NULL function pointers. */
static void pop3Sink_connect( pop3Sink_t *, const char * );
static void pop3Sink_delete( pop3Sink_t *, const char * );
static void pop3Sink_error( pop3Sink_t *, const char * );
static void pop3Sink_listStart( pop3Sink_t * );
static void pop3Sink_list( pop3Sink_t *, int, int );
static void pop3Sink_listComplete( pop3Sink_t * );
static void pop3Sink_noop( pop3Sink_t * );
static void pop3Sink_pass(pop3Sink_t *, const char * );
static void pop3Sink_quit(pop3Sink_t *, const char * );
static void pop3Sink_reset(pop3Sink_t *, const char * );
static void pop3Sink_retrieveStart( pop3Sink_t *, int, int );
static void pop3Sink_retrieve( pop3Sink_t *, const char * );
static void pop3Sink_retrieveComplete( pop3Sink_t * );
static void pop3Sink_sendCommandStart( pop3Sink_t * );
static void pop3Sink_sendCommand( pop3Sink_t *, const char * );
static void pop3Sink_sendCommandComplete( pop3Sink_t * );
static void pop3Sink_stat( pop3Sink_t *, int, int );
static void pop3Sink_topStart( pop3Sink_t *, int );
static void pop3Sink_top( pop3Sink_t *, const char * );
static void pop3Sink_topComplete( pop3Sink_t * );
static void pop3Sink_uidListStart( pop3Sink_t * );
static void pop3Sink_uidList( pop3Sink_t *, int, const char * );
static void pop3Sink_uidListComplete( pop3Sink_t * );
static void pop3Sink_user( pop3Sink_t *, const char * );
static void pop3Sink_xAuthListStart( pop3Sink_t * );
static void pop3Sink_xAuthList( pop3Sink_t *, int, const char * );
static void pop3Sink_xAuthListComplete( pop3Sink_t * );
static void pop3Sink_xSender( pop3Sink_t *, const char * );

#endif /* POP3_H */
