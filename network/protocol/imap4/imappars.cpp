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
#include "imap4pvt.h"
#include "prlog.h"
#include "imapbody.h"

extern "C" {
#include "xpgetstr.h"

extern int MK_OUT_OF_MEMORY;
extern int MK_SERVER_DISCONNECTED;
extern int MK_IMAP_DOWNLOADING_MESSAGE;
}

#define WHITESPACE " \015\012"     // token delimiter 

/* 45678901234567890123456789012345678901234567890123456789012345678901234567890
*/ 
TImapFlagAndUidState::TImapFlagAndUidState(int numberOfMessages, XP_Bool bSupportUserFlag)
{
	fNumberOfMessagesAdded = 0;
	fNumberOfMessageSlotsAllocated = numberOfMessages;
	if (!fNumberOfMessageSlotsAllocated)
		fNumberOfMessageSlotsAllocated = kFlagEntrySize;
	fFlags = (imapMessageFlagsType*) XP_ALLOC(sizeof(imapMessageFlagsType) * fNumberOfMessageSlotsAllocated); // new imapMessageFlagsType[fNumberOfMessageSlotsAllocated];
	
	fUids.SetSize(fNumberOfMessageSlotsAllocated);
	XP_MEMSET(fFlags, 0, sizeof(imapMessageFlagsType) * fNumberOfMessageSlotsAllocated);
	fSupportUserFlag = bSupportUserFlag;
	fNumberDeleted = 0;
}

TImapFlagAndUidState::TImapFlagAndUidState(const TImapFlagAndUidState& state, XP_Bool bSupportUserFlag)
{
	fNumberOfMessagesAdded = state.fNumberOfMessagesAdded;
	
	fNumberOfMessageSlotsAllocated = state.fNumberOfMessageSlotsAllocated;
	fFlags = (imapMessageFlagsType*) XP_ALLOC(sizeof(imapMessageFlagsType) * fNumberOfMessageSlotsAllocated); // new imapMessageFlagsType[fNumberOfMessageSlotsAllocated];
	fUids.CopyArray((IDArray*) &state.fUids);

	XP_MEMCPY(fFlags, state.fFlags, sizeof(imapMessageFlagsType) * fNumberOfMessageSlotsAllocated);
	fSupportUserFlag = bSupportUserFlag;
	fNumberDeleted = 0;
}

TImapFlagAndUidState::~TImapFlagAndUidState()
{
	FREEIF(fFlags);
}
	

// we need to reset our flags, (re-read all) but chances are the memory allocation needed will be
// very close to what we were already using

void TImapFlagAndUidState::Reset(uint32 howManyLeft)
{
	if (!howManyLeft)
		fNumberOfMessagesAdded = fNumberDeleted = 0;		// used space is still here
}


// Remove (expunge) a message from our array, since now it is gone for good

void TImapFlagAndUidState::ExpungeByIndex(uint32 index)
{
	uint32 counter = 0;
	
	if ((uint32) fNumberOfMessagesAdded < index)
		return;
	index--;
	fNumberOfMessagesAdded--;
	if (fFlags[index] & kImapMsgDeletedFlag)	// see if we already had counted this one as deleted
		fNumberDeleted--;
	for (counter = index; counter < (uint32) fNumberOfMessagesAdded; counter++)
	{
		fUids.SetAt(counter, fUids[counter + 1]);
		fFlags[counter] = fFlags[counter + 1];
	}
}


	// adds to sorted list.  protects against duplicates and going past fNumberOfMessageSlotsAllocated  
void TImapFlagAndUidState::AddUidFlagPair(imap_uid uid, imapMessageFlagsType flags)
{
	// make sure there is room for this pair
	if (fNumberOfMessagesAdded >= fNumberOfMessageSlotsAllocated)
	{
		fNumberOfMessageSlotsAllocated += kFlagEntrySize;
		fUids.SetSize(fNumberOfMessageSlotsAllocated);
		fFlags = (imapMessageFlagsType*) XP_REALLOC(fFlags, sizeof(imapMessageFlagsType) * fNumberOfMessageSlotsAllocated); // new imapMessageFlagsType[fNumberOfMessageSlotsAllocated];
	}

	// optimize the common case of placing on the end
	if (!fNumberOfMessagesAdded || (uid > (int32) fUids[fNumberOfMessagesAdded - 1]))
	{	
		fUids.SetAt(fNumberOfMessagesAdded, uid);
		fFlags[fNumberOfMessagesAdded] = flags;
		fNumberOfMessagesAdded++;
		if (flags & kImapMsgDeletedFlag)
			fNumberDeleted++;
		return;
	}
	
	// search for the slot for this uid-flag pair

	int32 insertionIndex = -1;
	XP_Bool foundIt = FALSE;

	GetMessageFlagsFromUID(uid, &foundIt, &insertionIndex);

	// Hmmm, is the server sending back unordered fetch responses?
	if (((int32) fUids[insertionIndex]) != uid)
	{
		// shift the uids and flags to the right
		for (int32 shiftIndex = fNumberOfMessagesAdded; shiftIndex > insertionIndex; shiftIndex--)
		{
			fUids.SetAt(shiftIndex, fUids[shiftIndex - 1]);
			fFlags[shiftIndex] = fFlags[shiftIndex - 1];
		}
		fFlags[insertionIndex] = flags;
		fUids.SetAt(insertionIndex, uid);
		fNumberOfMessagesAdded++;
		if (fFlags[insertionIndex] & kImapMsgDeletedFlag)
			fNumberDeleted++;
	} else {
		if ((fFlags[insertionIndex] & kImapMsgDeletedFlag) && !(flags & kImapMsgDeletedFlag))
			fNumberDeleted--;
		else
		if (!(fFlags[insertionIndex] & kImapMsgDeletedFlag) && (flags & kImapMsgDeletedFlag))
			fNumberDeleted++;
		fFlags[insertionIndex] = flags;
	}
}


int TImapFlagAndUidState::GetNumberOfMessages()
{
	return fNumberOfMessagesAdded;
}
	
int TImapFlagAndUidState::GetNumberOfDeletedMessages()
{
	return fNumberDeleted;
}
	
// since the uids are sorted, start from the back (rb)

imap_uid  TImapFlagAndUidState::GetHighestNonDeletedUID()
{
	uint32 index = fNumberOfMessagesAdded;
	do {
		if (index <= 0)
			return(0);
		index--;
		if (fUids[index] && !(fFlags[index] & kImapMsgDeletedFlag))
			return fUids[index];
	} while (index > 0);
	return 0;
}


// Has the user read the last message here ? Used when we first open the inbox to see if there
// really is new mail there.

XP_Bool TImapFlagAndUidState::IsLastMessageUnseen()
{
	uint32 index = fNumberOfMessagesAdded;

	if (index <= 0)
		return FALSE;
	index--;
	// if last message is deleted, it was probably filtered the last time around
	if (fUids[index] && (fFlags[index] & (kImapMsgSeenFlag | kImapMsgDeletedFlag)))
		return FALSE;
	return TRUE; 
}


imap_uid TImapFlagAndUidState::GetUidOfMessage(int zeroBasedIndex)
{
	if (zeroBasedIndex < fNumberOfMessagesAdded)
		return fUids[zeroBasedIndex];
	return (-1);	// so that value is non-zero and we don't ask for bad msgs
}


// find a message flag given a key with non-recursive binary search, since some folders
// may have thousand of messages, once we find the key set its index, or the index of
// where the key should be inserted

imapMessageFlagsType TImapFlagAndUidState::GetMessageFlagsFromUID(imap_uid uid, XP_Bool *foundIt, int32 *ndx)
{
	int index = 0;
	int hi = fNumberOfMessagesAdded - 1;
	int lo = 0;

	*foundIt = FALSE;
	*ndx = -1;
	while (lo <= hi)
	{
		index = (lo + hi) / 2;
		if (fUids[index] == (uint32) uid)
		{
			int returnFlags = fFlags[index];
			
			*foundIt = TRUE;
			*ndx = index;
			if (fSupportUserFlag)
				returnFlags |= kImapMsgSupportUserFlag;
			return returnFlags;
		}
		if (fUids[index] > (uint32) uid)
			hi = index -1;
		else if (fUids[index] < (uint32) uid)
			lo = index + 1;
	}
	index = lo;
	while ((index > 0) && (fUids[index] > (uint32) uid))
		index--;
	if (index < 0)
		index = 0;
	*ndx = index;
	return 0;
}



imapMessageFlagsType TImapFlagAndUidState::GetMessageFlags(int zeroBasedIndex)
{
	imapMessageFlagsType returnFlags = kNoImapMsgFlag;
	if (zeroBasedIndex < fNumberOfMessagesAdded)
		returnFlags = fFlags[zeroBasedIndex];

	if (fSupportUserFlag)
		returnFlags |= kImapMsgSupportUserFlag;

	return returnFlags;
}

extern "C" {
TImapFlagAndUidState *IMAP_CreateFlagState(int numberOfMessages)
{
	return new TImapFlagAndUidState(numberOfMessages);
}


void IMAP_DeleteFlagState(TImapFlagAndUidState *state)
{
	delete state;
}


int IMAP_GetFlagStateNumberOfMessages(TImapFlagAndUidState *state)
{
	return state->GetNumberOfMessages();
}
	

imap_uid IMAP_GetUidOfMessage(int zeroBasedIndex, TImapFlagAndUidState *state)
{
	return state->GetUidOfMessage(zeroBasedIndex);
}


imapMessageFlagsType IMAP_GetMessageFlags(int zeroBasedIndex, TImapFlagAndUidState *state)
{
	return state->GetMessageFlags(zeroBasedIndex);
}


imapMessageFlagsType IMAP_GetMessageFlagsFromUID(imap_uid uid, XP_Bool *foundIt, TImapFlagAndUidState *state)
{
	int32 index = -1;
	return state->GetMessageFlagsFromUID(uid, foundIt, &index);
}


}	// extern "C"

TSearchResultSequence::TSearchResultSequence()
{
	fListOfLines = XP_ListNew();
}

TSearchResultSequence *TSearchResultSequence::CreateSearchResultSequence()
{
	TSearchResultSequence *returnObject = new TSearchResultSequence;
	if (!returnObject ||
		!returnObject->fListOfLines)
	{
		delete returnObject;
		returnObject = nil;
	}
	
	return returnObject;
}

TSearchResultSequence::~TSearchResultSequence()
{
	if (fListOfLines)
		XP_ListDestroy(fListOfLines);
}


void TSearchResultSequence::ResetSequence()
{
	if (fListOfLines)
		XP_ListDestroy(fListOfLines);
	fListOfLines = XP_ListNew();
}

void TSearchResultSequence::AddSearchResultLine(const char *searchLine)
{
	// The first add becomes node 2.  Fix this.
	char *copiedSequence = XP_STRDUP(searchLine + 9); // 9 == "* SEARCH "

	if (copiedSequence)	// if we can't allocate this then the search won't hit
		XP_ListAddObjectToEnd(fListOfLines, copiedSequence);
}


TSearchResultIterator::TSearchResultIterator(TSearchResultSequence &sequence) :
	fSequence(sequence)
{
	ResetIterator();
}

TSearchResultIterator::~TSearchResultIterator()
{}

void  TSearchResultIterator::ResetIterator()
{
	fCurrentLine = fSequence.fListOfLines->next;
	if (fCurrentLine)
		fPositionInCurrentLine = (char *) fCurrentLine->object;
	else
		fPositionInCurrentLine = nil;
}

int32 TSearchResultIterator::GetNextMessageNumber()
{
	int32 returnValue = 0;
	if (fPositionInCurrentLine)
	{	
		returnValue = atoint32(fPositionInCurrentLine);
		
		// eat the current number
		while (isdigit(*++fPositionInCurrentLine))
			;
		
		if (*fPositionInCurrentLine == 0xD)	// found CR, no more digits on line
		{
			fCurrentLine = fCurrentLine->next;
			if (fCurrentLine)
				fPositionInCurrentLine = (char *) fCurrentLine->object;
			else
				fPositionInCurrentLine = nil;
		}
		else	// eat the space
			fPositionInCurrentLine++;
	}
	
	return returnValue;
}



////////////////// TIMAPGenericParser /////////////////////////


TIMAPGenericParser::TIMAPGenericParser() :
	fNextToken(nil),
	fCurrentLine(nil),
	fLineOfTokens(nil),
	fCurrentTokenPlaceHolder(nil),
	fStartOfLineOfTokens(nil),
	fSyntaxErrorLine(nil),
	fAtEndOfLine(FALSE),
	fDisconnected(FALSE),
	fSyntaxError(FALSE),
	fTokenizerAdvanced(FALSE)
{
}

TIMAPGenericParser::~TIMAPGenericParser()
{
	FREEIF( fCurrentLine );
	FREEIF( fStartOfLineOfTokens);
	FREEIF( fSyntaxErrorLine );
}

void TIMAPGenericParser::HandleMemoryFailure()
{
	SetConnected(FALSE);
}

void TIMAPGenericParser::ResetLexAnalyzer()
{
	FREEIF( fCurrentLine );
	FREEIF( fStartOfLineOfTokens );
	fTokenizerAdvanced = FALSE;
	
	fCurrentLine = fNextToken = fLineOfTokens = fStartOfLineOfTokens = fCurrentTokenPlaceHolder = nil;
	fAtEndOfLine = FALSE;
}

XP_Bool TIMAPGenericParser::LastCommandSuccessful()
{
	return Connected() && !SyntaxError();
}

void TIMAPGenericParser::SetSyntaxError(XP_Bool error)
{
	fSyntaxError = error;
	FREEIF( fSyntaxErrorLine );
	if (error)
	{
		XP_ASSERT(FALSE);	// If you hit this assert, file a bug on chrisf.  Promise to include a protocol log.
		fSyntaxErrorLine = XP_STRDUP(fCurrentLine);
		if (!fSyntaxErrorLine)
		{
			HandleMemoryFailure();
//			PR_LOG(IMAP, out, ("PARSER: Internal Syntax Error: <no line>"));
		}
		else
		{
//			if (!XP_STRCMP(fSyntaxErrorLine, CRLF))
//				PR_LOG(IMAP, out, ("PARSER: Internal Syntax Error: <CRLF>"));
//			else
//				PR_LOG(IMAP, out, ("PARSER: Internal Syntax Error: %s", fSyntaxErrorLine));
		}
	}
	else
		fSyntaxErrorLine = NULL;
}

char *TIMAPGenericParser::CreateSyntaxErrorLine()
{
	return XP_STRDUP(fSyntaxErrorLine);
}


XP_Bool TIMAPGenericParser::SyntaxError()
{
	return fSyntaxError;
}

void TIMAPGenericParser::SetConnected(XP_Bool connected)
{
	fDisconnected = !connected;
}

XP_Bool TIMAPGenericParser::Connected()
{
	return !fDisconnected;
}

XP_Bool TIMAPGenericParser::ContinueParse()
{
	return !fSyntaxError && !fDisconnected;
}


XP_Bool TIMAPGenericParser::at_end_of_line()
{
	return XP_STRCMP(fNextToken, CRLF) == 0;
}

void TIMAPGenericParser::skip_to_CRLF()
{
	while (Connected() && !at_end_of_line())
		fNextToken = GetNextToken();
}

// fNextToken initially should point to
// a string after the initial open paren ("(")
// After this call, fNextToken points to the
// first character after the matching close
// paren.  Only call GetNextToken to get the NEXT
// token after the one returned in fNextToken.
void TIMAPGenericParser::skip_to_close_paren()
{
	int numberOfCloseParensNeeded = 1;
	if (fNextToken && *fNextToken == ')')
	{
		numberOfCloseParensNeeded--;
		fNextToken++;
		if (!fNextToken || !*fNextToken)
			fNextToken = GetNextToken();
	}

	while (ContinueParse() && numberOfCloseParensNeeded > 0)
	{
		// go through fNextToken, count the number
		// of open and close parens, to account
		// for nested parens which might come in
		// the response
		char *loc = 0;
		for (loc = fNextToken; loc && *loc; loc++)
		{
			if (*loc == '(')
				numberOfCloseParensNeeded++;
			else if (*loc == ')')
				numberOfCloseParensNeeded--;
			if (numberOfCloseParensNeeded == 0)
			{
				fNextToken = loc + 1;
				if (!fNextToken || !*fNextToken)
					fNextToken = GetNextToken();
				break;	// exit the loop
			}
		}

		if (numberOfCloseParensNeeded > 0)
			fNextToken = GetNextToken();
	}
}

char *TIMAPGenericParser::GetNextToken()
{
	if (!fCurrentLine || fAtEndOfLine)
		AdvanceToNextLine();
	else if (Connected())
	{
		if (fTokenizerAdvanced)
		{
			fNextToken = XP_STRTOK_R(fLineOfTokens, WHITESPACE, &fCurrentTokenPlaceHolder);
			fTokenizerAdvanced = FALSE;
		}
		else
		{
			fNextToken = XP_STRTOK_R(nil, WHITESPACE, &fCurrentTokenPlaceHolder);
		}
		if (!fNextToken)
		{
			fAtEndOfLine = TRUE;
			fNextToken = CRLF;
		}
	}
	
	return fNextToken;
}

void TIMAPGenericParser::AdvanceToNextLine()
{
	FREEIF( fCurrentLine );
	FREEIF( fStartOfLineOfTokens);
	fTokenizerAdvanced = FALSE;
	
	XP_Bool ok = GetNextLineForParser(&fCurrentLine);
	if (!ok)
	{
		SetConnected(FALSE);
		fStartOfLineOfTokens = nil;
		fLineOfTokens = nil;
		fCurrentTokenPlaceHolder = nil;
		fNextToken = CRLF;
	}
	else if (fCurrentLine)	// might be NULL if we are would_block ?
	{
		fStartOfLineOfTokens = XP_STRDUP(fCurrentLine);
		if (fStartOfLineOfTokens)
		{
			fLineOfTokens = fStartOfLineOfTokens;
			fNextToken = XP_STRTOK_R(fLineOfTokens, WHITESPACE, &fCurrentTokenPlaceHolder);
			if (!fNextToken)
			{
				fAtEndOfLine = TRUE;
				fNextToken = CRLF;
			}
			else
				fAtEndOfLine = FALSE;
		}
		else
			HandleMemoryFailure();
	}
	else
		HandleMemoryFailure();
}

void TIMAPGenericParser::AdvanceTokenizerStartingPoint(int32 bytesToAdvance)
{
	FREEIF(fStartOfLineOfTokens);
	if (fCurrentLine)
	{
		fStartOfLineOfTokens = XP_STRDUP(fCurrentLine);
		if (fStartOfLineOfTokens && ((int32) XP_STRLEN(fStartOfLineOfTokens) >= bytesToAdvance))
		{
			fLineOfTokens = fStartOfLineOfTokens + bytesToAdvance;
			fTokenizerAdvanced = TRUE;
		}
		else
			HandleMemoryFailure();
	}
	else
		HandleMemoryFailure();
}

// Lots of things in the IMAP protocol are defined as an "astring."
// An astring is either an atom or a string.
// An atom is just a series of one or more characters such as:  hello
// A string can either be quoted or literal.
// Quoted:  "Test Folder 1"
// Literal: {13}Test Folder 1
// This function leaves us off with fCurrentTokenPlaceHolder immediately after
// the end of the Astring.  Call GetNextToken() to get the token after it.
char *TIMAPGenericParser::CreateAstring()
{
	if (*fNextToken == '{')
	{
		return CreateLiteral();		// literal
	}
	else if (*fNextToken == '"')
	{
		return CreateQuoted();		// quoted
	}
	else
	{
		return CreateAtom();		// atom
	}
}


// Create an atom
// This function does not advance the parser.
// Call GetNextToken() to get the next token after the atom.
char *TIMAPGenericParser::CreateAtom()
{
	char *rv = XP_STRDUP(fNextToken);
	//fNextToken = GetNextToken();
	return (rv);
}

// CreateNilString creates either NIL (reutrns NULL) or a string
// Call with fNextToken pointing to the thing which we think is the nilstring.
// This function leaves us off with fCurrentTokenPlaceHolder immediately after
// the end of the string, if it is a string, or at the NIL.
// Regardless of type, call GetNextToken() to get the token after it.
char *TIMAPGenericParser::CreateNilString()
{
	if (!XP_STRCASECMP(fNextToken, "NIL"))
	{
		//fNextToken = GetNextToken();
		return NULL;
	}
	else
		return CreateString();
}


// Create a string, which can either be quoted or literal,
// but not an atom.
// This function leaves us off with fCurrentTokenPlaceHolder immediately after
// the end of the String.  Call GetNextToken() to get the token after it.
char *TIMAPGenericParser::CreateString()
{
	if (*fNextToken == '{')
	{
		char *rv = CreateLiteral();		// literal
		return (rv);
	}
	else if (*fNextToken == '"')
	{
		char *rv = CreateQuoted();		// quoted
		//fNextToken = GetNextToken();
		return (rv);
	}
	else
	{
		SetSyntaxError(TRUE);
		return NULL;
	}
}


// This function leaves us off with fCurrentTokenPlaceHolder immediately after
// the end of the closing quote.  Call GetNextToken() to get the token after it.
// Note that if the current line ends without the
// closed quote then we have to fetch another line from the server, until
// we find the close quote.
char *TIMAPGenericParser::CreateQuoted(XP_Bool /*skipToEnd*/)
{
	char *currentChar = fCurrentLine + 
					    (fNextToken - fStartOfLineOfTokens)
					    + 1;	// one char past opening '"'

	int  charIndex = 0;
	int  tokenIndex = 0;
	XP_Bool closeQuoteFound = FALSE;
	char *returnString = XP_STRDUP(currentChar);
	
	while (returnString && !closeQuoteFound && ContinueParse())
	{
		if (!*(returnString + charIndex))
		{
			AdvanceToNextLine();
			StrAllocCat(returnString, fCurrentLine);
			charIndex++;
		}
		else if (*(returnString + charIndex) == '"')
		{
			// don't check to see if it was escaped, 
			// that was handled in the next clause
			closeQuoteFound = TRUE;
		}
		else if (*(returnString + charIndex) == '\\')
		{
			// eat the escape character
			XP_STRCPY(returnString + charIndex, returnString + charIndex + 1);
			// whatever the escaped character was, we want it
			charIndex++;
			
			// account for charIndex not reflecting the eat of the escape character
			tokenIndex++;
		}
		else
			charIndex++;
	}
	
	if (closeQuoteFound && returnString)
	{
		*(returnString + charIndex) = 0;
		//if ((charIndex == 0) && skipToEnd)	// it's an empty string.  Why skip to end?
		//	skip_to_CRLF();
		//else if (charIndex == XP_STRLEN(fCurrentLine))	// should we have this?
		//AdvanceToNextLine();
		//else 
		if (charIndex < (int) (XP_STRLEN(fNextToken) - 2))	// -2 because of the start and end quotes
		{
			// the quoted string was fully contained within fNextToken,
			// and there is text after the quote in fNextToken that we
			// still need
			int charDiff = XP_STRLEN(fNextToken) - charIndex - 1;
			fCurrentTokenPlaceHolder -= charDiff;
			if (!XP_STRCMP(fCurrentTokenPlaceHolder, CRLF))
				fAtEndOfLine = TRUE;
		}
		else
		{
			fCurrentTokenPlaceHolder += tokenIndex + charIndex + 2 - XP_STRLEN(fNextToken);
			if (!XP_STRCMP(fCurrentTokenPlaceHolder, CRLF))
				fAtEndOfLine = TRUE;
			/*
			tokenIndex += charIndex;
			fNextToken = currentChar + tokenIndex + 1;
			if (!XP_STRCMP(fNextToken, CRLF))
				fAtEndOfLine = TRUE;
			*/
		}
	}
	
	return returnString;
}


// This function leaves us off with fCurrentTokenPlaceHolder immediately after
// the end of the literal string.  Call GetNextToken() to get the token after it
// the literal string.
char *TIMAPGenericParser::CreateLiteral()
{
	int32 numberOfCharsInMessage = atoint32(fNextToken + 1);
	int32 charsReadSoFar = 0, currentLineLength = 0;
	int32 bytesToCopy = 0;
	
	char *returnString = (char *) XP_ALLOC(numberOfCharsInMessage + 1);
	
	if (returnString)
	{
		*(returnString + numberOfCharsInMessage) = 0; // Null terminate it first

		while (ContinueParse() && (charsReadSoFar < numberOfCharsInMessage))
		{
			AdvanceToNextLine();
			currentLineLength = XP_STRLEN(fCurrentLine);
			bytesToCopy = (currentLineLength > numberOfCharsInMessage - charsReadSoFar ?
						   numberOfCharsInMessage - charsReadSoFar : currentLineLength);
			XP_ASSERT (bytesToCopy);

			if (ContinueParse())
			{
				XP_MEMCPY(returnString + charsReadSoFar, fCurrentLine, bytesToCopy); 
				charsReadSoFar += bytesToCopy;
			}
		}
		
		if (ContinueParse())
		{
			if (bytesToCopy == 0)
			{
				skip_to_CRLF();
				fAtEndOfLine = TRUE;
				//fNextToken = GetNextToken();
			}
			else if (currentLineLength == bytesToCopy)
			{
				fAtEndOfLine = TRUE;
				//AdvanceToNextLine();
			}
			else
			{
				// Move fCurrentTokenPlaceHolder

				//fCurrentTokenPlaceHolder = fStartOfLineOfTokens + bytesToCopy;
				AdvanceTokenizerStartingPoint (bytesToCopy);
				if (!*fCurrentTokenPlaceHolder)	// landed on a token boundary
					fCurrentTokenPlaceHolder++;
				if (!XP_STRCMP(fCurrentTokenPlaceHolder, CRLF))
					fAtEndOfLine = TRUE;

				// The first token on the line might not
				// be at the beginning of the line.  There might be ONLY
				// whitespace before it, in which case, fNextToken
				// will be pointing to the right thing.  Otherwise,
				// we want to advance to the next token.
				/*
				int32 numCharsChecked = 0;
				XP_Bool allWhitespace = TRUE;
				while ((numCharsChecked < bytesToCopy)&& allWhitespace)
				{
					allWhitespace = (XP_STRCHR(WHITESPACE, fCurrentLine[numCharsChecked]) != NULL);
					numCharsChecked++;
				}

				if (!allWhitespace)
				{
					//fNextToken = fCurrentLine + bytesToCopy;
					fNextToken = GetNextToken();
					if (!XP_STRCMP(fNextToken, CRLF))
						fAtEndOfLine = TRUE;
				}
				*/
			}	
		}
	}
	
	return returnString;
}


// Call this to create a buffer containing all characters within
// a given set of parentheses.
// Call this with fNextToken[0]=='(', that is, the open paren
// of the group.
// It will allocate and return all characters up to and including the corresponding
// closing paren, and leave the parser in the right place afterwards.
char *TIMAPGenericParser::CreateParenGroup()
{
	int numOpenParens = 1;
	// count the number of parens in the current token
	int count, tokenLen = XP_STRLEN(fNextToken);
	for (count = 1; (count < tokenLen) && (numOpenParens > 0); count++)
	{
		if (fNextToken[count] == '(')
			numOpenParens++;
		else if (fNextToken[count] == ')')
			numOpenParens--;
	}

	// build up a buffer with the paren group.
	char *buf = NULL;
	if ((numOpenParens > 0) && ContinueParse())
	{
		// First, copy that first token from before
		StrAllocCat(buf, fNextToken);
		StrAllocCat(buf, " ");	// space that got stripped off the token

		// Go through the current line and look for the last close paren.
		// We're not trying to parse it just yet, just separate it out.
		int len = XP_STRLEN(fCurrentTokenPlaceHolder);
		for (count = 0; (count < len) && (numOpenParens > 0); count++)
		{
			if (fCurrentTokenPlaceHolder[count] == '(')
				numOpenParens++;
			else if (fCurrentTokenPlaceHolder[count] == ')')
				numOpenParens--;
		}

		if (count < len)
		{
			// we found the last close paren.
			// Set fNextToken, fCurrentTokenPlaceHolder, etc.
			char oldChar = fCurrentTokenPlaceHolder[count];
			fCurrentTokenPlaceHolder[count] = 0;
			StrAllocCat(buf, fCurrentTokenPlaceHolder);
			fCurrentTokenPlaceHolder[count] = oldChar;
			fCurrentTokenPlaceHolder = fCurrentTokenPlaceHolder + count;
			fNextToken = GetNextToken();
		}
		else
		{
			// there should always be either a space or CRLF after the response, right?
			SetSyntaxError(TRUE);
		}
	}
	else if ((numOpenParens == 0) && ContinueParse())
	{
		// the whole paren group response was a single token
		buf = XP_STRDUP(fNextToken);
	}

	if (numOpenParens < 0)
		SetSyntaxError(TRUE);

	return buf;
}


////////////////// TIMAPServerState /////////////////////////


TImapServerState::TImapServerState(TIMAP4BlockingConnection &imapConnection) :
	TIMAPGenericParser(),
	fServerConnection(imapConnection),
	fCurrentCommandTag(nil),
	fCurrentFolderReadOnly(FALSE),
	fNumberOfExistingMessages(0),
	fNumberOfRecentMessages(0),
	fNumberOfUnseenMessages(0),
	fSizeOfMostRecentMessage(0),
	fTotalDownloadSize(0),
	fIMAPstate(kNonAuthenticated),
	fSelectedMailboxName(nil),
	fFlagState(nil),
	fCurrentLineContainedFlagInfo(FALSE),
	fZeroLengthMessageUidString(nil),
	fReportingErrors(TRUE),
	fLastChunk(FALSE),
	fServerIsNetscape3xServer(FALSE),
	m_shell(NULL)
{
	fSearchResults = TSearchResultSequence::CreateSearchResultSequence();
	fMailAccountUrl = NULL;
	fManageFiltersUrl = NULL;
	fManageListsUrl = NULL;
	fFolderAdminUrl = NULL;
	fNetscapeServerVersionString = NULL;
	fXSenderInfo = NULL;
	fSupportsUserDefinedFlags = FALSE;
	fCapabilityFlag = kCapabilityUndefined; 
	fLastAlert = NULL;
}

TImapServerState::~TImapServerState()
{
	FREEIF( fCurrentCommandTag );
	//delete fFlagState;	// not our object
	delete fSearchResults; 
	FREEIF( fZeroLengthMessageUidString );
	FREEIF( fMailAccountUrl );
	FREEIF( fFolderAdminUrl );
	FREEIF( fNetscapeServerVersionString );
	FREEIF( fXSenderInfo );
	FREEIF( fLastAlert );
	FREEIF( fManageListsUrl );
	FREEIF( fManageFiltersUrl );
}

XP_Bool TImapServerState::LastCommandSuccessful()
{
	return (!CommandFailed() && 
		!fServerConnection.DeathSignalReceived() &&
		TIMAPGenericParser::LastCommandSuccessful());
}

// returns TRUE if things look ok to continue
XP_Bool TImapServerState::GetNextLineForParser(char **nextLine)
{
	XP_Bool rv = TRUE;
	*nextLine = fServerConnection.CreateNewLineFromSocket();
	if (fServerConnection.DeathSignalReceived() || (fServerConnection.GetConnectionStatus() <= 0))
		rv = FALSE;
	// we'd really like to try to silently reconnect, but we shouldn't put this
	// message up just in the interrupt case
	if (fServerConnection.GetConnectionStatus() <= 0 && !fServerConnection.DeathSignalReceived())
		fServerConnection.AlertUserEvent_UsingId(MK_SERVER_DISCONNECTED);
	return rv;
}

// This should probably be in the base class...
void TImapServerState::end_of_line()
{
	// if current commands failed, don't reset the lex analyzer
	// we need the info for the user
	if (!at_end_of_line())
		SetSyntaxError(TRUE);
	else if (fProcessingTaggedResponse && !fCurrentCommandFailed)
		ResetLexAnalyzer();	// no more tokens until we send a command
	else if (!fCurrentCommandFailed)
		fNextToken = GetNextToken();
}

XP_Bool	TImapServerState::CommandFailed()
{
	return fCurrentCommandFailed;
}

void TImapServerState::SetFlagState(TImapFlagAndUidState *state)
{
	fFlagState = state;
}

int32 TImapServerState::SizeOfMostRecentMessage()
{
	return fSizeOfMostRecentMessage;
}

// Call this when adding a pipelined command to the session
void TImapServerState::IncrementNumberOfTaggedResponsesExpected(const char *newExpectedTag)
{
	fNumberOfTaggedResponsesExpected++;
	FREEIF( fCurrentCommandTag );
	fCurrentCommandTag = XP_STRDUP(newExpectedTag);
	if (!fCurrentCommandTag)
		HandleMemoryFailure();
}
/* 
 response        ::= *response_data response_done
*/


void TImapServerState::InitializeState()
{
	fProcessingTaggedResponse = FALSE;
	fCurrentCommandFailed 	  = FALSE;
}

void TImapServerState::ParseIMAPServerResponse(const char *currentCommand)
{

	// Reinitialize the parser
	SetConnected(TRUE);
	SetSyntaxError(FALSE);

	// Reinitialize our state
	InitializeState();
	
	// the default is to not pipeline
	fNumberOfTaggedResponsesExpected = 1;
	int numberOfTaggedResponsesReceived = 0;

	char *copyCurrentCommand = XP_STRDUP(currentCommand);
	if (copyCurrentCommand && !fServerConnection.DeathSignalReceived())
	{
		char *placeInTokenString = NULL;
		char *tagToken           = XP_STRTOK_R(copyCurrentCommand, WHITESPACE,&placeInTokenString);
		char *commandToken       = XP_STRTOK_R(nil, WHITESPACE,&placeInTokenString);
		if (tagToken)
		{
			FREEIF( fCurrentCommandTag );
			fCurrentCommandTag = XP_STRDUP(tagToken);
			if (!fCurrentCommandTag)
				HandleMemoryFailure();
		}
		
		if (commandToken && ContinueParse())
			PreProcessCommandToken(commandToken, currentCommand);
		
		if (ContinueParse())
		{
			// Clears any syntax error lines
			SetSyntaxError(FALSE);
			ResetLexAnalyzer();
			
			do {
				fNextToken = GetNextToken();
				while (ContinueParse() && !XP_STRCMP(fNextToken, "*") )
				{
					response_data();
				}
				
				if (*fNextToken == '+')	// never pipeline APPEND or AUTHENTICATE
				{
					XP_ASSERT((fNumberOfTaggedResponsesExpected - numberOfTaggedResponsesReceived) == 1);
					numberOfTaggedResponsesReceived = fNumberOfTaggedResponsesExpected;
				}
				else
					numberOfTaggedResponsesReceived++;
					
				if (numberOfTaggedResponsesReceived < fNumberOfTaggedResponsesExpected)
				{
					response_tagged();
					fProcessingTaggedResponse = FALSE;
				}
				
			} while (ContinueParse() && (numberOfTaggedResponsesReceived < fNumberOfTaggedResponsesExpected));
			
			// check and see if the server is waiting for more input
			if (*fNextToken == '+')
				fIMAPstate = kWaitingForMoreClientInput;
			else
			{
				if (ContinueParse())
					response_done();
				
				if (ContinueParse() && !CommandFailed())
				{
					// a sucessful command may change the eIMAPstate
					ProcessOkCommand(commandToken);
				}
				else if (CommandFailed())
				{
					// a failed command may change the eIMAPstate
					ProcessBadCommand(commandToken);
					if (fReportingErrors)
						fServerConnection.AlertUserEventFromServer(fCurrentLine);
				}
			}
		}
	}
	else if (!fServerConnection.DeathSignalReceived())
		HandleMemoryFailure();

	FREEIF(copyCurrentCommand);
}

void TImapServerState::HandleMemoryFailure()
{
#ifdef DEBUG_chrisf
	XP_ASSERT(FALSE);
#endif
	fServerConnection.AlertUserEvent(XP_GetString(MK_OUT_OF_MEMORY));
	TIMAPGenericParser::HandleMemoryFailure();
}


// SEARCH is the only command that requires pre-processing for now.
// others will be added here.
void TImapServerState::PreProcessCommandToken(const char *commandToken,
										      const char *currentCommand)
{
	fCurrentCommandIsSingleMessageFetch = FALSE;
	
	if (!XP_STRCASECMP(commandToken, "SEARCH"))
		fSearchResults->ResetSequence();
	else if (!XP_STRCASECMP(commandToken, "SELECT") && currentCommand)
	{
		// the mailbox name must be quoted, so strip the quotes
		const char *openQuote = XP_STRSTR(currentCommand, "\"");
		XP_ASSERT(openQuote);
		
		FREEIF( fSelectedMailboxName);
		fSelectedMailboxName = XP_STRDUP(openQuote + 1);
		if (fSelectedMailboxName)
		{
			// strip the escape chars and the ending quote
			char *currentChar = fSelectedMailboxName;
			while (*currentChar)
			{
				if (*currentChar == '\\')
				{
					XP_STRCPY(currentChar, currentChar+1);
					currentChar++;	// skip what we are escaping
				}
				else if (*currentChar == '\"')
					*currentChar = 0;	// end quote
				else
					currentChar++;
			}
		}
		else
			HandleMemoryFailure();
		
		// we don't want bogus info for this new box
		//delete fFlagState;	// not our object
		//fFlagState = NULL;
	}
	else if (!XP_STRCASECMP(commandToken, "CLOSE"))
	{
		return;	// just for debugging
		// we don't want bogus info outside the selected state
		//delete fFlagState;	// not our object
		//fFlagState = NULL;
	}
	else if (!XP_STRCASECMP(commandToken, "UID"))
	{
	
		char *copyCurrentCommand = XP_STRDUP(currentCommand);
		if (copyCurrentCommand && !fServerConnection.DeathSignalReceived())
		{
			char *placeInTokenString = NULL;
			char *tagToken           = XP_STRTOK_R(copyCurrentCommand, WHITESPACE,&placeInTokenString);
			char *uidToken           = XP_STRTOK_R(nil, WHITESPACE,&placeInTokenString);
			char *fetchToken         = XP_STRTOK_R(nil, WHITESPACE,&placeInTokenString);
			
			if (!XP_STRCASECMP(fetchToken, "FETCH") )
			{
				char *uidStringToken = XP_STRTOK_R(nil, WHITESPACE,&placeInTokenString);
				if (!XP_STRCHR(uidStringToken, ',') && !XP_STRCHR(uidStringToken, ':'))	// , and : are uid delimiters
				{
					fCurrentCommandIsSingleMessageFetch = TRUE;
					fUidOfSingleMessageFetch = atoint32(uidStringToken);
				}
			}
			XP_FREE(copyCurrentCommand);
		}
	}
}

const char *TImapServerState::GetSelectedMailboxName()
{
	return fSelectedMailboxName;
}

TSearchResultIterator *TImapServerState::CreateSearchResultIterator()
{
	return new TSearchResultIterator(*fSearchResults);
}

TImapServerState::eIMAPstate TImapServerState::GetIMAPstate()
{
	return fIMAPstate;
}

void TImapServerState::PreauthSetAuthenticatedState()
{
	fIMAPstate = kAuthenticated;
}

void TImapServerState::ProcessOkCommand(const char *commandToken)
{
	if (!XP_STRCASECMP(commandToken, "LOGIN") ||
		!XP_STRCASECMP(commandToken, "AUTHENTICATE"))
		fIMAPstate = kAuthenticated;
	else if (!XP_STRCASECMP(commandToken, "LOGOUT"))
		fIMAPstate = kNonAuthenticated;
	else if (!XP_STRCASECMP(commandToken, "SELECT") ||
	         !XP_STRCASECMP(commandToken, "EXAMINE"))
		fIMAPstate = kFolderSelected;
	else if (!XP_STRCASECMP(commandToken, "CLOSE"))
		fIMAPstate = kAuthenticated;
	else if ((!XP_STRCASECMP(commandToken, "LIST")) ||
			 (!XP_STRCASECMP(commandToken, "LSUB")))
	{
		//fServerConnection.MailboxDiscoveryFinished();
		// This used to be reporting that we were finished
		// discovering folders for each time we issued a
		// LIST or LSUB.  So if we explicitly listed the
		// INBOX, or Trash, or namespaces, we would get multiple
		// "done" states, even though we hadn't finished.
		// Move this to be called from the connection object
		// itself.
	}
	else if (!XP_STRCASECMP(commandToken, "FETCH"))
	{
		if (fZeroLengthMessageUidString)
		{
			// "Deleting zero length message");
			fServerConnection.Store(fZeroLengthMessageUidString, "+Flags (\\Deleted)", TRUE);
			if (LastCommandSuccessful())
				fServerConnection.Expunge();

			FREEIF( fZeroLengthMessageUidString );
			fZeroLengthMessageUidString = nil;
		}
	}
	if (GetFillingInShell())
	{
		// There is a BODYSTRUCTURE response.  Now let's generate the stream...
		// that is, if we're not doing it already
		if (!m_shell->IsBeingGenerated())
		{
			TNavigatorImapConnection *navCon = fServerConnection.GetNavigatorConnection();
			XP_ASSERT(navCon);	// we should always have this

			m_shell->Generate(navCon ? navCon->GetCurrentUrl()->GetIMAPPartToFetch() : (char *)NULL);

			if ((navCon && navCon->GetPseudoInterrupted())
				|| fServerConnection.DeathSignalReceived())
			{
				// we were pseudointerrupted or interrupted
				if (!m_shell->IsShellCached())
				{
					// if it's not in the cache, then we were (pseudo)interrupted while generating
					// for the first time.  Delete it.
					delete m_shell;
				}
				navCon->PseudoInterrupt(FALSE);
			}
			else if (m_shell->GetIsValid())
			{
				// If we have a valid shell that has not already been cached, then cache it.
				if (!m_shell->IsShellCached())	// cache is responsible for destroying it
				{
//					PR_LOG(IMAP, out, ("BODYSHELL:  Adding shell to cache."));
					TIMAPHostInfo::AddShellToCacheForHost(fServerConnection.GetHostName(), m_shell);
				}
			}
			else
			{
				// The shell isn't valid, so we don't cache it.
				// Therefore, we have to destroy it here.
				delete m_shell;
			}
			m_shell = NULL;
		}
	}
}

void TImapServerState::ProcessBadCommand(const char *commandToken)
{
	if (!XP_STRCASECMP(commandToken, "LOGIN") ||
		!XP_STRCASECMP(commandToken, "AUTHENTICATE"))
		fIMAPstate = kNonAuthenticated;
	else if (!XP_STRCASECMP(commandToken, "LOGOUT"))
		fIMAPstate = kNonAuthenticated;	// ??
	else if (!XP_STRCASECMP(commandToken, "SELECT") ||
	         !XP_STRCASECMP(commandToken, "EXAMINE"))
		fIMAPstate = kAuthenticated;	// nothing selected
	else if (!XP_STRCASECMP(commandToken, "CLOSE"))
		fIMAPstate = kAuthenticated;	// nothing selected
	if (GetFillingInShell())
	{
		if (!m_shell->IsBeingGenerated())
		{
			delete m_shell;
			m_shell = NULL;
		}
	}
}
/*
 response_data   ::= "*" SPACE (resp_cond_state / resp_cond_bye /
                              mailbox_data / message_data / capability_data)
                              CRLF
 
 The RFC1730 grammar spec did not allow one symbol look ahead to determine
 between mailbox_data / message_data so I combined the numeric possibilities
 of mailbox_data and all of message_data into numeric_mailbox_data.
 
 The production implemented here is 

 response_data   ::= "*" SPACE (resp_cond_state / resp_cond_bye /
                              mailbox_data / numeric_mailbox_data / 
                              capability_data)
                              CRLF

Instead of comparing lots of strings and make function calls, try to pre-flight
the possibilities based on the first letter of the token.

*/
void TImapServerState::response_data()
{
	fNextToken = GetNextToken();
	
	if (ContinueParse())
	{
		switch (toupper(fNextToken[0]))
		{
		case 'O':			// OK
			if (toupper(fNextToken[1]) == 'K')
				resp_cond_state();
			else SetSyntaxError(TRUE);
			break;
		case 'N':			// NO
			if (toupper(fNextToken[1]) == 'O')
				resp_cond_state();
			else if (!XP_STRCASECMP(fNextToken, "NAMESPACE"))
				namespace_data();
			else SetSyntaxError(TRUE);
			break;
		case 'B':			// BAD
			if (!XP_STRCASECMP(fNextToken, "BAD"))
				resp_cond_state();
			else if (!XP_STRCASECMP(fNextToken, "BYE"))
				resp_cond_bye();
			else SetSyntaxError(TRUE);
			break;
		case 'F':
			if (!XP_STRCASECMP(fNextToken, "FLAGS"))
				mailbox_data();
			else SetSyntaxError(TRUE);
			break;
		case 'P':
			if (XP_STRCASECMP(fNextToken, "PERMANENTFLAGS"))
				mailbox_data();
			else SetSyntaxError(TRUE);
			break;
		case 'L':
			if (!XP_STRCASECMP(fNextToken, "LIST")  || !XP_STRCASECMP(fNextToken, "LSUB"))
				mailbox_data();
			else SetSyntaxError(TRUE);
			break;
		case 'M':
			if (!XP_STRCASECMP(fNextToken, "MAILBOX"))
				mailbox_data();
			else if (!XP_STRCASECMP(fNextToken, "MYRIGHTS"))
				myrights_data();
			else SetSyntaxError(TRUE);
			break;
		case 'S':
			if (!XP_STRCASECMP(fNextToken, "SEARCH"))
				mailbox_data();
			else if (!XP_STRCASECMP(fNextToken, "STATUS"))
			{
				XP_Bool gotMailboxName = FALSE;
				while (	ContinueParse() &&
						!at_end_of_line() )
				{
					fNextToken = GetNextToken();
					if (!fNextToken)
						break;

					if (!gotMailboxName)	// if we haven't got the mailbox name, get it
					{
						// this couldn't be more bogus, but I don't know how to parse the status response.
						// I need to find the next open parenthesis, and it looks like folder names with
						// parentheses are quoted...but not ones with spaces...
						fCurrentTokenPlaceHolder = XP_STRCHR(fCurrentTokenPlaceHolder, '(');

						gotMailboxName = TRUE;
						continue;
					}

					if (*fNextToken == '(') fNextToken++;
					if (!XP_STRCASECMP(fNextToken, "UIDNEXT"))
					{
						fNextToken = GetNextToken();
						if (fNextToken)
						{
							fCurrentResponseUID = atoint32(fNextToken);
							// if this token ends in ')', then it is the last token
							// else we advance
							if ( *(fNextToken + XP_STRLEN(fNextToken) - 1) == ')')
								fNextToken += XP_STRLEN(fNextToken) - 1;
						}
					}
					else if (!XP_STRCASECMP(fNextToken, "MESSAGES"))
					{
						fNextToken = GetNextToken();
						if (fNextToken)
						{
							fNumberOfExistingMessages = atoint32(fNextToken);
							// if this token ends in ')', then it is the last token
							// else we advance
							if ( *(fNextToken + XP_STRLEN(fNextToken) - 1) == ')')
								fNextToken += XP_STRLEN(fNextToken) - 1;
						}
					}
					else if (!XP_STRCASECMP(fNextToken, "UNSEEN"))
					{
						fNextToken = GetNextToken();
						if (fNextToken)
						{
							fNumberOfUnseenMessages = atoint32(fNextToken);
							// if this token ends in ')', then it is the last token
							// else we advance
							if ( *(fNextToken + XP_STRLEN(fNextToken) - 1) == ')')
								fNextToken += XP_STRLEN(fNextToken) - 1;
						}
					}
					else if (*fNextToken == ')')
						break;
					else if (!at_end_of_line())
						SetSyntaxError(TRUE);
				} 
			} else SetSyntaxError(TRUE);
			break;
		case 'C':
			if (!XP_STRCASECMP(fNextToken, "CAPABILITY"))
				capability_data();
			else SetSyntaxError(TRUE);
			break;
		case 'V':
			if (!XP_STRCASECMP(fNextToken, "VERSION"))
			{
				// figure out the version of the Netscape server here
				FREEIF(fNetscapeServerVersionString);
				fNextToken = GetNextToken();
				if (! fNextToken) 
					SetSyntaxError(TRUE);
				else
				{
					fNetscapeServerVersionString = CreateAstring();
					fNextToken = GetNextToken();
					if (fNetscapeServerVersionString)
					{
						fServerIsNetscape3xServer = (*fNetscapeServerVersionString == '3');
					}
				}
				skip_to_CRLF();
			}
			else SetSyntaxError(TRUE);
			break;
		case 'A':
			if (!XP_STRCASECMP(fNextToken, "ACL"))
			{
				acl_data();
			}
			else if (!XP_STRCASECMP(fNextToken, "ACCOUNT-URL"))
			{
				FREEIF(fMailAccountUrl);
				fNextToken = GetNextToken();
				if (! fNextToken) 
					SetSyntaxError(TRUE);
				else
				{
					fMailAccountUrl = CreateAstring();
					fNextToken = GetNextToken();
				}
			} 
			else SetSyntaxError(TRUE);
			break;
		case 'X':
			if (!XP_STRCASECMP(fNextToken, "XSERVERINFO"))
				xserverinfo_data();
			else if (!XP_STRCASECMP(fNextToken, "XMAILBOXINFO"))
				xmailboxinfo_data();
			else 
				SetSyntaxError(TRUE);
			break;
		default:
			if (IsNumericString(fNextToken))
				numeric_mailbox_data();
			else
				SetSyntaxError(TRUE);
			break;
		}
		
		if (ContinueParse())
		{
			PostProcessEndOfLine();
			end_of_line();
		}
	}
}


void TImapServerState::PostProcessEndOfLine()
{
	// for now we only have to do one thing here
	// a fetch response to a 'uid store' command might return the flags
	// before it returns the uid of the message.  So we need both before
	// we report the new flag info to the front end
	
	// also check and be sure that there was a UID in the current response
	if (fCurrentLineContainedFlagInfo && CurrentResponseUID())
	{
		fCurrentLineContainedFlagInfo = FALSE;
		fServerConnection.NotifyMessageFlags(fSavedFlagInfo, CurrentResponseUID());
	}
}


/*
 mailbox_data    ::=  "FLAGS" SPACE flag_list /
                               "LIST" SPACE mailbox_list /
                               "LSUB" SPACE mailbox_list /
                               "MAILBOX" SPACE text /
                               "SEARCH" [SPACE 1#nz_number] /
                               number SPACE "EXISTS" / number SPACE "RECENT"

This production was changed to accomodate predictive parsing 

 mailbox_data    ::=  "FLAGS" SPACE flag_list /
                               "LIST" SPACE mailbox_list /
                               "LSUB" SPACE mailbox_list /
                               "MAILBOX" SPACE text /
                               "SEARCH" [SPACE 1#nz_number]
*/
void TImapServerState::mailbox_data()
{
	XP_Bool userDefined = FALSE;
	XP_Bool perm = FALSE;
	
	if (!XP_STRCASECMP(fNextToken, "FLAGS")) {
		skip_to_CRLF();
	}
	else if (!XP_STRCASECMP(fNextToken, "LIST"))
	{
		fNextToken = GetNextToken();
		if (ContinueParse())
			mailbox_list(FALSE);
	}
	else if (!XP_STRCASECMP(fNextToken, "LSUB"))
	{
		fNextToken = GetNextToken();
		if (ContinueParse())
			mailbox_list(TRUE);
	}
	else if (!XP_STRCASECMP(fNextToken, "MAILBOX"))
		skip_to_CRLF();
	else if (!XP_STRCASECMP(fNextToken, "SEARCH"))
	{
		fSearchResults->AddSearchResultLine(fCurrentLine);
		fServerConnection.NotifySearchHit(fCurrentLine);
		skip_to_CRLF();
	}
}

/*
 mailbox_list    ::= "(" #("\Marked" / "\Noinferiors" /
                              "\Noselect" / "\Unmarked" / flag_extension) ")"
                              SPACE (<"> QUOTED_CHAR <"> / nil) SPACE mailbox
*/

void TImapServerState::mailbox_list(XP_Bool discoveredFromLsub)
{
	mailbox_spec *boxSpec = (mailbox_spec *)XP_ALLOC(sizeof(mailbox_spec));
	if (!boxSpec)
		HandleMemoryFailure();
	else
	{
		boxSpec->folderSelected = FALSE;
		boxSpec->box_flags = kNoFlags;
		boxSpec->allocatedPathName = NULL;
		boxSpec->hostName = NULL;
		boxSpec->connection = fServerConnection.GetNavigatorConnection();
		boxSpec->flagState = NULL;
		boxSpec->discoveredFromLsub = discoveredFromLsub;
		boxSpec->onlineVerified = TRUE;
		
		XP_Bool endOfFlags = FALSE;
		fNextToken++;	// eat the first "("
		do {
			if (!XP_STRNCASECMP(fNextToken, "\\Marked", 7))
				boxSpec->box_flags |= kMarked;	
			else if (!XP_STRNCASECMP(fNextToken, "\\Unmarked", 9))
				boxSpec->box_flags |= kUnmarked;	
			else if (!XP_STRNCASECMP(fNextToken, "\\Noinferiors", 12))
				boxSpec->box_flags |= kNoinferiors;	
			else if (!XP_STRNCASECMP(fNextToken, "\\Noselect", 9))
				boxSpec->box_flags |= kNoselect;	
			// we ignore flag extensions
			
			endOfFlags = *(fNextToken + XP_STRLEN(fNextToken) - 1) == ')';
			fNextToken = GetNextToken();
		} while (!endOfFlags && ContinueParse());
		
		if (ContinueParse())
		{
			if (*fNextToken == '"')
			{
				fNextToken++;
				if (*fNextToken == '\\')	// handle escaped char
					boxSpec->hierarchySeparator = *(fNextToken + 1);
				else
					boxSpec->hierarchySeparator = *fNextToken;
			}
			else	// likely NIL.  Discovered late in 4.02 that we do not handle literals here (e.g. {10} <10 chars>), although this is almost impossibly unlikely
				boxSpec->hierarchySeparator = kOnlineHierarchySeparatorUnknown;
			fNextToken = GetNextToken();	
			if (ContinueParse())
				mailbox(boxSpec);
		}
	}
}

/* mailbox         ::= "INBOX" / astring
*/
void TImapServerState::mailbox(mailbox_spec *boxSpec)
{
	char *boxname = NULL;
	
	if (!XP_STRCASECMP(fNextToken, "INBOX"))
	{
		boxname = XP_STRDUP("INBOX");
		fNextToken = GetNextToken();
	}
	else 
	{
		boxname = CreateAstring();
		fNextToken = GetNextToken();
	}
	
    if (boxname)
    {
    	// sometimes the uwash server will return a list response with a '/' hanging on the
    	// end.
    	
		/*
		// chrisf:  I think we should "discover" whatever the server tells us, and do any munging in
		// libmsg if it is necessary.
		if (XP_STRLEN(boxname) && *(boxname + XP_STRLEN(boxname) - 1) == TIMAPUrl::GetOnlineSubDirSeparator())
    		*(boxname + XP_STRLEN(boxname) - 1) = '\0';
		*/

		// should the namespace check go before or after the Utf7 conversion?
		TIMAPHostInfo::SetNamespaceHierarchyDelimiterFromMailboxForHost(fServerConnection.GetHostName(), boxname, boxSpec->hierarchySeparator);

		
		TIMAPNamespace *ns = TIMAPHostInfo::GetNamespaceForMailboxForHost(fServerConnection.GetHostName(),boxname);
		if (ns)
		{
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
		}

    	char *convertedName = fServerConnection.CreateUtf7ConvertedString(boxname, FALSE);
    	XP_FREE(boxname);
    	boxname = convertedName;
    }

	if (!boxname)
	{
		if (!fServerConnection.DeathSignalReceived())
			HandleMemoryFailure();
	}
	else
	{
		XP_ASSERT(boxSpec->connection);
		XP_ASSERT(boxSpec->connection->GetCurrentUrl());
		//boxSpec->hostName = NULL;
		//if (boxSpec->connection && boxSpec->connection->GetCurrentUrl())
		boxSpec->allocatedPathName = boxSpec->connection->GetCurrentUrl()->AllocateCanonicalPath(boxname);
		boxSpec->hostName = boxSpec->connection->GetCurrentUrl()->GetUrlHost();
		FREEIF( boxname);
		// storage for the boxSpec is now owned by server connection
		fServerConnection.DiscoverMailboxSpec(boxSpec);
		
		// if this was cancelled by the user,then we sure don't want to
		// send more mailboxes their way
		if (fServerConnection.GetConnectionStatus() < 0)
			SetConnected(FALSE);
	}
}


/*
 message_data    ::= nz_number SPACE ("EXPUNGE" /
                              ("FETCH" SPACE msg_fetch) / msg_obsolete)

was changed to

numeric_mailbox_data ::=  number SPACE "EXISTS" / number SPACE "RECENT"
 / nz_number SPACE ("EXPUNGE" /
                              ("FETCH" SPACE msg_fetch) / msg_obsolete)

*/
void TImapServerState::numeric_mailbox_data()
{
	int32 tokenNumber = atoint32(fNextToken);
	fNextToken = GetNextToken();
	
	if (ContinueParse())
	{
		if (!XP_STRCASECMP(fNextToken, "FETCH"))
		{
			fFetchResponseIndex = tokenNumber;
			fNextToken = GetNextToken();
			if (ContinueParse())
				msg_fetch(); 
		}
		else if (!XP_STRCASECMP(fNextToken, "EXISTS"))
		{
			fNumberOfExistingMessages = tokenNumber;
			fNextToken = GetNextToken();
		}
		else if (!XP_STRCASECMP(fNextToken, "RECENT"))
		{
			fNumberOfRecentMessages = tokenNumber;
			fNextToken = GetNextToken();
		}
		else if (!XP_STRCASECMP(fNextToken, "EXPUNGE"))
		{
			fFlagState->ExpungeByIndex((uint32) tokenNumber);
			skip_to_CRLF();
		}
		else
			msg_obsolete();
	}
}

/*
 msg_fetch       ::= "(" 1#("BODY" SPACE body /
                              "BODYSTRUCTURE" SPACE body /
                              "BODY[" section "]" SPACE nstring /
                              "ENVELOPE" SPACE envelope /
                              "FLAGS" SPACE "(" #(flag / "\Recent") ")" /
                              "INTERNALDATE" SPACE date_time /
                              "RFC822" [".HEADER" / ".TEXT"] SPACE nstring /
                              "RFC822.SIZE" SPACE number /
                              "UID" SPACE uniqueid) ")"

*/

void TImapServerState::msg_fetch()
{
	// we have not seen a uid response or flags for this fetch, yet
	fCurrentResponseUID = 0;
	fCurrentLineContainedFlagInfo = FALSE;

	// show any incremental progress, for instance, for header downloading
	fServerConnection.ShowProgress();

	fNextToken++;	// eat the '(' character
	
	// some of these productions are ignored for now
	while (ContinueParse() && (*fNextToken != ')') )
	{
		if (!XP_STRCASECMP(fNextToken, "FLAGS"))
		{
			if (fCurrentResponseUID == 0)
				fCurrentResponseUID = fFlagState->GetUidOfMessage(fFetchResponseIndex - 1);

			fNextToken = GetNextToken();
			if (ContinueParse())
				flags();
			
			if (ContinueParse())
			{	// eat the closing ')'
				fNextToken++;
				// there may be another ')' to close out
				// msg_fetch.  If there is then don't advance
				if (*fNextToken != ')')
					fNextToken = GetNextToken();
			}
		}
		else if (!XP_STRCASECMP(fNextToken, "UID"))
		{
			fNextToken = GetNextToken();
			if (ContinueParse())
			{
				fCurrentResponseUID = atoint32(fNextToken);
				if (fCurrentResponseUID > fHighestRecordedUID)
					fHighestRecordedUID = fCurrentResponseUID;
				
				// if this token ends in ')', then it is the last token
				// else we advance
				if ( *(fNextToken + XP_STRLEN(fNextToken) - 1) == ')')
					fNextToken += XP_STRLEN(fNextToken) - 1;
				else
					fNextToken = GetNextToken();
			}
		}
		else if (!XP_STRCASECMP(fNextToken, "RFC822") ||
				 !XP_STRCASECMP(fNextToken, "RFC822.HEADER") ||
				 !XP_STRNCASECMP(fNextToken, "BODY[HEADER",11) ||
				 !XP_STRNCASECMP(fNextToken, "BODY[]", 6) ||
				 !XP_STRCASECMP(fNextToken, "RFC822.TEXT") ||
				 (!XP_STRNCASECMP(fNextToken, "BODY[", 5) &&
				  XP_STRSTR(fNextToken, "HEADER"))
				 )
		{
			if (fCurrentResponseUID == 0)
				fCurrentResponseUID = fFlagState->GetUidOfMessage(fFetchResponseIndex - 1);

			if (!XP_STRCASECMP(fNextToken, "RFC822.HEADER") ||
	 			!XP_STRCASECMP(fNextToken, "BODY[HEADER]"))
			{
				// all of this message's headers
				fNextToken = GetNextToken();
				fDownloadingHeaders = TRUE;
				if (ContinueParse())
					msg_fetch_headers(NULL);
			}
			else if (!XP_STRNCASECMP(fNextToken, "BODY[HEADER.FIELDS",19))
			{
				fDownloadingHeaders = TRUE;
				// specific message header fields
				while (ContinueParse() && fNextToken[XP_STRLEN(fNextToken)-1] != ']')
					fNextToken = GetNextToken();
				if (ContinueParse())
				{
					fNextToken = GetNextToken();
					if (ContinueParse())
						msg_fetch_headers(NULL);
				}
			}
			else
			{
				char *whereHeader = XP_STRSTR(fNextToken, "HEADER");
				if (whereHeader)
				{
					char *startPartNum = fNextToken + 5;
					char *partNum = (char *)XP_ALLOC((whereHeader - startPartNum) * sizeof (char));
					if (partNum)
					{
						XP_STRNCPY_SAFE(partNum, startPartNum, (whereHeader - startPartNum));
						if (ContinueParse())
						{
							if (XP_STRSTR(fNextToken, "FIELDS"))
							{
								while (ContinueParse() && fNextToken[XP_STRLEN(fNextToken)-1] != ']')
									fNextToken = GetNextToken();
							}
							if (ContinueParse())
							{
								fNextToken = GetNextToken();
								if (ContinueParse())
									msg_fetch_headers(partNum);
							}
						}
						XP_FREE(partNum);
					}
				}
				else
				{
					fDownloadingHeaders = FALSE;

					XP_Bool chunk = FALSE;
					int32 origin = 0;
					if (!XP_STRNCASECMP(fNextToken, "BODY[]<", 7))
					{
						char *tokenCopy = 0;
						StrAllocCopy(tokenCopy, fNextToken);
						if (tokenCopy)
						{
							char *originString = tokenCopy + 7;	// where the byte number starts
							char *closeBracket = XP_STRCHR(tokenCopy,'>');
							if (closeBracket && originString && *originString)
							{
								*closeBracket = 0;
								origin = atoint32(originString);
								chunk = TRUE;
							}
							XP_FREE(tokenCopy);
						}
					}

					fNextToken = GetNextToken();
					if (ContinueParse())
					{
						msg_fetch_content(chunk, origin, MESSAGE_RFC822);
					}
				}
			}
		}
		else if (!XP_STRCASECMP(fNextToken, "RFC822.SIZE"))
		{
			fNextToken = GetNextToken();
			if (ContinueParse())
			{
				fSizeOfMostRecentMessage = atoint32(fNextToken);
				
				if (fSizeOfMostRecentMessage == 0 && CurrentResponseUID())
				{
					// on no, bogus Netscape 2.0 mail server bug
					char uidString[100];
					sprintf(uidString, "%ld", (long)CurrentResponseUID());
					
					if (!fZeroLengthMessageUidString)
						fZeroLengthMessageUidString = XP_STRDUP(uidString);
					else
					{
						StrAllocCat(fZeroLengthMessageUidString, ",");
						StrAllocCat(fZeroLengthMessageUidString, uidString);
					}
				}
				
				// if this token ends in ')', then it is the last token
				// else we advance
				if ( *(fNextToken + XP_STRLEN(fNextToken) - 1) == ')')
					fNextToken += XP_STRLEN(fNextToken) - 1;
				else
					fNextToken = GetNextToken();
			}
		}
		else if (!XP_STRCASECMP(fNextToken, "XSENDER"))
		{
			FREEIF(fXSenderInfo);
			fNextToken = GetNextToken();
			if (! fNextToken) 
				SetSyntaxError(TRUE);
			else
			{
				fXSenderInfo = CreateAstring(); 
				fNextToken = GetNextToken();
			}
		}
		// I only fetch RFC822 so I should never see these BODY responses
		else if (!XP_STRCASECMP(fNextToken, "BODY"))
			skip_to_CRLF(); // I never ask for this
		else if (!XP_STRCASECMP(fNextToken, "BODYSTRUCTURE"))
		{
			bodystructure_data();
		}
		else if (!XP_STRNCASECMP(fNextToken, "BODY[", 5) && XP_STRNCASECMP(fNextToken, "BODY[]", 6))
		{
			fDownloadingHeaders = FALSE;
			// A specific MIME part, or MIME part header
			mime_data();
		}
		else if (!XP_STRCASECMP(fNextToken, "ENVELOPE"))
			skip_to_CRLF(); // I never ask for this
		else if (!XP_STRCASECMP(fNextToken, "INTERNALDATE"))
			skip_to_CRLF(); // I will need to implement this
		else
			SetSyntaxError(TRUE);

	}
	
	if (ContinueParse())
	{
		if (CurrentResponseUID() && fCurrentLineContainedFlagInfo && fFlagState)
		{
			fFlagState->AddUidFlagPair(CurrentResponseUID(), fSavedFlagInfo);
			fCurrentLineContainedFlagInfo = FALSE;	// do not fire if in PostProcessEndOfLine
		}
		fCurrentLineContainedFlagInfo = FALSE;	// do not fire if in PostProcessEndOfLine
			
		fNextToken = GetNextToken();	// eat the ')' ending token
										// should be at end of line
	}
}

void TImapServerState::flags()
{
	imapMessageFlagsType messageFlags = kNoImapMsgFlag;
	
	// eat the opening '('
	fNextToken++;
						     
	while (ContinueParse() && (*fNextToken != ')'))
	{
		switch (toupper(fNextToken[1])) {
		case 'S':
			if (!XP_STRNCASECMP(fNextToken, "\\Seen",5))
				messageFlags |= kImapMsgSeenFlag;
			break;
		case 'A':
			if (!XP_STRNCASECMP(fNextToken, "\\Answered",9))
				messageFlags |= kImapMsgAnsweredFlag;
			break;
		case 'F':
			if (!XP_STRNCASECMP(fNextToken, "\\Flagged",8))
				messageFlags |= kImapMsgFlaggedFlag;
			else if (fSupportsUserDefinedFlags && !XP_STRNCASECMP(fNextToken, "\\Forwarded",10))
				messageFlags |= kImapMsgForwardedFlag;
			break;
		case 'D':
			if (!XP_STRNCASECMP(fNextToken, "\\Deleted",8))
				messageFlags |= kImapMsgDeletedFlag;
			else if (!XP_STRNCASECMP(fNextToken, "\\Draft",6))
				messageFlags |= kImapMsgDraftFlag;
			break;
		case 'R':
			if (!XP_STRNCASECMP(fNextToken, "\\Recent",7))
				messageFlags |= kImapMsgRecentFlag;
			break;
		case 'M':
			if (fSupportsUserDefinedFlags && !XP_STRNCASECMP(fNextToken, "\\MDNSent",7))
				messageFlags |= kImapMsgMDNSentFlag;
			break;
		default:
			break;
		}
			
		if (strcasestr(fNextToken, ")"))
		{
			// eat token chars until we get the ')'
			while (*fNextToken != ')')
				fNextToken++;
		}
		else
			fNextToken = GetNextToken();
	}
	
	if (ContinueParse())
		while(*fNextToken != ')')
			fNextToken++;
	
	fCurrentLineContainedFlagInfo = TRUE;	// handled in PostProcessEndOfLine
	fSavedFlagInfo = messageFlags;
}
	
/* 
 resp_cond_state ::= ("OK" / "NO" / "BAD") SPACE resp_text
*/
void TImapServerState::resp_cond_state()
{
	if ((!XP_STRCASECMP(fNextToken, "NO") ||
	     !XP_STRCASECMP(fNextToken, "BAD") ) &&
	    	fProcessingTaggedResponse)
		fCurrentCommandFailed = TRUE;
	
	fNextToken = GetNextToken();
	if (ContinueParse())
		resp_text();
}

/*
 resp_text       ::= ["[" resp_text_code "]" SPACE] (text_mime2 / text)
 
 was changed to in order to enable a one symbol look ahead predictive
 parser.
 
 resp_text       ::= ["[" resp_text_code  SPACE] (text_mime2 / text)
 */
void TImapServerState::resp_text()
{
	if (ContinueParse() && (*fNextToken == '['))
		resp_text_code();
		
	if (ContinueParse())
	{
		if (!XP_STRCMP(fNextToken, "=?"))
			text_mime2();
		else
			text();
	}
}
/*
 text_mime2       ::= "=?" <charset> "?" <encoding> "?"
                               <encoded-text> "?="
                               ;; Syntax defined in [MIME-2]
*/
void TImapServerState::text_mime2()
{
	skip_to_CRLF();
}

/*
 text            ::= 1*TEXT_CHAR

*/
void TImapServerState::text()
{
	skip_to_CRLF();
}


/*
 resp_text_code  ::= "ALERT" / "PARSE" /
                              "PERMANENTFLAGS" SPACE "(" #(flag / "\*") ")" /
                              "READ-ONLY" / "READ-WRITE" / "TRYCREATE" /
                              "UIDVALIDITY" SPACE nz_number /
                              "UNSEEN" SPACE nz_number /
                              atom [SPACE 1*<any TEXT_CHAR except "]">]
                              
 was changed to in order to enable a one symbol look ahead predictive
 parser.
 
  resp_text_code  ::= ("ALERT" / "PARSE" /
                              "PERMANENTFLAGS" SPACE "(" #(flag / "\*") ")" /
                              "READ-ONLY" / "READ-WRITE" / "TRYCREATE" /
                              "UIDVALIDITY" SPACE nz_number /
                              "UNSEEN" SPACE nz_number /
                              atom [SPACE 1*<any TEXT_CHAR except "]">] )
                      "]"

 
*/
void TImapServerState::resp_text_code()
{
	// this is a special case way of advancing the token
	// strtok won't break up "[ALERT]" into separate tokens
	if (XP_STRLEN(fNextToken) > 1)
		fNextToken++;
	else 
		fNextToken = GetNextToken();
	
	if (ContinueParse())
	{
		if (!XP_STRCASECMP(fNextToken,"ALERT]"))
		{
			char *alertMsg = fCurrentTokenPlaceHolder;	// advance past ALERT]
			if (alertMsg && *alertMsg && (!fLastAlert || XP_STRCMP(fNextToken, fLastAlert)))
			{
				fServerConnection.AlertUserEvent(alertMsg);
				FREEIF(fLastAlert);
				fLastAlert = XP_STRDUP(alertMsg);
			}
			fNextToken = GetNextToken();
		}
		else if (!XP_STRCASECMP(fNextToken,"PARSE]"))
		{
			// do nothing for now
			fNextToken = GetNextToken();
		}
		else if (!XP_STRCASECMP(fNextToken,"NETSCAPE]"))
		{
			skip_to_CRLF();
		}
		else if (!XP_STRCASECMP(fNextToken,"PERMANENTFLAGS"))
		{
			// do nothing but eat tokens until we see the ] or CRLF
			// we should see the ] but we don't want to go into an
			// endless loop if the CRLF is not there
#if 0
			do {
				fNextToken = GetNextToken();
			} while (!strcasestr(fNextToken, "]") && 
					 !at_end_of_line() &&
					 ContinueParse());
#else
			fSupportsUserDefinedFlags = FALSE;		// assume no unless told
			do {
				fNextToken = GetNextToken();
				if (!XP_STRNCASECMP(fNextToken, "\\*)]", 4))
					 fSupportsUserDefinedFlags = TRUE;
			} while (!at_end_of_line() && ContinueParse());
#endif
		}
		else if (!XP_STRCASECMP(fNextToken,"READ-ONLY]"))
		{
			fCurrentFolderReadOnly = TRUE;
			fNextToken = GetNextToken();
		}
		else if (!XP_STRCASECMP(fNextToken,"READ-WRITE]"))
		{
			fCurrentFolderReadOnly = FALSE;
			fNextToken = GetNextToken();
		}
		else if (!XP_STRCASECMP(fNextToken,"TRYCREATE]"))
		{
			// do nothing for now
			fNextToken = GetNextToken();
		}
		else if (!XP_STRCASECMP(fNextToken,"UIDVALIDITY"))
		{
			fNextToken = GetNextToken();
			if (ContinueParse())
			{
				fFolderUIDValidity = atoint32(fNextToken);
				fHighestRecordedUID = 0;
				fNextToken = GetNextToken();
			}
		}
		else if (!XP_STRCASECMP(fNextToken,"UNSEEN"))
		{
			fNextToken = GetNextToken();
			if (ContinueParse())
			{
				fNumberOfUnseenMessages = atoint32(fNextToken);
				fNextToken = GetNextToken();
			}
		}
		else 	// just text
		{
			// do nothing but eat tokens until we see the ] or CRLF
			// we should see the ] but we don't want to go into an
			// endless loop if the CRLF is not there
			do {
				fNextToken = GetNextToken();
			} while (!strcasestr(fNextToken, "]") && 
			         !at_end_of_line() &&
			         ContinueParse());
		}
	}
}

/*
 response_done   ::= response_tagged / response_fatal
 */
void TImapServerState::response_done()
{
	if (ContinueParse())
	{
		if (!XP_STRCMP(fCurrentCommandTag, fNextToken))
			response_tagged();
		else
			response_fatal();
	}
}

/*  response_tagged ::= tag SPACE resp_cond_state CRLF
*/
void TImapServerState::response_tagged()
{
	// eat the tag
	fNextToken = GetNextToken();
	if (ContinueParse())
	{
		fProcessingTaggedResponse = TRUE;
		resp_cond_state();
		if (ContinueParse())
			end_of_line();
	}
}

/* response_fatal  ::= "*" SPACE resp_cond_bye CRLF
*/
void TImapServerState::response_fatal()
{
	// eat the "*"
	fNextToken = GetNextToken();
	if (ContinueParse())
	{
		resp_cond_bye();
		if (ContinueParse())
			end_of_line();
	}
}
/*
resp_cond_bye   ::= "BYE" SPACE resp_text
                              ;; Server will disconnect condition
                    */
void TImapServerState::resp_cond_bye()
{
	SetConnected(FALSE);
	fIMAPstate = kNonAuthenticated;
	skip_to_CRLF();
}


void TImapServerState::msg_fetch_headers(const char *partNum)
{
	if (GetFillingInShell())
	{
		char *headerData = CreateAstring();
		fNextToken = GetNextToken();
		m_shell->AdoptMessageHeaders(headerData, partNum);
	}
	else
	{
		msg_fetch_content(FALSE, 0, MESSAGE_RFC822);
	}
}


/* nstring         ::= string / nil
string          ::= quoted / literal
 nil             ::= "NIL"

*/
void TImapServerState::msg_fetch_content(XP_Bool chunk, int32 origin, const char *content_type)
{
	// setup the stream for downloading this message.
	// Don't do it if we are filling in a shell or downloading a part.
	// DO do it if we are downloading a whole message as a result of
	// an invalid shell trying to generate.
	if ((!chunk || (origin == 0)) && 
		(GetFillingInShell() ? m_shell->GetGeneratingWholeMessage() : TRUE))
	{
		XP_ASSERT(fSizeOfMostRecentMessage > 0);
		fServerConnection.BeginMessageDownLoad(fSizeOfMostRecentMessage, 
										   content_type);
	}

	if (XP_STRCASECMP(fNextToken, "NIL"))
	{
		if (*fNextToken == '"')
			fLastChunk = msg_fetch_quoted(chunk, origin);
		else
			fLastChunk = msg_fetch_literal(chunk, origin);
	}
	else
		fNextToken = GetNextToken();	// eat "NIL"
	
	if (fLastChunk && (GetFillingInShell() ? m_shell->GetGeneratingWholeMessage() : TRUE))
	{
		// complete the message download
		if (ContinueParse())
			fServerConnection.NormalMessageEndDownload();
		else
			fServerConnection.AbortMessageDownLoad();
	}
}


/*
 quoted          ::= <"> *QUOTED_CHAR <">

          QUOTED_CHAR     ::= <any TEXT_CHAR except quoted_specials> /
                              "\" quoted_specials

          quoted_specials ::= <"> / "\"
*/
// This algorithm seems gross.  Isn't there an easier way?
XP_Bool TImapServerState::msg_fetch_quoted(XP_Bool chunk, int32 origin)
{
	uint32 numberOfCharsInThisChunk = 0;
	XP_Bool closeQuoteFound = FALSE;
	XP_Bool lastChunk = TRUE;	// whether or not this is the last chunk of this message
	char *scanLine = fCurrentLine++;	// eat the first "
	// this won't work!  The '"' char does not start
	// the line!  why would anybody format a message like this!
	while (!closeQuoteFound && ContinueParse())
	{
		char *quoteSubString = strcasestr(scanLine, "\"");
		while (quoteSubString && !closeQuoteFound)
		{
			if (quoteSubString > scanLine)
			{
				closeQuoteFound = *(quoteSubString - 1) != '\\';
				if (!closeQuoteFound)
					quoteSubString = strcasestr(quoteSubString+1, "\"");
			}
			else
				closeQuoteFound = TRUE;	// 1st char is a '"'
		}
		
		// send the string to the connection object so he can display or
		// cache or whatever
		fServerConnection.HandleMessageDownLoadLine(fCurrentLine, FALSE);
		numberOfCharsInThisChunk += XP_STRLEN(fCurrentLine);
		AdvanceToNextLine();
		scanLine = fCurrentLine;
	}

	lastChunk = !chunk || ((origin + numberOfCharsInThisChunk) >= (unsigned int) fTotalDownloadSize);
	return lastChunk;
}
/* msg_obsolete    ::= "COPY" / ("STORE" SPACE msg_fetch)
                              ;; OBSOLETE untagged data responses */
void TImapServerState::msg_obsolete()
{
	if (!XP_STRCASECMP(fNextToken, "COPY"))
		fNextToken = GetNextToken();
	else if (!XP_STRCASECMP(fNextToken, "STORE"))
	{
		fNextToken = GetNextToken();
		if (ContinueParse())
			msg_fetch();
	}
	else
		SetSyntaxError(TRUE);
	
}
void TImapServerState::capability_data()
{
	fCapabilityFlag = fCapabilityFlag | kCapabilityDefined;
	do {
		fNextToken = GetNextToken();
		// for now we only care about AUTH=LOGIN
		if (fNextToken) {
			if(! XP_STRCASECMP(fNextToken, "AUTH=LOGIN"))
				fCapabilityFlag |= kHasAuthLoginCapability;
			else if (! XP_STRCASECMP(fNextToken, "X-NETSCAPE"))
				fCapabilityFlag |= kHasXNetscapeCapability;
			else if (! XP_STRCASECMP(fNextToken, "XSENDER"))
				fCapabilityFlag |= kHasXSenderCapability;
			else if (! XP_STRCASECMP(fNextToken, "IMAP4"))
				fCapabilityFlag |= kIMAP4Capability;
			else if (! XP_STRCASECMP(fNextToken, "IMAP4rev1"))
				fCapabilityFlag |= kIMAP4rev1Capability;
			else if (! XP_STRNCASECMP(fNextToken, "IMAP4", 5))
				fCapabilityFlag |= kIMAP4other;
			else if (! XP_STRCASECMP(fNextToken, "X-NO-ATOMIC-RENAME"))
				fCapabilityFlag |= kNoHierarchyRename;
			else if (! XP_STRCASECMP(fNextToken, "X-NON-HIERARCHICAL-RENAME"))
				fCapabilityFlag |= kNoHierarchyRename;
			else if (! XP_STRCASECMP(fNextToken, "NAMESPACE"))
				fCapabilityFlag |= kNamespaceCapability;
			else if (! XP_STRCASECMP(fNextToken, "MAILBOXDATA"))
				fCapabilityFlag |= kMailboxDataCapability;
			else if (! XP_STRCASECMP(fNextToken, "ACL"))
				fCapabilityFlag |= kACLCapability;
			else if (! XP_STRCASECMP(fNextToken, "XSERVERINFO"))
				fCapabilityFlag |= kXServerInfoCapability;
		}
	} while (fNextToken && 
			 !at_end_of_line() &&
			 ContinueParse());

	TIMAPHostInfo::SetCapabilityForHost(fServerConnection.GetHostName(), fCapabilityFlag);
	TNavigatorImapConnection *navCon = fServerConnection.GetNavigatorConnection();
	XP_ASSERT(navCon);	// we should always have this
	if (navCon)
		navCon->CommitCapabilityForHostEvent();
	skip_to_CRLF();
}

void TImapServerState::xmailboxinfo_data()
{
	fNextToken = GetNextToken();
	if (!fNextToken)
		return;

	char *mailboxName = CreateAstring(); // XP_STRDUP(fNextToken);
	if (mailboxName)
	{
		do 
		{
			fNextToken = GetNextToken();
			if (fNextToken) 
			{
				if (!XP_STRCMP("MANAGEURL", fNextToken))
				{
					fNextToken = GetNextToken();
					fFolderAdminUrl = CreateAstring();
				}
				else if (!XP_STRCMP("POSTURL", fNextToken))
				{
					fNextToken = GetNextToken();
					// ignore this for now...
				}
			}
		} while (fNextToken && !at_end_of_line() && ContinueParse());
	}
}

void TImapServerState::xserverinfo_data()
{
	do 
	{
		fNextToken = GetNextToken();
		if (!fNextToken)
			break;
		if (!XP_STRCMP("MANAGEACCOUNTURL", fNextToken))
		{
			fNextToken = GetNextToken();
			fMailAccountUrl = CreateAstring();
		}
		else if (!XP_STRCMP("MANAGELISTSURL", fNextToken))
		{
			fNextToken = GetNextToken();
			fManageListsUrl = CreateAstring();
		}
		else if (!XP_STRCMP("MANAGEFILTERSURL", fNextToken))
		{
			fNextToken = GetNextToken();
			fManageFiltersUrl = CreateAstring();
		}
	} while (fNextToken && !at_end_of_line() && ContinueParse());
}


void TImapServerState::namespace_data()
{
	// flush the old one and create a new one, initialized with no namespaces
	TIMAPHostInfo::ClearServerAdvertisedNamespacesForHost(fServerConnection.GetHostName());
	EIMAPNamespaceType namespaceType = kPersonalNamespace;
	while ((namespaceType != kUnknownNamespace) && ContinueParse())
	{
		fNextToken = GetNextToken();
		while (at_end_of_line() && ContinueParse())
			fNextToken = GetNextToken();
		if (!XP_STRCASECMP(fNextToken,"NIL"))
		{
			// No namespace for this type.
			// Don't add anything to the Namespace object.
		}
		else if (fNextToken[0] == '(')
		{
			// There may be multiple namespaces of the same type.
			// Go through each of them and add them to our Namespace object.

			fNextToken++;
			while (fNextToken[0] == '(' && ContinueParse())
			{
				// we have another namespace for this namespace type
				fNextToken++;
				if (fNextToken[0] != '"')
				{
					SetSyntaxError(TRUE);
				}
				else
				{
					char *namespacePrefix = CreateQuoted(FALSE);

					fNextToken = GetNextToken();
					char *quotedDelimiter = fNextToken;
					char namespaceDelimiter = '\0';

					if (quotedDelimiter[0] == '"')
					{
						quotedDelimiter++;
						namespaceDelimiter = quotedDelimiter[0];
					}
					else if (!XP_STRNCASECMP(quotedDelimiter, "NIL", 3))
					{
						// NIL hierarchy delimiter.  Leave namespace delimiter NULL.
					}
					else
					{
						// not quoted or NIL.
						SetSyntaxError(TRUE);
					}
					if (ContinueParse())
					{
						TIMAPNamespace *newNamespace = new TIMAPNamespace(namespaceType, namespacePrefix, namespaceDelimiter, FALSE);
						if (newNamespace)
							TIMAPHostInfo::AddNewNamespaceForHost(fServerConnection.GetHostName(), newNamespace);

						skip_to_close_paren();	// Ignore any extension data
	
						XP_Bool endOfThisNamespaceType = (fNextToken[0] == ')');
						if (!endOfThisNamespaceType && fNextToken[0] != '(')	// no space between namespaces of the same type
						{
							SetSyntaxError(TRUE);
						}
					}
				}
			}
		}
		else
		{
			SetSyntaxError(TRUE);
		}
		switch (namespaceType)
		{
		case kPersonalNamespace:
			namespaceType = kOtherUsersNamespace;
			break;
		case kOtherUsersNamespace:
			namespaceType = kPublicNamespace;
			break;
		default:
			namespaceType = kUnknownNamespace;
			break;
		}
	}
	if (ContinueParse())
	{
		TNavigatorImapConnection *navCon = fServerConnection.GetNavigatorConnection();
		XP_ASSERT(navCon);	// we should always have this
		if (navCon)
			navCon->CommitNamespacesForHostEvent();
	}
	skip_to_CRLF();
}

void TImapServerState::myrights_data()
{
	fNextToken = GetNextToken();
	if (ContinueParse() && !at_end_of_line())
	{
		char *mailboxName = CreateAstring(); // XP_STRDUP(fNextToken);
		if (mailboxName)
		{
			fNextToken = GetNextToken();
			if (ContinueParse())
			{
				char *myrights = CreateAstring(); // XP_STRDUP(fNextToken);
				if (myrights)
				{
					TNavigatorImapConnection *navCon = fServerConnection.GetNavigatorConnection();
					XP_ASSERT(navCon);	// we should always have this
					if (navCon)
						navCon->AddFolderRightsForUser(mailboxName, NULL /* means "me" */, myrights);
					XP_FREE(myrights);
				}
				else
				{
					HandleMemoryFailure();
				}
				if (ContinueParse())
					fNextToken = GetNextToken();
			}
			XP_FREE(mailboxName);
		}
		else
		{
			HandleMemoryFailure();
		}
	}
	else
	{
		SetSyntaxError(TRUE);
	}
}

void TImapServerState::acl_data()
{
	fNextToken = GetNextToken();
	if (ContinueParse() && !at_end_of_line())
	{
		char *mailboxName = CreateAstring();	// XP_STRDUP(fNextToken);
		if (mailboxName && ContinueParse())
		{
			fNextToken = GetNextToken();
			while (ContinueParse() && !at_end_of_line())
			{
				char *userName = CreateAstring(); // XP_STRDUP(fNextToken);
				if (userName && ContinueParse())
				{
					fNextToken = GetNextToken();
					if (ContinueParse())
					{
						char *rights = CreateAstring(); // XP_STRDUP(fNextToken);
						if (rights)
						{
							TNavigatorImapConnection *navCon = fServerConnection.GetNavigatorConnection();
							XP_ASSERT(navCon);	// we should always have this
							if (navCon)
								navCon->AddFolderRightsForUser(mailboxName, userName, rights);
							XP_FREE(rights);
						}
						else
							HandleMemoryFailure();

						if (ContinueParse())
							fNextToken = GetNextToken();
					}
					XP_FREE(userName);
				}
				else
					HandleMemoryFailure();
			}
		}
		else
			HandleMemoryFailure();
	}
}


void TImapServerState::mime_data()
{
	if (XP_STRSTR(fNextToken, "MIME"))
		mime_header_data();
	else
		mime_part_data();
}

// mime_header_data should not be streamed out;  rather, it should be
// buffered in the TIMAPBodyShell.
// This is because we are still in the process of generating enough
// information from the server (such as the MIME header's size) so that
// we can construct the final output stream.
void TImapServerState::mime_header_data()
{
	char *partNumber = XP_STRDUP(fNextToken);
	if (partNumber)
	{
		char *start = partNumber+5, *end = partNumber+5;	// 5 == XP_STRLEN("BODY[")
		while (ContinueParse() && end && *end != 'M' && *end != 'm')
		{
			end++;
		}
		if (end && (*end == 'M' || *end == 'm'))
		{
			*(end-1) = 0;
			fNextToken = GetNextToken();
			char *mimeHeaderData = CreateAstring();	// is it really this simple?
			fNextToken = GetNextToken();
			if (m_shell)
			{
				m_shell->AdoptMimeHeader(start, mimeHeaderData);
			}
		}
		else
		{
			SetSyntaxError(TRUE);
		}
		XP_FREE(partNumber);	// partNumber is not adopted by the body shell.
	}
	else
	{
		HandleMemoryFailure();
	}
}

// Actual mime parts are filled in on demand (either from shell generation
// or from explicit user download), so we need to stream these out.
void TImapServerState::mime_part_data()
{
	char *checkOriginToken = XP_STRDUP(fNextToken);
	if (checkOriginToken)
	{
		uint32 origin = 0;
		XP_Bool originFound = FALSE;
		char *whereStart = XP_STRCHR(checkOriginToken, '<');
		if (whereStart)
		{
			char *whereEnd = XP_STRCHR(whereStart, '>');
			if (whereEnd)
			{
				*whereEnd = 0;
				whereStart++;
				origin = atoint32(whereStart);
				originFound = TRUE;
			}
		}
		XP_FREE(checkOriginToken);
		fNextToken = GetNextToken();
		msg_fetch_content(originFound, origin, MESSAGE_RFC822);	// keep content type as message/rfc822, even though the
																// MIME part might not be, because then libmime will
																// still handle and decode it.
	}
	else
		HandleMemoryFailure();
}

// FETCH BODYSTRUCTURE parser
// After exit, set fNextToken and fCurrentLine to the right things
void TImapServerState::bodystructure_data()
{
	fNextToken = GetNextToken();

	// separate it out first
	if (fNextToken && *fNextToken == '(')	// It has to start with an open paren.
	{
		char *buf = CreateParenGroup();

		if (ContinueParse())
		{
			if (!buf)
				HandleMemoryFailure();
			else
			{
				// Looks like we have what might be a valid BODYSTRUCTURE response.
				// Try building the shell from it here.
				TNavigatorImapConnection *navCon = fServerConnection.GetNavigatorConnection();
				XP_ASSERT(navCon);
				m_shell = new TIMAPBodyShell(navCon, buf, CurrentResponseUID(), GetSelectedMailboxName());
				/*
				if (m_shell)
				{
					if (!m_shell->GetIsValid())
					{
						SetSyntaxError(TRUE);
					}
				}
				*/
				XP_FREE(buf);
			}
		}
	}
	else
		SetSyntaxError(TRUE);
}

XP_Bool TImapServerState::GetFillingInShell()
{
	return (m_shell != NULL);
}

// Tells the server state parser to use a previously cached shell.
void	TImapServerState::UseCachedShell(TIMAPBodyShell *cachedShell)
{
	// We shouldn't already have another shell we're dealing with.
	if (m_shell && cachedShell)
	{
//		PR_LOG(IMAP, out, ("PARSER: Shell Collision"));
		XP_ASSERT(FALSE);
	}
	m_shell = cachedShell;
}


void TImapServerState::ResetCapabilityFlag() 
{
	TIMAPHostInfo::SetCapabilityForHost(fServerConnection.GetHostName(), kCapabilityUndefined); 
}

/*
 literal         ::= "{" number "}" CRLF *CHAR8
                              ;; Number represents the number of CHAR8 octets
*/
// returns TRUE if this is the last chunk and we should close the stream
XP_Bool TImapServerState::msg_fetch_literal(XP_Bool chunk, int32 origin)
{
	numberOfCharsInThisChunk = atoint32(fNextToken + 1); // might be the whole message
	charsReadSoFar = 0;
	static XP_Bool lastCRLFwasCRCRLF = FALSE;

	XP_Bool lastChunk = !chunk || (origin + numberOfCharsInThisChunk >= fTotalDownloadSize);

	TNavigatorImapConnection *navCon = fServerConnection.GetNavigatorConnection();
	if (navCon)
	{
		if (!lastCRLFwasCRCRLF && navCon->GetIOTunnellingEnabled() && (numberOfCharsInThisChunk > navCon->GetTunnellingThreshold()))
		{
			// One day maybe we'll make this smarter and know how to handle CR/LF boundaries across tunnels.
			// For now, we won't, even though it might not be too hard, because it is very rare and will add
			// some complexity.
			charsReadSoFar = navCon->OpenTunnel(numberOfCharsInThisChunk);
		}
	}

	// If we opened a tunnel, finish everything off here.  Otherwise, get everything here.
	// (just like before)

	while (ContinueParse() && (charsReadSoFar < numberOfCharsInThisChunk))
	{
		AdvanceToNextLine();
		if (ContinueParse())
		{
			if (lastCRLFwasCRCRLF && (*fCurrentLine == CR))
			{
				char *usableCurrentLine = 0;
				StrAllocCopy(usableCurrentLine, fCurrentLine + 1);
				FREEIF(fCurrentLine);
				if (usableCurrentLine)
					fCurrentLine = usableCurrentLine;
				else
					fCurrentLine = 0;
			}

			if (ContinueParse())
			{
				charsReadSoFar += XP_STRLEN(fCurrentLine);
				if (!fDownloadingHeaders && fCurrentCommandIsSingleMessageFetch)
				{
					fServerConnection.ProgressEventFunction_UsingId(MK_IMAP_DOWNLOADING_MESSAGE);
					if (fTotalDownloadSize > 0)
						fServerConnection.PercentProgressUpdateEvent(0,(100*(charsReadSoFar + origin))/fTotalDownloadSize);
				}
				if (charsReadSoFar > numberOfCharsInThisChunk)
				{	// this is rare.  If this msg ends in the middle of a line then only display the actual message.
					char *displayEndOfLine = (fCurrentLine + XP_STRLEN(fCurrentLine) - (charsReadSoFar - numberOfCharsInThisChunk));
					char saveit = *displayEndOfLine;
					*displayEndOfLine = 0;
					fServerConnection.HandleMessageDownLoadLine(fCurrentLine, !lastChunk);
					*displayEndOfLine = saveit;
					lastCRLFwasCRCRLF = (*(displayEndOfLine - 1) == CR);
				}
				else
				{
					lastCRLFwasCRCRLF = (*(fCurrentLine + XP_STRLEN(fCurrentLine) - 1) == CR);
					fServerConnection.HandleMessageDownLoadLine(fCurrentLine, !lastChunk && (charsReadSoFar == numberOfCharsInThisChunk));
				}
			}
		}
	}

	// This would be a good thing to log.
//	if (lastCRLFwasCRCRLF)
//		PR_LOG(IMAP, out, ("PARSER: CR/LF fell on chunk boundary."));
	
	if (ContinueParse())
	{
		if (charsReadSoFar > numberOfCharsInThisChunk)
		{
			// move the lexical analyzer state to the end of this message because this message
			// fetch ends in the middle of this line.
			//fCurrentTokenPlaceHolder = fLineOfTokens + XP_STRLEN(fCurrentLine) - (charsReadSoFar - numberOfCharsInThisChunk);
			AdvanceTokenizerStartingPoint(XP_STRLEN(fCurrentLine) - (charsReadSoFar - numberOfCharsInThisChunk));
			fNextToken = GetNextToken();
		}
		else
		{
			skip_to_CRLF();
			fNextToken = GetNextToken();
		}
	}
	else
	{
		lastCRLFwasCRCRLF = FALSE;
	}
	return lastChunk;
}

XP_Bool TImapServerState::CurrentFolderReadOnly()
{
	return fCurrentFolderReadOnly;
}

int32	TImapServerState::NumberOfMessages()
{
	return fNumberOfExistingMessages;
}

int32 	TImapServerState::NumberOfRecentMessages()
{
	return fNumberOfRecentMessages;
}

int32 	TImapServerState::NumberOfUnseenMessages()
{
	return fNumberOfUnseenMessages;
}

int32 TImapServerState::FolderUID()
{
	return fFolderUIDValidity;
}

uint32 TImapServerState::CurrentResponseUID()
{
	return fCurrentResponseUID;
}

uint32 TImapServerState::HighestRecordedUID()
{
	return fHighestRecordedUID;
}


XP_Bool TImapServerState::IsNumericString(const char *string)
{
	int i;
	for(i = 0; i < (int) strlen(string); i++)
	{
		if (! isdigit(string[i]))
		{
			return FALSE;
		}
	}

	return TRUE;
}

struct mailbox_spec *TImapServerState::CreateCurrentMailboxSpec(const char *mailboxName /* = NULL */)
{
	mailbox_spec *returnSpec = (mailbox_spec *) XP_CALLOC(1, sizeof(mailbox_spec) );
	if (returnSpec)
	{	
		char *convertedMailboxName = NULL;
		const char *mailboxNameToConvert = (mailboxName) ? mailboxName : fSelectedMailboxName;
	    if (mailboxNameToConvert)
	    {
	    	char *convertedName = fServerConnection.CreateUtf7ConvertedString(mailboxNameToConvert, FALSE);
	    	if (convertedName)
	    	{
	    		convertedMailboxName = fServerConnection.GetNavigatorConnection()->GetCurrentUrl()->AllocateCanonicalPath(convertedName);
	    		XP_FREE(convertedName);
	    	}
	    }

		returnSpec->folderSelected = TRUE;
		returnSpec->folder_UIDVALIDITY = fFolderUIDValidity;
		returnSpec->number_of_messages = fNumberOfExistingMessages;
		returnSpec->number_of_unseen_messages = fNumberOfUnseenMessages;
		returnSpec->number_of_recent_messages = fNumberOfRecentMessages;
		
		returnSpec->box_flags = kNoFlags;	// stub
		returnSpec->onlineVerified = FALSE;	// we're fabricating this.  The flags aren't verified.
		returnSpec->allocatedPathName = convertedMailboxName;
		returnSpec->connection = fServerConnection.GetNavigatorConnection();
		if (returnSpec->connection)
			returnSpec->hostName = returnSpec->connection->GetCurrentUrl()->GetUrlHost();
		else
			returnSpec->hostName = NULL;
		if (fFlagState)
			returnSpec->flagState = fFlagState; //copies flag state
		else
			returnSpec->flagState = NULL;
	}
	else
		HandleMemoryFailure();
	
	return returnSpec;
	
}

// zero stops a list recording of flags and causes the flags for
// each individual message to be sent back to libmsg 
void TImapServerState::ResetFlagInfo(int numberOfInterestingMessages)
{
	if (fFlagState)
		fFlagState->Reset(numberOfInterestingMessages);
}


XP_Bool TImapServerState::GetLastFetchChunkReceived()
{
	return fLastChunk;
}

void TImapServerState::ClearLastFetchChunkReceived()
{
	fLastChunk = FALSE;
}



//////////////////// TIMAPNamespace  /////////////////////////////////////////////////////////////



TIMAPNamespace::TIMAPNamespace(EIMAPNamespaceType type, const char *prefix, char delimiter, XP_Bool from_prefs)
{
	m_namespaceType = type;
	m_prefix = XP_STRDUP(prefix);
	m_fromPrefs = from_prefs;

	m_delimiter = delimiter;
	m_delimiterFilledIn = !m_fromPrefs;	// if it's from the prefs, we can't be sure about the delimiter until we list it.
}

TIMAPNamespace::~TIMAPNamespace()
{
	FREEIF(m_prefix);
}

void TIMAPNamespace::SetDelimiter(char delimiter)
{
	m_delimiter = delimiter;
	m_delimiterFilledIn = TRUE;
}

// returns -1 if this box is not part of this namespace,
// or the length of the prefix if it is part of this namespace
int TIMAPNamespace::MailboxMatchesNamespace(const char *boxname)
{
	if (!boxname) return -1;

	// If the namespace is part of the boxname
	if (XP_STRSTR(boxname, m_prefix) == boxname)
		return XP_STRLEN(m_prefix);

	// If the boxname is part of the prefix
	// (Used for matching Personal mailbox with Personal/ namespace, etc.)
	if (XP_STRSTR(m_prefix, boxname) == m_prefix)
		return XP_STRLEN(boxname);
	return -1;
}


TIMAPNamespaceList *TIMAPNamespaceList::CreateTIMAPNamespaceList()
{
	TIMAPNamespaceList *rv = new TIMAPNamespaceList();
	if (rv)
	{
		if (!rv->m_NamespaceList)
		{
			delete rv;
			rv = 0;
		}
	}
	return rv;
}

TIMAPNamespaceList::TIMAPNamespaceList()
{
	// Create a new list
	m_NamespaceList = XP_ListNew();
}

int TIMAPNamespaceList::GetNumberOfNamespaces()
{
	return m_NamespaceList ? XP_ListCount(m_NamespaceList) : 0;
}

int TIMAPNamespaceList::GetNumberOfNamespaces(EIMAPNamespaceType type)
{
	int nodeIndex = 0, count = 0;
	for (nodeIndex=XP_ListCount(m_NamespaceList); nodeIndex > 0; nodeIndex--)
	{
		TIMAPNamespace *nspace = (TIMAPNamespace *)XP_ListGetObjectNum(m_NamespaceList, nodeIndex);
		if (nspace->GetType() == type)
		{
			count++;
		}
	}
	return count;
}

int TIMAPNamespaceList::AddNewNamespace(TIMAPNamespace *ns)
{
	if (!m_NamespaceList)
		return -1;

	// If the namespace is from the NAMESPACE response, then we should see if there
	// are any namespaces previously set by the preferences, or the default namespace.  If so, remove these.

	if (!ns->GetIsNamespaceFromPrefs())
	{
		int nodeIndex = 0;
		for (nodeIndex=XP_ListCount(m_NamespaceList); nodeIndex > 0; nodeIndex--)
		{
			TIMAPNamespace *nspace = (TIMAPNamespace *)XP_ListGetObjectNum(m_NamespaceList, nodeIndex);
			if (nspace->GetIsNamespaceFromPrefs())
			{
				XP_ListRemoveObject(m_NamespaceList, nspace);
				delete nspace;
			}
		}
	}

	// Add the new namespace to the list.  This must come after the removing code,
	// or else we could never add the initial kDefaultNamespace type to the list.
	XP_ListAddObjectToEnd(m_NamespaceList, ns);

	return 0;
}


// chrisf - later, fix this to know the real concept of "default" namespace of a given type
TIMAPNamespace *TIMAPNamespaceList::GetDefaultNamespaceOfType(EIMAPNamespaceType type)
{
	TIMAPNamespace *rv = 0, *firstOfType = 0;
	if (!m_NamespaceList)
		return 0;

	int nodeIndex = 1, count = XP_ListCount(m_NamespaceList);
	for (nodeIndex= 1; nodeIndex <= count && !rv; nodeIndex++)
	{
		TIMAPNamespace *ns = (TIMAPNamespace *)XP_ListGetObjectNum(m_NamespaceList, nodeIndex);
		if (ns->GetType() == type)
		{
			if (!firstOfType)
				firstOfType = ns;
			if (!(*(ns->GetPrefix())))
			{
				// This namespace's prefix is ""
				// Therefore it is the default
				rv = ns;
			}
		}
	}
	if (!rv)
		rv = firstOfType;
	return rv;
}

TIMAPNamespaceList::~TIMAPNamespaceList()
{
	ClearNamespaces(TRUE, TRUE);
	if (m_NamespaceList)
		XP_ListDestroy (m_NamespaceList);
}

// ClearNamespaces removes and deletes the namespaces specified, and if there are no namespaces left,
void TIMAPNamespaceList::ClearNamespaces(XP_Bool deleteFromPrefsNamespaces, XP_Bool deleteServerAdvertisedNamespaces)
{
	int nodeIndex = 0;
	
	if (m_NamespaceList)
	{
		for (nodeIndex=XP_ListCount(m_NamespaceList); nodeIndex > 0; nodeIndex--)
		{
			TIMAPNamespace *ns = (TIMAPNamespace *) XP_ListGetObjectNum(m_NamespaceList, nodeIndex);
			if (ns->GetIsNamespaceFromPrefs())
			{
				if (deleteFromPrefsNamespaces)
				{
					XP_ListRemoveObject(m_NamespaceList, ns);
					delete ns;
				}
			}
			else if (deleteServerAdvertisedNamespaces)
			{
				XP_ListRemoveObject(m_NamespaceList, ns);
				delete ns;
			}
		}
	}
}

TIMAPNamespace *TIMAPNamespaceList::GetNamespaceNumber(int nodeIndex)
{
	int total = GetNumberOfNamespaces();
	XP_ASSERT(nodeIndex >= 1 && nodeIndex <= total);
	if (nodeIndex < 1) nodeIndex = 1;
	if (nodeIndex > total) nodeIndex = total;

	XP_ASSERT(m_NamespaceList);
	if (m_NamespaceList)
	{
		return (TIMAPNamespace *)XP_ListGetObjectNum(m_NamespaceList, nodeIndex);
	}

	return NULL;
}

TIMAPNamespace *TIMAPNamespaceList::GetNamespaceNumber(int nodeIndex, EIMAPNamespaceType type)
{
	int nodeCount = 0, count = 0;
	for (nodeCount=XP_ListCount(m_NamespaceList); nodeCount > 0; nodeCount--)
	{
		TIMAPNamespace *nspace = (TIMAPNamespace *)XP_ListGetObjectNum(m_NamespaceList, nodeCount);
		if (nspace->GetType() == type)
		{
			count++;
			if (count == nodeIndex)
				return nspace;
		}
	}
	return NULL;
}

TIMAPNamespace *TIMAPNamespaceList::GetNamespaceForMailbox(const char *boxname)
{
	// We want to find the LONGEST substring that matches the beginning of this mailbox's path.
	// This accounts for nested namespaces  (i.e. "Public/" and "Public/Users/")
	
	// Also, we want to match the namespace's mailbox to that namespace also:
	// The Personal box will match the Personal/ namespace, etc.

	// these lists shouldn't be too long (99% chance there won't be more than 3 or 4)
	// so just do a linear search

	int lengthMatched = -1;
	int currentMatchedLength = -1;
	TIMAPNamespace *rv = NULL;
	int nodeIndex = 0;

	if (!XP_STRCASECMP(boxname, "INBOX"))
		return GetDefaultNamespaceOfType(kPersonalNamespace);

	if (m_NamespaceList)
	{
		for (nodeIndex=XP_ListCount(m_NamespaceList); nodeIndex > 0; nodeIndex--)
		{
			TIMAPNamespace *nspace = (TIMAPNamespace *)XP_ListGetObjectNum(m_NamespaceList, nodeIndex);
			currentMatchedLength = nspace->MailboxMatchesNamespace(boxname);
			if (currentMatchedLength > lengthMatched)
			{
				rv = nspace;
				lengthMatched = currentMatchedLength;
			}
		}
	}

	return rv;
}


