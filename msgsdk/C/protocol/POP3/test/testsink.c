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

/*
 *@author derekt@netscape.com
 *@version 1.0
 */

#include "pop3.h"
#include "testsink.h"

/*Notification for the response to the connection to the server.*/
void POP3Test_connect( pop3Sink_t * in_pPOP3Sink, const char * in_responseMessage )
{
    printf( "%s\n", in_responseMessage );
}

/*Notification for the response to the DELE command.*/
void POP3Test_delete( pop3Sink_t * in_pPOP3Sink, const char * in_responseMessage )
{
    printf( "%s\n", in_responseMessage );
}

/*Notification for an error response.*/
void POP3Test_error( pop3Sink_t * in_pPOP3Sink, const char * in_responseMessage )
{
    printf( "ERROR:%s\n", in_responseMessage );
}

/*Notification for the start of the LIST command.*/
void POP3Test_listStart( pop3Sink_t * in_pPOP3Sink )
{
    printf( "LIST Start\n" );
}

/*Notification for the response to the LIST command.*/
void POP3Test_list( pop3Sink_t * in_pPOP3Sink, int in_messageNumber, int in_octetCount )
{
    printf( "%d %d\n", in_messageNumber, in_octetCount );
}

/*Notification for the completion of the LIST command.*/
void POP3Test_listComplete( pop3Sink_t * in_pPOP3Sink )
{
    printf( "LIST Complete\n" );
}

/*Notification for the response to the NOOP command.*/
void POP3Test_noop( pop3Sink_t * in_pPOP3Sink )
{
    printf( "NOOP\n" );
}

/*Notification for the response to the PASS command.*/
void POP3Test_pass(pop3Sink_t * in_pPOP3Sink, const char * in_responseMessage )
{
    printf( "%s\n", in_responseMessage );
}

/*Notification for the response to the QUIT command.*/
void POP3Test_quit(pop3Sink_t * in_pPOP3Sink, const char * in_responseMessage )
{
    printf( "%s\n", in_responseMessage );
}

/*Notification for the response to the RSET command.*/
void POP3Test_reset(pop3Sink_t * in_pPOP3Sink, const char * in_responseMessage )
{
    printf( "%s\n", in_responseMessage );
}

/*Notification for the start of a message from the RETR command.*/
void POP3Test_retrieveStart( pop3Sink_t * in_pPOP3Sink, int in_messageNumber, int in_octetCount )
{
    FILE * l_file = fopen( "retrieve.txt", "wb" );
    in_pPOP3Sink->pOpaqueData = (void*)l_file;
    printf( "RETR Start: %d %d\n", in_messageNumber, in_octetCount );
}

/*Notification for raw message from the RETR command.*/
void POP3Test_retrieve( pop3Sink_t * in_pPOP3Sink, const char * in_messageChunk )
{
    FILE * l_file = (FILE*)in_pPOP3Sink->pOpaqueData;
    fputs( in_messageChunk, l_file );
    printf( "%s", in_messageChunk );
}

/*Notification for the completion of the RETR command.*/
void POP3Test_retrieveComplete( pop3Sink_t * in_pPOP3Sink )
{
    FILE * l_file = (FILE*)in_pPOP3Sink->pOpaqueData;
    fclose( l_file );
    printf( "%RETR Complete\n" );
}

/*Notification for the start of the extended command.*/
void POP3Test_sendCommandStart( pop3Sink_t * in_pPOP3Sink )
{
    printf( "SENDCOMMAND Start\n" );
}

/*Notification for the response to sendCommand() method.*/
void POP3Test_sendCommand( pop3Sink_t * in_pPOP3Sink, const char * in_responseMessage )
{
    printf( "%s\n", in_responseMessage );
}

/*Notification for the completion of the extended command.*/
void POP3Test_sendCommandComplete( pop3Sink_t * in_pPOP3Sink )
{
    printf( "SENDCOMMAND Complete\n" );
}

/*Notification for the response to the STAT command.*/
void POP3Test_stat( pop3Sink_t * in_pPOP3Sink, int in_messageCount, int in_octetCount )
{
    printf( "%d %d\n", in_messageCount, in_octetCount );
}

/*Notification for the start of a message from the TOP command.*/
void POP3Test_topStart( pop3Sink_t * in_pPOP3Sink, int in_messageNumber )
{
    printf( "TOP Start: %d\n", in_messageNumber );
}

/*Notification for a line of the message from the TOP command.*/
void POP3Test_top( pop3Sink_t * in_pPOP3Sink, const char * in_responseLine )
{
    printf( "%s\n", in_responseLine );
}

/*Notification for the completion of the TOP command.*/
void POP3Test_topComplete( pop3Sink_t * in_pPOP3Sink )
{
    printf( "%TOP Complete\n" );
}

/*Notification for the start of the UIDL command.*/
void POP3Test_uidListStart( pop3Sink_t * in_pPOP3Sink )
{
    printf( "UIDL Start\n" );
}

/*Notification for the response to the UIDL command.*/
void POP3Test_uidList( pop3Sink_t * in_pPOP3Sink, int in_messageNumber, const char * in_uid )
{
    printf( "%d %s\n", in_messageNumber, in_uid );
}

/*Notification for the completion of the UIDL command.*/
void POP3Test_uidListComplete( pop3Sink_t * in_pPOP3Sink )
{
    printf( "UIDL Complete\n" );
}

/**Notification for the response to the USER command.*/
void POP3Test_user( pop3Sink_t * in_pPOP3Sink, const char * in_responseMessage )
{
    printf( "%s\n", in_responseMessage );
}

/*Notification for the start of the XAUTHLIST command.*/
void POP3Test_xAuthListStart( pop3Sink_t * in_pPOP3Sink )
{
    printf( "XAUTHLIST Start\n" );
}

/*Notification for the response to the XAUTHLIST command.*/
void POP3Test_xAuthList( pop3Sink_t * in_pPOP3Sink, int in_messageNumber, const char * in_responseMessage )
{
    printf( "%d %s\n", in_messageNumber, in_responseMessage );
}

/*Notification for the completion of the XAUTHLIST command.*/
void POP3Test_xAuthListComplete( pop3Sink_t * in_pPOP3Sink )
{
    printf( "XAUTHLIST Complete\n" );
}

/*Notification for the response to the XSENDER command.*/
void POP3Test_xSender( pop3Sink_t * in_pPOP3Sink, const char * in_emailAddress )
{
    printf( "%s\n", in_emailAddress );
}

