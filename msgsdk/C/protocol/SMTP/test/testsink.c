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
  
#include "smtp.h"
#include "testsink.h"

/**
*Notification sink for SMTP commands.
*@author derekt@netscape.com
*@version 1.0
*/

/*Notification for the response to the BDAT command.*/
void SMTPTest_bdat( smtpSink_t * in_pSink, int in_responseCode, const char * in_responseMessage )
{
    printf( "%d %s\n", in_responseCode, in_responseMessage );
}

/*Notification for the response to the connection to the server.*/
void SMTPTest_connect( smtpSink_t * in_pSink, int in_responseCode, const char * in_responseMessage )
{
    printf( "%d %s\n", in_responseCode, in_responseMessage );
}

/*Notification for the response to the DATA command.*/
void SMTPTest_data( smtpSink_t * in_pSink, int in_responseCode, const char * in_responseMessage )
{
    printf( "%d %s\n", in_responseCode, in_responseMessage );
}

/*Notification for the response to the EHLO command.*/
void SMTPTest_ehlo( smtpSink_t * in_pSink, int in_responseCode, const char * in_responseMessage )
{
    printf( "%d %s\n", in_responseCode, in_responseMessage );
}

/*Notification for the completion of the EHLO command.*/
void SMTPTest_ehloComplete(smtpSink_t * in_pSink)
{
    printf( "EHLO complete\n" );
}

/*Notification for the response to a server error.*/
void SMTPTest_error(smtpSink_t * in_pSink, int in_responseCode, const char * in_responseMessage )
{
    printf( "%d %s\n", in_responseCode, in_responseMessage );
}

/*Notification for the response to the EXPN command.*/
void SMTPTest_expand( smtpSink_t * in_pSink, int in_responseCode, const char * in_responseMessage )
{
    printf( "%d %s\n", in_responseCode, in_responseMessage );
}

/*Notification for the completion of the EXPN command.*/
void SMTPTest_expandComplete(smtpSink_t * in_pSink)
{
    printf( "EXPAND complete\n" );
}

/*Notification for the response to the HELP command.*/
void SMTPTest_help( smtpSink_t * in_pSink, int in_responseCode, const char * in_responseMessage )
{
    printf( "%d %s\n", in_responseCode, in_responseMessage );
}

/*Notification for the completion of the HELP command.*/
void SMTPTest_helpComplete(smtpSink_t * in_pSink)
{
    printf( "HELP complete\n" );
}

/*Notification for the response to the MAIL FROM command.*/
void SMTPTest_mailFrom( smtpSink_t * in_pSink, int in_responseCode, const char * in_responseMessage )
{
    printf( "%d %s\n", in_responseCode, in_responseMessage );
}

/*Notification for the response to the NOOP command.*/
void SMTPTest_noop( smtpSink_t * in_pSink, int in_responseCode, const char * in_responseMessage )
{
    printf( "%d %s\n", in_responseCode, in_responseMessage );
}

/*Notification for the response to the QUIT command.*/
void SMTPTest_quit( smtpSink_t * in_pSink, int in_responseCode, const char * in_responseMessage )
{
    printf( "%d %s\n", in_responseCode, in_responseMessage );
}

/*Notification for the response to the RCPT TO command.*/
void SMTPTest_rcptTo( smtpSink_t * in_pSink, int in_responseCode, const char * in_responseMessage )
{
    printf( "%d %s\n", in_responseCode, in_responseMessage );
}

/*Notification for the response to the RSET command.*/
void SMTPTest_reset( smtpSink_t * in_pSink, int in_responseCode, const char * in_responseMessage )
{
    printf( "%d %s\n", in_responseCode, in_responseMessage );
}

/*Notification for the response to sending the message.*/
void SMTPTest_send( smtpSink_t * in_pSink, int in_responseCode, const char * in_responseMessage )
{
    printf( "%d %s\n", in_responseCode, in_responseMessage );
}

/*Notification for the response to sending a generic command.*/
void SMTPTest_sendCommand( smtpSink_t * in_pSink, int in_responseCode, const char * in_responseMessage )
{
    printf( "%d %s\n", in_responseCode, in_responseMessage );
}

/*Notification for the completion of send a generic command.*/
void SMTPTest_sendCommandComplete(smtpSink_t * in_pSink)
{
    printf( "SENDCOMMAND complete\n" );
}

/*Notification for the response to the VRFY command.*/
void SMTPTest_verify( smtpSink_t * in_pSink, int in_responseCode, const char * in_responseMessage )
{
    printf( "%d %s\n", in_responseCode, in_responseMessage );
}
