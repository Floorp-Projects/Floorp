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

/* USAGE: pop3test.exe server user password */

#include "pop3.h"
#include "testsink.h"

/*Function prototype for settting sink pointers*/
void setSink( pop3Sink_t * pSink );

int main( int argc, char *argv[ ] )
{
    int l_nReturn;
    pop3Client_t * pClient = NULL;
    pop3Sink_t * pSink = NULL;

    /*Initialize the response sink.*/
    l_nReturn = pop3Sink_initialize( &pSink );

    if ( l_nReturn != NSMAIL_OK )
    {
        return -1;
    }

    /*Set the function pointers on the response sink.*/
    setSink( pSink );

    /*Initialize the client passing in the response sink.*/
    l_nReturn = pop3_initialize( &pClient, pSink );

    if ( l_nReturn != NSMAIL_OK )
    {
        return -1;
    }

    /*Connect to the POP3 server.*/
    l_nReturn = pop3_connect( pClient, argv[1], 110 );

    if ( l_nReturn != NSMAIL_OK )
    {
        return -1;
    }

    l_nReturn = pop3_processResponses( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return -1;
    }

    /*Specify a user name.*/
    l_nReturn = pop3_user( pClient, argv[2] );

    if ( l_nReturn != NSMAIL_OK )
    {
        return -1;
    }

    l_nReturn = pop3_processResponses( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return -1;
    }

    /*Specify a password.*/
    l_nReturn = pop3_pass( pClient, argv[3] );

    if ( l_nReturn != NSMAIL_OK )
    {
        return -1;
    }

    l_nReturn = pop3_processResponses( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return -1;
    }

    /*Delete a message on the server.*/
    l_nReturn = pop3_delete( pClient, 1 );

    if ( l_nReturn != NSMAIL_OK )
    {
        return -1;
    }

    l_nReturn = pop3_processResponses( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return -1;
    }

    /*Undelete any messages marked for deletion.*/
    l_nReturn = pop3_reset( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return -1;
    }

    l_nReturn = pop3_processResponses( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return -1;
    }

    /*List all messages.*/
    l_nReturn = pop3_list( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return -1;
    }

    l_nReturn = pop3_processResponses( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return -1;
    }

    /*List a specified message.*/
    l_nReturn = pop3_listA( pClient, 1 );

    if ( l_nReturn != NSMAIL_OK )
    {
        return -1;
    }

    l_nReturn = pop3_processResponses( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return -1;
    }

    /*Noop operation.*/
    l_nReturn = pop3_noop( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return -1;
    }

    l_nReturn = pop3_processResponses( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return -1;
    }

    /*Perform the stat command.*/
    l_nReturn = pop3_stat( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return -1;
    }

    l_nReturn = pop3_processResponses( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return -1;
    }

    /*List all messages and their UIDs.*/
    l_nReturn = pop3_uidList( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return -1;
    }

    l_nReturn = pop3_processResponses( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return -1;
    }

    /*List a message and its UID.*/
    l_nReturn = pop3_uidListA( pClient, 1 );

    if ( l_nReturn != NSMAIL_OK )
    {
        return -1;
    }

    l_nReturn = pop3_processResponses( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return -1;
    }

    /*List all messages and authenticated users.*/
    l_nReturn = pop3_xAuthList( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return -1;
    }

    l_nReturn = pop3_processResponses( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return -1;
    }

    /*List a message and its authenticated user.*/
    l_nReturn = pop3_xAuthListA( pClient, 1 );

    if ( l_nReturn != NSMAIL_OK )
    {
        return -1;
    }

    l_nReturn = pop3_processResponses( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return -1;
    }

    /*Give the e-mail address.*/
    l_nReturn = pop3_xSender( pClient, 1 );

    if ( l_nReturn != NSMAIL_OK )
    {
        return -1;
    }

    l_nReturn = pop3_processResponses( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return -1;
    }

    /*Perform the top command for header info and a specified number of lines.*/
    l_nReturn = pop3_top( pClient, 1, 10 );

    if ( l_nReturn != NSMAIL_OK )
    {
        return -1;
    }

    l_nReturn = pop3_processResponses( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return -1;
    }

    /*Retrieve a message.*/
    l_nReturn = pop3_retrieve( pClient, 2 );

    if ( l_nReturn != NSMAIL_OK )
    {
        return -1;
    }

    l_nReturn = pop3_processResponses( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return -1;
    }

    /*Quit.*/
    l_nReturn = pop3_quit( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return -1;
    }

    l_nReturn = pop3_processResponses( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return -1;
    }

    /*Free the client structure.*/
    pop3_free( &pClient );
    /*Free the sink structure.*/
    pop3Sink_free( &pSink );

    return 0;
}

/*Function to set the sink pointers.*/
void setSink( pop3Sink_t * pSink )
{
    pSink->connect = POP3Test_connect;
    pSink->dele = POP3Test_delete;
    pSink->error = POP3Test_error;
    pSink->listStart = POP3Test_listStart;
    pSink->list = POP3Test_list;
    pSink->listComplete = POP3Test_listComplete;
    pSink->noop = POP3Test_noop;
    pSink->pass = POP3Test_pass;
    pSink->quit = POP3Test_quit;
    pSink->reset = POP3Test_reset;
    pSink->retrieveStart = POP3Test_retrieveStart;
    pSink->retrieve = POP3Test_retrieve;
    pSink->retrieveComplete = POP3Test_retrieveComplete;
    pSink->sendCommandStart = POP3Test_sendCommandStart;
    pSink->sendCommand = POP3Test_sendCommand;
    pSink->sendCommandComplete = POP3Test_sendCommandComplete;
    pSink->stat = POP3Test_stat;
    pSink->topStart = POP3Test_topStart;
    pSink->top = POP3Test_top;
    pSink->topComplete = POP3Test_topComplete;
    pSink->uidListStart = POP3Test_uidListStart;
    pSink->uidList = POP3Test_uidList;
    pSink->uidListComplete = POP3Test_uidListComplete;
    pSink->user = POP3Test_user;
    pSink->xAuthListStart = POP3Test_xAuthListStart;
    pSink->xAuthList = POP3Test_xAuthList;
    pSink->xAuthListComplete = POP3Test_xAuthListComplete;
    pSink->xSender = POP3Test_xSender;
}
