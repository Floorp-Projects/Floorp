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

#ifndef POP3_H
#define POP3_H

/*
 * pop3.h
 * @author derekt@netscape.com
 * @version 1.0
 */

#include "nsmail.h"
#include <stdio.h>

/* Structure used by a client to communicate to a POP3 server. */
typedef struct pop3Client pop3Client_t;

/* Forward declaration of the POP3 notification sink. */
typedef struct pop3Sink * pop3SinkPtr_t;

/* Definition of the POP3 notification sink. */
typedef struct pop3Sink
{
    /* User data. */
    void * pOpaqueData;
    /* Notification for the response to the connection to the server. */
    void (*connect)( pop3SinkPtr_t in_pPOP3Sink, const char * in_responseMessage );
    /* Notification for the response to the DELE command. */
    void (*dele)( pop3SinkPtr_t in_pPOP3Sink, const char * in_responseMessage );
    /* Notification for an error. */
    void (*error)( pop3SinkPtr_t in_pPOP3Sink, const char * in_responseMessage );
    /* Notification for the start of the LIST command. */
    void (*listStart)( pop3SinkPtr_t in_pPOP3Sink );
    /* Notification for the response to the LIST command. */
    void (*list)( pop3SinkPtr_t in_pPOP3Sink, int in_messageNumber, int in_octetCount );
    /* Notification for the completion of the LIST command. */
    void (*listComplete)( pop3SinkPtr_t in_pPOP3Sink );
    /* Notification for the response to the NOOP command. */
    void (*noop)( pop3SinkPtr_t in_pPOP3Sink );
    /* Notification for the response to the PASS command. */
    void (*pass)(pop3SinkPtr_t in_pPOP3Sink, const char * in_responseMessage );
    /* Notification for the response to the QUIT command. */
    void (*quit)(pop3SinkPtr_t in_pPOP3Sink, const char * in_responseMessage );
    /* Notification for the response to the RSET command. */
    void (*reset)(pop3SinkPtr_t in_pPOP3Sink, const char * in_responseMessage );
    /* Notification for the start of the RETR command. */
    void (*retrieveStart)( pop3SinkPtr_t in_pPOP3Sink, int in_messageNumber, int in_octetCount );
    /* Notification for the response to the RETR command. */
    void (*retrieve)( pop3SinkPtr_t in_pPOP3Sink, const char * in_messageChunk );
    /* Notification for the completion of the RETR command. */
    void (*retrieveComplete)( pop3SinkPtr_t in_pPOP3Sink );
    /* Notification for the start of an extended command. */
    void (*sendCommandStart)( pop3SinkPtr_t in_pPOP3Sink );
    /* Notification for the response of an extended command. */
    void (*sendCommand)( pop3SinkPtr_t in_pPOP3Sink, const char * in_responseMessage );
    /* Notification for the completion of and extended command. */
    void (*sendCommandComplete)( pop3SinkPtr_t in_pPOP3Sink );
    /* Notification for the response to the STAT command. */
    void (*stat)( pop3SinkPtr_t in_pPOP3Sink, int in_messageCount, int in_octetCount );
    /* Notification for the start of the TOP command. */
    void (*topStart)( pop3SinkPtr_t in_pPOP3Sink, int in_messageNumber );
    /* Notification for the response to the TOP command. */
    void (*top)( pop3SinkPtr_t in_pPOP3Sink, const char * in_responseLine );
    /* Notification for the completion of the TOP command. */
    void (*topComplete)( pop3SinkPtr_t in_pPOP3Sink );
    /* Notification for the start of the UIDL command. */
    void (*uidListStart)( pop3SinkPtr_t in_pPOP3Sink );
    /* Notification for the response to the UIDL command. */
    void (*uidList)( pop3SinkPtr_t in_pPOP3Sink, int in_messageNumber, const char * in_uid );
    /* Notification for the completion of the UIDL command. */
    void (*uidListComplete)( pop3SinkPtr_t in_pPOP3Sink );
    /* Notification for the response to the USER command. */
    void (*user)( pop3SinkPtr_t in_pPOP3Sink, const char * in_responseMessage );
    /* Notification for the start of the XAUTHLIST command. */
    void (*xAuthListStart)( pop3SinkPtr_t in_pPOP3Sink );
    /* Notification for the response to the XAUTHLIST command. */
    void (*xAuthList)( pop3SinkPtr_t in_pPOP3Sink, int in_messageNumber, const char * in_responseMessage );
    /* Notification for the completion of the XAUTHLIST command. */
    void (*xAuthListComplete)( pop3SinkPtr_t in_pPOP3Sink );
    /* Notification for the response to the XSENDER command. */
    void (*xSender)( pop3SinkPtr_t in_pPOP3Sink, const char * in_emailAddress );

} pop3Sink_t;

#ifdef __cplusplus
extern "C" { 
#endif /* __cplusplus */

/* Initializes and allocates the pop3Client_t structure and sets the sink. */
int pop3_initialize( pop3Client_t ** out_ppPOP3, pop3SinkPtr_t in_pPOP3Sink );
/* Initializes and allocates the pop3Sink_t structure. */
int pop3Sink_initialize( pop3Sink_t ** out_ppPOP3Sink );

/* Function used to free the pop3Client_t structure and it's data members. */
void pop3_free( pop3Client_t ** in_ppPOP3 );
/* Function used to free the pop3Sink_t structure. */
void pop3Sink_free( pop3Sink_t ** in_ppPOP3Sink );

/* Function used to close the socket connection. */
int pop3_disconnect( pop3Client_t * in_pPOP3 );

/* Protocol commands. */
int pop3_connect( pop3Client_t * in_pPOP3, const char * in_server, unsigned short  in_port );
int pop3_delete( pop3Client_t * in_pPOP3, int in_messageNumber );
int pop3_list( pop3Client_t * in_pPOP3 );
int pop3_listA( pop3Client_t * in_pPOP3, int in_messageNumber );
int pop3_noop( pop3Client_t * in_pPOP3 );
int pop3_pass( pop3Client_t * in_pPOP3, const char * in_password );
int pop3_quit( pop3Client_t * in_pPOP3 );
int pop3_reset( pop3Client_t * in_pPOP3 );
int pop3_retrieve( pop3Client_t * in_pPOP3, int in_messageNumber );
int pop3_sendCommand( pop3Client_t * in_pPOP3, const char * in_command, boolean in_multiLine );
int pop3_stat( pop3Client_t * in_pPOP3 );
int pop3_top( pop3Client_t * in_pPOP3, int in_messageNumber, int in_lines );
int pop3_uidList( pop3Client_t * in_pPOP3 );
int pop3_uidListA( pop3Client_t * in_pPOP3, int in_messageNumber );
int pop3_user( pop3Client_t * in_pPOP3, const char * in_user );
int pop3_xAuthList( pop3Client_t * in_pPOP3 );
int pop3_xAuthListA( pop3Client_t * in_pPOP3, int in_messageNumber );
int pop3_xSender( pop3Client_t * in_pPOP3, int in_messageNumber );

/*
 * A function used to process the server responses for API commands.
 * This function will invoke the callback functions provided by the user
 * for all responses that are available at the time of execution.
 */
int pop3_processResponses( pop3Client_t * in_pPOP3 );

/* Sets the size of the message data chunk passed to the user. */
int pop3_setChunkSize( pop3Client_t * in_pPOP3, int in_chunkSize );

/* Sets a new response sink. */
int pop3_setResponseSink( pop3Client_t * in_pPOP3, pop3SinkPtr_t in_pPOP3Sink );

/* Sets the timeout used in pop3_processResponses(). */
int pop3_setTimeout( pop3Client_t * in_pPOP3, double in_timeout );

/* Function used for setting io and threading models. */
int pop3_set_option( pop3Client_t * in_pPOP3, int in_option, void * in_pOptionData );

/* Function used for getting io and threading models. */
int pop3_get_option( pop3Client_t * in_pPOP3, int in_option, void * in_pOptionData );

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* POP3_H */
