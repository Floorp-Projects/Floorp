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
#include "nsSMTPServerCallback.h"

static NS_DEFINE_IID(kISupportsIID,     NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kSMTPServiceIID,   NS_ISMTP_SERVICE_IID);


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

nsresult nsSMTPService::SendMail(nsString& aServer, 
                                 nsIMessage& aMessage,
                                 nsISMTPObserver * aObserver) 
{
  return NS_OK;
}

nsresult nsSMTPService::SendMail(nsString& aServer, 
                                 nsString& aFrom, 
                                 nsString& aTo, 
                                 nsString& aSubject, 
                                 nsString& aBody,
                                 nsISMTPObserver * aObserver)
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

  setSink(pSink, aObserver);
  

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





