/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#define FORCE_PR_LOG /* Allow logging in the release build */

#include "rosetta.h"
#include "mkutils.h" 

#include "imap4pvt.h"
#include "imap.h"
#include "imapbody.h"
#include "msgcom.h"
#include "msgnet.h"
#include "xpgetstr.h"
#include "secnav.h"
#include HG26363
#include "sslerr.h"
#include "prefapi.h"
#include "prlog.h"
#include "libi18n.h"
#include "prtime.h"
#include "netutils.h"
#include "nslocks.h"

#ifdef XP_MAC
#include "prpriv.h"             /* for NewNamedMonitor */
#else
#include "private/prpriv.h"
#endif

#define MAX_NEW_IMAP_FOLDER_COUNT 50
#define IMAP_DB_HEADERS "From To Cc Subject Date Priority X-Priority Message-ID References Newsgroups"


#define IMAP_YIELD(A) PR_Sleep(PR_INTERVAL_NO_WAIT)

extern "C"
{
extern int MK_OUT_OF_MEMORY;

/* 45678901234567890123456789012345678901234567890123456789012345678901234567890
*/ 

extern int XP_ERRNO_EWOULDBLOCK;
extern int XP_ERRNO_ENOTCONN;
extern int MK_TCP_READ_ERROR;
extern int MK_TCP_WRITE_ERROR;
extern int MK_POP3_NO_MESSAGES;
extern int MK_BAD_CONNECT;
extern int XP_FOLDER_RECEIVING_MESSAGE_OF;
extern int XP_RECEIVING_MESSAGE_HEADERS_OF;
extern int XP_RECEIVING_MESSAGE_FLAGS_OF;
extern int XP_IMAP_DELETING_MESSAGES;
extern int XP_IMAP_DELETING_MESSAGE;
extern int XP_IMAP_COPYING_MESSAGES_TO;
extern int XP_IMAP_COPYING_MESSAGE_TO;
extern int XP_IMAP_MOVING_MESSAGES_TO;
extern int XP_IMAP_MOVING_MESSAGE_TO;
extern int MK_MSG_IMAP_SERVER_NOT_IMAP4;
extern int MK_MSG_IMAP_SERVER_SAID;
extern int MK_MSG_TRASH_L10N_NAME;
extern int MK_IMAP_STATUS_CREATING_MAILBOX;
extern int MK_IMAP_STATUS_SELECTING_MAILBOX;
extern int MK_IMAP_STATUS_DELETING_MAILBOX;
extern int MK_IMAP_STATUS_RENAMING_MAILBOX;
extern int MK_IMAP_STATUS_LOOKING_FOR_MAILBOX;
extern int MK_IMAP_STATUS_SUBSCRIBE_TO_MAILBOX;
extern int MK_IMAP_STATUS_UNSUBSCRIBE_MAILBOX;
extern int MK_IMAP_STATUS_SEARCH_MAILBOX;
extern int MK_IMAP_STATUS_MSG_INFO;
extern int MK_IMAP_STATUS_CLOSE_MAILBOX;
extern int MK_IMAP_STATUS_EXPUNGING_MAILBOX;
extern int MK_IMAP_STATUS_LOGGING_OUT;
extern int MK_IMAP_STATUS_CHECK_COMPAT;
extern int MK_IMAP_STATUS_SENDING_LOGIN;
extern int MK_IMAP_STATUS_SENDING_AUTH_LOGIN;
extern int MK_IMAP_CREATE_FOLDER_BUT_NO_SUBSCRIBE;
extern int MK_IMAP_DELETE_FOLDER_BUT_NO_UNSUBSCRIBE;
extern int MK_IMAP_RENAME_FOLDER_BUT_NO_SUBSCRIBE;
extern int MK_IMAP_RENAME_FOLDER_BUT_NO_UNSUBSCRIBE;
extern int MK_IMAP_STATUS_GETTING_NAMESPACE;
extern int MK_IMAP_UPGRADE_NO_PERSONAL_NAMESPACE;
extern int MK_IMAP_UPGRADE_TOO_MANY_FOLDERS;
extern int MK_MSG_IMAP_DISCOVERING_MAILBOX;
extern int MK_IMAP_UPGRADE_PROMPT_USER;
extern int MK_IMAP_UPGRADE_PROMPT_USER_2;
extern int MK_IMAP_UPGRADE_WAIT_WHILE_UPGRADE;
extern int MK_IMAP_UPGRADE_PROMPT_QUESTION;
extern int MK_IMAP_UPGRADE_CUSTOM;
extern int MK_IMAP_UPGRADE_SUCCESSFUL;
extern int MK_MSG_DRAFTS_L10N_NAME;
extern int MK_IMAP_GETTING_ACL_FOR_FOLDER;
extern int MK_IMAP_GETTING_SERVER_INFO;
extern int MK_IMAP_GETTING_MAILBOX_INFO;
extern void NET_SetPopPassword2(const char *password);
extern int XP_PROMPT_ENTER_PASSWORD;
extern int XP_PASSWORD_FOR_POP3_USER;
extern int XP_MSG_IMAP_LOGIN_FAILED;

extern void net_graceful_shutdown(PRFileDesc* sock, XP_Bool isSecure);
}

extern PRLogModuleInfo* IMAP;
#define out PR_LOG_ALWAYS

static int32 gMIMEOnDemandThreshold = 15000;
static XP_Bool gMIMEOnDemand = FALSE;
static XP_Bool gOptimizedHeaders = FALSE;

static int32 gTunnellingThreshold = 2000;
static XP_Bool gIOTunnelling = FALSE;	// off for now

typedef struct _ImapConData {
	TNavigatorImapConnection *netConnection;


	void					 *offLineRetrievalData;		// really a DisplayOfflineImapState object
	
	uint32					 offLineMsgFlags;
	uint32					 offLineMsgKey;
	
	NET_StreamClass 		 *offlineDisplayStream;
} ImapConData;

typedef struct _GenericInfo {
	char *c, *hostName;
} GenericInfo;

typedef struct _StreamInfo {
	uint32	size;
	char	*content_type;
} StreamInfo;

typedef struct _ProgressInfo {
	char *message;
	int percent;
} ProgressInfo;

typedef struct _FolderQueryInfo {
	char *name, *hostName;
	XP_Bool rv;
} FolderQueryInfo;


typedef struct _StatusMessageInfo {
	uint32 msgID;
	char * extraInfo;
} StatusMessageInfo;

typedef struct _UploadMessageInfo {
	uint32 newMsgID;
	XP_File	fileId;
	char *dataBuffer;
	int32 bytesRemain;
} UploadMessageInfo;

struct utf_name_struct {
	XP_Bool toUtf7Imap;
	unsigned char *sourceString;
	unsigned char *convertedString;
};


typedef struct _TunnelInfo {
	int32 maxSize;
	PRFileDesc*	ioSocket;
	char**		inputSocketBuffer;
    int32*		inputBufferSize;
} TunnelInfo;

const char *ImapTRASH_FOLDER_NAME = NULL;

const int32 kImapSleepTime = 1000000;

int32 atoint32(char *ascii)
{
	char *endptr;
	int32 rvalue = XP_STRTOUL(ascii, &endptr, 10);
	return rvalue;
}

XP_Bool IMAP_ContextIsBiff(ActiveEntry *ce)
{
	return (ce->window_id == MSG_GetBiffContext() || ce->window_id->type == MWContextBiff);
}

XP_Bool IMAP_URLIsBiff(ActiveEntry *ce, TIMAPUrl &currentUrl)
{
	return (IMAP_ContextIsBiff(ce) && currentUrl.GetIMAPurlType() == TIMAPUrl::kSelectFolder);
}

static void
IMAP_LoadTrashFolderName(void)
{
	if (!ImapTRASH_FOLDER_NAME)
		ImapTRASH_FOLDER_NAME = MSG_GetSpecialFolderName(MK_MSG_TRASH_L10N_NAME);
}

void IMAP_DoNotDownLoadAnyMessageHeadersForMailboxSelect(TNavigatorImapConnection *connection)
{
	if (connection)
		connection->NotifyKeyList(NULL,0);
}

void IMAP_DownLoadMessageBodieForMailboxSelect(TNavigatorImapConnection *connection,
									           uint32 *messageKeys,	/* uint32* is adopted */
									           uint32 numberOfKeys)
{
	connection->NotifyKeyList(messageKeys, numberOfKeys);
}

void IMAP_BodyIdMonitor(TNavigatorImapConnection *connection, XP_Bool enter)
{
	connection->BodyIdMonitor(enter);
}

void TNavigatorImapConnection::BodyIdMonitor(XP_Bool enter)
{
	if (enter)
		PR_EnterMonitor(fWaitForBodyIdsMonitor);
	else
		PR_ExitMonitor(fWaitForBodyIdsMonitor);
}

void IMAP_DownLoadMessagesForMailboxSelect(TNavigatorImapConnection *connection,
                                           uint32 *messageKeys,	/* uint32* is adopted */
									       uint32 numberOfKeys)
{
    connection->NotifyKeyList(messageKeys, numberOfKeys);
}

char *IMAP_GetCurrentConnectionUrl(TNavigatorImapConnection *connection)
{
    return connection->GetCurrentConnectionURL();
}
    
void IMAP_UploadAppendMessageSize(TNavigatorImapConnection *connection, uint32 msgSize, imapMessageFlagsType flags)
{
    connection->NotifyAppendSize(msgSize, flags);
}
                                       
void IMAP_TerminateConnection (TNavigatorImapConnection *conn)
{
	conn->TellThreadToDie();
}

char *IMAP_CreateOnlineSourceFolderNameFromUrl(const char *url)
{
	TIMAPUrl urlObject(url, TRUE);
	return urlObject.CreateCanonicalSourceFolderPathString();
}

void IMAP_FreeBoxSpec(mailbox_spec *victim)
{
	if (victim)
	{
		FREEIF(victim->allocatedPathName);
	//	delete victim->flagState; // not owned by us, leave it alone
		XP_FREE(victim);
	}
}

XP_Bool IMAP_CheckNewMail(TNavigatorImapConnection *connection)
{
	return connection->CheckNewMail();
}

XP_Bool IMAP_NewMailDetected(TNavigatorImapConnection *connection)
{
	return connection->NewMailDetected();
}


// These C functions implemented here are usually executed as TImapFEEvent's

static
void SetBiffIndicator(void *biffStateVoid, void *blockingConnectionVoid)
{
    TNavigatorImapConnection *imapConnection = 
        (TNavigatorImapConnection *) blockingConnectionVoid;
    MSG_BIFF_STATE biffState = (MSG_BIFF_STATE) (uint32) biffStateVoid;
    MSG_SetBiffStateAndUpdateFE(biffState);
    imapConnection->NotifyEventCompletionMonitor();

}


#ifndef XP_OS2
static
#else
extern "OPTLINK"
#endif
void MessageUploadComplete(void *blockingConnectionVoid)        // called when upload is complete
{
    TNavigatorImapConnection *imapConnection = 
        (TNavigatorImapConnection *) blockingConnectionVoid;
    imapConnection->NotifyMessageUploadMonitor();
}

static
void UploadMessageEvent(void *blockingConnectionVoid, void *)
{
    TNavigatorImapConnection *imapConnection = 
        (TNavigatorImapConnection *) blockingConnectionVoid;
    
    BeginMessageUpload(imapConnection->GetActiveEntry()->window_id, 
                       imapConnection->GetIOSocket(),
                       MessageUploadComplete,
                       imapConnection);
}

static
void msgSetUserAuthenticated(void *blockingConnectionVoid, void *trueOrFalseVoid)
{
	TNavigatorImapConnection *imapConnection =
		(TNavigatorImapConnection *) blockingConnectionVoid;
	ActiveEntry *ce = imapConnection->GetActiveEntry();
	XP_Bool authenticated = (XP_Bool) (uint32) trueOrFalseVoid;
	if (ce && ce->URL_s->msg_pane)
		MSG_SetUserAuthenticated(MSG_GetMaster(ce->URL_s->msg_pane),
								 authenticated);
}

static 
void LiteSelectEvent(void *blockingConnectionVoid, void * /*unused*/)
{
	TNavigatorImapConnection *imapConnection =
		(TNavigatorImapConnection *) blockingConnectionVoid;
	ActiveEntry *ce = imapConnection->GetActiveEntry();
	if (ce && ce->URL_s->msg_pane)
		ReportLiteSelectUIDVALIDITY(ce->URL_s->msg_pane, 
									imapConnection->GetServerStateParser().FolderUID());
	imapConnection->NotifyEventCompletionMonitor();
}

static
void msgSetMailAccountURL(void *blockingConnectionVoid, void *hostName)
{
	TNavigatorImapConnection *imapConnection =
		(TNavigatorImapConnection *) blockingConnectionVoid;
	ActiveEntry *ce = imapConnection->GetActiveEntry();
	if (ce && ce->URL_s->msg_pane)
		MSG_SetHostMailAccountURL(MSG_GetMaster(ce->URL_s->msg_pane), (const char *) hostName,
							  imapConnection->GetServerStateParser().GetMailAccountUrl());
}

static void msgSetMailServerURLs(void *blockingConnectionVoid, void *hostName)
{
	TNavigatorImapConnection *imapConnection =
		(TNavigatorImapConnection *) blockingConnectionVoid;

	ActiveEntry *ce = imapConnection->GetActiveEntry();
	if (ce && ce->URL_s->msg_pane)
	{
		MSG_SetHostMailAccountURL(MSG_GetMaster(ce->URL_s->msg_pane), (const char *) hostName,
							  imapConnection->GetServerStateParser().GetMailAccountUrl());
		MSG_SetHostManageListsURL(MSG_GetMaster(ce->URL_s->msg_pane), (const char *) hostName,
							  imapConnection->GetServerStateParser().GetManageListsUrl());
		MSG_SetHostManageFiltersURL(MSG_GetMaster(ce->URL_s->msg_pane), (const char *) hostName,
							  imapConnection->GetServerStateParser().GetManageFiltersUrl());
	}
}

static void MOZ_THREADmsgSetFolderURL(void *blockingConnectionVoid, void *folderQueryInfo)
{
	TNavigatorImapConnection *imapConnection =
		(TNavigatorImapConnection *) blockingConnectionVoid;

	ActiveEntry *ce = imapConnection->GetActiveEntry();
	if (ce && ce->window_id->mailMaster)
	{
		FolderQueryInfo *queryInfo = (FolderQueryInfo *) folderQueryInfo;
		MSG_SetFolderAdminURL(ce->window_id->mailMaster, queryInfo->hostName, queryInfo->name, imapConnection->GetServerStateParser().GetManageFolderUrl());
	}
}

struct tFlagsKeyStruct {
    imapMessageFlagsType flags;
    MessageKey key;
};

typedef struct tFlagsKeyStruct tFlagsKeyStruct;

static
void NotifyMessageFlagsEvent( void *blockingConnectionVoid, void *flagsVoid)
{
    TNavigatorImapConnection *imapConnection = 
        (TNavigatorImapConnection *) blockingConnectionVoid;
    struct tFlagsKeyStruct *flagAndKey = (struct tFlagsKeyStruct *) flagsVoid;
    
    MSG_RecordImapMessageFlags(imapConnection->GetActiveEntry()->window_id->imapURLPane,
                               flagAndKey->key,
                               flagAndKey->flags);
    FREEIF( flagAndKey);
}

struct delete_message_struct {
	char *onlineFolderName;
	XP_Bool		deleteAllMsgs;
	char *msgIdString;
};

static
void ConvertImapUtf7(void *utf_name_structVoid, 
					void *blockingConnectionVoid)
{
    TNavigatorImapConnection *imapConnection = 
        (TNavigatorImapConnection *) blockingConnectionVoid;

	utf_name_struct *names = (utf_name_struct *) utf_name_structVoid;
	
	// NULL if bad conversion
	names->convertedString = NULL;
	
#ifdef XP_WIN16
	names->convertedString = (unsigned char *) XP_STRDUP((const char *) names->sourceString);
#else
	int16 fromCsid = names->toUtf7Imap  ? (INTL_DocToWinCharSetID(INTL_DefaultDocCharSetID(0)) & ~CS_AUTO): CS_IMAP4_UTF7;
	int16 toCsid   = !names->toUtf7Imap ? (INTL_DocToWinCharSetID(INTL_DefaultDocCharSetID(0)) & ~CS_AUTO): CS_IMAP4_UTF7;
	
	// convert from whatever to CS_UTF8
	unsigned char *utf8String = INTL_ConvertLineWithoutAutoDetect(fromCsid, CS_UTF8, names->sourceString, XP_STRLEN((const char *) names->sourceString));
	if (utf8String)
	{
		// convert from CS_UTF8 to whatever
		names->convertedString = INTL_ConvertLineWithoutAutoDetect(CS_UTF8, toCsid, utf8String, XP_STRLEN((const char *) utf8String));
		XP_FREE(utf8String);
	} 
	
#endif
	if (imapConnection)
		imapConnection->NotifyEventCompletionMonitor();
}

static 
void NotifyMessageDeletedEvent( void *blockingConnectionVoid, void *delete_message_structVoid)
{
    TNavigatorImapConnection *imapConnection = 
        (TNavigatorImapConnection *) blockingConnectionVoid;
        
	struct delete_message_struct *msgStruct = (struct delete_message_struct *) delete_message_structVoid;

	if (msgStruct->onlineFolderName)
		MSG_ImapMsgsDeleted(imapConnection->GetActiveEntry()->window_id->imapURLPane,
							msgStruct->onlineFolderName, imapConnection->GetHostName(),
							msgStruct->deleteAllMsgs,
							msgStruct->msgIdString);
	
	FREEIF( msgStruct->onlineFolderName );
	FREEIF( msgStruct->msgIdString);
	FREEIF( msgStruct);
}

static
void AddSearchResultEvent( void *blockingConnectionVoid, void *hitLineVoid)
{
    TNavigatorImapConnection *imapConnection = 
        (TNavigatorImapConnection *) blockingConnectionVoid;

    MSG_AddImapSearchHit(imapConnection->GetActiveEntry()->window_id,
                         (const char *) hitLineVoid);
                         
    imapConnection->NotifyEventCompletionMonitor();
}


static
void HeaderFetchCompletedEvent(void *blockingConnectionVoid, void *)
{
    TNavigatorImapConnection *imapConnection = 
        (TNavigatorImapConnection *) blockingConnectionVoid;
    NotifyHeaderFetchCompleted(imapConnection->GetActiveEntry()->window_id,imapConnection);
}

#ifdef XP_MAC 
// stuff used to measure performance of down loads
// #define MACPERF 1
#ifdef MACPERF
#include "Events.h"
UInt32 g_StartTimeOfDownLoad;
UInt32 g_TimeSpendInDownLoadFunction;
UInt32 g_NumberOfVisits;
#endif

#endif


/* this function ensures that the msg dislay stream is set up so that
   replying to a message causes the compose window to appear with the
   correct header info filled in and quoting works.
*/
   
#ifndef XP_OS2
static 
#else
extern "OPTLINK"
#endif
char * imap_mail_generate_html_header_fn (const char* /*dest*/, void *closure,
                                   MimeHeaders *headers)
{
    // This function is only called upon creation of a message pane.
    // If this is true then ce->URL_s->msg_pane will be set by
    // MSG_Pane::GetURL.
    ActiveEntry *ce = (ActiveEntry *) closure;
    MSG_Pane *messagepane = ce->URL_s->msg_pane;
   
	if (!messagepane && ce->window_id)
		messagepane = ce->window_id->imapURLPane;

    if (messagepane && (MSG_MESSAGEPANE == MSG_GetPaneType(messagepane)))
        MSG_ActivateReplyOptions (messagepane, headers);
    return 0;
}

/* this function generates the correct reference URLs for 
IMAP messages */

#ifndef XP_OS2
static
#else
extern "OPTLINK"
#endif
char * imap_mail_generate_reference_url_fn (const char *dest, void *closure,
								MimeHeaders*)
{
  ActiveEntry *cur_entry = (ActiveEntry *) closure;
  char *addr = cur_entry->URL_s->address;
  char *search = (addr ? XP_STRRCHR (addr, '>') + 1 : 0);
  char *id2;
  char *new_dest = 0;
  char *result = 0;
  MessageKey key = MSG_MESSAGEKEYNONE;
  char *keyStr = 0;

  if (!dest || !*dest) return 0;
  id2 = XP_STRDUP (dest);
  if (!id2) return 0;
  if (id2[XP_STRLEN (id2)-1] == '>')
	id2[XP_STRLEN (id2)-1] = 0;

  // convert the Message-ID to a Key
  MSG_Pane *pane = cur_entry->window_id->imapURLPane;
  if (pane)
  {
	  char *idInDB = id2;
	  if (id2[0] == '<')
	  {
		new_dest = NET_Escape (id2+1, URL_PATH);
		idInDB = id2 + 1;
	  }
	  else
	  {
		new_dest = NET_Escape (id2, URL_PATH);
	  }

	  if (0 != MSG_GetKeyFromMessageId(pane, idInDB, &key)  || key == MSG_MESSAGEKEYNONE)
		  goto CLEANUP;

  }
  else
  {
	  goto CLEANUP;
  }


  // now convert the message key to a string
  keyStr = PR_smprintf ("%ld", (long) key);

  result = (char *) XP_ALLOC ((search ? search - addr : 0) +
							(new_dest ? XP_STRLEN (new_dest) : 0) +
							40);
  if (result && new_dest)
	{
	  if (search)
		{
		  XP_MEMCPY (result, addr, search - addr);
		  result[search - addr] = 0;
		}
	  else if (addr)
		XP_STRCPY (result, addr);
	  else
		*result = 0;
	  XP_STRCAT (result, keyStr);
	}
CLEANUP:
  FREEIF (id2);
  FREEIF (new_dest);
  FREEIF (keyStr);
  return result;
}

MSG_Pane *IMAP_GetActiveEntryPane(ImapActiveEntry * imap_entry)
{
	ActiveEntry * ce = (ActiveEntry *) imap_entry;
	MSG_Pane *returnPane = ce->URL_s->msg_pane;
	
	if (!returnPane)	// can happen when libmime or layout reload a url
		returnPane = MSG_FindPaneOfContext(ce->window_id, MSG_ANYPANE);
		
	return returnPane;
}

NET_StreamClass *IMAP_CreateDisplayStream(ImapActiveEntry * imap_entry, XP_Bool clearCacheBit, uint32 size, const char *content_type)
{
	ActiveEntry * ce = (ActiveEntry *) imap_entry;
	
	if (clearCacheBit)
		ce->format_out = CLEAR_CACHE_BIT(ce->format_out);	// no need to use libnet cache

	if (ce->format_out == FO_PRESENT || ce->format_out == FO_CACHE_AND_PRESENT)
	{
	    IMAP_InitializeImapFeData(ce);
	}

	ce->URL_s->content_length = size;
	StrAllocCopy(ce->URL_s->content_type, content_type);
	NET_StreamClass *displayStream = NET_StreamBuilder(ce->format_out, ce->URL_s, ce->window_id);   
	
	return displayStream;
}


int
IMAP_InitializeImapFeData (ImapActiveEntry * imap_entry)
{
	ActiveEntry * ce = (ActiveEntry *) imap_entry;
	
	MimeDisplayOptions *opt;
  
	{	// make sure this is a valid imap url
		TIMAPUrl imapurl(ce->URL_s->address, ce->URL_s->internal_url);
  		if (!imapurl.ValidIMAPUrl())
  		{
  			XP_ASSERT(0);
  			return -1;
  		}
  	}

	if (ce->URL_s->fe_data)
	{
		XP_ASSERT(0);
		return -1;
	}

	opt = XP_NEW (MimeDisplayOptions);
	if (!opt) return MK_OUT_OF_MEMORY;
	XP_MEMSET (opt, 0, sizeof(*opt));

	opt->generate_reference_url_fn		  = imap_mail_generate_reference_url_fn;
	opt->generate_header_html_fn		  = imap_mail_generate_html_header_fn;
	opt->html_closure					  = ce;
	opt->missing_parts					  = ce->URL_s->content_modified;

	ce->URL_s->fe_data = opt;
	return 0;
}


extern "C" void IMAP_PseudoInterruptFetch(MWContext *context)
{
	// OK, we're going to lie and stick the connection object in the context.
	TNavigatorImapConnection *imapConnection = context->imapConnection;

	if (imapConnection)
	{
		imapConnection->PseudoInterrupt(TRUE);
	}
}



// This function creates an output stream used to present an RFC822 message or one of its parts
static
void SetupMsgWriteStream(void *blockingConnectionVoid, void *StreamInfoVoid)
{
#ifdef MACPERF
    g_StartTimeOfDownLoad = TickCount();
    g_TimeSpendInDownLoadFunction = 0;
    g_NumberOfVisits              = 0;
#endif

	PR_LOG(IMAP, out, ("STREAM: Begin Message Download Stream Event"));
    TNavigatorImapConnection *imapConnection = 
        (TNavigatorImapConnection *) blockingConnectionVoid;
    ActiveEntry *ce = imapConnection->GetActiveEntry();
	StreamInfo *si = (StreamInfo *)StreamInfoVoid;
	
    NET_StreamClass *outputStream = NULL;
    
    TIMAPUrl currentUrl(ce->URL_s->address, ce->URL_s->internal_url);
    if ((currentUrl.GetIMAPurlType() == TIMAPUrl::kOnlineToOfflineCopy) || 
    	(currentUrl.GetIMAPurlType() == TIMAPUrl::kOnlineToOfflineMove))
    	MSG_StartImapMessageToOfflineFolderDownload(ce->window_id);
    
    if (ce->window_id->currentIMAPfolder)
    {
        outputStream = CreateIMAPDownloadMessageStream(ce, si->size, si->content_type, ce->URL_s->content_modified);
    }
    else
    {
        if (ce->format_out == FO_PRESENT || ce->format_out == FO_CACHE_AND_PRESENT)
        {
            IMAP_InitializeImapFeData(ce);
        }

		ce->URL_s->content_length = si->size;
	    StrAllocCopy(ce->URL_s->content_type, si->content_type);
        outputStream = 
                     NET_StreamBuilder(ce->format_out, ce->URL_s, ce->window_id);       
    }
    
    if (outputStream)
    	imapConnection->SetOutputStream(outputStream);
    else
    {
    	// this should be impossible but be paranoid
    	imapConnection->SetConnectionStatus(-1);
	    imapConnection->GetServerStateParser().SetConnected(FALSE);
    }

	FREEIF(si->content_type);
	XP_FREE(si);

    imapConnection->NotifyEventCompletionMonitor();
}


// report the success of the recent online copy
static
void OnlineCopyReport(void *blockingConnectionVoid,
                      void *adoptedCopyStateVoid)
{
    TNavigatorImapConnection *imapConnection = 
        (TNavigatorImapConnection *) blockingConnectionVoid;

    ImapOnlineCopyState *copyState = (ImapOnlineCopyState *) adoptedCopyStateVoid;
    
    ReportSuccessOfOnlineCopy(imapConnection->GetActiveEntry()->window_id, *copyState);
    
    FREEIF( copyState);
}

static
void OnlineFolderDelete(void *blockingConnectionVoid,
                        void *adoptedmailboxNameVoid)
{
    TNavigatorImapConnection *imapConnection = 
        (TNavigatorImapConnection *) blockingConnectionVoid;

    char *boxNameAdopted = (char *) adoptedmailboxNameVoid;
    
    ReportSuccessOfOnlineDelete(imapConnection->GetActiveEntry()->window_id, imapConnection->GetHostName(), boxNameAdopted);
    
    FREEIF( boxNameAdopted);
}

static
void OnlineFolderCreateFailed(void *blockingConnectionVoid,
							  void *serverMessageVoid)
{
    TNavigatorImapConnection *imapConnection = 
        (TNavigatorImapConnection *) blockingConnectionVoid;

    char *serverMessage = (char *) serverMessageVoid;
    
    ReportFailureOfOnlineCreate(imapConnection->GetActiveEntry()->window_id, serverMessage);
    
    FREEIF( serverMessage);
}

static
void OnlineFolderRename(void *blockingConnectionVoid,
                        void *renameStructVoid)
{
    TNavigatorImapConnection *imapConnection = 
        (TNavigatorImapConnection *) blockingConnectionVoid;

    folder_rename_struct *renameStruct = (folder_rename_struct *) renameStructVoid;
    
    ReportSuccessOfOnlineRename(imapConnection->GetActiveEntry()->window_id, renameStruct);
    
    FREEIF( renameStruct->fOldName);
    FREEIF( renameStruct->fNewName);
	FREEIF( renameStruct->fHostName);
    FREEIF( renameStruct);
}

static
void MailboxDiscoveryDoneEvent(void *blockingConnectionVoid, void * /*unused*/)
{
    TNavigatorImapConnection *imapConnection = 
        (TNavigatorImapConnection *) blockingConnectionVoid;
    ReportMailboxDiscoveryDone(imapConnection->GetActiveEntry()->window_id, imapConnection->GetActiveEntry()->URL_s);
}


// This function tells mail master about a discovered imap mailbox
static
void PossibleIMAPmailbox(void *blockingConnectionVoid, 
						void *boxSpecVoid)
{
    TNavigatorImapConnection *imapConnection = 
        (TNavigatorImapConnection *) blockingConnectionVoid;

	MSG_Master *master = NULL;
	MWContext *context = imapConnection->GetActiveEntry()->window_id;
	if (context)
		master = context->mailMaster;
    XP_Bool broadcastDiscovery = TRUE;
	char *url = imapConnection->GetCurrentConnectionURL();
	if (url)
	{
		TIMAPUrl urlc(url, TRUE);
		if (urlc.GetIMAPurlType() == TIMAPUrl::kCreateFolder)
			broadcastDiscovery = FALSE;
		XP_FREE(url);
	}


    imapConnection->SetMailboxDiscoveryStatus(
    					DiscoverIMAPMailbox((mailbox_spec *) boxSpecVoid, 
											master,
											imapConnection->GetActiveEntry()->window_id,
											broadcastDiscovery));

	imapConnection->NotifyEventCompletionMonitor();
}

// tell the mail master about the newly selected mailbox
static
void UpdatedIMAPmailbox(void *blockingConnectionVoid, 
						void *boxSpecVoid)
{
    TNavigatorImapConnection *imapConnection = 
        (TNavigatorImapConnection *) blockingConnectionVoid;
    
    UpdateIMAPMailboxInfo((mailbox_spec *) boxSpecVoid, 
                          imapConnection->GetActiveEntry()->window_id);

}

// tell the mail master about the newly selected mailbox
static
void UpdatedIMAPmailboxStatus(void *blockingConnectionVoid, 
						void *boxSpecVoid)
{
    TNavigatorImapConnection *imapConnection = 
        (TNavigatorImapConnection *) blockingConnectionVoid;
    
    UpdateIMAPMailboxStatus((mailbox_spec *) boxSpecVoid, 
                          imapConnection->GetActiveEntry()->window_id);

}


struct uid_validity_info {
	char *canonical_boxname;
	const char *hostName;
	int32 returnValidity;
};

static 
void GetStoredUIDValidity(void *blockingConnectionVoid, 
						  void *ValidityVoid)
{
    TNavigatorImapConnection *imapConnection = 
        (TNavigatorImapConnection *) blockingConnectionVoid;
	uid_validity_info *info = (uid_validity_info *) ValidityVoid;
	
	info->returnValidity = GetUIDValidityForSpecifiedImapFolder(info->hostName, info->canonical_boxname, 
																imapConnection->GetActiveEntry()->window_id);
	imapConnection->NotifyEventCompletionMonitor();
}


struct msg_line_info {
    char   *adoptedMessageLine;
    uint32 uidOfMessage;
};


// This function adds one line to current message presentation
static
void ParseAdoptedMsgLine(void *blockingConnectionVoid, 
						void *adoptedmsg_line_info_Void)
{
#ifdef MACPERF
    UInt32 startTicks = TickCount();
#endif

    TNavigatorImapConnection *imapConnection = 
        (TNavigatorImapConnection *) blockingConnectionVoid;
    ActiveEntry *ce = imapConnection->GetActiveEntry();
    
    msg_line_info *downloadLineDontDelete = (msg_line_info *) adoptedmsg_line_info_Void;
    NET_StreamClass *outputStream = imapConnection->GetOutputStream();
    
    unsigned int streamBytesMax = (*outputStream->is_write_ready) (outputStream);
    char *stringToPut = downloadLineDontDelete->adoptedMessageLine;
    XP_Bool allocatedString = FALSE;
    
    if ( streamBytesMax < (XP_STRLEN(downloadLineDontDelete->adoptedMessageLine) + 2))
    {
    	// dup the streamBytesMax number of chars, then put the rest of the string back into event queue
    	if (streamBytesMax != 0)
    	{
	    	stringToPut = (char *) XP_ALLOC(streamBytesMax + 1);	// 1 for \0
	    	if (stringToPut)
	    	{
	    		XP_MEMCPY(stringToPut, downloadLineDontDelete->adoptedMessageLine, streamBytesMax);
	    		*(stringToPut + streamBytesMax) = 0;
	    		allocatedString = TRUE;
	    	}
	    	
	    	// shift buffer bytes
	    	XP_MEMMOVE(downloadLineDontDelete->adoptedMessageLine, 
	    			   downloadLineDontDelete->adoptedMessageLine + streamBytesMax, 
	    			   (XP_STRLEN(downloadLineDontDelete->adoptedMessageLine) + 1) - streamBytesMax);
    	}
    	
        // the output stream can't handle this event yet! put it back on the
        // queue
        TImapFEEvent *parseLineEvent = 
            new TImapFEEvent(ParseAdoptedMsgLine,                   // function to call
                             blockingConnectionVoid,                // access to current entry
                             adoptedmsg_line_info_Void,
							 FALSE);    // line to display
        if (parseLineEvent)
            imapConnection->GetFEEventQueue().
                    AdoptEventToBeginning(parseLineEvent);
        else
            imapConnection->HandleMemoryFailure();
    }


	if (streamBytesMax != 0)
	{
	    imapConnection->SetBytesMovedSoFarForProgress(imapConnection->GetBytesMovedSoFarForProgress() +
	                                                  XP_STRLEN(stringToPut));

		if (!imapConnection->GetPseudoInterrupted())
		{
			int bytesPut = 0;
			
			if (ce->window_id->currentIMAPfolder)
			{
					// the definition of put_block in net.h defines
					// the 3rd parameter as the length of the block,
					// but since we are always sending a null terminated
					// string, I am able to sent the uid instead
				bytesPut = (*outputStream->put_block) (outputStream,
											stringToPut,
											downloadLineDontDelete->uidOfMessage);
			}
			else    // this is a download for display
			{
				bytesPut = (*outputStream->put_block) (outputStream,
											stringToPut,
											XP_STRLEN(stringToPut));
			}
			
			if (bytesPut < 0)
			{
	    		imapConnection->SetConnectionStatus(-1);	// something bad happened, stop this url
	    		imapConnection->GetServerStateParser().SetConnected(FALSE);
			}
		}
    }
#ifdef MACPERF
        g_TimeSpendInDownLoadFunction += TickCount() - startTicks;
        g_NumberOfVisits++;
#endif

	if (allocatedString)
		XP_FREE(stringToPut);	// event not done yet
	else if (streamBytesMax != 0)
    	imapConnection->NotifyEventCompletionMonitor();
    
}

// This function closes the message presentation stream
static
void NormalEndMsgWriteStream(void *blockingConnectionVoid, void *)
{
	PR_LOG(IMAP, out, ("STREAM: Normal End Message Download Stream Event"));
    TNavigatorImapConnection *imapConnection = 
        (TNavigatorImapConnection *) blockingConnectionVoid;
    
    NET_StreamClass *outputStream = imapConnection->GetOutputStream();
    if (outputStream)
	{
		if (imapConnection->GetPseudoInterrupted())
			(*outputStream->abort)(outputStream, -1);
		else
    		(*outputStream->complete) (outputStream);
		outputStream->data_object = NULL;
	}
    
    ActiveEntry *ce = imapConnection->GetActiveEntry();
            
#ifdef MACPERF
    char perfMessage[500];
    sprintf(perfMessage, 
            (const char *) "Download time = %ld, mkimap4 time = %ld, for %ld visits",
            TickCount() - g_StartTimeOfDownLoad,
            g_TimeSpendInDownLoadFunction, g_NumberOfVisits);
            
    DebugStr((const unsigned char *) c2pstr(perfMessage));
#endif
    
}

// This function aborts the message presentation stream
static
void AbortMsgWriteStream(void *blockingConnectionVoid, void *)
{
	PR_LOG(IMAP, out, ("STREAM: Abort Message Download Stream Event"));
    TNavigatorImapConnection *imapConnection = 
        (TNavigatorImapConnection *) blockingConnectionVoid;
    
    NET_StreamClass *outputStream = imapConnection->GetOutputStream();
    if (outputStream && outputStream->data_object)
	{
    	(*outputStream->abort) (outputStream, -1);
		outputStream->data_object = NULL;
	}
    
    ActiveEntry *ce = imapConnection->GetActiveEntry();
#ifdef MACPERF
    char perfMessage[500];
    sprintf(perfMessage, 
            (const char *) "Download time = %ld, mkimap4 time = %ld, for %ld visits",
            TickCount() - g_StartTimeOfDownLoad,
            g_TimeSpendInDownLoadFunction, g_NumberOfVisits);
            
    DebugStr((const unsigned char *) c2pstr(perfMessage));
#endif
}

// This function displays a modal alert dialog based on
// a XP_GetString of the passed id
static
void AlertEventFunction_UsingId(void *blockingConnectionVoid, 
						        void *errorMessageIdVoid)
{
    TNavigatorImapConnection *imapConnection = 
        (TNavigatorImapConnection *) blockingConnectionVoid;
    ActiveEntry *ce = imapConnection->GetActiveEntry();

	// if this is a biff context,
	// don't put up the alert
	if (IMAP_ContextIsBiff(ce))
	{
	    imapConnection->NotifyEventCompletionMonitor();
		return;
	}

    uint32 errorId = (uint32) errorMessageIdVoid;
    char *errorString = XP_GetString(errorId);

    if (errorString)
#ifdef XP_UNIX
		FE_Alert_modal(ce->window_id, errorString);
#else
    	FE_Alert(ce->window_id, errorString);
#endif
    
    imapConnection->NotifyEventCompletionMonitor();
}

// This function displays a modal alert dialog
static
void AlertEventFunction(void *blockingConnectionVoid, 
						void *errorMessageVoid)
{
    TNavigatorImapConnection *imapConnection = 
        (TNavigatorImapConnection *) blockingConnectionVoid;
    ActiveEntry *ce = imapConnection->GetActiveEntry();

	// if this is a biff context,
	// don't put up the alert
	if (IMAP_ContextIsBiff(ce))
	{
	    imapConnection->NotifyEventCompletionMonitor();
		return;
	}
    
    FE_Alert(ce->window_id, (const char *) errorMessageVoid);
    
    imapConnection->NotifyEventCompletionMonitor();
}

// This function displays a modal alert dialog, appending the
// message to "The IMAP server responded: "
static
void AlertEventFunctionFromServer(void *blockingConnectionVoid, 
						void *serverSaidVoid)
{
    TNavigatorImapConnection *imapConnection = 
        (TNavigatorImapConnection *) blockingConnectionVoid;
    ActiveEntry *ce = imapConnection->GetActiveEntry();

	// if this is a biff context,
	// don't put up the alert
	if (IMAP_ContextIsBiff(ce))
	{
	    imapConnection->NotifyEventCompletionMonitor();
		return;
	}
    
	const char *serverSaid = (const char *) serverSaidVoid;
	if (serverSaid)
	{
		char *whereRealMessage = XP_STRCHR(serverSaid, ' ');
		if (whereRealMessage)
			whereRealMessage++;
		if (whereRealMessage)
			whereRealMessage = XP_STRCHR(whereRealMessage, ' ');
		if (whereRealMessage)
			whereRealMessage++;

		char *message = PR_smprintf(XP_GetString(MK_MSG_IMAP_SERVER_SAID), whereRealMessage ? whereRealMessage : serverSaid);

		if (message) FE_Alert(ce->window_id, message);
		FREEIF(message);
	}

    imapConnection->NotifyEventCompletionMonitor();
}

// until preferences are worked out, save a copy of the password here 
static char *gIMAPpassword=NULL;       // gross
static XP_Bool preAuthSucceeded = FALSE;

const char * IMAP_GetPassword()
{
	return gIMAPpassword;
}

void IMAP_SetPassword(const char *password)
{
	FREEIF(gIMAPpassword);
	if (password)
		gIMAPpassword = XP_STRDUP(password);
}

void IMAP_SetPasswordForHost(const char *host, const char *password)
{
	TIMAPHostInfo::SetPasswordForHost(host, password);
}

void IMAP_ResetAnyCachedConnectionInfo()
{
	if (gIMAPpassword)
	{
		FREEIF(gIMAPpassword);
		gIMAPpassword = NULL;
	}
	preAuthSucceeded = FALSE;

	TNavigatorImapConnection::ResetCachedConnectionInfo();
}

// This function displays a modal alert dialog
static
void GetPasswordEventFunction(void *blockingConnectionVoid, 
								void *userNameVoid)
{
    TNavigatorImapConnection *imapConnection = 
        (TNavigatorImapConnection *) blockingConnectionVoid;
    ActiveEntry *ce = imapConnection->GetActiveEntry();
    
#if 1
	char* fmt = XP_GetString( XP_PASSWORD_FOR_POP3_USER );

	if (fmt)
	{
		const char *host = imapConnection->GetHostName();
		size_t len = (XP_STRLEN(fmt) +
                     (host ? XP_STRLEN(host) : 0) + 300) 
					 * sizeof(char);
		char *prompt = (char*)XP_ALLOC(len);
		if (prompt)
		{
			PR_snprintf(prompt, len, fmt, (char*)userNameVoid, host);

			const char* password = FE_PromptPassword(ce->window_id, prompt);
	        if (password != NULL)
				TIMAPHostInfo::SetPasswordForHost(host, password);
			XP_FREE(prompt);
		}
	}
#else
    char *prompt = NULL;
	char *promptText = XP_GetString(XP_PROMPT_ENTER_PASSWORD);

	if (promptText)
		prompt = PR_smprintf(promptText, (char *) userNameVoid);    
    
    if (prompt)
    {
        const char *password = FE_PromptPassword(ce->window_id, prompt);
        if (password != NULL)
			TIMAPHostInfo::SetPasswordForHost(imapConnection->GetHostName(), password);
		XP_FREE(prompt);
    }
#endif    
    imapConnection->NotifyEventCompletionMonitor();
}


// This function alerts the FE that we are past the password check
// (The WinFE needs this for progress updates)
static
void PastPasswordCheckFunction(void *blockingConnectionVoid, 
								void* /*unused*/)
{
    TNavigatorImapConnection *imapConnection = 
        (TNavigatorImapConnection *) blockingConnectionVoid;
    ActiveEntry *ce = imapConnection->GetActiveEntry();
    
	if (ce->window_id->imapURLPane != NULL &&
		MSG_GetPaneType(ce->window_id->imapURLPane) != MSG_COMPOSITIONPANE)
		FE_PaneChanged (ce->window_id->imapURLPane, FALSE, MSG_PanePastPasswordCheck, 0);
    
    imapConnection->NotifyEventCompletionMonitor();
}

// This function updates the progress message at the bottom of the
// window
static
void ProgressEventFunction(void *blockingConnectionVoid, 
							void *progressMessageVoid)
{
    TNavigatorImapConnection *imapConnection = 
        (TNavigatorImapConnection *) blockingConnectionVoid;
    ActiveEntry *ce = imapConnection->GetActiveEntry();

    NET_Progress(ce->window_id, (char *)progressMessageVoid);

    imapConnection->NotifyEventCompletionMonitor();
}

// This function updates the progress message at the bottom of the
// window based on a XP_GetString of the passed id
static
void ProgressStatusFunction_UsingId(void *blockingConnectionVoid, 
						        void *errorMessageIdVoid)
{
    TNavigatorImapConnection *imapConnection = 
        (TNavigatorImapConnection *) blockingConnectionVoid;
    ActiveEntry *ce = imapConnection->GetActiveEntry();
    
    uint32 errorId = (uint32) errorMessageIdVoid;
    char *errorString = XP_GetString(errorId);
    
    if (errorString)
    	NET_Progress(ce->window_id, errorString);

    imapConnection->NotifyEventCompletionMonitor();
}

// This function updates the progress message at the bottom of the
// window based on a XP_GetString of the passed id and formats
// it with the passed in string
static
void ProgressStatusFunction_UsingIdWithString(void *blockingConnectionVoid, 
						        void *errorMessageStruct)
{
	char * progressString = NULL;
    TNavigatorImapConnection *imapConnection = 
        (TNavigatorImapConnection *) blockingConnectionVoid;
    ActiveEntry *ce = imapConnection->GetActiveEntry();
    
    StatusMessageInfo * msgInfo = (StatusMessageInfo *) errorMessageStruct;
	if (msgInfo)
		progressString = PR_sprintf_append(progressString, XP_GetString(msgInfo->msgID), msgInfo->extraInfo);
    
    if (progressString)
    	NET_Progress(ce->window_id, progressString);

	FREEIF(msgInfo->extraInfo);
	FREEIF(msgInfo);
	FREEIF(progressString);
    
    imapConnection->NotifyEventCompletionMonitor();
}

// This function updates the progress message at the bottom of the
// window, along with setting the percent bar
static
void PercentProgressEventFunction(void *blockingConnectionVoid, 
							void *progressVoid)
{
    TNavigatorImapConnection *imapConnection = 
        (TNavigatorImapConnection *) blockingConnectionVoid;
    ActiveEntry *ce = imapConnection->GetActiveEntry();
	ProgressInfo *progress = (ProgressInfo *)progressVoid;

    if (progress && progress->message)
			NET_Progress(ce->window_id, progress->message);

	FE_SetProgressBarPercent(ce->window_id, progress->percent);

	FREEIF(progress->message);
	FREEIF(progress);

    imapConnection->NotifyEventCompletionMonitor();
}




// This function uses the existing NET code to establish
// a connection with the IMAP server.
static
void StartIMAPConnection(TNavigatorImapConnection *imapConnection)
{

    ActiveEntry *ce = imapConnection->GetActiveEntry();
    
    FE_SetProgressBarPercent(ce->window_id, -1);

	HG83344

    ce->status = NET_BeginConnect(ce->URL_s->address,
                                  ce->URL_s->IPAddressString,
                "IMAP4",
                (HG26227 IMAP4_PORT),
                &ce->socket,
                HG27327,
                imapConnection->GetTCPConData(),
                ce->window_id,
                &ce->URL_s->error_msg,
                                 ce->socks_host,
                                 ce->socks_port,
                                 ce->URL_s->localIP);

    imapConnection->SetIOSocket(ce->socket);
    if(ce->socket != 0)
    NET_TotalNumberOfOpenConnections++;

    if(ce->status == MK_CONNECTED)
      {
		imapConnection->SetNeedsGreeting(TRUE);
		NET_SetReadSelect(ce->window_id, ce->socket);
      }
    else if(ce->status > -1)
      {
    ce->con_sock = ce->socket;  /* set con sock so we can select on it */
    NET_SetConnectSelect(ce->window_id, ce->con_sock);
      }
    else if(ce->status < 0)
      {
        /* close and clear the socket here
         * so that we don't try and send a RSET
         */
        if(ce->socket != 0)
          {
        NET_TotalNumberOfOpenConnections--;
        NET_ClearConnectSelect(ce->window_id, ce->socket);
        TRACEMSG(("Closing and clearing socket ce->socket: %d", 
                 ce->socket));
		net_graceful_shutdown(ce->socket, HG38373);
        PR_Close(ce->socket);
            ce->socket = NULL;
            imapConnection->SetIOSocket(ce->socket);
          }
      }
      
    imapConnection->SetConnectionStatus(ce->status);
}

// This function uses the existing NET code to establish
// a connection with the IMAP server.
static
void FinishIMAPConnection(void *blockingConnectionVoid, 
							void* /*unused*/)
{
    TNavigatorImapConnection *imapConnection = 
        (TNavigatorImapConnection *) blockingConnectionVoid;
    
    ActiveEntry *ce = imapConnection->GetActiveEntry();

	HG83344
		
    ce->status = NET_FinishConnect(ce->URL_s->address,
                "IMAP4",
                (HG72524 IMAP4_PORT),
                &ce->socket,
                imapConnection->GetTCPConData(),
                ce->window_id,
                &ce->URL_s->error_msg,
                ce->URL_s->localIP);

    imapConnection->SetIOSocket(ce->socket);
    if(ce->status == MK_CONNECTED)
	{
		NET_ClearConnectSelect(ce->window_id, ce->con_sock);
		NET_SetReadSelect(ce->window_id, ce->socket);
	}
    else if(ce->status > -1)
	{

    /* unregister the old CE_SOCK from the select list
     * and register the new value in the case that it changes
     */
		if(ce->con_sock != ce->socket)
		{
			NET_ClearConnectSelect(ce->window_id, ce->con_sock);
			ce->con_sock = ce->socket;
			NET_SetConnectSelect(ce->window_id, ce->con_sock);
		}
	}
    else if(ce->status < 0)
	{
        /* close and clear the socket here
         * so that we don't try and send a RSET
         */
		NET_TotalNumberOfOpenConnections--;
		NET_ClearConnectSelect(ce->window_id, ce->socket);
		TRACEMSG(("Closing and clearing socket ce->socket: %d", ce->socket));
		net_graceful_shutdown(ce->socket, HG73654);
		PR_Close(ce->socket);
        ce->socket = NULL;
        imapConnection->SetIOSocket(ce->socket);
	}
      
    imapConnection->SetConnectionStatus(ce->status);
    imapConnection->NotifyEventCompletionMonitor();
}



// member functions of the TImapFEEvent class
TImapFEEvent::TImapFEEvent(FEEventFunctionPointer *function, 
                           void *arg1, void*arg2,
						   XP_Bool executeWhenInterrupted) :
    fEventFunction(function),
    fArg1(arg1), fArg2(arg2),
	fExecuteWhenInterrupted(executeWhenInterrupted)
{
    
}

TImapFEEvent::~TImapFEEvent()
{
}

void TImapFEEvent::DoEvent()
{
    (*fEventFunction) (fArg1, fArg2);
}

// member functions of the TImapFEEventQueue class
TImapFEEventQueue::TImapFEEventQueue()
{
    fEventList      = XP_ListNew();
    fListMonitor    = PR_NewNamedMonitor("list-monitor");
    fNumberOfEvents = 0;
}

TImapFEEventQueue *TImapFEEventQueue::CreateFEEventQueue()
{
    TImapFEEventQueue *returnQueue = new TImapFEEventQueue;
    if (!returnQueue ||
        !returnQueue->fEventList ||
        !returnQueue->fListMonitor)
    {
        delete returnQueue;
        returnQueue = nil;
    }
    
    return returnQueue;
}

TImapFEEventQueue::~TImapFEEventQueue()
{
    if (fEventList)
        XP_ListDestroy(fEventList);
    if (fListMonitor)
        PR_DestroyMonitor(fListMonitor);
}

void TImapFEEventQueue::AdoptEventToEnd(TImapFEEvent *event)
{
    PR_EnterMonitor(fListMonitor);
    XP_ListAddObjectToEnd(fEventList, event);
    fNumberOfEvents++;
    PR_ExitMonitor(fListMonitor);
}

void TImapFEEventQueue::AdoptEventToBeginning(TImapFEEvent *event)
{
    PR_EnterMonitor(fListMonitor);
    XP_ListAddObject(fEventList, event);
    fNumberOfEvents++;
    PR_ExitMonitor(fListMonitor);
}

TImapFEEvent*   TImapFEEventQueue::OrphanFirstEvent()
{
    TImapFEEvent *returnEvent = nil;
    PR_EnterMonitor(fListMonitor);
    if (fNumberOfEvents)
    {
        fNumberOfEvents--;
        if (fNumberOfEvents == 0)
            PR_Notify(fListMonitor);
        returnEvent = (TImapFEEvent *) XP_ListRemoveTopObject(fEventList);
    }
    PR_ExitMonitor(fListMonitor);
    
    return returnEvent;
}

int     TImapFEEventQueue::NumberOfEventsInQueue()
{
	int returnEvents;
    PR_EnterMonitor(fListMonitor);
    returnEvents = fNumberOfEvents;
    PR_ExitMonitor(fListMonitor);
    return returnEvents;
}


TIMAP4BlockingConnection::TIMAP4BlockingConnection() :
    fConnectionStatus(0),   // we want NET_ProcessIMAP4 to continue for now
    fServerState(*this),
    fBlockedForIO(FALSE),
    fCloseNeededBeforeSelect(FALSE),
    fCurrentServerCommandTagNumber(0)
{
    fDataMemberMonitor       = PR_NewNamedMonitor("data-member-monitor");
    fIOMonitor               = PR_NewNamedMonitor("io-monitor");
    
    fDeleteModelIsMoveToTrash = FALSE;
}


XP_Bool TIMAP4BlockingConnection::ConstructionSuccess()
{
    return (fDataMemberMonitor && fIOMonitor);
}






TIMAP4BlockingConnection::~TIMAP4BlockingConnection()
{
    if (fDataMemberMonitor)
        PR_DestroyMonitor(fDataMemberMonitor);
    if (fIOMonitor)
        PR_DestroyMonitor(fIOMonitor);
}


TNavigatorImapConnection *TIMAP4BlockingConnection::GetNavigatorConnection()
{
    return NULL;
}


void TIMAP4BlockingConnection::NotifyIOCompletionMonitor()
{
    PR_EnterMonitor(fIOMonitor);
    fBlockedForIO = FALSE;
    PR_Notify(fIOMonitor);                  
    PR_ExitMonitor(fIOMonitor);
}


// escape any backslashes or quotes.  Backslashes are used a lot with our NT server
char *TIMAP4BlockingConnection::CreateEscapedMailboxName(const char *rawName)
{
	int numberOfEscapes = 0;
	char *escapedName = (char *) XP_ALLOC(XP_STRLEN(rawName) + 20);	// enough for 20 deep
	if (escapedName)
	{
		char *currentChar = escapedName;
		XP_STRCPY(escapedName,rawName);
		
		while (*currentChar)
		{
			if ((*currentChar == '\\') || (*currentChar == '\"'))
			{
				XP_MEMMOVE(currentChar+1, 	// destination
						   currentChar, 	// source
						   XP_STRLEN(currentChar) + 1);  // remaining string + nil
				*currentChar++ = '\\';
				
				if (++numberOfEscapes == 20)
				{
					char *oldString = escapedName;
					escapedName = (char *) XP_ALLOC(XP_STRLEN(oldString) + 20);	// enough for 20 deep
					XP_STRCPY(escapedName,oldString);
					currentChar = escapedName + (currentChar - oldString);
					FREEIF( oldString);
					numberOfEscapes = 0;
				}
			}
			currentChar++;
		}
	}
	
	return escapedName;
}



void TIMAP4BlockingConnection::IncrementCommandTagNumber()
{
    sprintf(fCurrentServerCommandTag,"%ld", (long) ++fCurrentServerCommandTagNumber);
}

#ifdef KMCENTEE_DEBUG 
int WaitForIOLoopCount;
int WaitForFEEventLoopCount;
#endif

void TIMAP4BlockingConnection::WaitForIOCompletion()
{
	PRIntervalTime sleepTime = PR_MicrosecondsToInterval(kImapSleepTime);

    PR_EnterMonitor(fIOMonitor);
    fBlockedForIO = TRUE;
    
#ifdef KMCENTEE_DEBUG 
	WaitForIOLoopCount=0;
#endif

    while (fBlockedForIO && !DeathSignalReceived() )
    {
        PR_Wait(fIOMonitor, sleepTime);  
#ifdef KMCENTEE_DEBUG 
	WaitForIOLoopCount++;
	if (WaitForIOLoopCount == 10)
		XP_ASSERT(FALSE);
#endif
    }               
    PR_ExitMonitor(fIOMonitor);
}

XP_Bool TIMAP4BlockingConnection::BlockedForIO()
{
    XP_Bool returnValue;
    
    PR_EnterMonitor(fIOMonitor);
    returnValue = fBlockedForIO;
    PR_ExitMonitor(fIOMonitor);
    
    return returnValue;
}


TImapServerState &TIMAP4BlockingConnection::GetServerStateParser()
{
    return fServerState;
}


void TIMAP4BlockingConnection::SetConnectionStatus(int status)
{
    PR_EnterMonitor(GetDataMemberMonitor());
    fConnectionStatus = status;
    PR_ExitMonitor(GetDataMemberMonitor());
}


int TIMAP4BlockingConnection::GetConnectionStatus()
{
    int returnStatus;
    PR_EnterMonitor(GetDataMemberMonitor());
    returnStatus = fConnectionStatus;
    PR_ExitMonitor(GetDataMemberMonitor());
    return returnStatus;
}


extern "C" void MSG_IMAP_KillConnection(TNavigatorImapConnection *imapConnection)
{
	if (imapConnection)
	{
		imapConnection->DeathSignalReceived();		// stop anything going on and kill thread
		//delete imapConnection;						// destroy anything else, not mine to destroy.
	}
}


void TNavigatorImapConnection::SelectMailbox(const char *mailboxName)
{
	TIMAP4BlockingConnection::SelectMailbox(mailboxName);

	int32 numOfMessagesInFlagState =  fFlagState->GetNumberOfMessages();
	TIMAPUrl::EIMAPurlType urlType = fCurrentUrl->GetIMAPurlType();
	// if we've selected a mailbox, and we're not going to do an update because of the
	// url type, but don't have the flags, go get them!
	if (urlType != TIMAPUrl::kSelectFolder && urlType != TIMAPUrl::kExpungeFolder && urlType != TIMAPUrl::kLiteSelectFolder &&
		urlType != TIMAPUrl::kDeleteAllMsgs && 
		((GetServerStateParser().NumberOfMessages() != numOfMessagesInFlagState) && (numOfMessagesInFlagState == 0))) 
	   	ProcessMailboxUpdate(FALSE);	

}

    // authenticated state commands 
void TIMAP4BlockingConnection::SelectMailbox(const char *mailboxName)
{
    ProgressEventFunction_UsingId (MK_IMAP_STATUS_SELECTING_MAILBOX);
    IncrementCommandTagNumber();
    
    fCloseNeededBeforeSelect = FALSE;		// initial value
	GetServerStateParser().ResetFlagInfo(0);    
    char *escapedName = CreateEscapedMailboxName(mailboxName);
    
    PR_snprintf(GetOutputBuffer(),                              // string to create
            kOutputBufferSize,                              // max size
            "%s select \"%s\"" CRLF,                    // format string
            GetServerCommandTag(),                  // command tag
            escapedName);
            
    FREEIF( escapedName);
           
    int                 ioStatus = WriteLineToSocket(GetOutputBuffer());
	ParseIMAPandCheckForNewMail();
}

void TIMAP4BlockingConnection::CreateMailbox(const char *mailboxName)
{
    ProgressEventFunction_UsingId (MK_IMAP_STATUS_CREATING_MAILBOX);

    IncrementCommandTagNumber();
    
    char *escapedName = CreateEscapedMailboxName(mailboxName);
    
    PR_snprintf(GetOutputBuffer(),                              // string to create
            kOutputBufferSize,                              // max size
            "%s create \"%s\"" CRLF,                    // format string
            GetServerCommandTag(),                  // command tag
            escapedName);
                 
    FREEIF( escapedName);

    int                 ioStatus = WriteLineToSocket(GetOutputBuffer());
    
	ParseIMAPandCheckForNewMail();
}

static void setup_message_flags_string(char *flagString, 
									   imapMessageFlagsType flags, XP_Bool userDefined)
{
  XP_ASSERT(flagString);
  flagString[0] = '\0';
  if (flags & kImapMsgSeenFlag)
	XP_STRCAT(flagString, "\\Seen ");
  if (flags & kImapMsgAnsweredFlag)
	XP_STRCAT(flagString, "\\Answered ");
  if (flags & kImapMsgFlaggedFlag)
	XP_STRCAT(flagString, "\\Flagged ");
  if (flags & kImapMsgDeletedFlag)
	XP_STRCAT(flagString, "\\Deleted ");
  if (flags & kImapMsgDraftFlag)
	XP_STRCAT(flagString, "\\Draft ");
  if (flags & kImapMsgRecentFlag)
	XP_STRCAT(flagString, "\\Recent ");
  if ((flags & kImapMsgForwardedFlag) && userDefined)
	XP_STRCAT(flagString, "Forwarded ");	// Not always available
  if ((flags & kImapMsgMDNSentFlag) && userDefined)
	XP_STRCAT(flagString, "MDNSent ");	// Not always available

  // eat the last space
  if (*flagString)
	*(flagString + XP_STRLEN(flagString) - 1) = '\0';
}

void TIMAP4BlockingConnection::AppendMessage(const char *mailboxName, 
                                         	const char *messageSizeString,
                               				imapMessageFlagsType flags)
{
    //ProgressUpdateEvent("Appending a message...");
    IncrementCommandTagNumber();
    
    char *escapedName = CreateEscapedMailboxName(mailboxName);
    
    // enough room for "\\Seen \\Answered \\Flagged \\Deleted \\Draft \\Recent \\Replied"
    char flagString[90];
    
	setup_message_flags_string(flagString, flags, SupportsUserDefinedFlags());
    
    PR_snprintf(GetOutputBuffer(),                              // string to create
            kOutputBufferSize,                              // max size
            "%s append \"%s\" (%s) {%s}" CRLF,               // format string
            GetServerCommandTag(),                  // command tag
            escapedName,
            flagString,
            messageSizeString);
            
    FREEIF( escapedName);

    int                 ioStatus = WriteLineToSocket(GetOutputBuffer());
    
	ParseIMAPandCheckForNewMail();
    
    if (GetServerStateParser().GetIMAPstate() == TImapServerState::kWaitingForMoreClientInput)
    {
    	OnlineCopyCompleted(kReadyForAppendData);
        UploadMessage();
		ParseIMAPandCheckForNewMail();
        if (GetServerStateParser().LastCommandSuccessful())
        	OnlineCopyCompleted(kInProgress);
        else
        	OnlineCopyCompleted(kFailedAppend);
    }
}


void TIMAP4BlockingConnection::DeleteMailbox(const char *mailboxName)
{
    ProgressEventFunction_UsingIdWithString (MK_IMAP_STATUS_DELETING_MAILBOX, mailboxName);

    IncrementCommandTagNumber();
    
    char *escapedName = CreateEscapedMailboxName(mailboxName);
    
    PR_snprintf(GetOutputBuffer(),                              // string to create
            kOutputBufferSize,                              // max size
            "%s delete \"%s\"" CRLF,                    // format string
            GetServerCommandTag(),                  // command tag
            escapedName);
            
    FREEIF( escapedName);
            
    int                 ioStatus = WriteLineToSocket(GetOutputBuffer());
    
	ParseIMAPandCheckForNewMail();
}

void TIMAP4BlockingConnection::RenameMailbox(const char *existingName, 
                                      const char *newName)
{
    ProgressEventFunction_UsingIdWithString (MK_IMAP_STATUS_RENAMING_MAILBOX, existingName);
    
	IncrementCommandTagNumber();
    
    char *escapedExistingName = CreateEscapedMailboxName(existingName);
    char *escapedNewName = CreateEscapedMailboxName(newName);
    
    PR_snprintf(GetOutputBuffer(),                              // string to create
            kOutputBufferSize,                              // max size
            "%s rename \"%s\" \"%s\"" CRLF,                 // format string
            GetServerCommandTag(),                  // command tag
            escapedExistingName,
            escapedNewName);
            
    FREEIF( escapedExistingName);
    FREEIF( escapedNewName);
            
    int                 ioStatus = WriteLineToSocket(GetOutputBuffer());
    
	ParseIMAPandCheckForNewMail();
}

int64 TIMAP4BlockingConnection::fgTimeStampOfNonPipelinedList = LL_ZERO;


void TIMAP4BlockingConnection::TimeStampListNow()
{
	fgTimeStampOfNonPipelinedList = PR_Now();
}

extern "C" {
int64 IMAP_GetTimeStampOfNonPipelinedList()
{
	return TIMAP4BlockingConnection::GetTimeStampOfNonPipelinedList();
}
}

void TIMAP4BlockingConnection::List(const char *mailboxPattern, XP_Bool pipeLined /* = FALSE */, XP_Bool dontTimeStamp /* = FALSE */)
{
    ProgressEventFunction_UsingId (MK_IMAP_STATUS_LOOKING_FOR_MAILBOX);

    IncrementCommandTagNumber();
    
    char *escapedPattern = CreateEscapedMailboxName(mailboxPattern);

    PR_snprintf(GetOutputBuffer(),                              // string to create
            kOutputBufferSize,                              // max size
            "%s list \"\" \"%s\"" CRLF,                   // format string
            GetServerCommandTag(),                  // command tag
            escapedPattern);
            
    FREEIF( escapedPattern);

	int                 ioStatus = WriteLineToSocket(GetOutputBuffer());
    
    if (pipeLined)
    	GetServerStateParser().IncrementNumberOfTaggedResponsesExpected(GetServerCommandTag());
    else
    {
		if (!dontTimeStamp)
    		TimeStampListNow();
		ParseIMAPandCheckForNewMail();
    }
}

void TIMAP4BlockingConnection::Lsub(const char *mailboxPattern,
									XP_Bool pipelined)
{
    ProgressEventFunction_UsingId (MK_IMAP_STATUS_LOOKING_FOR_MAILBOX);

    IncrementCommandTagNumber();
    
    char *escapedPattern = CreateEscapedMailboxName(mailboxPattern);

    PR_snprintf(GetOutputBuffer(),                              // string to create
            kOutputBufferSize,                              // max size
            "%s lsub \"\" \"%s\"" CRLF,                   // format string
            GetServerCommandTag(),                  // command tag
            escapedPattern);
            
    FREEIF( escapedPattern);

    int                 ioStatus = WriteLineToSocket(GetOutputBuffer());
    
    if (pipelined)
    	GetServerStateParser().IncrementNumberOfTaggedResponsesExpected(GetServerCommandTag());
    else
    {
    	TimeStampListNow();
		ParseIMAPandCheckForNewMail();
    }
}

void TIMAP4BlockingConnection::Subscribe(const char *mailboxName)
{
    ProgressEventFunction_UsingIdWithString (MK_IMAP_STATUS_SUBSCRIBE_TO_MAILBOX, mailboxName);

    IncrementCommandTagNumber();
    
    char *escapedName = CreateEscapedMailboxName(mailboxName);

    PR_snprintf(GetOutputBuffer(),                              // string to create
            kOutputBufferSize,                              // max size
            "%s subscribe \"%s\"" CRLF,                 // format string
            GetServerCommandTag(),                  // command tag
            escapedName);
            
    FREEIF( escapedName);

    int                 ioStatus = WriteLineToSocket(GetOutputBuffer());
    
	ParseIMAPandCheckForNewMail();
}

void TIMAP4BlockingConnection::Unsubscribe(const char *mailboxName)
{
    ProgressEventFunction_UsingIdWithString (MK_IMAP_STATUS_UNSUBSCRIBE_MAILBOX, mailboxName);
    IncrementCommandTagNumber();
    
    char *escapedName = CreateEscapedMailboxName(mailboxName);
    
    PR_snprintf(GetOutputBuffer(),                              // string to create
            kOutputBufferSize,                              // max size
            "%s unsubscribe \"%s\"" CRLF,               // format string
            GetServerCommandTag(),                  // command tag
            escapedName);
            
    FREEIF( escapedName);

    int                 ioStatus = WriteLineToSocket(GetOutputBuffer());

	ParseIMAPandCheckForNewMail();
}

void TIMAP4BlockingConnection::Search(const char *searchCriteria,
									  XP_Bool useUID, 
									  XP_Bool notifyHit /* TRUE */)
{
	fNotifyHit = notifyHit;
    ProgressEventFunction_UsingId (MK_IMAP_STATUS_SEARCH_MAILBOX);
    IncrementCommandTagNumber();
    
    char *formatString;
    // the searchCriteria string contains the 'search ....' string
    if (useUID)
        formatString = "%s uid %s\015\012";
    else
        formatString = "%s %s\015\012";

    PR_snprintf(GetOutputBuffer(),                              // string to create
            kOutputBufferSize,                              // max size
            formatString,                                   // format string
            GetServerCommandTag(),                  // command tag
            searchCriteria);
            
    int                 ioStatus = WriteLineToSocket(GetOutputBuffer());
    
	ParseIMAPandCheckForNewMail();
}

void TIMAP4BlockingConnection::Copy(const char *messageList,
                                    const char *destinationMailbox, 
                                    XP_Bool idsAreUid)
{
    IncrementCommandTagNumber();
    
    char *escapedDestination = CreateEscapedMailboxName(destinationMailbox);

    char *formatString;
    if (idsAreUid)
        formatString = "%s uid copy %s \"%s\"\015\012";
    else
        formatString = "%s copy %s \"%s\"\015\012";

		// since messageIds can be infinitely long, use a dynamic buffer rather than the fixed one
	const char *commandTag = GetServerCommandTag();
	int protocolStringSize = XP_STRLEN(formatString) + XP_STRLEN(messageList) + XP_STRLEN(commandTag) + XP_STRLEN(escapedDestination) + 1;
	char *protocolString = (char *) XP_ALLOC( protocolStringSize );

	if (protocolString)
	{
	    PR_snprintf(protocolString,                              // string to create
	            protocolStringSize,                              // max size
	            formatString,                                   // format string
	            commandTag,                  					// command tag
	            messageList,
	            escapedDestination);
	            
	    
	    int                 ioStatus = WriteLineToSocket(protocolString);
	    
		ParseIMAPandCheckForNewMail(protocolString);
	    XP_FREE(protocolString);
   	}
    else
        HandleMemoryFailure();
        
    FREEIF( escapedDestination);
}

void TIMAP4BlockingConnection::Store(const char *messageList,
                             const char *messageData, 
                                     XP_Bool idsAreUid)
{
   //ProgressUpdateEvent("Saving server message flags...");
   IncrementCommandTagNumber();
    
    char *formatString;
    if (idsAreUid)
        formatString = "%s uid store %s %s\015\012";
    else
        formatString = "%s store %s %s\015\012";
        
    // we might need to close this mailbox after this
	fCloseNeededBeforeSelect = fDeleteModelIsMoveToTrash && (XP_STRCASESTR(messageData, "\\Deleted"));

	const char *commandTag = GetServerCommandTag();
	int protocolStringSize = XP_STRLEN(formatString) + XP_STRLEN(messageList) + XP_STRLEN(messageData) + XP_STRLEN(commandTag) + 1;
	char *protocolString = (char *) XP_ALLOC( protocolStringSize );

	if (protocolString)
	{
	    PR_snprintf(protocolString,                              // string to create
	            protocolStringSize,                              // max size
	            formatString,                                   // format string
	            commandTag,                  					// command tag
	            messageList,
	            messageData);
	            
	    int                 ioStatus = WriteLineToSocket(protocolString);
	    
		ParseIMAPandCheckForNewMail(protocolString);
	    XP_FREE(protocolString);
    }
    else
    	HandleMemoryFailure();
}

void TIMAP4BlockingConnection::Close()
{
    ProgressEventFunction_UsingId (MK_IMAP_STATUS_CLOSE_MAILBOX);

    IncrementCommandTagNumber();
	GetServerStateParser().ResetFlagInfo(0);
    
    PR_snprintf(GetOutputBuffer(),                                      // string to create
            kOutputBufferSize,                                      // max size
            "%s close" CRLF,                                        // format string
            GetServerCommandTag());                         // command tag
            
    int                 ioStatus = WriteLineToSocket(GetOutputBuffer());
    
	ParseIMAPandCheckForNewMail();
}

void TIMAP4BlockingConnection::Check()
{
    //ProgressUpdateEvent("Checking mailbox...");
    IncrementCommandTagNumber();
    
    PR_snprintf(GetOutputBuffer(),                                      // string to create
            kOutputBufferSize,                                      // max size
            "%s check" CRLF,                                        // format string
            GetServerCommandTag());                         // command tag
            
    int                 ioStatus = WriteLineToSocket(GetOutputBuffer());
    
	ParseIMAPandCheckForNewMail();
}

void TIMAP4BlockingConnection::Expunge()
{
    ProgressEventFunction_UsingId (MK_IMAP_STATUS_EXPUNGING_MAILBOX);
    IncrementCommandTagNumber();
    
    PR_snprintf(GetOutputBuffer(),                                      // string to create
            kOutputBufferSize,                                      // max size
            "%s expunge" CRLF,                                      // format string
            GetServerCommandTag());                         // command tag
            
    int                 ioStatus = WriteLineToSocket(GetOutputBuffer());
    
	ParseIMAPandCheckForNewMail();
}

// any state commands
void TIMAP4BlockingConnection::Logout()
{
	ProgressEventFunction_UsingId (MK_IMAP_STATUS_LOGGING_OUT);
	IncrementCommandTagNumber();

	PR_snprintf(GetOutputBuffer(),                                      // string to create
		  kOutputBufferSize,                                      // max size
		  "%s logout" CRLF,                                       // format string
		  GetServerCommandTag());                         // command tag
    
	int                 ioStatus = WriteLineToSocket(GetOutputBuffer());

	// the socket may be dead before we read the response, so drop it.
	ParseIMAPandCheckForNewMail();
}

void TIMAP4BlockingConnection::Noop()
{
    //ProgressUpdateEvent("noop...");
    IncrementCommandTagNumber();
    
    PR_snprintf(GetOutputBuffer(),                                      // string to create
            kOutputBufferSize,                                      // max size
            "%s noop" CRLF,                                         // format string
            GetServerCommandTag());                         // command tag
            
    int                 ioStatus = WriteLineToSocket(GetOutputBuffer());
    
	ParseIMAPandCheckForNewMail();
}


void TIMAP4BlockingConnection::Capability()
{

    ProgressEventFunction_UsingId (MK_IMAP_STATUS_CHECK_COMPAT);
    IncrementCommandTagNumber();
    
    PR_snprintf(GetOutputBuffer(),                                      // string to create
            kOutputBufferSize,                                      // max size
            "%s capability" CRLF,                                         // format string
            GetServerCommandTag());                         // command tag
            
    int                 ioStatus = WriteLineToSocket(GetOutputBuffer());
    
	ParseIMAPandCheckForNewMail();
}

void TIMAP4BlockingConnection::XServerInfo()
{

    ProgressEventFunction_UsingId (MK_IMAP_GETTING_SERVER_INFO);
    IncrementCommandTagNumber();
    
    PR_snprintf(GetOutputBuffer(),                                      // string to create
            kOutputBufferSize,                                      // max size
            "%s XSERVERINFO MANAGEACCOUNTURL MANAGELISTSURL MANAGEFILTERSURL" CRLF,
            GetServerCommandTag());                         // command tag
            
    int                 ioStatus = WriteLineToSocket(GetOutputBuffer());
    
	ParseIMAPandCheckForNewMail();
}

void TIMAP4BlockingConnection::XMailboxInfo(const char *mailboxName)
{

    ProgressEventFunction_UsingId (MK_IMAP_GETTING_MAILBOX_INFO);
    IncrementCommandTagNumber();
    
    PR_snprintf(GetOutputBuffer(),                                      // string to create
            kOutputBufferSize,                                      // max size
            "%s XMAILBOXINFO %s MANAGEURL POSTURL" CRLF,
            GetServerCommandTag(),
			mailboxName);                         // command tag
            
    int                 ioStatus = WriteLineToSocket(GetOutputBuffer());
    
	ParseIMAPandCheckForNewMail();
}





void TIMAP4BlockingConnection::Namespace()
{

    ProgressEventFunction_UsingId (MK_IMAP_STATUS_GETTING_NAMESPACE);
    IncrementCommandTagNumber();
    
    PR_snprintf(GetOutputBuffer(),                                      // string to create
            kOutputBufferSize,                                      // max size
            "%s namespace" CRLF,                                         // format string
            GetServerCommandTag());                         // command tag
            
    int                 ioStatus = WriteLineToSocket(GetOutputBuffer());
    
	ParseIMAPandCheckForNewMail();
}


void TIMAP4BlockingConnection::MailboxData()
{
    IncrementCommandTagNumber();

    PR_snprintf(GetOutputBuffer(),                                      // string to create
            kOutputBufferSize,                                      // max size
            "%s mailboxdata" CRLF,                                         // format string
            GetServerCommandTag());                         // command tag
            
    int                 ioStatus = WriteLineToSocket(GetOutputBuffer());
	ParseIMAPandCheckForNewMail();
}


void TIMAP4BlockingConnection::GetMyRightsForFolder(const char *mailboxName)
{
    IncrementCommandTagNumber();

    char *escapedName = CreateEscapedMailboxName(mailboxName);
    
    PR_snprintf(GetOutputBuffer(),                              // string to create
            kOutputBufferSize,                              // max size
            "%s myrights \"%s\"" CRLF,               // format string
            GetServerCommandTag(),                  // command tag
            escapedName);
            
    FREEIF( escapedName);
            
    int                 ioStatus = WriteLineToSocket(GetOutputBuffer());
    
    ParseIMAPandCheckForNewMail();
}

void TIMAP4BlockingConnection::GetACLForFolder(const char *mailboxName)
{
    IncrementCommandTagNumber();

    char *escapedName = CreateEscapedMailboxName(mailboxName);
    
    PR_snprintf(GetOutputBuffer(),                              // string to create
            kOutputBufferSize,                              // max size
            "%s getacl \"%s\"" CRLF,               // format string
            GetServerCommandTag(),                  // command tag
            escapedName);
            
    FREEIF( escapedName);
            
    int                 ioStatus = WriteLineToSocket(GetOutputBuffer());
    
    ParseIMAPandCheckForNewMail();
}

void TIMAP4BlockingConnection::StatusForFolder(const char *mailboxName)
{
    IncrementCommandTagNumber();

    char *escapedName = CreateEscapedMailboxName(mailboxName);
    
    PR_snprintf(GetOutputBuffer(),                              // string to create
            kOutputBufferSize,                              // max size
            "%s STATUS \"%s\" (UIDNEXT MESSAGES UNSEEN)" CRLF,               // format string
            GetServerCommandTag(),                  // command tag
            escapedName);
            
    FREEIF( escapedName);
            
    int                 ioStatus = WriteLineToSocket(GetOutputBuffer());
    
    ParseIMAPandCheckForNewMail();
}

void TNavigatorImapConnection::StatusForFolder(const char *mailboxName)
{
	TIMAP4BlockingConnection::StatusForFolder(mailboxName);
    mailbox_spec *new_spec = GetServerStateParser().CreateCurrentMailboxSpec(mailboxName);
	if (new_spec)
		UpdateMailboxStatus(new_spec);
}

void TIMAP4BlockingConnection::Netscape()
{
    //ProgressUpdateEvent("Netscape...");
    IncrementCommandTagNumber();
    
    PR_snprintf(GetOutputBuffer(),                                      // string to create
            kOutputBufferSize,                                      // max size
            "%s netscape" CRLF,                                         // format string
            GetServerCommandTag());                         // command tag
            
    int                 ioStatus = WriteLineToSocket(GetOutputBuffer());
    
	ParseIMAPandCheckForNewMail();
}


void TIMAP4BlockingConnection::InsecureLogin(const char *userName, 
                                             const char *password)
{

    ProgressEventFunction_UsingId (MK_IMAP_STATUS_SENDING_LOGIN);
    IncrementCommandTagNumber();
    
    PR_snprintf(GetOutputBuffer(),                                              // string to create
            kOutputBufferSize,                                      // max size
            "%s login \"%s\" \"%s\"" CRLF,                          // format string
            GetServerCommandTag(),                  // command tag
            userName, 
            password);
            
    int                 ioStatus = WriteLineToSocket(GetOutputBuffer());
	ParseIMAPandCheckForNewMail();
}


void TIMAP4BlockingConnection::AuthLogin(const char *userName, 
                                             const char *password)
{
	ProgressEventFunction_UsingId (MK_IMAP_STATUS_SENDING_AUTH_LOGIN);
    IncrementCommandTagNumber();

	const char *currentTagToken = GetServerCommandTag();
	char *currentCommand = NULL;
    
    PR_snprintf(GetOutputBuffer(),                       // string to create
				kOutputBufferSize,                       // max size
				"%s authenticate login" CRLF,            // format string
				GetServerCommandTag());                  // command tag
            
    int ioStatus = WriteLineToSocket(GetOutputBuffer());


	StrAllocCopy(currentCommand, GetOutputBuffer());

	ParseIMAPandCheckForNewMail();

	if (GetServerStateParser().LastCommandSuccessful()) {
		char *base64Str = NET_Base64Encode((char*)userName, XP_STRLEN(userName));
		PR_snprintf(GetOutputBuffer(),
					kOutputBufferSize,
					"%s" CRLF,
					base64Str);
		XP_FREEIF(base64Str);
		ioStatus = WriteLineToSocket(GetOutputBuffer());
		ParseIMAPandCheckForNewMail(currentCommand);
		if (GetServerStateParser().LastCommandSuccessful()) {
			char *base64Str = NET_Base64Encode((char*)password, XP_STRLEN(password));
			PR_snprintf(GetOutputBuffer(),
						kOutputBufferSize,
						"%s" CRLF,
						base64Str);
			XP_FREEIF(base64Str);
			ioStatus = WriteLineToSocket(GetOutputBuffer());
			ParseIMAPandCheckForNewMail(currentCommand);
			if (GetServerStateParser().LastCommandSuccessful())
			{
				FREEIF(currentCommand);
				return;
			}
		}
	}

	FREEIF(currentCommand);
	// fall back to use InsecureLogin()
	InsecureLogin(userName, password);
}



void TIMAP4BlockingConnection::ProcessBIFFRequest()
{
    if (GetConnectionStatus() == MK_WAITING_FOR_CONNECTION)
		EstablishServerConnection();
    if (GetConnectionStatus() >= 0)
        SetConnectionStatus(-1);                // stub!
}



char *TIMAP4BlockingConnection::GetOutputBuffer()
{
    return fOutputBuffer;
}

char *TIMAP4BlockingConnection::GetServerCommandTag()
{
    return fCurrentServerCommandTag;
}

PRMonitor *TIMAP4BlockingConnection::GetDataMemberMonitor()
{
    return fDataMemberMonitor;
}

XP_Bool TIMAP4BlockingConnection::HandlingMultipleMessages(char *messageIdString)
{
	return (XP_STRCHR(messageIdString,',') != NULL ||
		    XP_STRCHR(messageIdString,':') != NULL);
}




/*******************  TNavigatorImapConnection *******************************/


void TNavigatorImapConnection::StartProcessingActiveEntries()
{
    XP_Bool receivedNullEntry = FALSE;
    
    while (!receivedNullEntry && !DeathSignalReceived())
    {
        WaitForNextActiveEntry();
        
        if (!DeathSignalReceived())
        {
	            // while I'm processing this URL nobody can change it!
	        PR_EnterMonitor(fActiveEntryMonitor);   
	        if (fCurrentEntry)
	        {
	        	XP_Bool killedEarly = FALSE;
	        	// fCurrentUrl is accessed repeatedly by CurrentConnectionIsMove
	        	// CurrentConnectionIsMove is called from Mozilla thread
				LIBNET_LOCK();
				if (!DeathSignalReceived())
				{
		            delete fCurrentUrl;
		            fCurrentUrl = new TIMAPUrl(fCurrentEntry->URL_s->address, fCurrentEntry->URL_s->internal_url);
	           	}
	           	else
	           		killedEarly = TRUE;
				LIBNET_UNLOCK();
				
	            if (!fCurrentUrl && !killedEarly)
	                HandleMemoryFailure();
	            else if (!killedEarly)
	            {
	                // process the url
	                ProcessCurrentURL();
	                
	                // wait for the event queue to empty before we trash 
	                // fCurrentEntry.  Some of the events reference it.
	                WaitForEventQueueEmptySignal();
	            }
	                
	            // this URL is finished so delete it in case
	            // it is referenced before we create the new one
	            // the next time through this loop
				PR_EnterMonitor(GetDataMemberMonitor());
				delete fCurrentUrl;
				fCurrentUrl = NULL;
				PR_ExitMonitor(GetDataMemberMonitor());


	            fCurrentEntry = NULL;
	        }
	        else
	        {
	            receivedNullEntry = TRUE;
	            if (GetIOSocket() != 0)
	                Logout();
	        }
	        PR_ExitMonitor(fActiveEntryMonitor);  
        }  
    }



	PRIntervalTime sleepTime = PR_MicrosecondsToInterval(kImapSleepTime);
	PR_EnterMonitor(fPermissionToDieMonitor);
	while (!fIsSafeToDie)
		PR_Wait(fPermissionToDieMonitor, sleepTime);
	PR_ExitMonitor(fPermissionToDieMonitor);
}


XP_Bool TNavigatorImapConnection::BlockedWaitingForActiveEntry()
{
    return !PR_InMonitor(fActiveEntryMonitor);
}

char *TNavigatorImapConnection::GetCurrentConnectionURL()
{   
	PR_EnterMonitor(GetDataMemberMonitor());
    char *rv = fCurrentEntry->URL_s->address ?
		XP_STRDUP(fCurrentEntry->URL_s->address) : (char *)NULL;
	PR_ExitMonitor(GetDataMemberMonitor());
	return rv;
}


void TNavigatorImapConnection::WaitForEventQueueEmptySignal()
{
	// notice that this Wait function is not like the others in that
	// it does not wake up and check DeathSignalReceived()
	// sometimes it is run after DeathSignalReceived and the thread
	// has to drain its final fe events before death.
	PRIntervalTime sleepTime = kImapSleepTime;

    PR_EnterMonitor(fEventQueueEmptySignalMonitor);
    while(!fEventQueueEmptySignalHappened)
        PR_Wait(fEventQueueEmptySignalMonitor, sleepTime);
    fEventQueueEmptySignalHappened = FALSE;
    PR_ExitMonitor(fEventQueueEmptySignalMonitor);
}

void TNavigatorImapConnection::SignalEventQueueEmpty()
{
    PR_EnterMonitor(fEventQueueEmptySignalMonitor);
    fEventQueueEmptySignalHappened = TRUE;
    PR_Notify(fEventQueueEmptySignalMonitor);
    PR_ExitMonitor(fEventQueueEmptySignalMonitor);
}

void TNavigatorImapConnection::WaitForNextAppendMessageSize()
{
	PRIntervalTime sleepTime = kImapSleepTime;

    PR_EnterMonitor(fMsgCopyDataMonitor);
    while(!fMsgAppendSizeIsNew && !DeathSignalReceived())
        PR_Wait(fMsgCopyDataMonitor, sleepTime);
    PR_ExitMonitor(fMsgCopyDataMonitor);
}


void TNavigatorImapConnection::WaitForPotentialListOfMsgsToFetch(uint32 **msgIdList, uint32 &msgCount)
{
	PRIntervalTime sleepTime = kImapSleepTime;

    PR_EnterMonitor(fMsgCopyDataMonitor);
    while(!fFetchMsgListIsNew && !DeathSignalReceived())
        PR_Wait(fMsgCopyDataMonitor, sleepTime);
    fFetchMsgListIsNew = FALSE;

    *msgIdList = fFetchMsgIdList;
    msgCount   = fFetchCount;
    
    PR_ExitMonitor(fMsgCopyDataMonitor);
}


void TNavigatorImapConnection::NotifyKeyList(uint32 *keys, uint32 keyCount)
{
    PR_EnterMonitor(fMsgCopyDataMonitor);
    fFetchMsgIdList = keys;
    fFetchCount  	= keyCount;
    fFetchMsgListIsNew = TRUE;
    PR_Notify(fMsgCopyDataMonitor);
    PR_ExitMonitor(fMsgCopyDataMonitor);
}

void TNavigatorImapConnection::WaitForMessageUploadToComplete()
{
	PRIntervalTime sleepTime = kImapSleepTime;

    PR_EnterMonitor(fMessageUploadMonitor);
    while(!fMessageUploadCompleted && !DeathSignalReceived())
        PR_Wait(fMessageUploadMonitor, sleepTime);
    fMessageUploadCompleted = FALSE;
    PR_ExitMonitor(fMessageUploadMonitor);
}

void TNavigatorImapConnection::NotifyMessageUploadMonitor()
{
    PR_EnterMonitor(fMessageUploadMonitor);
    fMessageUploadCompleted = TRUE;
    PR_Notify(fMessageUploadMonitor);
    PR_ExitMonitor(fMessageUploadMonitor);
}

void TNavigatorImapConnection::NotifyAppendSize(uint32 msgSize, imapMessageFlagsType flags)
{
    PR_EnterMonitor(fMsgCopyDataMonitor);
    fAppendMessageSize = msgSize;
    fAppendMsgFlags = flags & ~kImapMsgRecentFlag;	  // don't let recent flag through, since we can't store it.
    fMsgAppendSizeIsNew = TRUE;
    PR_Notify(fMsgCopyDataMonitor);
    PR_ExitMonitor(fMsgCopyDataMonitor);
}

imapMessageFlagsType TNavigatorImapConnection::GetAppendFlags()
{
    imapMessageFlagsType returnValue;
    PR_EnterMonitor(fMsgCopyDataMonitor);
    returnValue = fAppendMsgFlags;
    PR_ExitMonitor(fMsgCopyDataMonitor);
    
    return returnValue;
}

uint32 TNavigatorImapConnection::GetAppendSize()
{
    uint32 returnValue;
    PR_EnterMonitor(fMsgCopyDataMonitor);
    returnValue = fAppendMessageSize;
    fMsgAppendSizeIsNew = FALSE;
    PR_ExitMonitor(fMsgCopyDataMonitor);
    
    return returnValue;
}

void TNavigatorImapConnection::SetFolderInfo(MSG_FolderInfo *folder, XP_Bool folderNeedsSubscribing, 
											 XP_Bool folderNeedsACLRefreshed)
{
	PR_EnterMonitor(GetDataMemberMonitor());
	fFolderInfo = folder;
	fFolderNeedsSubscribing = folderNeedsSubscribing;
	fFolderNeedsACLRefreshed = folderNeedsACLRefreshed;
	PR_ExitMonitor(GetDataMemberMonitor());
}

void TNavigatorImapConnection::WaitForNextActiveEntry()
{
	PRIntervalTime sleepTime = kImapSleepTime;

    PR_EnterMonitor(fActiveEntryMonitor);
    while(!fNextEntryEventSignalHappened && !DeathSignalReceived())
        PR_Wait(fActiveEntryMonitor, sleepTime);
    fNextEntryEventSignalHappened = FALSE;
    PR_ExitMonitor(fActiveEntryMonitor);
}

void TNavigatorImapConnection::SubmitActiveEntry(ActiveEntry * ce, XP_Bool newConnection)
{
    PR_EnterMonitor(fActiveEntryMonitor);
    fCurrentEntry = ce;
    if (newConnection)
    	StartIMAPConnection(this);
    else
    	SetConnectionStatus(0);	// keep netlib running, connection is reused
    fNextEntryEventSignalHappened = TRUE;
    PR_Notify(fActiveEntryMonitor);
    PR_ExitMonitor(fActiveEntryMonitor);    
}

XPPtrArray *TNavigatorImapConnection::connectionList;

TNavigatorImapConnection::TNavigatorImapConnection(const char *hostName) :
    TIMAP4BlockingConnection(),
    fCurrentEntry(nil), 
    fFEeventCompleted(FALSE),
	fTunnelCompleted(FALSE),
    fInputBufferSize(0),
    fInputSocketBuffer(nil),
    fBlockingThread(nil),
    fTCPConData(nil),
    fIOSocket(NULL),
    fCurrentUrl(nil),
    fEventQueueEmptySignalHappened(FALSE),
    fMessageUploadCompleted(FALSE),
    fHierarchyNameState(kNoOperationInProgress),
    fHierarchyMover(nil),
    fBytesMovedSoFarForProgress(0),
    fDeletableChildren(nil),
    fOnlineBaseFolderExists(FALSE),
    fFetchMsgIdList(NULL),
    fFetchCount(0),
    fFetchMsgListIsNew(FALSE),
    fMsgAppendSizeIsNew(FALSE),
    fAppendMessageSize(0),
    fNextEntryEventSignalHappened(FALSE),
    fThreadShouldDie(FALSE),
    fIsSafeToDie(FALSE),
    fProgressStringId(0),
    fProgressIndex(0),
	fNeedGreeting(FALSE),
	fNeedNoop(FALSE),
    fProgressCount(0),
    fOutputStream(nil),
	fPseudoInterrupted(FALSE),
	fAutoSubscribe(TRUE),
	fAutoUnsubscribe(TRUE),
	fShouldUpgradeToSubscription(FALSE),
	fUpgradeShouldLeaveAlone(FALSE),
	fUpgradeShouldSubscribeAll(FALSE),
	fFolderInfo(NULL),
	fFolderNeedsSubscribing(FALSE),
	fFolderNeedsACLRefreshed(FALSE)
{
    fEventCompletionMonitor                    = PR_NewNamedMonitor("event-completion-monitor");
    fActiveEntryMonitor                        = PR_NewNamedMonitor("active-entry-monitor");
    fEventQueueEmptySignalMonitor			   = PR_NewNamedMonitor("event-queue-empty-signal-monitor");
    fMessageUploadMonitor                      = PR_NewNamedMonitor("message-upload-monitor");
    fMsgCopyDataMonitor                        = PR_NewNamedMonitor("msg-copy-data-monitor");
    fThreadDeathMonitor                        = PR_NewNamedMonitor("thread-death-monitor");
    fPermissionToDieMonitor                    = PR_NewNamedMonitor("die-monitor");
	fPseudoInterruptMonitor					   = PR_NewNamedMonitor("imap-pseudo-interrupt-monitor");
	fWaitForBodyIdsMonitor					   = PR_NewNamedMonitor("wait-for-body-ids-monitor");
	fTunnelCompletionMonitor				   = PR_NewNamedMonitor("imap-io-tunnelling-monitor");
    
    fFEEventQueue = TImapFEEventQueue::CreateFEEventQueue();
	fSocketInfo = new TIMAPSocketInfo();
	fListedMailboxList = XP_ListNew();
	fHostName = XP_STRDUP(hostName);
	fFlagState = new TImapFlagAndUidState(kFlagEntrySize, FALSE);
	fMailToFetch = fGetHeaders = fNewMail = FALSE;
	fLastProgressStringId = 0;
	fLastPercent = -1;
	LL_I2L(fLastProgressTime, 0);
	ResetProgressInfo();

	fActive = fTrackingTime = FALSE;
	fStartTime = fEndTime = 0;
	fTooFastTime = 2;
	fIdealTime = 4;
	fChunkAddSize = 2048;
	fChunkStartSize = fChunkSize = 10240;
	fFetchByChunks = TRUE;
	fChunkThreshold = fChunkSize + (fChunkSize / 2);
	fMaxChunkSize = 40960;

	connectionList->Add(this);
}


void TNavigatorImapConnection::Configure(XP_Bool GetHeaders, int32 TooFastTime, int32 IdealTime,
									int32 ChunkAddSize, int32 ChunkSize, int32 ChunkThreshold,
									XP_Bool FetchByChunks, int32 MaxChunkSize)
{
	PR_EnterMonitor(fMsgCopyDataMonitor);
	fGetHeaders = GetHeaders;
	fTooFastTime = TooFastTime;		// secs we read too little too fast
	fIdealTime = IdealTime;		// secs we read enough in good time
	fChunkAddSize = ChunkAddSize;		// buffer size to add when wasting time
	fChunkSize = ChunkSize;
	fChunkThreshold = ChunkThreshold;
	fFetchByChunks = FetchByChunks;
	fMaxChunkSize = MaxChunkSize;
	PR_ExitMonitor(fMsgCopyDataMonitor);
}


TIMAPHostInfo::TIMAPHostInfo(const char *hostName)
{
	fHostName = XP_STRDUP(hostName);
	fNextHost = NULL;
	fCachedPassword = NULL;
	fCapabilityFlags = kCapabilityUndefined;
	fHierarchyDelimiters = NULL;
	fHaveWeEverDiscoveredFolders = FALSE;
	fCanonicalOnlineSubDir = NULL;
	fNamespaceList = TIMAPNamespaceList::CreateTIMAPNamespaceList();
	fUsingSubscription = TRUE;
	fOnlineTrashFolderExists = FALSE;
	fShouldAlwaysListInbox = TRUE;
	fShellCache = TIMAPBodyShellCache::Create();
	fPasswordVerifiedOnline = FALSE;
}

TIMAPHostInfo::~TIMAPHostInfo()
{
	FREEIF(fHostName);
	if (fCachedPassword)
		XP_FREE(fCachedPassword);
	FREEIF(fHierarchyDelimiters);
	delete fNamespaceList;
	delete fShellCache;
}

TIMAPHostInfo *TIMAPHostInfo::FindHost(const char *hostName)
{
	TIMAPHostInfo *host;

	for (host = fHostInfoList; host; host = host->fNextHost)
	{
		if (!XP_STRCASECMP(hostName, host->fHostName))
			return host;
	}
	return host;
}

// reset any cached connection info - delete the lot of 'em
void TIMAPHostInfo::ResetAll()
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	TIMAPHostInfo *nextHost = NULL;
	for (TIMAPHostInfo *host = fHostInfoList; host; host = nextHost)
	{
		nextHost = host->fNextHost;
		delete host;
	}
	fHostInfoList = NULL;
	PR_ExitMonitor(gCachedHostInfoMonitor);
}

/* static */ XP_Bool TIMAPHostInfo::AddHostToList(const char *hostName)
{
	TIMAPHostInfo *newHost=NULL;
	PR_EnterMonitor(gCachedHostInfoMonitor);
	if (!FindHost(hostName))
	{
		// stick it on the front
		newHost = new TIMAPHostInfo(hostName);
		if (newHost)
		{
			newHost->fNextHost = fHostInfoList;
			fHostInfoList = newHost;
		}
	}
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return newHost != NULL;
}

XP_Bool IMAP_HostHasACLCapability(const char *hostName)
{
	return (TIMAPHostInfo::GetCapabilityForHost(hostName) & kACLCapability);
}

uint32	IMAP_GetCapabilityForHost(const char *hostName)
{
	return TIMAPHostInfo::GetCapabilityForHost(hostName);
}

/* static */ uint32 TIMAPHostInfo::GetCapabilityForHost(const char *hostName)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);

	TIMAPHostInfo *host = FindHost(hostName);
	uint32 ret = (host) ? host->fCapabilityFlags : 0;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return ret;
}
/* static */ XP_Bool TIMAPHostInfo::SetCapabilityForHost(const char *hostName, uint32 capability)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	TIMAPHostInfo *host = FindHost(hostName);
	if (host)
		host->fCapabilityFlags = capability;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host != 0);
}

/* static */ char *TIMAPHostInfo::GetPasswordForHost(const char *hostName)
{
	char *ret = NULL;

	PR_EnterMonitor(gCachedHostInfoMonitor);
	TIMAPHostInfo *host = FindHost(hostName);
	if (host)
		ret = host->fCachedPassword;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return ret;
}

/* static */ XP_Bool TIMAPHostInfo::SetPasswordForHost(const char *hostName, const char *password)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	TIMAPHostInfo *host = FindHost(hostName);
	if (host)
	{
		FREEIF(host->fCachedPassword);
		if (password)
			host->fCachedPassword = XP_STRDUP(password);
	}
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host != 0);
}

/* static */ XP_Bool TIMAPHostInfo::SetPasswordVerifiedOnline(const char *hostName)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	TIMAPHostInfo *host = FindHost(hostName);
	if (host)
		host->fPasswordVerifiedOnline = TRUE;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host != 0);
}

/* static */ XP_Bool TIMAPHostInfo::GetPasswordVerifiedOnline(const char *hostName)
{
	XP_Bool ret = FALSE;

	PR_EnterMonitor(gCachedHostInfoMonitor);
	TIMAPHostInfo *host = FindHost(hostName);
	if (host)
		ret = host->fPasswordVerifiedOnline;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return ret;
}

/* static */ char *TIMAPHostInfo::GetHierarchyDelimiterStringForHost(const char *hostName)
{
	char *ret = NULL;

	PR_EnterMonitor(gCachedHostInfoMonitor);
	TIMAPHostInfo *host = FindHost(hostName);
	if (host)
		ret = host->fHierarchyDelimiters;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return ret;
}

/* static */ XP_Bool TIMAPHostInfo::AddHierarchyDelimiter(const char *hostName, char delimiter)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	TIMAPHostInfo *host = FindHost(hostName);
	if (host)
	{
		if (!host->fHierarchyDelimiters)
		{
			host->fHierarchyDelimiters = PR_smprintf("%c",delimiter);
		}
		else if (!XP_STRCHR(host->fHierarchyDelimiters, delimiter))
		{
			char *tmpDelimiters = PR_smprintf("%s%c",host->fHierarchyDelimiters,delimiter);
			FREEIF(host->fHierarchyDelimiters);
			host->fHierarchyDelimiters = tmpDelimiters;
		}
	}
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host != 0);
}

/* static */ XP_Bool TIMAPHostInfo::GetHostIsUsingSubscription(const char *hostName)
{
	XP_Bool ret = FALSE;

	PR_EnterMonitor(gCachedHostInfoMonitor);
	TIMAPHostInfo *host = FindHost(hostName);
	if (host)
		ret = host->fUsingSubscription;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return ret;
}

/* static */ XP_Bool TIMAPHostInfo::SetHostIsUsingSubscription(const char *hostName, XP_Bool usingSubscription)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	TIMAPHostInfo *host = FindHost(hostName);
	if (host)
		host->fUsingSubscription = usingSubscription;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host != 0);
}

/* static */ XP_Bool TIMAPHostInfo::GetHostHasAdminURL(const char *hostName)
{
	XP_Bool ret = FALSE;

	PR_EnterMonitor(gCachedHostInfoMonitor);
	TIMAPHostInfo *host = FindHost(hostName);
	if (host)
		ret = host->fHaveAdminURL;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return ret;
}

/* static */ XP_Bool TIMAPHostInfo::SetHostHasAdminURL(const char *hostName, XP_Bool haveAdminURL)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	TIMAPHostInfo *host = FindHost(hostName);
	if (host)
		host->fHaveAdminURL = haveAdminURL;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host != 0);
}


/* static */ XP_Bool TIMAPHostInfo::GetHaveWeEverDiscoveredFoldersForHost(const char *hostName)
{
	XP_Bool ret = FALSE;

	PR_EnterMonitor(gCachedHostInfoMonitor);
	TIMAPHostInfo *host = FindHost(hostName);
	if (host)
		ret = host->fHaveWeEverDiscoveredFolders;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return ret;
}

/* static */ XP_Bool TIMAPHostInfo::SetHaveWeEverDiscoveredFoldersForHost(const char *hostName, XP_Bool discovered)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	TIMAPHostInfo *host = FindHost(hostName);
	if (host)
		host->fHaveWeEverDiscoveredFolders = discovered;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host != 0);
}

/* static */ XP_Bool TIMAPHostInfo::SetOnlineTrashFolderExistsForHost(const char *hostName, XP_Bool exists)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	TIMAPHostInfo *host = FindHost(hostName);
	if (host)
		host->fOnlineTrashFolderExists = exists;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host != 0);
}

/* static */ XP_Bool TIMAPHostInfo::GetOnlineTrashFolderExistsForHost(const char *hostName)
{
	XP_Bool ret = FALSE;

	PR_EnterMonitor(gCachedHostInfoMonitor);
	TIMAPHostInfo *host = FindHost(hostName);
	if (host)
		ret = host->fOnlineTrashFolderExists;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return ret;
}

/* static */ XP_Bool TIMAPHostInfo::AddNewNamespaceForHost(const char *hostName, TIMAPNamespace *ns)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	TIMAPHostInfo *host = FindHost(hostName);
	if (host)
	{
		host->fNamespaceList->AddNewNamespace(ns);
	}
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host != 0);
}


/* static */ TIMAPNamespace *TIMAPHostInfo::GetNamespaceForMailboxForHost(const char *hostName, const char *mailbox_name)
{
	TIMAPNamespace *ret = 0;

	PR_EnterMonitor(gCachedHostInfoMonitor);
	TIMAPHostInfo *host = FindHost(hostName);
	if (host)
	{
		ret = host->fNamespaceList->GetNamespaceForMailbox(mailbox_name);
	}
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return ret;
}


/* static */ XP_Bool TIMAPHostInfo::ClearPrefsNamespacesForHost(const char *hostName)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	TIMAPHostInfo *host = FindHost(hostName);
	if (host)
	{
		host->fNamespaceList->ClearNamespaces(TRUE, FALSE);
	}
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host != 0);
}


/* static */ XP_Bool TIMAPHostInfo::ClearServerAdvertisedNamespacesForHost(const char *hostName)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	TIMAPHostInfo *host = FindHost(hostName);
	if (host)
	{
		host->fNamespaceList->ClearNamespaces(FALSE, TRUE);
	}
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host != 0);
}

/* static */ TIMAPNamespace *TIMAPHostInfo::GetDefaultNamespaceOfTypeForHost(const char *hostName, EIMAPNamespaceType type)
{
	TIMAPNamespace *ret = NULL;

	PR_EnterMonitor(gCachedHostInfoMonitor);
	TIMAPHostInfo *host = FindHost(hostName);
	if (host)
	{
		ret = host->fNamespaceList->GetDefaultNamespaceOfType(type);
	}
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return ret;
}

/* static */ XP_Bool TIMAPHostInfo::GetNamespacesOverridableForHost(const char *hostName)
{
	XP_Bool ret = FALSE;

	PR_EnterMonitor(gCachedHostInfoMonitor);
	TIMAPHostInfo *host = FindHost(hostName);
	if (host)
		ret = host->fNamespacesOverridable;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return ret;
}

/* static */ XP_Bool TIMAPHostInfo::SetNamespacesOverridableForHost(const char *hostName, XP_Bool overridable)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	TIMAPHostInfo *host = FindHost(hostName);
	if (host)
		host->fNamespacesOverridable = overridable;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host != 0);
}

/* static */ int TIMAPHostInfo::GetNumberOfNamespacesForHost(const char *hostName)
{
	int ret = 0;

	PR_EnterMonitor(gCachedHostInfoMonitor);
	TIMAPHostInfo *host = FindHost(hostName);
	if (host)
	{
		ret = host->fNamespaceList->GetNumberOfNamespaces();
	}
	PR_ExitMonitor(gCachedHostInfoMonitor);
	XP_ASSERT(ret > 0);
	return ret;
}

/* static */ TIMAPNamespace	*TIMAPHostInfo::GetNamespaceNumberForHost(const char *hostName, int n)
{
	TIMAPNamespace *ret = 0;

	PR_EnterMonitor(gCachedHostInfoMonitor);
	TIMAPHostInfo *host = FindHost(hostName);
	if (host)
	{
		ret = host->fNamespaceList->GetNamespaceNumber(n);
	}
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return ret;
}

// Make sure this is running in the Mozilla thread when called
/* static */ XP_Bool TIMAPHostInfo::CommitNamespacesForHost(const char *hostName, MSG_Master *master)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	TIMAPHostInfo *host = FindHost(hostName);
	if (host)
	{
		EIMAPNamespaceType type = kPersonalNamespace;
		for (int i = 1; i <= 3; i++)
		{
			switch(i)
			{
			case 1:
				type = kPersonalNamespace;
				break;
			case 2:
				type = kPublicNamespace;
				break;
			case 3:
				type = kOtherUsersNamespace;
				break;
			default:
				type = kPersonalNamespace;
				break;
			}
			
			int32 numInNS = host->fNamespaceList->GetNumberOfNamespaces(type);
			if (numInNS == 0)
			{
				MSG_SetNamespacePrefixes(master, host->fHostName, type, NULL);
			}
			else if (numInNS >= 1)
			{
				char *pref = PR_smprintf("");
				for (int count = 1; count <= numInNS; count++)
				{
					TIMAPNamespace *ns = host->fNamespaceList->GetNamespaceNumber(count, type);
					if (ns)
					{
						if (count > 1)
						{
							// append the comma
							char *tempPref = PR_smprintf("%s,",pref);
							FREEIF(pref);
							pref = tempPref;
						}
						char *tempPref = PR_smprintf("%s\"%s\"",pref,ns->GetPrefix());
						FREEIF(pref);
						pref = tempPref;
					}
				}
				if (pref)
				{
					MSG_SetNamespacePrefixes(master, host->fHostName, type, pref);
					XP_FREE(pref);
				}
			}
		}
	}
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host != 0);
}

// Caller must free the returned string
// Returns NULL if there is no personal namespace on the given host
/* static */ char *TIMAPHostInfo::GetOnlineInboxPathForHost(const char *hostName)
{
	char *ret = NULL;

	PR_EnterMonitor(gCachedHostInfoMonitor);
	TIMAPHostInfo *host = FindHost(hostName);
	if (host)
	{
		TIMAPNamespace *ns = NULL;
		ns = host->fNamespaceList->GetDefaultNamespaceOfType(kPersonalNamespace);
		if (ns)
		{
			ret = PR_smprintf("%sINBOX",ns->GetPrefix());
		}
	}
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return ret;
}

/* static */ XP_Bool TIMAPHostInfo::GetShouldAlwaysListInboxForHost(const char* /*hostName*/)
{
	XP_Bool ret = TRUE;

	/*
	PR_EnterMonitor(gCachedHostInfoMonitor);
	TIMAPHostInfo *host = FindHost(hostName);
	if (host)
		ret = host->fShouldAlwaysListInbox;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	*/
	return ret;
}

/* static */ XP_Bool TIMAPHostInfo::SetShouldAlwaysListInboxForHost(const char *hostName, XP_Bool shouldList)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	TIMAPHostInfo *host = FindHost(hostName);
	if (host)
		host->fShouldAlwaysListInbox = shouldList;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host != 0);
}

/* static */ XP_Bool TIMAPHostInfo::SetNamespaceHierarchyDelimiterFromMailboxForHost(const char *hostName, const char *boxName, char delimiter)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	TIMAPHostInfo *host = FindHost(hostName);
	if (host)
	{
		TIMAPNamespace *ns = host->fNamespaceList->GetNamespaceForMailbox(boxName);
		if (ns && !ns->GetIsDelimiterFilledIn())
		{
			ns->SetDelimiter(delimiter);
		}
	}
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host != 0);
}

/* static */ XP_Bool TIMAPHostInfo::AddShellToCacheForHost(const char *hostName, TIMAPBodyShell *shell)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	TIMAPHostInfo *host = FindHost(hostName);
	if (host)
	{
		if (host->fShellCache)
		{
			XP_Bool rv = host->fShellCache->AddShellToCache(shell);
			PR_ExitMonitor(gCachedHostInfoMonitor);
			return rv;
		}
		else
		{
			PR_ExitMonitor(gCachedHostInfoMonitor);
			return FALSE;
		}
	}
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host != 0);
}

/* static */ TIMAPBodyShell	*TIMAPHostInfo::FindShellInCacheForHost(const char *hostName, const char *mailboxName, const char *UID)
{
	TIMAPBodyShell *ret = NULL;

	PR_EnterMonitor(gCachedHostInfoMonitor);
	TIMAPHostInfo *host = FindHost(hostName);
	if (host)
	{
		if (host->fShellCache)
			ret = host->fShellCache->FindShellForUID(UID, mailboxName);
	}
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return ret;
}



extern "C" {

extern int IMAP_AddIMAPHost(const char *hostName, XP_Bool usingSubscription, XP_Bool overrideNamespaces,
					 const char *personalNamespacePrefixes, const char *publicNamespacePrefixes, 
					 const char *otherUsersNamespacePrefixes, XP_Bool haveAdminURL)
{
	TIMAPHostInfo::AddHostToList(hostName);
	TIMAPHostInfo::SetHostIsUsingSubscription(hostName,usingSubscription);
	TIMAPHostInfo::SetNamespacesOverridableForHost(hostName, overrideNamespaces);
	TIMAPHostInfo::SetHostHasAdminURL(hostName, haveAdminURL);

	if (personalNamespacePrefixes)
	{
		int numNamespaces = IMAP_UnserializeNamespaces(personalNamespacePrefixes, NULL, 0);
		char **prefixes = (char**) XP_CALLOC(numNamespaces, sizeof(char*));
		if (prefixes)
		{
			int len = IMAP_UnserializeNamespaces(personalNamespacePrefixes, prefixes, numNamespaces);
			for (int i = 0; i < len; i++)
			{
				char *thisns = prefixes[i];
				char delimiter = '/';	// a guess
				if (XP_STRLEN(thisns) >= 1)
					delimiter = thisns[XP_STRLEN(thisns)-1];
				TIMAPNamespace *ns = new TIMAPNamespace(kPersonalNamespace, thisns, delimiter, TRUE);
				if (ns)
					TIMAPHostInfo::AddNewNamespaceForHost(hostName, ns);
				FREEIF(thisns);
			}
		}
	}
	if (publicNamespacePrefixes)
	{
		int numNamespaces = IMAP_UnserializeNamespaces(publicNamespacePrefixes, NULL, 0);
		char **prefixes = (char**) XP_CALLOC(numNamespaces, sizeof(char*));
		if (prefixes)
		{
			int len = IMAP_UnserializeNamespaces(publicNamespacePrefixes, prefixes, numNamespaces);
			for (int i = 0; i < len; i++)
			{
				char *thisns = prefixes[i];
				char delimiter = '/';	// a guess
				if (XP_STRLEN(thisns) >= 1)
					delimiter = thisns[XP_STRLEN(thisns)-1];
				TIMAPNamespace *ns = new TIMAPNamespace(kPublicNamespace, thisns, delimiter, TRUE);
				if (ns)
					TIMAPHostInfo::AddNewNamespaceForHost(hostName, ns);
				FREEIF(thisns);
			}
		}
	}
	if (otherUsersNamespacePrefixes)
	{
		int numNamespaces = IMAP_UnserializeNamespaces(otherUsersNamespacePrefixes, NULL, 0);
		char **prefixes = (char**) XP_CALLOC(numNamespaces, sizeof(char*));
		if (prefixes)
		{
			int len = IMAP_UnserializeNamespaces(otherUsersNamespacePrefixes, prefixes, numNamespaces);
			for (int i = 0; i < len; i++)
			{
				char *thisns = prefixes[i];
				char delimiter = '/';	// a guess
				if (XP_STRLEN(thisns) >= 1)
					delimiter = thisns[XP_STRLEN(thisns)-1];
				TIMAPNamespace *ns = new TIMAPNamespace(kOtherUsersNamespace, thisns, delimiter, TRUE);
				if (ns)
					TIMAPHostInfo::AddNewNamespaceForHost(hostName, ns);
				FREEIF(thisns);
			}
		}
	}
	return 0;
}

extern void IMAP_SetShouldAlwaysListInboxForHost(const char *hostName, XP_Bool shouldList)
{
	TIMAPHostInfo::SetShouldAlwaysListInboxForHost(hostName, shouldList);
}

extern int IMAP_GetNumberOfNamespacesForHost(const char *hostName)
{
	return TIMAPHostInfo::GetNumberOfNamespacesForHost(hostName);
}

extern XP_Bool IMAP_SetHostIsUsingSubscription(const char *hostname, XP_Bool using_subscription)
{
	return TIMAPHostInfo::SetHostIsUsingSubscription(hostname, using_subscription);
}

#define SERIALIZER_SEPARATORS ","

/* prefixes is an array of strings;  len is the length of that array.
   If there is only one string, simply copy it and return it.
   Otherwise, put them in quotes and comma-delimit them. 
   Returns a newly allocated string. */
extern char *IMAP_SerializeNamespaces(char **prefixes, int len)
{
	char *rv = NULL;
	if (len <= 0)
		return rv;
	if (len == 1)
	{
		rv = XP_STRDUP(prefixes[0]);
		return rv;
	}
	else
	{
		for (int i = 0; i < len; i++)
		{
			char *temp = NULL;
			if (i == 0)
				temp = PR_smprintf("\"%s\"",prefixes[i]);	/* quote the string */
			else
				temp = PR_smprintf(",\"%s\"",prefixes[i]);	/* comma, then quote the string */

			char *next = PR_smprintf("%s%s",rv,temp);
			FREEIF(rv);
			rv = next;
		}
		return rv;
	}
}

/* str is the string which needs to be unserialized.
   If prefixes is NULL, simply returns the number of namespaces in str.  (len is ignored)
   If prefixes is not NULL, it should be an array of length len which is to be filled in
   with newly-allocated string.  Returns the number of strings filled in.
*/
extern int IMAP_UnserializeNamespaces(const char *str, char **prefixes, int len)
{
	if (!str)
		return 0;
	if (!prefixes)
	{
		if (str[0] != '"')
			return 1;
		else
		{
			int count = 0;
			char *ourstr = XP_STRDUP(str);
			if (ourstr)
			{
				char *token = XP_STRTOK( ourstr, SERIALIZER_SEPARATORS );
				while (token != NULL)
				{
					token = XP_STRTOK( NULL, SERIALIZER_SEPARATORS );
					count++;
				}
				XP_FREE(ourstr);
			}
			return count;
		}
	}
	else
	{
		if ((str[0] != '"') && (len >= 1))
		{
			prefixes[0] = XP_STRDUP(str);
			return 1;
		}
		else
		{
			int count = 0;
			char *ourstr = XP_STRDUP(str);
			if (ourstr)
			{
				char *token = XP_STRTOK( ourstr, SERIALIZER_SEPARATORS );
				while ((count < len) && (token != NULL))
				{
					char *current = XP_STRDUP(token), *where = current;
					if (where[0] == '"')
						where++;
					if (where[XP_STRLEN(where)-1] == '"')
						where[XP_STRLEN(where)-1] = 0;
					prefixes[count] = XP_STRDUP(where);
					FREEIF(current);
					token = XP_STRTOK( NULL, SERIALIZER_SEPARATORS );
					count++;
				}
				XP_FREE(ourstr);
			}
			return count;
		}
	}
}

}	// extern "C"

///////////////// TNavigatorImapConnection //////////////////////////////



//static 
void TNavigatorImapConnection::ImapStartup()
{
	if (!fFindingMailboxesMonitor)
        fFindingMailboxesMonitor = PR_NewNamedMonitor("finding-mailboxes-monitor");
	if (!fUpgradeToSubscriptionMonitor)
        fUpgradeToSubscriptionMonitor = PR_NewNamedMonitor("upgrade-to-imap-subscription-monitor");
	if (!TIMAPHostInfo::gCachedHostInfoMonitor)
		TIMAPHostInfo::gCachedHostInfoMonitor = PR_NewNamedMonitor("accessing-hostlist-monitor");
}

//static
void TNavigatorImapConnection::ImapShutDown()
{
	if (fFindingMailboxesMonitor)
	{
		PR_DestroyMonitor(fFindingMailboxesMonitor);
		fFindingMailboxesMonitor = NULL;
	}
	if (fUpgradeToSubscriptionMonitor)
	{
		PR_DestroyMonitor(fUpgradeToSubscriptionMonitor);
		fUpgradeToSubscriptionMonitor = NULL;
	}
	TIMAPHostInfo::ResetAll();
	if (TIMAPHostInfo::gCachedHostInfoMonitor)
	{
		PR_DestroyMonitor(TIMAPHostInfo::gCachedHostInfoMonitor);
		TIMAPHostInfo::gCachedHostInfoMonitor = NULL;
	}
}


TNavigatorImapConnection *
TNavigatorImapConnection::Create(const char *hostName)
{
    TNavigatorImapConnection *connection = new TNavigatorImapConnection(hostName);
    
    if (!connection ||
        !connection->ConstructionSuccess())
    {
        delete connection;
        connection = nil;
		return NULL;
    }
	connection->fServerState.SetCapabilityFlag(TIMAPHostInfo::GetCapabilityForHost(hostName));
	connection->fServerState.SetFlagState(connection->fFlagState);
    
    return connection;
}

XP_Bool TNavigatorImapConnection::ConstructionSuccess()
{
    return (TIMAP4BlockingConnection::ConstructionSuccess() &&
            fEventCompletionMonitor &&
            fFEEventQueue &&
            fActiveEntryMonitor &&
            fFindingMailboxesMonitor &&
			fUpgradeToSubscriptionMonitor &&
            fEventQueueEmptySignalMonitor &&
            fMessageUploadMonitor &&
            fMsgCopyDataMonitor &&
            fThreadDeathMonitor &&
			fPermissionToDieMonitor &&
			fPseudoInterruptMonitor &&
			fTunnelCompletionMonitor &&
			fSocketInfo &&
			fFlagState &&
			fListedMailboxList);
}

XP_Bool TNavigatorImapConnection::GetPseudoInterrupted()
{
	XP_Bool rv = FALSE;
	PR_EnterMonitor(fPseudoInterruptMonitor);
	rv = fPseudoInterrupted;
	PR_ExitMonitor(fPseudoInterruptMonitor);
	return rv;
}

void TNavigatorImapConnection::PseudoInterrupt(XP_Bool the_interrupt)
{
	PR_EnterMonitor(fPseudoInterruptMonitor);
	fPseudoInterrupted = the_interrupt;
	if (the_interrupt)
	{
		Log("CONTROL", NULL, "PSEUDO-Interrupted");
		//PR_LOG(IMAP, out, ("PSEUDO-Interrupted"));
	}
	PR_ExitMonitor(fPseudoInterruptMonitor);
}

void	TNavigatorImapConnection::SetActive(XP_Bool active)
{
	PR_EnterMonitor(GetDataMemberMonitor());
	fActive = active;
	PR_ExitMonitor(GetDataMemberMonitor());
}

XP_Bool	TNavigatorImapConnection::GetActive()
{
	XP_Bool ret;
	PR_EnterMonitor(GetDataMemberMonitor());
	ret = fActive;
	PR_ExitMonitor(GetDataMemberMonitor());
	return ret;
}

/* static */ XP_Bool	TNavigatorImapConnection::IsConnectionActive(const char *hostName, const char *folderName)
{
	XP_Bool ret = FALSE;
	if (!connectionList)
		connectionList = new XPPtrArray;

	// go through connection list looking for connection in selected state on folder
	for (int32 i = 0; i < connectionList->GetSize() && !ret; i++)
	{
		TNavigatorImapConnection *curConnection = (TNavigatorImapConnection *) connectionList->GetAt(i);
		// we should add semaphores for these variables (state and selectedmailboxname).
		PR_EnterMonitor(curConnection->GetDataMemberMonitor());
 		char *mailboxName = (curConnection->fCurrentUrl) ? curConnection->fCurrentUrl->CreateServerSourceFolderPathString() : 0;
 
  		if (curConnection->fActive && !XP_STRCMP(hostName, curConnection->fHostName)
 				&& folderName && mailboxName && !XP_STRCMP(folderName, mailboxName/* curConnection->GetServerStateParser().GetSelectedMailboxName() */))
  			ret = TRUE;
#ifdef DEBUG_bienvenu
		XP_Trace("connection active = %s state = %d selected mailbox name = %s\n", (curConnection->fActive) ? "TRUE" : "FALSE",
			curConnection->GetServerStateParser().GetIMAPstate(), (mailboxName) ? mailboxName : "NULL" /* curConnection->GetServerStateParser().GetSelectedMailboxName()*/);
#endif
 		FREEIF(mailboxName);
		PR_ExitMonitor(curConnection->GetDataMemberMonitor());
	}
	return ret;
}

// a thread safe accessor to fCurrentUrl
XP_Bool TNavigatorImapConnection::CurrentConnectionIsMove()
{
	XP_Bool isMove;
	PR_EnterMonitor(GetDataMemberMonitor());

    isMove =  (fCurrentUrl != NULL) && 
            ((fCurrentUrl->GetIMAPurlType() == TIMAPUrl::kOnlineCopy) ||
             (fCurrentUrl->GetIMAPurlType() == TIMAPUrl::kOnlineMove) ||
             (fCurrentUrl->GetIMAPurlType() == TIMAPUrl::kOnlineToOfflineCopy) ||
             (fCurrentUrl->GetIMAPurlType() == TIMAPUrl::kOnlineToOfflineMove) ||
             (fCurrentUrl->GetIMAPurlType() == TIMAPUrl::kOfflineToOnlineMove));
             
	PR_ExitMonitor(GetDataMemberMonitor());
	
	return isMove;
}




void TNavigatorImapConnection::SetSubscribeParameters(XP_Bool autoSubscribe,
													  XP_Bool autoUnsubscribe, XP_Bool autoSubscribeOnOpen, XP_Bool autoUnsubscribeFromNoSelect,
													  XP_Bool shouldUpgrade, XP_Bool upgradeLeaveAlone, XP_Bool upgradeSubscribeAll)
{
	PR_EnterMonitor(GetDataMemberMonitor());
	fAutoSubscribe = autoSubscribe;
	fAutoUnsubscribe = autoUnsubscribe;
	fAutoSubscribeOnOpen = autoSubscribeOnOpen;
	fAutoUnsubscribeFromNoSelect = autoUnsubscribeFromNoSelect;
	PR_ExitMonitor(GetDataMemberMonitor());

	PR_EnterMonitor(fUpgradeToSubscriptionMonitor);
	fShouldUpgradeToSubscription = shouldUpgrade;
	fUpgradeShouldLeaveAlone = upgradeLeaveAlone;
	fUpgradeShouldSubscribeAll = upgradeSubscribeAll;
	PR_ExitMonitor(fUpgradeToSubscriptionMonitor);
}


void TNavigatorImapConnection::TellThreadToDie(XP_Bool isSafeToDie)
{
	if (GetIOSocket() != 0 && (fCurrentEntry && fCurrentEntry->status !=
							   MK_WAITING_FOR_CONNECTION))
	{
		char *logoutString = PR_smprintf("xxxx logout\r\n");
		if (logoutString)
		{
			NET_BlockingWrite(GetIOSocket(), logoutString,
							  XP_STRLEN(logoutString));
			Log("NET","WR",logoutString);
			//PR_LOG(IMAP, out, ("%s",logoutString));
			Log("CONTROL","From TellThreadToDie","LOGOUT");
			//PR_LOG(IMAP, out, ("Logged out from TellThreadToDie"));
			XP_FREEIF(logoutString);
		}
	}

	PR_EnterMonitor(fThreadDeathMonitor);
	fThreadShouldDie = TRUE;
	PR_ExitMonitor(fThreadDeathMonitor);

	// signal all of the monitors that might be in PR_Wait
	// they will wake up and see fThreadShouldDie==TRUE;
	PR_EnterMonitor(fEventCompletionMonitor);
	PR_Notify(fEventCompletionMonitor);
	PR_ExitMonitor(fEventCompletionMonitor);

	PR_EnterMonitor(fTunnelCompletionMonitor);
	PR_Notify(fTunnelCompletionMonitor);
	PR_ExitMonitor(fTunnelCompletionMonitor);

	PR_EnterMonitor(fMessageUploadMonitor);
	PR_Notify(fMessageUploadMonitor);
	PR_ExitMonitor(fMessageUploadMonitor);
	
	PR_EnterMonitor(fMsgCopyDataMonitor);
	PR_Notify(fMsgCopyDataMonitor);
	PR_ExitMonitor(fMsgCopyDataMonitor);
	
	PR_EnterMonitor(fIOMonitor);
	PR_Notify(fIOMonitor);
	PR_ExitMonitor(fIOMonitor);

	if (isSafeToDie)
		SetIsSafeToDie();
}

XP_Bool TNavigatorImapConnection::DeathSignalReceived()
{
	XP_Bool returnValue;
	PR_EnterMonitor(fThreadDeathMonitor);
	returnValue = fThreadShouldDie;
	PR_ExitMonitor(fThreadDeathMonitor);
	
	return returnValue;
}



TNavigatorImapConnection::~TNavigatorImapConnection()
{
    // This is either being deleted as the last thing that the
    // imap thread does or during a url interrupt, in which case
    // the thread was already killed, so don't destroy the thread
    // here
        
    if (GetIOSocket() != 0)
	{
		// Logout(); *** We still need to close down the socket
		net_graceful_shutdown(GetIOSocket(), HG32830);
		PR_Close(GetIOSocket());
		SetIOSocket(NULL);    // delete connection
		if (GetActiveEntry())
		  GetActiveEntry()->socket = 0;
	}
	GetServerStateParser().SetFlagState(NULL);

    if (fEventCompletionMonitor)
        PR_DestroyMonitor(fEventCompletionMonitor);

    if (fTunnelCompletionMonitor)
		PR_DestroyMonitor(fTunnelCompletionMonitor);

    if (fActiveEntryMonitor)
        PR_DestroyMonitor(fActiveEntryMonitor);
    
    if (fEventQueueEmptySignalMonitor)
        PR_DestroyMonitor(fEventQueueEmptySignalMonitor);
    
    if (fMessageUploadMonitor)
        PR_DestroyMonitor(fMessageUploadMonitor);
        
    if (fMsgCopyDataMonitor)
        PR_DestroyMonitor(fMsgCopyDataMonitor);
        
    if (fThreadDeathMonitor)
        PR_DestroyMonitor(fThreadDeathMonitor);

	if (fPermissionToDieMonitor)
		PR_DestroyMonitor(fPermissionToDieMonitor);

	if (fPseudoInterruptMonitor)
		PR_DestroyMonitor(fPseudoInterruptMonitor);
    
	if (fWaitForBodyIdsMonitor)
		PR_DestroyMonitor(fWaitForBodyIdsMonitor);
    if (fFEEventQueue)
        delete fFEEventQueue;
    
    if (fHierarchyMover)
        delete fHierarchyMover;
    
    if (fCurrentUrl)
    	delete fCurrentUrl;

	if (fSocketInfo)
		delete (fSocketInfo);

	if (fFlagState)
		delete fFlagState;

	while ((TIMAPMailboxInfo *) XP_ListRemoveTopObject(fListedMailboxList))
	{
	}
	XP_ListDestroy(fListedMailboxList);
	preAuthSucceeded = FALSE;
 	FREEIF(fInputSocketBuffer);
	FREEIF(fHostName);
	connectionList->Remove(this);
}


TNavigatorImapConnection *TNavigatorImapConnection::GetNavigatorConnection()
{
    return this;
}


void TNavigatorImapConnection::HandleMemoryFailure()
{
    PR_EnterMonitor(GetDataMemberMonitor());
    AlertUserEvent(XP_GetString(MK_OUT_OF_MEMORY));
    SetConnectionStatus(-1);        // stop this midnight train to crashville
    PR_ExitMonitor(GetDataMemberMonitor());
}

void TNavigatorImapConnection::SetMailboxDiscoveryStatus(EMailboxDiscoverStatus status)
{
    PR_EnterMonitor(GetDataMemberMonitor());
	fDiscoveryStatus = status;
    PR_ExitMonitor(GetDataMemberMonitor());
}

EMailboxDiscoverStatus TNavigatorImapConnection::GetMailboxDiscoveryStatus( )
{
	EMailboxDiscoverStatus returnStatus;
    PR_EnterMonitor(GetDataMemberMonitor());
	returnStatus = fDiscoveryStatus;
    PR_ExitMonitor(GetDataMemberMonitor());
    
    return returnStatus;
}


void TNavigatorImapConnection::WaitForFEEventCompletion()
{
	PRIntervalTime sleepTime = kImapSleepTime;

    PR_EnterMonitor(fEventCompletionMonitor);
//#ifdef KMCENTEE_DEBUG 
//	WaitForFEEventLoopCount=0;
//#endif

    while (!fFEeventCompleted && !DeathSignalReceived())
    {
        PR_Wait(fEventCompletionMonitor, sleepTime); 
//#ifdef KMCENTEE_DEBUG 
//	WaitForFEEventLoopCount++;
//	if (WaitForFEEventLoopCount == 10)
//		XP_ASSERT(FALSE);
//#endif
    }
    fFEeventCompleted = FALSE;                   
    PR_ExitMonitor(fEventCompletionMonitor);
}

void TNavigatorImapConnection::WaitForTunnelCompletion()
{
	PRIntervalTime sleepTime = kImapSleepTime;

    PR_EnterMonitor(fTunnelCompletionMonitor);

    while (!fTunnelCompleted && !DeathSignalReceived())
    {
        PR_Wait(fTunnelCompletionMonitor, sleepTime); 
    }
    fTunnelCompleted = FALSE;
    PR_ExitMonitor(fTunnelCompletionMonitor);
}


void TNavigatorImapConnection::NotifyEventCompletionMonitor()
{
    PR_EnterMonitor(fEventCompletionMonitor);
    fFEeventCompleted = TRUE;
    PR_Notify(fEventCompletionMonitor);                     
    PR_ExitMonitor(fEventCompletionMonitor);
}

void TNavigatorImapConnection::NotifyTunnelCompletionMonitor()
{
    PR_EnterMonitor(fTunnelCompletionMonitor);
	fTunnelCompleted = TRUE;
    PR_Notify(fTunnelCompletionMonitor);
    PR_ExitMonitor(fTunnelCompletionMonitor);
}

TImapFEEventQueue &TNavigatorImapConnection::GetFEEventQueue()
{
    return *fFEEventQueue;
}


void TNavigatorImapConnection::BeginMessageDownLoad(
                                      uint32 total_message_size, // for user, headers and body
                                      const char *content_type)
{
	char *sizeString = PR_smprintf("OPEN Size: %ld", total_message_size);
	Log("STREAM",sizeString,"Begin Message Download Stream");
	FREEIF(sizeString);
	//PR_LOG(IMAP, out, ("STREAM: Begin Message Download Stream.  Size: %ld", total_message_size));
	StreamInfo *si = (StreamInfo *) XP_ALLOC (sizeof (StreamInfo));		// This will be freed in the event
	if (si)
	{
		si->size = total_message_size;
		si->content_type = XP_STRDUP(content_type);
		if (si->content_type)
		{
			TImapFEEvent *setupStreamEvent = 
				new TImapFEEvent(SetupMsgWriteStream,   // function to call
								this,                                   // access to current entry
								(void *) si,
								TRUE);		// ok to run when interrupted because si is FREE'd in the event

			if (setupStreamEvent)
			{
				fFEEventQueue->AdoptEventToEnd(setupStreamEvent);
				WaitForFEEventCompletion();
			}
			else
				HandleMemoryFailure();
			fFromHeaderSeen = FALSE;
		}
		else
			HandleMemoryFailure();
	}
	else
		HandleMemoryFailure();
}


void TNavigatorImapConnection::AddXMozillaStatusLine(uint16 /* flags */) // flags not use now
{
	static char statusLine[] = "X-Mozilla-Status: 0201\r\n";
	HandleMessageDownLoadLine(statusLine, FALSE);
}


void TNavigatorImapConnection::HandleMessageDownLoadLine(const char *line, XP_Bool chunkEnd)
{
    // when we duplicate this line, whack it into the native line
    // termination.  Do not assume that it really ends in CRLF
    // to start with, even though it is supposed to be RFC822

	// If we are fetching by chunks, we can make no assumptions about
	// the end-of-line terminator, and we shouldn't mess with it.
    
    // leave enough room for two more chars. (CR and LF)
    char *localMessageLine = (char *) XP_ALLOC(strlen(line) + 3);
    if (localMessageLine)
        strcpy(localMessageLine,line);
    char *endOfLine = localMessageLine + strlen(localMessageLine);

	if (!chunkEnd)
	{
#if (LINEBREAK_LEN == 1)
		if ((endOfLine - localMessageLine) >= 2 &&
			 endOfLine[-2] == CR &&
			 endOfLine[-1] == LF)
			{
			  /* CRLF -> CR or LF */
			  endOfLine[-2] = LINEBREAK[0];
			  endOfLine[-1] = '\0';
			}
		  else if (endOfLine > localMessageLine + 1 &&
				   endOfLine[-1] != LINEBREAK[0] &&
			   ((endOfLine[-1] == CR) || (endOfLine[-1] == LF)))
			{
			  /* CR -> LF or LF -> CR */
			  endOfLine[-1] = LINEBREAK[0];
			}
		else // no eol characters at all
		  {
		    endOfLine[0] = LINEBREAK[0]; // CR or LF
		    endOfLine[1] = '\0';
		  }
#else
		  if (((endOfLine - localMessageLine) >= 2 && endOfLine[-2] != CR) ||
			   ((endOfLine - localMessageLine) >= 1 && endOfLine[-1] != LF))
			{
			  if ((endOfLine[-1] == CR) || (endOfLine[-1] == LF))
			  {
				  /* LF -> CRLF or CR -> CRLF */
				  endOfLine[-1] = LINEBREAK[0];
				  endOfLine[0]  = LINEBREAK[1];
				  endOfLine[1]  = '\0';
			  }
			  else	// no eol characters at all
			  {
				  endOfLine[0] = LINEBREAK[0];	// CR
				  endOfLine[1] = LINEBREAK[1];	// LF
				  endOfLine[2] = '\0';
			  }
			}
#endif
	}
    
	const char *xSenderInfo = GetServerStateParser().GetXSenderInfo();

	if (xSenderInfo && *xSenderInfo && !fFromHeaderSeen)
	{
		if (!XP_STRNCMP("From: ", localMessageLine, 6))
		{
			fFromHeaderSeen = TRUE;
			if (XP_STRSTR(localMessageLine, xSenderInfo) != NULL)
				AddXMozillaStatusLine(0);
			GetServerStateParser().FreeXSenderInfo();
		}
	}
    // if this line is for a different message, or the incoming line is too big
    if (((fDownLoadLineCache.CurrentUID() != GetServerStateParser().CurrentResponseUID()) && !fDownLoadLineCache.CacheEmpty()) ||
        (fDownLoadLineCache.SpaceAvailable() < (strlen(localMessageLine) + 1)) )
    {
		if (!fDownLoadLineCache.CacheEmpty())
		{
			msg_line_info *downloadLineDontDelete = fDownLoadLineCache.GetCurrentLineInfo();
			PostLineDownLoadEvent(downloadLineDontDelete);
		}
		fDownLoadLineCache.ResetCache();
	}
     
    // so now the cache is flushed, but this string might still be to big
    if (fDownLoadLineCache.SpaceAvailable() < (strlen(localMessageLine) + 1) )
    {
        // has to be dynamic to pass to other win16 thread
		msg_line_info *downLoadInfo = (msg_line_info *) XP_ALLOC(sizeof(msg_line_info));
        if (downLoadInfo)
        {
          	downLoadInfo->adoptedMessageLine = localMessageLine;
          	downLoadInfo->uidOfMessage = GetServerStateParser().CurrentResponseUID();
          	PostLineDownLoadEvent(downLoadInfo);
          	if (!DeathSignalReceived())
          		XP_FREE(downLoadInfo);
          	else
          	{
          		// this is very rare, interrupt while waiting to display a huge single line
          		// Net_InterruptIMAP will read this line so leak the downLoadInfo
          		
          		// set localMessageLine to NULL so the FREEIF( localMessageLine) leaks also
          		localMessageLine = NULL;
          	}
        }
	}
    else
		fDownLoadLineCache.CacheLine(localMessageLine, GetServerStateParser().CurrentResponseUID());
	FREEIF( localMessageLine);
}

char* TNavigatorImapConnection::CreateUtf7ConvertedString(const char *sourceString, XP_Bool toUtf7Imap)
{
	char *convertedString = NULL;
	utf_name_struct *conversion = (utf_name_struct *) XP_ALLOC(sizeof(utf_name_struct));
	if (conversion)
	{
		conversion->toUtf7Imap = toUtf7Imap;
		conversion->sourceString = (unsigned char *) XP_STRDUP(sourceString);
		conversion->convertedString = NULL;
	}
	
    TImapFEEvent *convertEvent = 
        new TImapFEEvent(ConvertImapUtf7, // function to call
                        conversion,                             
                        this,
						TRUE);

    if (convertEvent && sourceString && conversion)
    {
        fFEEventQueue->AdoptEventToEnd(convertEvent);
        WaitForFEEventCompletion();
        convertedString = (char *) conversion->convertedString;
    }
    
    if (!DeathSignalReceived())
	{
		FREEIF(conversion->sourceString);
    	FREEIF(conversion);	// leak these 8 bytes if we were interrupted here.
	}
    
    return convertedString;
}


void TNavigatorImapConnection::PostLineDownLoadEvent(msg_line_info *downloadLineDontDelete)
{
    TImapFEEvent *endEvent = 
        new TImapFEEvent(ParseAdoptedMsgLine, // function to call
                        this,                             // access to current entry
                        (void *) downloadLineDontDelete,
						FALSE);         // line/msg info

    if (endEvent && downloadLineDontDelete && downloadLineDontDelete->adoptedMessageLine)
    {
        fFEEventQueue->AdoptEventToEnd(endEvent);
        
        // I just put a big buffer in the event queue that I need to reuse, so wait.
        // The alternative would be to realloc the buffer each time it gets placed in the
        // queue, but I don't want the perf hit a continuously doing an alloc/delete
        // of a big buffer.  Also, I don't a queue full of huge buffers laying around!
        WaitForFEEventCompletion();
        
        // we better yield here, dealing with a huge buffer made for a slow fe event.
        // because this event was processed, the queue is empty.  Yielding here will
        // cause NET_ProcessIMAP4 to exit.
        if (!DeathSignalReceived())
        	PR_Sleep(PR_INTERVAL_NO_WAIT);
    }
    else
        HandleMemoryFailure();
}


// Make sure that we receive data chunks in a size that allows the user to see some
// progress. Dont want to bore them just yet.
// If we received our chunk too fast, ask for more
// if we received our chunk too slow, ask for less, etc

void TNavigatorImapConnection::AdjustChunkSize()
{
	fEndTime = XP_TIME();
	fTrackingTime = FALSE;
	time_t t = fEndTime - fStartTime;
	if (t < 0)
		return;							// bogus for some reason
	if (t <= fTooFastTime) {
		fChunkSize += fChunkAddSize;
		fChunkThreshold = fChunkSize + (fChunkSize / 2);
		if (fChunkSize > fMaxChunkSize)
			fChunkSize = fMaxChunkSize;
	}
	else if (t <= fIdealTime)
		return;
	else {
		if (fChunkSize > fChunkStartSize)
			fChunkSize = fChunkStartSize;
		else if (fChunkSize > (fChunkAddSize * 2))
			fChunkSize -= fChunkAddSize;
		fChunkThreshold = fChunkSize + (fChunkSize / 2);
	}
}



void TNavigatorImapConnection::NormalMessageEndDownload()
{
	Log("STREAM", "CLOSE", "Normal Message End Download Stream");
	//PR_LOG(IMAP, out, ("STREAM: Normal End Message Download Stream"));
	if (fTrackingTime)
		AdjustChunkSize();
	if (!fDownLoadLineCache.CacheEmpty())
	{
	    msg_line_info *downloadLineDontDelete = fDownLoadLineCache.GetCurrentLineInfo();
	    PostLineDownLoadEvent(downloadLineDontDelete);
	    fDownLoadLineCache.ResetCache();
    }

    TImapFEEvent *endEvent = 
        new TImapFEEvent(NormalEndMsgWriteStream,	// function to call
                        this,						// access to current entry
                        nil,						// unused
						TRUE);

    if (endEvent)
        fFEEventQueue->AdoptEventToEnd(endEvent);
    else
        HandleMemoryFailure();
}

void TNavigatorImapConnection::AbortMessageDownLoad()
{
	Log("STREAM", "CLOSE", "Abort Message  Download Stream");
	//PR_LOG(IMAP, out, ("STREAM: Abort Message Download Stream"));
	if (fTrackingTime)
		AdjustChunkSize();
	if (!fDownLoadLineCache.CacheEmpty())
	{
	    msg_line_info *downloadLineDontDelete = fDownLoadLineCache.GetCurrentLineInfo();
	    PostLineDownLoadEvent(downloadLineDontDelete);
	    fDownLoadLineCache.ResetCache();
    }

    TImapFEEvent *endEvent = 
        new TImapFEEvent(AbortMsgWriteStream,	// function to call
                        this,					// access to current entry
                        nil,					// unused
						TRUE);

    if (endEvent)
        fFEEventQueue->AdoptEventToEnd(endEvent);
    else
        HandleMemoryFailure();
	//MSG_FolderSetGettingMail(fFolderInfo, FALSE);
}

char *TNavigatorImapConnection::CreatePossibleTrashName(const char *prefix)
{
	IMAP_LoadTrashFolderName();

	char *returnTrash = (char *) XP_ALLOC(XP_STRLEN(prefix) + XP_STRLEN(ImapTRASH_FOLDER_NAME) + 1);
	if (returnTrash)
	{
		XP_STRCPY(returnTrash, prefix);
		XP_STRCAT(returnTrash, ImapTRASH_FOLDER_NAME);
	}
	return returnTrash;
}



void TNavigatorImapConnection::CanonicalChildList(const char *canonicalPrefix, XP_Bool pipeLined)
{
	char *truncatedPrefix = XP_STRDUP(canonicalPrefix);
	if (truncatedPrefix)
	{
		if (*(truncatedPrefix + XP_STRLEN(truncatedPrefix) - 1) == '/')
			*(truncatedPrefix + XP_STRLEN(truncatedPrefix) - 1) = '\0';
		
		char *serverPrefix = fCurrentUrl->AllocateServerPath(truncatedPrefix);
		if (serverPrefix)
		{
			char *utf7ListArg = CreateUtf7ConvertedString(serverPrefix,TRUE);
			if (utf7ListArg)
			{
				char *pattern = PR_smprintf("%s%c%c",utf7ListArg, fCurrentUrl->GetOnlineSubDirSeparator(),'%');
				if (pattern)
				{
					List(pattern,pipeLined);
					XP_FREE(pattern);
				}
				XP_FREE(utf7ListArg);
			}
			XP_FREE(serverPrefix);
		}
		XP_FREE(truncatedPrefix);
	}
}

void TNavigatorImapConnection::NthLevelChildList(const char *canonicalPrefix, int depth)
{
	XP_ASSERT(depth >= 0);
	if (depth < 0) return;
	char *truncatedPrefix = XP_STRDUP(canonicalPrefix);
	if (truncatedPrefix)
	{
		if (*(truncatedPrefix + XP_STRLEN(truncatedPrefix) - 1) == '/')
			*(truncatedPrefix + XP_STRLEN(truncatedPrefix) - 1) = '\0';
		
		char *serverPrefix = fCurrentUrl->AllocateServerPath(truncatedPrefix);
		if (serverPrefix)
		{
			char *utf7ListArg = CreateUtf7ConvertedString(serverPrefix,TRUE);
			if (utf7ListArg)
			{
				char *pattern = PR_smprintf("%s",utf7ListArg);
				int count = 0;
				char *suffix = PR_smprintf("%c%c",fCurrentUrl->GetOnlineSubDirSeparator(),'%');
				if (suffix)
				{
					while (pattern && (count < depth))
					{
						StrAllocCat(pattern, suffix);
						count++;
					}
					if (pattern)
					{
						List(pattern);
					}
					XP_FREE(suffix);
				}
				XP_FREEIF(pattern);
				XP_FREE(utf7ListArg);
			}
			XP_FREE(serverPrefix);
		}
		XP_FREE(truncatedPrefix);
	}
}

static
void MOZTHREAD_ChildDiscoverySucceeded(void *blockingConnectionVoid,
                      void* /*unused*/)
{
    TNavigatorImapConnection *imapConnection = 
        (TNavigatorImapConnection *) blockingConnectionVoid;

    ReportSuccessOfChildMailboxDiscovery(imapConnection->GetActiveEntry()->window_id);
}


void TNavigatorImapConnection::ChildDiscoverySucceeded()
{
    TImapFEEvent *succeedEvent = 
        new TImapFEEvent(MOZTHREAD_ChildDiscoverySucceeded,              // function to call
                        this,                                   // access to current entry/context
                        NULL,
						TRUE);
                                                        

    if (succeedEvent)
        fFEEventQueue->AdoptEventToEnd(succeedEvent);
    else
        HandleMemoryFailure();
}

void TNavigatorImapConnection::DiscoverMailboxSpec(mailbox_spec *adoptedBoxSpec)
{
	IMAP_LoadTrashFolderName();

	TIMAPNamespace *ns = TIMAPHostInfo::GetDefaultNamespaceOfTypeForHost(fCurrentUrl->GetUrlHost(), kPersonalNamespace);
	const char *hostDir = ns ? ns->GetPrefix() : 0;
	// if (!hostDir)	// no personal or default namespaces - only public? or none altogether?
	char *canonicalSubDir = NULL;
	if (hostDir)
	{
		canonicalSubDir = XP_STRDUP(hostDir);
		if (canonicalSubDir && *canonicalSubDir && (*(canonicalSubDir + XP_STRLEN(canonicalSubDir) - 1) == '/'))
			*(canonicalSubDir + XP_STRLEN(canonicalSubDir) - 1) = 0;
	}
		
    switch (fHierarchyNameState)
	{
	case kNoOperationInProgress:
	case kDiscoverTrashFolderInProgress:
	case kListingForInfoAndDiscovery:
		{
            if (canonicalSubDir && XP_STRSTR(adoptedBoxSpec->allocatedPathName, canonicalSubDir))
                fOnlineBaseFolderExists = TRUE;

            if (ns && hostDir)	// if no personal namespace, there can be no Trash folder
			{
				if (!TIMAPHostInfo::GetOnlineTrashFolderExistsForHost(fCurrentUrl->GetUrlHost()) && XP_STRSTR(adoptedBoxSpec->allocatedPathName, ImapTRASH_FOLDER_NAME))
				{
					XP_Bool trashExists = FALSE;
					char *trashMatch = CreatePossibleTrashName(hostDir);
					if (trashMatch)
					{
						trashExists = XP_STRCMP(trashMatch, adoptedBoxSpec->allocatedPathName) == 0;
						TIMAPHostInfo::SetOnlineTrashFolderExistsForHost(fCurrentUrl->GetUrlHost(), trashExists);
	                	FREEIF(trashMatch);

	                	// special case check for cmu trash, child of inbox
						if (!TIMAPHostInfo::GetOnlineTrashFolderExistsForHost(fCurrentUrl->GetUrlHost()) && (ns->GetDelimiter() == '.'))
	                	{
							//char *inboxPath = TIMAPHostInfo::GetOnlineInboxPathForHost(fCurrentUrl->GetUrlHost());
							char *inboxPath = PR_smprintf("INBOX");
							if (inboxPath)
							{
								char *inboxAsParent = PR_smprintf("%s/",inboxPath);
	                			if (inboxAsParent)
								{
									trashMatch = CreatePossibleTrashName(inboxAsParent);	// "INBOX/"
									if (trashMatch)
									{
			                			trashExists = XP_STRCMP(trashMatch, adoptedBoxSpec->allocatedPathName) == 0;
										TIMAPHostInfo::SetOnlineTrashFolderExistsForHost(fCurrentUrl->GetUrlHost(), trashExists);
			                			FREEIF(trashMatch);
									}
									XP_FREE(inboxAsParent);
								}
								XP_FREE(inboxPath);
							}
						}
					}
					
					if (trashExists)
	                	adoptedBoxSpec->box_flags |= kImapTrash;
				}
			}

			// Discover the folder (shuttle over to libmsg, yay)
			// Do this only if the folder name is not empty (i.e. the root)
			if (*adoptedBoxSpec->allocatedPathName)
			{
				char *boxNameCopy = XP_STRDUP(adoptedBoxSpec->allocatedPathName);

				TImapFEEvent *endEvent =
					new TImapFEEvent(PossibleIMAPmailbox, // function to call
									this,                 // access to current entry/context
									(void *) adoptedBoxSpec,
									TRUE);
														// now owned by msg folder

				if (endEvent)
				{
					char *listArg = XP_STRDUP(adoptedBoxSpec->allocatedPathName);
					fFEEventQueue->AdoptEventToEnd(endEvent);
					WaitForFEEventCompletion();
                
					if ((GetMailboxDiscoveryStatus() != eContinue) && 
						(GetMailboxDiscoveryStatus() != eContinueNew) &&
						(GetMailboxDiscoveryStatus() != eListMyChildren))
					{
                    	SetConnectionStatus(-1);
					}
					else if (listArg && (GetMailboxDiscoveryStatus() == eListMyChildren) &&
						(!TIMAPHostInfo::GetHostIsUsingSubscription(fCurrentUrl->GetUrlHost()) || GetSubscribingNow()))
					{
						XP_ASSERT(FALSE);	// we should never get here anymore.
                    	SetMailboxDiscoveryStatus(eContinue);
                    	if (listArg)
                    		CanonicalChildList(listArg,TRUE);
					}
					else if (GetMailboxDiscoveryStatus() == eContinueNew)
					{
						if (fHierarchyNameState == kListingForInfoAndDiscovery && boxNameCopy)
						{
							// remember the info here also
							TIMAPMailboxInfo *mb = new TIMAPMailboxInfo(boxNameCopy);
							XP_ListAddObject(fListedMailboxList, mb);
						}
						SetMailboxDiscoveryStatus(eContinue);
					}
					FREEIF(listArg);
				}
				else
					HandleMemoryFailure();

				FREEIF(boxNameCopy);
			}
        }
        break;
    case kDiscoverBaseFolderInProgress:
        {
            if (canonicalSubDir && XP_STRSTR(adoptedBoxSpec->allocatedPathName, canonicalSubDir))
                fOnlineBaseFolderExists = TRUE;
        }
        break;
    case kDeleteSubFoldersInProgress:
        {
            XP_ListAddObject(fDeletableChildren, adoptedBoxSpec->allocatedPathName);
			delete adoptedBoxSpec->flagState;
            FREEIF( adoptedBoxSpec);
        }
        break;
	case kListingForInfoOnly:
		{
			//UpdateProgressWindowForUpgrade(adoptedBoxSpec->allocatedPathName);
			ProgressEventFunction_UsingIdWithString(MK_MSG_IMAP_DISCOVERING_MAILBOX, adoptedBoxSpec->allocatedPathName);
			TIMAPMailboxInfo *mb = new TIMAPMailboxInfo(adoptedBoxSpec->allocatedPathName);
			XP_ListAddObject(fListedMailboxList, mb);
			IMAP_FreeBoxSpec(adoptedBoxSpec);
		}
		break;
	case kDiscoveringNamespacesOnly:
		{
			IMAP_FreeBoxSpec(adoptedBoxSpec);
		}
		break;
    default:
        XP_ASSERT(FALSE);
        break;
	}
	FREEIF(canonicalSubDir);
}

void TNavigatorImapConnection::OnlineCopyCompleted(ImapOnlineCopyState copyState)
{
    ImapOnlineCopyState *orphanedCopyState = (ImapOnlineCopyState *) XP_ALLOC(sizeof(ImapOnlineCopyState));
    if (orphanedCopyState)
        *orphanedCopyState = copyState;

    TImapFEEvent *endEvent = 
        new TImapFEEvent(OnlineCopyReport,              // function to call
                        this,                                   // access to current entry/context
                        (void *) orphanedCopyState,
						TRUE);    // storage passed
                                                        

    if (endEvent && orphanedCopyState)
        fFEEventQueue->AdoptEventToEnd(endEvent);
    else
        HandleMemoryFailure();
}

void TNavigatorImapConnection::FolderDeleted(const char *mailboxName)
{
    char *convertedName = CreateUtf7ConvertedString((char *) mailboxName,FALSE);
    char *orphanedMailboxName = convertedName ? fCurrentUrl->AllocateCanonicalPath(convertedName) : 0;
    
    TImapFEEvent *deleteEvent = 
        new TImapFEEvent(OnlineFolderDelete,               // function to call
                        this,                                              // access to current entry/context
                        (void *) orphanedMailboxName,
						TRUE); // storage passed
                                                        

    if (deleteEvent && orphanedMailboxName)
        fFEEventQueue->AdoptEventToEnd(deleteEvent);
    else
        HandleMemoryFailure();
    
    FREEIF(convertedName);
}

void TNavigatorImapConnection::FolderNotCreated(const char *serverMessageResponse)
{
    char *serverMessage = XP_STRDUP(serverMessageResponse);
    
    TImapFEEvent *noCreateEvent = 
        new TImapFEEvent(OnlineFolderCreateFailed,               // function to call
                        this,                                              // access to current entry/context
                        (void *) serverMessage,
						TRUE); // storage passed
                                                        

    if (noCreateEvent && serverMessage)
        fFEEventQueue->AdoptEventToEnd(noCreateEvent);
    else
        HandleMemoryFailure();
}

void TNavigatorImapConnection::FolderRenamed(const char *oldName,
                                     const char *newName)
{
    if ((fHierarchyNameState == kNoOperationInProgress) ||
		(fHierarchyNameState == kListingForInfoAndDiscovery))

    {
    	char *oldConvertedName = CreateUtf7ConvertedString((char *) oldName,FALSE);
    	char *newConvertedName = CreateUtf7ConvertedString((char *) newName,FALSE);
    	
    	if (oldConvertedName && newConvertedName)
    	{
	        folder_rename_struct *orphanRenameStruct = (folder_rename_struct *) XP_ALLOC(sizeof(folder_rename_struct));
			orphanRenameStruct->fHostName = XP_STRDUP(GetHostName());
	        orphanRenameStruct->fOldName = fCurrentUrl->AllocateCanonicalPath(oldConvertedName);
	        orphanRenameStruct->fNewName = fCurrentUrl->AllocateCanonicalPath(newConvertedName);
	        
	        
	        TImapFEEvent *renameEvent = 
	            new TImapFEEvent(OnlineFolderRename,               // function to call
	                            this,                                              // access to current entry/context
	                            (void *) orphanRenameStruct,
								TRUE);  // storage passed
	                                                            

	        if (renameEvent && orphanRenameStruct && 
	                           orphanRenameStruct->fOldName && 
	                           orphanRenameStruct->fNewName)
	            fFEEventQueue->AdoptEventToEnd(renameEvent);
	        else
	            HandleMemoryFailure();
        }
        else
        	HandleMemoryFailure();
       
       FREEIF(oldConvertedName);
       FREEIF(newConvertedName);
    }
}

void TNavigatorImapConnection::MailboxDiscoveryFinished()
{
    if (!GetSubscribingNow() &&
		((fHierarchyNameState == kNoOperationInProgress) || 
		 (fHierarchyNameState == kListingForInfoAndDiscovery)))
    {
		TIMAPNamespace *ns = TIMAPHostInfo::GetDefaultNamespaceOfTypeForHost(fCurrentUrl->GetUrlHost(), kPersonalNamespace);
		const char *personalDir = ns ? ns->GetPrefix() : 0;
		// if (!personalDir) // no personal or default NS - only public folders
		if (!TIMAPHostInfo::GetOnlineTrashFolderExistsForHost(fCurrentUrl->GetUrlHost()) &&
			fDeleteModelIsMoveToTrash && TIMAPHostInfo::GetHostIsUsingSubscription(fCurrentUrl->GetUrlHost()))
		{
			// maybe we're not subscribed to the Trash folder
			if (personalDir)
			{
				char *originalTrashName = CreatePossibleTrashName(personalDir);
				fHierarchyNameState = kDiscoverTrashFolderInProgress;
				List(originalTrashName);
				fHierarchyNameState = kNoOperationInProgress;
			}
		}

		// There is no Trash folder (either LIST'd or LSUB'd), and we're using the
		// Delete-is-move-to-Trash model, and there is a personal namespace
		if (!TIMAPHostInfo::GetOnlineTrashFolderExistsForHost(fCurrentUrl->GetUrlHost()) &&
			fDeleteModelIsMoveToTrash && personalDir)	
    	{
			const char *trashPrefix = personalDir;
    		char *trashName = CreatePossibleTrashName(trashPrefix);
    		if (trashName)
    		{
    			GetServerStateParser().SetReportingErrors(FALSE);
    			XP_Bool created = CreateMailboxRespectingSubscriptions(trashName);
				/*
				// we shouldn't need this case anymore, since we're handling namespaces
				// the right way, I think.
				if (!created && (TIMAPUrl::GetOnlineSubDirSeparator() == '.'))
    			{
    				trashPrefix = "INBOX.";
    				FREEIF(trashName);
    				trashName = CreatePossibleTrashName(trashPrefix);
    				if (trashName)
    					created = CreateMailboxRespectingSubscriptions(trashName);
    			}
				*/
    			GetServerStateParser().SetReportingErrors(TRUE);
    			
    				// force discovery of new trash folder.
    			if (created)
				{
					fHierarchyNameState = kDiscoverTrashFolderInProgress;
    				List(trashName);
					fHierarchyNameState = kNoOperationInProgress;
				}
    			else
					TIMAPHostInfo::SetOnlineTrashFolderExistsForHost(fCurrentUrl->GetUrlHost(), TRUE); // we only try this once, not every time we create any folder.
   				FREEIF(trashName);
    		}
    	}
		TIMAPHostInfo::SetHaveWeEverDiscoveredFoldersForHost(fCurrentUrl->GetUrlHost(), TRUE);
		SetFolderDiscoveryFinished();
    }
}


void TNavigatorImapConnection::SetFolderDiscoveryFinished()
{
	TImapFEEvent *discoveryDoneEvent = 
	    new TImapFEEvent(MailboxDiscoveryDoneEvent,     // function to call
	                    this,                                              // access to current entry/context
	                    NULL,
						TRUE);
	                                                    

	if (discoveryDoneEvent)
	    fFEEventQueue->AdoptEventToEnd(discoveryDoneEvent);
	else
	    HandleMemoryFailure();
}

void TNavigatorImapConnection::UpdatedMailboxSpec(mailbox_spec *adoptedBoxSpec)
{
    TImapFEEvent *endEvent = 
        new TImapFEEvent(UpdatedIMAPmailbox,    // function to call
                        this,                                   // access to current entry/context
                        (void *) adoptedBoxSpec,
						TRUE);       // storage passed
                                                    // now owned by msg folder

    if (endEvent)
        fFEEventQueue->AdoptEventToEnd(endEvent);
    else
        HandleMemoryFailure();
}

void TNavigatorImapConnection::UpdateMailboxStatus(mailbox_spec *adoptedBoxSpec)
{
    TImapFEEvent *endEvent = 
        new TImapFEEvent(UpdatedIMAPmailboxStatus,    // function to call
                        this,                                   // access to current entry/context
                        (void *) adoptedBoxSpec,
						TRUE);       // storage passed
                                                    // now owned by msg folder

    if (endEvent)
        fFEEventQueue->AdoptEventToEnd(endEvent);
    else
        HandleMemoryFailure();
}

void TNavigatorImapConnection::UploadMessage()
{
    TImapFEEvent *endEvent = 
        new TImapFEEvent(UploadMessageEvent,    // function to call
                        this,                                   // access to current entry/context
                        nil,
						TRUE);                                   // we are the knights who say nyet!

    if (endEvent)
    {
        fFEEventQueue->AdoptEventToEnd(endEvent);
        WaitForMessageUploadToComplete();
    }
    else
        HandleMemoryFailure();
}


static void
UploadMessageFileHandler(void *blockingConnectionVoid, void *msgInfo)
{
    TNavigatorImapConnection *imapConnection = 
        (TNavigatorImapConnection *) blockingConnectionVoid;
    ActiveEntry *ce = imapConnection->GetActiveEntry();
	UploadMessageInfo *uploadMsgInfo = (UploadMessageInfo*) msgInfo;
	uint32 bytesRead = 0;
	int ioStatus = 0;

	if (! uploadMsgInfo->dataBuffer)
	{							// use 4k buffer
		uploadMsgInfo->dataBuffer = 
			(char *) XP_ALLOC((kOutputBufferSize << 1)+1);
	}

	XP_ASSERT(uploadMsgInfo->fileId && uploadMsgInfo->dataBuffer);

	if(!uploadMsgInfo->fileId || !uploadMsgInfo->dataBuffer)
	{
	    if (uploadMsgInfo->fileId)
		  XP_FileClose(uploadMsgInfo->fileId);
	    XP_FREEIF(uploadMsgInfo->dataBuffer);
		XP_FREEIF(uploadMsgInfo);
		XP_InterruptContext(ce->window_id);
		return;
	}

	XP_ASSERT(uploadMsgInfo->bytesRemain > 0);

	bytesRead = XP_FileRead(uploadMsgInfo->dataBuffer,
							(kOutputBufferSize << 1),
							uploadMsgInfo->fileId);
	*(uploadMsgInfo->dataBuffer+bytesRead) = 0;
	ioStatus = NET_BlockingWrite(ce->socket, uploadMsgInfo->dataBuffer,
								 bytesRead); 
	// PR_LOG(IMAP, out, ("%s", uploadMsgInfo->dataBuffer));

	uploadMsgInfo->bytesRemain -= bytesRead;

	if (uploadMsgInfo->bytesRemain <= 0)
	{
		ioStatus = NET_BlockingWrite(ce->socket, CRLF, XP_STRLEN(CRLF));
		imapConnection->NotifyMessageUploadMonitor();
	}
	else
	  {
		TImapFEEvent *uploadMsgFromFileEvent =
		  new TImapFEEvent(UploadMessageFileHandler,
						   (void *)imapConnection,
						   (void *)msgInfo,
						   TRUE);
		if (uploadMsgFromFileEvent)
		  {
			imapConnection->GetFEEventQueue().AdoptEventToEnd(uploadMsgFromFileEvent);
		  }
		else
		  {
			imapConnection->HandleMemoryFailure();
		  }
	  }
}

XP_Bool TNavigatorImapConnection::NewMailDetected()
{
	return fNewMail;
}


void TNavigatorImapConnection::ParseIMAPandCheckForNewMail(char *buff /* = NULL */)
{
	XP_Bool oldMail = fNewMail;
	
	if (buff)
		GetServerStateParser().ParseIMAPServerResponse(buff);
	else
		GetServerStateParser().ParseIMAPServerResponse(GetOutputBuffer());

	int32 numOfMessagesInFlagState =  fFlagState->GetNumberOfMessages();

	if ((GetServerStateParser().NumberOfMessages() != numOfMessagesInFlagState) && (numOfMessagesInFlagState > 0)) {
		fNewMail = TRUE;
		if (!fFlagState->IsLastMessageUnseen())
			fNewMail = FALSE;
	} else {
		fNewMail = FALSE;
	}
	if (fNewMail != oldMail)
	{
		if (fNewMail)
			fCurrentBiffState = MSG_BIFF_NewMail;
		else
			fCurrentBiffState = MSG_BIFF_NoMail;
        SendSetBiffIndicatorEvent(fCurrentBiffState);
	}
}


void TNavigatorImapConnection::UploadMessageFromFile(const char *srcFilePath,
													 const char *mailboxName,
													 imapMessageFlagsType flags)
{
	XP_StatStruct st;

	if (-1 != XP_Stat (srcFilePath, &st, xpFileToPost))
	  {
		char *escapedName = CreateEscapedMailboxName(mailboxName);

		if (!escapedName)
		{
			XP_FREEIF(escapedName);
			HandleMemoryFailure();
			return;
		}
		/* status command only available on IMAP4rev1 and beyond
		 * lets try the status first to get the next UID
		 */
		MessageKey newkey = MSG_MESSAGEKEYNONE;
		int ioStatus;

		char flagString[70];
		long bytesRead = 0;
		setup_message_flags_string(flagString, flags, SupportsUserDefinedFlags());

		IncrementCommandTagNumber();
		PR_snprintf (GetOutputBuffer(), kOutputBufferSize,
					 "%s append \"%s\" (%s) {%ld}" CRLF,
					 GetServerCommandTag(),
					 escapedName,
					 flagString,
					 (long) st.st_size);

		ioStatus = WriteLineToSocket(GetOutputBuffer());

		ParseIMAPandCheckForNewMail();

		UploadMessageInfo *msgInfo = 
		  (UploadMessageInfo*) XP_ALLOC(sizeof(UploadMessageInfo));
		if (!msgInfo)
		{
			HandleMemoryFailure();
			return;
		}
		msgInfo->bytesRemain = st.st_size;
		msgInfo->newMsgID = newkey;
		msgInfo->dataBuffer = 0;
		msgInfo->fileId = XP_FileOpen(srcFilePath, xpFileToPost, XP_FILE_READ_BIN);

		TImapFEEvent *uploadMsgFromFileEvent =
		  new TImapFEEvent(UploadMessageFileHandler,
						   (void *)this,
						   (void *)msgInfo,
						   TRUE);
		if (uploadMsgFromFileEvent)
		  {
			fFEEventQueue->AdoptEventToEnd(uploadMsgFromFileEvent);
			PR_Sleep(PR_INTERVAL_NO_WAIT);
			WaitForMessageUploadToComplete();
			
			ParseIMAPandCheckForNewMail();

			if (GetServerStateParser().LastCommandSuccessful()
				&& MSG_IsSaveDraftDeliveryState(GetActiveEntry()->URL_s->fe_data))
			{
				GetServerStateParser().SetIMAPstate(TImapServerState::kFolderSelected); // *** sucks.....

				if ((GetServerStateParser().GetSelectedMailboxName() &&
					 XP_STRCMP(GetServerStateParser().GetSelectedMailboxName(),
							   escapedName)))
				{
					if (fCloseNeededBeforeSelect)
						Close();
					SelectMailbox(escapedName);
				}

				const char *messageId = NULL;
				if (GetActiveEntry()->URL_s->fe_data)
					messageId = MSG_GetMessageIdFromState(
						GetActiveEntry()->URL_s->fe_data);
				if (GetServerStateParser().LastCommandSuccessful() && 
					messageId)
				{
					char *tmpBuffer = PR_smprintf (
						"SEARCH SEEN HEADER Message-ID %s",
						messageId);
					GetServerStateParser().ResetSearchResultSequence();
					Search(tmpBuffer, TRUE, FALSE);
					if (GetServerStateParser().LastCommandSuccessful())
					{
						TSearchResultIterator *searchResult = 
							GetServerStateParser().CreateSearchResultIterator();
						newkey = searchResult->GetNextMessageNumber();
						delete searchResult;
						if (newkey != MSG_MESSAGEKEYNONE)
							MSG_SetIMAPMessageUID(newkey, GetActiveEntry()->URL_s->fe_data);
						XP_Bool bSuc = GetServerStateParser().LastCommandSuccessful();
					}
				}
			}

			XP_FREEIF(msgInfo->dataBuffer);
			XP_FileClose(msgInfo->fileId);
			XP_FREEIF(msgInfo);
		  }
		else
		  {
			HandleMemoryFailure();
		  }

		XP_FREEIF(escapedName);
	  }
}
													

void TNavigatorImapConnection::NotifyMessageFlags( imapMessageFlagsType flags, MessageKey key)
{
    tFlagsKeyStruct *keyAndFlag = (tFlagsKeyStruct *) XP_ALLOC( sizeof(tFlagsKeyStruct));
    keyAndFlag->flags = flags;
    keyAndFlag->key   = key;
    
    TImapFEEvent *flagsEvent = 
        new TImapFEEvent(NotifyMessageFlagsEvent,       // function to call
                        this,                                           // access to current entry/context
                        keyAndFlag,
						TRUE);                            // storage passed

    if (flagsEvent)
        fFEEventQueue->AdoptEventToEnd(flagsEvent);
    else
        HandleMemoryFailure();
}

void TNavigatorImapConnection::NotifySearchHit(const char *hitLine)
{
	if (!fNotifyHit)
		return;
    TImapFEEvent *hitEvent = 
        new TImapFEEvent(AddSearchResultEvent,  // function to call
                        this,                                           // access to current entry/context
                        (void *)hitLine,
						TRUE);

    if (hitEvent)
    {
        fFEEventQueue->AdoptEventToEnd(hitEvent);
        WaitForFEEventCompletion();
    }
    else
        HandleMemoryFailure();
    
}

static void
ArbitraryHeadersEvent(void *blockingConnectionVoid, void *valueVoid)
{
    TNavigatorImapConnection *imapConnection = 
        (TNavigatorImapConnection *) blockingConnectionVoid;	
    ActiveEntry *ce = imapConnection->GetActiveEntry();
	GenericInfo *value = (GenericInfo *)valueVoid;

	value->c = MSG_GetArbitraryHeadersForHost(ce->window_id->mailMaster, value->hostName);
    imapConnection->NotifyEventCompletionMonitor();
}

// Returns a newly allocated, space-delimited string of arbitrary headers
// which are used for filters on this host.
char *TNavigatorImapConnection::GetArbitraryHeadersToDownload()
{
	GenericInfo *value = (GenericInfo *)XP_ALLOC(sizeof(GenericInfo));
	if (!value)
		return NULL;

	value->c = NULL;
	value->hostName = XP_STRDUP(fCurrentUrl->GetUrlHost());
	if (!value->hostName)
	{
		XP_FREE(value->c);
		XP_FREE(value);
		return NULL;
	}
	TImapFEEvent *headerEvent = 
		new TImapFEEvent(ArbitraryHeadersEvent,     // function to call
						this,                                   // access to current entry/context
						value,
						FALSE);

	if (headerEvent)
	{
		fFEEventQueue->AdoptEventToEnd(headerEvent);
		WaitForFEEventCompletion();
	}
	else
		HandleMemoryFailure();

	char *rv = NULL;
	if (!DeathSignalReceived())
		rv = value->c;
	XP_FREE(value->hostName);
	XP_FREE(value);
	return rv;
}

void TNavigatorImapConnection::HeaderFetchCompleted()
{
    TImapFEEvent *endEvent = 
        new TImapFEEvent(HeaderFetchCompletedEvent,     // function to call
                        this,                                   // access to current entry/context
                        nil,
						TRUE);                                   // we are the knights who say nyet!

    if (endEvent)
        fFEEventQueue->AdoptEventToEnd(endEvent);
    else
        HandleMemoryFailure();
}

// Please call only with a single message ID
void TNavigatorImapConnection::Bodystructure(const char *messageId,
											 XP_Bool idIsUid)
{
    IncrementCommandTagNumber();
    
    char commandString[256];
    if (idIsUid)
    	XP_STRCPY(commandString, "%s UID fetch");
    else
    	XP_STRCPY(commandString, "%s fetch");

	XP_STRCAT(commandString, " %s (BODYSTRUCTURE)");
	XP_STRCAT(commandString,CRLF);

	const char *commandTag = GetServerCommandTag();
	int protocolStringSize = XP_STRLEN(commandString) + XP_STRLEN(messageId) + XP_STRLEN(commandTag) + 1;
	char *protocolString = (char *) XP_ALLOC( protocolStringSize );
    
    if (protocolString)
    {
	    PR_snprintf(protocolString,                                      // string to create
	            protocolStringSize,                                      // max size
	            commandString,                                   // format string
	            commandTag,                          // command tag
	            messageId);
	            
	    int ioStatus = WriteLineToSocket(protocolString);
	    

		ParseIMAPandCheckForNewMail(protocolString);
	    XP_FREE(protocolString);
   	}
    else
        HandleMemoryFailure();
}

void TNavigatorImapConnection::PipelinedFetchMessageParts(const char *uid,
														  TIMAPMessagePartIDArray *parts)
{
	// assumes no chunking

	// build up a string to fetch
	char *stringToFetch = NULL, *what = NULL;
	int32 currentPartNum = 0;
	while ((parts->GetNumParts() > currentPartNum) && !DeathSignalReceived())
	{
		TIMAPMessagePartID *currentPart = parts->GetPart(currentPartNum);
		if (currentPart)
		{
			// Do things here depending on the type of message part
			// Append it to the fetch string
			if (currentPartNum > 0)
				StrAllocCat(stringToFetch, " ");

			switch (currentPart->GetFields())
			{
			case kMIMEHeader:
				what = PR_smprintf("BODY[%s.MIME]",currentPart->GetPartNumberString());
				if (what)
				{
					StrAllocCat(stringToFetch, what);
					XP_FREE(what);
				}
				else
					HandleMemoryFailure();
				break;
			case kRFC822HeadersOnly:
				if (currentPart->GetPartNumberString())
				{
					what = PR_smprintf("BODY[%s.HEADER]", currentPart->GetPartNumberString());
					if (what)
					{
						StrAllocCat(stringToFetch, what);
						XP_FREE(what);
					}
					else
						HandleMemoryFailure();
				}
				else
				{
					// headers for the top-level message
					StrAllocCat(stringToFetch, "BODY[HEADER]");
				}
				break;
			default:
				XP_ASSERT(FALSE);	// we should only be pipelining MIME headers and Message headers
				break;
			}

		}
		currentPartNum++;
	}

	// Run the single, pipelined fetch command
	if ((parts->GetNumParts() > 0) && !DeathSignalReceived() && !GetPseudoInterrupted() && stringToFetch)
	{
	    IncrementCommandTagNumber();

		char *commandString = PR_smprintf("%s UID fetch %s (%s)%s", GetServerCommandTag(), uid, stringToFetch, CRLF);
		if (commandString)
		{
			int ioStatus = WriteLineToSocket(commandString);
			ParseIMAPandCheckForNewMail(commandString);
			XP_FREE(commandString);
		}
		else
			HandleMemoryFailure();
		XP_FREE(stringToFetch);
	}
}


void TNavigatorImapConnection::FetchMessage(const char *messageIds,
                                            eFetchFields whatToFetch,
                                            XP_Bool idIsUid,
											uint32 startByte, uint32 endByte,
											char *part)
{
    IncrementCommandTagNumber();
    
    char commandString[350];
    if (idIsUid)
    	XP_STRCPY(commandString, "%s UID fetch");
    else
    	XP_STRCPY(commandString, "%s fetch");
    
    switch (whatToFetch) {
        case kEveryThingRFC822:
			if (fTrackingTime)
				AdjustChunkSize();			// we started another segment
			fStartTime = XP_TIME();			// save start of download time
			fTrackingTime = TRUE;
			if (GetServerStateParser().ServerHasIMAP4Rev1Capability())
			{
				if (GetServerStateParser().GetCapabilityFlag() & kHasXSenderCapability)
					XP_STRCAT(commandString, " %s (XSENDER UID RFC822.SIZE BODY[]");
				else
					XP_STRCAT(commandString, " %s (UID RFC822.SIZE BODY[]");
			}
			else
			{
				if (GetServerStateParser().GetCapabilityFlag() & kHasXSenderCapability)
					XP_STRCAT(commandString, " %s (XSENDER UID RFC822.SIZE RFC822");
				else
					XP_STRCAT(commandString, " %s (UID RFC822.SIZE RFC822");
			}
			if (endByte > 0)
			{
				// if we are retrieving chunks
				char *byterangeString = PR_smprintf("<%ld.%ld>",startByte,endByte);
				if (byterangeString)
				{
					XP_STRCAT(commandString, byterangeString);
					XP_FREE(byterangeString);
				}
			}
			XP_STRCAT(commandString, ")");
            break;

        case kEveryThingRFC822Peek:
        	{
	        	char *formatString = "";
	        	uint32 server_capabilityFlags = GetServerStateParser().GetCapabilityFlag();
	        	
	        	if (server_capabilityFlags & kIMAP4rev1Capability)
	        	{
	        		// use body[].peek since rfc822.peek is not in IMAP4rev1
	        		if (server_capabilityFlags & kHasXSenderCapability)
	        			formatString = " %s (XSENDER UID RFC822.SIZE BODY.PEEK[])";
	        		else
	        			formatString = " %s (UID RFC822.SIZE BODY.PEEK[])";
	        	}
	        	else
	        	{
	        		if (server_capabilityFlags & kHasXSenderCapability)
	        			formatString = " %s (XSENDER UID RFC822.SIZE RFC822.peek)";
	        		else
	        			formatString = " %s (UID RFC822.SIZE RFC822.peek)";
	        	}
	        
				XP_STRCAT(commandString, formatString);
			}
            break;
        case kHeadersRFC822andUid:
			if (GetServerStateParser().ServerHasIMAP4Rev1Capability())
			{
				if (gOptimizedHeaders)
				{
					char *headersToDL = NULL;
					char *arbitraryHeaders = GetArbitraryHeadersToDownload();
					if (arbitraryHeaders)
					{
						headersToDL = PR_smprintf("%s %s",IMAP_DB_HEADERS,arbitraryHeaders);
						XP_FREE(arbitraryHeaders);
					}
					else
					{
						headersToDL = PR_smprintf("%s",IMAP_DB_HEADERS);
					}
					if (headersToDL)
					{
						char *what = PR_smprintf(" BODY.PEEK[HEADER.FIELDS (%s)])", headersToDL);
						if (what)
						{
							XP_STRCAT(commandString, " %s (UID RFC822.SIZE FLAGS");
							XP_STRCAT(commandString, what);
							XP_FREE(what);
						}
						else
						{
							XP_STRCAT(commandString, " %s (UID RFC822.SIZE BODY.PEEK[HEADER] FLAGS)");
						}
						XP_FREE(headersToDL);
					}
					else
					{
						XP_STRCAT(commandString, " %s (UID RFC822.SIZE BODY.PEEK[HEADER] FLAGS)");
					}
				}
				else
					XP_STRCAT(commandString, " %s (UID RFC822.SIZE BODY.PEEK[HEADER] FLAGS)");
			}
			else
				XP_STRCAT(commandString, " %s (UID RFC822.SIZE RFC822.HEADER FLAGS)");
            break;
        case kUid:
			XP_STRCAT(commandString, " %s (UID)");
            break;
        case kFlags:
			XP_STRCAT(commandString, " %s (FLAGS)");
            break;
        case kRFC822Size:
			XP_STRCAT(commandString, " %s (RFC822.SIZE)");
			break;
		case kRFC822HeadersOnly:
			if (GetServerStateParser().ServerHasIMAP4Rev1Capability())
			{
				if (part)
				{
					XP_STRCAT(commandString, " %s (BODY[");
					char *what = PR_smprintf("%s.HEADER])", part);
					if (what)
					{
						XP_STRCAT(commandString, what);
						XP_FREE(what);
					}
					else
						HandleMemoryFailure();
				}
				else
				{
					// headers for the top-level message
					XP_STRCAT(commandString, " %s (BODY[HEADER])");
				}
			}
			else
				XP_STRCAT(commandString, " %s (RFC822.HEADER)");
			break;
		case kMIMEPart:
			XP_STRCAT(commandString, " %s (BODY[%s]");
			if (endByte > 0)
			{
				// if we are retrieving chunks
				char *byterangeString = PR_smprintf("<%ld.%ld>",startByte,endByte);
				if (byterangeString)
				{
					XP_STRCAT(commandString, byterangeString);
					XP_FREE(byterangeString);
				}
			}
			XP_STRCAT(commandString, ")");
			break;
		case kMIMEHeader:
			XP_STRCAT(commandString, " %s (BODY[%s.MIME])");
    		break;
	};

	XP_STRCAT(commandString,CRLF);

		// since messageIds can be infinitely long, use a dynamic buffer rather than the fixed one
	const char *commandTag = GetServerCommandTag();
	int protocolStringSize = XP_STRLEN(commandString) + XP_STRLEN(messageIds) + XP_STRLEN(commandTag) + 1 +
		(part ? XP_STRLEN(part) : 0);
	char *protocolString = (char *) XP_ALLOC( protocolStringSize );
    
    if (protocolString)
    {
		if ((whatToFetch == kMIMEPart) ||
			(whatToFetch == kMIMEHeader))
		{
			PR_snprintf(protocolString,                                      // string to create
					protocolStringSize,                                      // max size
					commandString,                                   // format string
					commandTag,                          // command tag
					messageIds,
					part);
		}
		else
		{
			PR_snprintf(protocolString,                                      // string to create
					protocolStringSize,                                      // max size
					commandString,                                   // format string
					commandTag,                          // command tag
					messageIds);
		}
	            
	    int                 ioStatus = WriteLineToSocket(protocolString);
	    

		ParseIMAPandCheckForNewMail(protocolString);
	    XP_FREE(protocolString);
   	}
    else
        HandleMemoryFailure();
}


void TNavigatorImapConnection::FetchTryChunking(const char *messageIds,
                                            eFetchFields whatToFetch,
                                            XP_Bool idIsUid,
											char *part,
											uint32 downloadSize)
{
	GetServerStateParser().SetTotalDownloadSize(downloadSize);
	if (fFetchByChunks && GetServerStateParser().ServerHasIMAP4Rev1Capability() &&
		(downloadSize > (uint32) fChunkThreshold))
	{
		uint32 startByte = 0;
		GetServerStateParser().ClearLastFetchChunkReceived();
		while (!DeathSignalReceived() && !GetPseudoInterrupted() && 
			!GetServerStateParser().GetLastFetchChunkReceived() &&
			GetServerStateParser().ContinueParse())
		{
			uint32 sizeToFetch = startByte + fChunkSize > downloadSize ?
				downloadSize - startByte : fChunkSize;
			FetchMessage(messageIds, 
						 whatToFetch,
						 idIsUid,
						 startByte, sizeToFetch,
						 part);
			startByte += sizeToFetch;
		}

		// Only abort the stream if this is a normal message download
		// Otherwise, let the body shell abort the stream.
		if ((whatToFetch == TIMAP4BlockingConnection::kEveryThingRFC822)
			&&
			((startByte > 0 && (startByte < downloadSize) &&
			(DeathSignalReceived() || GetPseudoInterrupted())) ||
			!GetServerStateParser().ContinueParse()))
		{
			AbortMessageDownLoad();
			PseudoInterrupt(FALSE);
		}
	}
	else
	{
		// small message, or (we're not chunking and not doing bodystructure),
		// or the server is not rev1.
		// Just fetch the whole thing.
		FetchMessage(messageIds, whatToFetch,idIsUid, 0, 0, part);
	}
}


PRMonitor *TNavigatorImapConnection::fFindingMailboxesMonitor = nil;
PRMonitor *TNavigatorImapConnection::fUpgradeToSubscriptionMonitor = nil;
XP_Bool   TNavigatorImapConnection::fHaveWeEverCheckedForSubscriptionUpgrade = FALSE;
MSG_BIFF_STATE   TNavigatorImapConnection::fCurrentBiffState = MSG_BIFF_Unknown;
PRMonitor *TIMAPHostInfo::gCachedHostInfoMonitor = nil;
TIMAPHostInfo *TIMAPHostInfo::fHostInfoList = nil;

void TNavigatorImapConnection::ResetCachedConnectionInfo()
{
	fCurrentBiffState = MSG_BIFF_Unknown;
	TIMAPHostInfo::ResetAll();
}




static
void WriteLineEvent(void *blockingConnectionVoid,
							 void *sockInfoVoid)
{
    TNavigatorImapConnection *imapConnection = 
        (TNavigatorImapConnection *) blockingConnectionVoid;
    
    ActiveEntry *ce = imapConnection->GetActiveEntry();

	TIMAPSocketInfo *sockInfo = (TIMAPSocketInfo *)sockInfoVoid;

	if (sockInfo && sockInfo->writeLine)
	{
		LIBNET_LOCK();
		sockInfo->writeStatus = (int) NET_BlockingWrite(ce->socket,
			sockInfo->writeLine,
	        XP_STRLEN(sockInfo->writeLine));

		FREEIF(sockInfo->writeLine);
		LIBNET_UNLOCK();
	}
	else
	{
		XP_ASSERT(FALSE);
	}

	imapConnection->NotifyEventCompletionMonitor();
}


static
void ReadFirstLineFromSocket(void *blockingConnectionVoid,
							 void *sockInfoVoid)
{
    TNavigatorImapConnection *imapConnection = 
        (TNavigatorImapConnection *) blockingConnectionVoid;
    
    ActiveEntry *ce = imapConnection->GetActiveEntry();

	TIMAPSocketInfo *sockInfo = (TIMAPSocketInfo *)sockInfoVoid;

	if (sockInfo)
	{
		LIBNET_LOCK();
		Bool pause = sockInfo->GetPauseForRead();
		sockInfo->readStatus = NET_BufferedReadLine(sockInfo->GetIOSocket(),
													sockInfo->GetNewLineBuffer(),
													sockInfo->GetInputSocketBuffer(),
													sockInfo->GetInputBufferSize(),
													&pause);

		LIBNET_UNLOCK();
	}
	else
	{
		XP_ASSERT(FALSE);
	}

	imapConnection->NotifyEventCompletionMonitor();
}

#ifndef XP_WIN16
#define DO_THREADED_IMAP_SOCKET_IO
#endif

#ifndef DO_THREADED_IMAP_SOCKET_IO	// do socket writes in the mozilla thread 

int TNavigatorImapConnection::WriteLineToSocket(char *line)
{
    int returnValue = 0;
     
    if (!DeathSignalReceived())
    {
		if (line) fSocketInfo->writeLine = XP_STRDUP(line);

		fSocketInfo->writeStatus = 0;
		TImapFEEvent *feWriteLineEvent = 
					new TImapFEEvent(WriteLineEvent,  // function to call
					this,                  // for access to current
                                           // entry and monitor
                    fSocketInfo,
					TRUE);
        
        fFEEventQueue->AdoptEventToEnd(feWriteLineEvent);
        PR_Sleep(PR_INTERVAL_NO_WAIT);
        
        // wait here for the read first line io to finish
        WaitForFEEventCompletion();
	
		int socketError = SOCKET_ERRNO;
		Log("NET","WR",line);
		//PR_LOG(IMAP, out, ("WR: %s",line));
		int writeStatus = fSocketInfo->writeStatus;

	    if(writeStatus <= 0)
	    {
	        if (fCurrentEntry && fCurrentEntry->URL_s)
				fCurrentEntry->URL_s->error_msg = 
		            NET_ExplainErrorDetails(MK_TCP_WRITE_ERROR, socketError);
	        returnValue = MK_TCP_WRITE_ERROR;
			GetServerStateParser().SetConnected(FALSE);
	    }
	    else
	        PR_Sleep(PR_INTERVAL_NO_WAIT);   
    }          
        
    
    return returnValue;
}


char *TNavigatorImapConnection::CreateNewLineFromSocket()
{
    Bool    pauseForRead = TRUE;
    char *newLine = nil;
    int socketReadStatus = -1;

    while (pauseForRead && !DeathSignalReceived())
    {
		fSocketInfo->SetIOSocket(GetIOSocket());
		fSocketInfo->SetPauseForRead(pauseForRead);
		fSocketInfo->SetReadStatus(socketReadStatus);
		fSocketInfo->SetInputSocketBuffer(&fInputSocketBuffer);
		fSocketInfo->SetInputBufferSize(&fInputBufferSize);

		TImapFEEvent *feReadFirstLineEvent = 
					new TImapFEEvent(ReadFirstLineFromSocket,  // function to call
					this,                  // for access to current
                                           // entry and monitor
                    fSocketInfo,
					TRUE);
        
        fFEEventQueue->AdoptEventToEnd(feReadFirstLineEvent);
        PR_Sleep(PR_INTERVAL_NO_WAIT);
        
        // wait here for the read first line io to finish
        WaitForFEEventCompletion();
          
		if (fSocketInfo->newLine)
			newLine = XP_STRDUP(fSocketInfo->newLine);
		socketReadStatus = fSocketInfo->GetReadStatus();
		//if (*(fSocketInfo->GetNewLineBuffer())) newLine = XP_STRDUP(*(socketStuff->pNewLine));
		//FREEIF(socketStuff->pNewLine);

        int socketError = PR_GetError();
        if (newLine)
			Log("NET","RD", newLine);
			//PR_LOG(IMAP, out, ("RD: %s",newLine));

        if (socketReadStatus <= 0)       // error
        {

#ifdef KMCENTEE_DEBUG 
			XP_ASSERT(socketError != PR_NOT_CONNECTED_ERROR);
#endif
	        if (socketError == PR_WOULD_BLOCK_ERROR
				|| socketError == PR_NOT_CONNECTED_ERROR)
	        {
	            pauseForRead = TRUE;
	            WaitForIOCompletion();
	        }
            else
            {
        		LIBNET_LOCK();
            	if (fCurrentEntry && fCurrentEntry->URL_s)
					fCurrentEntry->URL_s->error_msg =
						NET_ExplainErrorDetails(MK_TCP_READ_ERROR, 
						                        socketError);
            	pauseForRead = FALSE;
            	socketReadStatus = MK_TCP_READ_ERROR;
				GetServerStateParser().SetConnected(FALSE);
            	LIBNET_UNLOCK();
        	}
        }
        else if (pauseForRead && !newLine)
            WaitForIOCompletion();
        else
            pauseForRead = FALSE;
    }
    
    // the comments for NET_BufferedReadLine say that newLine is allocated
    // before it is set.  TO me this means that the caller owns the newLine
    // storage.  But I have seen it stepped on and we have assertion failures
    // when we delete it!
    char *bogusNewLine = NULL;
    if (newLine)
    {
        bogusNewLine = XP_STRDUP(newLine);
        if (bogusNewLine)
        	StrAllocCat(bogusNewLine, CRLF);
        

        if (!bogusNewLine)
        	HandleMemoryFailure();
    }
    
    SetConnectionStatus(socketReadStatus);
    FREEIF(newLine);

    return bogusNewLine;
}


#else  // not Win16: do it the usual way

int TNavigatorImapConnection::WriteLineToSocket(char *line)
{
    int returnValue = 0;
     
    if (!DeathSignalReceived())
    {
		LIBNET_LOCK();
	    int writeStatus = 0;
	    
	    // check for death signal again inside LIBNET_LOCK because
	    // we may have locked on LIBNET_LOCK because this context
	    // was being interrupted and interrupting the context means
	    // DeathSignalReceived is true and fCurrentEntry was deleted.
		if (!DeathSignalReceived())
	    	writeStatus = (int) NET_BlockingWrite(fCurrentEntry->socket,
	                                              line,
	                                              XP_STRLEN(line));
	    	
	    int socketError = SOCKET_ERRNO;
		Log("NET","WR",line);
		//PR_LOG(IMAP, out, ("WR: %s",line));
	    LIBNET_UNLOCK();
		
	    if(writeStatus <= 0)
	    {
	    	LIBNET_LOCK();
	    	if (!DeathSignalReceived())
		        fCurrentEntry->URL_s->error_msg = 
		            NET_ExplainErrorDetails(MK_TCP_WRITE_ERROR, socketError);
		    LIBNET_UNLOCK();
	        returnValue = MK_TCP_WRITE_ERROR;
			GetServerStateParser().SetConnected(FALSE);
	    }
	    else
	        PR_Sleep(PR_INTERVAL_NO_WAIT);   
    }          
        
    
    return returnValue;
}

char *TNavigatorImapConnection::CreateNewLineFromSocket()
{
    Bool    pauseForRead = TRUE;
    char    *newLine = nil;
    int socketReadStatus = -1;

    while (pauseForRead && !DeathSignalReceived())
    {
        LIBNET_LOCK();
	    // check for death signal again inside LIBNET_LOCK because
	    // we may have locked on LIBNET_LOCK because this context
	    // was being interrupted and interrupting the context means
	    // DeathSignalReceived is true and fCurrentEntry was deleted.
        if (!DeathSignalReceived())
	        socketReadStatus = NET_BufferedReadLine(GetIOSocket(),
	                                                &newLine,
	                                                &fInputSocketBuffer,
	                                                &fInputBufferSize,
	                                                &pauseForRead);
                                                
        int socketError = PR_GetError();
        if (newLine)
			Log("NET","RD",newLine);
			//PR_LOG(IMAP, out, ("RD: %s",newLine));
        LIBNET_UNLOCK();

        if (socketReadStatus <= 0)       // error
        {

#ifdef KMCENTEE_DEBUG 
			XP_ASSERT(socketError != PR_NOT_CONNECTED_ERROR);
#endif
	        if (socketError == PR_WOULD_BLOCK_ERROR
				|| socketError == PR_NOT_CONNECTED_ERROR)
	        {
	            pauseForRead = TRUE;
	            WaitForIOCompletion();
	        }
            else
            {
        		LIBNET_LOCK();
        		if (!DeathSignalReceived())
        		{
	            	fCurrentEntry->URL_s->error_msg =
	                NET_ExplainErrorDetails(MK_TCP_READ_ERROR, 
	                                            socketError);
                }
            	pauseForRead = FALSE;
            	socketReadStatus = MK_TCP_READ_ERROR;
				GetServerStateParser().SetConnected(FALSE);
            	LIBNET_UNLOCK();
        	}
        }
        else if (pauseForRead && !newLine)
            WaitForIOCompletion();
        else
            pauseForRead = FALSE;
        
    }
    
    // the comments for NET_BufferedReadLine say that newLine is allocated
    // before it is set.  TO me this means that the caller owns the newLine
    // storage.  But I have seen it stepped on and we have assertion failures
    // when we delete it!
    char *bogusNewLine = NULL;
    if (newLine)
    {
        bogusNewLine = XP_STRDUP(newLine);
        if (bogusNewLine)
       		StrAllocCat(bogusNewLine, CRLF);

        if (!bogusNewLine)
        	HandleMemoryFailure();
    }
    
    SetConnectionStatus(socketReadStatus);
    
    return bogusNewLine;
}

#endif	// not win16


void TNavigatorImapConnection::SetIOSocket(PRFileDesc *socket)
{
    PR_EnterMonitor(GetDataMemberMonitor());
    fIOSocket = socket;
    PR_ExitMonitor(GetDataMemberMonitor());
}

PRFileDesc * TNavigatorImapConnection::GetIOSocket()
{
    PRFileDesc * returnSocket;
    PR_EnterMonitor(GetDataMemberMonitor());
    returnSocket = fIOSocket;
    PR_ExitMonitor(GetDataMemberMonitor());
    return returnSocket;
}

void TNavigatorImapConnection::Logout()
{
    TIMAP4BlockingConnection::Logout();
    SetIOSocket(0);
}

void TNavigatorImapConnection::SetOutputStream(NET_StreamClass *outputStream)
{
    PR_EnterMonitor(GetDataMemberMonitor());
    fOutputStream = outputStream;
    PR_ExitMonitor(GetDataMemberMonitor());
}

NET_StreamClass *TNavigatorImapConnection::GetOutputStream()
{
    NET_StreamClass *returnStream;
    PR_EnterMonitor(GetDataMemberMonitor());
    returnStream = fOutputStream;
    PR_ExitMonitor(GetDataMemberMonitor());
    return returnStream;
}

void TNavigatorImapConnection::SetIsSafeToDie()		// called by TellThreadToDie
{
    PR_EnterMonitor(fPermissionToDieMonitor);
	fIsSafeToDie = TRUE;
	PR_Notify(fPermissionToDieMonitor);
    PR_ExitMonitor(fPermissionToDieMonitor);
}


void TNavigatorImapConnection::SetBlockingThread(PRThread *blockingThread)
{
    fBlockingThread = blockingThread;
}

ActiveEntry      *TNavigatorImapConnection::GetActiveEntry()
{
    return fCurrentEntry;
}

// (Use Get/SetCurrentEntryStatus()) for passing values back to libnet about the currently running ActiveEntry.
void TNavigatorImapConnection::SetCurrentEntryStatus(int status)
{
	// There was a deadlock when the imap thread was waiting for this libnet lock and the mozilla thread
	// held this lock, finished the current url and tried to launch another url from the url exit function.
	// This caused the mozilla thread to never release the LIBNET_LOCK.
	
	XP_ASSERT(GetConnectionStatus() >= 0);
	
    LIBNET_LOCK();
	if (!DeathSignalReceived())
    {
	    ActiveEntry *ce = GetActiveEntry();
		if (ce)
			ce->status = status;
	}
	LIBNET_UNLOCK();
}


int TNavigatorImapConnection::GetCurrentEntryStatus()
{
    int returnStatus = 0;
    LIBNET_LOCK();
    if (!DeathSignalReceived())
    {
	    ActiveEntry *ce = GetActiveEntry();
		if (ce)
			returnStatus = ce->status;
	}
	LIBNET_UNLOCK();
    return returnStatus;
}

TCP_ConData      **TNavigatorImapConnection::GetTCPConData()
{
    return &fTCPConData;
}

void TNavigatorImapConnection::EstablishServerConnection()
{
    // let the fe-thread start the connection since we are using
    // old non-thread safe functions to do it.

    // call NET_FinishConnect until we are connected or errored out
    while (!DeathSignalReceived() && (GetConnectionStatus() == MK_WAITING_FOR_CONNECTION))
    {
        TImapFEEvent *feFinishConnectionEvent = 
            new TImapFEEvent(FinishIMAPConnection,  // function to call
                             this,                  // for access to current
                                                    // entry and monitor
                             nil,
							 TRUE);
        
        fFEEventQueue->AdoptEventToEnd(feFinishConnectionEvent);
        PR_Sleep(PR_INTERVAL_NO_WAIT);
        
        // wait here for the connection finish io to finish
        WaitForFEEventCompletion();
		if (!DeathSignalReceived() && (GetConnectionStatus() == MK_WAITING_FOR_CONNECTION))
			WaitForIOCompletion();
        
    }       
    
	if (GetConnectionStatus() == MK_CONNECTED)
    {
        // get the one line response from the IMAP server
        char *serverResponse = CreateNewLineFromSocket();
        if (serverResponse)
		{
			if (!XP_STRNCASECMP(serverResponse, "* OK", 4))
			{
				SetConnectionStatus(0);
				preAuthSucceeded = FALSE;
				//if (!XP_STRNCASECMP(serverResponse, "* OK Netscape IMAP4rev1 Service 3.0", 35))
				//	GetServerStateParser().SetServerIsNetscape30Server();
			}
			else if (!XP_STRNCASECMP(serverResponse, "* PREAUTH", 9))
			{
				// we've been pre-authenticated.
				// we can skip the whole password step, right into the
				// kAuthenticated state
				GetServerStateParser().PreauthSetAuthenticatedState();

				// tell the master that we're authenticated
				XP_Bool loginSucceeded = TRUE;
				TImapFEEvent *alertEvent = 
					new TImapFEEvent(msgSetUserAuthenticated,  // function to call
							 this,                // access to current entry
							 (void *)loginSucceeded,
							 TRUE);            
				if (alertEvent)
				{
					fFEEventQueue->AdoptEventToEnd(alertEvent);
				}
				else
					HandleMemoryFailure();

				preAuthSucceeded = TRUE;
           		if (fCurrentBiffState == MSG_BIFF_Unknown)
           		{
           			fCurrentBiffState = MSG_BIFF_NoMail;
               		SendSetBiffIndicatorEvent(fCurrentBiffState);
           		}

				if (GetServerStateParser().GetCapabilityFlag() ==
					kCapabilityUndefined)
					Capability();

				if ( !(GetServerStateParser().GetCapabilityFlag() & 
						(kIMAP4Capability | 
						 kIMAP4rev1Capability | 
						 kIMAP4other) ) )
				{
					AlertUserEvent_UsingId(MK_MSG_IMAP_SERVER_NOT_IMAP4);
					SetCurrentEntryStatus(-1);			
					SetConnectionStatus(-1);        // stop netlib
					preAuthSucceeded = FALSE;
				}
				else
				{
					ProcessAfterAuthenticated();
					// the connection was a success
					SetConnectionStatus(0);
				}
			}
			else
			{
				preAuthSucceeded = FALSE;
				SetConnectionStatus(MK_BAD_CONNECT);
			}

			fNeedGreeting = FALSE;
			FREEIF(serverResponse);
		}
        else
            SetConnectionStatus(MK_BAD_CONNECT);
    }

    if ((GetConnectionStatus() < 0) && !DeathSignalReceived())
	{
		if (!MSG_Biff_Master_NikiCallingGetNewMail())
    		AlertUserEvent_UsingId(MK_BAD_CONNECT);
	}
    
}


void TNavigatorImapConnection::ProcessStoreFlags(const char *messageIds,
                                                 XP_Bool idsAreUids,
                                                 imapMessageFlagsType flags,
                                                 XP_Bool addFlags)
{
	if (!flags)
		return;
		
    char *flagString;
	XP_Bool userF = GetServerStateParser().SupportsUserFlags();
    
	if (!userF && (flags == kImapMsgForwardedFlag || 
				   flags == kImapMsgMDNSentFlag)) // if only flag, and not supported bail out
		return;
    if (addFlags)
        flagString = XP_STRDUP("+Flags (");
    else
        flagString = XP_STRDUP("-Flags (");
    
    if (flags & kImapMsgSeenFlag)
        StrAllocCat(flagString, "\\Seen ");
    if (flags & kImapMsgAnsweredFlag)
        StrAllocCat(flagString, "\\Answered ");
    if (flags & kImapMsgFlaggedFlag)
        StrAllocCat(flagString, "\\Flagged ");
    if (flags & kImapMsgDeletedFlag)
        StrAllocCat(flagString, "\\Deleted ");
    if (flags & kImapMsgDraftFlag)
        StrAllocCat(flagString, "\\Draft ");
	if ((flags & kImapMsgForwardedFlag) && userF)
        StrAllocCat(flagString, "Forwarded ");	// if supported
	if ((flags & kImapMsgMDNSentFlag) && userF)
        StrAllocCat(flagString, "MDNSent ");	// if supported

    // replace the final space with ')'
    *(flagString + XP_STRLEN(flagString) - 1) = ')';
    
    Store(messageIds, flagString, idsAreUids);
    FREEIF(flagString);
}


void TNavigatorImapConnection::ProcessAfterAuthenticated()
{
	// if we're a netscape server, and we haven't got the admin url, get it
	if (!TIMAPHostInfo::GetHostHasAdminURL(fCurrentUrl->GetUrlHost()))
	{
		if (GetServerStateParser().GetCapabilityFlag() & kXServerInfoCapability)
		{
			XServerInfo();
			if (GetServerStateParser().LastCommandSuccessful()) 
			{
				TImapFEEvent *alertEvent = 
					new TImapFEEvent(msgSetMailServerURLs,  // function to call
									 this,                // access to current entry
									 (void *) fCurrentUrl->GetUrlHost(),
									 TRUE);            
				if (alertEvent)
				{
					fFEEventQueue->AdoptEventToEnd(alertEvent);
					// WaitForFEEventCompletion();
				}
				else
					HandleMemoryFailure();
			}
		}
		else if (GetServerStateParser().GetCapabilityFlag() & kHasXNetscapeCapability)
		{
			Netscape();
			if (GetServerStateParser().LastCommandSuccessful()) 
			{
				TImapFEEvent *alertEvent = 
					new TImapFEEvent(msgSetMailAccountURL,  // function to call
									 this,                // access to current entry
									 (void *) fCurrentUrl->GetUrlHost(),
									 TRUE);
				if (alertEvent)
				{
					fFEEventQueue->AdoptEventToEnd(alertEvent);
					// WaitForFEEventCompletion();
				}
				else
					HandleMemoryFailure();
			}
		}
	}

	if (GetServerStateParser().ServerHasNamespaceCapability() &&
		TIMAPHostInfo::GetNamespacesOverridableForHost(fCurrentUrl->GetUrlHost()))
	{
		Namespace();
	}
}

XP_Bool TNavigatorImapConnection::TryToLogon()
{

	int32 logonTries = 0;
	XP_Bool loginSucceeded = FALSE;

	do
	{
		char *passwordForHost = TIMAPHostInfo::GetPasswordForHost(fCurrentUrl->GetUrlHost());
	    XP_Bool imapPasswordIsNew = FALSE;
	    XP_Bool setUserAuthenticatedIsSafe = FALSE;
		ActiveEntry *ce = GetActiveEntry();
		MSG_Master *master = (ce) ? ce->window_id->mailMaster : 0;

	    const char *userName = MSG_GetIMAPHostUsername(master, fCurrentUrl->GetUrlHost());
		if (userName && !passwordForHost)
	    {
	        TImapFEEvent *fePasswordEvent = 
	            new TImapFEEvent(GetPasswordEventFunction,
	                             this,
	                             (void*) userName,
								 TRUE);
	        
	        if (fePasswordEvent)
	        {
	            fFEEventQueue->AdoptEventToEnd(fePasswordEvent);
	            IMAP_YIELD(PR_INTERVAL_NO_WAIT);
	            
	            // wait here password prompt to finish
	            WaitForFEEventCompletion();
	            imapPasswordIsNew = TRUE;
				passwordForHost = TIMAPHostInfo::GetPasswordForHost(fCurrentUrl->GetUrlHost());
	        }
	        else
	            HandleMemoryFailure();
	    }
	    
	    if (userName && passwordForHost)
	    {
			XP_Bool prefBool = FALSE;

			XP_Bool lastReportingErrors = GetServerStateParser().GetReportingErrors();
			GetServerStateParser().SetReportingErrors(FALSE);	// turn off errors - we'll put up our own.

			PREF_GetBoolPref("mail.auth_login", &prefBool);
			if (prefBool) {
				if (GetServerStateParser().GetCapabilityFlag() ==
					kCapabilityUndefined)
					Capability();
				if (GetServerStateParser().GetCapabilityFlag() &
					kHasAuthLoginCapability)
				{
					AuthLogin (userName, passwordForHost);
					logonTries++;	// I think this counts against most servers as a logon try
				}
				else
					InsecureLogin(userName, passwordForHost);
			}
			else
				InsecureLogin(userName, passwordForHost);
	        
			if (!GetServerStateParser().LastCommandSuccessful())
	        {
	            // login failed!
	            // if we failed because of an interrupt, then do not bother the user
	            if (!DeathSignalReceived())
	            {
					AlertUserEvent_UsingId(XP_MSG_IMAP_LOGIN_FAILED);
		            if (passwordForHost != NULL)
		            {
						TIMAPHostInfo::SetPasswordForHost(fCurrentUrl->GetUrlHost(), NULL);
		            }
		            fCurrentBiffState = MSG_BIFF_Unknown;
		            SendSetBiffIndicatorEvent(fCurrentBiffState);
	            }
	            
	            HandleCurrentUrlError();
//	            SetConnectionStatus(-1);        // stop netlib
	        }
	        else	// login succeeded
	        {
				MSG_SetIMAPHostPassword(ce->window_id->mailMaster, fCurrentUrl->GetUrlHost(), passwordForHost);
				imapPasswordIsNew = !TIMAPHostInfo::GetPasswordVerifiedOnline(fCurrentUrl->GetUrlHost());
				if (imapPasswordIsNew)
					TIMAPHostInfo::SetPasswordVerifiedOnline(fCurrentUrl->GetUrlHost());
				NET_SetPopPassword2(passwordForHost); // bug 53380
				if (imapPasswordIsNew) 
				{
	                if (fCurrentBiffState == MSG_BIFF_Unknown)
	                {
	                	fCurrentBiffState = MSG_BIFF_NoMail;
	                    SendSetBiffIndicatorEvent(fCurrentBiffState);
	                }
	                LIBNET_LOCK();
	                if (!DeathSignalReceived())
	                {
	                	setUserAuthenticatedIsSafe = GetActiveEntry()->URL_s->msg_pane != NULL;
						MWContext *context = GetActiveEntry()->window_id;
						if (context)
							FE_RememberPopPassword(context, SECNAV_MungeString(passwordForHost));
					}
					LIBNET_UNLOCK();
				}
				loginSucceeded = TRUE;
	        }
			GetServerStateParser().SetReportingErrors(lastReportingErrors);  // restore it

			if (loginSucceeded && imapPasswordIsNew)
			{
				TImapFEEvent *alertEvent = 
					new TImapFEEvent(msgSetUserAuthenticated,  // function to call
									 this,                // access to current entry
									 (void *)loginSucceeded,
									 TRUE); 
				if (alertEvent)
				{
					fFEEventQueue->AdoptEventToEnd(alertEvent);
					// WaitForFEEventCompletion();
				}
				else
					HandleMemoryFailure();
			}

			if (loginSucceeded)
			{
				ProcessAfterAuthenticated();
			}
	    }
	    else
	    {
			// The user hit "Cancel" on the dialog box
	        //AlertUserEvent("Login cancelled.");
			HandleCurrentUrlError();
			SetCurrentEntryStatus(-1);
	        SetConnectionStatus(-1);        // stop netlib
			break;
	    }
	}

	while (!loginSucceeded && ++logonTries < 4);

	if (!loginSucceeded)
	{
		fCurrentBiffState = MSG_BIFF_Unknown;
		SendSetBiffIndicatorEvent(fCurrentBiffState);
		HandleCurrentUrlError();
		SetConnectionStatus(-1);        // stop netlib
	}

	return loginSucceeded;
}


void TNavigatorImapConnection::ProcessCurrentURL()
{
    XP_ASSERT(GetFEEventQueue().NumberOfEventsInQueue() == 0);
    
    if (fCurrentUrl->ValidIMAPUrl())
    {
		// Reinitialize the parser
		GetServerStateParser().InitializeState();

		// establish the connection and login
        if ((GetConnectionStatus() == MK_WAITING_FOR_CONNECTION) ||
			fNeedGreeting)
            EstablishServerConnection();

        if (!DeathSignalReceived() && (GetConnectionStatus() >= 0) &&
            (GetServerStateParser().GetIMAPstate() == 
			 TImapServerState::kNonAuthenticated))
        {
			/* if we got here, the server's greeting should not have
			   been PREAUTH */
			if (GetServerStateParser().GetCapabilityFlag() ==
				kCapabilityUndefined)
				Capability();
			
			if ( !(GetServerStateParser().GetCapabilityFlag() & 
					(kIMAP4Capability | 
					 kIMAP4rev1Capability | 
					 kIMAP4other) ) )
			{
				AlertUserEvent_UsingId(MK_MSG_IMAP_SERVER_NOT_IMAP4);
				SetCurrentEntryStatus(-1);			
				SetConnectionStatus(-1);        // stop netlib
			}
			else
			{
				TryToLogon();
	        }
        }

        if (!DeathSignalReceived() && (GetConnectionStatus() >= 0))
        {
			FindMailboxesIfNecessary();

	        if (fCurrentUrl->GetUrlIMAPstate() == 
	                TIMAPUrl::kAuthenticatedStateURL)
	            ProcessAuthenticatedStateURL();
	        else    // must be kSelectedStateURL
	            ProcessSelectedStateURL();

			// The URL has now been processed
            if (GetConnectionStatus() < 0)
            	HandleCurrentUrlError();
			if (GetServerStateParser().LastCommandSuccessful())
				SetCurrentEntryStatus(0);
	        SetConnectionStatus(-1);        // stop netlib

            if (DeathSignalReceived())
            	HandleCurrentUrlError();
        }
        else
        	HandleCurrentUrlError(); 
    }
    else 
    {
		if (!fCurrentUrl->ValidIMAPUrl())
        	AlertUserEvent("Invalid IMAP4 url");
		SetCurrentEntryStatus(-1);
        SetConnectionStatus(-1);
    }  
	
	PseudoInterrupt(FALSE);	// clear this, because we must be done interrupting?
    
    //ProgressUpdateEvent("Current IMAP Url Completed");
}

void TNavigatorImapConnection::FolderHeaderDump(uint32 *msgUids, uint32 msgCount)
{
	FolderMsgDump(msgUids, msgCount, TIMAP4BlockingConnection::kHeadersRFC822andUid);
	
    if (GetServerStateParser().NumberOfMessages())
    {    
        HeaderFetchCompleted();
        
		// if an INBOX exists on this host
		//char *inboxName = TIMAPHostInfo::GetOnlineInboxPathForHost(fCurrentUrl->GetUrlHost());
		char *inboxName = PR_smprintf("INBOX");
		if (inboxName)
		{
	        // if this was the inbox, turn biff off
			if (!XP_STRCASECMP(GetServerStateParser().GetSelectedMailboxName(), inboxName))
			{
				fCurrentBiffState = MSG_BIFF_NoMail;
	    		SendSetBiffIndicatorEvent(fCurrentBiffState);
    		}
			XP_FREE(inboxName);
		}
    }
}

void TNavigatorImapConnection::ShowProgress()
{
	if (fProgressStringId)
	{
		char *progressString = NULL;
		const char *mailboxName = GetServerStateParser().GetSelectedMailboxName();
		progressString = PR_sprintf_append(progressString, XP_GetString(fProgressStringId), (mailboxName) ? mailboxName : "", ++fProgressIndex, fProgressCount);
		if (progressString)
			PercentProgressUpdateEvent(progressString,(100*(fProgressIndex))/fProgressCount );
		FREEIF(progressString);
	}
}

void TNavigatorImapConnection::FolderMsgDump(uint32 *msgUids, uint32 msgCount, TIMAP4BlockingConnection::eFetchFields fields)
{
	switch (fields) {
	case TIMAP4BlockingConnection::kHeadersRFC822andUid:
		fProgressStringId =  XP_RECEIVING_MESSAGE_HEADERS_OF;
		break;
	case TIMAP4BlockingConnection::kFlags:
		fProgressStringId =  XP_RECEIVING_MESSAGE_FLAGS_OF;
		break;
	default:
		fProgressStringId =  XP_FOLDER_RECEIVING_MESSAGE_OF;
		break;
	}
	
	fProgressIndex = 0;
	fProgressCount = msgCount;

	FolderMsgDumpRecurse(msgUids, msgCount, fields);
	
	fProgressStringId = 0;
}

uint32 TNavigatorImapConnection::CountMessagesInIdString(const char *idString)
{
	uint32 numberOfMessages = 0;
	char *uidString = XP_STRDUP(idString);

	if (uidString)
	{
		// This is in the form <id>,<id>, or <id1>:<id2>
		char curChar = *uidString;
		XP_Bool isRange = FALSE;
		int32	curToken;
		int32	saveStartToken=0;

		for (char *curCharPtr = uidString; curChar && *curCharPtr;)
		{
			char *currentKeyToken = curCharPtr;
			curChar = *curCharPtr;
			while (curChar != ':' && curChar != ',' && curChar != '\0')
				curChar = *curCharPtr++;
			*(curCharPtr - 1) = '\0';
			curToken = atol(currentKeyToken);
			if (isRange)
			{
				while (saveStartToken < curToken)
				{
					numberOfMessages++;
					saveStartToken++;
				}
			}

			numberOfMessages++;
			isRange = (curChar == ':');
			if (isRange)
				saveStartToken = curToken + 1;
		}
		XP_FREE(uidString);
	}
	return numberOfMessages;
}

char *IMAP_AllocateImapUidString(uint32 *msgUids, uint32 msgCount)
{
	int blocksAllocated = 1;
	char *returnIdString = (char *) XP_ALLOC(256);
	if (returnIdString)
	{
		char *currentidString = returnIdString;
		*returnIdString  = 0;
		
		int32 startSequence = (msgCount > 0) ? msgUids[0] : -1;
		int32 curSequenceEnd = startSequence;
		uint32 total = msgCount;

		for (uint32 keyIndex=0; returnIdString && (keyIndex < total); keyIndex++)
		{
			int32 curKey = msgUids[keyIndex];
			int32 nextKey = (keyIndex + 1 < total) ? msgUids[keyIndex + 1] : -1;
			XP_Bool lastKey = (nextKey == -1);

			if (lastKey)
				curSequenceEnd = curKey;
			if (nextKey == curSequenceEnd + 1 && !lastKey)
			{
				curSequenceEnd = nextKey;
				continue;
			}
			else if (curSequenceEnd > startSequence)
			{
				sprintf(currentidString, "%ld:%ld,", startSequence, curSequenceEnd);
				startSequence = nextKey;
				curSequenceEnd = startSequence;
			}
			else
			{
				startSequence = nextKey;
				curSequenceEnd = startSequence;
				sprintf(currentidString, "%ld,", msgUids[keyIndex]);
			}
			currentidString += XP_STRLEN(currentidString);
			if ((currentidString + 20) > (returnIdString + (blocksAllocated * 256)))
			{
				returnIdString = (char *) XP_REALLOC(returnIdString, ++blocksAllocated*256);
				if (returnIdString)
					currentidString = returnIdString + XP_STRLEN(returnIdString);
			}
		}
	}
	
	if (returnIdString && *returnIdString)
		*(returnIdString + XP_STRLEN(returnIdString) - 1) = 0;	// eat the comma
	
	return returnIdString;
}


void TNavigatorImapConnection::FolderMsgDumpRecurse(uint32 *msgUids, uint32 msgCount, TIMAP4BlockingConnection::eFetchFields fields)
{
   	PastPasswordCheckEvent();

   	if (msgCount <= 200)
   	{
   		char *idString = IMAP_AllocateImapUidString(msgUids, msgCount);	// 20 * 200
   		if (idString)
   		{
			FetchMessage(idString, 
	                     fields,
	                     TRUE);                  // msg ids are uids                 
	        XP_FREE(idString);
		}
	    else
	        HandleMemoryFailure();
   	}
   	else
   	{
   		FolderMsgDumpRecurse(msgUids, 200, fields);
   		FolderMsgDumpRecurse(msgUids + 200, msgCount - 200, fields);
   	}
}   	
   	


void TNavigatorImapConnection::SendSetBiffIndicatorEvent(MSG_BIFF_STATE /*newState*/)
{
    /* TImapFEEvent *biffIndicatorEvent = 
        new TImapFEEvent(SetBiffIndicator,  // function to call
                         (void *) (unsigned long) newState, 
                         this);                  

	if (newState == MSG_BIFF_NewMail)
		fMailToFetch = TRUE;
	else
		fMailToFetch = FALSE;
    if (biffIndicatorEvent)
    {
        fFEEventQueue->AdoptEventToEnd(biffIndicatorEvent);
        WaitForFEEventCompletion();
    }
    else
        HandleMemoryFailure(); */
}



// We get called to see if there is mail waiting for us at the server, even if it may have been
// read elsewhere. We just want to know if we should download headers or not.

XP_Bool TNavigatorImapConnection::CheckNewMail()
{
	if (!fGetHeaders)
		return FALSE;		// use biff instead
	return TRUE;
	/* if (!fMailToFetch && fNewMail) {		// This was the old style biff, we don't want this anymore
		fMailToFetch = TRUE;
		if (fCurrentBiffState != MSG_BIFF_NewMail)
		{
			fCurrentBiffState = MSG_BIFF_NewMail;
			//SendSetBiffIndicatorEvent(fCurrentBiffState); deadlock
		}
	}
	return fMailToFetch; */
}




// Use the noop to tell the server we are still here, and therefore we are willing to receive
// status updates. The recent or exists response from the server could tell us that there is
// more mail waiting for us, but we need to check the flags of the mail and the high water mark
// to make sure that we do not tell the user that there is new mail when perhaps they have
// already read it in another machine.

void TNavigatorImapConnection::PeriodicBiff()
{
	
	MSG_BIFF_STATE startingState = fCurrentBiffState;
	
	//if (fCurrentBiffState == MSG_BIFF_NewMail)	// dont do this since another computer could be used to read messages
	//	return;
	if (!fFlagState)
		return;
	if (GetServerStateParser().GetIMAPstate() == TImapServerState::kFolderSelected)
    {
		Noop();	// check the latest number of messages
        if (GetServerStateParser().NumberOfMessages() != fFlagState->GetNumberOfMessages())
        {
			uint32 id = GetServerStateParser().HighestRecordedUID() + 1;
			char fetchStr[100];						// only update flags
			uint32 added = 0, deleted = 0;
			
			deleted = fFlagState->GetNumberOfDeletedMessages();
			added = fFlagState->GetNumberOfMessages();
			if (!added || (added == deleted))	// empty keys, get them all
				id = 1;

			//sprintf(fetchStr, "%ld:%ld", id, id + GetServerStateParser().NumberOfMessages() - fFlagState->GetNumberOfMessages());
			sprintf(fetchStr, "%ld:*", id);
			FetchMessage(fetchStr, TIMAP4BlockingConnection::kFlags, TRUE);

			if (fFlagState && ((uint32) fFlagState->GetHighestNonDeletedUID() >= id) && fFlagState->IsLastMessageUnseen())
				fCurrentBiffState = MSG_BIFF_NewMail;
			else
				fCurrentBiffState = MSG_BIFF_NoMail;
        }
        else
            fCurrentBiffState = MSG_BIFF_NoMail;
    }
    else
    	fCurrentBiffState = MSG_BIFF_Unknown;
    
    if (startingState != fCurrentBiffState)
    	SendSetBiffIndicatorEvent(fCurrentBiffState);
}

void TNavigatorImapConnection::HandleCurrentUrlError()
{
	if (fCurrentUrl->GetIMAPurlType() == TIMAPUrl::kSelectFolder)
	{
		// let the front end know the select failed so they
		// don't leave the view without a database.
		mailbox_spec *notSelectedSpec = (mailbox_spec *) XP_CALLOC(1, sizeof( mailbox_spec));
		if (notSelectedSpec)
		{
			notSelectedSpec->allocatedPathName = fCurrentUrl->CreateCanonicalSourceFolderPathString();
			notSelectedSpec->hostName = fCurrentUrl->GetUrlHost();
			notSelectedSpec->folderSelected = FALSE;
			notSelectedSpec->flagState = NULL;
			notSelectedSpec->onlineVerified = FALSE;
			UpdatedMailboxSpec(notSelectedSpec);
		}
	}
}

XP_Bool TNavigatorImapConnection::RenameHierarchyByHand(const char *oldParentMailboxName, const char *newParentMailboxName)
{
	XP_Bool renameSucceeded = TRUE;
	fDeletableChildren = XP_ListNew();

	XP_Bool nonHierarchicalRename = ((GetServerStateParser().GetCapabilityFlag() & kNoHierarchyRename)
									|| MailboxIsNoSelectMailbox(oldParentMailboxName));

	if (fDeletableChildren)
	{
		fHierarchyNameState = kDeleteSubFoldersInProgress;
		TIMAPNamespace *ns = TIMAPHostInfo::GetNamespaceForMailboxForHost(fCurrentUrl->GetUrlHost(), oldParentMailboxName);	// for delimiter
		if (!ns)
		{
			if (!XP_STRCASECMP(oldParentMailboxName, "INBOX"))
				ns = TIMAPHostInfo::GetDefaultNamespaceOfTypeForHost(fCurrentUrl->GetUrlHost(), kPersonalNamespace);
		}
		if (ns)
		{
			char *pattern = PR_smprintf("%s%c*", oldParentMailboxName,ns->GetDelimiter());
			if (pattern)
			{
				if (TIMAPHostInfo::GetHostIsUsingSubscription(fCurrentUrl->GetUrlHost()))
					Lsub(pattern);
				else
					List(pattern);
				XP_FREE(pattern);
			}
		}
		fHierarchyNameState = kNoOperationInProgress;
		
		if (GetServerStateParser().LastCommandSuccessful())
			renameSucceeded = RenameMailboxRespectingSubscriptions(oldParentMailboxName,newParentMailboxName, TRUE);	// rename this, and move subscriptions
		
		int numberToDelete = XP_ListCount(fDeletableChildren);
		
		for (int childIndex = 1; (childIndex <= numberToDelete) && renameSucceeded; childIndex++)
		{
			// the imap parser has already converted to a non UTF7 string in the canonical
			// format so convert it back
			char *currentName = (char *) XP_ListGetObjectNum(fDeletableChildren, childIndex);
		    if (currentName)
		    {
		    	char *serverName = fCurrentUrl->AllocateServerPath(currentName);
		    	char *convertedName = serverName ? CreateUtf7ConvertedString(serverName, TRUE) : (char *)NULL;
		    	FREEIF(serverName);
		    	currentName = convertedName;	// currentName not leaked, deleted in XP_ListDestroy
		    }
		    
		    // calculate the new name and do the rename
		    char *newChildName = (char *) XP_ALLOC(XP_STRLEN(currentName) + XP_STRLEN(newParentMailboxName) + 1);
		    if (newChildName)
		    {
		    	XP_STRCPY(newChildName, newParentMailboxName);
		    	XP_STRCAT(newChildName, currentName + XP_STRLEN(oldParentMailboxName));
		    	RenameMailboxRespectingSubscriptions(currentName,newChildName, nonHierarchicalRename);	// pass in xNonHierarchicalRename to
																										// determine if we should really reanme,
																										// or just move subscriptions
				renameSucceeded = GetServerStateParser().LastCommandSuccessful();
				XP_FREE(newChildName);
		    }
		    FREEIF(currentName);
		}
		
		XP_ListDestroy(fDeletableChildren);
		fDeletableChildren = NULL;
	}
	return renameSucceeded;
}

XP_Bool TNavigatorImapConnection::DeleteSubFolders(const char *selectedMailbox)
{
	XP_Bool deleteSucceeded = TRUE;
	fDeletableChildren = XP_ListNew();
	
	if (fDeletableChildren)
	{
		fHierarchyNameState = kDeleteSubFoldersInProgress;
		char *pattern = PR_smprintf("%s%c*", selectedMailbox, fCurrentUrl->GetOnlineSubDirSeparator());
		if (pattern)
		{
			List(pattern);
			XP_FREE(pattern);
		}
		fHierarchyNameState = kNoOperationInProgress;
		
		// this should be a short list so perform a sequential search for the
		// longest name mailbox.  Deleting the longest first will hopefully prevent the server
		// from having problems about deleting parents
		int numberToDelete = XP_ListCount(fDeletableChildren);
		
		deleteSucceeded = GetServerStateParser().LastCommandSuccessful();
		for (int outerIndex = 1; (outerIndex <= numberToDelete) && deleteSucceeded; outerIndex++)
		{
			char *longestName = NULL;
			for (int innerIndex = 1; innerIndex <= XP_ListCount(fDeletableChildren); innerIndex++)
			{
				char *currentName = (char *) XP_ListGetObjectNum(fDeletableChildren, innerIndex);
				if (!longestName || (XP_STRLEN(longestName) < XP_STRLEN(currentName) ) )
					longestName = currentName;
			}
			XP_ASSERT(longestName);
			XP_ListRemoveObject(fDeletableChildren, longestName);
			
			// the imap parser has already converted to a non UTF7 string in the canonical
			// format so convert it back
		    if (longestName)
		    {
		    	char *serverName = fCurrentUrl->AllocateServerPath(longestName);
		    	char *convertedName = serverName ? CreateUtf7ConvertedString(serverName, TRUE) : 0;
		    	FREEIF(serverName);
		    	XP_FREE(longestName);
		    	longestName = convertedName;
		    }
			
			// some imap servers include the selectedMailbox in the list of 
			// subfolders of the selectedMailbox.  Check for this so we don't delete
			// the selectedMailbox (usually the trash and doing an empty trash)
			
			// The Cyrus imap server ignores the "INBOX.Trash" constraining string passed
			// to the list command.  Be defensive and make sure we only delete children of the trash
			if (longestName && 
				XP_STRCMP(selectedMailbox, longestName) &&
				!XP_STRNCMP(selectedMailbox, longestName, XP_STRLEN(selectedMailbox)))
			{	
	            XP_Bool deleted = DeleteMailboxRespectingSubscriptions(longestName);
				if (deleted)
					FolderDeleted(longestName);
				deleteSucceeded = deleted;
			}
			FREEIF(longestName);
		}
	
		XP_ListDestroy(fDeletableChildren);
		fDeletableChildren = NULL;
	}
	return deleteSucceeded;
}


void TNavigatorImapConnection::ProcessMailboxUpdate(XP_Bool handlePossibleUndo, XP_Bool /*fromBiffUpdate*/)
{
	if (DeathSignalReceived())
		return;
    // fetch the flags and uids of all existing messages or new ones
    if (!DeathSignalReceived() && GetServerStateParser().NumberOfMessages())
    {
    	if (handlePossibleUndo)
    	{
	    	// undo any delete flags we may have asked to
	    	char *undoIds = fCurrentUrl->CreateListOfMessageIdsString();
	    	if (undoIds && *undoIds)
	    	{
	    		// if this string started with a '-', then this is an undo of a delete
	    		// if its a '+' its a redo
	    		if (*undoIds == '-')
					Store(undoIds+1, "-FLAGS (\\Deleted)", TRUE);	// most servers will fail silently on a failure, deal with it?
	    		else  if (*undoIds == '+')
					Store(undoIds+1, "+FLAGS (\\Deleted)", TRUE);	// most servers will fail silently on a failure, deal with it?
				else 
					XP_ASSERT(FALSE);
			}
			FREEIF(undoIds);
		}
    	
		if (!DeathSignalReceived())	// only expunge if not reading messages manually and before fetching new
        {
            if (fFlagState && (fFlagState->GetNumberOfDeletedMessages() >= 40) && fDeleteModelIsMoveToTrash)
				Expunge();	// might be expensive, test for user cancel
        }

        // make the parser record these flags
		char fetchStr[100];
		uint32 added = 0, deleted = 0;

		if (fFlagState)
		{
			added = fFlagState->GetNumberOfMessages();
			deleted = fFlagState->GetNumberOfDeletedMessages();
		}
		if (fFlagState  && (!added || (added == deleted)))
			FetchMessage("1:*", TIMAP4BlockingConnection::kFlags, TRUE);	// id string shows uids
		else {
			sprintf(fetchStr, "%ld:*", GetServerStateParser().HighestRecordedUID() + 1);
			FetchMessage(fetchStr, TIMAP4BlockingConnection::kFlags, TRUE);			// only new messages please
		}
    }
    else if (!DeathSignalReceived())
    	GetServerStateParser().ResetFlagInfo(0);
        
    mailbox_spec *new_spec = GetServerStateParser().CreateCurrentMailboxSpec();
	if (new_spec && !DeathSignalReceived())
	{
		MWContext *ct = NULL;
		
		LIBNET_LOCK();
		if (!DeathSignalReceived())
		{
			// if this is an expunge url, libmsg will not ask for headers
			if (fCurrentUrl->GetIMAPurlType() == TIMAPUrl::kExpungeFolder)
				new_spec->box_flags |= kJustExpunged;
				
			ct = new_spec->connection->fCurrentEntry->window_id;
			ct->imapURLPane = MSG_FindPane(fCurrentEntry->window_id, MSG_ANYPANE);
		}
		LIBNET_UNLOCK();
		if (ct)
		{
			if (!ct->currentIMAPfolder)
				ct->currentIMAPfolder = (MSG_IMAPFolderInfoMail *) MSG_FindImapFolder(ct->imapURLPane, fCurrentUrl->GetUrlHost(), "INBOX"); // use real folder name
			/* MSG_IMAPFolderInfoMail *imapInbox;
			if (urlFolder)
				imapInbox = URL_s->msg_pane->GetMaster()->FindImapMailFolder(urlFolder->GetHostName(), "INBOX");
			else
				imapInbox = URL_s->msg_pane->GetMaster()->FindImapMailFolder("INBOX");
			if (imapInbox)
				imapInbox->NotifyFolderLoaded(URL_s->msg_pane);
			 */
			PR_EnterMonitor(fWaitForBodyIdsMonitor);
			UpdatedMailboxSpec(new_spec);
		}
	}
	else if (!new_spec)
		HandleMemoryFailure();
    
    // Block until libmsg decides whether to download headers or not.
    uint32 *msgIdList;
    uint32 msgCount = 0;
    
	if (!DeathSignalReceived())
	{
		WaitForPotentialListOfMsgsToFetch(&msgIdList, msgCount);

		if (new_spec)
			PR_ExitMonitor(fWaitForBodyIdsMonitor);

		if (msgIdList && !DeathSignalReceived() && GetServerStateParser().LastCommandSuccessful())
		{
			FolderHeaderDump(msgIdList, msgCount);
			FREEIF( msgIdList);
		}
			// this might be bogus, how are we going to do pane notification and stuff when we fetch bodies without
			// headers!
	}
    // wait for a list of bodies to fetch. 
    if (!DeathSignalReceived() && GetServerStateParser().LastCommandSuccessful())
    {
        WaitForPotentialListOfMsgsToFetch(&msgIdList, msgCount);
        if ( msgCount && !DeathSignalReceived() && GetServerStateParser().LastCommandSuccessful())
    	{
    		FolderMsgDump(msgIdList, msgCount, TIMAP4BlockingConnection::kEveryThingRFC822Peek);
    		FREEIF(msgIdList);
    	}
	}
	if (DeathSignalReceived())
		GetServerStateParser().ResetFlagInfo(0);
}


void TNavigatorImapConnection::ProcessSelectedStateURL()
{
    char *mailboxName = fCurrentUrl->CreateServerSourceFolderPathString();
    if (mailboxName)
    {
    	char *convertedName = CreateUtf7ConvertedString(mailboxName, TRUE);
    	XP_FREE(mailboxName);
    	mailboxName = convertedName;
    }
    
    if (mailboxName && !DeathSignalReceived())
    {
		//char *inboxName = TIMAPHostInfo::GetOnlineInboxPathForHost(fCurrentUrl->GetUrlHost());
		char *inboxName = PR_smprintf("INBOX");
		TInboxReferenceCount inboxCounter(inboxName ? !XP_STRCASECMP(mailboxName, inboxName) : FALSE);
		FREEIF(inboxName);
    	
    	if ( (fCurrentUrl->GetIMAPurlType() == TIMAPUrl::kBiff) &&
    		 (inboxCounter.GetInboxUsageCount() > 1))
    	{
    		FREEIF(mailboxName);
    		return;
    	}
    	
    	XP_Bool selectIssued = FALSE;
        if (GetServerStateParser().GetIMAPstate() == TImapServerState::kFolderSelected)
        {
            if (GetServerStateParser().GetSelectedMailboxName() && 
                XP_STRCMP(GetServerStateParser().GetSelectedMailboxName(),
                          mailboxName))
            {       // we are selected in another folder
            	if (fCloseNeededBeforeSelect)
                	Close();
                if (GetServerStateParser().LastCommandSuccessful()) 
                {
                	selectIssued = TRUE;
					AutoSubscribeToMailboxIfNecessary(mailboxName);
                    SelectMailbox(mailboxName);
                }
            }
            else if (!GetServerStateParser().GetSelectedMailboxName())
            {       // why are we in the selected state with no box name?
                SelectMailbox(mailboxName);
                selectIssued = TRUE;
            }
            else
            {
            	// get new message counts, if any, from server
            	ProgressEventFunction_UsingId (MK_IMAP_STATUS_SELECTING_MAILBOX);
				if (fNeedNoop)
				{
            		Noop();	// I think this is needed when we're using a cached connection
					fNeedNoop = FALSE;
				}
            }
        }
        else
        {
            // go to selected state
			AutoSubscribeToMailboxIfNecessary(mailboxName);
            SelectMailbox(mailboxName);
            selectIssued = TRUE;
        }

		if (selectIssued)
		{
			RefreshACLForFolderIfNecessary(mailboxName);
		}
        
        XP_Bool uidValidityOk = TRUE;
        if (GetServerStateParser().LastCommandSuccessful() && selectIssued && 
           (fCurrentUrl->GetIMAPurlType() != TIMAPUrl::kSelectFolder) && (fCurrentUrl->GetIMAPurlType() != TIMAPUrl::kLiteSelectFolder))
        {
        	uid_validity_info *uidStruct = (uid_validity_info *) XP_ALLOC(sizeof(uid_validity_info));
        	if (uidStruct)
        	{
        		uidStruct->returnValidity = kUidUnknown;
				uidStruct->hostName = fCurrentUrl->GetUrlHost();
        		uidStruct->canonical_boxname = fCurrentUrl->CreateCanonicalSourceFolderPathString();
			    TImapFEEvent *endEvent = 
			        new TImapFEEvent(GetStoredUIDValidity,    // function to call
			                        this,                     // access to current entry/context
			                        (void *) uidStruct,
									FALSE);       // retain storage ownership here

			    if (endEvent)
			    {
			        fFEEventQueue->AdoptEventToEnd(endEvent);
			        WaitForFEEventCompletion();
			        
			        // error on the side of caution, if the fe event fails to set uidStruct->returnValidity, then assume that UIDVALIDITY
			        // did not role.  This is a common case event for attachments that are fetched within a browser context.
					if (!DeathSignalReceived())
						uidValidityOk = (uidStruct->returnValidity == kUidUnknown) || (uidStruct->returnValidity == GetServerStateParser().FolderUID());
			    }
			    else
			        HandleMemoryFailure();
			    
			    FREEIF(uidStruct->canonical_boxname);
			    XP_FREE(uidStruct);
        	}
        	else
        		HandleMemoryFailure();
        }
            
        if (GetServerStateParser().LastCommandSuccessful() && !DeathSignalReceived() && (uidValidityOk || fCurrentUrl->GetIMAPurlType() == TIMAPUrl::kDeleteAllMsgs))
        {
        	if (fCurrentUrl->GetIMAPurlType() == TIMAPUrl::kLiteSelectFolder)
        	{
				if (GetServerStateParser().LastCommandSuccessful())
				{
					TImapFEEvent *liteselectEvent = 
						new TImapFEEvent(LiteSelectEvent,       // function to call
										 (void *) this,
										 NULL, TRUE);

					if (liteselectEvent)
					{
						fFEEventQueue->AdoptEventToEnd(liteselectEvent);
						WaitForFEEventCompletion();
					}
					else
						HandleMemoryFailure();
					// need to update the mailbox count - is this a good place?
					ProcessMailboxUpdate(FALSE);	// handle uidvalidity change
				}
            }
            else if (fCurrentUrl->GetIMAPurlType() == TIMAPUrl::kMsgFetch)
            {
                char *messageIdString =  
                    fCurrentUrl->CreateListOfMessageIdsString();
                if (messageIdString)
                {
					// we dont want to send the flags back in a group
					// GetServerStateParser().ResetFlagInfo(0);
					if (HandlingMultipleMessages(messageIdString))
					{
						// multiple messages, fetch them all
	                    FetchMessage(messageIdString, 
									 TIMAP4BlockingConnection::kEveryThingRFC822Peek,
									 fCurrentUrl->MessageIdsAreUids());
					}
					else
					{
						// A single message ID

						// First, let's see if we're requesting a specific MIME part
						char *imappart = fCurrentUrl->GetIMAPPartToFetch();
						if (imappart)
						{
							if (fCurrentUrl->MessageIdsAreUids())
							{
								// We actually want a specific MIME part of the message.
								// The Body Shell will generate it, even though we haven't downloaded it yet.

								TIMAPBodyShell *foundShell = TIMAPHostInfo::FindShellInCacheForHost(fCurrentUrl->GetUrlHost(),
									GetServerStateParser().GetSelectedMailboxName(), messageIdString);
								if (!foundShell)
								{
									// The shell wasn't in the cache.  Deal with this case later.
									Log("SHELL",NULL,"Loading part, shell not found in cache!");
									//PR_LOG(IMAP, out, ("BODYSHELL: Loading part, shell not found in cache!"));
									// The parser will extract the part number from the current URL.
									Bodystructure(messageIdString, fCurrentUrl->MessageIdsAreUids());
								}
								else
								{
									Log("SHELL", NULL, "Loading Part, using cached shell.");
									//PR_LOG(IMAP, out, ("BODYSHELL: Loading part, using cached shell."));
									foundShell->SetConnection(this);
									GetServerStateParser().UseCachedShell(foundShell);
									foundShell->Generate(imappart);
									GetServerStateParser().UseCachedShell(NULL);
								}
							}
							else
							{
								// Message IDs are not UIDs.
								XP_ASSERT(FALSE);
							}
							XP_FREE(imappart);
						}
						else
						{
							// downloading a single message: try to do it by bodystructure, and/or do it by chunks
							uint32 messageSize = GetMessageSize(messageIdString,
								fCurrentUrl->MessageIdsAreUids());
							
							// We need to check the format_out bits to see if we are allowed to leave out parts,
							// or if we are required to get the whole thing.  Some instances where we are allowed
							// to do it by parts:  when viewing a message, or its source
							// Some times when we're NOT allowed:  when forwarding a message, saving it, moving it, etc.
							ActiveEntry *ce = GetActiveEntry();
							XP_Bool allowedToBreakApart = (ce  && !DeathSignalReceived()) ? ce->URL_s->allow_content_change : FALSE;

							if (gMIMEOnDemand &&
								allowedToBreakApart && 
								!GetShouldFetchAllParts() &&
								GetServerStateParser().ServerHasIMAP4Rev1Capability() &&
								(messageSize > (uint32) gMIMEOnDemandThreshold) &&
								!fCurrentUrl->MimePartSelectorDetected())	// if a ?part=, don't do BS.
							{
								// OK, we're doing bodystructure

								// Before fetching the bodystructure, let's check our body shell cache to see if
								// we already have it around.
								TIMAPBodyShell *foundShell = NULL;
								SetContentModified(TRUE);  // This will be looked at by the cache
								if (fCurrentUrl->MessageIdsAreUids())
								{
									foundShell = TIMAPHostInfo::FindShellInCacheForHost(fCurrentUrl->GetUrlHost(),
										GetServerStateParser().GetSelectedMailboxName(), messageIdString);
									if (foundShell)
									{
										Log("SHELL",NULL,"Loading message, using cached shell.");
										//PR_LOG(IMAP, out, ("BODYSHELL: Loading message, using cached shell."));
										foundShell->SetConnection(this);
										GetServerStateParser().UseCachedShell(foundShell);
										foundShell->Generate(NULL);
										GetServerStateParser().UseCachedShell(NULL);
									}
								}

								if (!foundShell)
									Bodystructure(messageIdString, fCurrentUrl->MessageIdsAreUids());
							}
							else
							{
								// Not doing bodystructure.  Fetch the whole thing, and try to do
								// it by parts.
								SetContentModified(FALSE);
								FetchTryChunking(messageIdString, TIMAP4BlockingConnection::kEveryThingRFC822,
									fCurrentUrl->MessageIdsAreUids(), NULL, messageSize);
							}
						}
				
					}
                    FREEIF( messageIdString);
                }
                else
                    HandleMemoryFailure();
            }
            else if ((fCurrentUrl->GetIMAPurlType() == TIMAPUrl::kSelectFolder) || (fCurrentUrl->GetIMAPurlType() == TIMAPUrl::kExpungeFolder))
            {
            	if (fCurrentUrl->GetIMAPurlType() == TIMAPUrl::kExpungeFolder)
            		Expunge();
            	ProcessMailboxUpdate(TRUE);
            }
            else if (fCurrentUrl->GetIMAPurlType() == TIMAPUrl::kMsgHeader)
            {
                char *messageIdString =  
                    fCurrentUrl->CreateListOfMessageIdsString();
                if (messageIdString)
                {
                    // we don't want to send the flags back in a group
            //        GetServerStateParser().ResetFlagInfo(0);
                    FetchMessage(messageIdString, 
                                 TIMAP4BlockingConnection::kHeadersRFC822andUid,
                                 fCurrentUrl->MessageIdsAreUids());
                    FREEIF( messageIdString);
                }
                else
                    HandleMemoryFailure();
            }
            else if (fCurrentUrl->GetIMAPurlType() == TIMAPUrl::kSearch)
            {
                char *searchCriteriaString = 
                    fCurrentUrl->CreateSearchCriteriaString();
                if (searchCriteriaString)
                {
                    Search(searchCriteriaString,fCurrentUrl->MessageIdsAreUids());
                        // drop the results on the floor for now
                    FREEIF( searchCriteriaString);
                }
                else
                    HandleMemoryFailure();
            }
            else if (fCurrentUrl->GetIMAPurlType() == TIMAPUrl::kDeleteMsg)
            {
                char *messageIdString = 
                    fCurrentUrl->CreateListOfMessageIdsString();
                        
                if (messageIdString)
                {
					if (HandlingMultipleMessages(messageIdString))
						ProgressEventFunction_UsingId (XP_IMAP_DELETING_MESSAGES);
					else
						ProgressEventFunction_UsingId(XP_IMAP_DELETING_MESSAGE);
                    Store(messageIdString, "+FLAGS (\\Deleted)", 
                          fCurrentUrl->MessageIdsAreUids());
                    
                    if (GetServerStateParser().LastCommandSuccessful())
                    {
                    	struct delete_message_struct *deleteMsg = (struct delete_message_struct *) XP_ALLOC (sizeof(struct delete_message_struct));

						// convert name back from utf7
						utf_name_struct *nameStruct = (utf_name_struct *) XP_ALLOC(sizeof(utf_name_struct));
						char *convertedCanonicalName = NULL;
						if (nameStruct)
						{
    						nameStruct->toUtf7Imap = FALSE;
							nameStruct->sourceString = (unsigned char *) GetServerStateParser().GetSelectedMailboxName();
							nameStruct->convertedString = NULL;
							ConvertImapUtf7(nameStruct, NULL);
							if (nameStruct->convertedString)
								convertedCanonicalName = fCurrentUrl->AllocateCanonicalPath((char *) nameStruct->convertedString);
						}

						deleteMsg->onlineFolderName = convertedCanonicalName;
						deleteMsg->deleteAllMsgs = FALSE;
                    	deleteMsg->msgIdString   = messageIdString;	// storage adopted, do not delete
                    	messageIdString = nil;		// deleting nil is ok
                    	
					    TImapFEEvent *deleteEvent = 
					        new TImapFEEvent(NotifyMessageDeletedEvent,       // function to call
					                        (void *) this,
					                        (void *) deleteMsg, TRUE);

					    if (deleteEvent)
					        fFEEventQueue->AdoptEventToEnd(deleteEvent);
					    else
					        HandleMemoryFailure();
                    }
                    FREEIF( messageIdString);
                }
                else
                    HandleMemoryFailure();
            }
            else if (fCurrentUrl->GetIMAPurlType() == TIMAPUrl::kDeleteAllMsgs)
            {
            	uint32 numberOfMessages = GetServerStateParser().NumberOfMessages();
            	if (numberOfMessages)
				{
					char messageIdString[100];	// enough for bazillion msgs
					sprintf(messageIdString, "1:*");
                    
					Store(messageIdString, "+FLAGS (\\Deleted)", FALSE);	// use sequence #'s 
                
					if (GetServerStateParser().LastCommandSuccessful())
						Expunge();      // expunge messages with deleted flag
					if (GetServerStateParser().LastCommandSuccessful())
					{
						struct delete_message_struct *deleteMsg = (struct delete_message_struct *) XP_ALLOC (sizeof(struct delete_message_struct));

						// convert name back from utf7
						utf_name_struct *nameStruct = (utf_name_struct *) XP_ALLOC(sizeof(utf_name_struct));
						char *convertedCanonicalName = NULL;
						if (nameStruct)
						{
    						nameStruct->toUtf7Imap = FALSE;
							nameStruct->sourceString = (unsigned char *) GetServerStateParser().GetSelectedMailboxName();
							nameStruct->convertedString = NULL;
							ConvertImapUtf7(nameStruct, NULL);
							if (nameStruct->convertedString)
								convertedCanonicalName = fCurrentUrl->AllocateCanonicalPath((char *) nameStruct->convertedString);
						}

						deleteMsg->onlineFolderName = convertedCanonicalName;
						deleteMsg->deleteAllMsgs = TRUE;
						deleteMsg->msgIdString   = nil;

						TImapFEEvent *deleteEvent = 
							new TImapFEEvent(NotifyMessageDeletedEvent,       // function to call
											(void *) this,
											(void *) deleteMsg, TRUE);

						if (deleteEvent)
							fFEEventQueue->AdoptEventToEnd(deleteEvent);
						else
							HandleMemoryFailure();
					}
                
				}
                DeleteSubFolders(mailboxName);
            }
			else if (fCurrentUrl->GetIMAPurlType() == TIMAPUrl::kAppendMsgFromFile)
			{
				char *sourceMessageFile =
					XP_STRDUP(GetActiveEntry()->URL_s->post_data);
				char *mailboxName =  
					fCurrentUrl->CreateServerSourceFolderPathString();
				
				if (mailboxName)
				{
					char *convertedName = CreateUtf7ConvertedString(mailboxName, TRUE);
					XP_FREE(mailboxName);
					mailboxName = convertedName;
				}
				if (mailboxName)
				{
					UploadMessageFromFile(sourceMessageFile, mailboxName,
										  kImapMsgSeenFlag);
				}
				else
					HandleMemoryFailure();
				FREEIF( sourceMessageFile );
				FREEIF( mailboxName );
			}
            else if (fCurrentUrl->GetIMAPurlType() == TIMAPUrl::kAddMsgFlags)
            {
                char *messageIdString = 
                    fCurrentUrl->CreateListOfMessageIdsString();
                        
                if (messageIdString)
                {       
                    ProcessStoreFlags(messageIdString, fCurrentUrl->MessageIdsAreUids(),
                                      fCurrentUrl->GetMsgFlags(), TRUE);
                    
                    FREEIF( messageIdString);
                    /*
                    if ( !DeathSignalReceived() &&
                    	  GetServerStateParser().Connected() &&
                         !GetServerStateParser().SyntaxError())
                    {
                        //if (fCurrentUrl->GetMsgFlags() & kImapMsgDeletedFlag)
                        //    Expunge();      // expunge messages with deleted flag
                        Check();        // flush servers flag state
                    }
                    */
                }
                else
                    HandleMemoryFailure();
            }
            else if (fCurrentUrl->GetIMAPurlType() == TIMAPUrl::kSubtractMsgFlags)
            {
                char *messageIdString = 
                    fCurrentUrl->CreateListOfMessageIdsString();
                        
                if (messageIdString)
                {
                    ProcessStoreFlags(messageIdString, fCurrentUrl->MessageIdsAreUids(),
                                      fCurrentUrl->GetMsgFlags(), FALSE);
                    
                    FREEIF( messageIdString);
                }
                else
                    HandleMemoryFailure();
            }
            else if (fCurrentUrl->GetIMAPurlType() == TIMAPUrl::kSetMsgFlags)
            {
                char *messageIdString = 
                    fCurrentUrl->CreateListOfMessageIdsString();
                        
                if (messageIdString)
                {
                    ProcessStoreFlags(messageIdString, fCurrentUrl->MessageIdsAreUids(),
                                      fCurrentUrl->GetMsgFlags(), TRUE);
                    ProcessStoreFlags(messageIdString, fCurrentUrl->MessageIdsAreUids(),
                                      ~fCurrentUrl->GetMsgFlags(), FALSE);
                    
                    FREEIF( messageIdString);
                }
                else
                    HandleMemoryFailure();
            }
            else if (fCurrentUrl->GetIMAPurlType() == TIMAPUrl::kBiff)
            {
                PeriodicBiff(); 
            }
            else if ((fCurrentUrl->GetIMAPurlType() == TIMAPUrl::kOnlineCopy) ||
                 (fCurrentUrl->GetIMAPurlType() == TIMAPUrl::kOnlineMove))
            {
                char *messageIdString = 
                    fCurrentUrl->CreateListOfMessageIdsString();
                char *destinationMailbox = 
                    fCurrentUrl->CreateServerDestinationFolderPathString();
			    if (destinationMailbox)
			    {
			    	char *convertedName = CreateUtf7ConvertedString(destinationMailbox, TRUE);
			    	XP_FREE(destinationMailbox);
			    	destinationMailbox = convertedName;
			    }

                if (messageIdString && destinationMailbox)
                {
					if (fCurrentUrl->GetIMAPurlType() == TIMAPUrl::kOnlineMove) {
						if (HandlingMultipleMessages(messageIdString))
							ProgressEventFunction_UsingIdWithString (XP_IMAP_MOVING_MESSAGES_TO, destinationMailbox);
						else
							ProgressEventFunction_UsingIdWithString (XP_IMAP_MOVING_MESSAGE_TO, destinationMailbox); 
					}
					else {
						if (HandlingMultipleMessages(messageIdString))
							ProgressEventFunction_UsingIdWithString (XP_IMAP_COPYING_MESSAGES_TO, destinationMailbox);
						else
							ProgressEventFunction_UsingIdWithString (XP_IMAP_COPYING_MESSAGE_TO, destinationMailbox); 
					}

                    Copy(messageIdString, destinationMailbox, 
                         fCurrentUrl->MessageIdsAreUids());
                    FREEIF( destinationMailbox);
					ImapOnlineCopyState copyState;
					if (DeathSignalReceived())
						copyState = kInterruptedState;
					else
						copyState = GetServerStateParser().LastCommandSuccessful() ? 
                                kSuccessfulCopy : kFailedCopy;
                    OnlineCopyCompleted(copyState);
                    
                    if (GetServerStateParser().LastCommandSuccessful() &&
                        (fCurrentUrl->GetIMAPurlType() == TIMAPUrl::kOnlineMove))
                    {
                        Store(messageIdString, "+FLAGS (\\Deleted)", 
                              fCurrentUrl->MessageIdsAreUids());
                        
						XP_Bool storeSuccessful = GetServerStateParser().LastCommandSuccessful();
                        	
                        OnlineCopyCompleted( storeSuccessful ? kSuccessfulDelete : kFailedDelete);
                    }
                    FREEIF( messageIdString);
                }
                else
                    HandleMemoryFailure();
            }
            else if ((fCurrentUrl->GetIMAPurlType() == TIMAPUrl::kOnlineToOfflineCopy) ||
                 (fCurrentUrl->GetIMAPurlType() == TIMAPUrl::kOnlineToOfflineMove))
            {
                char *messageIdString =  
                    fCurrentUrl->CreateListOfMessageIdsString();
                if (messageIdString)
                {
					fProgressStringId =  XP_FOLDER_RECEIVING_MESSAGE_OF;
					fProgressIndex = 0;
					fProgressCount = CountMessagesInIdString(messageIdString);
					
					FetchMessage(messageIdString, 
                                 TIMAP4BlockingConnection::kEveryThingRFC822Peek,
                                 fCurrentUrl->MessageIdsAreUids());
                      
                    fProgressStringId = 0;           
                    OnlineCopyCompleted(
                        GetServerStateParser().LastCommandSuccessful() ? 
                                kSuccessfulCopy : kFailedCopy);

                    if (GetServerStateParser().LastCommandSuccessful() &&
                        (fCurrentUrl->GetIMAPurlType() == TIMAPUrl::kOnlineToOfflineMove))
                    {
                        Store(messageIdString, "+FLAGS (\\Deleted)", 
                              fCurrentUrl->MessageIdsAreUids());
						XP_Bool storeSuccessful = GetServerStateParser().LastCommandSuccessful();

                        OnlineCopyCompleted( storeSuccessful ? kSuccessfulDelete : kFailedDelete);
                    }
                    
                    FREEIF( messageIdString);
                }
                else
                    HandleMemoryFailure();
            }
        }
        else if (GetServerStateParser().LastCommandSuccessful() && !uidValidityOk)
        	ProcessMailboxUpdate(FALSE);	// handle uidvalidity change
        
        FREEIF( mailboxName);
    }
    else if (!DeathSignalReceived())
        HandleMemoryFailure();
    
}

void TNavigatorImapConnection::AutoSubscribeToMailboxIfNecessary(const char *mailboxName)
{
	if (fFolderNeedsSubscribing)	// we don't know about this folder - we need to subscribe to it / list it.
	{
		fHierarchyNameState = kListingForInfoOnly;
		List(mailboxName);
		fHierarchyNameState = kNoOperationInProgress;

		// removing and freeing it as we go.
		TIMAPMailboxInfo *mb = NULL;
		int total = XP_ListCount(fListedMailboxList);
		do
		{
			mb = (TIMAPMailboxInfo *) XP_ListRemoveTopObject(fListedMailboxList);
			delete mb;
		} while (mb);

		// if the mailbox exists (it was returned from the LIST response)
		if (total > 0)
		{
			// Subscribe to it, if the pref says so
			if (TIMAPHostInfo::GetHostIsUsingSubscription(fCurrentUrl->GetUrlHost()) && fAutoSubscribeOnOpen)
			{
				Subscribe(mailboxName);
			}

			// Always LIST it anyway, to get it into the folder lists,
			// so that we can continue to perform operations on it, at least
			// for this session.
			fHierarchyNameState = kNoOperationInProgress;
			List(mailboxName);
		}

		// We should now be subscribed to it, and have it in our folder lists
		// and panes.  Even if something failed, we don't want to try this again.
		fFolderNeedsSubscribing = FALSE;

	}
}

void TNavigatorImapConnection::FindMailboxesIfNecessary()
{
    PR_EnterMonitor(fFindingMailboxesMonitor);
    // biff should not discover mailboxes
	XP_Bool foundMailboxesAlready = TIMAPHostInfo::GetHaveWeEverDiscoveredFoldersForHost(fCurrentUrl->GetUrlHost());
    if (!foundMailboxesAlready &&
		(fCurrentUrl->GetIMAPurlType() != TIMAPUrl::kBiff) &&
		(fCurrentUrl->GetIMAPurlType() != TIMAPUrl::kDiscoverAllBoxesUrl) &&
		(fCurrentUrl->GetIMAPurlType() != TIMAPUrl::kUpgradeToSubscription) &&
		!GetSubscribingNow())
    {
		DiscoverMailboxList();

		// If we decide to do it, here is where we should check to see if
		// a namespace exists (personal namespace only?) and possibly
		// create it if it doesn't exist.
	}
    PR_ExitMonitor(fFindingMailboxesMonitor);
}


// DiscoverMailboxList() is used to actually do the discovery of folders
// for a host.  This is used both when we initially start up (and re-sync)
// and also when the user manually requests a re-sync, by collapsing and
// expanding a host in the folder pane.  This is not used for the subscribe
// pane.
// DiscoverMailboxList() also gets the ACLs for each newly discovered folder
void TNavigatorImapConnection::DiscoverMailboxList()
{
	SetMailboxDiscoveryStatus(eContinue);
	if (GetServerStateParser().ServerHasACLCapability())
		fHierarchyNameState = kListingForInfoAndDiscovery;
	else
		fHierarchyNameState = kNoOperationInProgress;

	// Pretend that the Trash folder doesn't exist, so we will rediscover it if we need to.
	TIMAPHostInfo::SetOnlineTrashFolderExistsForHost(fCurrentUrl->GetUrlHost(), FALSE);

	// iterate through all namespaces and LSUB them.
	for (int i = TIMAPHostInfo::GetNumberOfNamespacesForHost(fCurrentUrl->GetUrlHost()); i >= 1; i-- )
	{
		TIMAPNamespace *ns = TIMAPHostInfo::GetNamespaceNumberForHost(fCurrentUrl->GetUrlHost(), i);
		if (ns)
		{
			const char *prefix = ns->GetPrefix();
			if (prefix)
			{

				if (*prefix)	// only do it for non-empty namespace prefixes
				{
					// Explicitly discover each Namespace, so that we can create subfolders of them,
					mailbox_spec *boxSpec = (mailbox_spec *) XP_CALLOC(1, sizeof(mailbox_spec) );
					if (boxSpec)
					{
						boxSpec->folderSelected = FALSE;
						boxSpec->hostName = fCurrentUrl->GetUrlHost();
						boxSpec->connection = this;
						boxSpec->flagState = NULL;
						boxSpec->discoveredFromLsub = TRUE;
						boxSpec->onlineVerified = TRUE;
						boxSpec->box_flags = kNoselect;
						boxSpec->hierarchySeparator = ns->GetDelimiter();
						boxSpec->allocatedPathName = fCurrentUrl->AllocateCanonicalPath(ns->GetPrefix());

						switch (ns->GetType())
						{
						case kPersonalNamespace:
							boxSpec->box_flags |= kPersonalMailbox;
							break;
						case kPublicNamespace:
							boxSpec->box_flags |= kPublicMailbox;
							break;
						case kOtherUsersNamespace:
							boxSpec->box_flags |= kOtherUsersMailbox;
							break;
						default:	// (kUnknownNamespace)
							break;
						}

						DiscoverMailboxSpec(boxSpec);
					}
					else
						HandleMemoryFailure();
				}

				// now do the folders within this namespace
				char *pattern = NULL, *pattern2 = NULL;
				if (TIMAPHostInfo::GetHostIsUsingSubscription(fCurrentUrl->GetUrlHost()))
					pattern = PR_smprintf("%s*", prefix);
				else
				{
					pattern = PR_smprintf("%s%%", prefix);
					char delimiter = ns->GetDelimiter();
					if (delimiter)
					{
						// delimiter might be NIL, in which case there's no hierarchy anyway
						pattern2 = PR_smprintf("%s%%%c%%", prefix, delimiter);
					}
				}
				if (pattern)
				{
					if (TIMAPHostInfo::GetHostIsUsingSubscription(fCurrentUrl->GetUrlHost())) // && !GetSubscribingNow())	should never get here from subscribe pane
						Lsub(pattern);
					else
					{
						List(pattern);
						if (pattern2)
						{
							List(pattern2);
							XP_FREE(pattern2);
						}
					}
					XP_FREE(pattern);
				}
			}
		}
	}

	// explicitly LIST the INBOX if (a) we're not using subscription, or (b) we are using subscription and
	// the user wants us to always show the INBOX.
	if (GetServerStateParser().LastCommandSuccessful() && 
		(!TIMAPHostInfo::GetHostIsUsingSubscription(fCurrentUrl->GetUrlHost()) ||
		TIMAPHostInfo::GetShouldAlwaysListInboxForHost(fCurrentUrl->GetUrlHost())))
	{
		List("INBOX", FALSE, TRUE);
	}
	fHierarchyNameState = kNoOperationInProgress;

	MailboxDiscoveryFinished();

	// Get the ACLs for newly discovered folders
	if (GetServerStateParser().ServerHasACLCapability())
	{
		int total = XP_ListCount(fListedMailboxList), count = 0;
		GetServerStateParser().SetReportingErrors(FALSE);
		if (total)
		{
			ProgressEventFunction_UsingId(MK_IMAP_GETTING_ACL_FOR_FOLDER);
			TIMAPMailboxInfo *mb = NULL;
			do
			{
				mb = (TIMAPMailboxInfo *) XP_ListRemoveTopObject(fListedMailboxList);
				if (mb)
				{
					if (FolderNeedsACLInitialized(mb->GetMailboxName()))
						RefreshACLForFolder(mb->GetMailboxName());
					PercentProgressUpdateEvent(NULL, (count*100)/total);
					delete mb;	// this is the last time we're using the list, so delete the entries here
					count++;
				}
			} while (mb && !DeathSignalReceived());
		}
	}
}


void TNavigatorImapConnection::DiscoverAllAndSubscribedBoxes()
{
	// used for subscribe pane
	// iterate through all namespaces
	for (int i = TIMAPHostInfo::GetNumberOfNamespacesForHost(fCurrentUrl->GetUrlHost()); i >= 1; i-- )
	{
		TIMAPNamespace *ns = TIMAPHostInfo::GetNamespaceNumberForHost(fCurrentUrl->GetUrlHost(), i);
		if (ns)
		{
			const char *prefix = ns->GetPrefix();
			if (prefix)
			{
				// Fills in the hierarchy delimiters for some namespaces, for the case that they
				// were filled in manually from the preferences.
				if (ns->GetIsDelimiterFilledIn())
				{
					fHierarchyNameState = kDiscoveringNamespacesOnly;
					List(prefix);
					fHierarchyNameState = kNoOperationInProgress;
				}
				
				char *allPattern = PR_smprintf("%s*",prefix);
				char *topLevelPattern = PR_smprintf("%s%%",prefix);
				char *secondLevelPattern = NULL;
				char delimiter = ns->GetDelimiter();
				if (delimiter)
				{
					// Hierarchy delimiter might be NIL, in which case there's no hierarchy anyway
					secondLevelPattern = PR_smprintf("%s%%%c%%",prefix, delimiter);
				}
				if (allPattern)
				{
					Lsub(allPattern);	// LSUB all the subscribed
					XP_FREE(allPattern);
				}
				if (topLevelPattern)
				{
					List(topLevelPattern);	// LIST the top level
					XP_FREE(topLevelPattern);
				}
				if (secondLevelPattern)
				{
					List(secondLevelPattern);	// LIST the second level
					XP_FREE(secondLevelPattern);
				}
			}
		}
	}
	SetFolderDiscoveryFinished();
}


// report the success of the upgrade to IMAP subscription
static
void MOZTHREAD_SubscribeUpgradeFinished(void *blockingConnectionVoid,
                      void *endStateVoid)
{
    TNavigatorImapConnection *imapConnection = 
        (TNavigatorImapConnection *) blockingConnectionVoid;

	EIMAPSubscriptionUpgradeState *endState = (EIMAPSubscriptionUpgradeState *) endStateVoid;

	MSG_ReportSuccessOfUpgradeToIMAPSubscription(imapConnection->GetActiveEntry()->window_id, endState);
    imapConnection->NotifyEventCompletionMonitor();
}

void TNavigatorImapConnection::UpgradeToSubscriptionFinishedEvent(EIMAPSubscriptionUpgradeState endState)
{
    EIMAPSubscriptionUpgradeState *orphanedUpgradeState = (EIMAPSubscriptionUpgradeState *) XP_ALLOC(sizeof(EIMAPSubscriptionUpgradeState));
    if (orphanedUpgradeState)
        *orphanedUpgradeState = endState;

	TImapFEEvent *upgradeEvent = 
	    new TImapFEEvent(MOZTHREAD_SubscribeUpgradeFinished,               // function to call
	                    this,                                              // access to current entry/context
	                    orphanedUpgradeState,
						FALSE);								// storage passed
	                                                    

	if (upgradeEvent)
	{
	    fFEEventQueue->AdoptEventToEnd(upgradeEvent);
		WaitForFEEventCompletion();
	}
	else
	    HandleMemoryFailure();
}

static void
MOZTHREAD_GetOldHostEvent(void *blockingConnectionVoid,
                      void *pOldHostNameVoid)
{
    TNavigatorImapConnection *imapConnection = 
        (TNavigatorImapConnection *) blockingConnectionVoid;

	char **pOldHostName = (char **) pOldHostNameVoid;
	// fill in the value
	PREF_CopyCharPref("network.hosts.pop_server", pOldHostName);
    imapConnection->NotifyEventCompletionMonitor();
}

void TNavigatorImapConnection::GetOldIMAPHostNameEvent(char **oldHostName)
{
	TImapFEEvent *getHostEvent = 
	    new TImapFEEvent(MOZTHREAD_GetOldHostEvent,               // function to call
	                    this,                                              // access to current entry/context
	                    oldHostName,
						FALSE);	                                                    

	if (getHostEvent)
	{
	    fFEEventQueue->AdoptEventToEnd(getHostEvent);
		WaitForFEEventCompletion();
	}
	else
	    HandleMemoryFailure();
}


static void
MOZTHREAD_PromptUserForSubscriptionUpgradePath(void *blockingConnectionVoid, void *valueVoid)
{
    TNavigatorImapConnection *imapConnection = 
        (TNavigatorImapConnection *) blockingConnectionVoid;	
    ActiveEntry *ce = imapConnection->GetActiveEntry();
	XP_Bool *value = (XP_Bool *)valueVoid;
	XP_Bool shouldWeUpgrade = FALSE;

	char *promptString = XP_GetString(MK_IMAP_UPGRADE_PROMPT_USER);
	if (promptString)
	{
		char *promptString2 = XP_GetString(MK_IMAP_UPGRADE_PROMPT_USER_2);
		if (promptString2)
		{
			char *finalPrompt = StrAllocCat(promptString, promptString2);
			if (finalPrompt)
			{
				shouldWeUpgrade = FE_Confirm(ce->window_id, finalPrompt);
			}
			else
				shouldWeUpgrade = FALSE;
		}
		else
			shouldWeUpgrade = FALSE;
	}
	else
		shouldWeUpgrade = FALSE;

	if (!shouldWeUpgrade)
		FE_Alert(ce->window_id, XP_GetString(MK_IMAP_UPGRADE_CUSTOM));

	*value = shouldWeUpgrade;
    imapConnection->NotifyEventCompletionMonitor();
}

// Returns TRUE if we need to automatically upgrade them, and FALSE if
// they want to do it manually (through the Subscribe UI).
XP_Bool TNavigatorImapConnection::PromptUserForSubscribeUpgradePath()
{
	XP_Bool *value = (XP_Bool *)XP_ALLOC(sizeof(XP_Bool));
	XP_Bool rv = FALSE;

	if (!value) return FALSE;

	TImapFEEvent *getHostEvent = 
	    new TImapFEEvent(MOZTHREAD_PromptUserForSubscriptionUpgradePath,               // function to call
	                    this,                                              // access to current entry/context
	                    value,
						FALSE);

	if (getHostEvent)
	{
	    fFEEventQueue->AdoptEventToEnd(getHostEvent);
		WaitForFEEventCompletion();
	}
	else
	    HandleMemoryFailure();

	rv = *value;
	XP_FREE(value);
	return rv;
}

// Should only use this for upgrading to subscribe model
void TNavigatorImapConnection::SubscribeToAllFolders()
{
	fHierarchyNameState = kListingForInfoOnly;
	TIMAPMailboxInfo *mb = NULL;

	// This will fill in the list
	if (!DeathSignalReceived())
		List("*");

	int total = XP_ListCount(fListedMailboxList), count = 0;
	GetServerStateParser().SetReportingErrors(FALSE);
	if (!DeathSignalReceived()) do
	{
		mb = (TIMAPMailboxInfo *) XP_ListRemoveTopObject(fListedMailboxList);
		if (mb)
		{
			Subscribe(mb->GetMailboxName());
			PercentProgressUpdateEvent(NULL, (count*100)/total);
			delete mb;
			count++;
		}
	} while (mb && !DeathSignalReceived());
	
	PercentProgressUpdateEvent(NULL, 100);

	GetServerStateParser().SetReportingErrors(TRUE);

	fHierarchyNameState = kNoOperationInProgress;
}

int TNavigatorImapConnection::GetNumberOfListedMailboxesWithUnlistedChildren()
{
	int count = 0, listSize = XP_ListCount(fListedMailboxList), i = 1;
	for (i = 1; i <= listSize; i++) 
	{
		TIMAPMailboxInfo *mb = (TIMAPMailboxInfo *)XP_ListGetObjectNum(fListedMailboxList, i);
		if (!mb->GetChildrenListed())
			count++;
	}
	return count;
}

void TNavigatorImapConnection::UpgradeToSubscription()
{
	EIMAPSubscriptionUpgradeState endState = kEverythingDone;
	if (fCurrentUrl->GetShouldSubscribeToAll())
	{
		SubscribeToAllFolders();
	}
	else
	{
		TIMAPNamespace *ns = TIMAPHostInfo::GetDefaultNamespaceOfTypeForHost(fCurrentUrl->GetUrlHost(), kPersonalNamespace);
		if (ns && ns->GetType()==kPersonalNamespace)
		{
			// This should handle the case of getting the personal namespace from either the
			// prefs or the NAMESPACE extension.
			// We will list all the personal folders, and subscribe to them all.
			// We can do this, because we know they're personal, and the user said to try
			// to subscribe to all.

			fHierarchyNameState = kDiscoveringNamespacesOnly;
			List(ns->GetPrefix());

			fHierarchyNameState = kListingForInfoOnly;
			char *pattern = PR_smprintf("%s*",ns->GetPrefix());
			if (pattern && !DeathSignalReceived())
				List(pattern);
			fHierarchyNameState = kNoOperationInProgress;

			// removing and freeing it as we go.

			GetServerStateParser().SetReportingErrors(FALSE);
			int total = XP_ListCount(fListedMailboxList), count = 0;
			TIMAPMailboxInfo *mb = 0;
			if (!DeathSignalReceived()) do
			{
				mb = (TIMAPMailboxInfo *) XP_ListRemoveTopObject(fListedMailboxList);
				if (mb)
				{
					Subscribe(mb->GetMailboxName());
					PercentProgressUpdateEvent(NULL, (count*100)/total);
					delete mb;
					count++;
				}
			} while (mb && !DeathSignalReceived());
			PercentProgressUpdateEvent(NULL, 100);
			GetServerStateParser().SetReportingErrors(TRUE);
		}
		else if (!ns)
		{
			// There is no personal or default namespace.  This means that the server 
			// supports the NAMESPACE extension, and has other namespaces, but explicitly
			// has no personal namespace.  I would be very surprised if we EVER encounter
			// this situation, since the user must have been using Communicator 4.0 on
			// a server that supported the NAMESPACE extension and only had, say,
			// public folders.  Not likely.  All we can really do is alert the user
			// and bring up the Subscribe UI.
			AlertUserEvent_UsingId(MK_IMAP_UPGRADE_NO_PERSONAL_NAMESPACE);
			endState = kBringUpSubscribeUI;
		}
		else if (!GetServerStateParser().ServerHasNamespaceCapability())
		{
			// NAMESPACE extension not supported, and we only have a default namespace
			// (That is, no explicit personal namespace is specified -- only "")
			if ((GetServerStateParser().GetCapabilityFlag() &
				kHasXNetscapeCapability) &&
				!DeathSignalReceived())
			{
				Netscape();
			}
			if (GetServerStateParser().ServerIsNetscape3xServer())
			{
				// Server is a Netscape 3.0 Messaging Server.  We know that all the
				// folders are personal mail folders, and can subscribe to all (*).
				SubscribeToAllFolders();
			}
			else
			{
				// Worst case.  It's not a Netscape 3.0 server, NAMESPACE is not supported,
				// the user hasn't manually specified a personal namespace prefix, and
				// Mission Control hasn't told us anything.  Do the best we can.
				// Progressively LIST % until either we run out of folders or we encounter
				// 50 (MAX_NEW_IMAP_FOLDER_COUNT)
				
				const char *namespacePrefix = ns->GetPrefix();	// Has to be "", right?

				// List the prefix, to get the hierarchy delimiter.
				// We probably shouldn't subscribe to the namespace itself.
				fHierarchyNameState = kDiscoveringNamespacesOnly;
				if (!DeathSignalReceived())
					List(namespacePrefix);	

				// Now, generate the list
				// Do it one level at a time, and stop either when nothing comes back or
				// we pass the threshold mailbox count (50)
				fHierarchyNameState = kListingForInfoOnly;
				char *firstPattern = PR_smprintf("%s%%", namespacePrefix);
				if (firstPattern && !DeathSignalReceived())
				{	
					List(firstPattern);
					XP_FREE(firstPattern);
				}
				while ((XP_ListCount(fListedMailboxList) < MAX_NEW_IMAP_FOLDER_COUNT) &&
					(GetNumberOfListedMailboxesWithUnlistedChildren() > 0))
				{
					int listSize = XP_ListCount(fListedMailboxList), i = 1;
					for (i = 1; (i <= listSize) && !DeathSignalReceived(); i++) 
					{
						TIMAPMailboxInfo *mb = (TIMAPMailboxInfo *)XP_ListGetObjectNum(fListedMailboxList, i);
						if (!mb->GetChildrenListed())
						{
							char delimiter = ns->GetDelimiter();
							// If there's a NIL hierarchy delimter, then we don't need to list children
							if (delimiter)
							{
								char *pattern = PR_smprintf("%s%s%c%%",namespacePrefix,mb->GetMailboxName(), ns->GetDelimiter());
								if (pattern && !DeathSignalReceived())
									List(pattern);
							}
							mb->SetChildrenListed(TRUE);
						}
					}
				}
				fHierarchyNameState = kNoOperationInProgress;

				if (XP_ListCount(fListedMailboxList) < MAX_NEW_IMAP_FOLDER_COUNT)
				{
					// Less than the threshold (50?)
					// Iterate through the list and subscribe to each mailbox,
					// removing and freeing it as we go.
					GetServerStateParser().SetReportingErrors(FALSE);
					int total = XP_ListCount(fListedMailboxList), count = 0;
					TIMAPMailboxInfo *mb = 0;
					if (!DeathSignalReceived()) do
					{
						mb = (TIMAPMailboxInfo *)XP_ListRemoveTopObject(fListedMailboxList);
						if (mb)
						{
							Subscribe(mb->GetMailboxName());
							PercentProgressUpdateEvent(NULL, (100*count)/total);
							delete mb;
							count++;
						}
					} while (mb && !DeathSignalReceived());
					PercentProgressUpdateEvent(NULL, 100);
					GetServerStateParser().SetReportingErrors(TRUE);
				}
				else
				{
					// More than the threshold.  Bring up the subscribe UI.
					// We still must free the storage in the list
					TIMAPMailboxInfo *mb = 0;
					do
					{
						mb = (TIMAPMailboxInfo *) XP_ListRemoveTopObject(fListedMailboxList);
						delete mb;
					} while (mb);
					AlertUserEvent_UsingId(MK_IMAP_UPGRADE_TOO_MANY_FOLDERS);
					endState = kBringUpSubscribeUI;
				}
			}
		}
	}
	if (!DeathSignalReceived())	// if not interrupted
		UpgradeToSubscriptionFinishedEvent(endState);
}


void TNavigatorImapConnection::ProcessAuthenticatedStateURL()
{
    if (fCurrentUrl->GetIMAPurlType() == TIMAPUrl::kLsubFolders)
    {
        //XP_ASSERT(FALSE);   // now handled in FindMailboxesIfNecessary
	    // **** use to find out whether Drafts, Sent, & Templates folder
		// exists or not even the user didn't subscribe to it
		char *mailboxName =
			fCurrentUrl->CreateServerDestinationFolderPathString();
		if (mailboxName)
		{
			char *convertedName =
				CreateUtf7ConvertedString(mailboxName, TRUE);
			if (convertedName)
			{
				FREEIF(mailboxName);
				mailboxName = convertedName;
			}
			ProgressEventFunction_UsingId
				(MK_IMAP_STATUS_LOOKING_FOR_MAILBOX);
			IncrementCommandTagNumber();
			PR_snprintf(GetOutputBuffer(), kOutputBufferSize,
						"%s list \"\" \"%s\"" CRLF,
						GetServerCommandTag(),
						mailboxName);
			int ioStatus = WriteLineToSocket(GetOutputBuffer());
			TimeStampListNow();
			ParseIMAPandCheckForNewMail();
							// send list command to the server; modify
							// DiscoverIMAPMailbox to check for the
							// existence of the folder
			
			FREEIF(mailboxName);
		}
		else
		{
			HandleMemoryFailure();
		}
    }
	else if (fCurrentUrl->GetIMAPurlType() == TIMAPUrl::kGetMailAccountUrl) {
		if (GetServerStateParser().GetCapabilityFlag() &
			kHasXNetscapeCapability) {
			Netscape();
			if (GetServerStateParser().LastCommandSuccessful()) {
				TImapFEEvent *alertEvent = 
					new TImapFEEvent(msgSetMailAccountURL,  // function to call
									 this,                // access to current entry
									 (void *) fCurrentUrl->GetUrlHost(),
									 TRUE);
				if (alertEvent)
				{
					fFEEventQueue->AdoptEventToEnd(alertEvent);
					// WaitForFEEventCompletion();
				}
				else
					HandleMemoryFailure();
			}
		}
	}
    else
    {
    	// even though we don't have to to be legal protocol, Close any select mailbox
    	// so we don't miss any info these urls might change for the selected folder
    	// (e.g. msg count after append)
		if (GetServerStateParser().GetIMAPstate() == TImapServerState::kFolderSelected)
		{
			// now we should be avoiding an implicit Close because it performs an implicit Expunge
			// authenticated state urls should not use a cached connection that is in the selected state
			XP_ASSERT(FALSE);
			//Close();
		}
			
        if (fCurrentUrl->GetIMAPurlType() == TIMAPUrl::kOfflineToOnlineMove)
        {
            char *destinationMailbox =  
                fCurrentUrl->CreateServerDestinationFolderPathString();
            
		    if (destinationMailbox)
		    {
		    	char *convertedName = CreateUtf7ConvertedString(destinationMailbox, TRUE);
		    	XP_FREE(destinationMailbox);
		    	destinationMailbox = convertedName;
		    }
            if (destinationMailbox)
            {
                uint32 appendSize = 0;
                do {
                    WaitForNextAppendMessageSize();
                    appendSize = GetAppendSize();
                    if (!DeathSignalReceived() && appendSize)
                    {
                        char messageSizeString[100];
                        sprintf(messageSizeString, "%ld",(long) appendSize);
                        AppendMessage(destinationMailbox, messageSizeString, GetAppendFlags());
                    }
                } while (appendSize && GetServerStateParser().LastCommandSuccessful());
                FREEIF( destinationMailbox);
            }
            else
                HandleMemoryFailure();
        }
		else if (fCurrentUrl->GetIMAPurlType() == TIMAPUrl::kAppendMsgFromFile)
		{
			char *sourceMessageFile =
			    XP_STRDUP(GetActiveEntry()->URL_s->post_data);
            char *mailboxName =  
                fCurrentUrl->CreateServerSourceFolderPathString();
            
		    if (mailboxName)
		    {
		    	char *convertedName = CreateUtf7ConvertedString(mailboxName, TRUE);
		    	XP_FREE(mailboxName);
		    	mailboxName = convertedName;
		    }
            if (mailboxName)
            {
				UploadMessageFromFile(sourceMessageFile, mailboxName,
									  kImapMsgSeenFlag);
            }
            else
                HandleMemoryFailure();
			FREEIF( sourceMessageFile );
            FREEIF( mailboxName );
		}
        else if (fCurrentUrl->GetIMAPurlType() == TIMAPUrl::kDiscoverAllBoxesUrl)
        {
			XP_ASSERT(!GetSubscribingNow());	// should not get here from subscribe UI
			DiscoverMailboxList();
        }
        else if (fCurrentUrl->GetIMAPurlType() == TIMAPUrl::kDiscoverAllAndSubscribedBoxesUrl)
        {
			XP_ASSERT(GetSubscribingNow());
			DiscoverAllAndSubscribedBoxes();
        }
       else
        {
            char *sourceMailbox = fCurrentUrl->CreateServerSourceFolderPathString();
		    if (sourceMailbox)
		    {
		    	char *convertedName = CreateUtf7ConvertedString(sourceMailbox, TRUE);
		    	XP_FREE(sourceMailbox);
		    	sourceMailbox = convertedName;
		    }
            
            if (sourceMailbox)
            {
                if (fCurrentUrl->GetIMAPurlType() == TIMAPUrl::kCreateFolder)
                {
                    XP_Bool created = CreateMailboxRespectingSubscriptions(sourceMailbox);
                    if (created)
					{
						List(sourceMailbox);
					}
					else
						FolderNotCreated(sourceMailbox);
                }
                else if (fCurrentUrl->GetIMAPurlType() == TIMAPUrl::kDiscoverChildrenUrl)
                {
                	char *canonicalParent = fCurrentUrl->CreateCanonicalSourceFolderPathString();
                	if (canonicalParent)
                	{
						NthLevelChildList(canonicalParent, 2);
						//CanonicalChildList(canonicalParent,FALSE);
                		XP_FREE(canonicalParent);
                	}
                }
				else if (fCurrentUrl->GetIMAPurlType() == TIMAPUrl::kDiscoverLevelChildrenUrl)
				{
                	char *canonicalParent = fCurrentUrl->CreateCanonicalSourceFolderPathString();
					int depth = fCurrentUrl->GetChildDiscoveryDepth();
   					if (canonicalParent)
					{
						NthLevelChildList(canonicalParent, depth);
						if (GetServerStateParser().LastCommandSuccessful())
							ChildDiscoverySucceeded();
						XP_FREE(canonicalParent);
					}
				}
				else if (fCurrentUrl->GetIMAPurlType() == TIMAPUrl::kSubscribe)
				{
					Subscribe(sourceMailbox);
				}
				else if (fCurrentUrl->GetIMAPurlType() == TIMAPUrl::kUnsubscribe)
				{
					Unsubscribe(sourceMailbox);
				}
				else if (fCurrentUrl->GetIMAPurlType() == TIMAPUrl::kRefreshACL)
				{
					RefreshACLForFolder(sourceMailbox);
				}
				else if (fCurrentUrl->GetIMAPurlType() == TIMAPUrl::kRefreshAllACLs)
				{
					RefreshAllACLs();
				}
				else if (fCurrentUrl->GetIMAPurlType() == TIMAPUrl::kListFolder)
				{
					char *folderName = fCurrentUrl->CreateCanonicalSourceFolderPathString();
					List(folderName);
				}
				else if (fCurrentUrl->GetIMAPurlType() == TIMAPUrl::kUpgradeToSubscription)
				{
					UpgradeToSubscription();
				}
				else if (fCurrentUrl->GetIMAPurlType() == TIMAPUrl::kFolderStatus)
				{
					StatusForFolder(sourceMailbox);
				}
				else if (fCurrentUrl->GetIMAPurlType() == TIMAPUrl::kRefreshFolderUrls)
				{
					XMailboxInfo(sourceMailbox);
					if (GetServerStateParser().LastCommandSuccessful()) 
					{
						InitializeFolderUrl(sourceMailbox);
					}
				}
                else if (fCurrentUrl->GetIMAPurlType() == TIMAPUrl::kDeleteFolder)
                {
                	XP_Bool deleted = DeleteSubFolders(sourceMailbox);
                    if (deleted)
                        deleted = DeleteMailboxRespectingSubscriptions(sourceMailbox);
					if (deleted)
						FolderDeleted(sourceMailbox);
                }
                else if (fCurrentUrl->GetIMAPurlType() == TIMAPUrl::kRenameFolder)
                {
                    char *destinationMailbox = 
                        fCurrentUrl->CreateServerDestinationFolderPathString();
				    if (destinationMailbox)
				    {
				    	char *convertedName = CreateUtf7ConvertedString(destinationMailbox, TRUE);
				    	XP_FREE(destinationMailbox);
				    	destinationMailbox = convertedName;
				    }
                    if (destinationMailbox)
                    {
						XP_Bool renamed = RenameHierarchyByHand(sourceMailbox, destinationMailbox);
                        if (renamed)
                            FolderRenamed(sourceMailbox, destinationMailbox);
                            
                        FREEIF( destinationMailbox);
                    }
                    else
                        HandleMemoryFailure();
                }       
                else if (fCurrentUrl->GetIMAPurlType() == TIMAPUrl::kMoveFolderHierarchy)
                {
                    char *destinationMailbox = 
                        fCurrentUrl->CreateServerDestinationFolderPathString();
                    if (destinationMailbox)
                    {
                    	// worst case length
                    	char *newBoxName = (char *) XP_ALLOC(XP_STRLEN(destinationMailbox) + XP_STRLEN(sourceMailbox) + 2);
                    	if (newBoxName)
                    	{
                    		char onlineDirSeparator = fCurrentUrl->GetOnlineSubDirSeparator();
                    		XP_STRCPY(newBoxName, destinationMailbox);
                    		
                    		char *leafSeparator = XP_STRRCHR(sourceMailbox, onlineDirSeparator);
                    		if (!leafSeparator)
                    			leafSeparator = sourceMailbox;	// this is a root level box
                    		else
                    			leafSeparator++;
                    		
                    		if ( *newBoxName && ( *(newBoxName + XP_STRLEN(newBoxName) - 1) != onlineDirSeparator))
                    		{
                    			char separatorStr[2];
                    			separatorStr[0] = onlineDirSeparator;
                    			separatorStr[1] = 0;
                    			XP_STRCAT(newBoxName, separatorStr);
                    		}
                    		XP_STRCAT(newBoxName, leafSeparator);
							XP_Bool renamed = RenameHierarchyByHand(sourceMailbox, newBoxName);
	                        if (renamed)
	                            FolderRenamed(sourceMailbox, newBoxName);
                    	}
                    }
                }
                

                FREEIF( sourceMailbox);
            }
            else
                HandleMemoryFailure();
        }
    }
}

int32 TNavigatorImapConnection::GetMessageSizeForProgress()
{
    return GetServerStateParser().SizeOfMostRecentMessage();
}

int32 TNavigatorImapConnection::GetBytesMovedSoFarForProgress()
{
    return fBytesMovedSoFarForProgress;
}

void  TNavigatorImapConnection::SetBytesMovedSoFarForProgress(int32 bytesMoved)
{
    fBytesMovedSoFarForProgress = bytesMoved;
}


void TNavigatorImapConnection::AlertUserEvent_UsingId(uint32 msgId)
{
    TImapFEEvent *alertEvent = 
        new TImapFEEvent(AlertEventFunction_UsingId,            // function to call
                         this,                                  // access to current entry
                         (void *) msgId,
						 TRUE);                                // alert message
    if (alertEvent)
    {
        fFEEventQueue->AdoptEventToEnd(alertEvent);
        WaitForFEEventCompletion();
    }
    else
        HandleMemoryFailure();
}

void TNavigatorImapConnection::AlertUserEvent(char *message)
{
    TImapFEEvent *alertEvent = 
        new TImapFEEvent(AlertEventFunction,            // function to call
                         this,                                          // access to current entry
                         message,
						 TRUE);                                      // alert message
    if (alertEvent)
    {
        fFEEventQueue->AdoptEventToEnd(alertEvent);
        WaitForFEEventCompletion();
    }
    else
        HandleMemoryFailure();
}

void TNavigatorImapConnection::AlertUserEventFromServer(char *serverSaid)
{
    TImapFEEvent *alertEvent = 
        new TImapFEEvent(AlertEventFunctionFromServer,            // function to call
                         this,                                          // access to current entry
                         serverSaid,
						 TRUE);                                      // alert message
    if (alertEvent)
    {
        fFEEventQueue->AdoptEventToEnd(alertEvent);
        WaitForFEEventCompletion();
    }
    else
        HandleMemoryFailure();
}

void TNavigatorImapConnection::ProgressUpdateEvent(char *message)
{
    TImapFEEvent *progressEvent = 
        new TImapFEEvent(ProgressEventFunction,         // function to call
                         this,                                          // access to current entry
                         message,
						 TRUE);                                      // progress message
    
    if (progressEvent)
    {
        fFEEventQueue->AdoptEventToEnd(progressEvent);
        WaitForFEEventCompletion();
    }
    else
        HandleMemoryFailure();
}

void TNavigatorImapConnection::ProgressEventFunction_UsingId(uint32 msgId)
{
	if (msgId != (uint32) fLastProgressStringId)
	{
		TImapFEEvent *progressEvent = 
			new TImapFEEvent(ProgressStatusFunction_UsingId,            // function to call
							 this,                                  // access to current entry
							 (void *) msgId,
							 TRUE);								// alert message id
		if (progressEvent)
		{
			fFEEventQueue->AdoptEventToEnd(progressEvent);
			WaitForFEEventCompletion();
		}
		else
			HandleMemoryFailure();
		fLastProgressStringId = msgId;
	}
}

void TNavigatorImapConnection::ProgressEventFunction_UsingIdWithString(uint32 msgId, const char* extraInfo)
{
	StatusMessageInfo *msgInfo = (StatusMessageInfo *) XP_ALLOC(sizeof(StatusMessageInfo)); // free in event handler
	if (msgInfo) {
		if (extraInfo) {
			msgInfo->msgID = msgId;
			msgInfo->extraInfo = XP_STRDUP (extraInfo);									// free in event handler
			TImapFEEvent *progressEvent = 
				new TImapFEEvent(ProgressStatusFunction_UsingIdWithString,				// function to call
								 this,													// access to current entry
								 (void *) msgInfo,
								 TRUE);										// alert message info
			if (progressEvent)
			{
				fFEEventQueue->AdoptEventToEnd(progressEvent);
				WaitForFEEventCompletion();
			}
			else
				HandleMemoryFailure();
		}
	}
}

void TNavigatorImapConnection::PercentProgressUpdateEvent(char *message, int percent)
{
	int64 nowMS;
	if (percent == fLastPercent)
		return;	// hasn't changed, right? So just return. Do we need to clear this anywhere?

	if (percent < 100)	// always need to do 100%
	{
		int64 minIntervalBetweenProgress;

		LL_I2L(minIntervalBetweenProgress, 250);
		int64 diffSinceLastProgress;
		LL_I2L(nowMS, PR_IntervalToMilliseconds(PR_IntervalNow()));
		LL_SUB(diffSinceLastProgress, nowMS, fLastProgressTime); // r = a - b
		LL_SUB(diffSinceLastProgress, diffSinceLastProgress, minIntervalBetweenProgress); // r = a - b
		if (!LL_GE_ZERO(diffSinceLastProgress))
			return;
	}

    ProgressInfo *progress = (ProgressInfo *) XP_ALLOC(sizeof(ProgressInfo)); // free in event handler
	if (progress)
	{
		// we can do the event without a message - it will set the percent bar
		if (message)
			progress->message = XP_STRDUP(message);  // free in event handler
		else
			progress->message = 0;
		progress->percent = percent;
		fLastPercent = percent;
		
		TImapFEEvent *progressEvent = 
			new TImapFEEvent(PercentProgressEventFunction,         // function to call
							 this,                                          // access to current entry
							 progress,
							 TRUE);                                      // progress message

		fLastProgressTime = nowMS;
		if (progressEvent)
		{
			fFEEventQueue->AdoptEventToEnd(progressEvent);
			WaitForFEEventCompletion();
		}
		else
			HandleMemoryFailure();
	}
}

void TNavigatorImapConnection::PastPasswordCheckEvent()
{
	TImapFEEvent *pwCheckEvent = 
		new TImapFEEvent(PastPasswordCheckFunction, this, NULL, TRUE);
	
	if (pwCheckEvent)
	{
		fFEEventQueue->AdoptEventToEnd(pwCheckEvent);
		WaitForFEEventCompletion();
	}
	else
		HandleMemoryFailure();
}


static void CommitNamespacesFunction(void *blockingConnectionVoid, void *hostNameVoid)
{
    TNavigatorImapConnection *imapConnection = 
        (TNavigatorImapConnection *) blockingConnectionVoid;
    ActiveEntry *ce = imapConnection->GetActiveEntry();
    
	char *hostName = (char *)hostNameVoid;

	if (hostName)
		TIMAPHostInfo::CommitNamespacesForHost(hostName, ce->window_id->mailMaster);

	imapConnection->NotifyEventCompletionMonitor();
}

void TNavigatorImapConnection::CommitNamespacesForHostEvent()
{
	TImapFEEvent *commitEvent = 
		new TImapFEEvent(CommitNamespacesFunction,this,(void *)(fCurrentUrl->GetUrlHost()), TRUE);
	
	if (commitEvent)
	{
		fFEEventQueue->AdoptEventToEnd(commitEvent);
		WaitForFEEventCompletion();
	}
	else
		HandleMemoryFailure();
}

static void CommitCapabilityFunction(void *blockingConnectionVoid, void *hostNameVoid)
{
    TNavigatorImapConnection *imapConnection = 
        (TNavigatorImapConnection *) blockingConnectionVoid;
    ActiveEntry *ce = imapConnection->GetActiveEntry();
    
	char *hostName = (char *)hostNameVoid;

	if (hostName)
		MSG_CommitCapabilityForHost(hostName, ce->window_id->mailMaster);

	imapConnection->NotifyEventCompletionMonitor();
}

void TNavigatorImapConnection::CommitCapabilityForHostEvent()
{
	TImapFEEvent *commitEvent = 
		new TImapFEEvent(CommitCapabilityFunction,this,(void *)(fCurrentUrl->GetUrlHost()), TRUE);
	
	if (commitEvent)
	{
		fFEEventQueue->AdoptEventToEnd(commitEvent);
		WaitForFEEventCompletion();
	}
	else
		HandleMemoryFailure();
}

typedef struct _MessageSizeInfo
{
	char *id;
	char *folderName;
	XP_Bool idIsUid;
	uint32 size;
} MessageSizeInfo;


// Retrieves the message size given a message UID
// Retrieves the message size given a UID or message sequence number
static
void GetMessageSizeEvent(void *blockingConnectionVoid,
							void *sizeInfoVoid)
{
    TNavigatorImapConnection *imapConnection = 
        (TNavigatorImapConnection *) blockingConnectionVoid;
    ActiveEntry *ce = imapConnection->GetActiveEntry();
	MessageSizeInfo *sizeInfo = (MessageSizeInfo *)sizeInfoVoid;
	MSG_Pane *masterPane = ce->URL_s->msg_pane;		// we need some pane to get to the master
    if (!masterPane)
    	masterPane = MSG_FindPane(ce->window_id, MSG_ANYPANE);	// last ditch effort

	sizeInfo->size = MSG_GetIMAPMessageSizeFromDB(masterPane, imapConnection->GetHostName(), sizeInfo->folderName, sizeInfo->id, sizeInfo->idIsUid);

    imapConnection->NotifyEventCompletionMonitor();
}


uint32 TNavigatorImapConnection::GetMessageSize(const char *messageId, XP_Bool idsAreUids)
{
	
	MessageSizeInfo *sizeInfo = (MessageSizeInfo *)XP_ALLOC(sizeof(MessageSizeInfo));
	if (sizeInfo)
	{
		const char *folderFromParser = GetServerStateParser().GetSelectedMailboxName();
		if (folderFromParser)
		{
			sizeInfo->id = (char *)XP_ALLOC(XP_STRLEN(messageId) + 1);
			sizeInfo->folderName = (char *)XP_ALLOC(XP_STRLEN(folderFromParser) + 1);
			XP_STRCPY(sizeInfo->id, messageId);
			sizeInfo->idIsUid = idsAreUids;

			if (sizeInfo->id && sizeInfo->folderName)
			{
				XP_STRCPY(sizeInfo->folderName, folderFromParser);
				TImapFEEvent *getMessageSizeEvent = new TImapFEEvent(GetMessageSizeEvent, this, sizeInfo, FALSE);

				if (getMessageSizeEvent)
				{
					fFEEventQueue->AdoptEventToEnd(getMessageSizeEvent);
					WaitForFEEventCompletion();
				}
				else
				{
					HandleMemoryFailure();
					return 0;
				}
				XP_FREE(sizeInfo->id);
				XP_FREE(sizeInfo->folderName);
			}
			else
			{
				HandleMemoryFailure();
				XP_FREE(sizeInfo);
				return 0;
			}

			int32 rv = 0;
			if (!DeathSignalReceived())
				rv = sizeInfo->size;
			XP_FREE(sizeInfo);
			return rv;
		}
		else
		{
			HandleMemoryFailure();
			return 0;
		}
	}
	else
	{
		HandleMemoryFailure();
		return 0;
	}
}



static void
MOZTHREAD_FolderIsNoSelectEvent(void *blockingConnectionVoid, void *valueVoid)
{
    TNavigatorImapConnection *imapConnection = 
        (TNavigatorImapConnection *) blockingConnectionVoid;	
    ActiveEntry *ce = imapConnection->GetActiveEntry();
	FolderQueryInfo *value = (FolderQueryInfo *)valueVoid;

	value->rv = MSG_IsFolderNoSelect(ce->window_id->mailMaster, value->name, value->hostName);
    imapConnection->NotifyEventCompletionMonitor();
}



// Returns TRUE if the mailbox is a NoSelect mailbox.
// If we don't know about it, returns FALSE.
XP_Bool TNavigatorImapConnection::MailboxIsNoSelectMailbox(const char *mailboxName)
{
	FolderQueryInfo *value = (FolderQueryInfo *)XP_ALLOC(sizeof(FolderQueryInfo));
	XP_Bool rv = FALSE;

	if (!value) return FALSE;

	value->name = XP_STRDUP(mailboxName);
	if (!value->name)
	{
		XP_FREE(value);
		return FALSE;
	}

	value->hostName = XP_STRDUP(fCurrentUrl->GetUrlHost());
	if (!value->hostName)
	{
		XP_FREE(value->name);
		XP_FREE(value);
		return FALSE;
	}

	TImapFEEvent *theEvent = 
	    new TImapFEEvent(MOZTHREAD_FolderIsNoSelectEvent,               // function to call
	                    this,                                              // access to current entry/context
	                    value,
						FALSE);	                                                    

	if (theEvent)
	{
	    fFEEventQueue->AdoptEventToEnd(theEvent);
		WaitForFEEventCompletion();
	}
	else
	    HandleMemoryFailure();

	rv = value->rv;
	XP_FREE(value->hostName);
	XP_FREE(value->name);
	XP_FREE(value);
	return rv;
}

/////////////////// Begin Subscription Management Functions /////////////////////////////


// returns TRUE is the create succeeded (regardless of subscription changes)
XP_Bool TNavigatorImapConnection::CreateMailboxRespectingSubscriptions(const char *mailboxName)
{
	TIMAP4BlockingConnection::CreateMailbox(mailboxName);
	XP_Bool rv = GetServerStateParser().LastCommandSuccessful();
	if (rv)
	{
		if (fAutoSubscribe) // auto-subscribe is on
		{
			// create succeeded - let's subscribe to it
			XP_Bool reportingErrors = GetServerStateParser().GetReportingErrors();
			GetServerStateParser().SetReportingErrors(FALSE);
			Subscribe(mailboxName);
			GetServerStateParser().SetReportingErrors(reportingErrors);
		}
	}
	return (rv);
}

// returns TRUE is the delete succeeded (regardless of subscription changes)
XP_Bool TNavigatorImapConnection::DeleteMailboxRespectingSubscriptions(const char *mailboxName)
{
	XP_Bool rv = TRUE;
	if (!MailboxIsNoSelectMailbox(mailboxName))
	{
		// Only try to delete it if it really exists
		TIMAP4BlockingConnection::DeleteMailbox(mailboxName);
		rv = GetServerStateParser().LastCommandSuccessful();
	}

	// We can unsubscribe even if the mailbox doesn't exist.
	if (rv && fAutoUnsubscribe) // auto-unsubscribe is on
	{
		XP_Bool reportingErrors = GetServerStateParser().GetReportingErrors();
		GetServerStateParser().SetReportingErrors(FALSE);
		Unsubscribe(mailboxName);
		GetServerStateParser().SetReportingErrors(reportingErrors);

	}
	return (rv);
}

// returns TRUE is the rename succeeded (regardless of subscription changes)
// reallyRename tells us if we should really do the rename (TRUE) or if we should just move subscriptions (FALSE)
XP_Bool TNavigatorImapConnection::RenameMailboxRespectingSubscriptions(const char *existingName, const char *newName, XP_Bool reallyRename)
{
	XP_Bool rv = TRUE;
	if (reallyRename && !MailboxIsNoSelectMailbox(existingName))
	{
		TIMAP4BlockingConnection::RenameMailbox(existingName, newName);
		rv = GetServerStateParser().LastCommandSuccessful();
	}
	
	if (rv)
	{
		if (fAutoSubscribe)	// if auto-subscribe is on
		{
			XP_Bool reportingErrors = GetServerStateParser().GetReportingErrors();
			GetServerStateParser().SetReportingErrors(FALSE);
			Subscribe(newName);
			GetServerStateParser().SetReportingErrors(reportingErrors);
		}
		if (fAutoUnsubscribe) // if auto-unsubscribe is on
		{
			XP_Bool reportingErrors = GetServerStateParser().GetReportingErrors();
			GetServerStateParser().SetReportingErrors(FALSE);
			Unsubscribe(existingName);
			GetServerStateParser().SetReportingErrors(reportingErrors);
		}
	}
	return (rv);
}


XP_Bool TNavigatorImapConnection::GetSubscribingNow()
{
	return (fCurrentEntry && fCurrentEntry->window_id && fCurrentEntry->window_id->imapURLPane &&
		(MSG_SUBSCRIBEPANE == MSG_GetPaneType(fCurrentEntry->window_id->imapURLPane)));
}


/////////////////// End Subscription Management Functions /////////////////////////////

/////////////////// Begin ACL Stuff ///////////////////////////////////////////////////

void TNavigatorImapConnection::RefreshACLForFolderIfNecessary(const char *mailboxName)
{
	if (fFolderNeedsACLRefreshed && GetServerStateParser().ServerHasACLCapability())
	{
		RefreshACLForFolder(mailboxName);
		fFolderNeedsACLRefreshed = FALSE;
	}
}

void TNavigatorImapConnection::RefreshACLForFolder(const char *mailboxName)
{

	TIMAPNamespace *ns = TIMAPHostInfo::GetNamespaceForMailboxForHost(fCurrentUrl->GetUrlHost(), mailboxName);
	if (ns)
	{
		switch (ns->GetType())
		{
		case kPersonalNamespace:
			// It's a personal folder, most likely.
			// I find it hard to imagine a server that supports ACL that doesn't support NAMESPACE,
			// so most likely we KNOW that this is a personal, rather than the default, namespace.

			// First, clear what we have.
			ClearAllFolderRights(mailboxName);
			// Now, get the new one.
			GetACLForFolder(mailboxName);
			// We're all done, refresh the icon/flags for this folder
			RefreshFolderACLView(mailboxName);
			break;
		default:
			// We know it's a public folder or other user's folder.
			// We only want our own rights

			// First, clear what we have
			ClearAllFolderRights(mailboxName);
			// Now, get the new one.
			GetMyRightsForFolder(mailboxName);
			// We're all done, refresh the icon/flags for this folder
			RefreshFolderACLView(mailboxName);
			break;
		}
	}
	else
	{
		// no namespace, not even default... can this happen?
		XP_ASSERT(FALSE);
	}
}

void TNavigatorImapConnection::RefreshAllACLs()
{
	fHierarchyNameState = kListingForInfoOnly;
	TIMAPMailboxInfo *mb = NULL;

	// This will fill in the list
	List("*");

	int total = XP_ListCount(fListedMailboxList), count = 0;
	GetServerStateParser().SetReportingErrors(FALSE);
	do
	{
		mb = (TIMAPMailboxInfo *) XP_ListRemoveTopObject(fListedMailboxList);
		if (mb)
		{
			RefreshACLForFolder(mb->GetMailboxName());
			PercentProgressUpdateEvent(NULL, (count*100)/total);
			delete mb;
			count++;
		}
	} while (mb);
	
	PercentProgressUpdateEvent(NULL, 100);
	GetServerStateParser().SetReportingErrors(TRUE);

	fHierarchyNameState = kNoOperationInProgress;
}

static void ClearFolderRightsEvent(void *blockingConnectionVoid, void *aclRightsInfoVoid)
{
    TNavigatorImapConnection *imapConnection = 
        (TNavigatorImapConnection *) blockingConnectionVoid;
    ActiveEntry *ce = imapConnection->GetActiveEntry();
    
	TIMAPACLRightsInfo *aclRights = (TIMAPACLRightsInfo *)aclRightsInfoVoid;

	MSG_ClearFolderRightsForFolder(ce->window_id->mailMaster, aclRights->hostName, aclRights->mailboxName);

	imapConnection->NotifyEventCompletionMonitor();
}

void TNavigatorImapConnection::ClearAllFolderRights(const char *mailboxName)
{
    TIMAPACLRightsInfo *aclRightsInfo = new TIMAPACLRightsInfo();
	if (aclRightsInfo)
	{
		aclRightsInfo->hostName = XP_STRDUP(fCurrentUrl->GetUrlHost());
		aclRightsInfo->mailboxName = XP_STRDUP(mailboxName);
		aclRightsInfo->rights = NULL;
		aclRightsInfo->userName = NULL;

		if (aclRightsInfo->hostName && aclRightsInfo->mailboxName)
		{

			TImapFEEvent *clearFolderRightsEvent = 
				new TImapFEEvent(ClearFolderRightsEvent,              // function to call
								this,                                   // access to current entry/context
								aclRightsInfo,
								FALSE);
                                                        

			if (clearFolderRightsEvent)
			{
				fFEEventQueue->AdoptEventToEnd(clearFolderRightsEvent);
		        WaitForFEEventCompletion();
			}
			else
				HandleMemoryFailure();

			XP_FREE(aclRightsInfo->hostName);
			XP_FREE(aclRightsInfo->mailboxName);
		}
		else
			HandleMemoryFailure();
		delete aclRightsInfo;
	}
	else
		HandleMemoryFailure();
}


static void AddFolderRightsEvent(void *blockingConnectionVoid, void *aclRightsInfoVoid)
{
    TNavigatorImapConnection *imapConnection = 
        (TNavigatorImapConnection *) blockingConnectionVoid;
    ActiveEntry *ce = imapConnection->GetActiveEntry();
    
	TIMAPACLRightsInfo *aclRights = (TIMAPACLRightsInfo *)aclRightsInfoVoid;

	MSG_AddFolderRightsForUser(ce->window_id->mailMaster, aclRights->hostName, aclRights->mailboxName, aclRights->userName, aclRights->rights);

	imapConnection->NotifyEventCompletionMonitor();
}

void TNavigatorImapConnection::AddFolderRightsForUser(const char *mailboxName, const char *userName, const char *rights)
{
    TIMAPACLRightsInfo *aclRightsInfo = new TIMAPACLRightsInfo();
	if (aclRightsInfo)
	{
		aclRightsInfo->hostName = XP_STRDUP(fCurrentUrl->GetUrlHost());
		aclRightsInfo->mailboxName = XP_STRDUP(mailboxName);
		if (userName)
			aclRightsInfo->userName = XP_STRDUP(userName);
		else
			aclRightsInfo->userName = NULL;
		aclRightsInfo->rights = XP_STRDUP(rights);
		

		if (aclRightsInfo->hostName && aclRightsInfo->mailboxName && aclRightsInfo->rights && 
			userName ? (aclRightsInfo->userName != NULL) : TRUE)
		{

			TImapFEEvent *addFolderRightsEvent = 
				new TImapFEEvent(AddFolderRightsEvent,              // function to call
								this,                                   // access to current entry/context
								aclRightsInfo,
								FALSE);
                                                        

			if (addFolderRightsEvent)
			{
				fFEEventQueue->AdoptEventToEnd(addFolderRightsEvent);
		        WaitForFEEventCompletion();
			}
			else
				HandleMemoryFailure();

			XP_FREE(aclRightsInfo->hostName);
			XP_FREE(aclRightsInfo->mailboxName);
			XP_FREE(aclRightsInfo->rights);
			if (aclRightsInfo->userName)
				XP_FREE(aclRightsInfo->userName);
		}
		else
			HandleMemoryFailure();
		delete aclRightsInfo;
	}
	else
		HandleMemoryFailure();
}

static void RefreshFolderRightsEvent(void *blockingConnectionVoid, void *aclRightsInfoVoid)
{
    TNavigatorImapConnection *imapConnection = 
        (TNavigatorImapConnection *) blockingConnectionVoid;
    ActiveEntry *ce = imapConnection->GetActiveEntry();
    
	TIMAPACLRightsInfo *aclRights = (TIMAPACLRightsInfo *)aclRightsInfoVoid;

	MSG_RefreshFolderRightsViewForFolder(ce->window_id->mailMaster, aclRights->hostName, aclRights->mailboxName);

	imapConnection->NotifyEventCompletionMonitor();
}

void TNavigatorImapConnection::RefreshFolderACLView(const char *mailboxName)
{
    TIMAPACLRightsInfo *aclRightsInfo = new TIMAPACLRightsInfo();
	if (aclRightsInfo)
	{
		aclRightsInfo->hostName = XP_STRDUP(fCurrentUrl->GetUrlHost());
		aclRightsInfo->mailboxName = XP_STRDUP(mailboxName);
		aclRightsInfo->rights = NULL;
		aclRightsInfo->userName = NULL;

		if (aclRightsInfo->hostName && aclRightsInfo->mailboxName)
		{

			TImapFEEvent *refreshFolderRightsEvent = 
				new TImapFEEvent(RefreshFolderRightsEvent,              // function to call
								this,                                   // access to current entry/context
								aclRightsInfo,
								FALSE);
                                                        

			if (refreshFolderRightsEvent)
			{
				fFEEventQueue->AdoptEventToEnd(refreshFolderRightsEvent);
		        WaitForFEEventCompletion();
			}
			else
				HandleMemoryFailure();

			XP_FREE(aclRightsInfo->hostName);
			XP_FREE(aclRightsInfo->mailboxName);
		}
		else
			HandleMemoryFailure();
		delete aclRightsInfo;
	}
	else
		HandleMemoryFailure();
}


static void
MOZTHREAD_FolderNeedsACLInitialized(void *blockingConnectionVoid, void *valueVoid)
{
    TNavigatorImapConnection *imapConnection = 
        (TNavigatorImapConnection *) blockingConnectionVoid;	
    ActiveEntry *ce = imapConnection->GetActiveEntry();
	FolderQueryInfo *value = (FolderQueryInfo *)valueVoid;

	value->rv = !MSG_IsFolderACLInitialized(ce->window_id->mailMaster, value->name, value->hostName);
    imapConnection->NotifyEventCompletionMonitor();
}


XP_Bool TNavigatorImapConnection::FolderNeedsACLInitialized(const char *folderName)
{
	FolderQueryInfo *value = (FolderQueryInfo *)XP_ALLOC(sizeof(FolderQueryInfo));
	XP_Bool rv = FALSE;

	if (!value) return FALSE;

	value->name = XP_STRDUP(folderName);
	if (!value->name)
	{
		XP_FREE(value);
		return FALSE;
	}

	value->hostName = XP_STRDUP(fCurrentUrl->GetUrlHost());
	if (!value->hostName)
	{
		XP_FREE(value->name);
		XP_FREE(value);
		return FALSE;
	}

	TImapFEEvent *folderACLInitEvent = 
	    new TImapFEEvent(MOZTHREAD_FolderNeedsACLInitialized,               // function to call
	                    this,                                              // access to current entry/context
	                    value, FALSE);

	if (folderACLInitEvent)
	{
	    fFEEventQueue->AdoptEventToEnd(folderACLInitEvent);
		WaitForFEEventCompletion();
	}
	else
	    HandleMemoryFailure();

	rv = value->rv;
	XP_FREE(value->hostName);
	XP_FREE(value->name);
	XP_FREE(value);
	return rv;
}

/////////////////// End ACL Stuff /////////////////////////////////////////////////////

XP_Bool TNavigatorImapConnection::InitializeFolderUrl(const char *folderName)
{
	FolderQueryInfo *value = (FolderQueryInfo *)XP_ALLOC(sizeof(FolderQueryInfo));
	XP_Bool rv = FALSE;

	if (!value) return FALSE;

	value->name = XP_STRDUP(folderName);
	if (!value->name)
	{
		XP_FREE(value);
		return FALSE;
	}

	value->hostName = XP_STRDUP(fCurrentUrl->GetUrlHost());
	if (!value->hostName)
	{
		XP_FREE(value->name);
		XP_FREE(value);
		return FALSE;
	}

	TImapFEEvent *folderURLInitEvent = 
	    new TImapFEEvent(MOZ_THREADmsgSetFolderURL,               // function to call
	                    this,                                              // access to current entry/context
	                    value, TRUE);

	if (folderURLInitEvent)
	{
	    fFEEventQueue->AdoptEventToEnd(folderURLInitEvent);
		WaitForFEEventCompletion();
	}
	else
	    HandleMemoryFailure();

	rv = value->rv;
	XP_FREE(value->hostName);
	XP_FREE(value->name);
	XP_FREE(value);
	return rv;
}


static void
MOZTHREAD_GetShowAttachmentsInline(void *blockingConnectionVoid, void *valueVoid)
{
    TNavigatorImapConnection *imapConnection = 
        (TNavigatorImapConnection *) blockingConnectionVoid;	
	XP_Bool *value = (XP_Bool *)valueVoid;

	PREF_GetBoolPref("mail.inline_attachments", value);

    imapConnection->NotifyEventCompletionMonitor();
}

XP_Bool	TNavigatorImapConnection::GetShowAttachmentsInline()
{
	XP_Bool *value = (XP_Bool *)XP_ALLOC(sizeof(XP_Bool));
	XP_Bool rv = FALSE;

	if (!value) return FALSE;

	TImapFEEvent *levent = 
	    new TImapFEEvent(MOZTHREAD_GetShowAttachmentsInline,               // function to call
	                    this,                                              // access to current entry/context
	                    value, FALSE);

	if (levent)
	{
	    fFEEventQueue->AdoptEventToEnd(levent);
		WaitForFEEventCompletion();
	}
	else
	    HandleMemoryFailure();

	rv = *value;
	XP_FREE(value);
	return rv;
}

void TNavigatorImapConnection::SetContentModified(XP_Bool modified)
{
	ActiveEntry *ce = GetActiveEntry();
	if (ce  && !DeathSignalReceived())
		ce->URL_s->content_modified = modified;
}

XP_Bool	TNavigatorImapConnection::GetShouldFetchAllParts()
{
	ActiveEntry *ce = GetActiveEntry();
	if (ce  && !DeathSignalReceived())
		return ce->URL_s->content_modified;
	else
		return TRUE;
}

void TNavigatorImapConnection::Log(const char *logSubName, const char *extraInfo, const char *logData)
{
	static char *nonAuthStateName = "NA";
	static char *authStateName = "A";
	static char *selectedStateName = "S";
	static char *waitingStateName = "W";
	char *stateName = NULL;

	switch (GetServerStateParser().GetIMAPstate())
	{
	case TImapServerState::kFolderSelected:
		if (fCurrentUrl)
		{
			if (extraInfo)
				PR_LOG(IMAP, out, ("%s:%s-%s:%s:%s: %s", fCurrentUrl->GetUrlHost(),selectedStateName, GetServerStateParser().GetSelectedMailboxName(), logSubName, extraInfo, logData));
			else
				PR_LOG(IMAP, out, ("%s:%s-%s:%s: %s", fCurrentUrl->GetUrlHost(),selectedStateName, GetServerStateParser().GetSelectedMailboxName(), logSubName, logData));
		}
		return;
		break;
	case TImapServerState::kNonAuthenticated:
		stateName = nonAuthStateName;
		break;
	case TImapServerState::kAuthenticated:
		stateName = authStateName;
		break;
	case TImapServerState::kWaitingForMoreClientInput:
		stateName = waitingStateName;
		break;
	}

	if (fCurrentUrl)
	{
		if (extraInfo)
			PR_LOG(IMAP, out, ("%s:%s:%s:%s: %s",fCurrentUrl->GetUrlHost(),stateName,logSubName,extraInfo,logData));
		else
			PR_LOG(IMAP, out, ("%s:%s:%s: %s",fCurrentUrl->GetUrlHost(),stateName,logSubName,logData));
	}
}


// essentially, just a copy of parseadoptedmsgline, except it doesn't notify the eventcompletion monitor
static
void MOZTHREAD_TunnelOutStream(void *blockingConnectionVoid, void *adoptedmsg_line_info_Void)
{
    TNavigatorImapConnection *imapConnection = 
        (TNavigatorImapConnection *) blockingConnectionVoid;
    ActiveEntry *ce = imapConnection->GetActiveEntry();
    
    msg_line_info *downloadLineDontDelete = (msg_line_info *) adoptedmsg_line_info_Void;
    NET_StreamClass *outputStream = imapConnection->GetOutputStream();
    
    unsigned int streamBytesMax = (*outputStream->is_write_ready) (outputStream);
    char *stringToPut = downloadLineDontDelete->adoptedMessageLine;
    XP_Bool allocatedString = FALSE;
    
    if ( streamBytesMax < (XP_STRLEN(downloadLineDontDelete->adoptedMessageLine) + 2))
    {
    	// dup the streamBytesMax number of chars, then put the rest of the string back into event queue
    	if (streamBytesMax != 0)
    	{
	    	stringToPut = (char *) XP_ALLOC(streamBytesMax + 1);	// 1 for \0
	    	if (stringToPut)
	    	{
	    		XP_MEMCPY(stringToPut, downloadLineDontDelete->adoptedMessageLine, streamBytesMax);
	    		*(stringToPut + streamBytesMax) = 0;
	    		allocatedString = TRUE;
	    	}
	    	
	    	// shift buffer bytes
	    	XP_MEMMOVE(downloadLineDontDelete->adoptedMessageLine, 
	    			   downloadLineDontDelete->adoptedMessageLine + streamBytesMax, 
	    			   (XP_STRLEN(downloadLineDontDelete->adoptedMessageLine) + 1) - streamBytesMax);
    	}
    	
        // the output stream can't handle this event yet! put it back on the
        // queue
        TImapFEEvent *parseLineEvent = 
            new TImapFEEvent(MOZTHREAD_TunnelOutStream,                   // function to call
                             blockingConnectionVoid,                // access to current entry
                             adoptedmsg_line_info_Void,
							 FALSE);    // line to display
        if (parseLineEvent)
            imapConnection->GetFEEventQueue().
                    AdoptEventToBeginning(parseLineEvent);
        else
            imapConnection->HandleMemoryFailure();
    }


	if (streamBytesMax != 0)
	{
	    imapConnection->SetBytesMovedSoFarForProgress(imapConnection->GetBytesMovedSoFarForProgress() +
	                                                  XP_STRLEN(stringToPut));

		if (!imapConnection->GetPseudoInterrupted())
		{
			int bytesPut = 0;
			
			if (ce->window_id->currentIMAPfolder)
			{
					// the definition of put_block in net.h defines
					// the 3rd parameter as the length of the block,
					// but since we are always sending a null terminated
					// string, I am able to sent the uid instead
				bytesPut = (*outputStream->put_block) (outputStream,
											stringToPut,
											downloadLineDontDelete->uidOfMessage);
			}
			else    // this is a download for display
			{
				bytesPut = (*outputStream->put_block) (outputStream,
											stringToPut,
											XP_STRLEN(stringToPut));
			}
			
			if (bytesPut < 0)
			{
	    		imapConnection->SetConnectionStatus(-1);	// something bad happened, stop this url
	    		imapConnection->GetServerStateParser().SetConnected(FALSE);
			}
		}
    }
	if (allocatedString)
		XP_FREE(stringToPut);	// event not done yet
//	else if (streamBytesMax != 0)
//    	imapConnection->NotifyEventCompletionMonitor();  
}

static void
MOZTHREAD_ProcessTunnel(void *blockingConnectionVoid, void *tunnelInfoVoid)
{
    TNavigatorImapConnection *imapConnection = 
        (TNavigatorImapConnection *) blockingConnectionVoid;
	TunnelInfo *info = (TunnelInfo *)tunnelInfoVoid;

	if (info->maxSize <= 150)
	{
		// We want to read less than the size of a line.
		// Close the tunnel, and switch back to the IMAP thread
		// to do the protocol parsing, etc.
		PR_LOG(IMAP, out, ("TUNNEL: Leaving Mozilla Thread with leftover size %ld",info->maxSize));
	    imapConnection->NotifyTunnelCompletionMonitor();
	}
	else
	{
		// We know there's enough data to read.  So let's do it.

		// Read a line
		char *newLine = NULL;
		XP_Bool pauseForRead = TRUE;
		int socketReadStatus = NET_BufferedReadLine(info->ioSocket,
												&newLine,
												info->inputSocketBuffer,
												info->inputBufferSize,
												&pauseForRead);

		if (socketReadStatus > 0)
		{
			// Set it up to handle reading what's left over
			if (newLine)
			{
				char *lineToStream = PR_smprintf("%s%s",newLine, CRLF);
				if (lineToStream)
				{
					info->maxSize -= XP_STRLEN(lineToStream);
					XP_ASSERT(info->maxSize > 0);
					TImapFEEvent *event = 
						new TImapFEEvent(MOZTHREAD_ProcessTunnel,	// function to call
										 blockingConnectionVoid,	// access to current entry
										 tunnelInfoVoid,
										 FALSE);
					if (event)
						imapConnection->GetFEEventQueue().
								AdoptEventToBeginning(event);
					else
						imapConnection->HandleMemoryFailure();


					// Finally, stream it out

					msg_line_info *outputStreamInfo = (msg_line_info *) XP_ALLOC(sizeof( msg_line_info));
					if (outputStreamInfo)
					{
						outputStreamInfo->adoptedMessageLine = lineToStream;
						// Whoa, actually call one event from another.
						// Let's let it worry about making sure everything is streamed out before we get called here again.
						MOZTHREAD_TunnelOutStream(blockingConnectionVoid, outputStreamInfo);
						XP_FREE(outputStreamInfo);
					}
					else
						imapConnection->HandleMemoryFailure();
					XP_FREE(lineToStream);
				}
				else
					imapConnection->HandleMemoryFailure();
			}
			else
			{
				// no line yet, but there might have been data read (filling up the buffer)
				// Put ourselves back on the queue
				TImapFEEvent *event = 
					new TImapFEEvent(MOZTHREAD_ProcessTunnel,	// function to call
									 blockingConnectionVoid,	// access to current entry
									 tunnelInfoVoid,
									 FALSE);
				if (event)
					imapConnection->GetFEEventQueue().
							AdoptEventToBeginning(event);
				else
					imapConnection->HandleMemoryFailure();
			}
		}
		else
		{
			// socketReadStatus <= 0
			int socketError = PR_GetError();

			// If wouldblock, just put this event back on the queue
			if (socketError == XP_ERRNO_EWOULDBLOCK)
			{
				TImapFEEvent *event = 
					new TImapFEEvent(MOZTHREAD_ProcessTunnel,	// function to call
									 blockingConnectionVoid,	// access to current entry
									 tunnelInfoVoid,
									 FALSE);
				if (event)
					imapConnection->GetFEEventQueue().
							AdoptEventToBeginning(event);
				else
					imapConnection->HandleMemoryFailure();
			}
			else
			{
				// Some socket error - handle this here
				XP_ASSERT(FALSE);
			}
		}
	}
}

int32 TNavigatorImapConnection::OpenTunnel (int32 maxNumberOfBytesToRead)
{
	char *bytesToRead = PR_smprintf("Max bytes to read: %ld",maxNumberOfBytesToRead);
	if (bytesToRead)
	{
		Log("TUNNEL","Opening Tunnel", bytesToRead);
		XP_FREE(bytesToRead);
	}
	else
	{
		Log("TUNNEL",NULL,"Opening Tunnel");
	}
	TunnelInfo *info = (TunnelInfo *)XP_ALLOC(sizeof(TunnelInfo));
	int32 originalMax = maxNumberOfBytesToRead, rv = 0;
	if (info)
	{
		info->maxSize = maxNumberOfBytesToRead;
		info->ioSocket = GetIOSocket();
		info->inputSocketBuffer = &fInputSocketBuffer;
		info->inputBufferSize = &fInputBufferSize;

		TImapFEEvent *event = 
			new TImapFEEvent(MOZTHREAD_ProcessTunnel,	// function to call
							this,						// access to current entry/context
							info,						// the tunneling info
							FALSE);						// Interrupts would be bad for this

		if (event)
		{
			fFEEventQueue->AdoptEventToEnd(event);
			//WaitForFEEventCompletion();
			WaitForTunnelCompletion();
		}
		else
			HandleMemoryFailure();

		rv = originalMax - info->maxSize;

		XP_FREE(info);
	}
	char *bytesSoFar = PR_smprintf("Bytes read so far: %ld", rv);
	if (bytesSoFar)
	{
		Log("TUNNEL","Closing Tunnel", bytesSoFar);
		XP_FREE(bytesSoFar);
	}
	else
	{
		Log("TUNNEL",NULL,"Closing Tunnel");
	}
	return rv;
}

int32 TNavigatorImapConnection::GetTunnellingThreshold()
{
	return gTunnellingThreshold;
}

XP_Bool TNavigatorImapConnection::GetIOTunnellingEnabled()
{
	return gIOTunnelling;
}


PR_STATIC_CALLBACK(void)

imap_thread_main_function(void *imapConnectionVoid)
{
    TNavigatorImapConnection    *imapConnection = 
        (TNavigatorImapConnection *) imapConnectionVoid;
    
    imapConnection->StartProcessingActiveEntries();
    
    delete imapConnection;
}

    
/* start a imap4 load
 */
extern int MK_POP3_PASSWORD_UNDEFINED;

//#define KEVIN_DEBUG 1
#if KEVIN_DEBUG
static int gNumberOfVisits;
static int gNumberOfVisitsProcessingEvents;
static int gNumberOfEventsForCurrentUrl;
#endif

static
void NotifyOfflineFolderLoad(TIMAPUrl &currentUrl, MWContext *window_id)
{
	mailbox_spec *notSelectedSpec = (mailbox_spec *) XP_CALLOC(1, sizeof( mailbox_spec));
	if (notSelectedSpec)
	{
		notSelectedSpec->allocatedPathName =currentUrl.CreateCanonicalSourceFolderPathString();
		notSelectedSpec->hostName =currentUrl.GetUrlHost();
		notSelectedSpec->folderSelected = FALSE;
		notSelectedSpec->flagState = NULL;
		notSelectedSpec->onlineVerified = FALSE;
		UpdateIMAPMailboxInfo(notSelectedSpec, window_id);
	}
}

static XP_Bool gotPrefs = FALSE, useLibnetCacheForIMAP4Fetch;
static XP_Bool gotUpgradePrefs = FALSE;
static XP_Bool fTriedSavedPassword = FALSE;
static XP_Bool triedUpgradingToSubscription = FALSE;
static XP_Bool autoSubscribe = TRUE, autoUnsubscribe = TRUE, autoSubscribeOnOpen = TRUE, autoUnsubscribeFromNoSelect = TRUE;
static XP_Bool allowMultipleFolderConnections = TRUE;
static XP_Bool gGetHeaders = FALSE;
static int32 gTooFastTime = 2;
static int32 gIdealTime = 4;
static int32 gChunkAddSize = 2048;
static int32 gChunkSize = 10240;
static int32 gChunkThreshold = 10240 + 4096;
static XP_Bool gFetchByChunks = TRUE;
static int32 gMaxChunkSize = 40960;

/* static */ int
TNavigatorImapConnection::SetupConnection(ActiveEntry * ce, MSG_Master *master, XP_Bool loadingURL)
{
    int32 returnValue = MK_WAITING_FOR_CONNECTION;
    TIMAPUrl currentUrl(ce->URL_s->address, ce->URL_s->internal_url);
	XP_Bool thisHostUsingSubscription = FALSE;
	XP_Bool shouldUpgradeToSubscription = FALSE, upgradeLeaveAlone = FALSE, upgradeSubscribeAll = FALSE;

	if (!gotUpgradePrefs)
	{
		shouldUpgradeToSubscription = MSG_ShouldUpgradeToIMAPSubscription(master);
		
		PREF_GetBoolPref("mail.imap.upgrade.leave_subscriptions_alone",	&upgradeLeaveAlone);
		PREF_GetBoolPref("mail.imap.upgrade.auto_subscribe_to_all",		&upgradeSubscribeAll);

		gotUpgradePrefs = TRUE;
	}

	char *serverMailboxPath = (currentUrl.GetUrlIMAPstate() ==
							   TIMAPUrl::kAuthenticatedStateURL) ? 0 :
		currentUrl.CreateCanonicalSourceFolderPathString(); 

	// if we are appending message even though it is in
	// TIMAPUrl::kAuthenticatedStateURL we should try to reuse the cached
	// connection. The reason for that is that the user might already have the
	// folder opened using a new connection to append message can cause the
	// thread window view out of sync.
	if (!serverMailboxPath && currentUrl.GetIMAPurlType() ==
		TIMAPUrl::kAppendMsgFromFile)
		serverMailboxPath =
			currentUrl.CreateCanonicalSourceFolderPathString();

	XP_Bool	waitingForCachedConnection = FALSE;
	XP_Bool newConnection = FALSE;
	TNavigatorImapConnection *blockingConnection = NULL;
	if (master)     
	{
	    blockingConnection = MSG_UnCacheImapConnection(master, currentUrl.GetUrlHost(), serverMailboxPath);
		// if it's in the cache, by definition, it's not in use, I believe.
		// this code doesn't work if you don't have remember password turned on, because we never make a connection!
		if (!blockingConnection && IMAP_URLIsBiff(ce, currentUrl) 
				&& TNavigatorImapConnection::IsConnectionActive(currentUrl.GetUrlHost(), serverMailboxPath))
		{
			PR_LOG(IMAP, out, ("URL: Biff blocking on active connection %s", ce->URL_s->address));
			return -1;		// we didn't have a cached connection but the inbox is busy, so biff should wait
		}
	}


	if (blockingConnection && (currentUrl.GetIMAPurlType() == TIMAPUrl::kSelectFolder || currentUrl.GetIMAPurlType() == TIMAPUrl::kDeleteAllMsgs))
		blockingConnection->SetNeedsNoop(TRUE);
	if (blockingConnection && 
	    (blockingConnection->GetServerStateParser().GetIMAPstate() == TImapServerState::kFolderSelected) &&
	    (currentUrl.GetUrlIMAPstate() == TIMAPUrl::kAuthenticatedStateURL) )
	{
	    // We dont want to be in the selected state in case we operate on the selected mailbox
	    // and we do not want an implicit Close() so shut this connection down
	    IMAP_TerminateConnection(blockingConnection);
	    blockingConnection = NULL;
	}
	ImapConData *newConnectionData = (ImapConData *) ce->con_data;

	if (blockingConnection)
	{
		MSG_SetFolderRunningIMAPUrl(ce->URL_s->msg_pane, currentUrl.GetUrlHost(), serverMailboxPath, MSG_RunningOnline);
		// let's use the cached connection
	    newConnectionData->netConnection = blockingConnection;
	    ce->socket = blockingConnection->GetIOSocket();
	    ce->con_sock = ce->socket;

		ce->local_file = FALSE;
	    NET_TotalNumberOfOpenConnections++;
	    NET_SetReadSelect(ce->window_id, ce->socket);
	    FE_SetProgressBarPercent(ce->window_id, -1);
	    // I don't think I need to call FE_SetConnectSelect since we are already connected
	}
	else
	{
		if (!allowMultipleFolderConnections)
		{
			XP_Bool folderActive = TNavigatorImapConnection::IsConnectionActive(currentUrl.GetUrlHost(), serverMailboxPath);
			if (folderActive)
			{
				waitingForCachedConnection = TRUE;
				if (loadingURL)
					PR_LOG(IMAP, out, ("URL: Blocking on active connection %s", ce->URL_s->address));

				ce->local_file = TRUE;
				// Note: Chrisf: This needs to be a PRFileDesc, we're not sure how to do this so we'll leave it for you
				// to fix when you get back...
//				ce->socket = 1;
				if (loadingURL)
					NET_SetCallNetlibAllTheTime(ce->window_id, "mkimap4");
			}
		}
		// create the blocking connection object
		if (!waitingForCachedConnection)
		{
			HG73299
				blockingConnection = TNavigatorImapConnection::Create(currentUrl.GetUrlHost());
		}
	    if (blockingConnection)
	    {
			MSG_FolderInfo *folder = MSG_SetFolderRunningIMAPUrl(ce->URL_s->msg_pane, currentUrl.GetUrlHost(), serverMailboxPath, MSG_RunningOnline);
			// Find out if delete is move-to-trash
			XP_Bool deleteIsMoveToTrash = MSG_GetIMAPHostDeleteIsMoveToTrash(master, currentUrl.GetUrlHost());
			int	threadPriority = 31;  // medium priority was 15, but that may have slowed down thru-put
#ifdef XP_UNIX
			threadPriority = 31;     // highest priority for unix, especially SSL
#endif


	        newConnection = TRUE;
	        // start the imap thread
	        PRThread *imapThread = PR_CreateThread(PR_USER_THREAD, imap_thread_main_function, blockingConnection,
												   PR_PRIORITY_NORMAL, PR_LOCAL_THREAD, PR_UNJOINABLE_THREAD,
												   0);		// standard stack
	        // save the thread object so an IMAP interrupt will
	        // cause the TIMAP4BlockingConnection destructor to
	        // destroy it.                                                                          
	        blockingConnection->SetBlockingThread(imapThread);
	        newConnectionData->netConnection = blockingConnection;

			XP_Bool folderNeedsSubscribing = FALSE;
			XP_Bool folderNeedsACLRefreshed = FALSE;
			if (folder)
			{
				folderNeedsSubscribing = MSG_GetFolderNeedsSubscribing(folder);
				folderNeedsACLRefreshed = MSG_GetFolderNeedsACLListed(folder);
			}
			blockingConnection->SetFolderInfo(folder, folderNeedsSubscribing, folderNeedsACLRefreshed);		
			
			blockingConnection->SetSubscribeParameters(autoSubscribe, autoUnsubscribe,
					autoSubscribeOnOpen, autoUnsubscribeFromNoSelect, shouldUpgradeToSubscription, upgradeLeaveAlone, upgradeSubscribeAll);
			blockingConnection->SetDeleteIsMoveToTrash(deleteIsMoveToTrash);
			blockingConnection->Configure(gGetHeaders, gTooFastTime, gIdealTime, gChunkAddSize, gChunkSize,
										gChunkThreshold, gFetchByChunks, gMaxChunkSize);
		}
	}
	
	if (blockingConnection)
	{
		blockingConnection->SetActive(TRUE);
		if (ce->window_id &&
			((ce->window_id->type == MWContextMail) ||
			 (ce->window_id->type == MWContextNews)))
		{
			FE_UpdateStopState(ce->window_id);
		}
		PR_Sleep(PR_INTERVAL_NO_WAIT);	            
	    blockingConnection->SubmitActiveEntry(ce, newConnection);

		// force netlib to be called all the time, whether there is IO
	    // or not.  This enables us to process the fe event queue.
	    // net_call_all_the_time_count is a global used by the existing netlib
		//if(net_call_all_the_time_count < 1)
		if (loadingURL)
			NET_SetCallNetlibAllTheTime(ce->window_id, "mkimap4");
	    blockingConnection->SetCallingNetlibAllTheTime(TRUE);

	    // msg copy code has to use the TNavigatorImapConnection 'C' interface
	    if (ce->window_id->msgCopyInfo)
	        MSG_StoreNavigatorIMAPConnectionInMoveState(ce->window_id, blockingConnection);
		ce->window_id->imapConnection = blockingConnection;
	    
	    
#if KEVIN_DEBUG
		gNumberOfVisits=0;
		gNumberOfVisitsProcessingEvents=0;
		gNumberOfEventsForCurrentUrl=0;
#endif
	    returnValue = NET_ProcessIMAP4(ce);
	}
	else if (!waitingForCachedConnection)
	{
	    FE_Alert(ce->window_id, XP_GetString(MK_OUT_OF_MEMORY));
	    returnValue = -1;
	}
    FREEIF(serverMailboxPath);
	return returnValue;
} 

MODULE_PRIVATE int32
NET_IMAP4Load (ActiveEntry * ce)
{
    int returnValue=0;

	PR_LOG(IMAP, out, ("URL: Loading %s",ce->URL_s->address));

	// Get the master and a master Pane.  We will need them a few times later.
   	MSG_Pane *masterPane = ce->URL_s->msg_pane;		// we need some pane to get to the master, libimg will run fetch urls to get mime parts
   	if (!masterPane)
   		masterPane = MSG_FindPane(ce->window_id, MSG_ANYPANE);	// last ditch effort, GROSS!
	MSG_Master *master = ce->window_id->mailMaster;
	if (!master)
	{
		master = MSG_GetMaster(masterPane);
		ce->window_id->mailMaster = master;
	}
	XP_ASSERT(master);

	// check to see if we have all the prefs we need for this load
	if (!gotPrefs)
	{
		// cacheing pref
		PREF_GetBoolPref("mail.imap.cache_fetch_responses",&useLibnetCacheForIMAP4Fetch);

		PREF_GetBoolPref("mail.imap.allow_multiple_folder_connections", &allowMultipleFolderConnections);
		
		// subscription prefs
		PREF_GetBoolPref("mail.imap.auto_subscribe",&autoSubscribe);
		PREF_GetBoolPref("mail.imap.auto_unsubscribe", &autoUnsubscribe);
		PREF_GetBoolPref("mail.imap.auto_subscribe_on_open",&autoSubscribeOnOpen);
		PREF_GetBoolPref("mail.imap.auto_unsubscribe_from_noselect_folders",&autoUnsubscribeFromNoSelect);
		
		// chunk size and header prefs
		PREF_GetBoolPref("mail.imap.new_mail_get_headers", &gGetHeaders);
		PREF_GetIntPref("mail.imap.chunk_fast", &gTooFastTime);		// secs we read too little too fast
		PREF_GetIntPref("mail.imap.chunk_ideal", &gIdealTime);		// secs we read enough in good time
		PREF_GetIntPref("mail.imap.chunk_add", &gChunkAddSize);		// buffer size to add when wasting time
		PREF_GetIntPref("mail.imap.chunk_size", &gChunkSize);
		PREF_GetIntPref("mail.imap.min_chunk_size_threshold",	&gChunkThreshold);
		PREF_GetBoolPref("mail.imap.fetch_by_chunks", &gFetchByChunks);
		PREF_GetIntPref("mail.imap.max_chunk_size", &gMaxChunkSize);

		// bodystructure prefs
		PREF_GetBoolPref("mail.imap.mime_parts_on_demand", &gMIMEOnDemand);
		PREF_GetIntPref("mail.imap.mime_parts_on_demand_threshold", &gMIMEOnDemandThreshold);
		PREF_GetBoolPref("mail.imap.optimize_header_dl", &gOptimizedHeaders);

		PREF_GetIntPref("mail.imap.tunnelling_threshold", &gTunnellingThreshold);
		PREF_GetBoolPref("mail.imap.io_tunnelling", &gIOTunnelling);

		gotPrefs = TRUE;
	}

	ce->window_id->imapConnection = NULL;
    // this might be simple note that the filtering is done
    if (!XP_STRCMP(ce->URL_s->address, kImapFilteringCompleteURL))
    {
    	MSG_ImapInboxFilteringComplete(ce->URL_s->msg_pane);
    	return -1;
    }
    
    // this might be simple note that the on/off synch is done
    if (!XP_STRCMP(ce->URL_s->address, kImapOnOffSynchCompleteURL))
    {
    	MSG_ImapOnOffLineSynchComplete(ce->URL_s->msg_pane);
    	return -1;
    }
    
	// if this url is bogus, be defensive and leave before we make a connection
    TIMAPUrl currentUrl(ce->URL_s->address, ce->URL_s->internal_url);
    if (!currentUrl.ValidIMAPUrl())
    	return -1;

	const char *password = NULL;

	TIMAPHostInfo::AddHostToList(currentUrl.GetUrlHost());

    // try cached password for this host; if this fails, ask libmsg.
	XP_Bool savePassword = FALSE;
	password = TIMAPHostInfo::GetPasswordForHost(currentUrl.GetUrlHost()) ;
	if (!password)
	{
		savePassword = TRUE;
		password  = MSG_GetIMAPHostPassword(ce->window_id->mailMaster, currentUrl.GetUrlHost());
	}
/*	if (!fTriedSavedPassword) */ {
		// Try getting the password from the prefs, if it was saved
		if (savePassword && password && (XP_STRLEN(password) > 0)) {	// make sure we actually have one in the prefs
			TIMAPHostInfo::SetPasswordForHost(currentUrl.GetUrlHost(), password);
		}
		fTriedSavedPassword = TRUE;
	}

    // if this is a biff and we have no valid password, then bail before we make a connection
    if (IMAP_URLIsBiff(ce, currentUrl)) 
	{
		if (!MSG_GetIMAPHostIsSecure(master, currentUrl.GetUrlHost()) && !password)
			return -1;
	}

    // if we want to use the libnet cache:
	// if this is not a message fetch, make sure this url is not cached.  How can
    // libnet cache a load mailbox command?  duh.
    if (!useLibnetCacheForIMAP4Fetch ||
		((currentUrl.GetIMAPurlType() != TIMAPUrl::kMsgFetch) ||
        currentUrl.MimePartSelectorDetected()))
    	ce->format_out = CLEAR_CACHE_BIT(ce->format_out);

	// If this is a message fetch URL
	// See if it is a force-reload-all-attachments URL.  If so, strip off the
	int whereStart = XP_STRLEN(ce->URL_s->address) - 9;  // 9 == XP_STRLEN("&allparts")
	if (whereStart > 0)
	{
		if (!XP_STRNCASECMP(ce->URL_s->address + whereStart, "&allparts", 9))
		{
			// It is a force reload all - strip off the "&allparts"
			*(ce->URL_s->address+whereStart) = 0;
			ce->URL_s->content_modified = TRUE;	// tiny hack.  Set this bit to mean this is a
											// forced reload.
		}
	}    

    ImapConData *newConnectionData = (ImapConData *) XP_CALLOC(sizeof(ImapConData), 1);
    if (!newConnectionData)
    {
        FE_Alert(ce->window_id, XP_GetString(MK_OUT_OF_MEMORY));
        return -1;
    }
	ce->con_data = newConnectionData;
    newConnectionData->offLineRetrievalData = NULL;
    newConnectionData->offlineDisplayStream = NULL;
    
    if ((currentUrl.GetIMAPurlType() == TIMAPUrl::kMsgFetch) && (currentUrl.MessageIdsAreUids()) )
    {
    	char *serverMailboxPath = currentUrl.CreateCanonicalSourceFolderPathString();
    	char *msgIdString = currentUrl.CreateListOfMessageIdsString();
    	
    	MSG_Pane *masterPane = ce->URL_s->msg_pane;		// we need some pane to get to the master, libimg will run fetch urls to get mime parts
    	if (!masterPane)
    		masterPane = MSG_FindPane(ce->window_id, MSG_ANYPANE);	// last ditch effort, GROSS!
    	
    	newConnectionData->offLineMsgKey = atoint32(msgIdString);
    	newConnectionData->offLineMsgFlags = MSG_GetImapMessageFlags(masterPane, currentUrl.GetUrlHost(), serverMailboxPath, newConnectionData->offLineMsgKey );

    	if ((newConnectionData->offLineMsgFlags & MSG_FLAG_OFFLINE) || NET_IsOffline())
    	{
			MSG_SetFolderRunningIMAPUrl(masterPane, currentUrl.GetUrlHost(), serverMailboxPath, MSG_RunningOffline);
			ce->local_file = TRUE;
			ce->socket = NULL;
			NET_SetCallNetlibAllTheTime(ce->window_id, "mkimap4");
			/// do not cache this url since its already local
			ce->format_out = CLEAR_CACHE_BIT(ce->format_out);

			MSG_StartOfflineImapRetrieval(masterPane, 
											currentUrl.GetUrlHost(), serverMailboxPath, 
											newConnectionData->offLineMsgKey, 
											&newConnectionData->offLineRetrievalData);
			
			returnValue = NET_ProcessIMAP4(ce);
    	}
    	
    	FREEIF(serverMailboxPath);
    	FREEIF(msgIdString);
    }
    
    if (!newConnectionData->offLineRetrievalData && !NET_IsOffline())
    {
		returnValue = TNavigatorImapConnection::SetupConnection(ce, master, TRUE);
    }
    else if (!newConnectionData->offLineRetrievalData && NET_IsOffline())
    {
    	// we handled offline fetch earlier, now handle offline select
    	if (currentUrl.GetIMAPurlType() == TIMAPUrl::kSelectFolder)
    		NotifyOfflineFolderLoad(currentUrl, ce->window_id);
    	returnValue = MK_OFFLINE;
    }


    return returnValue;
}

// this is called from the memory cache to notify us that a cached imap message has finished loading 
void IMAP_URLFinished(URL_Struct *URL_s)
{
	TIMAPUrl currentUrl(URL_s->address, URL_s->internal_url);
    char *serverMailboxPath = currentUrl.CreateCanonicalSourceFolderPathString();
	MSG_FolderInfo *folder = MSG_SetFolderRunningIMAPUrl(URL_s->msg_pane, currentUrl.GetUrlHost(), serverMailboxPath, MSG_NotRunning);
	FREEIF(serverMailboxPath);
	MSG_IMAPUrlFinished(folder, URL_s);
}

static MSG_FolderInfo *imap_clear_running_imap_url(ActiveEntry *ce)
{
	TIMAPUrl currentUrl(ce->URL_s->address, ce->URL_s->internal_url);
    char *serverMailboxPath = currentUrl.CreateCanonicalSourceFolderPathString();
	MSG_FolderInfo *folder = MSG_SetFolderRunningIMAPUrl(ce->URL_s->msg_pane, currentUrl.GetUrlHost(), serverMailboxPath, MSG_NotRunning);
	FREEIF(serverMailboxPath);
	return folder;
}


/* NET_ProcessOfflineIMAP4  will control the state machine that
 * loads messages from a imap db
 *
 * returns negative if the transfer is finished or error'd out
 *
 * returns zero or more if the transfer needs to be continued.
 */
static int
NET_ProcessOfflineIMAP4 (ActiveEntry *ce, ImapConData *cd)
{	
	int keepCallingMe = 1;
	
	if (!cd->offlineDisplayStream)
	{
        if (ce->format_out == FO_PRESENT || ce->format_out == FO_CACHE_AND_PRESENT)
        {
            IMAP_InitializeImapFeData(ce);
        }
        
		ce->URL_s->content_length = MSG_GetOfflineMessageSize(cd->offLineRetrievalData);
		StrAllocCopy(ce->URL_s->content_type, ce->URL_s->content_length ? MESSAGE_RFC822 : TEXT_HTML);
		cd->offlineDisplayStream = NET_StreamBuilder(ce->format_out, ce->URL_s, ce->window_id); 
        if (!cd->offlineDisplayStream)
        	ce->status = -1;      
	}
	
	if (ce->status >= 0)
	{
		int32 read_size = (*cd->offlineDisplayStream->is_write_ready) (cd->offlineDisplayStream);
		if(!read_size)
			return(0);  /* wait until we are ready to write */
		else
			read_size = MIN(read_size, NET_Socket_Buffer_Size);

		ce->status = MSG_ProcessOfflineImap(cd->offLineRetrievalData, NET_Socket_Buffer, read_size);
		if(ce->status > 0)
		{
			ce->bytes_received += ce->status;
			(*cd->offlineDisplayStream->put_block) (cd->offlineDisplayStream, NET_Socket_Buffer, ce->status);
		}

		if (ce->status == 0)
		{
			if (cd->offlineDisplayStream)
				(*cd->offlineDisplayStream->complete) (cd->offlineDisplayStream);
			NET_ClearCallNetlibAllTheTime(ce->window_id, "mkimap4");
			
			// clean up the offline info, in case we need to mark read online
			ce->local_file = FALSE;
			ce->socket = 0;
			cd->offLineRetrievalData = NULL;
			keepCallingMe = -1;
		}
	}
	
	if (ce->status < 0)
		keepCallingMe = -1;

	// here we need to check if we have offline events saved up while we were running this url
	// (and not plain old offline events getting played back). And we need to figure out how to run
	// them from here...because we will interrupt current url if not careful.
	if (keepCallingMe < 0)
	{
		// tell libmsg that we're not running an imap url for this folder anymore
		MSG_FolderInfo *folder = imap_clear_running_imap_url(ce);
		MSG_IMAPUrlFinished(folder, ce->URL_s);
	}

	return keepCallingMe;
}




static void imap_interrupt_possible_libmsg_select(ActiveEntry *ce)
{
	// if this was a folder load then tell libmsg it was interrupted
	TIMAPUrl interruptedURL(ce->URL_s->address, ce->URL_s->internal_url);
	if (interruptedURL.GetIMAPurlType() == TIMAPUrl::kSelectFolder)
	{
    	char *folderName = interruptedURL.CreateCanonicalSourceFolderPathString();
    	MSG_InterruptImapFolderLoad( 	MSG_FindPaneOfContext(ce->window_id, MSG_ANYPANE), 
    									interruptedURL.GetUrlHost(), folderName);
    	FREEIF(folderName);
	}
}

void	TNavigatorImapConnection::ResetProgressInfo()
{
	LL_I2L(fLastProgressTime, 0);
	fLastPercent = -1;
	fLastProgressStringId = -1;
}

/* NET_ProcessIMAP4  will control the state machine that
 * loads messages from a imap4 server
 *
 * returns negative if the transfer is finished or error'd out
 *
 * returns zero or more if the transfer needs to be continued.
 */
MODULE_PRIVATE int32
NET_ProcessIMAP4 (ActiveEntry *ce)
{
#if KEVIN_DEBUG
		gNumberOfVisits++;
#endif
    MSG_Master *currentMaster = ce->window_id->mailMaster;
	if (!currentMaster && ce->URL_s->msg_pane)        // if there is no pane or master, then I can't find the master where the cache is
		currentMaster = MSG_GetMaster(ce->URL_s->msg_pane);

	ImapConData *cd = (ImapConData *) ce->con_data;
	XP_Bool shouldDieAtEnd = FALSE;

	if (cd->offLineRetrievalData)
	{
		return NET_ProcessOfflineIMAP4(ce, cd);
	}
	
    TNavigatorImapConnection *imapConnection = cd->netConnection;
	if (!imapConnection)
	{
		TNavigatorImapConnection::SetupConnection(ce, currentMaster, FALSE);
		return MK_WAITING_FOR_CONNECTION;
	}
    assert(imapConnection);
    
    // if this is a move/copy operation, be sure to let libmsg do its thing.
    if (imapConnection->CurrentConnectionIsMove() && ce->window_id->msgCopyInfo &&
        !MSG_IsMessageCopyFinished(ce->window_id->msgCopyInfo))
    {
        int finishReturn = MSG_FinishCopyingMessages(ce->window_id);

        if (finishReturn < 0)
        {
        	// handled in ReportSuccessOfOnlineCopy
            
        }
    }
	
		// NotifyIOCompletionMonitor even if we end up here because we are calling netlib all of the time.
		// an extra NotifyIOCompletionMonitor will cause one extra poll of the port.
		// This can happen after we finish processing a bunch of fe events and go
		// into a BlockedForIO state.  So, we might randomly do one extra poll per 
		// blocked io read.  After this poll, calling netlib off the time will 
		// be turned off if we are still blocked on io.
    if (imapConnection->BlockedForIO())
    {
        // we entered NET_ProcessIMAP4 because a net read is finished
        imapConnection->NotifyIOCompletionMonitor();    // will give time to imap thread
    }
    else
        PR_Sleep(PR_INTERVAL_NO_WAIT);     // give the imap thread some time.
        
    int connectionStatus = imapConnection->GetConnectionStatus();

    int eventLimitNotReached = 2;   // if we don't limit the events, then huge downloads
                                    // are effectively synchronous

#if KEVIN_DEBUG
	if (imapConnection->GetFEEventQueue().NumberOfEventsInQueue())
		gNumberOfVisitsProcessingEvents++;
#endif

    while (eventLimitNotReached-- && (connectionStatus >= 0) && 
            imapConnection->GetFEEventQueue().NumberOfEventsInQueue())
    {       
        if (imapConnection->GetFEEventQueue().NumberOfEventsInQueue())
        {
            TImapFEEvent *event = 
                imapConnection->GetFEEventQueue().OrphanFirstEvent();
            event->DoEvent();
            delete event;
#if KEVIN_DEBUG
		gNumberOfEventsForCurrentUrl++;
#endif
        }
        
        // PR_Sleep(PR_INTERVAL_NO_WAIT);  // when this yield was here, a huge message download,
                        // now that it is buffered, always caused an event
                        // in the queue but never tripped eventLimitNotReached.
                        // Since each buffered event is relatively slow, (e.g.
                        // html parsing 1500 bytes) the effect was to cause
                        // synchronous downloads
        connectionStatus = imapConnection->GetConnectionStatus();
    }
    
    //if (eventLimitNotReached == -1)
    //      DebugStr("\pTouch the sky");
    
        
    if (connectionStatus < 0)
    {
        // we are done or errored out
		ce->window_id->imapConnection = NULL;
        XP_Bool diedWhileCallingNetlibSet = imapConnection->GetCallingNetlibAllTheTime();
        
        // some events can fail and put another event in the queue
        // this is rare and catastrophic anyway so only process the number
        // of events we know about when we start this loop
        int numberOfEventsToProcess = imapConnection->GetFEEventQueue().NumberOfEventsInQueue();
        while (numberOfEventsToProcess-- && imapConnection->GetFEEventQueue().NumberOfEventsInQueue())
        {
            TImapFEEvent *event = 
                imapConnection->GetFEEventQueue().OrphanFirstEvent();
            event->DoEvent();
            delete event;
#if KEVIN_DEBUG
			gNumberOfEventsForCurrentUrl++;
#endif
        }
        imapConnection->SignalEventQueueEmpty();
        
		// tell libmsg that we're not running an imap url for this folder anymore
		MSG_FolderInfo *folder = imap_clear_running_imap_url(ce);

        FREEIF(ce->con_data);
        if (ce->socket != 0)
        {
            // stop calling me, i am finished
	        NET_ClearReadSelect(ce->window_id, ce->socket);
	        NET_ClearConnectSelect(ce->window_id, ce->socket);
	        NET_TotalNumberOfOpenConnections--;
        
            // maybe we can cache this connection
            XP_Bool wasCached = FALSE;

            if (currentMaster && 	// do not cache a non-authenticated or disconnected connection.
            	(imapConnection->GetServerStateParser().GetIMAPstate() != TImapServerState::kNonAuthenticated) &&
            	(imapConnection->GetServerStateParser().Connected()) &&
            	!imapConnection->GetServerStateParser().SyntaxError())
            {
				TIMAPUrl currentUrl(ce->URL_s->address, ce->URL_s->internal_url);
				imapConnection->ResetProgressInfo();

				wasCached = MSG_TryToCacheImapConnection(currentMaster, currentUrl.GetUrlHost(),
														 imapConnection->GetServerStateParser().GetSelectedMailboxName(),
                                                         imapConnection);
            }
            if (!wasCached)
            {
				char *logoutString = PR_smprintf("xxxx logout\r\n");

				if (logoutString) 
				{
					PR_LOG(IMAP, out, ("%s",logoutString));
					PR_LOG(IMAP, out, ("Logged out from NET_ProcessIMAP4"));
					NET_BlockingWrite(ce->socket, logoutString, XP_STRLEN(logoutString));
					XP_FREEIF(logoutString);
				}

				if (!imapConnection->GetServerStateParser().Connected())
				{
					imap_interrupt_possible_libmsg_select(ce);
				}

				shouldDieAtEnd = TRUE;
				imapConnection->SetIOSocket(NULL);    // delete connection
				net_graceful_shutdown(ce->socket, HG87263);
				PR_Close(ce->socket);                                           
			}
			else
			{
				imapConnection->SetActive(FALSE);
			}
        
        	ce->socket = 0;
        }
        else
        	shouldDieAtEnd = TRUE;
        

        // Whoa!
        // net_call_all_the_time_count is a global used by the existing netlib
	    //net_call_all_the_time_count--;
	    //if(net_call_all_the_time_count < 1)
		if (diedWhileCallingNetlibSet)
	    {
       		NET_ClearCallNetlibAllTheTime(ce->window_id, "mkimap4");
    		imapConnection->SetCallingNetlibAllTheTime(FALSE);
		}
#if KEVIN_DEBUG
		char debugString[250];
		sprintf(debugString, "visits = %ld, usefull visits = %ld, # of events = %ld",
			gNumberOfVisits,
			gNumberOfVisitsProcessingEvents,
			gNumberOfEventsForCurrentUrl);
		DebugStr(c2pstr(debugString));
#endif
		// here we need to check if we have offline events saved up while we were running this url
		// (and not plain old offline events getting played back). And we need to figure out how to run
		// them from here...because we will interrupt current url if not careful.
		MSG_IMAPUrlFinished(folder, ce->URL_s);
    }
    else
    {
    	if (imapConnection->BlockedForIO() && !imapConnection->GetFEEventQueue().NumberOfEventsInQueue())
    	{
    		// if we are blocked and there are no events to process
    		// then stop calling me until io completes
    		if (imapConnection->GetCallingNetlibAllTheTime())
    		{
    			imapConnection->SetCallingNetlibAllTheTime(FALSE);
	       		NET_ClearCallNetlibAllTheTime(ce->window_id, "mkimap4");
    		}
    	}
    	else if (!imapConnection->GetCallingNetlibAllTheTime())
    	{
			// the thread might need to process events, keep entering
			imapConnection->SetCallingNetlibAllTheTime(TRUE);
			NET_SetCallNetlibAllTheTime(ce->window_id, "mkimap4");
    	}
    }
	if (shouldDieAtEnd)
        imapConnection->TellThreadToDie();
	// the imapConnection may now be deleted, so don't use it!
    return connectionStatus;
}


/* abort the connection in progress
 */
MODULE_PRIVATE int32
NET_InterruptIMAP4(ActiveEntry * ce)
{
	PR_LOG(IMAP, out, ("INTERRUPT Entered"));
	ImapConData *cd = (ImapConData *) ce->con_data;
	
    TNavigatorImapConnection *imapConnection = cd->netConnection;
    
	if (!imapConnection || cd->offLineRetrievalData)
	{
		void *offLineRetrievalData = cd->offLineRetrievalData;
    	ce->status = MK_INTERRUPTED;
		ce->window_id->imapConnection = NULL;
	    FREEIF(cd);
		return MSG_InterruptOfflineImap(offLineRetrievalData);
	}
	
    // Whoa!
    // net_call_all_the_time_count is a global used by the existing netlib
	if (imapConnection->GetCallingNetlibAllTheTime())
	{
		imapConnection->SetCallingNetlibAllTheTime(FALSE);
      		NET_ClearCallNetlibAllTheTime(ce->window_id, "mkimap4");
	}

	// tell libmsg that we're not running an imap url for this folder anymore
	imap_clear_running_imap_url(ce);

    // tell the imap thread that we are shutting down
    // pass FALSE and call SetIsSafeToDie later
    imapConnection->TellThreadToDie(FALSE);

    // some events can fail and put another event in the queue
    // this is rare and catastrophic anyway so only process the number
    // of events we know about when we start this loop
    int numberOfEventsToProcess = imapConnection->GetFEEventQueue().NumberOfEventsInQueue();
    while (numberOfEventsToProcess-- && imapConnection->GetFEEventQueue().NumberOfEventsInQueue())
    {
        TImapFEEvent *event = 
            imapConnection->GetFEEventQueue().OrphanFirstEvent();
		if (event->ShouldExecuteWhenInterrupted())
		{
			if (event->GetEventFunction() == NormalEndMsgWriteStream)
				event->SetEventFunction(AbortMsgWriteStream);
			event->DoEvent();
		}
        delete event;
    }
    imapConnection->SignalEventQueueEmpty();
    
    imap_interrupt_possible_libmsg_select(ce);

    FREEIF(ce->con_data);
    ce->status = MK_INTERRUPTED;
    if (ce->socket != 0)
    {
		/* *** imapConnection->TellThreadToDie(FALSE) sends out the
		 * logout command string already
		 *
		char *logoutString = PR_smprintf("xxxx logout\r\n");
		if (logoutString)
		{
			PR_LOG(IMAP, out, ("%s",logoutString));
			PR_LOG(IMAP, out, ("Logged out from NET_InterruptIMAP4"));
			NET_BlockingWrite(ce->socket, logoutString, XP_STRLEN(logoutString));
			XP_FREEIF(logoutString);
		}
		*
		*/
		NET_ClearReadSelect(ce->window_id, ce->socket);
		NET_ClearConnectSelect(ce->window_id, ce->socket);
		TRACEMSG(("Closing and clearing socket ce->socket: %d",
                  ce->socket));
	    imapConnection->SetIOSocket(NULL);
		net_graceful_shutdown(ce->socket, HG83733);
		PR_Close(ce->socket);
		imapConnection->SetIOSocket(NULL);
		NET_TotalNumberOfOpenConnections--;
		ce->socket = 0;
    }

	// tell it to stop
    imapConnection->SetIsSafeToDie();
	// the imapConnection may now be deleted, so don't use it!

	ce->window_id->imapConnection = NULL;

    return MK_INTERRUPTED;
}

/* called on shutdown to clean up */
extern "C" void net_CleanupIMAP4(void);
extern "C" void
net_CleanupIMAP4(void)
{
}

extern "C" void
NET_InitIMAP4Protocol(void)
{
    static NET_ProtoImpl imap4_proto_impl;

    imap4_proto_impl.init = NET_IMAP4Load;
    imap4_proto_impl.process = NET_ProcessIMAP4;
    imap4_proto_impl.interrupt = NET_InterruptIMAP4;
    imap4_proto_impl.resume = NULL;
    imap4_proto_impl.cleanup = net_CleanupIMAP4;

    NET_RegisterProtocolImplementation(&imap4_proto_impl, IMAP_TYPE_URL);
}

TLineDownloadCache::TLineDownloadCache()
{
    fLineInfo = (msg_line_info *) XP_ALLOC(sizeof( msg_line_info));
    fLineInfo->adoptedMessageLine = fLineCache;
    fLineInfo->uidOfMessage = 0;
    fBytesUsed = 0;
}

TLineDownloadCache::~TLineDownloadCache()
{
    FREEIF( fLineInfo);
}

uint32 TLineDownloadCache::CurrentUID()
{
    return fLineInfo->uidOfMessage;
}

uint32 TLineDownloadCache::SpaceAvailable()
{
    return kDownLoadCacheSize - fBytesUsed;
}

msg_line_info *TLineDownloadCache::GetCurrentLineInfo()
{
    return fLineInfo;
}
    
void TLineDownloadCache::ResetCache()
{
    fBytesUsed = 0;
}
    
XP_Bool TLineDownloadCache::CacheEmpty()
{
    return fBytesUsed == 0;
}

void TLineDownloadCache::CacheLine(const char *line, uint32 uid)
{
    uint32 lineLength = XP_STRLEN(line);
    XP_ASSERT((lineLength + 1) <= SpaceAvailable());
    
    fLineInfo->uidOfMessage = uid;
    
    XP_STRCPY(fLineInfo->adoptedMessageLine + fBytesUsed, line);
    fBytesUsed += lineLength;
}

HG00374


TIMAPSocketInfo::TIMAPSocketInfo() :
fIOSocket(0),
pInputSocketBuffer(nil),
pInputBufferSize(nil),
newLine(nil),
pauseForRead(FALSE),
readStatus(-1)
{
}

// TInboxReferenceCount functions

PRMonitor	*TInboxReferenceCount::fgDataSafeMonitor = nil;
int			TInboxReferenceCount::fgInboxUsageCount = 0;

TInboxReferenceCount::TInboxReferenceCount(XP_Bool bumpInboxCount)
{
    if (fgDataSafeMonitor)
    {
    	PR_EnterMonitor(fgDataSafeMonitor);
    	fInboxCountWasBumped = bumpInboxCount;
    	if (fInboxCountWasBumped)
    		fgInboxUsageCount++;
    	PR_ExitMonitor(fgDataSafeMonitor);
    }
    else
    	fInboxCountWasBumped = FALSE;
}

TInboxReferenceCount::~TInboxReferenceCount()
{
	if (fInboxCountWasBumped && fgDataSafeMonitor)
	{
    	PR_EnterMonitor(fgDataSafeMonitor);
    	fgInboxUsageCount--;
    	PR_ExitMonitor(fgDataSafeMonitor);
	}
}
	
int TInboxReferenceCount::GetInboxUsageCount()
{
	int returnCount = 0;
	
	if (fgDataSafeMonitor)
	{
    	PR_EnterMonitor(fgDataSafeMonitor);
    	returnCount = fgInboxUsageCount;
    	PR_ExitMonitor(fgDataSafeMonitor);
	}
	
	return returnCount;
}

//static 
void TInboxReferenceCount::ImapStartup()
{
	if (!fgDataSafeMonitor)
        fgDataSafeMonitor = PR_NewNamedMonitor("inbox-data-safe-monitor");
}

//static
void TInboxReferenceCount::ImapShutDown()
{
	if (fgDataSafeMonitor)
	{
		PR_DestroyMonitor(fgDataSafeMonitor);
		fgDataSafeMonitor = NULL;
	}
}


TIMAPMailboxInfo::TIMAPMailboxInfo(const char *name)
{
	m_mailboxName = XP_STRDUP(name);
	m_childrenListed = FALSE;
}

TIMAPMailboxInfo::~TIMAPMailboxInfo()
{
	FREEIF(m_mailboxName);
}

void IMAP_StartupImap()
{
	TInboxReferenceCount::ImapStartup();
	TNavigatorImapConnection::ImapStartup();
}

void IMAP_ShutdownImap()
{
	TInboxReferenceCount::ImapShutDown();
	TNavigatorImapConnection::ImapShutDown();
}

XP_Bool IMAP_HaveWeBeenAuthenticated()
{
	return (gIMAPpassword || preAuthSucceeded);
}
