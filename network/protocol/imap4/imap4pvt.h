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
/* classes and functions used to implement the IMAP4 netlib module.
*/

/* 45678901234567890123456789012345678901234567890123456789012345678901234567890
*/ 
#ifndef IMAP4PVT_H
#define IMAP4PVT_H

#include "rosetta.h"
#include "mkimap4.h"
#include "prthread.h"
#include "prtypes.h"
#include "prmon.h"
#include "xp_list.h"
#include "imap.h"
#include "imaphier.h"
#include "idarray.h"
#include "ptrarray.h"
#include "mktcp.h"


#define IMAP4_PORT 143  /* the iana port for imap4 */
#define IMAP4_PORT_SSL_DEFAULT 993 /* use a separate port for imap4 over ssl */

/* amount to expand for imap entry flags when we need more */
#define kFlagEntrySize	100

// convenient function for atol on int32's
int32 atoint32(char *ascii);

/*
    Using a separate thread to drive the IMAP4 server connection algorithm
    is being introduced in 4.0.  Since this adding another thread to an
    otherwise single threaded program (navigator), then we need a mechanism
    for the IMAP thread to use existing, non thread safe, code.
    It is a basic rule that all pre-existing code should be executed in the 
    main 'mozilla' thread.
    
    TImapFEEventQueue and TImapFEEvent exist to accomplish this goal.
    
    Whenever the imap thread needs to do something like post an alert or
    operate a thermo, then it will add an TImapFEEvent to a TImapFEEventQueue.
    
    The main (mozilla) thread, in NET_ProcessIMAP4, will periodically check the queue
    and execute any events.

    The mozilla thread will also run events in NET_InterruptIMAP4.  However, some events
	should not be run when interrupted.  This is mainly because of sharing memory
	between threads through these events, but it can also be for reasons of context
	as well.  (For instace, running a certain event might not make sense if we get
	interrupted).  In general, if any event dispatcher creates an event and passes memory
	to it which will be freed in the IMAP thread after that event has run, then it should
	tell it not to execute when interrupted.  This is because interruption tells the thread
	to die, which notifies the Event Completion monitor.  So, we think these events have
	finished, and therefore free the memory;  however, the next thing that NET_InterruptIMAP4
	does is try to run these events, which access freed memory, and then bad things happen...

*/
typedef void (FEEventFunctionPointer)(void*,void*);

class TImapFEEvent {
public:
    TImapFEEvent(FEEventFunctionPointer *function, void *arg1, void*arg2, XP_Bool executeWhenInterrupted);
    virtual ~TImapFEEvent();

    virtual XP_Bool ShouldExecuteWhenInterrupted() { return fExecuteWhenInterrupted; }
    virtual void DoEvent();
	virtual FEEventFunctionPointer *GetEventFunction() { return fEventFunction; }
	virtual void					SetEventFunction(FEEventFunctionPointer *f) { fEventFunction = f; }

private:
    FEEventFunctionPointer *fEventFunction;
    void                   *fArg1, *fArg2;
	XP_Bool					fExecuteWhenInterrupted;
};

// all of the member functions of TImapFEEventQueue are multithread safe
class TImapFEEventQueue {
public:
    static TImapFEEventQueue *CreateFEEventQueue();
    virtual ~TImapFEEventQueue();
    
    virtual void            AdoptEventToEnd(TImapFEEvent *event);
    virtual void            AdoptEventToBeginning(TImapFEEvent *event);
    virtual TImapFEEvent*   OrphanFirstEvent();
    
    virtual int             NumberOfEventsInQueue();
protected:
    TImapFEEventQueue();
    
private:
    XP_List     *fEventList;
    PRMonitor   *fListMonitor;
    int          fNumberOfEvents;
};


/* This class is used by the imap module to get info about a
   url.  This ensures that the knowledge about the URL syntax
   is not spread across hundreds of strcmp calls.
*/

#define IMAP_URL_TOKEN_SEPARATOR ">"

class TIMAPUrl {
public:
    TIMAPUrl(const char *url_string, XP_Bool internal);
    virtual ~TIMAPUrl();
    
    XP_Bool ValidIMAPUrl();

    enum EUrlIMAPstate {
        kAuthenticatedStateURL,
        kSelectedStateURL
    };
    
    EUrlIMAPstate GetUrlIMAPstate();
    
    
    enum EIMAPurlType {
            // kAuthenticatedStateURL urls 
        kTest,
        kSelectFolder,
        kLiteSelectFolder,
        kExpungeFolder,
        kCreateFolder,
        kDeleteFolder,
        kRenameFolder,
        kMoveFolderHierarchy,
        kLsubFolders,
		kGetMailAccountUrl,
		kDiscoverChildrenUrl,
		kDiscoverLevelChildrenUrl,
		kDiscoverAllBoxesUrl,
		kDiscoverAllAndSubscribedBoxesUrl,
		kAppendMsgFromFile,
		kSubscribe,
		kUnsubscribe,
		kRefreshACL,
		kRefreshAllACLs,
		kListFolder,
		kUpgradeToSubscription,
		kFolderStatus,
		kRefreshFolderUrls,
        
            // kSelectedStateURL urls
        kMsgFetch,
        kMsgHeader,
        kSearch,
        kDeleteMsg,
        kDeleteAllMsgs,
        kAddMsgFlags,
        kSubtractMsgFlags,
        kSetMsgFlags,
        kOnlineCopy,
        kOnlineMove,
        kOnlineToOfflineCopy,
        kOnlineToOfflineMove,
        kOfflineToOnlineMove,
        kBiff
    };
    
    EIMAPurlType GetIMAPurlType();
        
    const char *GetUrlHost() { return fHostSubString; }
    
    // these functions have Preconditions
    char *CreateCanonicalSourceFolderPathString();
    char *CreateCanonicalDestinationFolderPathString();
    
    char *CreateServerSourceFolderPathString();
    char *CreateServerDestinationFolderPathString();
    
    char *CreateSearchCriteriaString();
    char *CreateListOfMessageIdsString();
	char *GetIMAPPartToFetch();
    imapMessageFlagsType GetMsgFlags(); // kAddMsgFlags, kSubtractMsgFlags, or kSetMsgFlags only
    
    XP_Bool MessageIdsAreUids();
    XP_Bool MimePartSelectorDetected() { return fMimePartSelectorDetected; }
      
    void SetOnlineSubDirSeparator(char onlineDirSeparator);
    char GetOnlineSubDirSeparator();

    char *AllocateServerPath(const char *canonicalPath);
    char *AllocateCanonicalPath(const char *serverPath);

	int GetChildDiscoveryDepth() { return fDiscoveryDepth; }
	XP_Bool	GetShouldSubscribeToAll();

private:
    // assigning and copying not allowed
    TIMAPUrl(const TIMAPUrl& copy);
    TIMAPUrl& operator=(const TIMAPUrl& copy);
    
    void Parse();
    void ParseSearchCriteriaString();
    void ParseListofMessageIds();
    void ParseUidChoice();
    void ParseMsgFlags();
	void ParseChildDiscoveryDepth();

    void ParseFolderPath(char **resultingCanonicalPath);
    
    char *ReplaceCharsInCopiedString(const char *stringToCopy, char oldChar, char newChar);
    
    
    char            *fUrlString;
    char            *fHostSubString;
    char            *fUrlidSubString;
    char            *fSourceCanonicalFolderPathSubString;
    char            *fDestinationCanonicalFolderPathSubString;
    char            *fSearchCriteriaString;
    char            *fListOfMessageIds;
    char			*fTokenPlaceHolder;
    char		     fOnlineSubDirSeparator;	// not static anymore - one delimiter for each URL we run.
    
    imapMessageFlagsType fFlags;
    
    XP_Bool         fIdsAreUids;
    XP_Bool			fMimePartSelectorDetected;
    
    EUrlIMAPstate   fIMAPstate;
    EIMAPurlType    fUrlType;
    
    XP_Bool         fValidURL;
	XP_Bool			fInternal;
	int				fDiscoveryDepth;
};


class TSearchResultSequence {
public:
    virtual ~TSearchResultSequence();
    static TSearchResultSequence *CreateSearchResultSequence();
    
    virtual void AddSearchResultLine(const char *searchLine);
    virtual void ResetSequence();
    
    friend class TSearchResultIterator;
private:
    TSearchResultSequence();
    XP_List *fListOfLines;
};

class TSearchResultIterator {
public:
    TSearchResultIterator(TSearchResultSequence &sequence);
    virtual ~TSearchResultIterator();
    
    void  ResetIterator();
    int32 GetNextMessageNumber();   // returns 0 at end of list
private:
    TSearchResultSequence &fSequence;
    XP_List *fCurrentLine;
    char    *fPositionInCurrentLine;
};


class TIMAP4BlockingConnection;


class TImapFlagAndUidState {
public:
    TImapFlagAndUidState(int numberOfMessages, XP_Bool bSupportUserFlag = FALSE);
    TImapFlagAndUidState(const TImapFlagAndUidState& state, XP_Bool bSupportUserFlag = FALSE);
    virtual				 ~TImapFlagAndUidState();
    
    int					 GetNumberOfMessages();
    int					 GetNumberOfDeletedMessages();
    
    imap_uid             GetUidOfMessage(int zeroBasedIndex);
    imapMessageFlagsType GetMessageFlags(int zeroBasedIndex);
    imapMessageFlagsType GetMessageFlagsFromUID(imap_uid uid, XP_Bool *foundIt, int32 *ndx);
	XP_Bool				 IsLastMessageUnseen(void);
    void				 ExpungeByIndex(uint32 index);
	// adds to sorted list.  protects against duplicates and going past fNumberOfMessageSlotsAllocated  
    void				 AddUidFlagPair(imap_uid uid, imapMessageFlagsType flags);
    
    imap_uid			 GetHighestNonDeletedUID();
	void				 Reset(uint32 howManyLeft);
private:
    
    int32                   fNumberOfMessagesAdded;
    int32					fNumberOfMessageSlotsAllocated;
	int32					fNumberDeleted;
    IDArray                 fUids;
    imapMessageFlagsType    *fFlags;
	XP_Bool					fSupportUserFlag;
};


class TIMAPGenericParser {

public:
	TIMAPGenericParser();
	virtual ~TIMAPGenericParser();

    // Connected() && !SyntaxError()
	// Add any specific stuff in the derived class
    virtual XP_Bool     LastCommandSuccessful();
    
    XP_Bool     SyntaxError();
    virtual XP_Bool     ContinueParse();
    
    // if we get disconnected, end the current url processing and report to the
    // the user.
    XP_Bool			    Connected();
    virtual void        SetConnected(XP_Bool error);
    
    char        *CreateSyntaxErrorLine();

protected:

	// This is a pure virtual member which must be overridden in the derived class
	// for each different implementation of a TIMAPGenericParser.
	// For instance, one implementation (the TImapServerState) might get the next line
	// from an open socket, whereas another implementation might just get it from a buffer somewhere.
	// This fills in nextLine with the buffer, and returns TRUE if everything is OK.
	// Returns FALSE if there was some error encountered.  In that case, we reset the parser.
	virtual XP_Bool	GetNextLineForParser(char **nextLine) = 0;	

    virtual void	HandleMemoryFailure();
    virtual void    skip_to_CRLF();
    virtual void    skip_to_close_paren();
	virtual char	*CreateString();
	virtual char	*CreateAstring();
	virtual char	*CreateNilString();
    virtual char    *CreateLiteral();
	virtual char	*CreateAtom();
    virtual char    *CreateQuoted(XP_Bool skipToEnd = TRUE);
	virtual char	*CreateParenGroup();
    virtual void        SetSyntaxError(XP_Bool error);
    virtual XP_Bool     at_end_of_line();

    char *GetNextToken();
    void AdvanceToNextLine();
	void AdvanceTokenizerStartingPoint (int32 bytesToAdvance);
    void ResetLexAnalyzer();

protected:
	// use with care
    char                     *fNextToken;
    char                     *fCurrentLine;
	char					 *fLineOfTokens;
    char                     *fStartOfLineOfTokens;
    char                     *fCurrentTokenPlaceHolder;

private:
    XP_Bool                   fAtEndOfLine;
	XP_Bool					  fTokenizerAdvanced;

    char                     *fSyntaxErrorLine;
    XP_Bool                   fDisconnected;
    XP_Bool                   fSyntaxError;

};

class TIMAPNamespace;
class TIMAPNamespaceList;
class TIMAPBodyShell;

class TImapServerState : public TIMAPGenericParser {
public:
    TImapServerState(TIMAP4BlockingConnection &imapConnection);
    virtual ~TImapServerState();

	// Overridden from the base parser class
	virtual XP_Bool     LastCommandSuccessful();
    virtual void		HandleMemoryFailure();

    virtual void		ParseIMAPServerResponse(const char *currentCommand);
	virtual void		InitializeState();
    XP_Bool				CommandFailed();
    
    enum eIMAPstate {
        kNonAuthenticated,
        kAuthenticated,
        kFolderSelected,
        kWaitingForMoreClientInput
    } ;

    virtual eIMAPstate GetIMAPstate();
	virtual void SetIMAPstate(eIMAPstate state) { fIMAPstate = state; };
    
    const char *GetSelectedMailboxName();   // can be NULL

	// if we get a PREAUTH greeting from the server, initialize the parser to begin in
	// the kAuthenticated state
    void		PreauthSetAuthenticatedState();

    // these functions represent the state of the currently selected
    // folder
    XP_Bool     CurrentFolderReadOnly();
    int32         NumberOfMessages();
    int32         NumberOfRecentMessages();
    int32         NumberOfUnseenMessages();
    int32       FolderUID();
    uint32      CurrentResponseUID();
    uint32      HighestRecordedUID();
    int32       SizeOfMostRecentMessage();
	void		SetTotalDownloadSize(int32 newSize) { fTotalDownloadSize = newSize; }
    
    TSearchResultIterator *CreateSearchResultIterator();
    void ResetSearchResultSequence() {fSearchResults->ResetSequence();}
    
    // create a struct mailbox_spec from our info, used in
    // libmsg c interface
    struct mailbox_spec *CreateCurrentMailboxSpec(const char *mailboxName = NULL);
    
    // zero stops a list recording of flags and causes the flags for
    // each individual message to be sent back to libmsg 
    void ResetFlagInfo(int numberOfInterestingMessages);
    
    // set this to false if you don't want to alert the user to server 
    // error messages
    void SetReportingErrors(XP_Bool reportThem) { fReportingErrors=reportThem;}
	XP_Bool GetReportingErrors() { return fReportingErrors; }

	uint32 GetCapabilityFlag() { return fCapabilityFlag; }
	void   SetCapabilityFlag(uint32 capability) {fCapabilityFlag = capability;}
	XP_Bool ServerHasIMAP4Rev1Capability() { return (fCapabilityFlag & kIMAP4rev1Capability); }
	XP_Bool ServerHasACLCapability() { return (fCapabilityFlag & kACLCapability); }
	XP_Bool ServerHasNamespaceCapability() { return (fCapabilityFlag & kNamespaceCapability); }
	XP_Bool ServerIsNetscape3xServer() { return fServerIsNetscape3xServer; }
	XP_Bool ServerHasServerInfo() {return (fCapabilityFlag & kXServerInfoCapability);}
	void ResetCapabilityFlag() ;

	const char *GetMailAccountUrl() { return fMailAccountUrl; }
	const char *GetXSenderInfo() { return fXSenderInfo; }
	void FreeXSenderInfo() { FREEIF(fXSenderInfo); }
	const char *GetManageListsUrl() { return fManageListsUrl; }
	const char *GetManageFiltersUrl() {return fManageFiltersUrl;}
	const char *GetManageFolderUrl() {return fFolderAdminUrl;}


	TImapFlagAndUidState     *GetCurrentFlagState() { return fFlagState; }

    // Call this when adding a pipelined command to the session
    void IncrementNumberOfTaggedResponsesExpected(const char *newExpectedTag);
    
	// Interrupt a Fetch, without really Interrupting (through netlib)
	XP_Bool GetLastFetchChunkReceived(); 
	void ClearLastFetchChunkReceived(); 
	virtual XP_Bool	SupportsUserFlags() { return fSupportsUserDefinedFlags; };
	void SetFlagState(TImapFlagAndUidState *state);

	XP_Bool GetFillingInShell();
	void	UseCachedShell(TIMAPBodyShell *cachedShell);

protected:
    virtual void    flags();
    virtual void    response_data();
    virtual void    resp_text();
    virtual void    resp_cond_state();
    virtual void    text_mime2();
    virtual void    text();
    virtual void    resp_text_code();
    virtual void    response_done();
    virtual void    response_tagged();
    virtual void    response_fatal();
    virtual void    resp_cond_bye();
    virtual void    mailbox_data();
    virtual void    numeric_mailbox_data();
    virtual void    capability_data();
	virtual void	xserverinfo_data();
	virtual void	xmailboxinfo_data();
	virtual void	namespace_data();
	virtual void	myrights_data();
	virtual void	acl_data();
	virtual void	bodystructure_data();
	virtual void	mime_data();
	virtual void	mime_part_data();
	virtual void	mime_header_data();
    virtual void    msg_fetch();
    virtual void    msg_obsolete();
	virtual void	msg_fetch_headers(const char *partNum);
    virtual void    msg_fetch_content(XP_Bool chunk, int32 origin, const char *content_type);
    virtual XP_Bool	msg_fetch_quoted(XP_Bool chunk, int32 origin);
    virtual XP_Bool	msg_fetch_literal(XP_Bool chunk, int32 origin);
    virtual void    mailbox_list(XP_Bool discoveredFromLsub);
    virtual void    mailbox(mailbox_spec *boxSpec);
    
    virtual XP_Bool     IsNumericString(const char *string);
    
    virtual void        ProcessOkCommand(const char *commandToken);
    virtual void        ProcessBadCommand(const char *commandToken);
    virtual void        PreProcessCommandToken(const char *commandToken,
                                               const char *currentCommand);
    virtual void		PostProcessEndOfLine();

	// Overridden from the TIMAPGenericParser, to retrieve the next line
	// from the open socket.
	virtual XP_Bool		GetNextLineForParser(char **nextLine);
	virtual void    end_of_line();

private:
    XP_Bool                   fProcessingTaggedResponse;
    XP_Bool                   fCurrentCommandFailed;
    XP_Bool					  fReportingErrors;
    
    
    XP_Bool                   fCurrentFolderReadOnly;
    
    XP_Bool                   fCurrentLineContainedFlagInfo;
    imapMessageFlagsType	  fSavedFlagInfo;


	XP_Bool					  fSupportsUserDefinedFlags;

    int32                     fFolderUIDValidity;
    int32                     fNumberOfUnseenMessages;
    int32                     fNumberOfExistingMessages;
    int32                     fNumberOfRecentMessages;
    uint32                    fCurrentResponseUID;
    uint32                    fHighestRecordedUID;
    int32                     fSizeOfMostRecentMessage;
	int32					  fTotalDownloadSize;
    
    int						  fNumberOfTaggedResponsesExpected;
    
    char                     *fCurrentCommandTag;
    
    char					 *fZeroLengthMessageUidString;

    char                     *fSelectedMailboxName;
    
    TSearchResultSequence    *fSearchResults;
    
    TImapFlagAndUidState     *fFlagState;		// NOT owned by this object, it's a copy, do not destroy
    
    eIMAPstate               fIMAPstate;

	uint32					  fCapabilityFlag; 
	char					 *fMailAccountUrl;
	char					 *fNetscapeServerVersionString;
	char					 *fXSenderInfo; /* changed per message download */
	char					 *fLastAlert;	/* used to avoid displaying the same alert over and over */
	char					 *fManageListsUrl;
	char					 *fManageFiltersUrl;
	char					 *fFolderAdminUrl;
	
	// used for index->uid mapping
	XP_Bool fCurrentCommandIsSingleMessageFetch;
	int32	fUidOfSingleMessageFetch;
	int32	fFetchResponseIndex;

	// used for aborting a fetch stream when we're pseudo-Interrupted
	XP_Bool fDownloadingHeaders;
	int32 numberOfCharsInThisChunk;
	int32 charsReadSoFar;
	XP_Bool fLastChunk;

	// Is the server a Netscape 3.x Messaging Server?
	XP_Bool	fServerIsNetscape3xServer;

	// points to the current body shell, if any
	TIMAPBodyShell			*m_shell;

	// The connection object
    TIMAP4BlockingConnection &fServerConnection;
};

/*
    This virtual base class contains the interface and the code
    that drives the algorithms for performing imap4 server
    transactions.  
    
    This base class contains no knowledge of 'how things were' before
    4.0.  The knowledge of our URL language, how we do socket io,
    and how we perform events in other threads is found in a concrete
    subclass.
*/

#define kOutputBufferSize 2048

class TNavigatorImapConnection;
    

class TIMAP4BlockingConnection {
public:
    enum eFetchFields {
        kEveryThingRFC822,
        kEveryThingRFC822Peek,
        kHeadersRFC822andUid,
        kUid,
        kFlags,
		kRFC822Size,
		kRFC822HeadersOnly,
		kMIMEPart,
        kMIMEHeader
	};
    

    virtual ~TIMAP4BlockingConnection();
    
	// This should be used only for setting the connection status, i.e. MK_CONNECTED, MK_WAITING_FOR_CONNECTION, etc.
	// This should not be used for getting/setting the status of the currently running ActiveEntry.
	// (Use Get/SetCurrentEntryStatus()) for passing values back to libnet about the currently running ActiveEntry.
    int					GetConnectionStatus();
    void         		SetConnectionStatus(int status);

    // safe downcast, returns non-null if cast worked
    virtual TNavigatorImapConnection *GetNavigatorConnection();
    
	// Have we detected new mail ?
	virtual XP_Bool NewMailDetected(void) { return FALSE; };
	virtual void ParseIMAPandCheckForNewMail(char *buff = NULL) = 0;
    
    // These 2 functions are thread safe.  If BlockedForIO then it will
    // become unblocked with a call to NotifyIOCompletionMonitor
    void              NotifyIOCompletionMonitor();
    XP_Bool           BlockedForIO();
        
    virtual void ProcessBIFFRequest();
    
    // non-authenticated state commands
    virtual void InsecureLogin(const char *userName, const char *password);
        // Authenticate will happen later

	virtual void AuthLogin(const char *userName, const char *password);
    
    // authenticated state commands 
    virtual void SelectMailbox(const char *mailboxName);
	// will do examine as needed
    virtual void CreateMailbox(const char *mailboxName);
    virtual void DeleteMailbox(const char *mailboxName);
    virtual void RenameMailbox(const char *existingName, const char *newName);
    
    virtual void AppendMessage(const char *mailboxName, 
                               const char *messageSizeString,
                               imapMessageFlagsType flags);

    virtual void Subscribe(const char *mailboxName);
    virtual void Unsubscribe(const char *mailboxName);
    
    virtual void List(const char *mailboxPattern,XP_Bool pipeLined = FALSE, XP_Bool dontTimeStamp = FALSE);
    virtual void Lsub(const char *mailboxPattern, XP_Bool pipelined = FALSE);
    
    // selected state commands
    virtual void FetchMessage(const char *messageIds, 
                              eFetchFields whatToFetch,
                              XP_Bool idAreUid,
							  uint32 startByte = 0, uint32 endByte = 0,
							  char *part = 0) = 0;
	virtual void Bodystructure(const char *messageId, XP_Bool idIsUid) = 0;
    
    virtual void Close();
    virtual void Expunge();
    virtual void Check();
        
    virtual void Search(const char *searchCriteria, XP_Bool useUID,
                        XP_Bool notifyHit = TRUE);
    
    virtual void Store(const char *messageList,
                       const char *messageData, 
                       XP_Bool idsAreUid);
    
    virtual void Copy( const char *messageList,
                       const char *destinationMailbox, 
                       XP_Bool idsAreUid);
        
        
	virtual void Netscape();                      
	virtual void XServerInfo();
	virtual void XMailboxInfo(const char *mailboxName);
    
    
    // any state commands
    virtual void Logout();
    virtual void Noop();
	virtual void Capability();

    virtual void Namespace();
	virtual void MailboxData();
	virtual void GetMyRightsForFolder(const char *mailboxName);
	virtual void GetACLForFolder(const char *mailboxName);

    virtual void StatusForFolder(const char *mailboxName);

    virtual void AlertUserEvent_UsingId(uint32 msgId) = 0;
    virtual void AlertUserEvent(char *message) = 0;
	virtual void AlertUserEventFromServer (char *serverSaid) = 0;
	virtual void ProgressUpdateEvent(char *message) = 0;
    virtual void ProgressEventFunction_UsingId(uint32 msgId) = 0;
	virtual void ProgressEventFunction_UsingIdWithString(uint32 msgId, const char * extraInfo) = 0;
    virtual void PercentProgressUpdateEvent(char *message, int percent) = 0;
	virtual void PastPasswordCheckEvent() = 0;
	
	// does a blocking read of the socket, returns a newly allocated
    // line or nil and a status
    virtual char *CreateNewLineFromSocket() = 0;
    // does a write to the socket
    virtual int WriteLineToSocket(char *line) = 0;

    // Use these functions to stream a message fetch to the current context
    // Used initially by TImapServerState
    virtual void BeginMessageDownLoad(uint32 total_size,
                                      const char *content_type)=0;   // some downloads are header only
    virtual void HandleMessageDownLoadLine(const char *line, XP_Bool chunkEnd)=0;
    virtual void NormalMessageEndDownload()=0;
    virtual void AbortMessageDownLoad()=0;
	virtual void AddXMozillaStatusLine(uint16 flags)=0;	// for XSender auth info
    
    // used to notify the fe to upload the message to this 
    // connection's socket
    virtual void UploadMessage() = 0;
    
    // notify the fe about a message's imap message flags
    virtual void NotifyMessageFlags( imapMessageFlagsType flags, MessageKey key) = 0;
    
    // notify the fe about a search hit
    virtual void NotifySearchHit(const char *hitLine) = 0;

    // a call back from TImapServerState to handle mailbox discovery
    virtual void DiscoverMailboxSpec(mailbox_spec *adoptedBoxSpec) = 0;
    
    // notify the fe about the state of the selected mailbox
    virtual void UpdatedMailboxSpec(mailbox_spec *adoptedBoxSpec) = 0;
	virtual void UpdateMailboxStatus(mailbox_spec *adoptedBoxSpec) = 0;
    
    // notify the fe that the current online copy is done
    virtual void OnlineCopyCompleted(ImapOnlineCopyState copyState) = 0;

    // notify the fe that a folder was deleted
    virtual void FolderDeleted(const char *mailboxName) = 0;

	// notify the fe that a folder creation failed
	virtual void FolderNotCreated(const char *mailboxName) = 0;
    
    // notify the fe that a folder was deleted
    virtual void FolderRenamed(const char *oldName,
                               const char *newName) = 0;

    // notify the fe that we're done finding folders
    virtual void MailboxDiscoveryFinished() = 0;

    // return the server response parser
    virtual TImapServerState &GetServerStateParser();
    
    
    // This function will report the low mem conditions to the user,
    // empty the fe event queue and set the connection status to negative 
    // so netlib will exit ProcessImap4.
    virtual void HandleMemoryFailure() = 0;
    

    // if you call TellThreadToDie(FALSE) then you are responsible for 
    // calling SetIsSafeToDie when you are ready for the imap thread to
    // delete the connection.
    virtual void              TellThreadToDie(XP_Bool isSafeToDie = TRUE)=0;
    virtual void			  SetIsSafeToDie()=0;	
    virtual XP_Bool			  DeathSignalReceived()=0;

    virtual void		ShowProgress()=0;
	virtual const char  *GetHostName()=0;

    void				SetDeleteIsMoveToTrash(XP_Bool deleteIsMoveToTrash) {fDeleteModelIsMoveToTrash = deleteIsMoveToTrash;}

    // used the convert to/from UTF7IMAP
	virtual char* CreateUtf7ConvertedString(const char *sourceString, XP_Bool toUtf7Imap)=0;
	
	static int64 GetTimeStampOfNonPipelinedList() { return fgTimeStampOfNonPipelinedList; }
protected:
    void         WaitForIOCompletion();
    void         WaitForFEEventCompletion();
	void		 WaitForTunnelCompletion();
    
    // manage the IMAP server command tags
    void IncrementCommandTagNumber();
    
    char *CreateEscapedMailboxName(const char *rawName);

    
    // establish connection, this will not alert the user if there is a problem
    // affects GetConnectionStatus()
    virtual void EstablishServerConnection() = 0;
    
    
    // called from concrete subclass constructor
    TIMAP4BlockingConnection();
    virtual XP_Bool     ConstructionSuccess();  // called by subclass factory
    
    char *GetOutputBuffer();
    char *GetServerCommandTag();
    PRMonitor *GetDataMemberMonitor();

	// returns TRUE if the messageIdString indicates that this is an operation
	// on more than one message.
	XP_Bool HandlingMultipleMessages(char *messageIdString);
	
	void				TimeStampListNow();

	XP_Bool				SupportsUserDefinedFlags() { return fServerState.SupportsUserFlags(); };

protected:
	static int64		fgTimeStampOfNonPipelinedList;
    TImapServerState    fServerState;
    XP_Bool             fBlockedForIO;
    XP_Bool				fCloseNeededBeforeSelect;
    XP_Bool				fDeleteModelIsMoveToTrash;
    int                 fConnectionStatus;
    PRMonitor           *fDataMemberMonitor;
    PRMonitor           *fIOMonitor;
    char                fCurrentServerCommandTag[10];   // enough for a billion
    int                 fCurrentServerCommandTagNumber;
    char                fOutputBuffer[kOutputBufferSize];
	XP_Bool				fFromHeaderSeen;
    XP_Bool             fNotifyHit;
};


#define kDownLoadCacheSize 1536
struct msg_line_info;

class TLineDownloadCache {
public:
    TLineDownloadCache();
    virtual ~TLineDownloadCache();

    uint32  CurrentUID();
    uint32  SpaceAvailable();
    XP_Bool CacheEmpty();
    
    msg_line_info *GetCurrentLineInfo();
    
    void ResetCache();
    void CacheLine(const char *line, uint32 uid);
private:
    char   fLineCache[kDownLoadCacheSize];
    uint32 fBytesUsed;
    
    msg_line_info *fLineInfo;
    
};

class TIMAPSocketInfo;    
class TIMAPBodyShellCache;

// this is a linked list of host info's    
class TIMAPHostInfo 
{
public:

	// Host List
	static XP_Bool	AddHostToList(const char *hostName);
	static void ResetAll();

	// Capabilities
	static uint32	GetCapabilityForHost(const char *hostName);
	static XP_Bool	SetCapabilityForHost(const char *hostName, uint32 capability);
	static XP_Bool	GetHostHasAdminURL(const char *hostName);
	static XP_Bool	SetHostHasAdminURL(const char *hostName, XP_Bool hasAdminUrl);
	// Subscription
	static XP_Bool	GetHostIsUsingSubscription(const char *hostName);
	static XP_Bool	SetHostIsUsingSubscription(const char *hostName, XP_Bool usingSubscription);

	// Passwords
	static char		*GetPasswordForHost(const char *hostName);
	static XP_Bool	SetPasswordForHost(const char *hostName, const char *password);
	static XP_Bool  GetPasswordVerifiedOnline(const char *hostName);
	static XP_Bool	SetPasswordVerifiedOnline(const char *hostName);

	// Folders
	static XP_Bool	SetHaveWeEverDiscoveredFoldersForHost(const char *hostName, XP_Bool discovered);
	static XP_Bool	GetHaveWeEverDiscoveredFoldersForHost(const char *hostName);

	// Trash Folder
	static XP_Bool	SetOnlineTrashFolderExistsForHost(const char *hostName, XP_Bool exists);
	static XP_Bool	GetOnlineTrashFolderExistsForHost(const char *hostName);
	
	// INBOX
	static char			*GetOnlineInboxPathForHost(const char *hostName);
	static XP_Bool		GetShouldAlwaysListInboxForHost(const char *hostName);
	static XP_Bool		SetShouldAlwaysListInboxForHost(const char *hostName, XP_Bool shouldList);

	// Namespaces
	static TIMAPNamespace		*GetNamespaceForMailboxForHost(const char *hostName, const char *mailbox_name);
	static XP_Bool				AddNewNamespaceForHost(const char *hostName, TIMAPNamespace *ns);
	static XP_Bool				ClearServerAdvertisedNamespacesForHost(const char *hostName);
	static XP_Bool				ClearPrefsNamespacesForHost(const char *hostName);
	static TIMAPNamespace		*GetDefaultNamespaceOfTypeForHost(const char *hostName, EIMAPNamespaceType type);
	static XP_Bool				SetNamespacesOverridableForHost(const char *hostName, XP_Bool overridable);
	static XP_Bool				GetNamespacesOverridableForHost(const char *hostName);
	static int					GetNumberOfNamespacesForHost(const char *hostName);
	static TIMAPNamespace		*GetNamespaceNumberForHost(const char *hostName, int n);
	static XP_Bool				CommitNamespacesForHost(const char *hostName, MSG_Master *master);


	// Hierarchy Delimiters
	static XP_Bool	AddHierarchyDelimiter(const char *hostName, char delimiter);
	static char		*GetHierarchyDelimiterStringForHost(const char *hostName);
	static XP_Bool	SetNamespaceHierarchyDelimiterFromMailboxForHost(const char *hostName, const char *boxName, char delimiter);

	// Message Body Shells
	static XP_Bool	AddShellToCacheForHost(const char *hostName, TIMAPBodyShell *shell);
	static TIMAPBodyShell	*FindShellInCacheForHost(const char *hostName, const char *mailboxName, const char *UID);

	static PRMonitor	*gCachedHostInfoMonitor;
	static TIMAPHostInfo *fHostInfoList;
protected:
	TIMAPHostInfo(const char *hostName);
	static TIMAPHostInfo *FindHost(const char *hostName);
	~TIMAPHostInfo();
	char			*fHostName;
	char			*fCachedPassword;
	TIMAPHostInfo	*fNextHost;
	uint32			fCapabilityFlags;
	char			*fHierarchyDelimiters;	// string of top-level hierarchy delimiters
	XP_Bool			fHaveWeEverDiscoveredFolders;
	char			*fCanonicalOnlineSubDir;
	TIMAPNamespaceList			*fNamespaceList;
	XP_Bool			fNamespacesOverridable;
	XP_Bool			fUsingSubscription;
	XP_Bool			fOnlineTrashFolderExists;
	XP_Bool			fShouldAlwaysListInbox;
	XP_Bool			fHaveAdminURL;
	XP_Bool			fPasswordVerifiedOnline;
	TIMAPBodyShellCache	*fShellCache;
};

class MSG_FolderInfo;
class TIMAPMessagePartIDArray;

class TNavigatorImapConnection : public TIMAP4BlockingConnection {
public:
    static TNavigatorImapConnection* 
            Create(const char *hostName);
    
    virtual ~TNavigatorImapConnection();

    static SetupConnection(ActiveEntry * ce, MSG_Master *master, XP_Bool loadingURL);

	// Performs a PR_LOG on logData, formatting it to a standard IMAP i/o log format.
	// This checks the state of the parser and tells us which connection we're using, the selected folder, etc.
	// logSubName is a name like "BODYSHELL," "NETWORK," "URL," etc... basically, a way to subcategorize what
	// type of log entry this is.
	// extraInfo is any extra notes that should be added to this log entry.  For instance, a "NETWORK" entry
	// might also pass in "READ" or "WRITE" as extraInfo.  extraInfo can be NULL, in which case we don't add anything.
	// logData is the data to be logged.

	void Log(const char *logSubName, const char *extraInfo, const char *logData);


    // checks for new mail and gets headers if nescesary
	virtual XP_Bool CheckNewMail(void);
    virtual void SelectMailbox(const char *mailboxName);
	virtual void Configure(XP_Bool GetHeaders, int32 TooFastTime, int32 IdealTime,
							int32 ChunkAddSize, int32 ChunkSize, int32 ChunkThreshold,
							XP_Bool FetchByChunks, int32 MaxChunkSize);

    // dumb but usefull safe downcast, returns non-null if cast worked
    virtual TNavigatorImapConnection *GetNavigatorConnection();

	// gets/sets the status of the currently running ActiveEntry.
	// This should not be used for setting the connection status, i.e. MK_CONNECTED, MK_WAITING_FOR_CONNECTION, etc.
    int					GetCurrentEntryStatus();
    void         		SetCurrentEntryStatus(int status);

	// call by imap thread to endlessly process active entries
    virtual void StartProcessingActiveEntries();
    
    // thread safe:  Called by main thread to submit ActiveEntry
    // for processing
    // Will block if !BlockedWaitingForActiveEntry
    virtual void    SubmitActiveEntry(ActiveEntry * ce, XP_Bool newConnection);
    
    // returns true if another thread is blocked waiting
    virtual XP_Bool BlockedWaitingForActiveEntry();

    /* I don't know about this!
    // thread safe call to add another ActiveEntry to the queue
    virtual void AddActiveEntryJob(ActiveEntry *entry);
    
    // thread safe call that will 
    //      1. Stop processing the current entry if it shares
    //         the same context with the passed entry
    //
    //      2. Remove entries from the queue if they share the
    //         same context with the passed entry
    virtual void AbortActiveEntry(ActiveEntry *entry);
    */

    
	// Tunnels
	virtual int32 OpenTunnel (int32 maxNumberOfBytesToRead);
	XP_Bool GetIOTunnellingEnabled();
	int32	GetTunnellingThreshold();

    
    TImapFEEventQueue &GetFEEventQueue();
    void              SetBlockingThread(PRThread *blockingThread);
    
    // if you call TellThreadToDie(FALSE) then you are responsible for 
    // calling SetIsSafeToDie when you are ready for the imap thread to
    // delete the connection.
    virtual void              TellThreadToDie(XP_Bool isSafeToDie = TRUE);
    virtual void			  SetIsSafeToDie();	

    virtual XP_Bool			  DeathSignalReceived();
    
    // this function is thread safe
    // this function is used by the fe functions to signal their completion
    void              NotifyEventCompletionMonitor();

    // This function is threadsafe
	// It is used by certain FE events to signal the completion of an IO tunnel.
	void			  NotifyTunnelCompletionMonitor();

    // this function is thread safe
    // this function is used by the fe signal complete message upload
    void              NotifyMessageUploadMonitor();
    
    // this function is thread safe
    // notifies that the fe thread is done processing fe events
    void              SignalEventQueueEmpty();
    
    // not thread safe.  intended to be called from the fe event functions
    ActiveEntry      *GetActiveEntry();
    TCP_ConData      **GetTCPConData();
    
    
    // these functions might seem silly but I don't want the IMAP thread 
    // to ever muck with the ActiveEntry structure fields, so the socket 
    // will be set when we begin the connection.  These setters and getters 
    // are thread safe
    void            SetIOSocket(PRFileDesc *socket);
    PRFileDesc *    GetIOSocket();
    
    void            SetOutputStream(NET_StreamClass *outputStream);
    NET_StreamClass *GetOutputStream();
        
	virtual XP_Bool			UseSSL() { return FALSE; };
    
	virtual void FetchMessage(const char *messageIds, 
                              eFetchFields whatToFetch,
                              XP_Bool idAreUid,
							  uint32 startByte = 0, uint32 endByte = 0,
							  char *part = 0);
	virtual void FetchTryChunking(const char *messageIds,
									eFetchFields whatToFetch,
									XP_Bool idAreUid,
									char *part,
									uint32 downloadSize);
	virtual void PipelinedFetchMessageParts(const char *uid,
											TIMAPMessagePartIDArray *parts);
	virtual void Bodystructure(const char *messageId, XP_Bool idIsUid);
                              
    virtual void Logout();

    virtual void AlertUserEvent_UsingId(uint32 msgId);
    virtual void AlertUserEvent(char *message);
    virtual void AlertUserEventFromServer(char *serverSaid);
    virtual void ProgressUpdateEvent(char *message);
    virtual void ProgressEventFunction_UsingId(uint32 msgId);
	virtual void ProgressEventFunction_UsingIdWithString(uint32 msgId, const char* extraInfo);
    virtual void PercentProgressUpdateEvent(char *message, int percent);
    virtual void PastPasswordCheckEvent();

    void         WaitForFEEventCompletion();
    void         WaitForTunnelCompletion();
    
    // does a blocking read of the socket, returns a newly allocated
    // line or nil and a status
    virtual char *CreateNewLineFromSocket();
    
    // does a write to the socket
    virtual int WriteLineToSocket(char *line);
    
    // establish connection, this will not alert the user if there is a 
    // problem. affects GetConnectionStatus()
    virtual void EstablishServerConnection();
    
    // Use these functions to stream a message fetch to the current context
    // Used initially by TImapServerState
    virtual void BeginMessageDownLoad(uint32 total_size, // for user, headers and body
                                      const char *content_type);     // some downloads are header only
    virtual void HandleMessageDownLoadLine(const char *line, XP_Bool chunkEnd);
    virtual void NormalMessageEndDownload();
    virtual void AbortMessageDownLoad();
    virtual void PostLineDownLoadEvent(msg_line_info *downloadLineDontDelete);
	virtual void AddXMozillaStatusLine(uint16 flags);	// for XSender auth info
    
    // used to notify the fe to upload the message to this 
    // connection's socket
    virtual void UploadMessage();
    
    virtual void UploadMessageFromFile(const char *srcFilePath,
	                                   const char *mailboxName,
	                                   imapMessageFlagsType flags);

    // notify the fe about a message's imap message flags
    virtual void NotifyMessageFlags( imapMessageFlagsType flags, MessageKey key);

    // notify the fe about a search hit
    virtual void NotifySearchHit(const char *hitLine);

    // a call back from TImapServerState to handle mailbox discovery
    virtual void DiscoverMailboxSpec(mailbox_spec *adoptedBoxSpec);
    
    // notify the fe about the state of the selected mailbox
    virtual void UpdatedMailboxSpec(mailbox_spec *adoptedBoxSpec);
	virtual void UpdateMailboxStatus(mailbox_spec *adoptedBoxSpec);
    
    // notify the fe that the current online copy is done
    virtual void OnlineCopyCompleted(ImapOnlineCopyState copyState);
    
    // notify the fe that a folder was deleted
    virtual void FolderDeleted(const char *mailboxName);

    // notify the fe that a folder was renamed
    virtual void FolderRenamed(const char *oldName,
                               const char *newName);

	// notify the fe that a folder creation failed
	virtual void FolderNotCreated(const char *mailboxName);
    
	// handle any post-mailbox-discovery tasks (Trash folder, etc.)
    virtual void MailboxDiscoveryFinished();

	// notify the fe that we're done finding folders
	virtual void SetFolderDiscoveryFinished();

    // This function will report the low mem conditions to the user,
    // empty the fe event queue and set the connection status to negative 
    // so netlib will exit ProcessImap4.
    // threadsafe
    virtual void HandleMemoryFailure();

    // These three functions are used to graph the progress of a 
    // message download
    int32 GetMessageSizeForProgress();
    int32 GetBytesMovedSoFarForProgress();
    void  SetBytesMovedSoFarForProgress(int32 bytesMoved);
    
    // Returns true if a msg move/copy is happening
    XP_Bool CurrentConnectionIsMove();
    
    // receive the list of msg ids for fetching msgs
    void NotifyKeyList(uint32 *bodyKeys, uint32 bodyCount);  // storage adopted
    
    // receive the size of the next message append
    void NotifyAppendSize(uint32 msgSize, imapMessageFlagsType flags);
    
    virtual XP_Bool NewMailDetected(void);
	virtual void ParseIMAPandCheckForNewMail(char *buff = NULL);

	// returns a newly allocated string
    char *GetCurrentConnectionURL();
    
    // These calls are not thread safe.  Called from the mozilla
    // thread to keep track of whether we have set call netlib all the time
    inline void SetCallingNetlibAllTheTime(XP_Bool callingNetlib) 
    	{ fCallingNetlibAllTheTime = callingNetlib; };
    	
    inline XP_Bool GetCallingNetlibAllTheTime() { return fCallingNetlibAllTheTime; };
    
    static void ResetCachedConnectionInfo();
    
	// sets the folder info for the currently running URL
	void SetFolderInfo(MSG_FolderInfo *folder, XP_Bool folderNeedsSubscribing,
		XP_Bool folderNeedsACLRefreshed);

    virtual void		ShowProgress();
    
    virtual void		SetMailboxDiscoveryStatus(EMailboxDiscoverStatus status);
    virtual EMailboxDiscoverStatus GetMailboxDiscoveryStatus();
	//static XP_Bool HaveWeEverDiscoveredNewMailboxes() { return fHaveWeEverFoundImapMailboxes; }

	void BodyIdMonitor(XP_Bool enter);

	void SetNeedsGreeting(XP_Bool need) { fNeedGreeting = need; }
	void SetNeedsNoop(XP_Bool need) {fNeedNoop = need;}
	static void ImapStartup();
	static void ImapShutDown();

	TIMAPUrl            *GetCurrentUrl() {return fCurrentUrl;}

    // used the convert to/from UTF7IMAP
	virtual char* CreateUtf7ConvertedString(const char *sourceString, XP_Bool toUtf7Imap);

	uint32 GetMessageSize(const char *messageId, XP_Bool idsAreUids);

	virtual const char *GetHostName() {return fHostName;}
	void PseudoInterrupt(XP_Bool interrupt);
	void SetSubscribeParameters(XP_Bool autoSubscribe,
								XP_Bool autoUnsubscribe, XP_Bool autoSubscribeOnOpen, XP_Bool autoUnsubscribeFromNoSelect,
								XP_Bool shouldUpgrade, XP_Bool upgradeLeaveAlone, XP_Bool upgradeSubscribeAll);
	XP_Bool GetSubscribingNow();

	// includes subscription management
    XP_Bool CreateMailboxRespectingSubscriptions(const char *mailboxName);
    XP_Bool DeleteMailboxRespectingSubscriptions(const char *mailboxName);
    XP_Bool RenameMailboxRespectingSubscriptions(const char *existingName, const char *newName, XP_Bool reallyRename);

	// notifies libmsg that we have a new personal/default namespace that we're using
	void CommitNamespacesForHostEvent();
	// notifies libmsg that we have new capability data for the current host
	void CommitCapabilityForHostEvent();

	// Adds a set of rights for a given user on a given mailbox on the current host.
	// if userName is NULL, it means "me," or MYRIGHTS.
	// rights is a single string of rights, as specified by RFC2086, the IMAP ACL extension.
	void AddFolderRightsForUser(const char *mailboxName, const char *userName, const char *rights);
	// Clears all rights for a given folder, for all users.
	void ClearAllFolderRights(const char *mailboxName);
	// Refreshes the icon / flags for the given folder
	void RefreshFolderACLView(const char *mailboxName);
	XP_Bool FolderNeedsACLInitialized(const char *folderName);
	// Returns true if we know that the given mailbox on the current host is \NoSelect
	XP_Bool	MailboxIsNoSelectMailbox(const char *mailboxName);
    virtual void StatusForFolder(const char *mailboxName);
	XP_Bool InitializeFolderUrl(const char *folderName);

	// We really need to break out the thread synchronizer from the
	// connection class...
	XP_Bool	GetShowAttachmentsInline();

	XP_Bool GetPseudoInterrupted();

	void	ResetProgressInfo();
	void	SetActive(XP_Bool active);
	XP_Bool	GetActive();

	// Sets whether or not the content referenced by the current ActiveEntry has been modified.
	// Used for MIME parts on demand.
	void	SetContentModified(XP_Bool modified);
	XP_Bool	GetShouldFetchAllParts();

protected:
    TNavigatorImapConnection(const char *hostName);

    // Given the canonical pattern prefix, list the children of the pattern
	void CanonicalChildList(const char *canonicalPrefix, XP_Bool pipeLined);
	void NthLevelChildList(const char *canonicalPrefix, int depth);
	void ChildDiscoverySucceeded();
    
    // called within the IMAP thread
	virtual XP_Bool TryToLogon();
    virtual void ProcessCurrentURL();

	// called immediately after we have been authenticated, to do
	// an assorted variety of housekeeping (NAMESPACE, etc...)
	virtual void ProcessAfterAuthenticated();
    
    virtual void WaitForNextActiveEntry();
    virtual void WaitForEventQueueEmptySignal();
    
    virtual void WaitForPotentialListOfMsgsToFetch(uint32 **msgs, uint32 &count);
    virtual void WaitForNextAppendMessageSize();


    virtual uint32 GetAppendSize();
	virtual imapMessageFlagsType GetAppendFlags();
    
    virtual void WaitForMessageUploadToComplete();

	XP_Bool DeleteSubFolders(const char *selectedMailbox);
	XP_Bool RenameHierarchyByHand(const char *oldParentMailboxName, const char *newParentMailboxName);

    virtual XP_Bool     ConstructionSuccess();  // called by subclass factory

    virtual void ProcessStoreFlags(const char *messageIds,
                                    XP_Bool idsAreUids,
                                    imapMessageFlagsType flags,
                                    XP_Bool addFlags);


    virtual void ProcessAuthenticatedStateURL();
    virtual void ProcessSelectedStateURL();
    virtual void HandleCurrentUrlError();
    virtual void ProcessMailboxUpdate(XP_Bool handlePossibleUndo, XP_Bool fromBiffUpdate = FALSE);
    
    virtual void HeaderFetchCompleted(); 
    
    virtual void FolderHeaderDump(uint32 *hdrUids, uint32 hdrCount);
    virtual void FolderMsgDump(uint32 *bodyUids, uint32 bodyCount, TIMAP4BlockingConnection::eFetchFields fields);
    virtual void FolderMsgDumpRecurse(uint32 *bodyUids, uint32 bodyCount, TIMAP4BlockingConnection::eFetchFields fields);
    
    virtual void PeriodicBiff();
    virtual void SendSetBiffIndicatorEvent(MSG_BIFF_STATE newState);
    
	virtual void AutoSubscribeToMailboxIfNecessary(const char *mailboxName);
    virtual void FindMailboxesIfNecessary();
    virtual void DiscoverMailboxList();
    virtual void DiscoverAllAndSubscribedBoxes();

	int GetNumberOfListedMailboxesWithUnlistedChildren();
	void GetOldIMAPHostNameEvent(char **oldHostName);
	void UpgradeToSubscriptionFinishedEvent(EIMAPSubscriptionUpgradeState);
	XP_Bool PromptUserForSubscribeUpgradePath();
	virtual void SubscribeToAllFolders();
	virtual void UpgradeToSubscription();

	virtual void RefreshACLForFolder(const char *mailboxName);
	virtual void RefreshACLForFolderIfNecessary(const char *mailboxName);
	virtual void RefreshAllACLs();

    char *CreatePossibleTrashName(const char *prefix);
    
	uint32 CountMessagesInIdString(const char *idString);

	void AdjustChunkSize(void);

	char *GetArbitraryHeadersToDownload();

protected:
    TImapFEEventQueue   *fFEEventQueue;
    ActiveEntry         *fCurrentEntry;
    int32               fInputBufferSize;
    char                *fInputSocketBuffer;
	TImapFlagAndUidState *fFlagState;
	time_t				fStartTime;					// time in secs we started to download last message
	time_t				fEndTime;					// ending time
	int32				fTooFastTime;				// secs, buffer filled ? make it bigger
	int32				fIdealTime;					// secs, buffer filled, good.
	int32				fChunkAddSize;				// size to add to buffer when not big enough
	int32				fChunkStartSize;			// size of buffer to start with or to reset to in bad connections
	int32				fMaxChunkSize;				// max size buffer can grow
	XP_Bool				fNewMail;					// is there new mail waiting for next biff ?
	XP_Bool				fTrackingTime;				// are we supposed to track it ?

private:
    XP_Bool             fFEeventCompleted;
	XP_Bool				fTunnelCompleted;
    XP_Bool             fEventQueueEmptySignalHappened;
    XP_Bool             fMessageUploadCompleted;
    XP_Bool             fFetchMsgListIsNew;
    XP_Bool             fMsgAppendSizeIsNew;
    XP_Bool             fNextEntryEventSignalHappened;
    XP_Bool             fThreadShouldDie;
    XP_Bool				fCallingNetlibAllTheTime;
    XP_Bool				fIsSafeToDie; 
    XP_Bool				fNeedGreeting;
	XP_Bool				fNeedNoop;
	XP_Bool				fMailToFetch;
	XP_Bool				fGetHeaders;
	XP_Bool				fShouldUpgradeToSubscription;
	XP_Bool				fUpgradeShouldLeaveAlone;
	XP_Bool				fUpgradeShouldSubscribeAll;

    int					fProgressStringId;
    uint32				fProgressIndex;
    uint32				fProgressCount;
	int64				fLastProgressTime;				
	int					fLastPercent;
	int					fLastProgressStringId;

    
    uint32				*fFetchMsgIdList;
    uint32				fFetchCount;
    
    TIMAPUrl            *fCurrentUrl;
    TCP_ConData         *fTCPConData;
    PRThread            *fBlockingThread;
    PRMonitor           *fEventCompletionMonitor;
	PRMonitor			*fTunnelCompletionMonitor;
    PRMonitor           *fActiveEntryMonitor;
    PRMonitor           *fEventQueueEmptySignalMonitor;
    PRMonitor           *fMessageUploadMonitor;
    PRMonitor           *fMsgCopyDataMonitor;
    PRMonitor           *fThreadDeathMonitor;
	PRMonitor			*fPermissionToDieMonitor;
	PRMonitor			*fPseudoInterruptMonitor;
	PRMonitor			*fWaitForBodyIdsMonitor;
    PRFileDesc          *fIOSocket;
    NET_StreamClass     *fOutputStream;
    
    enum EMailboxHierarchyNameState {
        kNoOperationInProgress,
        kDiscoverBaseFolderInProgress,
		kDiscoverTrashFolderInProgress,
        kDeleteSubFoldersInProgress,
		kListingForInfoOnly,
		kListingForInfoAndDiscovery,
		kDiscoveringNamespacesOnly
    };
    
    XP_Bool fOnlineBaseFolderExists;
    
    ImapHierarchyMover *fHierarchyMover;
    const char *fRootToBeRenamed;
    EMailboxHierarchyNameState fHierarchyNameState;
    
    XP_List    *fDeletableChildren;
    
    int32               fBytesMovedSoFarForProgress;
    
    uint32               fAppendMessageSize;
    imapMessageFlagsType fAppendMsgFlags;
    
    TLineDownloadCache  fDownLoadLineCache;
    
    EMailboxDiscoverStatus fDiscoveryStatus;

    static PRMonitor    *fFindingMailboxesMonitor;
    static PRMonitor    *fUpgradeToSubscriptionMonitor;
	static XP_Bool		fHaveWeEverCheckedForSubscriptionUpgrade;
	static MSG_BIFF_STATE fCurrentBiffState;

	// chunking prefs
	XP_Bool fFetchByChunks;
	int32 fChunkSize;
	int32 fChunkThreshold;

	XP_Bool fAutoSubscribe, fAutoUnsubscribe, fAutoSubscribeOnOpen, fAutoUnsubscribeFromNoSelect;
	
	XP_List	*fListedMailboxList;
	static XP_Bool	IsConnectionActive(const char *hostName, const char *folderName);

protected:
	static XPPtrArray *connectionList;
	TIMAPSocketInfo		*fSocketInfo;
	XP_Bool fPseudoInterrupted;
	char	*fHostName;
	MSG_FolderInfo	*fFolderInfo;
	XP_Bool	fFolderNeedsSubscribing, fFolderNeedsACLRefreshed;
	XP_Bool	fActive;

};




HG83666


class TIMAPSocketInfo {

	// methods
public:
	TIMAPSocketInfo();

	PRFileDesc *	GetIOSocket() { return fIOSocket; }
	void		SetIOSocket(PRFileDesc *sock) { fIOSocket = sock; }

	char**		GetNewLineBuffer() { return &newLine; }
	void		SetNewLineBuffer(char *buffer) { newLine = buffer; }
	
	int			GetReadStatus() { return readStatus; }
	void		SetReadStatus(int status) { readStatus = status; }

	int32*		GetInputBufferSize() { return pInputBufferSize; }
	void		SetInputBufferSize(int32 *size) { pInputBufferSize = size; }

	char**		GetInputSocketBuffer() { return pInputSocketBuffer; }
	void		SetInputSocketBuffer(char **buf) { pInputSocketBuffer = buf; }

	Bool		GetPauseForRead() { return pauseForRead; }
	void		SetPauseForRead(Bool pause) { pauseForRead = pause; }

	// members
public:
	PRFileDesc	*fIOSocket;
	char**		pInputSocketBuffer;
    int32*		pInputBufferSize;
	char*		newLine;
	Bool		pauseForRead;
	int			readStatus;
	int			writeStatus;
	char*		writeLine;
};


// This class is only used for passing data
// between the IMAP and mozilla thread
class TIMAPACLRightsInfo
{
public:
	char *hostName, *mailboxName, *userName, *rights;
};

// a simple reference counting class used to keep Biff from interrupting an INBOX connection
// the inbox usage count is incremented by one if TRUE is passed to the constructor and then
// decremented by the destructor
class TInboxReferenceCount {
public:
	TInboxReferenceCount(XP_Bool bumpInboxCount);
	~TInboxReferenceCount();
	
	int	GetInboxUsageCount();
	
	static void ImapStartup();
	static void ImapShutDown();
private:
    static PRMonitor    *fgDataSafeMonitor;
    static int			fgInboxUsageCount;
    XP_Bool				fInboxCountWasBumped;
};	



class TIMAPNamespace
{

public:
	TIMAPNamespace(EIMAPNamespaceType type, const char *prefix, char delimiter, XP_Bool from_prefs);

	~TIMAPNamespace();

	EIMAPNamespaceType	GetType() { return m_namespaceType; }
	const char *		GetPrefix() { return m_prefix; }
	char				GetDelimiter() { return m_delimiter; }
	void				SetDelimiter(char delimiter);
	XP_Bool				GetIsDelimiterFilledIn() { return m_delimiterFilledIn; }
	XP_Bool				GetIsNamespaceFromPrefs() { return m_fromPrefs; }

	// returns -1 if this box is not part of this namespace,
	// or the length of the prefix if it is part of this namespace
	int					MailboxMatchesNamespace(const char *boxname);

protected:
	EIMAPNamespaceType m_namespaceType;
	char	*m_prefix;
	char	m_delimiter;
	XP_Bool	m_fromPrefs;
	XP_Bool m_delimiterFilledIn;

};


// represents a list of namespaces for a given host
// Basically, a C++ wrapper around an XP_List
class TIMAPNamespaceList
{
public:
	~TIMAPNamespaceList();

	static TIMAPNamespaceList *CreateTIMAPNamespaceList();

	void ClearNamespaces(XP_Bool deleteFromPrefsNamespaces, XP_Bool deleteServerAdvertisedNamespaces);
	int	GetNumberOfNamespaces();
	int	GetNumberOfNamespaces(EIMAPNamespaceType);
	TIMAPNamespace *GetNamespaceNumber(int nodeIndex);
	TIMAPNamespace *GetNamespaceNumber(int nodeIndex, EIMAPNamespaceType);

	TIMAPNamespace *GetDefaultNamespaceOfType(EIMAPNamespaceType type);
	int AddNewNamespace(TIMAPNamespace *ns);
	TIMAPNamespace *GetNamespaceForMailbox(const char *boxname);

protected:
	TIMAPNamespaceList();	// use CreateTIMAPNamespaceList to create one

	XP_List *m_NamespaceList;

};


// This class is currently only used for the one-time upgrade
// process from a LIST view to the subscribe model.
// It basically contains the name of a mailbox and whether or not
// its children have been listed.
class TIMAPMailboxInfo
{
public:
	TIMAPMailboxInfo(const char *name);
	virtual ~TIMAPMailboxInfo();
	void SetChildrenListed(XP_Bool childrenListed) { m_childrenListed = childrenListed; }
	XP_Bool GetChildrenListed() { return m_childrenListed; }
	const char *GetMailboxName() { return m_mailboxName; }

protected:
	XP_Bool m_childrenListed;
	char *m_mailboxName;
};



#endif	// IMAP4PVT_H
