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
#include "mkutils.h"

#ifndef __imap__
#include "imap.h"
#endif

#ifndef _MCOM_H_
#include "xp_mcom.h"
#endif

/* 45678901234567890123456789012345678901234567890123456789012345678901234567890
*/ 

char useme;

static char *createStartOfIMAPurl(const char *imapHost, int additionalSize)
{
	static const char *formatString = "IMAP://%s?";

	char *returnString = XP_ALLOC(XP_STRLEN(formatString) + XP_STRLEN(imapHost) + additionalSize);
	if (returnString)
		sprintf(returnString, formatString, imapHost);
	
	return returnString;
}


/* Selecting a mailbox */
/* imap4://HOST>select>MAILBOXPATH */
char *CreateImapMailboxSelectUrl(const char *imapHost, 
								 const char *mailbox,
								 char  hierarchySeparator,
								 const char *undoDeleteIdentifierList)
{
	static const char *formatString = "select>%c%s>%s";
	
		/* 22 enough for huge index string  */
	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + 
														XP_STRLEN(mailbox) + 
														(undoDeleteIdentifierList ? XP_STRLEN(undoDeleteIdentifierList) : 1) + 
														22);

	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), 
				formatString, 
				hierarchySeparator, 
				mailbox, 
				undoDeleteIdentifierList ? undoDeleteIdentifierList : "");
	
	return returnString;
}

/* lite select, used to verify UIDVALIDITY while going on/offline */
char *CreateImapMailboxLITESelectUrl(const char *imapHost, 
								 const char *mailbox,
								 char  hierarchySeparator)
{
	static const char *formatString = "liteselect>%c%s";
	
	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + XP_STRLEN(mailbox));

	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), 
				formatString, 
				hierarchySeparator, 
				mailbox);
	
	return returnString;
}

/* expunge, used in traditional imap delete model */
char *CreateImapMailboxExpungeUrl(const char *imapHost, 
								 const char *mailbox,
								 char  hierarchySeparator)
{
	static const char *formatString = "expunge>%c%s";
	
	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + XP_STRLEN(mailbox));

	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), 
				formatString, 
				hierarchySeparator, 
				mailbox);
	
	return returnString;
}

/* Creating a mailbox */
/* imap4://HOST>create>MAILBOXPATH */
char *CreateImapMailboxCreateUrl(const char *imapHost, const char *mailbox,char hierarchySeparator)
{
	static const char *formatString = "create>%c%s";

	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + XP_STRLEN(mailbox));
	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString, hierarchySeparator, mailbox);
	
	return returnString;
}

/* discover the mailboxes of this account  */
char *CreateImapAllMailboxDiscoveryUrl(const char *imapHost)
{
	static const char *formatString = "discoverallboxes";

	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString));
	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString);
	
	return returnString;
}


/* discover the mailboxes of this account, and the subscribed mailboxes  */
char *CreateImapAllAndSubscribedMailboxDiscoveryUrl(const char *imapHost)
{
	static const char *formatString = "discoverallandsubscribedboxes";

	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString));
	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString);
	
	return returnString;
}

/* discover the children of this mailbox */
char *CreateImapChildDiscoveryUrl(const char *imapHost, const char *mailbox,char hierarchySeparator)
{
	static const char *formatString = "discoverchildren>%c%s";

	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + XP_STRLEN(mailbox));
	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString, hierarchySeparator, mailbox);
	
	return returnString;
}
/* discover the n-th level deep children of this mailbox */
char *CreateImapLevelChildDiscoveryUrl(const char *imapHost, const char *mailbox,char hierarchySeparator, int n)
{
	static const char *formatString = "discoverlevelchildren>%d>%c%s";

	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + XP_STRLEN(mailbox));
	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString, n, hierarchySeparator, mailbox);
	
	return returnString;
}

/* deleting a mailbox */
/* imap4://HOST>delete>MAILBOXPATH */
char *CreateImapMailboxDeleteUrl(const char *imapHost, const char *mailbox, char hierarchySeparator)
{
	static const char *formatString = "delete>%c%s";

	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + XP_STRLEN(mailbox));
	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString, hierarchySeparator, mailbox);
	
	return returnString;
}

/* renaming a mailbox */
/* imap4://HOST>rename>OLDNAME>NEWNAME */
char *CreateImapMailboxRenameLeafUrl(const char *imapHost, 
								 const char *oldBoxPathName,
								 char hierarchySeparator,
								 const char *newBoxLeafName)
{
	static const char *formatString = "rename>%c%s>%c%s";

	char *returnString = NULL;
	
	/* figure out the new mailbox name */
	char *slash;
	char *newPath = XP_ALLOC(XP_STRLEN(oldBoxPathName) + XP_STRLEN(newBoxLeafName) + 1);
	if (newPath)
	{
		XP_STRCPY (newPath, oldBoxPathName);
		slash = XP_STRRCHR (newPath, '/'); 
		if (slash)
			slash++;
		else
			slash = newPath;	/* renaming a 1st level box */
			
		XP_STRCPY (slash, newBoxLeafName);
		
		
		returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + XP_STRLEN(oldBoxPathName) + XP_STRLEN(newPath));
		if (returnString)
			sprintf(returnString + XP_STRLEN(returnString), formatString, hierarchySeparator, oldBoxPathName, hierarchySeparator, newPath);
		
		XP_FREE( newPath);
	}
	
	return returnString;
}

/* renaming a mailbox, moving hierarchy */
/* imap4://HOST>movefolderhierarchy>OLDNAME>NEWNAME */
/* oldBoxPathName is the old name of the child folder */
/* destinationBoxPathName is the name of the new parent */
char *CreateImapMailboxMoveFolderHierarchyUrl(const char *imapHost, 
								              const char *oldBoxPathName,
								              char  oldHierarchySeparator,
								              const char *newBoxPathName,
								              char  newHierarchySeparator)
{
	static const char *formatString = "movefolderhierarchy>%c%s>%c%s";

	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + XP_STRLEN(oldBoxPathName) + XP_STRLEN(newBoxPathName));
	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString, oldHierarchySeparator, oldBoxPathName, newHierarchySeparator, newBoxPathName);

	return returnString;
}

/* listing available mailboxes */
/* imap4://HOST>list>referenceName>MAILBOXPATH */
/* MAILBOXPATH can contain wildcard */
/* **** jefft -- I am using this url to detect whether an mailbox
   exists on the Imap sever
 */
char *CreateImapListUrl(const char *imapHost,
						const char *mailbox,
						const char hierarchySeparator)
{
	static const char *formatString = "list>%c%s";

	char *returnString = createStartOfIMAPurl(imapHost,
											  XP_STRLEN(formatString) +
											  XP_STRLEN(mailbox) + 1);
	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString,
				hierarchySeparator, mailbox);

	return returnString;
}

/* biff */
char *CreateImapBiffUrl(const char *imapHost,
						const char *mailbox,
						char hierarchySeparator,
						uint32 uidHighWater)
{
	static const char *formatString = "biff>%c%s>%ld";
	
		/* 22 enough for huge uid string  */
	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + 
														XP_STRLEN(mailbox) + 22);

	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString, hierarchySeparator, mailbox, (long)uidHighWater);
	
	return returnString;
}


static const char *sequenceString = "SEQUENCE";
static const char *uidString = "UID";

/* fetching RFC822 messages */
/* imap4://HOST>fetch><UID/SEQUENCE>>MAILBOXPATH>x */
/*   'x' is the message UID or sequence number list */
/* will set the 'SEEN' flag */
char *CreateImapMessageFetchUrl(const char *imapHost,
								const char *mailbox,
								char hierarchySeparator,
								const char *messageIdentifierList,
								XP_Bool messageIdsAreUID)
{
	static const char *formatString = "fetch>%s>%c%s>%s";

	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + XP_STRLEN(sequenceString) + XP_STRLEN(mailbox) + XP_STRLEN(messageIdentifierList));
	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString, messageIdsAreUID ? uidString : sequenceString, hierarchySeparator, mailbox, messageIdentifierList);
	
	return returnString;
}

/* fetching the headers of RFC822 messages */
/* imap4://HOST>header><UID/SEQUENCE>>MAILBOXPATH>x */
/*   'x' is the message UID or sequence number list */
/* will not affect the 'SEEN' flag */
char *CreateImapMessageHeaderUrl(const char *imapHost,
								 const char *mailbox,
								 char hierarchySeparator,
								 const char *messageIdentifierList,
								 XP_Bool messageIdsAreUID)
{
	static const char *formatString = "header>%s>%c%s>%s";

	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + XP_STRLEN(sequenceString) + XP_STRLEN(mailbox) + XP_STRLEN(messageIdentifierList));
	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString, messageIdsAreUID ? uidString : sequenceString, hierarchySeparator, mailbox, messageIdentifierList);
	
	return returnString;
}

/* search an online mailbox */
/* imap4://HOST>search><UID/SEQUENCE>>MAILBOXPATH>SEARCHSTRING */
/*   'x' is the message sequence number list */
char *CreateImapSearchUrl(const char *imapHost,
						  const char *mailbox,
						  char hierarchySeparator,
						  const char *searchString,
						  XP_Bool messageIdsAreUID)
{
	static const char *formatString = "search>%s>%c%s>%s";

	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + XP_STRLEN(sequenceString) + XP_STRLEN(mailbox) + XP_STRLEN(searchString));
	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString, messageIdsAreUID ? uidString : sequenceString, hierarchySeparator, mailbox, searchString);
	
	return returnString;	
}

/* delete messages */
/* imap4://HOST>deletemsg><UID/SEQUENCE>>MAILBOXPATH>x */
/*   'x' is the message UID or sequence number list */
char *CreateImapDeleteMessageUrl(const char *imapHost,
								 const char *mailbox,
								 char hierarchySeparator,
								 const char *messageIds,
								 XP_Bool idsAreUids)
{
	static const char *formatString = "deletemsg>%s>%c%s>%s";

	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + XP_STRLEN(sequenceString) + XP_STRLEN(mailbox) + XP_STRLEN(messageIds));
	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString, idsAreUids ? uidString : sequenceString, hierarchySeparator, mailbox, messageIds);
	
	return returnString;
}

/* delete all messages */
/* imap4://HOST>deleteallmsgs>MAILBOXPATH */
char *CreateImapDeleteAllMessagesUrl(const char *imapHost,
								     const char *mailbox,
								     char hierarchySeparator)
{
	static const char *formatString = "deleteallmsgs>%c%s";

	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + XP_STRLEN(mailbox));
	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString, hierarchySeparator, mailbox);
	
	return returnString;
}

/* store +flags url */
/* imap4://HOST>store+flags><UID/SEQUENCE>>MAILBOXPATH>x>f */
/*   'x' is the message UID or sequence number list */
/*   'f' is the byte of flags */
char *CreateImapAddMessageFlagsUrl(const char *imapHost,
								   const char *mailbox,
								   char hierarchySeparator,
								   const char *messageIds,
								   imapMessageFlagsType flags,
								   XP_Bool idsAreUids)
{
	static const char *formatString = "addmsgflags>%s>%c%s>%s>%d";

	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + XP_STRLEN(sequenceString) + XP_STRLEN(mailbox) + XP_STRLEN(messageIds) + 10);
	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString, idsAreUids ? uidString : sequenceString, hierarchySeparator, mailbox, messageIds, (int) flags);
	
	return returnString;
}

/* store -flags url */
/* imap4://HOST>store-flags><UID/SEQUENCE>>MAILBOXPATH>x>f */
/*   'x' is the message UID or sequence number list */
/*   'f' is the byte of flags */
char *CreateImapSubtractMessageFlagsUrl(const char *imapHost,
								        const char *mailbox,
								        char hierarchySeparator,
								        const char *messageIds,
								        imapMessageFlagsType flags,
								        XP_Bool idsAreUids)
{
	static const char *formatString = "subtractmsgflags>%s>%c%s>%s>%d";

	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + XP_STRLEN(sequenceString) + XP_STRLEN(mailbox) + XP_STRLEN(messageIds) + 10);
	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString, idsAreUids ? uidString : sequenceString, hierarchySeparator, mailbox, messageIds, (int) flags);
	
	return returnString;
}

/* set flags url, make the flags match */
char *CreateImapSetMessageFlagsUrl(const char *imapHost,
								        const char *mailbox,
								   		char  hierarchySeparator,
								        const char *messageIds,
								        imapMessageFlagsType flags,
								        XP_Bool idsAreUids)
{
	static const char *formatString = "setmsgflags>%s>%c%s>%s>%d";

	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + XP_STRLEN(sequenceString) + XP_STRLEN(mailbox) + XP_STRLEN(messageIds) + 10);
	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString, idsAreUids ? uidString : sequenceString, hierarchySeparator, mailbox, messageIds, (int) flags);
	
	return returnString;
}

/* copy messages from one online box to another */
/* imap4://HOST>onlineCopy><UID/SEQUENCE>>SOURCEMAILBOXPATH>x>
			DESTINATIONMAILBOXPATH */
/*   'x' is the message UID or sequence number list */
char *CreateImapOnlineCopyUrl(const char *imapHost,
							  const char *sourceMailbox,
							  char  sourceHierarchySeparator,
							  const char *messageIds,
							  const char *destinationMailbox,
							  char  destinationHierarchySeparator,
							  XP_Bool idsAreUids,
							  XP_Bool isMove)
{
	static const char *formatString = "%s>%s>%c%s>%s>%c%s";
	static const char *moveString   = "onlinemove";
	static const char *copyString   = "onlinecopy";


	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + XP_STRLEN(moveString) + XP_STRLEN(sequenceString) + XP_STRLEN(sourceMailbox) + XP_STRLEN(messageIds) + XP_STRLEN(destinationMailbox));
	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString, 
														isMove ? moveString : copyString, 
														idsAreUids ? uidString : sequenceString, 
														sourceHierarchySeparator, sourceMailbox,
														messageIds,
														destinationHierarchySeparator, destinationMailbox);

	
	
	return returnString;
}

/* copy messages from one online box to another */
/* imap4://HOST>onlineCopy><UID/SEQUENCE>>SOURCEMAILBOXPATH>x>
			DESTINATIONMAILBOXPATH */
/*   'x' is the message UID or sequence number list */
char *CreateImapOnToOfflineCopyUrl(const char *imapHost,
							       const char *sourceMailbox,
							       char  sourceHierarchySeparator,
							       const char *messageIds,
							       const char *destinationMailbox,
							       XP_Bool idsAreUids,
							       XP_Bool isMove)
{
	static const char *formatString = "%s>%s>%c%s>%s>%c%s";
	static const char *moveString   = "onlinetoofflinemove";
	static const char *copyString   = "onlinetoofflinecopy";


	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + XP_STRLEN(moveString) + XP_STRLEN(sequenceString) + XP_STRLEN(sourceMailbox) + XP_STRLEN(messageIds) + XP_STRLEN(destinationMailbox));
	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString, 
														isMove ? moveString : copyString, 
														idsAreUids ? uidString : sequenceString, 
														sourceHierarchySeparator, sourceMailbox,
														messageIds, 
														kOnlineHierarchySeparatorUnknown, destinationMailbox);

	
	
	return returnString;
}

/* copy messages from an offline box to an online box */
/* imap4://HOST>offtoonCopy>SOURCEMAILBOXPATH>x>
			DESTINATIONMAILBOXPATH */
/*   'x' is the size of the message to upload */
char *CreateImapOffToOnlineCopyUrl(const char *imapHost,
							       const char *destinationMailbox,
							       char  destinationHierarchySeparator)
{
	static const char *formatString = "offlinetoonlinecopy>%c%s";

	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + XP_STRLEN(destinationMailbox));
	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString, destinationHierarchySeparator, destinationMailbox);
	
	return returnString;
}

/* get mail account rul */
/* imap4://HOST>NETSCAPE */
char *CreateImapManageMailAccountUrl(const char *imapHost)
{
	static const char *formatString = "netscape";

	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + 1);
	StrAllocCat(returnString, formatString);;
	
	return returnString;
}

/* append message from file url */
/* imap4://HOST>appendmsgfromfile>DESTINATIONMAILBOXPATH */
char *CreateImapAppendMessageFromFileUrl(const char *imapHost,
										 const char *destinationMailboxPath,
										 const char hierarchySeparator,
										 XP_Bool isDraft)
{
	const char *formatString = isDraft ? "appenddraftfromfile>%c%s" :
		"appendmsgfromfile>%c%s";
	char *returnString = 
	  createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) +
						   XP_STRLEN(destinationMailboxPath));

	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString, 
				hierarchySeparator, destinationMailboxPath);

	return returnString;
}

/* Subscribe to a mailbox on the given IMAP host */
char *CreateIMAPSubscribeMailboxURL(const char *imapHost, const char *mailboxName)
{
	/* we don't need the hierarchy delimiter, so just use slash ("/") */
	static const char *formatString = "subscribe>/%s";	

	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + XP_STRLEN(mailboxName));
	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString, mailboxName);
	
	return returnString;

}

/* Unsubscribe from a mailbox on the given IMAP host */
char *CreateIMAPUnsubscribeMailboxURL(const char *imapHost, const char *mailboxName)
{
	/* we don't need the hierarchy delimiter, so just use slash ("/") */
	static const char *formatString = "unsubscribe>/%s";

	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + XP_STRLEN(mailboxName));
	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString, mailboxName);
	
	return returnString;

}


/* Refresh the ACL for a folder on the given IMAP host */
char *CreateIMAPRefreshACLForFolderURL(const char *imapHost, const char *mailboxName)
{
	/* we don't need the hierarchy delimiter, so just use slash ("/") */
	static const char *formatString = "refreshacl>/%s";

	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + XP_STRLEN(mailboxName));
	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString, mailboxName);
	
	return returnString;

}

/* Refresh the ACL for all folders on the given IMAP host */
char *CreateIMAPRefreshACLForAllFoldersURL(const char *imapHost)
{
	/* we don't need the hierarchy delimiter, so just use slash ("/") */
	static const char *formatString = "refreshallacls>/";

	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString));
	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString);
	
	return returnString;

}

/* Auto-Upgrade to IMAP subscription */
char *CreateIMAPUpgradeToSubscriptionURL(const char *imapHost, XP_Bool subscribeToAll)
{
	static char *formatString = "upgradetosubscription>/";
	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString));
	if (subscribeToAll)
		formatString[XP_STRLEN(formatString)-1] = '.';

	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString);
	
	return returnString;

}

/* do a status command on a folder on the given IMAP host */
char *CreateIMAPStatusFolderURL(const char *imapHost, const char *mailboxName, char  hierarchySeparator)
{
	static const char *formatString = "folderstatus>%c%s";
	
	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + XP_STRLEN(mailboxName));

	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), 
				formatString, 
				hierarchySeparator, 
				mailboxName);
	
	return returnString;

}

/* Refresh the admin url for a folder on the given IMAP host */
char *CreateIMAPRefreshFolderURLs(const char *imapHost, const char *mailboxName)
{
	/* we don't need the hierarchy delimiter, so just use slash ("/") */
	static const char *formatString = "refreshfolderurls>/%s";

	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + XP_STRLEN(mailboxName));
	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString, mailboxName);
	
	return returnString;

}

/* Force the reload of all parts of the message given in url */
char *IMAP_CreateReloadAllPartsUrl(const char *url)
{
	char *returnUrl = PR_smprintf("%s&allparts", url);
	return returnUrl;
}

/* Explicitly LIST a given mailbox, and refresh its flags in the folder list */
char *CreateIMAPListFolderURL(const char *imapHost, const char *mailboxName)
{
	/* we don't need the hierarchy delimiter, so just use slash ("/") */
	static const char *formatString = "listfolder>/%s";

	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + XP_STRLEN(mailboxName));
	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString, mailboxName);
	
	return returnString;
}
