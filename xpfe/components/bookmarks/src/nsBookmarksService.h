/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef bookmarksservice___h___
#define bookmarksservice___h___

#include "nsIRDFDataSource.h"
#include "nsIRDFRemoteDataSource.h"
#include "nsIStreamListener.h"
#include "nsIRDFObserver.h"
#include "nsISupportsArray.h"
#include "nsIStringBundle.h"
#include "nsITimer.h"
#include "nsIRDFNode.h"
#include "nsIBookmarksService.h"
#include "nsString.h"
#include "nsIFileSpec.h"

#ifdef	DEBUG
#ifdef	XP_MAC
#include <Timer.h>
#endif
#endif

class nsBookmarksService : public nsIBookmarksService,
			   public nsIRDFDataSource,
			   public nsIRDFRemoteDataSource,
			   public nsIStreamListener,
			   public nsIRDFObserver
{
protected:
	nsIRDFDataSource*		mInner;
	PRBool				mBookmarksAvailable;
	PRBool				mDirty;
	PRBool				busySchedule;
	nsCOMPtr<nsIRDFResource>	busyResource;
	PRUint32			htmlSize;
	nsCOMPtr<nsISupportsArray>      mObservers;
	nsCOMPtr<nsIStringBundle>	mBundle;
	nsString			mPersonalToolbarName;
    PRInt32             mUpdateBatchNest;
static	nsCOMPtr<nsITimer>		mTimer;

#ifdef	XP_MAC
	PRBool				mIEFavoritesAvailable;

	nsresult ReadFavorites();
#endif

static	void	FireTimer(nsITimer* aTimer, void* aClosure);
nsresult	ExamineBookmarkSchedule(nsIRDFResource *theBookmark, PRBool & examineFlag);
nsresult	GetBookmarkToPing(nsIRDFResource **theBookmark);

	nsresult GetBookmarksFile(nsFileSpec* aResult);
	nsresult WriteBookmarks(nsFileSpec *bookmarksFile, nsIRDFDataSource *ds, nsIRDFResource *root);
	nsresult WriteBookmarksContainer(nsIRDFDataSource *ds, nsOutputFileStream strm, nsIRDFResource *container, PRInt32 level, nsISupportsArray *parentArray);
	nsresult GetTextForNode(nsIRDFNode* aNode, nsString& aResult);
	nsresult UpdateBookmarkLastModifiedDate(nsIRDFResource *aSource);
	nsresult WriteBookmarkProperties(nsIRDFDataSource *ds, nsOutputFileStream strm, nsIRDFResource *node,
					 nsIRDFResource *property, const char *htmlAttrib, PRBool isFirst);
	PRBool   CanAccept(nsIRDFResource* aSource, nsIRDFResource* aProperty, nsIRDFNode* aTarget);

	nsresult getArgumentN(nsISupportsArray *arguments, nsIRDFResource *res, PRInt32 offset, nsIRDFNode **argValue);
	nsresult insertBookmarkItem(nsIRDFResource *src, nsISupportsArray *aArguments, PRInt32 parentArgIndex, nsIRDFResource *objType);
	nsresult deleteBookmarkItem(nsIRDFResource *src, nsISupportsArray *aArguments, PRInt32 parentArgIndex, nsIRDFResource *objType);
	nsresult setFolderHint(nsIRDFResource *src, nsIRDFResource *objType);
	nsresult getFolderViaHint(nsIRDFResource *src, PRBool fallbackFlag, nsIRDFResource **folder);
	nsresult importBookmarks(nsISupportsArray *aArguments);
	nsresult exportBookmarks(nsISupportsArray *aArguments);

	nsresult getResourceFromLiteralNode(nsIRDFNode *node, nsIRDFResource **res);

	nsresult ChangeURL(nsIRDFResource* aOldURL,
                           nsIRDFResource* aNewURL);

	nsresult getLocaleString(const char *key, nsString &str);

	// nsIStreamObserver methods:
	NS_DECL_NSISTREAMOBSERVER

	// nsIStreamListener methods:
	NS_DECL_NSISTREAMLISTENER

public:
	nsBookmarksService();
	virtual ~nsBookmarksService();
	nsresult Init();

	// nsISupports
	NS_DECL_ISUPPORTS

	// nsIBookmarksService
	NS_DECL_NSIBOOKMARKSSERVICE

	// nsIRDFDataSource
	NS_IMETHOD GetURI(char* *uri);

	NS_IMETHOD GetSource(nsIRDFResource* property,
			     nsIRDFNode* target,
			     PRBool tv,
			     nsIRDFResource** source) {
		return mInner->GetSource(property, target, tv, source);
	}

	NS_IMETHOD GetSources(nsIRDFResource* property,
			      nsIRDFNode* target,
			      PRBool tv,
			      nsISimpleEnumerator** sources) {
		return mInner->GetSources(property, target, tv, sources);
	}

	NS_IMETHOD GetTarget(nsIRDFResource* source,
			     nsIRDFResource* property,
			     PRBool tv,
			     nsIRDFNode** target);

	NS_IMETHOD GetTargets(nsIRDFResource* source,
			      nsIRDFResource* property,
			      PRBool tv,
			      nsISimpleEnumerator** targets) {
		return mInner->GetTargets(source, property, tv, targets);
	}

	NS_IMETHOD Assert(nsIRDFResource* aSource,
			  nsIRDFResource* aProperty,
			  nsIRDFNode* aTarget,
			  PRBool aTruthValue);

	NS_IMETHOD Unassert(nsIRDFResource* aSource,
			    nsIRDFResource* aProperty,
			    nsIRDFNode* aTarget);

	NS_IMETHOD Change(nsIRDFResource* aSource,
			  nsIRDFResource* aProperty,
			  nsIRDFNode* aOldTarget,
			  nsIRDFNode* aNewTarget);

	NS_IMETHOD Move(nsIRDFResource* aOldSource,
			nsIRDFResource* aNewSource,
			nsIRDFResource* aProperty,
			nsIRDFNode* aTarget);

	NS_IMETHOD HasAssertion(nsIRDFResource* source,
				nsIRDFResource* property,
				nsIRDFNode* target,
				PRBool tv,
				PRBool* hasAssertion) {
		return mInner->HasAssertion(source, property, target, tv, hasAssertion);
	}

	NS_IMETHOD AddObserver(nsIRDFObserver* aObserver);
	NS_IMETHOD RemoveObserver(nsIRDFObserver* aObserver);

        NS_IMETHOD HasArcIn(nsIRDFNode *aNode, nsIRDFResource *aArc, PRBool *_retval) {
	    return mInner->HasArcIn(aNode, aArc, _retval);
	}

        NS_IMETHOD HasArcOut(nsIRDFResource *aSource, nsIRDFResource *aArc, PRBool *_retval) {
#ifdef	XP_MAC
	    // on the Mac, IE favorites are stored in an HTML file.
	    // Defer importing this files contents until necessary.

	    if ((aSource == kNC_IEFavoritesRoot) && (mIEFavoritesAvailable == PR_FALSE))
	    {
		ReadFavorites();
	    }
#endif
	    return mInner->HasArcOut(aSource, aArc, _retval);
	}

	NS_IMETHOD ArcLabelsIn(nsIRDFNode* node,
			       nsISimpleEnumerator** labels) {
		return mInner->ArcLabelsIn(node, labels);
	}

	NS_IMETHOD ArcLabelsOut(nsIRDFResource* source,
				nsISimpleEnumerator** labels)
	{
#ifdef	XP_MAC

		// on the Mac, IE favorites are stored in an HTML file.
		// Defer importing this files contents until necessary.

		if ((source == kNC_IEFavoritesRoot) && (mIEFavoritesAvailable == PR_FALSE))
		{
			ReadFavorites();
		}
#endif

		return mInner->ArcLabelsOut(source, labels);
	}

	NS_IMETHOD GetAllResources(nsISimpleEnumerator** aResult)
	{
#ifdef	XP_MAC
		if (mIEFavoritesAvailable == PR_FALSE)
		{
			ReadFavorites();
		}
#endif
		return mInner->GetAllResources(aResult);
	}

	NS_IMETHOD GetAllCommands(nsIRDFResource* source,
				  nsIEnumerator/*<nsIRDFResource>*/** commands);

	NS_IMETHOD GetAllCmds(nsIRDFResource* source,
                              nsISimpleEnumerator/*<nsIRDFResource>*/** commands);

	NS_IMETHOD IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
				    nsIRDFResource*   aCommand,
				    nsISupportsArray/*<nsIRDFResource>*/* aArguments,
				    PRBool* aResult);

	NS_IMETHOD DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
			     nsIRDFResource*   aCommand,
			     nsISupportsArray/*<nsIRDFResource>*/* aArguments);

	// nsIRDFRemoteDataSource
	NS_DECL_NSIRDFREMOTEDATASOURCE

	// nsIRDFObserver
	NS_DECL_NSIRDFOBSERVER
};

#endif // bookmarksservice___h___