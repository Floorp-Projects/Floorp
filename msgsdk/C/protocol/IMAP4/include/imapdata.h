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

#ifndef IMAPDATA_H
#define IMAPDATA_H

#include "nsio.h"

/************************************************************************************ 
* File: imapdata.h
* This header contains general internal definitions
************************************************************************************/

/*********************************************************************************** 
* Structure used by a client to communicate to an imap4 server.
************************************************************************************/ 
typedef struct imap4Client 
{ 
	/*Communication structure.*/
    IO_t* pIO; 

	/*Get/Set variables*/
    int chunkSize;
    double timeout;
    imap4Sink_t * pimap4Sink;

	/*Tracks pending commands.. used by processResponses*/ 
    LinkedList_t * pPendingCommands; 

	/*Input stream for append*/
	nsmail_inputstream_t* pLiteralStream;

	/*The tagID associated with commands sent to the server*/
	long tagID;

	/*Flags*/
	boolean bReadHeader;
	boolean bConnected;

}imap4Client_i_t; 

/*Internal object for storing the intermediate data while waiting for the server response*/ 
typedef struct CommandObject 
{ 
	struct CommandObject * next;
	char* pTag;
	char* pCommand;
	void* pData;
	nsmail_outputstream_t* pOutputStream;
} CommandObject_t;


/*********************************************************************************** 
* Parsing Utilities
************************************************************************************/ 
static int parseTaggedLine(imap4Client_t* in_pimap4, char* in_data);
static int parseError(imap4Client_t* in_pimap4, char* in_data);
static int parseOk(imap4Client_t* in_pimap4, char* in_data);
static int parseNo(imap4Client_t* in_pimap4, char* in_data);
static int parseBad(imap4Client_t* in_pimap4, char* in_data);
static int parseCapability(imap4Client_t* in_pimap4, char* in_data);
static int parseList(imap4Client_t* in_pimap4, char* in_data);
static int parseLsub(imap4Client_t* in_pimap4, char* in_data);
static int parseStatus(imap4Client_t* in_pimap4, char* in_data);
static int parseSearch(imap4Client_t* in_pimap4, char* in_data);
static int parseFlags(imap4Client_t* in_pimap4, char* in_data);
static int parseBye(imap4Client_t* in_pimap4, char* in_data);
static int parseRecent(imap4Client_t* in_pimap4, char* in_data);
static int parseExists(imap4Client_t* in_pimap4, char* in_data);
static int parseExpunge(imap4Client_t* in_pimap4, char* in_data);
static int parseFetch(imap4Client_t* in_pimap4, char* in_data);
static int parsePlus(imap4Client_t* in_pimap4, char* in_data);
static int parseNamespace(imap4Client_t* in_pimap4, char* in_data);
static int parseAcl(imap4Client_t* in_pimap4, char* in_data);
static int parseMyrights(imap4Client_t* in_pimap4, char* in_data);
static int parseListrights(imap4Client_t* in_pimap4, char* in_data);

/*********************************************************************************** 
* Default sink methods
************************************************************************************/ 
static void sink_taggedLine(imap4SinkPtr_t in_pimap4Sink, char* in_tag, 
					 const char* in_status, const char* in_reason);
static void sink_error(imap4SinkPtr_t in_pimap4Sink, const char* in_tag, 
				const char* in_status, const char* in_reason);
static void sink_ok(imap4SinkPtr_t in_pimap4Sink, const char* in_responseCode, 
			 const char* in_information);
static void sink_rawResponse(imap4SinkPtr_t in_pimap4Sink, const char* in_data);
static void sink_fetchStart(imap4SinkPtr_t in_pimap4Sink, int in_msg);
static void sink_fetchEnd(imap4SinkPtr_t in_pimap4Sink);
static void sink_fetchSize(imap4SinkPtr_t in_pimap4Sink, int in_size);
static void sink_fetchData(imap4SinkPtr_t in_pimap4Sink, 
					const char* in_data, int in_bytesRead, 
					int in_totalBytes);
static void sink_fetchFlags(imap4SinkPtr_t in_pimap4Sink, const char* in_flags);
static void sink_fetchBodyStructure(imap4SinkPtr_t in_pimap4Sink, 
							 const char* in_bodyStructure);
static void sink_fetchEnvelope(imap4SinkPtr_t in_pimap4Sink, 
						const char** in_value, int in_valueLength);
static void sink_fetchInternalDate(imap4SinkPtr_t in_pimap4Sink, 
							const char* in_internalDate);
static void sink_fetchHeader(imap4SinkPtr_t in_pimap4Sink, 
					  const char* in_field, const char* in_value);
static void sink_fetchUid(imap4SinkPtr_t in_pimap4Sink, int in_uid);
static void sink_lsub(imap4SinkPtr_t in_pimap4Sink, const char* in_attribute, 
			   const char* in_delimeter, const char* in_name);
static void sink_list(imap4SinkPtr_t in_pimap4Sink, const char* in_attribute, 
			   const char* in_delimeter, const char* in_name);
static void sink_searchStart(imap4SinkPtr_t in_pimap4Sink);
static void sink_search(imap4SinkPtr_t in_pimap4Sink, int in_message);
static void sink_searchEnd(imap4SinkPtr_t in_pimap4Sink);
static void sink_statusMessages(imap4SinkPtr_t in_pimap4Sink, int in_messages);
static void sink_statusRecent(imap4SinkPtr_t in_pimap4Sink, int in_recent);
static void sink_statusUidnext(imap4SinkPtr_t in_pimap4Sink, int in_uidNext);
static void sink_statusUidvalidity(imap4SinkPtr_t in_pimap4Sink, int in_uidValidity);
static void sink_statusUnseen(imap4SinkPtr_t in_pimap4Sink, int in_unSeen);
static void sink_capability(imap4SinkPtr_t in_pimap4Sink, const char* in_listing);
static void sink_exists(imap4SinkPtr_t in_pimap4Sink, int in_messages);
static void sink_expunge(imap4SinkPtr_t in_pimap4Sink, int in_message);
static void sink_recent(imap4SinkPtr_t in_pimap4Sink, int in_messages);
static void sink_flags(imap4SinkPtr_t in_pimap4Sink, const char* in_flags);
static void sink_bye(imap4SinkPtr_t in_pimap4Sink, const char* in_reason);
static void sink_nameSpaceStart(imap4SinkPtr_t in_pimap4Sink);
static void sink_nameSpacePersonal(imap4SinkPtr_t in_pimap4Sink, 
						  const char* in_personal);
static void sink_nameSpaceOtherUsers(imap4SinkPtr_t in_pimap4Sink, 
							const char* in_otherUsers);
static void sink_nameSpaceShared(imap4SinkPtr_t in_pimap4Sink, const char* in_shared);
static void sink_nameSpaceEnd(imap4SinkPtr_t in_pimap4Sink);
static void sink_aclStart(imap4SinkPtr_t in_pimap4Sink, const char* in_mailbox);
static void sink_aclIdentifierRight(imap4SinkPtr_t in_pimap4Sink, 
						   const char* in_identifier, const char* in_rights);
static void sink_aclEnd(imap4SinkPtr_t in_pimap4Sink);
static void sink_listRightsStart(imap4SinkPtr_t in_pimap4Sink, const char* in_mailbox, 
						 const char* in_identifier);
static void sink_listRightsRequiredRights(imap4SinkPtr_t in_pimap4Sink, 
								 const char* in_requiredRights);
static void sink_listRightsOptionalRights(imap4SinkPtr_t in_pimap4Sink, 
								 const char* in_optionalRights);
static void sink_listRightsEnd(imap4SinkPtr_t in_pimap4Sink);
static void sink_myRights(imap4SinkPtr_t in_pimap4Sink, const char* in_mailbox, const char* in_rights);

/*********************************************************************************** 
* General helpers
************************************************************************************/ 
void myFree(void* in_block);

/*********************************************************************************** 
* Constant command strings
************************************************************************************/ 

/*Any state*/
const static char* g_szLogout = "LOGOUT";
const static char* g_szCapability = "CAPABILITY";
const static char* g_szNoop = "NOOP";
const static char* g_szConnect = "CONNECT";

/*Non-authenticated state*/
const static char* g_szLogin = "LOGIN";
const static char* g_szAuthenticate = "AUTHENTICATE";

/*Authenticated state*/
const static char* g_szSelect = "SELECT";
const static char* g_szExamine = "EXAMINE";
const static char* g_szCreate = "CREATE";
const static char* g_szDelete = "DELETE";
const static char* g_szRename = "RENAME";
const static char* g_szSubscribe = "SUBSCRIBE";
const static char* g_szUnsubscribe = "UNSUBSCRIBE";
const static char* g_szList = "LIST";
const static char* g_szLsub = "LSUB";
const static char* g_szStatus = "STATUS";
const static char* g_szAppend = "APPEND";

/*Selected state*/
const static char* g_szCheck = "CHECK";
const static char* g_szClose = "CLOSE";
const static char* g_szExpunge = "EXPUNGE";
const static char* g_szSearch = "SEARCH";
const static char* g_szFetch = "FETCH";
const static char* g_szStore = "STORE";
const static char* g_szCopy = "COPY";
const static char* g_szUid = "UID";

/*Extended Commands*/
const static char* g_szSetACL = "SETACL";
const static char* g_szDeleteACL = "DELETEACL";
const static char* g_szGetACL = "GETACL";
const static char* g_szNameSpace = "NAMESPACE";
const static char* g_szListRights = "LISTRIGHTS";

/*Other helpers*/
const static char* g_szFetchMsgDataItem = "(FLAGS RFC822.SIZE ENVELOPE BODYSTRUCTURE)";
const static char* g_szBody = "BODY";
const static char* g_szCharset = "CHARSET ";
const static char* g_szText = "TEXT ";
const static char* g_szHeader = "HEADER ";
const static char* g_szRFC822Size = "(RFC822.SIZE)";
const static char* g_szPlus = "+";
const static char* g_szNil = "NIL";

/*Server response types*/
const static char* g_szFlags = "FLAGS";
const static char* g_szOk = "OK";
const static char* g_szNo= "NO";
const static char* g_szBad = "BAD";
const static char* g_szPreauth = "PREAUTH";
const static char* g_szBye = "BYE";
const static char* g_szRecent = "RECENT";
const static char* g_szExists = "EXISTS";

/*Extended server response types*/
const static char* g_szAcl = "ACL";
const static char* g_szMyRights = "MYRIGHTS";

/*Symbols*/
const static char* g_szSeps = " \n\t";
const static char* g_szCR = "\r";
const static char* g_szSpace = " ";
const static char* g_szDot = ".";
const static char* g_szStar = "*";
const static char* g_szQuote = "\"";
const static int g_nLeftParenthesis = '(';
const static int g_nRightParenthesis = ')';
const static char* g_szLeftSquareBrace = "[";
const static char* g_szRightSquareBrace = "]";
const static char* g_szLeftAngleBrace = "<";
const static char* g_szRightAngleBrace = ">";
const static char* g_szLeftCurlyBrace = "{";
const static char* g_szRightCurlyBrace = "}";

/*Some fetch data items*/
const static char* g_szBodyStructure = "BODYSTRUCTURE";
const static char* g_szRfc822 = "RFC822";
const static char* g_szRfc822Size = "RFC822.SIZE";
const static char* g_szRfc822Text = "RFC822.TEXT";
const static char* g_szRfc822Header = "RFC822.HEADER";
const static char* g_szEnvelope = "ENVELOPE";
const static char* g_szInternalDate = "INTERNALDATE";

/*Generic constants*/
const static char* g_szUntagged = "UNTAGGED";

/*Fetch data item types*/
enum fetchDataItems {
	UID,
	INTERNALDATE,
	BODYSTRUCTURE,
	FLAGS,
	ENVELOPE,
	MESSAGE,
	RFC822TEXT,
	RFC822SIZE,
	UNKNOWN
}dataItems;

/*The maximum number of commands tokens*/
#define MAXTOKENS 15

/*The maximum number of fields in an envelope*/
#define MAXENVFIELDS 10

/*Formally in response.h*/
#define MSGSIZE 10 

/*The response types*/ 
typedef enum imap4_ResponseType 
{ 
	RT_TAG,
	RT_NO,
	RT_BAD,
	RT_OK,
	RT_NOTAGGED,
	RT_BADTAGGED,
	RT_OKTAGGED,
	RT_CAPABILITY, 
	RT_LIST, 
	RT_LSUB, 
	RT_STATUS, 
	RT_SEARCH, 
	RT_FLAGS, 
	RT_EXISTS, 
	RT_EXPUNGE, 
	RT_RECENT, 
	RT_BYE, 
	RT_FETCH,
	RT_PLUS,
	RT_NAMESPACE,
	RT_ACL,
	RT_LISTRIGHTS,
	RT_MYRIGHTS,
	RT_PREAUTH,
	RT_RAWRESPONSE
}imap4_ResponseType_t; 

#endif /*IMAPDATA_H*/


