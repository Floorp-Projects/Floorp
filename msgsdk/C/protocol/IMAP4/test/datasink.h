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

#ifndef DATASINK_H
#define DATASINK_H

#include <stdio.h>
#include "imap4.h"

void taggedLine(imap4SinkPtr_t in_pimap4Sink, char* in_tag, const char* in_status, const char* in_reason);
void error(imap4SinkPtr_t in_pimap4Sink, const char* in_tag, const char* in_status, const char* in_reason);
void ok(imap4SinkPtr_t in_pimap4Sink, const char* in_responseCode, const char* in_information);
void no(imap4SinkPtr_t in_pimap4Sink, const char* in_responseCode, const char* in_information);
void bad(imap4SinkPtr_t in_pimap4Sink, const char* in_responseCode, const char* in_information);
void rawResponse(imap4SinkPtr_t in_pimap4Sink, const char* in_data);

/*Fetch Response*/
void fetchStart(imap4SinkPtr_t in_pimap4Sink, int in_msg);
void fetchEnd(imap4SinkPtr_t in_pimap4Sink);
void fetchSize(imap4SinkPtr_t in_pimap4Sink, int in_size);
void fetchData(imap4SinkPtr_t in_pimap4Sink, const char* in_data, int in_bytesRead, int in_totalBytes);
void fetchFlags(imap4SinkPtr_t in_pimap4Sink, const char* in_flags);
void fetchBodyStructure(imap4SinkPtr_t in_pimap4Sink, const char* in_bodyStructure);
void fetchEnvelope(imap4SinkPtr_t in_pimap4Sink, const char** in_value, int in_valueLength);
void fetchInternalDate(imap4SinkPtr_t in_pimap4Sink, const char* in_internalDate);
void fetchHeader(imap4SinkPtr_t in_pimap4Sink, const char* in_field, const char* in_value);
void fetchUid(imap4SinkPtr_t in_pimap4Sink, int in_uid);

/*Lsub Response*/
void lsub(imap4SinkPtr_t in_pimap4Sink, const char* in_attribute, const char* in_delimeter, const char* in_name);

/*List Response*/
void list(imap4SinkPtr_t in_pimap4Sink, const char* in_attribute, const char* in_delimeter, const char* in_name);

/*Search Response*/
void searchStart(imap4SinkPtr_t in_pimap4Sink);
void search(imap4SinkPtr_t in_pimap4Sink, int in_message);
void searchEnd(imap4SinkPtr_t in_pimap4Sink);

/*Status Response*/
void statusMessages(imap4SinkPtr_t in_pimap4Sink, int in_messages);
void statusRecent(imap4SinkPtr_t in_pimap4Sink, int in_recent);
void statusUidnext(imap4SinkPtr_t in_pimap4Sink, int in_uidNext);
void statusUidvalidity(imap4SinkPtr_t in_pimap4Sink, int in_uidValidity);
void statusUnseen(imap4SinkPtr_t in_pimap4Sink, int in_unSeen);

/*Capability Response*/
void capability(imap4SinkPtr_t in_pimap4Sink, const char* in_listing);

/*Exists Response*/
void exists(imap4SinkPtr_t in_pimap4Sink, int in_messages);

/*Expunge Response*/
void expunge(imap4SinkPtr_t in_pimap4Sink, int in_message);

/*Recent Response*/
void recent(imap4SinkPtr_t in_pimap4Sink, int in_messages);

/*Flags Response*/
void flags(imap4SinkPtr_t in_pimap4Sink, const char* in_flags);

/*Bye Response*/
void bye(imap4SinkPtr_t in_pimap4Sink, const char* in_reason);

/*Namespace Response*/
void nameSpaceStart(imap4SinkPtr_t in_pimap4Sink);
void nameSpacePersonal(imap4SinkPtr_t in_pimap4Sink, const char* in_personal);
void nameSpaceOtherUsers(imap4SinkPtr_t in_pimap4Sink, const char* in_otherUsers);
void nameSpaceShared(imap4SinkPtr_t in_pimap4Sink, const char* in_shared);
void nameSpaceEnd(imap4SinkPtr_t in_pimap4Sink);


/*ACL Responses*/
void aclStart(imap4SinkPtr_t in_pimap4Sink, const char* in_mailbox);
void aclIdentifierRight(imap4SinkPtr_t in_pimap4Sink, const char* in_identifier, const char* in_rights);
void aclEnd(imap4SinkPtr_t in_pimap4Sink);

/*LISTRIGHTS Responses*/
void listRightsStart(imap4SinkPtr_t in_pimap4Sink, const char* in_mailbox, const char* in_identifier);
void listRightsRequiredRights(imap4SinkPtr_t in_pimap4Sink, const char* in_requiredRights);
void listRightsOptionalRights(imap4SinkPtr_t in_pimap4Sink, const char* in_optionalRights);
void listRightsEnd(imap4SinkPtr_t in_pimap4Sink);

/*MYRIGHTS Responses*/
void myRights(imap4SinkPtr_t in_pimap4Sink, const char* in_mailbox, const char* in_rights);


/*Data Structures*/
char* sink_fetchUIDResults;
boolean bStoreUID;

#endif /*DATASINK_H*/

