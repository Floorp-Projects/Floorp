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

#ifndef nsSMTPServerCallback_h___
#define nsSMTPServerCallback_h___

#include "smtp.h"
#include "nsISMTPObserver.h"

void SMTPServerCallback_bdat( smtpSink_t *, int, const char * );
void SMTPServerCallback_connect( smtpSink_t *, int, const char * );
void SMTPServerCallback_data( smtpSink_t *, int, const char * );
void SMTPServerCallback_ehlo( smtpSink_t *, int, const char * );
void SMTPServerCallback_ehloComplete(smtpSink_t *);
void SMTPServerCallback_error( smtpSink_t *, int, const char * );
void SMTPServerCallback_expand( smtpSink_t *, int, const char * );
void SMTPServerCallback_expandComplete(smtpSink_t *);
void SMTPServerCallback_help( smtpSink_t *, int, const char * );
void SMTPServerCallback_helpComplete(smtpSink_t *);
void SMTPServerCallback_mailFrom( smtpSink_t *, int, const char * );
void SMTPServerCallback_noop( smtpSink_t *, int, const char * );
void SMTPServerCallback_quit( smtpSink_t *, int, const char * );
void SMTPServerCallback_rcptTo( smtpSink_t *, int, const char * );
void SMTPServerCallback_reset( smtpSink_t *, int, const char * );
void SMTPServerCallback_send( smtpSink_t *, int, const char * );
void SMTPServerCallback_sendCommand( smtpSink_t *, int, const char * );
void SMTPServerCallback_sendCommandComplete(smtpSink_t *);
void SMTPServerCallback_verify( smtpSink_t *, int, const char * );

void setSink(smtpSink_t * pSink,nsISMTPObserver * aObserver);

#endif /* nsSMTPServerCallback_h___ */
