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

/* 
TIMAPBodyShell and associated classes
*/ 

#ifndef IMAPBODY_H
#define IMAPBODY_H

#include "imap.h"
#include "xp_list.h"


typedef enum _TIMAPBodypartType {
	IMAP_BODY_MESSAGE_RFC822,
	IMAP_BODY_MESSAGE_HEADER,
	IMAP_BODY_LEAF,
	IMAP_BODY_MULTIPART
} TIMAPBodypartType;

class TIMAPGenericParser;
class TNavigatorImapConnection;
class TIMAPBodyShell;
class XPPtrArray;
class TIMAPBodypartMessage;
typedef struct xp_HashTable *XP_HashTable;


class TIMAPBodypart : public TIMAPGenericParser
{
public:
	// Construction
	static TIMAPBodypart *CreatePart(TIMAPBodyShell *shell, char *partNum, const char *buf, TIMAPBodypart *parentPart);
	virtual XP_Bool GetIsValid() { return m_isValid; }
	virtual void	SetIsValid(XP_Bool valid);
	virtual TIMAPBodypartType	GetType() = 0;

	// Generation
	virtual int32	Generate(XP_Bool /*stream*/, XP_Bool /* prefetch */) { return -1; }		// Generates an HTML representation of this part.  Returns content length generated, -1 if failed.
	virtual void	AdoptPartDataBuffer(char *buf);		// Adopts storage for part data buffer.  If NULL, sets isValid to FALSE.
	virtual void	AdoptHeaderDataBuffer(char *buf);	// Adopts storage for header data buffer.  If NULL, sets isValid to FALSE.
	virtual XP_Bool	ShouldFetchInline() { return TRUE; }	// returns TRUE if this part should be fetched inline for generation.
	virtual XP_Bool	PreflightCheckAllInline() { return TRUE; }

	virtual XP_Bool ShouldExplicitlyFetchInline();
	virtual XP_Bool ShouldExplicitlyNotFetchInline();

protected:																// If stream is FALSE, simply returns the content length that will be generated
	virtual int32	GeneratePart(XP_Bool stream, XP_Bool prefetch);					// the body of the part itself
	virtual int32	GenerateMIMEHeader(XP_Bool stream, XP_Bool prefetch);				// the MIME headers of the part
	virtual int32	GenerateBoundary(XP_Bool stream, XP_Bool prefetch, XP_Bool lastBoundary);	// Generates the MIME boundary wrapper for this part.
																			// lastBoundary indicates whether or not this should be the boundary for the
																			// final MIME part of the multipart message.
	virtual int32	GenerateEmptyFilling(XP_Bool stream, XP_Bool prefetch);			// Generates (possibly empty) filling for a part that won't be filled in inline.

	// Part Numbers / Hierarchy
public:
	virtual int		GetPartNumber()	{ return m_partNumber; }	// Returns the part number on this hierarchy level
	virtual char	*GetPartNumberString() { return m_partNumberString; }
	virtual TIMAPBodypart	*FindPartWithNumber(const char *partNum);	// Returns the part object with the given number
	virtual TIMAPBodypart	*GetParentPart() { return m_parentPart; }	// Returns the parent of this part.
																		// We will define a part of type message/rfc822 to be the
																		// parent of its body and header.
																		// A multipart is a parent of its child parts.
																		// All other leafs do not have children.

	// Other / Helpers
public:
	virtual ~TIMAPBodypart();
	virtual XP_Bool	GetNextLineForParser(char **nextLine);
	virtual XP_Bool ContinueParse();	// overrides the parser, but calls it anyway
	virtual TIMAPBodypartMessage	*GetTIMAPBodypartMessage() { return NULL; }


protected:
	virtual void	QueuePrefetchMIMEHeader();
	//virtual void	PrefetchMIMEHeader();			// Initiates a prefetch for the MIME header of this part.
	virtual XP_Bool ParseIntoObjects() = 0;	// Parses buffer and fills in both this and any children with associated objects
									// Returns TRUE if it produced a valid Shell
									// Must be overridden in the concerte derived class
	const char	*GetBodyType() { return m_bodyType; }
	const char	*GetBodySubType() { return m_bodySubType; }

	TIMAPBodypart(TIMAPBodyShell *shell, char *partNumber, const char *buf, TIMAPBodypart *parentPart);

protected:
	TIMAPBodyShell *m_shell;		// points back to the shell
	XP_Bool	m_isValid;				// If this part is valid.
	int		m_partNumber;			// part number on this hierarchy level
	char	*m_partNumberString;	// string representation of this part's full-hierarchy number.  Define 0 to be the top-level message
	char	*m_partData;			// data for this part.  NULL if not filled in yet.
	char	*m_headerData;			// data for this part's MIME header.  NULL if not filled in yet.
	char	*m_boundaryData;		// MIME boundary for this part
	int32	m_partLength;
	int32	m_contentLength;		// Total content length which will be Generate()'d.  -1 if not filled in yet.
	char	*m_responseBuffer;		// The buffer for this object
	TIMAPBodypart	*m_parentPart;	// Parent of this part

	// Fields	- Filled in from parsed BODYSTRUCTURE response (as well as others)
	char	*m_contentType;			// constructed from m_bodyType and m_bodySubType
	char	*m_bodyType;
	char	*m_bodySubType;
	char	*m_bodyID;
	char	*m_bodyDescription;
	char	*m_bodyEncoding;
	// we ignore extension data for now


};



// Message headers
// A special type of TIMAPBodypart
// These may be headers for the top-level message,
// or any body part of type message/rfc822.
class TIMAPMessageHeaders : public TIMAPBodypart
{
public:
	TIMAPMessageHeaders(TIMAPBodyShell *shell, char *partNum, TIMAPBodypart *parentPart);
	virtual TIMAPBodypartType	GetType();
	virtual int32	Generate(XP_Bool stream, XP_Bool prefetch);	// Generates an HTML representation of this part.  Returns content length generated, -1 if failed.
	virtual XP_Bool	ShouldFetchInline();
	virtual void QueuePrefetchMessageHeaders();
protected:
	virtual XP_Bool ParseIntoObjects();	// Parses m_responseBuffer and fills in m_partList with associated objects
										// Returns TRUE if it produced a valid Shell

};


class TIMAPBodypartMultipart : public TIMAPBodypart
{
public:
	TIMAPBodypartMultipart(TIMAPBodyShell *shell, char *partNum, const char *buf, TIMAPBodypart *parentPart);
	virtual TIMAPBodypartType	GetType();
	virtual ~TIMAPBodypartMultipart();
	virtual XP_Bool	ShouldFetchInline();
	virtual XP_Bool	PreflightCheckAllInline();
	virtual int32	Generate(XP_Bool stream, XP_Bool prefetch);		// Generates an HTML representation of this part.  Returns content length generated, -1 if failed.
	virtual TIMAPBodypart	*FindPartWithNumber(const char *partNum);	// Returns the part object with the given number

protected:
	virtual XP_Bool ParseIntoObjects();

protected:
	XPPtrArray			*m_partList;			// An ordered list of top-level body parts for this shell
};


// The name "leaf" is somewhat misleading, since a part of type message/rfc822 is technically
// a leaf, even though it can contain other parts within it.
class TIMAPBodypartLeaf : public TIMAPBodypart
{
public:
	TIMAPBodypartLeaf(TIMAPBodyShell *shell, char *partNum, const char *buf, TIMAPBodypart *parentPart);
	virtual TIMAPBodypartType	GetType();
	virtual int32	Generate(XP_Bool stream, XP_Bool prefetch); 	// Generates an HTML representation of this part.  Returns content length generated, -1 if failed.
	virtual XP_Bool	ShouldFetchInline();		// returns TRUE if this part should be fetched inline for generation.
	virtual XP_Bool	PreflightCheckAllInline();

protected:
	virtual XP_Bool ParseIntoObjects();

};


class TIMAPBodypartMessage : public TIMAPBodypartLeaf
{
public:
	TIMAPBodypartMessage(TIMAPBodyShell *shell, char *partNum, const char *buf, TIMAPBodypart *parentPart, XP_Bool topLevelMessage);
	virtual TIMAPBodypartType	GetType();
	virtual ~TIMAPBodypartMessage();
	virtual int32	Generate(XP_Bool stream, XP_Bool prefetch);
	virtual XP_Bool	ShouldFetchInline();
	virtual XP_Bool	PreflightCheckAllInline();
	virtual TIMAPBodypart	*FindPartWithNumber(const char *partNum);	// Returns the part object with the given number
	void	AdoptMessageHeaders(char *headers);			// Fills in buffer (and adopts storage) for header object
														// partNum specifies the message part number to which the
														// headers correspond.  NULL indicates the top-level message
	virtual TIMAPBodypartMessage	*GetTIMAPBodypartMessage() { return this; }
	virtual	XP_Bool		GetIsTopLevelMessage() { return m_topLevelMessage; }

protected:
	virtual XP_Bool ParseIntoObjects();

protected:
	TIMAPMessageHeaders		*m_headers;				// Every body shell should have headers
	TIMAPBodypart			*m_body;	
	XP_Bool					m_topLevelMessage;		// Whether or not this is the top-level message

};


class TIMAPMessagePartIDArray;

// We will refer to a Body "Shell" as a hierarchical object representation of a parsed BODYSTRUCTURE
// response.  A shell contains representations of Shell "Parts."  A Body Shell can undergo essentially
// two operations: Construction and Generation.
// Shell Construction occurs by parsing a BODYSTRUCTURE response into empty Parts.
// Shell Generation generates a "MIME Shell" of the message and streams it to libmime for
// display.  The MIME Shell has selected (inline) parts filled in, and leaves all others
// for on-demand retrieval through explicit part fetches.

class TIMAPBodyShell
{
public:

	// Construction
	TIMAPBodyShell(TNavigatorImapConnection *connection, const char *bs, uint32 UID, const char *folderName);		// Constructor takes in a buffer containing an IMAP
										// bodystructure response from the server, with the associated
										// tag/command/etc. stripped off.
										// That is, it takes in something of the form:
										// (("TEXT" "PLAIN" .....  ))
	virtual ~TIMAPBodyShell();
	void	SetConnection(TNavigatorImapConnection *con) { m_connection = con; }	// To be used after a shell is uncached
	virtual XP_Bool GetIsValid() { return m_isValid; }
	virtual void	SetIsValid(XP_Bool valid);

	// Prefetch
	void	AddPrefetchToQueue(TIMAP4BlockingConnection::eFetchFields, const char *partNum);	// Adds a message body part to the queue to be prefetched
									// in a single, pipelined command
	void	FlushPrefetchQueue();	// Runs a single pipelined command which fetches all of the
									// elements in the prefetch queue
	void	AdoptMessageHeaders(char *headers, const char *partNum);	// Fills in buffer (and adopts storage) for header object
																		// partNum specifies the message part number to which the
																		// headers correspond.  NULL indicates the top-level message
	void	AdoptMimeHeader(const char *partNum, char *mimeHeader);	// Fills in buffer (and adopts storage) for MIME headers in appropriate object.
																		// If object can't be found, sets isValid to FALSE.

	// Generation
	virtual int32 Generate(char *partNum);	// Streams out an HTML representation of this IMAP message, going along and
									// fetching parts it thinks it needs, and leaving empty shells for the parts
									// it doesn't.
									// Returns number of bytes generated, or -1 if invalid.
									// If partNum is not NULL, then this works to generates a MIME part that hasn't been downloaded yet
									// and leaves out all other parts.  By default, to generate a normal message, partNum should be NULL.

	XP_Bool	GetShowAttachmentsInline();		// Returns TRUE if the user has the pref "Show Attachments Inline" set.
											// Returns FALSE if the setting is "Show Attachments as Links"
	XP_Bool	PreflightCheckAllInline();		// Returns TRUE if all parts are inline, FALSE otherwise.  Does not generate anything.

	// Helpers
	TNavigatorImapConnection *GetConnection() { return m_connection; }
	XP_Bool	GetPseudoInterrupted();
	XP_Bool DeathSignalReceived();
	const char	*GetUID() { return m_UID; }
	const char	*GetFolderName() { return m_folderName; }
	char	*GetGeneratingPart() { return m_generatingPart; }
	XP_Bool	IsBeingGenerated() { return m_isBeingGenerated; }	// Returns TRUE if this is in the process of being
																// generated, so we don't re-enter
	XP_Bool IsShellCached() { return m_cached; }
	void	SetIsCached(XP_Bool isCached) { m_cached = isCached; }
	XP_Bool	GetGeneratingWholeMessage() { return m_generatingWholeMessage; }

protected:

	TIMAPBodypartMessage	*m_message;

	TIMAPMessagePartIDArray	*m_prefetchQueue;	// array of pipelined part prefetches.  Ok, so it's not really a queue.

	XP_Bool				m_isValid;
	TNavigatorImapConnection	*m_connection;	// Connection, for filling in parts
	char				*m_UID;					// UID of this message
	char				*m_folderName;			// folder that contains this message
	char				*m_generatingPart;		// If a specific part is being generated, this is it.  Otherwise, NULL.
	XP_Bool				m_isBeingGenerated;		// TRUE if this body shell is in the process of being generated
	XP_Bool				m_showAttachmentsInline;
	XP_Bool				m_gotAttachmentPref;
	XP_Bool				m_cached;				// Whether or not this shell is cached
	XP_Bool				m_generatingWholeMessage;	// whether or not we are generating the whole (non-MPOD) message
													// Set to FALSE if we are generating by parts
};



// This class caches shells, so we don't have to always go and re-fetch them.
// This does not cache any of the filled-in inline parts;  those are cached individually
// in the libnet memory cache.  (ugh, how will we do that?)
// Since we'll only be retrieving shells for messages over a given size, and since the
// shells themselves won't be very large, this cache will not grow very big (relatively)
// and should handle most common usage scenarios.

// A body cache is associated with a given host, spanning folders.
// It should pay attention to UIDVALIDITY.

class TIMAPBodyShellCache
{

public:
	static TIMAPBodyShellCache	*Create();
	virtual ~TIMAPBodyShellCache();

	XP_Bool			AddShellToCache(TIMAPBodyShell *shell);		// Adds shell to cache, possibly ejecting
																// another entry based on scheme in EjectEntry().
	TIMAPBodyShell	*FindShellForUID(const char *UID, const char *mailboxName);	// Looks up a shell in the cache given the message's UID.
	TIMAPBodyShell	*FindShellForUID(uint32 UID, const char *mailboxName);	// Looks up a shell in the cache given the message's UID.
															// Returns the shell if found, NULL if not found.

protected:
	TIMAPBodyShellCache();
	XP_Bool	EjectEntry();	// Chooses an entry to eject;  deletes that entry;  and ejects it from the cache,
							// clearing up a new space.  Returns TRUE if it found an entry to eject, FALSE otherwise.
	uint32	GetSize() { return m_shellList->GetSize(); }
	uint32	GetMaxSize() { return 20; }


protected:
	XPPtrArray		*m_shellList;	// For maintenance
	XP_HashTable	m_shellHash;	// For quick lookup based on UID

};




// MessagePartID and MessagePartIDArray are used for pipelining prefetches.

class TIMAPMessagePartID
{
public:
	TIMAPMessagePartID(TIMAP4BlockingConnection::eFetchFields fields, const char *partNumberString);
	TIMAP4BlockingConnection::eFetchFields		GetFields() { return m_fields; }
	const char		*GetPartNumberString() { return m_partNumberString; }


protected:
	const char *m_partNumberString;
	TIMAP4BlockingConnection::eFetchFields m_fields;
};


class TIMAPMessagePartIDArray : public XPPtrArray {
public:
	TIMAPMessagePartIDArray();
	~TIMAPMessagePartIDArray();

	void				RemoveAndFreeAll();
	int					GetNumParts() {return GetSize();}
	TIMAPMessagePartID	*GetPart(int i) 
	{
		XP_ASSERT(i >= 0 && i < GetSize());
		return (TIMAPMessagePartID *) GetAt(i);
	}
};


#endif // IMAPBODY_H
