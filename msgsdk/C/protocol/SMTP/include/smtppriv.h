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

#ifndef SMTPPRIV_H
#define SMTPPRIV_H

/*
 * smtppriv.h
 * @author derekt@netscape.com
 * @version 1.0
 */

#include "nsmail.h"
#include "nsio.h"
#include "linklist.h"

/* Default port number. */
#define DEFAULT_SMTP_PORT 25

/* Maximum definitions. */
#define MAX_DOMAIN_LENGTH 64
#define MAX_USERHNAME_LENGTH 64
#define MAX_RCPTBUFFER_LENGTH 100
#define MAX_PATH_LENGTH 256
#define MAX_COMMANDLINE_LENGTH 512
#define MAX_REPLYLINE_LENGTH 512
#define MAX_TEXTLINE_LENGTH 1000
#define MAX_BUFFER_LENGTH 1024

/* Response types for SMTP. */
typedef enum smtp_ResponseType
{
    BDAT,
	CONN,
    DATA,
    EHLO,
    EXPN,
    HELP,
    MAIL,
    NOOP,
    QUIT,
    RCPT,
    RSET,
    SEND,
    SENDCOMMAND,
    VRFY
} smtp_ResponseType_t;

/* Response variables for SMTP. */
const static smtp_ResponseType_t SMTPResponse_BDAT = BDAT;
const static smtp_ResponseType_t SMTPResponse_CONN = CONN;
const static smtp_ResponseType_t SMTPResponse_DATA = DATA;
const static smtp_ResponseType_t SMTPResponse_EHLO = EHLO;
const static smtp_ResponseType_t SMTPResponse_EXPN = EXPN;
const static smtp_ResponseType_t SMTPResponse_HELP = HELP;
const static smtp_ResponseType_t SMTPResponse_MAIL = MAIL;
const static smtp_ResponseType_t SMTPResponse_NOOP = NOOP;
const static smtp_ResponseType_t SMTPResponse_QUIT = QUIT;
const static smtp_ResponseType_t SMTPResponse_RCPT = RCPT;
const static smtp_ResponseType_t SMTPResponse_RSET = RSET;
const static smtp_ResponseType_t SMTPResponse_SEND = SEND;
const static smtp_ResponseType_t SMTPResponse_SENDCOMMAND = SENDCOMMAND;
const static smtp_ResponseType_t SMTPResponse_VRFY = VRFY;

/* Constants used for sending commands. */
const static char bdat[] = "BDAT ";
const static char data[] = "DATA";
const static char ehlo[] = "EHLO ";
const static char helo[] = "HELO ";
const static char expn[] = "EXPN ";
const static char help[] = "HELP ";
const static char mailFrom[] = "MAIL FROM:<";
const static char noop[] = "NOOP";
const static char quit[] = "QUIT";
const static char rcptTo[] = "RCPT TO:<";
const static char rset[] = "RSET";
const static char vrfy[] = "VRFY ";
const static char rBracket[] = ">";
const static char eomessage[] = "\r\n.";
const static char bdatLast[] = " LAST";
const static char empty[] = "";
const static char pipelining[] = "PIPELINING";

/*
 * Structure used by a client to communicate to a SMTP server.
 * This structure stores IO_t structure which deals with
 * the specifics of communication.
 */
typedef struct smtpClient
{
	/* Communication structure. */
    IO_t io;

	/* List of command types sent to the server. */
    LinkedList_t * pCommandList;

	/* Flag indicating if a response must be processed. */
    boolean mustProcess;

	/* State variable indicating if the DATA command was last executed. */
    boolean fDATASent;

	/* Last character sent to the server used for byte stuffing messages. */
    char lastSentChar;

	/* Boolean variable indicating if the server supports PIPELINING. */
    boolean pipeliningSupported;

    /* Get/Set variables */
    int chunkSize;
    double timeout;
    smtpSink_t * smtpSink;
    boolean pipelining;

    /* Re-used variables */
    char * messageData;
    char commandBuffer[512];
    char responseLine[512];
} smtpClient_i_t;

/* Function pointer for responses. */
typedef void (*sinkMethod_t)( smtpSinkPtr_t, int , const char * );
typedef void (*sinkMethodComplete_t)( smtpSinkPtr_t );

/* Internal parsing functions. */
static int setStatusInfo( const char * in_responseLine, int * out_responseCode );
static int parseSingleLine( smtpClient_t *, sinkMethod_t );
static int parseMultiLine( smtpClient_t *, sinkMethod_t, sinkMethodComplete_t );
static int parseData( smtpClient_t * in_pSMTP );
static int parseEhlo( smtpClient_t * );
static void invokeCallback( smtpSink_t *, sinkMethod_t, int, const char * );

#ifdef SMTP_HIGHLEVEL_API

/*
 * A higher level API
 * @author derekt@netscape.com
 * @version 1.0
 */

typedef struct returnInfo
{
    boolean error;
    char errorBuffer[512];
} returnInfo_t;

/* Internal high level functions. */
void cleanup( smtpClient_t * in_pClient, smtpSink_t * in_pSink );
void cleanupServerError( smtpClient_t * in_pClient, smtpSink_t * fin_pSink, char * out_serverError );

#endif /*SMTP_HIGHLEVEL_API*/

#endif /* SMTP_H */
