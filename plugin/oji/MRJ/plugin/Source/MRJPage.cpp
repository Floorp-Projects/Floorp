/*
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
	MRJPage.cpp
	
	Encapsulates the new MRJ Page API, which loads applets into a common context.
	
	by Patrick C. Beard.
 */

#include "MRJPage.h"
#include "MRJSession.h"

#include "StringUtils.h"

MRJPage::MRJPage(MRJSession* session, UInt32 documentID, const char* codeBase, const char* archive, Boolean mayScript)
	:	mRefCount(0), mNextPage(NULL), mSession(session), mPageRef(NULL),
		mDocumentID(documentID), mCodeBase(strdup(codeBase)), mArchive(strdup(archive)), mMayScript(mayScript)
{
	pushPage();

	if (&::JMNewAppletPage != NULL) {
		OSStatus status = ::JMNewAppletPage(&mPageRef, session->getSessionRef());
		if (status != noErr) mPageRef = NULL;
	}
}

MRJPage::MRJPage(MRJSession* session, const MRJPageAttributes& attributes)
	:	mRefCount(0), mNextPage(NULL), mSession(session), mPageRef(NULL),
		mDocumentID(attributes.documentID), mCodeBase(strdup(attributes.codeBase)),
		mArchive(strdup(attributes.archive)), mMayScript(attributes.mayScript)
{
	pushPage();

	if (&::JMNewAppletPage != NULL) {
		OSStatus status = ::JMNewAppletPage(&mPageRef, session->getSessionRef());
		if (status != noErr) mPageRef = NULL;
	}
}

MRJPage::~MRJPage()
{
	popPage();
	
	if (&::JMDisposeAppletPage != NULL && mPageRef != NULL) {
		OSStatus status = ::JMDisposeAppletPage(mPageRef);
		mPageRef = NULL;
	}
	
	if (mCodeBase != NULL) {
		delete[] mCodeBase;
		mCodeBase = NULL;
	}
	
	if (mArchive != NULL) {
		delete[] mArchive;
		mArchive = NULL;
	}
}

UInt16 MRJPage::AddRef()
{
	return (++mRefCount);
}

UInt16 MRJPage::Release()
{
	UInt16 result = --mRefCount;
	if (result == 0) {
		delete this;
	}
	return result;
}

Boolean MRJPage::createContext(JMAWTContextRef* outContext, const JMAWTContextCallbacks * callbacks, JMClientData data)
{
	OSStatus status = noErr;
	if (&::JMNewAWTContextInPage != NULL && mPageRef != NULL) {
		status = ::JMNewAWTContextInPage(outContext, mSession->getSessionRef(), mPageRef, callbacks, data);
	} else {
		status = ::JMNewAWTContext(outContext, mSession->getSessionRef(), callbacks, data);
	}
	return (status == noErr);
}

static MRJPage* thePageList = NULL;

MRJPage* MRJPage::getFirstPage()
{
	return thePageList;
}

MRJPage* MRJPage::getNextPage()
{
	return mNextPage;
}

void MRJPage::pushPage()
{
	// put this on the global list of pages.
	mNextPage = thePageList;
	thePageList = this;
}

void MRJPage::popPage()
{
	// Remove this page from the global list.
	MRJPage** link = &thePageList;
	MRJPage* page  = *link;
	while (page != NULL) {
		if (page == this) {
			*link = mNextPage;
			mNextPage = NULL;
			break;
		}
		link = &page->mNextPage;
		page = *link;
	}
}
