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

#include <stdlib.h>
#include <stdio.h>
#include "imap4.h"
#include "datasink.h"
#include "nsmail.h"

/***********************************************************************
* General
***********************************************************************/
const static char* g_szSearchLiteral = "Date: Mon, 7 Feb 1994 21:52:25 -0800 (PST)\r\nFrom: Fred Foobar <foobar@Blurdybloop.COM>\r\nSubject: Search test\r\nTo: mooch@owatagu.siam.edu\r\nMessage-Id: <B27397-0100000@Blurdybloop.COM>\r\nMIME-Version: 1.0\r\nContent-Type: TEXT/PLAIN; CHARSET=US-ASCII\r\n\r\nHello Joe, do you think we can meet at 3:30 tomorrow?\r\n\r\n";	

void checkResult(int in_retCode)
{
	if(in_retCode != NSMAIL_OK)
	{
		printf("Error %d \n",in_retCode);
	}
}

/* called to start writing to the stream */
void start(void *rock)
{
	return;
}

/* write (CRLF-separated) output in 'buf', of size 'size'. May be called multiple times */
void writeNS(void *rock, const char *buf, unsigned size)
{
	fwrite(buf, sizeof(char), size, (FILE*)rock);
}

/* Called at the end, when all data has been written out */
void finish(void *rock)
{
	return;
}

/* Returns the number of bytes actually read and -1 on eof on the stream and 
 * may return an NSMAIL_ERR* (int value < 0) in case of any other error.
 * Can be invoked multiple times. Each read returns the next sequence of bytes
 * in the stream, starting past the last byte returned by previous read.
 * The buf space allocated and freed by the caller. The size parameter 
 * specifies the maximum number of bytes to be returned by read. If the number
 * of bytes actually returned by read is less than size, an eof on the stream 
 * is assumed.
 */
int readNS(void *rock, char *buf, unsigned size)
{
	int l_numRead = 0;

	l_numRead = fread(buf, sizeof(char), size, (FILE*)rock);

	if((feof((FILE*)rock) != 0) && (l_numRead == 0))
	{
		return -1;
	}
	return l_numRead;
}

/* Reset the stream to the beginning. A subsequent read returns data from the 
  start of the stream. 
*/
void rewindNS(void *rock)
{
	rewind((FILE*)rock);
}

/* Closes the stream, freeing any internal buffers and rock etc. 
   Once a stream is closed it is illegal to attempt read() or rewind() 
   or close() on the stream.  After close(), the nsmail_inputstream structure
   corresponding to the stream needs to be freed by the user of the stream.
*/
void closeNS(void *rock)
{
	fclose((FILE*)rock);
}



/**********************************************************************
* Sets the methods on the sink
***********************************************************************/
void setSinkMethods(imap4Sink_t* out_pSink)
{
	out_pSink->taggedLine = taggedLine;
	out_pSink->error = error;
	out_pSink->ok = ok;
	out_pSink->rawResponse = rawResponse;
	out_pSink->fetchStart = fetchStart;
	out_pSink->fetchEnd = fetchEnd;
	out_pSink->fetchSize = fetchSize;
	out_pSink->fetchData = fetchData;
	out_pSink->fetchFlags = fetchFlags;
	out_pSink->fetchBodyStructure = fetchBodyStructure;
	out_pSink->fetchEnvelope = fetchEnvelope;
	out_pSink->fetchInternalDate = fetchInternalDate;
	out_pSink->fetchHeader = fetchHeader;
	out_pSink->fetchUid = fetchUid;
	out_pSink->lsub = lsub;
	out_pSink->list = list;
	out_pSink->searchStart = searchStart;
	out_pSink->search = search;
	out_pSink->searchEnd = searchEnd;
	out_pSink->statusMessages = statusMessages;
	out_pSink->statusRecent = statusRecent;
	out_pSink->statusUidnext = statusUidnext;
	out_pSink->statusUidvalidity = statusUidvalidity;
	out_pSink->statusUnseen = statusUnseen;
	out_pSink->capability = capability;
	out_pSink->exists = exists;
	out_pSink->expunge = expunge;
	out_pSink->recent = recent;
	out_pSink->flags = flags;
	out_pSink->bye = bye;
	out_pSink->nameSpaceStart = nameSpaceStart;
	out_pSink->nameSpacePersonal = nameSpacePersonal;
	out_pSink->nameSpaceOtherUsers = nameSpaceOtherUsers;
	out_pSink->nameSpaceShared = nameSpaceShared;
	out_pSink->nameSpaceEnd = nameSpaceEnd;
	out_pSink->aclStart = aclStart;
	out_pSink->aclIdentifierRight = aclIdentifierRight;
	out_pSink->aclEnd = aclEnd;
	out_pSink->listRightsStart = listRightsStart;
	out_pSink->listRightsRequiredRights = listRightsRequiredRights;
	out_pSink->listRightsOptionalRights = listRightsOptionalRights;
	out_pSink->listRightsEnd = listRightsEnd;
	out_pSink->myRights = myRights;
}


/***********************************************************************************
* The Test Runs
************************************************************************************/

/**
* Test the following commands:
* noop, select, fetch, close, create, rename, list, delete
* copy, store, search, expunge, status
*/
void testRun1(imap4Client_t* in_pimap4Client)
{
	int l_retCode = 0;
	void* in_pData = NULL;
	char* out_pTagID = NULL;
	FILE* l_pFile = NULL;
    nsmail_inputstream_t l_fetchStream; 

    /*
    * Select INBOX, FETCH messages, CLOSE mailbox, test NOOP
    */

	l_retCode = imap4_noop(in_pimap4Client, &out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);

	l_retCode = imap4_capability(in_pimap4Client, &out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);

	l_retCode = imap4_list(in_pimap4Client, "\"\"", "*", &out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);

	l_retCode = imap4_status(in_pimap4Client, "INBOX", "(MESSAGES UIDNEXT RECENT)", 
		&out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);

	l_retCode = imap4_select(in_pimap4Client, "INBOX", 
		&out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);

	l_pFile = fopen( "msg1.out", "rb" ); 
	if(l_pFile == NULL)
	{
		printf("msg1.out could not be opened\n");
	}
    l_fetchStream.rock = (void*)l_pFile;
    l_fetchStream.read = readNS;
    l_fetchStream.rewind = rewindNS;
    l_fetchStream.close = closeNS;
	l_retCode = imap4_append(in_pimap4Client, "INBOX", "(\\Draft)", "\"24-Jan-1998 13:12:45 +0000\"", &l_fetchStream, &out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);
	fclose(l_pFile);

	l_pFile = fopen( "msg1.out", "rb" ); 
	if(l_pFile == NULL)
	{
		printf("msg1.out could not be opened\n");
	}
    l_fetchStream.rock = (void*)l_pFile;
    l_fetchStream.read = readNS;
    l_fetchStream.rewind = rewindNS;
    l_fetchStream.close = closeNS;
	l_retCode = imap4_append(in_pimap4Client, "INBOX", "(\\Draft)", NULL, &l_fetchStream, &out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);
	fclose(l_pFile);

	l_retCode = imap4_noop(in_pimap4Client, &out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);

	l_retCode = imap4_fetch(in_pimap4Client, "1", "(BODY[] BODYSTRUCTURE INTERNALDATE)", &out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);

	l_retCode = imap4_close(in_pimap4Client, &out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);

    /*
    * Create mailbox, rename mailbox, list mailboxes, delete mailbox
    * fetch
    */

	l_retCode = imap4_create(in_pimap4Client, "NEWMAILBOX", &out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);

	l_retCode = imap4_rename(in_pimap4Client, "NEWMAILBOX", "RENAMEDBOX", &out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);

	l_retCode = imap4_list(in_pimap4Client, "\"\"", "*", &out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);

	l_retCode = imap4_delete(in_pimap4Client, "RENAMEDBOX", &out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);

    /*
    * select inbox, copy msg, delete msg, search, expunge, 
    * close mailbox
    */

	l_retCode = imap4_select(in_pimap4Client, "INBOX", &out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);

	l_retCode = imap4_copy(in_pimap4Client, "1:2", "INBOX", &out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);

    /* The following line of code will set the DELETED flag on all the
       messages in the currently selected mailbox.  This has been removed
       so that users who run this test program do not accidentally delete
       their inbox.
       derekt 06/03/98
    */

    /*
	l_retCode = imap4_store(in_pimap4Client, "1:2", "+FLAGS", "(\\DELETED)", &out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);
    */

	l_retCode = imap4_search(in_pimap4Client, "DELETED", &out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);

	l_retCode = imap4_expunge(in_pimap4Client, &out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);

	l_retCode = imap4_close(in_pimap4Client, &out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);

}


/**
* Testing search, fetch ,delete
*/
void testRun2(imap4Client_t* in_pimap4Client)
{
	int l_retCode = 0;
	void* in_pData = NULL;
	char* out_pTagID = NULL;
	FILE* l_pFile = NULL;
    nsmail_inputstream_t l_fetchStream; 

	l_pFile = fopen( "msg1.out", "rb" ); 
	if(l_pFile == NULL)
	{
		printf("msg1.out could not be opened\n");
	}
    l_fetchStream.rock = (void*)l_pFile;
    l_fetchStream.read = readNS;
    l_fetchStream.rewind = rewindNS;
    l_fetchStream.close = closeNS;

	l_retCode = imap4_select(in_pimap4Client, "INBOX", &out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);

	l_retCode = imap4_append(in_pimap4Client, "INBOX", "(\\Draft)", NULL, &l_fetchStream, &out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);
	
	l_retCode = imap4_search(in_pimap4Client, "SUBJECT \"afternoon meeting\"", &out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);

	l_retCode = imap4_fetch(in_pimap4Client, "1:2", "BODY[HEADER]", &out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);

	l_retCode = imap4_fetch(in_pimap4Client, "1:2", "BODY[]", &out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);

	l_retCode = imap4_fetch(in_pimap4Client, "1", "ENVELOPE", &out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);

	l_retCode = imap4_store(in_pimap4Client, "1:2", 
		"+FLAGS", "(\\DELETED)", &out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);

	l_retCode = imap4_close(in_pimap4Client, &out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);
}

/**
* Testing check, examine
* fetchPartialBodyPart
*/
void testRun3(imap4Client_t* in_pimap4Client)
{
	int l_retCode = 0;
	char* out_pTagID = NULL;

	l_retCode = imap4_select(in_pimap4Client, "INBOX", 
		&out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);

	l_retCode = imap4_check(in_pimap4Client, &out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);

	l_retCode = imap4_close(in_pimap4Client, &out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);

	l_retCode = imap4_examine(in_pimap4Client, "INBOX", 
		&out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);

	l_retCode = imap4_close(in_pimap4Client, &out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);
}

/**
* Testing subscribe, lsub, unsubscribe
*/
void testRun4(imap4Client_t* in_pimap4Client)
{
	int l_retCode = 0;
	void* in_pData = NULL;
	char* out_pTagID = NULL;

	l_retCode = imap4_select(in_pimap4Client, "INBOX", 
		&out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);

	l_retCode = imap4_create(in_pimap4Client, "NEWSBOX", 
		&out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);

	l_retCode = imap4_lsub(in_pimap4Client, "\"\"", 
		"*", &out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);

	l_retCode = imap4_subscribe(in_pimap4Client, "NEWSBOX", 
		&out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);

	l_retCode = imap4_lsub(in_pimap4Client, "\"\"", "*", 
		&out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);
	
	l_retCode = imap4_unsubscribe(in_pimap4Client, "NEWSBOX", 
		&out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);

	l_retCode = imap4_delete(in_pimap4Client, "NEWSBOX", 
		&out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);

	l_retCode = imap4_close(in_pimap4Client, &out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);
}

/**
* Testing search
*/
void testRun5(imap4Client_t* in_pimap4Client)
{
	int l_retCode = 0;
	void* in_pData = NULL;
	char* out_pTagID = NULL;
	FILE* l_pFile = NULL;
    nsmail_inputstream_t l_fetchStream; 

	l_retCode = imap4_select(in_pimap4Client, "INBOX", 
		&out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);


	l_pFile = fopen( "msg1.out", "rb" ); 
	if(l_pFile == NULL)
	{
		printf("msg1.out could not be opened\n");
	}
    l_fetchStream.rock = (void*)l_pFile;
    l_fetchStream.read = readNS;
    l_fetchStream.rewind = rewindNS;
    l_fetchStream.close = closeNS;

	l_retCode = imap4_select(in_pimap4Client, "INBOX", &out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);

	l_retCode = imap4_append(in_pimap4Client, "INBOX", "(\\Draft)", NULL, &l_fetchStream, &out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);

	l_retCode = imap4_search(in_pimap4Client, "DRAFT", 
		&out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);

	l_retCode = imap4_store(in_pimap4Client, "1", "+FLAGS", 
		"(\\DELETED)", &out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);

	l_retCode = imap4_close(in_pimap4Client, &out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);
}

/**
* Testing sendCommand.
*/
void testRun6(imap4Client_t* in_pimap4Client)
{
	int l_retCode = 0;
	void* in_pData = NULL;
	char* out_pTagID = NULL;

	l_retCode = imap4_sendCommand(in_pimap4Client, "NOOP", &out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);
}

/**
* Testing uidCopy, uidFetch, uidSearch, uidStore
*/
void testRun7(imap4Client_t* in_pimap4Client)
{
	void* in_pData = NULL;
	char* out_pTagID = NULL;
	int l_retCode = 0;

	l_retCode = imap4_select(in_pimap4Client, "INBOX", &out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);

	sink_fetchUIDResults = NULL;
	bStoreUID = TRUE;
	l_retCode = imap4_fetch(in_pimap4Client, "1:2", "UID", &out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);
	bStoreUID = FALSE;

	l_retCode = imap4_uidCopy(in_pimap4Client, sink_fetchUIDResults, "INBOX", 
		&out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);

	l_retCode = imap4_uidSearch(in_pimap4Client, sink_fetchUIDResults, &out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);

	l_retCode = imap4_uidStore(in_pimap4Client, sink_fetchUIDResults, "+FLAGS", 
		"(\\DELETED)", &out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);

	l_retCode = imap4_close(in_pimap4Client, &out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);
}



/**
* Testing extended IMAP commeands: namespace, setacl, deleteacl,
* getacl, listrights, myrights
*/
void testRun8(imap4Client_t* in_pimap4Client)
{
	int l_retCode = 0;
	void* in_pData = NULL;
	char* out_pTagID = NULL;

    /*
    * Namespace extension
    */

	l_retCode = imap4_nameSpace(in_pimap4Client, &out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);

    /*
    * ACL extension
    */
	l_retCode = imap4_myRights(in_pimap4Client, "inbox", &out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);

	l_retCode = imap4_getACL(in_pimap4Client, "inbox", &out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);

	l_retCode = imap4_deleteACL(in_pimap4Client, "inbox", "sama44", &out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);

	l_retCode = imap4_setACL(in_pimap4Client, "inbox", "sama44", "lrswipcda", &out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);

	l_retCode = imap4_listRights(in_pimap4Client, "inbox", "sama44", &out_pTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(in_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&out_pTagID);
}


main()
{
	int l_retCode = 0;
/*	const char* l_szServer = "sama.mcom.com";
	char l_szUser[] = "sama44";
	char l_szPassword[] = "sama44";
*/
	const char* l_szServer = "alterego.mcom.com";
	char l_szUser[] = "imaptest";
	char l_szPassword[] = "test";

	char* l_szTagID = NULL;
	char* l_szValue = NULL;

	/*Initialize and connect to IMAP server*/
	imap4Client_t * l_pimap4Client = NULL;
	imap4Sink_t* l_pimap4Sink = NULL;
	l_retCode = imap4Sink_initialize(&l_pimap4Sink);
	checkResult(l_retCode);
	setSinkMethods(l_pimap4Sink);
	l_retCode = imap4_initialize(&l_pimap4Client, l_pimap4Sink);
	checkResult(l_retCode);
	
	l_retCode = imap4_connect(l_pimap4Client, l_szServer, 143, &l_szTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(l_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&l_szTagID);

	l_retCode = imap4_login(l_pimap4Client, l_szUser, l_szPassword, NULL);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(l_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&l_szTagID);

	/****************************************************************************
	* TestRuns
	*****************************************************************************/

	testRun1(l_pimap4Client);
	testRun2(l_pimap4Client);
	testRun3(l_pimap4Client);
	testRun4(l_pimap4Client);
	testRun5(l_pimap4Client);
	testRun6(l_pimap4Client);
	testRun7(l_pimap4Client);

	/*Namespace and ACL Extensions*/
	testRun8(l_pimap4Client);

	/*End Session*/
	l_retCode = imap4_logout(l_pimap4Client, &l_szTagID);
	checkResult(l_retCode);
	l_retCode = imap4_processResponses(l_pimap4Client);
	checkResult(l_retCode);
	imap4Tag_free(&l_szTagID);

	imap4_free(&l_pimap4Client);
	imap4Sink_free(&l_pimap4Sink);

	printf("Session ended.\n");
	getchar();
	return 0;
}

