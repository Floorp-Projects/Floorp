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
/* this file defines the syntax of the imap4 url's and offers functions
   that create url strings.  If the functions do not offer enough 
   functionality then let kevin know before you starting creating strings
   from scratch. */
   
#ifndef IMAP4URL_H
#define IMAP4URL_H

#include "xp_mcom.h"

/* 45678901234567890123456789012345678901234567890123456789012345678901234567890
*/ 


XP_BEGIN_PROTOS

/* need mailbox status urls to get the number of message and the
  number of unread messages */

/* Creating a mailbox */
/* imap4://HOST?create?MAILBOXPATH */
char *CreateImapMailboxCreateUrl(const char *imapHost, const char *mailbox);

/* deleting a mailbox */
/* imap4://HOST?delete?MAILBOXPATH */
char *CreateImapMailboxDeleteUrl(const char *imapHost, const char *mailbox);

/* renaming a mailbox */
/* imap4://HOST?rename?OLDNAME?NEWNAME */
char *CreateImapMailboxRenameUrl(const char *imapHost, 
								 const char *oldBoxName,
								 const char *newBoxName);

/* listing available mailboxes */
/* imap4://HOST?list */
char *CreateImapListUrl(const char *imapHost);


/* fetching RFC822 messages */
/* imap4://HOST?fetch?<UID/SEQUENCE>?MAILBOXPATH?x */
/*   'x' is the message UID or sequence number list */
/* will set the 'SEEN' flag */
char *CreateImapMessageFetchUrl(const char *imapHost,
								const char *mailbox,
								const char *messageIdentifierList,
								XP_Bool messageIdsAreUID);
								
								
/* fetching the headers of RFC822 messages */
/* imap4://HOST?header?<UID/SEQUENCE>?MAILBOXPATH?x */
/*   'x' is the message UID or sequence number list */
/* will not affect the 'SEEN' flag */
char *CreateImapMessageHeaderUrl(const char *imapHost,
								 const char *mailbox,
								 const char *messageIdentifierList,
								 XP_Bool messageIdsAreUID);
								 
/* dump headers url.  Notify the front end when the mailbox is selected and
   when each message line is dumped */
char *CreateImapMessageHeaderDumpUrl(const char *imapHost,
								     const char *mailbox);

/* search an online mailbox */
/* imap4://HOST?search?<UID/SEQUENCE>?MAILBOXPATH?SEARCHSTRING */
/*   'x' is the message sequence number list */
char *CreateImapSearchUrl(const char *imapHost,
						  const char *mailbox,
						  const char *searchString,
						  XP_Bool messageIdsAreUID);

/* delete messages */
/* imap4://HOST?deletemsg?<UID/SEQUENCE>?MAILBOXPATH?x */
/*   'x' is the message UID or sequence number list */
char *CreateImapDeleteMessageUrl(const char *imapHost,
								 const char *mailbox,
								 const char *messageIds,
								 XP_Bool idsAreUids);

/* mark messages as read */
/* imap4://HOST?markread?<UID/SEQUENCE>?MAILBOXPATH?x */
/*   'x' is the message UID or sequence number list */
char *CreateImapMarkMessageReadUrl(const char *imapHost,
								   const char *mailbox,
								   const char *messageIds,
								   XP_Bool idsAreUids);

/* mark messages as unread */
/* imap4://HOST?markunread?<UID/SEQUENCE>?MAILBOXPATH?x */
/*   'x' is the message UID or sequence number list */
char *CreateImapMarkMessageUnReadUrl(const char *imapHost,
								     const char *mailbox,
								 	 const char *messageIds,
								 	 XP_Bool idsAreUids);

/* copy messages from one online box to another */
/* imap4://HOST?onlineCopy?<UID/SEQUENCE>?
         SOURCEMAILBOXPATH?x?DESTINATIONMAILBOXPATH */
/*   'x' is the message UID or sequence number list */
char *CreateImapOnlineCopyUrl(const char *imapHost,
							  const char *sourceMailbox,
							  const char *messageIds,
							  const char *destinationMailbox,
							  XP_Bool idsAreUids);

#if DOTHISSTUFFLATER
/* copy a message from an online box to an offline box */
/* imap4://HOST?ontooffCopy?SOURCEMAILBOXPATH?number=x?
         DESTINATIONMAILBOXPATH */
/*   'x' is the message sequence number */
char *CreateImapOnToOfflineCopyUrl(const char *imapHost,
							       const char *sourceOnlineMailbox,
							       int32 messageSequenceNumber,
							       const char *destinationOfflineMailbox);

/* copy a message from an offline box to an online box */
/* imap4://HOST?offtoonCopy?SOURCEMAILBOXPATH?number=x?
         DESTINATIONMAILBOXPATH */
/*   'x' is the message sequence number */
char *CreateImapOffToOnlineCopyUrl(const char *imapHost,
							       const char *sourceOnlineMailbox,
							       int32 messageSequenceNumber,
							       const char *destinationOfflineMailbox);

/* get mail account rul */
/* imap4://HOST?NETSCAPE */
char *CreateImapManageMailAccountUrl(const char *imapHost);

/* append message from file url */
/* imap4://HOST?appendmsgfromfile>DESTINATIONMAILBOXPATH */
char *CreateImapAppendMessageFromFileUrl(const char *imapHost,
										 const char *destinationMailboxPath,
										 const char hierarchySeparator,
										 XP_Bool isDraft);

#endif

XP_END_PROTOS

#endif /* IMAP4URL_H */

