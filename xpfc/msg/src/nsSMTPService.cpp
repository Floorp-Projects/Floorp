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
#include "nsIMIMEMessage.h"

static NS_DEFINE_IID(kISupportsIID,     NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kSMTPServiceIID,   NS_ISMTP_SERVICE_IID);
static NS_DEFINE_IID(kIMIMEMessageIID,  NS_IMIME_MESSAGE_IID);

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
  /*
   * Check to see if nsIMessage supports the MIME interface,
   * and if so , send this message as MIME as do regular 
   * plain/text mail
   */

  nsIMIMEMessage * mime_message = nsnull;
  
  nsresult res = NS_OK;    

  res = aMessage.QueryInterface(kIMIMEMessageIID, (void**)&mime_message);

  if (res == NS_OK)
    return (SendMail(aServer, *mime_message, aObserver));

  /*
   * this is not a MIME message. Send as plain text
   */

  nsString str_from;
  nsString str_to;
  nsString str_subject;
  nsString str_body;

  aMessage.GetSender(str_from);
  aMessage.GetRecipients(str_to);
  aMessage.GetSubject(str_subject);
  aMessage.GetBody(str_body);

  return (SendMail(aServer, str_from,str_to, str_subject, str_body));

}

nsresult nsSMTPService::SendMail(nsString& aServer, 
                                 nsIMIMEMessage& aMIMEMessage,
                                 nsISMTPObserver * aObserver) 
{
  return NS_OK;
}

// XXX: Need to return proper XPCOM error results (nsresult)
//      converted from the msg-sdk results

nsresult nsSMTPService::SendMail(nsString& aServer, 
                                 nsString& aFrom, 
                                 nsString& aTo, 
                                 nsString& aSubject, 
                                 nsString& aBody,
                                 nsISMTPObserver * aObserver)
{
  /*
   * Get a bunch of character strings for passing to SMTP directly
   */

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

  /*
   * Setup the sink and buffer...
   */

  PRInt32 res;
  nsmail_inputstream_t * l_inputStream;
  smtpClient_t * pClient = NULL;
  smtpSink_t * pSink = NULL;

  buf_inputStream_create (message, nsCRT::strlen(message), &l_inputStream);

  res = smtpSink_initialize( &pSink );

  if (NSMAIL_OK != res)
    return res;

  setSink(pSink, aObserver);
  
  /*
   * Now send the message ....
   */

  res = smtp_initialize( &pClient, pSink );
  if (NSMAIL_OK != res)
    return res;

  res = smtp_setChunkSize( pClient, 1048576 );
  if (NSMAIL_OK != res)
    return res;

  res = smtp_connect( pClient, server, 25 );
  if (NSMAIL_OK != res)
    return res;

  res = smtp_processResponses( pClient );
  if (NSMAIL_OK != res)
    return res;

  res = smtp_ehlo( pClient, domain );
  if (NSMAIL_OK != res)
    return res;

  res = smtp_processResponses( pClient );
  if (NSMAIL_OK != res)
    return res;

  res = smtp_mailFrom( pClient, from, NULL );
  if (NSMAIL_OK != res)
    return res;

  res = smtp_processResponses( pClient );
  if (NSMAIL_OK != res)
    return res;

  res = smtp_rcptTo( pClient, to, NULL );
  if (NSMAIL_OK != res)
    return res;

  res = smtp_processResponses( pClient );
  if (NSMAIL_OK != res)
    return res;

  res = smtp_data( pClient );
  if (NSMAIL_OK != res)
    return res;

  res = smtp_processResponses( pClient );
  if (NSMAIL_OK != res)
    return res;

  res = smtp_sendStream( pClient, l_inputStream );
  if (NSMAIL_OK != res)
    return res;

  res = smtp_processResponses( pClient );
  if (NSMAIL_OK != res)
    return res;

  res = smtp_expand( pClient, from );
  if (NSMAIL_OK != res)
    return res;

  res = smtp_processResponses( pClient );
  if (NSMAIL_OK != res)
    return res;

  res = smtp_noop( pClient );
  if (NSMAIL_OK != res)
    return res;

  res = smtp_processResponses( pClient );
  if (NSMAIL_OK != res)
    return res;

  res = smtp_reset( pClient );
  if (NSMAIL_OK != res)
    return res;

  res = smtp_processResponses( pClient );
  if (NSMAIL_OK != res)
    return res;

  res = smtp_verify( pClient, from );
  if (NSMAIL_OK != res)
    return res;

  res = smtp_processResponses( pClient );
  if (NSMAIL_OK != res)
    return res;

  res = smtp_quit( pClient );
  if (NSMAIL_OK != res)
    return res;

  res = smtp_processResponses( pClient );
  if (NSMAIL_OK != res)
    return res;

  /*
   * Cleanup 
   */

  nsStream_free (l_inputStream);
  smtp_free( &pClient );
  smtpSink_free( &pSink );

  delete server;
  delete from;
  delete to;
  delete domain;
  delete message;
  delete header;

  return NS_OK;
}





