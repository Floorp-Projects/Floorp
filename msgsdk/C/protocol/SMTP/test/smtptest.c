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
  
/* USAGE: SMTPTest.exe server domain mailFrom rcptTo messageFile */

#include "smtp.h"
#include "nsStream.h"
#include "testsink.h"
#include <time.h>

/*Function prototype for settting sink pointers*/
void setSink( smtpSink_t * pSink );

/*Function for reading data from a file.*/
int Test_read( void *rock, char * buf, unsigned size )
{
    if ( fgets( buf, size, (FILE*)rock ) == NULL )
    {
        return -1;
    }

    return strlen( buf );
}

int main( int argc, char *argv[ ] )
{
    int l_nReturn;
    FILE * pFile;
    nsmail_inputstream_t l_inputStream;
    smtpClient_t * pClient = NULL;
    smtpSink_t * pSink = NULL;

    /*Initialize the response sink.*/
    l_nReturn = smtpSink_initialize( &pSink );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    /*Set the function pointers on the response sink.*/
    setSink( pSink );

    /*Initialize the client passing in the response sink.*/
    l_nReturn = smtp_initialize( &pClient, pSink );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    /*Set the internal buffer chunk size.*/
    l_nReturn = smtp_setChunkSize( pClient, 1048576 );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    /*Connect to the SMTP server.*/
    l_nReturn = smtp_connect( pClient, argv[1], 25 );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    l_nReturn = smtp_processResponses( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    /*Send the EHLO command passing in the domain name.*/
    l_nReturn = smtp_ehlo( pClient, argv[2] );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    l_nReturn = smtp_processResponses( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    /*Send the MAIL FROM command.*/
    l_nReturn = smtp_mailFrom( pClient, argv[3], NULL );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    l_nReturn = smtp_processResponses( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    /*Send the RCPT TO command.*/
    l_nReturn = smtp_rcptTo( pClient, argv[4], NULL );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    l_nReturn = smtp_processResponses( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    /*Send the DATA command.*/
    l_nReturn = smtp_data( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    l_nReturn = smtp_processResponses( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    /*Open the file for reading.*/
    pFile = fopen( argv[5], "rb" );

    if ( pFile == NULL )
    {
        return -1;
    }

    /* Setup the input stream.*/
    l_inputStream.read = Test_read;
    l_inputStream.rock = pFile;

    /* Send the message.*/
    l_nReturn = smtp_sendStream( pClient, &l_inputStream );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    /*Close the file.*/
    fclose( pFile );

    l_nReturn = smtp_processResponses( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    /*Send the EXPN command.*/
    l_nReturn = smtp_expand( pClient, argv[3] );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    l_nReturn = smtp_processResponses( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    /*Send the HELP command.*/
    l_nReturn = smtp_help( pClient, argv[3] );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    l_nReturn = smtp_processResponses( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    /*Send the NOOP command.*/
    l_nReturn = smtp_noop( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    l_nReturn = smtp_processResponses( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    /*Send the RSET command.*/
    l_nReturn = smtp_reset( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    l_nReturn = smtp_processResponses( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    /*Send the VRFY command.*/
    l_nReturn = smtp_verify( pClient, argv[3] );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    l_nReturn = smtp_processResponses( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    /*Send a generic command to the server.*/
    l_nReturn = smtp_sendCommand( pClient, "HELP help" );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    l_nReturn = smtp_processResponses( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    l_nReturn = smtp_quit( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    l_nReturn = smtp_processResponses( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    /*Free the client structure.*/
    smtp_free( &pClient );
    /*Free the sink structure.*/
    smtpSink_free( &pSink );

    return 0;
}

/*Function to set the sink pointers.*/
void setSink( smtpSink_t * pSink )
{
    pSink->bdat = SMTPTest_bdat;
    pSink->connect = SMTPTest_connect;
    pSink->data = SMTPTest_data;
    pSink->ehlo = SMTPTest_ehlo;
    pSink->ehloComplete = SMTPTest_ehloComplete;
    pSink->expand = SMTPTest_expand;
    pSink->expandComplete = SMTPTest_expandComplete;
    pSink->help = SMTPTest_help;
    pSink->helpComplete = SMTPTest_helpComplete;
    pSink->mailFrom = SMTPTest_mailFrom;
    pSink->noop = SMTPTest_noop;
    pSink->quit = SMTPTest_quit;
    pSink->rcptTo = SMTPTest_rcptTo;
    pSink->reset = SMTPTest_reset;
    pSink->send = SMTPTest_send;
    pSink->sendCommand = SMTPTest_sendCommand;
    pSink->sendCommandComplete = SMTPTest_sendCommandComplete;
    pSink->verify = SMTPTest_verify;
}
