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

#ifndef SMTP_H
#define SMTP_H

/*
 * smtp.h
 * @author derekt@netscape.com
 * @version 1.0
 */

#include "nsmail.h"
#include <stdio.h>

/* Structure used by a client to communicate to a SMTP server. */
typedef struct smtpClient smtpClient_t;

/* Forward declaration of the SMTP notification sink. */
typedef struct smtpSink * smtpSinkPtr_t;

/* Definition of the SMTP notification sink. */
typedef struct smtpSink 
{
    /* User data. */
    void * pOpaqueData;
    /* Notification for the response to the BDAT command. */
    void (*bdat)( smtpSinkPtr_t in_psmtpSink, int in_responseCode, const char * in_responseMessage );
    /* Notification for the response to the connection to the server. */
    void (*connect)( smtpSinkPtr_t in_psmtpSink, int in_responseCode, const char * in_responseMessage );
    /* Notification for the response to the DATA command. */
    void (*data)( smtpSinkPtr_t in_psmtpSink, int in_responseCode, const char * in_responseMessage );
    /* Notification for the response to the EHLO command. */
    void (*ehlo)( smtpSinkPtr_t in_psmtpSink, int in_responseCode, const char * in_serverExtension );
    /* Notification for the completion of the EHLO command. */
    void (*ehloComplete)(smtpSinkPtr_t in_psmtpSink );
    /* Notification for an error. */
    void (*error)( smtpSinkPtr_t in_psmtpSink, int in_responseCode, const char * in_errorMessage );
    /* Notification for the response to the EXPN command. */
    void (*expand)( smtpSinkPtr_t in_psmtpSink, int in_responseCode, const char * in_emailAddress );
    /* Notification for the completion of the EXPN command. */
    void (*expandComplete)(smtpSinkPtr_t in_psmtpSink);
    /* Notification for the response to the HELP command. */
    void (*help)( smtpSinkPtr_t in_psmtpSink, int in_responseCode, const char * in_helpMessage );
    /* Notification for the completion of the HELP command. */
    void (*helpComplete)(smtpSinkPtr_t in_psmtpSink );
    /* Notification for the response to the MAIL FROM command. */
    void (*mailFrom)( smtpSinkPtr_t in_psmtpSink, int in_responseCode, const char * in_responseMessage );
    /* Notification for the response to the NOOP command. */
    void (*noop)( smtpSinkPtr_t in_psmtpSink, int in_responseCode, const char * in_responseMessage );
    /* Notification for the response to the QUIT command. */
    void (*quit)( smtpSinkPtr_t in_psmtpSink, int in_responseCode, const char * in_responseMessage );
    /* Notification for the response to the RCPT TO command. */
    void (*rcptTo)( smtpSinkPtr_t in_psmtpSink, int in_responseCode, const char * in_responseMessage );
    /* Notification for the response to the RSET command. */
    void (*reset)( smtpSinkPtr_t in_psmtpSink, int in_responseCode, const char * in_responseMessage );
    /* Notification for the response to data sent to the server. */
    void (*send)( smtpSinkPtr_t in_psmtpSink, int in_responseCode, const char * in_responseMessage );
    /* Notification for the response to sendCommand() method. */
    void (*sendCommand)( smtpSinkPtr_t in_psmtpSink, int in_responseCode, const char * in_responseMessage );
    /* Notification for the completion of the extended command. */
    void (*sendCommandComplete)(smtpSinkPtr_t in_psmtpSink );
    /* Notification for the response to the VRFY command. */
    void (*verify)( smtpSinkPtr_t in_psmtpSink, int in_responseCode, const char * in_responseMessage );
} smtpSink_t;

#ifdef __cplusplus
extern "C" { 
#endif /* __cplusplus */

/* Initializes and allocates the smtpClient_t structure and sets the sink. */
int smtp_initialize( smtpClient_t ** out_ppSMTP, smtpSink_t * in_psmtpSink );
/* Initializes and allocates the smtpSink_t structure. */
int smtpSink_initialize( smtpSink_t ** out_ppsmtpSink );

/* Function used to free the smtpClient_t structure and it's data members. */
void smtp_free( smtpClient_t ** in_ppSMTP );
/* Function used to free the smtpSink_t structure. */
void smtpSink_free( smtpSink_t ** in_ppsmtpSink );

/* Function used to close the socket connection. */
int smtp_disconnect( smtpClient_t * in_pSMTP );

/* Protocol commands */
int smtp_bdat( smtpClient_t * in_pSMTP, 
                const char * in_data, 
                unsigned int length, 
                boolean in_fLast );
int smtp_connect( smtpClient_t * in_pSMTP, 
                    const char * in_server, 
                    unsigned short in_port );
int smtp_data( smtpClient_t * in_pSMTP );
int smtp_ehlo( smtpClient_t * in_pSMTP, const char * in_domain );
int smtp_expand( smtpClient_t * in_pSMTP, const char * in_mailingList );
int smtp_help( smtpClient_t * in_pSMTP, const char * in_helpTopic );
int smtp_mailFrom( smtpClient_t * in_pSMTP, 
                    const char * in_reverseAddress, 
                    const char * in_esmtpParams );
int smtp_noop( smtpClient_t * in_pSMTP );
int smtp_quit( smtpClient_t * in_pSMTP );
int smtp_rcptTo( smtpClient_t * in_pSMTP, 
                    const char * in_forwardAddress, 
                    const char * in_esmtpParams );
int smtp_reset( smtpClient_t * in_pSMTP );
int smtp_send( smtpClient_t * in_pSMTP, const char * in_data );
int smtp_sendStream( smtpClient_t * in_pSMTP, nsmail_inputstream_t * in_inputStream );
int smtp_sendCommand( smtpClient_t * in_pSMTP, const char * in_command );
int smtp_verify( smtpClient_t * in_pSMTP, const char * in_user );
/*
int smtp_sendMessage( const char * in_server, 
                        const char * in_domain, 
                        const char * in_sender, 
                        const char * in_recipients, 
                        const char * in_message,
                        char * out_serverError );
int smtp_sendMessageStream( const char * in_server, 
                            const char * in_domain, 
                            const char * in_sender, 
                            const char * in_recipients, 
                            nsmail_inputstream_t * in_inputStream,
                            char * out_serverError );
*/
/*
 * A function used to process the server responses for API commands.
 * This function will invoke the callback functions provided by the user
 * for all responses that are available at the time of execution.
 */
int smtp_processResponses( smtpClient_t * in_pSMTP );

/* Sets the size of the message data chunk sent to the server. */
int smtp_setChunkSize( smtpClient_t * in_pSMTP, int in_chunkSize );

/* Attempts to enable PIPELINING. */
int smtp_setPipelining( smtpClient_t * in_pSMTP, boolean in_enablePipelining );

/* Sets a new response sink. */
int smtp_setResponseSink( smtpClient_t * in_pSMTP, smtpSink_t * in_psmtpSink );

/* Sets the timeout used in smtp_processResponses(). */
int smtp_setTimeout( smtpClient_t * in_pSMTP, double in_timeout );

/* Function used for setting io and threading models. */
int smtp_set_option( smtpClient_t * in_pSMTP, int in_option, void * in_pOptionData );

/* Function used for getting io and threading models. */
int smtp_get_option( smtpClient_t * in_pSMTP, int in_option, void * in_pOptionData );

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SMTP_H */


