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
#include "imap.h"
#include "imap4pvt.h"

#define ISHEX(c) ( ((c) >= '0' && (c) <= '9') || ((c) >= 'a' && (c) <= 'f') || ((c) >= 'A' && (c) <= 'F') )
#define NONHEX(c) (!ISHEX(c))

extern "C" char * escape_unescaped_percents(const char *incomingURL);

extern "C"
{
char * escape_unescaped_percents(const char *incomingURL)
{
	const char *inC;
	char *outC;
	char *result = (char *) XP_ALLOC(XP_STRLEN(incomingURL)*3+1);

	if (result)
	{
		for(inC = incomingURL, outC = result; *inC != '\0'; inC++)
		{
			if (*inC == '%')
			{
				// Check if either of the next two characters are non-hex.
				if ( !*(inC+1) || NONHEX(*(inC+1)) || !*(inC+2) || NONHEX(*(inC+2)) )
				{
					// Hex characters don't follow, escape the 
					// percent char
					*outC++ = '%'; *outC++ = '2'; *outC++ = '5';
				}
				else
				{
					// Hex characters follow, so assume the percent 
					// is escaping something else
					*outC++ = *inC;
				}
			}
			else
				*outC++ = *inC;
		}
		*outC = '\0';
	}

	return result;
}
}

// member functions of the TIMAPUrl class
TIMAPUrl::TIMAPUrl(const char *url_string, XP_Bool internal)
 : fHostSubString(nil),
   fUrlidSubString(nil),
   fSourceCanonicalFolderPathSubString(nil),
   fDestinationCanonicalFolderPathSubString(nil),
   fSearchCriteriaString(nil),
   fListOfMessageIds(nil),
   fTokenPlaceHolder(nil),
   fFlags(0),
   fIdsAreUids(FALSE),
   fIMAPstate(kAuthenticatedStateURL), 
   fUrlType(kTest),
   fValidURL(FALSE),
   fMimePartSelectorDetected(FALSE),
   fInternal(internal),
   fDiscoveryDepth(0)
{  
	fOnlineSubDirSeparator = '/';	// initial guess
	fUrlString = escape_unescaped_percents(url_string); // this duplicates url_string
	fUrlString = NET_UnEscape(fUrlString); // ### mwelch - Put spaces and '>'s back in.
	Parse();
}


TIMAPUrl::~TIMAPUrl()
{
	FREEIF( fUrlString);
}

void TIMAPUrl::ParseFolderPath(char **resultingCanonicalPath)
{
	*resultingCanonicalPath = fTokenPlaceHolder ? XP_STRTOK_R(nil, IMAP_URL_TOKEN_SEPARATOR, &fTokenPlaceHolder) : (char *)NULL;
	
	if (!*resultingCanonicalPath)
	{
		fValidURL = FALSE;
		return;
	}

	// The delimiter will be set for a given URL, but will not be statically available
	// from an arbitrary URL.  It is the creator's responsibility to fill in the correct
	// delimiter from the folder's namespace when creating the URL.
	char dirSeparator = *(*resultingCanonicalPath)++;
	if (dirSeparator != kOnlineHierarchySeparatorUnknown)
		SetOnlineSubDirSeparator( dirSeparator);
	
	// if dirSeparator == kOnlineHierarchySeparatorUnknown, then this must be a create
	// of a top level imap box.  If there is an online subdir, we will automatically
	// use its separator.  If there is not an online subdir, we don't need a separator.
	
	/*
	
	// I don't think we want to do any of this anymore (for 5.0).

	const char *hostDir = TIMAPHostInfo::GetPersonalOrDefaultOnlineSubDirForHost(GetUrlHost());
	if (!hostDir)
	{
		// couldn't find the host in our list
		fValidURL = FALSE;
		return;
	}
	int lengthOfImapSubDirString = XP_STRLEN(hostDir);
	
	if (*resultingCanonicalPath && 
	    ((lengthOfImapSubDirString + XP_STRLEN("INBOX")) == XP_STRLEN(*resultingCanonicalPath)) &&
	    !XP_STRCASECMP("INBOX", *resultingCanonicalPath + lengthOfImapSubDirString))
	{
		// this is the inbox
		*resultingCanonicalPath = "INBOX";
	}
	*/
}


void TIMAPUrl::ParseSearchCriteriaString()
{
	fSearchCriteriaString = fTokenPlaceHolder ? XP_STRTOK_R(nil, IMAP_URL_TOKEN_SEPARATOR, &fTokenPlaceHolder) : (char *)NULL;
	if (!fSearchCriteriaString)
		fValidURL = FALSE;
}


void TIMAPUrl::ParseChildDiscoveryDepth()
{
	char *discoveryDepth = fTokenPlaceHolder ? XP_STRTOK_R(nil, IMAP_URL_TOKEN_SEPARATOR, &fTokenPlaceHolder) : (char *)NULL;
	if (!discoveryDepth)
	{
		fValidURL = FALSE;
		fDiscoveryDepth = 0;
		return;
	}
	fDiscoveryDepth = atoi(discoveryDepth);
}

void TIMAPUrl::ParseUidChoice()
{
	char *uidChoiceString = fTokenPlaceHolder ? XP_STRTOK_R(nil, IMAP_URL_TOKEN_SEPARATOR, &fTokenPlaceHolder) : (char *)NULL;
	if (!uidChoiceString)
		fValidURL = FALSE;
	else
		fIdsAreUids = XP_STRCMP(uidChoiceString, "UID") == 0;
}

void TIMAPUrl::ParseMsgFlags()
{
	char *flagsPtr = fTokenPlaceHolder ? XP_STRTOK_R(nil, IMAP_URL_TOKEN_SEPARATOR, &fTokenPlaceHolder) : (char *)NULL;
	if (flagsPtr)
	{
		// the url is encodes the flags byte as ascii 
		int intFlags = atoi(flagsPtr);
		fFlags = (imapMessageFlagsType) intFlags;	// cast here 
	}
	else
		fFlags = 0;
}

void TIMAPUrl::ParseListofMessageIds()
{
	fListOfMessageIds = fTokenPlaceHolder ? XP_STRTOK_R(nil, IMAP_URL_TOKEN_SEPARATOR, &fTokenPlaceHolder) : (char *)NULL;
	if (!fListOfMessageIds)
		fValidURL = FALSE;
	else
	{
		fMimePartSelectorDetected = XP_STRSTR(fListOfMessageIds, "&part=") != 0;
	}
}

void TIMAPUrl::Parse()
{
	fValidURL = TRUE;	// hope for the best
	
			// first token separator is a "?" so others can grab the host
	char *urlStartToken = XP_STRTOK_R(fUrlString, "?", &fTokenPlaceHolder);
	
	if (!XP_STRNCASECMP(urlStartToken, "IMAP://",7) )
	{
		fHostSubString  = urlStartToken + 7;
		fUrlidSubString = fTokenPlaceHolder ? XP_STRTOK_R(nil, IMAP_URL_TOKEN_SEPARATOR, &fTokenPlaceHolder) : (char *)NULL;
		if (!fUrlidSubString)
		{
			fValidURL = FALSE;
			return;
		}
		
		if (!XP_STRCASECMP(fUrlidSubString, "fetch"))
		{
			fIMAPstate 					 = kSelectedStateURL;
			fUrlType   					 = kMsgFetch;
			ParseUidChoice();
			ParseFolderPath(&fSourceCanonicalFolderPathSubString);
			ParseListofMessageIds();
		}
		else if (fInternal)
		{
			if (!XP_STRCASECMP(fUrlidSubString, "header"))
			{
				fIMAPstate 					 = kSelectedStateURL;
				fUrlType   					 = kMsgHeader;
				ParseUidChoice();
				ParseFolderPath(&fSourceCanonicalFolderPathSubString);
				ParseListofMessageIds();
			}
			else if (!XP_STRCASECMP(fUrlidSubString, "deletemsg"))
			{
				fIMAPstate 					 = kSelectedStateURL;
				fUrlType   					 = kDeleteMsg;
				ParseUidChoice();
				ParseFolderPath(&fSourceCanonicalFolderPathSubString);
				ParseListofMessageIds();
			}
			else if (!XP_STRCASECMP(fUrlidSubString, "deleteallmsgs"))
			{
				fIMAPstate 					 = kSelectedStateURL;
				fUrlType   					 = kDeleteAllMsgs;
				ParseFolderPath(&fSourceCanonicalFolderPathSubString);
			}
			else if (!XP_STRCASECMP(fUrlidSubString, "addmsgflags"))
			{
				fIMAPstate 					 = kSelectedStateURL;
				fUrlType   					 = kAddMsgFlags;
				ParseUidChoice();
				ParseFolderPath(&fSourceCanonicalFolderPathSubString);
				ParseListofMessageIds();
				ParseMsgFlags();
			}
			else if (!XP_STRCASECMP(fUrlidSubString, "subtractmsgflags"))
			{
				fIMAPstate 					 = kSelectedStateURL;
				fUrlType   					 = kSubtractMsgFlags;
				ParseUidChoice();
				ParseFolderPath(&fSourceCanonicalFolderPathSubString);
				ParseListofMessageIds();
				ParseMsgFlags();
			}
			else if (!XP_STRCASECMP(fUrlidSubString, "setmsgflags"))
			{
				fIMAPstate 					 = kSelectedStateURL;
				fUrlType   					 = kSetMsgFlags;
				ParseUidChoice();
				ParseFolderPath(&fSourceCanonicalFolderPathSubString);
				ParseListofMessageIds();
				ParseMsgFlags();
			}
			else if (!XP_STRCASECMP(fUrlidSubString, "onlinecopy"))
			{
				fIMAPstate 					 = kSelectedStateURL;
				fUrlType   					 = kOnlineCopy;
				ParseUidChoice();
				ParseFolderPath(&fSourceCanonicalFolderPathSubString);
				ParseListofMessageIds();
				ParseFolderPath(&fDestinationCanonicalFolderPathSubString);
			}
			else if (!XP_STRCASECMP(fUrlidSubString, "onlinemove"))
			{
				fIMAPstate 					 = kSelectedStateURL;
				fUrlType   					 = kOnlineMove;
				ParseUidChoice();
				ParseFolderPath(&fSourceCanonicalFolderPathSubString);
				ParseListofMessageIds();
				ParseFolderPath(&fDestinationCanonicalFolderPathSubString);
			}
			else if (!XP_STRCASECMP(fUrlidSubString, "onlinetoofflinecopy"))
			{
				fIMAPstate 					 = kSelectedStateURL;
				fUrlType   					 = kOnlineToOfflineCopy;
				ParseUidChoice();
				ParseFolderPath(&fSourceCanonicalFolderPathSubString);
				ParseListofMessageIds();
				ParseFolderPath(&fDestinationCanonicalFolderPathSubString);
			}
			else if (!XP_STRCASECMP(fUrlidSubString, "onlinetoofflinemove"))
			{
				fIMAPstate 					 = kSelectedStateURL;
				fUrlType   					 = kOnlineToOfflineMove;
				ParseUidChoice();
				ParseFolderPath(&fSourceCanonicalFolderPathSubString);
				ParseListofMessageIds();
				ParseFolderPath(&fDestinationCanonicalFolderPathSubString);
			}
			else if (!XP_STRCASECMP(fUrlidSubString, "offlinetoonlinecopy"))
			{
				fIMAPstate 					 = kAuthenticatedStateURL;
				fUrlType   					 = kOfflineToOnlineMove;
				ParseFolderPath(&fDestinationCanonicalFolderPathSubString);
			}
			else if (!XP_STRCASECMP(fUrlidSubString, "search"))
			{
				fIMAPstate 					 = kSelectedStateURL;
				fUrlType   					 = kSearch;
				ParseUidChoice();
				ParseFolderPath(&fSourceCanonicalFolderPathSubString);
				ParseSearchCriteriaString();
			}
			else if (!XP_STRCASECMP(fUrlidSubString, "test"))
			{
				fIMAPstate 					 = kAuthenticatedStateURL;
				fUrlType   					 = kTest;
			}
			else if (!XP_STRCASECMP(fUrlidSubString, "select"))
			{
				fIMAPstate 					 = kSelectedStateURL;
				fUrlType   					 = kSelectFolder;
				ParseFolderPath(&fSourceCanonicalFolderPathSubString);
				if (fTokenPlaceHolder && *fTokenPlaceHolder)
					ParseListofMessageIds();
				else
					fListOfMessageIds = "";
			}
			else if (!XP_STRCASECMP(fUrlidSubString, "liteselect"))
			{
				fIMAPstate 					 = kSelectedStateURL;
				fUrlType   					 = kLiteSelectFolder;
				ParseFolderPath(&fSourceCanonicalFolderPathSubString);
			}
			else if (!XP_STRCASECMP(fUrlidSubString, "expunge"))
			{
				fIMAPstate 					 = kSelectedStateURL;
				fUrlType   					 = kExpungeFolder;
				ParseFolderPath(&fSourceCanonicalFolderPathSubString);
				fListOfMessageIds = "";		// no ids to UNDO
			}
			else if (!XP_STRCASECMP(fUrlidSubString, "create"))
			{
				fIMAPstate 					 = kAuthenticatedStateURL;
				fUrlType   					 = kCreateFolder;
				ParseFolderPath(&fSourceCanonicalFolderPathSubString);
			}
			else if (!XP_STRCASECMP(fUrlidSubString, "discoverchildren"))
			{
				fIMAPstate 					 = kAuthenticatedStateURL;
				fUrlType   					 = kDiscoverChildrenUrl;
				ParseFolderPath(&fSourceCanonicalFolderPathSubString);
			}
			else if (!XP_STRCASECMP(fUrlidSubString, "discoverlevelchildren"))
			{
				fIMAPstate 					 = kAuthenticatedStateURL;
				fUrlType   					 = kDiscoverLevelChildrenUrl;
				ParseChildDiscoveryDepth();
				ParseFolderPath(&fSourceCanonicalFolderPathSubString);
			}
			else if (!XP_STRCASECMP(fUrlidSubString, "discoverallboxes"))
			{
				fIMAPstate 					 = kAuthenticatedStateURL;
				fUrlType   					 = kDiscoverAllBoxesUrl;
			}
			else if (!XP_STRCASECMP(fUrlidSubString, "discoverallandsubscribedboxes"))
			{
				fIMAPstate 					 = kAuthenticatedStateURL;
				fUrlType   					 = kDiscoverAllAndSubscribedBoxesUrl;
			}
			else if (!XP_STRCASECMP(fUrlidSubString, "delete"))
			{
				fIMAPstate 					 = kAuthenticatedStateURL;
				fUrlType   					 = kDeleteFolder;
				ParseFolderPath(&fSourceCanonicalFolderPathSubString);
			}
			else if (!XP_STRCASECMP(fUrlidSubString, "rename"))
			{
				fIMAPstate 					 = kAuthenticatedStateURL;
				fUrlType   					 = kRenameFolder;
				ParseFolderPath(&fSourceCanonicalFolderPathSubString);
				ParseFolderPath(&fDestinationCanonicalFolderPathSubString);
			}
			else if (!XP_STRCASECMP(fUrlidSubString, "movefolderhierarchy"))
			{
				fIMAPstate 					 = kAuthenticatedStateURL;
				fUrlType   					 = kMoveFolderHierarchy;
				ParseFolderPath(&fSourceCanonicalFolderPathSubString);
				if (fTokenPlaceHolder && *fTokenPlaceHolder)	// handle promote to root
					ParseFolderPath(&fDestinationCanonicalFolderPathSubString);
			}
			else if (!XP_STRCASECMP(fUrlidSubString, "list"))
			{
				fIMAPstate 					 = kAuthenticatedStateURL;
				fUrlType   					 = kLsubFolders;
				ParseFolderPath(&fDestinationCanonicalFolderPathSubString);
			}
			else if (!XP_STRCASECMP(fUrlidSubString, "biff"))
			{
				fIMAPstate 					 = kSelectedStateURL;
				fUrlType   					 = kBiff;
				ParseFolderPath(&fSourceCanonicalFolderPathSubString);
				ParseListofMessageIds();
			}
			else if (!XP_STRCASECMP(fUrlidSubString, "netscape"))
			{
				fIMAPstate 					 = kAuthenticatedStateURL;
				fUrlType   					 = kGetMailAccountUrl;
			}
			else if (!XP_STRCASECMP(fUrlidSubString, "appendmsgfromfile"))
			{
				fIMAPstate					 = kAuthenticatedStateURL;
				fUrlType					 = kAppendMsgFromFile;
				ParseFolderPath(&fSourceCanonicalFolderPathSubString);
			}
			else if (!XP_STRCASECMP(fUrlidSubString, "appenddraftfromfile"))
			{
				fIMAPstate					 = kSelectedStateURL;
				fUrlType					 = kAppendMsgFromFile;
				ParseFolderPath(&fSourceCanonicalFolderPathSubString);
			}
			else if (!XP_STRCASECMP(fUrlidSubString, "subscribe"))
			{
				fIMAPstate					 = kAuthenticatedStateURL;
				fUrlType					 = kSubscribe;
				ParseFolderPath(&fSourceCanonicalFolderPathSubString);
			}
			else if (!XP_STRCASECMP(fUrlidSubString, "unsubscribe"))
			{
				fIMAPstate					 = kAuthenticatedStateURL;
				fUrlType					 = kUnsubscribe;
				ParseFolderPath(&fSourceCanonicalFolderPathSubString);
			}
			else if (!XP_STRCASECMP(fUrlidSubString, "refreshacl"))
			{
				fIMAPstate					= kAuthenticatedStateURL;
				fUrlType					= kRefreshACL;
				ParseFolderPath(&fSourceCanonicalFolderPathSubString);
			}
			else if (!XP_STRCASECMP(fUrlidSubString, "refreshfolderurls"))
			{
				fIMAPstate					= kAuthenticatedStateURL;
				fUrlType					= kRefreshFolderUrls;
				ParseFolderPath(&fSourceCanonicalFolderPathSubString);
			}
			else if (!XP_STRCASECMP(fUrlidSubString, "refreshallacls"))
			{
				fIMAPstate					= kAuthenticatedStateURL;
				fUrlType					= kRefreshAllACLs;
			}
			else if (!XP_STRCASECMP(fUrlidSubString, "listfolder"))
			{
				fIMAPstate					= kAuthenticatedStateURL;
				fUrlType					= kListFolder;
				ParseFolderPath(&fSourceCanonicalFolderPathSubString);
			}
			else if (!XP_STRCASECMP(fUrlidSubString, "upgradetosubscription"))
			{
				fIMAPstate					= kAuthenticatedStateURL;
				fUrlType					= kUpgradeToSubscription;
				ParseFolderPath(&fSourceCanonicalFolderPathSubString);
			}
			else if (!XP_STRCASECMP(fUrlidSubString, "folderstatus"))
			{
				fIMAPstate					= kAuthenticatedStateURL;
				fUrlType					= kFolderStatus;
				ParseFolderPath(&fSourceCanonicalFolderPathSubString);
			}
			else
				fValidURL = FALSE;	
		}
		else fValidURL = FALSE;
	}
	else
		fValidURL = FALSE;	
}

TIMAPUrl::EUrlIMAPstate TIMAPUrl::GetUrlIMAPstate()
{
	return fIMAPstate;
}

TIMAPUrl::EIMAPurlType TIMAPUrl::GetIMAPurlType()
{
	return fUrlType;
}



char *TIMAPUrl::CreateCanonicalSourceFolderPathString()
{
	return XP_STRDUP(fSourceCanonicalFolderPathSubString ? fSourceCanonicalFolderPathSubString : "");
}

char *TIMAPUrl::CreateCanonicalDestinationFolderPathString()
{
	return XP_STRDUP(fDestinationCanonicalFolderPathSubString);
}

char *TIMAPUrl::CreateServerSourceFolderPathString()
{
	return AllocateServerPath(fSourceCanonicalFolderPathSubString);
}

char *TIMAPUrl::CreateServerDestinationFolderPathString()
{
	// its possible for the destination folder path to be the root
	if (!fDestinationCanonicalFolderPathSubString)
		return XP_STRDUP("");
	else
		return AllocateServerPath(fDestinationCanonicalFolderPathSubString);
}

char *TIMAPUrl::CreateSearchCriteriaString()
{
	return XP_STRDUP(fSearchCriteriaString);
}

char *TIMAPUrl::CreateListOfMessageIdsString()
{
	char *returnIdString = XP_STRDUP(fListOfMessageIds);
	if (returnIdString)
	{
		// mime may have glommed a "&part=" for a part download
		// we return the entire message and let mime extract
		// the part. Pop and news work this way also.
		// this algorithm truncates the "&part" string.
		char *currentChar = returnIdString;
		while (*currentChar && (*currentChar != '&'))
			currentChar++;
		if (*currentChar == '&')
			*currentChar = 0;

		// we should also strip off anything after "/;section="
		// since that can specify an IMAP MIME part
		char *wherepart = XP_STRSTR(returnIdString, "/;section=");
		if (wherepart)
			*wherepart = 0;
	}
	return returnIdString;
}

char *TIMAPUrl::GetIMAPPartToFetch()
{
	char *wherepart = NULL, *rv = NULL;
	if (fListOfMessageIds && (wherepart = XP_STRSTR(fListOfMessageIds, "/;section=")) != NULL)
	{
		wherepart += 10; // XP_STRLEN("/;section=")
		if (wherepart)
		{
			char *wherelibmimepart = XP_STRSTR(wherepart, "&part=");
			int len = XP_STRLEN(fListOfMessageIds), numCharsToCopy = 0;
			if (wherelibmimepart)
				numCharsToCopy = (wherelibmimepart - wherepart);
			else
				numCharsToCopy = XP_STRLEN(fListOfMessageIds) - (wherepart - fListOfMessageIds);
			if (numCharsToCopy)
			{
				rv = (char *) XP_ALLOC(sizeof(char) * (numCharsToCopy + 1));
				if (rv)
					XP_STRNCPY_SAFE(rv, wherepart, numCharsToCopy + 1);	// appends a \0
			}
		}
	}
	return rv;
}


imapMessageFlagsType TIMAPUrl::GetMsgFlags()	// kAddMsgFlags or kSubtractMsgFlags only
{
	return fFlags;
}



XP_Bool TIMAPUrl::ValidIMAPUrl()
{
	return fValidURL;
}

XP_Bool TIMAPUrl::MessageIdsAreUids()
{
	return fIdsAreUids;
}

char *TIMAPUrl::AllocateServerPath(const char *canonicalPath)
{
	XP_ASSERT(GetOnlineSubDirSeparator() != kOnlineHierarchySeparatorUnknown);
	return ReplaceCharsInCopiedString(canonicalPath, '/', GetOnlineSubDirSeparator());
}

char *TIMAPUrl::AllocateCanonicalPath(const char *serverPath)
{	
	char *canonicalPath = ReplaceCharsInCopiedString(serverPath, GetOnlineSubDirSeparator() , '/');
	
	// eat any escape characters for escaped dir separators
	if (canonicalPath)
	{
		char *currentEscapeSequence = XP_STRSTR(canonicalPath, "\\/");
		while (currentEscapeSequence)
		{
			XP_STRCPY(currentEscapeSequence, currentEscapeSequence+1);
			currentEscapeSequence = XP_STRSTR(currentEscapeSequence+1, "\\/");
		}
	}
	
	return canonicalPath;
}

char *TIMAPUrl::ReplaceCharsInCopiedString(const char *stringToCopy, char oldChar, char newChar)
{	
	char oldCharString[2];
	*oldCharString = oldChar;
	*(oldCharString+1) = 0;
	
	char *translatedString = XP_STRDUP(stringToCopy);
	char *currentSeparator = strstr(translatedString, oldCharString);
	
	while(currentSeparator)
	{
		*currentSeparator = newChar;
		currentSeparator = strstr(currentSeparator+1, oldCharString);
	}

	return translatedString;
}

void TIMAPUrl::SetOnlineSubDirSeparator(char onlineDirSeparator)
{
	fOnlineSubDirSeparator = onlineDirSeparator;
}

char TIMAPUrl::GetOnlineSubDirSeparator()
{
	return fOnlineSubDirSeparator;
}

XP_Bool	TIMAPUrl::GetShouldSubscribeToAll()
{
	return (GetOnlineSubDirSeparator() == '.');
}

#if 0
// According to the comment in imap.h, where the prototype lives and has been commented out,
// this is obsolete.  So let's get rid of the "no prototype" warning.
extern "C" {
	void
	IMAP_SetNamespacesFromPrefs(const char *hostName, char *personalPrefix, char *publicPrefixes, char *otherUsersPrefixes)
	{
		TIMAPHostInfo::ClearPrefsNamespacesForHost(hostName);
		if (XP_STRCMP(personalPrefix,""))
		{
			TIMAPNamespace *ns = new TIMAPNamespace(kPersonalNamespace, personalPrefix, '/', TRUE);
			if (ns)
				TIMAPHostInfo::AddNewNamespaceForHost(hostName, ns);
		}
		if (XP_STRCMP(publicPrefixes,""))
		{
			TIMAPNamespace *ns = new TIMAPNamespace(kPublicNamespace, publicPrefixes, '/', TRUE);
			if (ns)
				TIMAPHostInfo::AddNewNamespaceForHost(hostName, ns);
		}
		if (XP_STRCMP(otherUsersPrefixes,""))
		{
			TIMAPNamespace *ns = new TIMAPNamespace(kOtherUsersNamespace, otherUsersPrefixes, '/', TRUE);
			if (ns)
				TIMAPHostInfo::AddNewNamespaceForHost(hostName, ns);
		}
	}
}	// extern "C"
#endif // 0

