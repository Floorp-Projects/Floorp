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

#include "smtp.h"
#include "nsStream.h"
#include "nsString.h"
#include "nsSMTPServerCallback.h"

void setSink(smtpSink_t * pSink, nsISMTPObserver * aObserver)
{
  pSink->bdat                 = SMTPServerCallback_bdat;
  pSink->connect              = SMTPServerCallback_connect;
  pSink->data                 = SMTPServerCallback_data;
  pSink->ehlo                 = SMTPServerCallback_ehlo;
  pSink->ehloComplete         = SMTPServerCallback_ehloComplete;
  pSink->error                = SMTPServerCallback_error;
  pSink->expand               = SMTPServerCallback_expand;
  pSink->expandComplete       = SMTPServerCallback_expandComplete;
  pSink->help                 = SMTPServerCallback_help;
  pSink->helpComplete         = SMTPServerCallback_helpComplete;
  pSink->mailFrom             = SMTPServerCallback_mailFrom;
  pSink->noop                 = SMTPServerCallback_noop;
  pSink->quit                 = SMTPServerCallback_quit;
  pSink->rcptTo               = SMTPServerCallback_rcptTo;
  pSink->reset                = SMTPServerCallback_reset;
  pSink->send                 = SMTPServerCallback_send;
  pSink->sendCommand          = SMTPServerCallback_sendCommand;
  pSink->sendCommandComplete  = SMTPServerCallback_sendCommandComplete;
  pSink->verify               = SMTPServerCallback_verify;

  pSink->pOpaqueData = (void*)aObserver;
}

// XXX: what to do for responses that have no code or message from server (ie completion callbacks)? 
//      Right now, we send '0' to the observer for the code and a cusom message for the response.

/*Notification for the response to the BDAT command.*/
void SMTPServerCallback_bdat( smtpSink_t * in_pSink, int in_responseCode, const char * in_responseMessage )
{
  nsISMTPObserver * obs = (nsISMTPObserver *) in_pSink->pOpaqueData;

  if (nsnull != obs)
    obs->ServerResponse(nsSMTPMethod_bdat, in_responseCode, nsString(in_responseMessage));
}

/*Notification for the response to the connection to the server.*/
void SMTPServerCallback_connect( smtpSink_t * in_pSink, int in_responseCode, const char * in_responseMessage )
{
  nsISMTPObserver * obs = (nsISMTPObserver *) in_pSink->pOpaqueData;

  if (nsnull != obs)
    obs->ServerResponse(nsSMTPMethod_connect, in_responseCode, nsString(in_responseMessage));
}

/*Notification for the response to the DATA command.*/
void SMTPServerCallback_data( smtpSink_t * in_pSink, int in_responseCode, const char * in_responseMessage )
{
  nsISMTPObserver * obs = (nsISMTPObserver *) in_pSink->pOpaqueData;

  if (nsnull != obs)
    obs->ServerResponse(nsSMTPMethod_data, in_responseCode, nsString(in_responseMessage));
}

/*Notification for the response to the EHLO command.*/
void SMTPServerCallback_ehlo( smtpSink_t * in_pSink, int in_responseCode, const char * in_responseMessage )
{
  nsISMTPObserver * obs = (nsISMTPObserver *) in_pSink->pOpaqueData;

  if (nsnull != obs)
    obs->ServerResponse(nsSMTPMethod_ehlo, in_responseCode, nsString(in_responseMessage));
}

/*Notification for the completion of the EHLO command.*/
void SMTPServerCallback_ehloComplete(smtpSink_t * in_pSink)
{
  nsISMTPObserver * obs = (nsISMTPObserver *) in_pSink->pOpaqueData;

  if (nsnull != obs)
    obs->ServerResponse(nsSMTPMethod_ehloComplete, 0, nsString("EHLO complete\n"));

}

/*Notification for the response to a server error.*/
void SMTPServerCallback_error(smtpSink_t * in_pSink, int in_responseCode, const char * in_responseMessage )
{
  nsISMTPObserver * obs = (nsISMTPObserver *) in_pSink->pOpaqueData;

  if (nsnull != obs)
    obs->ServerResponse(nsSMTPMethod_error, in_responseCode, nsString(in_responseMessage));
}

/*Notification for the response to the EXPN command.*/
void SMTPServerCallback_expand( smtpSink_t * in_pSink, int in_responseCode, const char * in_responseMessage )
{
  nsISMTPObserver * obs = (nsISMTPObserver *) in_pSink->pOpaqueData;

  if (nsnull != obs)
    obs->ServerResponse(nsSMTPMethod_expand, in_responseCode, nsString(in_responseMessage));
}

/*Notification for the completion of the EXPN command.*/
void SMTPServerCallback_expandComplete(smtpSink_t * in_pSink)
{
  nsISMTPObserver * obs = (nsISMTPObserver *) in_pSink->pOpaqueData;

  if (nsnull != obs)
    obs->ServerResponse(nsSMTPMethod_expandComplete, 0, nsString("EXPAND complete\n"));
}

/*Notification for the response to the HELP command.*/
void SMTPServerCallback_help( smtpSink_t * in_pSink, int in_responseCode, const char * in_responseMessage )
{
  nsISMTPObserver * obs = (nsISMTPObserver *) in_pSink->pOpaqueData;

  if (nsnull != obs)
    obs->ServerResponse(nsSMTPMethod_help, in_responseCode, nsString(in_responseMessage));
}

/*Notification for the completion of the HELP command.*/
void SMTPServerCallback_helpComplete(smtpSink_t * in_pSink)
{
  nsISMTPObserver * obs = (nsISMTPObserver *) in_pSink->pOpaqueData;

  if (nsnull != obs)
    obs->ServerResponse(nsSMTPMethod_helpComplete, 0, nsString("HElp Complete\n"));
}

/*Notification for the response to the MAIL FROM command.*/
void SMTPServerCallback_mailFrom( smtpSink_t * in_pSink, int in_responseCode, const char * in_responseMessage )
{
  nsISMTPObserver * obs = (nsISMTPObserver *) in_pSink->pOpaqueData;

  if (nsnull != obs)
    obs->ServerResponse(nsSMTPMethod_mailFrom, in_responseCode, nsString(in_responseMessage));
}

/*Notification for the response to the NOOP command.*/
void SMTPServerCallback_noop( smtpSink_t * in_pSink, int in_responseCode, const char * in_responseMessage )
{
  nsISMTPObserver * obs = (nsISMTPObserver *) in_pSink->pOpaqueData;

  if (nsnull != obs)
    obs->ServerResponse(nsSMTPMethod_noop, in_responseCode, nsString(in_responseMessage));
}

/*Notification for the response to the QUIT command.*/
void SMTPServerCallback_quit( smtpSink_t * in_pSink, int in_responseCode, const char * in_responseMessage )
{
  nsISMTPObserver * obs = (nsISMTPObserver *) in_pSink->pOpaqueData;

  if (nsnull != obs)
    obs->ServerResponse(nsSMTPMethod_quit, in_responseCode, nsString(in_responseMessage));
}

/*Notification for the response to the RCPT TO command.*/
void SMTPServerCallback_rcptTo( smtpSink_t * in_pSink, int in_responseCode, const char * in_responseMessage )
{
  nsISMTPObserver * obs = (nsISMTPObserver *) in_pSink->pOpaqueData;

  if (nsnull != obs)
    obs->ServerResponse(nsSMTPMethod_rcpTo, in_responseCode, nsString(in_responseMessage));
}

/*Notification for the response to the RSET command.*/
void SMTPServerCallback_reset( smtpSink_t * in_pSink, int in_responseCode, const char * in_responseMessage )
{
  nsISMTPObserver * obs = (nsISMTPObserver *) in_pSink->pOpaqueData;

  if (nsnull != obs)
    obs->ServerResponse(nsSMTPMethod_reset, in_responseCode, nsString(in_responseMessage));
}

/*Notification for the response to sending the message.*/
void SMTPServerCallback_send( smtpSink_t * in_pSink, int in_responseCode, const char * in_responseMessage )
{
  nsISMTPObserver * obs = (nsISMTPObserver *) in_pSink->pOpaqueData;

  if (nsnull != obs)
    obs->ServerResponse(nsSMTPMethod_send, in_responseCode, nsString(in_responseMessage));
}

/*Notification for the response to sending a generic command.*/
void SMTPServerCallback_sendCommand( smtpSink_t * in_pSink, int in_responseCode, const char * in_responseMessage )
{
  nsISMTPObserver * obs = (nsISMTPObserver *) in_pSink->pOpaqueData;

  if (nsnull != obs)
    obs->ServerResponse(nsSMTPMethod_sendCommand, in_responseCode, nsString(in_responseMessage));

}

/*Notification for the completion of send a generic command.*/
void SMTPServerCallback_sendCommandComplete(smtpSink_t * in_pSink)
{
  nsISMTPObserver * obs = (nsISMTPObserver *) in_pSink->pOpaqueData;

  if (nsnull != obs)
    obs->ServerResponse(nsSMTPMethod_sendCommandComplete, 0, nsString("SENDCOMMAND complete\n"));
}

/*Notification for the response to the VRFY command.*/
void SMTPServerCallback_verify( smtpSink_t * in_pSink, int in_responseCode, const char * in_responseMessage )
{
  nsISMTPObserver * obs = (nsISMTPObserver *) in_pSink->pOpaqueData;

  if (nsnull != obs)
    obs->ServerResponse(nsSMTPMethod_verify, in_responseCode, nsString(in_responseMessage));
}


