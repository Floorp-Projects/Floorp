/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the MRJ Carbon OJI Plugin.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):  Patrick C. Beard <beard@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

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

#if !TARGET_CARBON
	if (&::JMNewAppletPage != NULL) {
		OSStatus status = ::JMNewAppletPage(&mPageRef, session->getSessionRef());
		if (status != noErr) mPageRef = NULL;
	}
#endif
}

MRJPage::MRJPage(MRJSession* session, const MRJPageAttributes& attributes)
	:	mRefCount(0), mNextPage(NULL), mSession(session), mPageRef(NULL),
		mDocumentID(attributes.documentID), mCodeBase(strdup(attributes.codeBase)),
		mArchive(strdup(attributes.archive)), mMayScript(attributes.mayScript)
{
	pushPage();

#if !TARGET_CARBON
	if (&::JMNewAppletPage != NULL) {
		OSStatus status = ::JMNewAppletPage(&mPageRef, session->getSessionRef());
		if (status != noErr) mPageRef = NULL;
	}
#endif
}

MRJPage::~MRJPage()
{
	popPage();
	
#if !TARGET_CARBON
	if (&::JMDisposeAppletPage != NULL && mPageRef != NULL) {
		OSStatus status = ::JMDisposeAppletPage(mPageRef);
		mPageRef = NULL;
	}
#endif
	
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
#if !TARGET_CARBON
	OSStatus status = noErr;
	if (&::JMNewAWTContextInPage != NULL && mPageRef != NULL) {
		status = ::JMNewAWTContextInPage(outContext, mSession->getSessionRef(), mPageRef, callbacks, data);
	} else {
		status = ::JMNewAWTContext(outContext, mSession->getSessionRef(), callbacks, data);
	}
	return (status == noErr);
#else
    return false;
#endif
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
