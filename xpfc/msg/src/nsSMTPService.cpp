/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "nsSMTPService.h"
#include "nsxpfcCIID.h"

#include "smtp.h"
#include "nsStream.h"
#include <time.h>
#include <stdio.h>
#include "nsCRT.h"

static NS_DEFINE_IID(kISupportsIID,     NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kSMTPServiceIID,   NS_ISMTP_SERVICE_IID);

void SMTPTest_bdat( smtpSink_t *, int, const char * );
void SMTPTest_connect( smtpSink_t *, int, const char * );
void SMTPTest_data( smtpSink_t *, int, const char * );
void SMTPTest_ehlo( smtpSink_t *, int, const char * );
void SMTPTest_ehloComplete(smtpSink_t *);
void SMTPTest_expand( smtpSink_t *, int, const char * );
void SMTPTest_expandComplete(smtpSink_t *);
void SMTPTest_help( smtpSink_t *, int, const char * );
void SMTPTest_helpComplete(smtpSink_t *);
void SMTPTest_mailFrom( smtpSink_t *, int, const char * );
void SMTPTest_noop( smtpSink_t *, int, const char * );
void SMTPTest_quit( smtpSink_t *, int, const char * );
void SMTPTest_rcptTo( smtpSink_t *, int, const char * );
void SMTPTest_reset( smtpSink_t *, int, const char * );
void SMTPTest_send( smtpSink_t *, int, const char * );
void SMTPTest_sendCommand( smtpSink_t *, int, const char * );
void SMTPTest_sendCommandComplete(smtpSink_t *);
void SMTPTest_verify( smtpSink_t *, int, const char * );

void setSink( smtpSink_t * pSink );

nsSMTPService :: nsSMTPService()
{
  NS_INIT_REFCNT();
}

nsSMTPService :: ~nsSMTPService()  
{
}

NS_IMPL_ADDREF(nsSMTPService)
NS_IMPL_RELEASE(nsSMTPService)
NS_IMPL_QUERY_INTERFACE(nsSMTPService, kSMTPServiceIID)

nsresult nsSMTPService::Init()
{
  return NS_OK;
}

nsresult nsSMTPService::SendMail(nsString& aServer, nsIMessage& aMessage) 
{
  return NS_OK;
}

nsresult nsSMTPService::SendMail(nsString& aServer, 
                                 nsString& aFrom, 
                                 nsString& aTo, 
                                 nsString& aSubject, 
                                 nsString& aBody)
{
    char * from     = aFrom.ToNewCString();
    char * to       = aTo.ToNewCString();
    char * header   = aSubject.ToNewCString();
    char * message  = nsnull;

    nsString data("Subject: ");
    data += header;
    data += "\r\n";
    data += aBody.ToNewCString();

    message  = data.ToNewCString();

    nsString strdomain ;

    PRInt32 offset = aServer.Find("@");
    aServer.Right(strdomain, aServer.Length() - offset);
    char * domain = strdomain.ToNewCString();

    nsString strserver;
    aServer.Left(strserver, offset);
    char * server   = strserver.ToNewCString();

    int l_nReturn;
    nsmail_inputstream_t * l_inputStream;
    smtpClient_t * pClient = NULL;
    smtpSink_t * pSink = NULL;

    buf_inputStream_create (message, nsCRT::strlen(message), &l_inputStream);

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
    l_nReturn = smtp_connect( pClient, server, 25 );

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
    l_nReturn = smtp_ehlo( pClient, domain );

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
    l_nReturn = smtp_mailFrom( pClient, from, NULL );

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
    l_nReturn = smtp_rcptTo( pClient, to, NULL );

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

    /* Send the message.*/
    l_nReturn = smtp_sendStream( pClient, l_inputStream );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    l_nReturn = smtp_processResponses( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    /*Send the EXPN command.*/
    l_nReturn = smtp_expand( pClient, from );

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
    l_nReturn = smtp_help( pClient, from );

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
    l_nReturn = smtp_verify( pClient, from );

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

    nsStream_free (l_inputStream);

    /*Free the client structure.*/
    smtp_free( &pClient );
    /*Free the sink structure.*/
    smtpSink_free( &pSink );

  delete server;
  delete from;
  delete to;
  delete domain;
  delete message;
  delete header;

  return NS_OK;
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


