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

#ifndef TESTSINK_H
#define TESTSINK_H

#include <stdio.h>

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

#endif /* TESTSINK_H */
