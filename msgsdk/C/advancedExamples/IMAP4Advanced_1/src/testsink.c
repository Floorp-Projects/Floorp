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

#include "testsink.h"

extern boolean g_serverError;

void taggedLine(imap4SinkPtr_t in_pimap4Sink, char* in_tag, const char* in_status, const char* in_reason)
{
	printf("taggedLine tag: %s\n", in_tag);
	printf("taggedLine status: %s\n", in_status);
	printf("taggedLine reason: %s\n", in_reason);
}

void error(imap4SinkPtr_t in_pimap4Sink, const char* in_tag, const char* in_status, const char* in_reason)
{
    g_serverError = TRUE;
	printf("ERROR: %s %s %s\n", in_tag, in_status, in_reason);
}

void ok(imap4SinkPtr_t in_pimap4Sink, const char* in_responseCode, const char* in_information)
{
	if(in_responseCode != NULL)
	{
		printf("OK code: %s\n", in_responseCode);
	}
	printf("OK info: %s\n", in_information );
}

void no(imap4SinkPtr_t in_pimap4Sink, const char* in_responseCode, const char* in_information)
{
	if(in_responseCode != NULL)
	{
		printf("NO code: %s\n", in_responseCode);
	}
	printf("NO info: %s\n", in_information );
}

void bad(imap4SinkPtr_t in_pimap4Sink, const char* in_responseCode, const char* in_information)
{
	if(in_responseCode != NULL)
	{
		printf("BAD code: %s\n", in_responseCode);
	}
	printf("BAD info: %s\n", in_information );
}

void rawResponse(imap4SinkPtr_t in_pimap4Sink, const char* in_data)
{
	printf("RAWRESPONSE: %s\n", in_data);
}


/*Fetch Response*/
void fetchStart(imap4SinkPtr_t in_pimap4Sink, int in_msg)
{
	printf("FetchStart: %d\n", in_msg);
}

void fetchEnd(imap4SinkPtr_t in_pimap4Sink)
{
	printf("fetchEnd\n");
}

void fetchSize(imap4SinkPtr_t in_pimap4Sink, int in_size)
{
	printf("fetchSize: %d\n", in_size);
}

void fetchData(imap4SinkPtr_t in_pimap4Sink, const char* in_data, int in_bytesRead, int in_totalBytes)
{
    printf("fetchData: %s\n BytesRead: %d TotalBytes: %d\n", in_data, in_bytesRead, in_totalBytes);
}

void fetchFlags(imap4SinkPtr_t in_pimap4Sink, const char* in_flags)
{
	printf("fetchFlags: %s\n", in_flags);
}

void fetchBodyStructure(imap4SinkPtr_t in_pimap4Sink, const char* in_bodyStructure)
{
	printf("fetchBodyStructure: %s\n", in_bodyStructure);
}

void fetchEnvelope(imap4SinkPtr_t in_pimap4Sink, const char** in_value, int in_valueLength)
{
	int i=0;
	for(i=0; i<in_valueLength; i++)
	{
		printf("fetchEnvelope: %s\n", in_value[i]);
	}
}

void fetchInternalDate(imap4SinkPtr_t in_pimap4Sink, const char* in_internalDate)
{
	printf("fetchInternalDate: %s\n", in_internalDate);
}

void fetchHeader(imap4SinkPtr_t in_pimap4Sink, const char* in_field, const char* in_value)
{
	printf("fetchHeader:%s Value:%s\n", in_field, in_value);
}

void fetchUid(imap4SinkPtr_t in_pimap4Sink, int in_uid)
{
	char l_buffer[100];

	if(bStoreUID)
	{
		if(sink_fetchUIDResults == NULL)
		{
			/*Arbitrary size for quick test*/
			sink_fetchUIDResults = (char*)malloc(100*sizeof(char));
			if(sink_fetchUIDResults == NULL)
			{
				return;
			}
			memset(sink_fetchUIDResults, 0, 100*sizeof(char));
		}
		else
		{
			strcat(sink_fetchUIDResults, ":");
		}
		sprintf(l_buffer, "%d", in_uid);
		strcat(sink_fetchUIDResults, l_buffer);
		printf("fetchUid: %s\n", sink_fetchUIDResults);
	}
	else
	{
		printf("fetchUid: %d\n", in_uid);
	}
}


/*Lsub Response*/
void lsub(imap4SinkPtr_t in_pimap4Sink, const char* in_attribute, const char* in_delimeter, const char* in_name)
{
	printf("lsub attribute: %s\n", in_attribute);
	printf("lsub delimeter: %s\n", in_delimeter);
	printf("lsub name: %s\n", in_name);
}


/*List Response*/
void list(imap4SinkPtr_t in_pimap4Sink, const char* in_attribute, const char* in_delimeter, const char* in_name)
{
	printf("list attribute: %s\n", in_attribute);
	printf("list delimeter: %s\n", in_delimeter);
	printf("list name: %s\n", in_name);
}


/*Search Response*/
void searchStart(imap4SinkPtr_t in_pimap4Sink)
{
	printf("searchStart\n");
}

void search(imap4SinkPtr_t in_pimap4Sink, int in_message)
{
	printf("search: %d\n", in_message);
}

void searchEnd(imap4SinkPtr_t in_pimap4Sink)
{
	printf("searchEnd\n");
}


/*Status Response*/
void statusMessages(imap4SinkPtr_t in_pimap4Sink, int in_messages)
{
	printf("statusMessages: %d\n", in_messages);
}

void statusRecent(imap4SinkPtr_t in_pimap4Sink, int in_recent)
{
	printf("statusRecent: %d\n", in_recent);
}

void statusUidnext(imap4SinkPtr_t in_pimap4Sink, int in_uidNext)
{
	printf("statusUidnext: %d\n", in_uidNext);
}

void statusUidvalidity(imap4SinkPtr_t in_pimap4Sink, int in_uidValidity)
{
	printf("statusUidvalidity: %d\n", in_uidValidity);
}

void statusUnseen(imap4SinkPtr_t in_pimap4Sink, int in_unSeen)
{
	printf("statusUnseen: %d\n", in_unSeen);
}


/*Capability Response*/
void capability(imap4SinkPtr_t in_pimap4Sink, const char* in_listing)
{
	printf("capability: %s\n", in_listing);
}


/*Exists Response*/
void exists(imap4SinkPtr_t in_pimap4Sink, int in_messages)
{
	printf("exists: %d\n", in_messages);
}


/*Expunge Response*/
void expunge(imap4SinkPtr_t in_pimap4Sink, int in_message)
{
	printf("expunge: %d\n", in_message);
}


/*Recent Response*/
void recent(imap4SinkPtr_t in_pimap4Sink, int in_messages)
{
	printf("recent: %d\n", in_messages);
}


/*Flags Response*/
void flags(imap4SinkPtr_t in_pimap4Sink, const char* in_flags)
{
	printf("flags: %s\n", in_flags);
}


/*Bye Response*/
void bye(imap4SinkPtr_t in_pimap4Sink, const char* in_reason)
{
	printf("bye: %s\n", in_reason);
}


/*Namespace Response*/
void nameSpaceStart(imap4SinkPtr_t in_pimap4Sink)
{
	printf("nameSpaceStart\n");
}

void nameSpacePersonal(imap4SinkPtr_t in_pimap4Sink, const char* in_personal)
{
	printf("nameSpacePersonal %s\n", in_personal);
}

void nameSpaceOtherUsers(imap4SinkPtr_t in_pimap4Sink, const char* in_otherUsers)
{
	printf("nameSpaceOtherUsers: %s\n", in_otherUsers);
}

void nameSpaceShared(imap4SinkPtr_t in_pimap4Sink, const char* in_shared)
{
	printf("nameSpaceShared: %s\n", in_shared);
}

void nameSpaceEnd(imap4SinkPtr_t in_pimap4Sink)
{
	printf("nameSpaceEnd\n");
}



/*ACL Responses*/
void aclStart(imap4SinkPtr_t in_pimap4Sink, const char* in_mailbox)
{
	printf("aclStart: %s\n", in_mailbox);
}

void aclIdentifierRight(imap4SinkPtr_t in_pimap4Sink, const char* in_identifier, const char* in_rights)
{
	printf("aclIdentifierRight: %s %s\n", in_identifier, in_rights);
}

void aclEnd(imap4SinkPtr_t in_pimap4Sink)
{
	printf("aclEnd\n");
}


/*LISTRIGHTS Responses*/
void listRightsStart(imap4SinkPtr_t in_pimap4Sink, const char* in_mailbox, const char* in_identifier)
{
	printf("listRightsStart: %s %s\n", in_mailbox, in_identifier);
}

void listRightsRequiredRights(imap4SinkPtr_t in_pimap4Sink, const char* in_requiredRights)
{
	printf("listRightsRequiredRights: %s\n", in_requiredRights);
}

void listRightsOptionalRights(imap4SinkPtr_t in_pimap4Sink, const char* in_optionalRights)
{
	printf("listRightsOptionalRights: %s\n", in_optionalRights);
}

void listRightsEnd(imap4SinkPtr_t in_pimap4Sink)
{
	printf("listRightsEnd\n");
}


/*MYRIGHTS Responses*/
void myRights(imap4SinkPtr_t in_pimap4Sink, const char* in_mailbox, const char* in_rights)
{
	printf("myRights: %s %s\n", in_mailbox, in_rights);
}

