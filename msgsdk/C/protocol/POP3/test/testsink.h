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

void POP3Test_connect( pop3Sink_t * in_pPOP3Sink, const char * in_responseMessage );
void POP3Test_delete( pop3Sink_t * in_pPOP3Sink, const char * in_responseMessage );
void POP3Test_error( pop3Sink_t * in_pPOP3Sink, const char * in_responseMessage );
void POP3Test_listStart( pop3Sink_t * in_pPOP3Sink );
void POP3Test_list( pop3Sink_t * in_pPOP3Sink, int in_messageNumber, int in_octetCount );
void POP3Test_listComplete( pop3Sink_t * in_pPOP3Sink );
void POP3Test_noop( pop3Sink_t * in_pPOP3Sink );
void POP3Test_pass(pop3Sink_t * in_pPOP3Sink, const char * in_responseMessage );
void POP3Test_quit(pop3Sink_t * in_pPOP3Sink, const char * in_responseMessage );
void POP3Test_reset(pop3Sink_t * in_pPOP3Sink, const char * in_responseMessage );
void POP3Test_retrieveStart( pop3Sink_t * in_pPOP3Sink, int in_messageNumber, int in_octetCount );
void POP3Test_retrieve( pop3Sink_t * in_pPOP3Sink, const char * in_messageChunk );
void POP3Test_retrieveComplete( pop3Sink_t * in_pPOP3Sink );
void POP3Test_sendCommandStart( pop3Sink_t * in_pPOP3Sink );
void POP3Test_sendCommand( pop3Sink_t * in_pPOP3Sink, const char * in_responseMessage );
void POP3Test_sendCommandComplete( pop3Sink_t * in_pPOP3Sink );
void POP3Test_stat( pop3Sink_t * in_pPOP3Sink, int in_messageCount, int in_octetCount );
void POP3Test_topStart( pop3Sink_t * in_pPOP3Sink, int in_messageNumber );
void POP3Test_top( pop3Sink_t * in_pPOP3Sink, const char * in_responseLine );
void POP3Test_topComplete( pop3Sink_t * in_pPOP3Sink );
void POP3Test_uidListStart( pop3Sink_t * in_pPOP3Sink );
void POP3Test_uidList( pop3Sink_t * in_pPOP3Sink, int in_messageNumber, const char * in_uid );
void POP3Test_uidListComplete( pop3Sink_t * in_pPOP3Sink );
void POP3Test_user( pop3Sink_t * in_pPOP3Sink, const char * in_responseMessage );
void POP3Test_xAuthListStart( pop3Sink_t * in_pPOP3Sink );
void POP3Test_xAuthList( pop3Sink_t * in_pPOP3Sink, int in_messageNumber, const char * in_responseMessage );
void POP3Test_xAuthListComplete( pop3Sink_t * in_pPOP3Sink );
void POP3Test_xSender( pop3Sink_t * in_pPOP3Sink, const char * in_emailAddress );

#endif /* TESTSINK_H */
