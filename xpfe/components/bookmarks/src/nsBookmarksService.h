/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef bookmarksservice___h___
#define bookmarksservice___h___

#include "nsIRDFDataSource.h"
#include "nsIRDFRemoteDataSource.h"
#include "nsIRDFPropagatableDataSource.h"
#include "nsIStreamListener.h"
#include "nsIRDFObserver.h"
#include "nsISupportsArray.h"
#include "nsCOMArray.h"
#include "nsIStringBundle.h"
#include "nsITimer.h"
#include "nsIRDFNode.h"
#include "nsIBookmarksService.h"
#include "nsString.h"
#include "nsIFile.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"
#include "nsCOMArray.h"
#include "nsIIOService.h"
#include "nsICacheService.h"
#include "nsICacheSession.h"
#include "nsITransactionManager.h"
#include "nsICharsetResolver.h"

class nsIOutputStream;

#ifdef DEBUG
#if defined(XP_MAC) || defined(XP_MACOSX)
#include <Timer.h>
#endif
#endif

class nsBookmarksService : public nsIBookmarksService,
                           public nsIRDFDataSource,
                           public nsIRDFRemoteDataSource,
                           public nsIRDFPropagatableDataSource,
                           public nsIStreamListener,
                           public nsICharsetResolver,
                           public nsIRDFObserver,
                           public nsIObserver,
                           public nsSupportsWeakReference
{
protected:
    nsIRDFDataSource*               mInner;
    nsCOMPtr<nsIRDFResource>        busyResource;
    nsCOMArray<nsIRDFObserver>      mObservers;
    nsCOMPtr<nsIStringBundle>       mBundle;
    nsCOMPtr<nsITimer>              mTimer;
    nsCOMPtr<nsIIOService>          mNetService;
    nsCOMPtr<nsICacheService>       mCacheService;
    nsCOMPtr<nsICacheSession>       mCacheSession;
    nsCOMPtr<nsITransactionManager> mTransactionManager;
    nsCOMPtr<nsILocalFile>          mBookmarksFile;

    PRUint32      htmlSize;
    PRInt32       mUpdateBatchNest;
    nsXPIDLString mPersonalToolbarName;
    nsXPIDLString mBookmarksRootName;
    PRBool        mDirty;
    PRBool        mBrowserIcons;
    PRBool        mAlwaysLoadIcons;
    PRBool        busySchedule;

    // System Bookmark parsing
#if defined(XP_WIN)
    // @param aDirectory      - Favorites Folder to import from.
    // @param aParentResource - Folder into which to place imported
    //                          Favorites.
    nsresult      ParseFavoritesFolder(nsIFile* aDirectory, 
                                       nsIRDFResource* aParentResource);
#elif defined(XP_MAC) || defined(XP_MACOSX)
    PRBool        mIEFavoritesAvailable;

    nsresult      ReadFavorites();
#endif

#if defined(XP_WIN) || defined(XP_MAC) || defined(XP_MACOSX)
    void          HandleSystemBookmarks(nsIRDFNode* aNode);
#endif

    static void FireTimer(nsITimer* aTimer, void* aClosure);

    nsresult ExamineBookmarkSchedule(nsIRDFResource *theBookmark, PRBool & examineFlag);

    nsresult GetBookmarkToPing(nsIRDFResource **theBookmark);

    nsresult EnsureBookmarksFile();

    nsresult WriteBookmarks(nsIFile* bookmarksFile, nsIRDFDataSource *ds,
                            nsIRDFResource *root);

    nsresult WriteBookmarksContainer(nsIRDFDataSource *ds,
                                     nsIOutputStream* strm,
                                     nsIRDFResource *container,
                                     PRInt32 level,
                                     nsCOMArray<nsIRDFResource>& parentArray);

    nsresult SerializeBookmarks(nsIURI* aURI);

    nsresult GetTextForNode(nsIRDFNode* aNode, nsString& aResult);

    nsresult GetSynthesizedType(nsIRDFResource *aNode, nsIRDFNode **aType);

    nsresult UpdateBookmarkLastModifiedDate(nsIRDFResource *aSource);

    nsresult WriteBookmarkProperties(nsIRDFDataSource *ds,
                                     nsIOutputStream* strm,
                                     nsIRDFResource *node,
                                     nsIRDFResource *property,
                                     const char *htmlAttrib,
                                     PRBool isFirst);

    PRBool   CanAccept(nsIRDFResource* aSource, nsIRDFResource* aProperty, nsIRDFNode* aTarget);

    nsresult getArgumentN(nsISupportsArray *arguments, nsIRDFResource *res,
                          PRInt32 offset, nsIRDFNode **argValue);

    nsresult insertBookmarkItem(nsIRDFResource *src,
                                nsISupportsArray *aArguments,
                                nsIRDFResource *objType);

    nsresult deleteBookmarkItem(nsIRDFResource *src,
                                nsISupportsArray *aArguments,
                                PRInt32 parentArgIndex);

    nsresult setFolderHint(nsIRDFResource *src, nsIRDFResource *objType);

    nsresult getFolderViaHint(nsIRDFResource *src, PRBool fallbackFlag,
                              nsIRDFResource **folder);

    nsresult importBookmarks(nsISupportsArray *aArguments);

    nsresult exportBookmarks(nsISupportsArray *aArguments);

    nsresult ProcessCachedBookmarkIcon(nsIRDFResource* aSource,
                                       const PRUnichar *iconURL,
                                       nsIRDFNode** aTarget);

    void AnnotateBookmarkSchedule(nsIRDFResource* aSource,
                                  PRBool scheduleFlag);

    nsresult InsertResource(nsIRDFResource* aResource,
                            nsIRDFResource* aParentFolder, PRInt32 aIndex);

    nsresult getLocaleString(const char *key, nsString &str);

    static int PR_CALLBACK
    Compare(const void* aElement1, const void* aElement2, void* aData);

    nsresult
    Sort(nsIRDFResource* aFolder, nsIRDFResource* aProperty,
         PRInt32 aDirection, PRBool aFoldersFirst, PRBool aRecurse);

    nsresult
    GetURLFromResource(nsIRDFResource* aResource, nsAString& aURL);

    nsresult
    CopyResource(nsIRDFResource* aOldResource, nsIRDFResource* aNewResource);

    nsresult
    SetNewPersonalToolbarFolder(nsIRDFResource* aFolder);

    nsresult LoadBookmarks();
    nsresult initDatasource();

    // nsIStreamObserver methods:
    NS_DECL_NSIREQUESTOBSERVER

    // nsIStreamListener methods:
    NS_DECL_NSISTREAMLISTENER

    NS_DECL_NSICHARSETRESOLVER

    // nsIObserver methods:
    NS_DECL_NSIOBSERVER

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
                         nsIRDFResource** source)
    {
        return mInner->GetSource(property, target, tv, source);
    }

    NS_IMETHOD GetSources(nsIRDFResource* property,
                          nsIRDFNode* target,
                          PRBool tv,
                          nsISimpleEnumerator** sources)
    {
        return mInner->GetSources(property, target, tv, sources);
    }

    NS_IMETHOD GetTarget(nsIRDFResource* source,
                         nsIRDFResource* property,
                         PRBool tv,
                         nsIRDFNode** target);

    NS_IMETHOD GetTargets(nsIRDFResource* source,
                          nsIRDFResource* property,
                          PRBool tv,
                          nsISimpleEnumerator** targets)
    {
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
                            PRBool* hasAssertion);

    NS_IMETHOD AddObserver(nsIRDFObserver* aObserver);
    NS_IMETHOD RemoveObserver(nsIRDFObserver* aObserver);

    NS_IMETHOD HasArcIn(nsIRDFNode *aNode, nsIRDFResource *aArc, PRBool *_retval);
    NS_IMETHOD HasArcOut(nsIRDFResource *aSource, nsIRDFResource *aArc, PRBool *_retval);

    NS_IMETHOD ArcLabelsIn(nsIRDFNode* node,
                           nsISimpleEnumerator** labels)
    {
        return mInner->ArcLabelsIn(node, labels);
    }

    NS_IMETHOD ArcLabelsOut(nsIRDFResource* source,
                            nsISimpleEnumerator** labels);

    NS_IMETHOD GetAllResources(nsISimpleEnumerator** aResult);

    NS_IMETHOD GetAllCmds(nsIRDFResource* source,
                          nsISimpleEnumerator/*<nsIRDFResource>*/** commands);

    NS_IMETHOD IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                nsIRDFResource*   aCommand,
                                nsISupportsArray/*<nsIRDFResource>*/* aArguments,
                                PRBool* aResult);

    NS_IMETHOD DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                         nsIRDFResource*   aCommand,
                         nsISupportsArray/*<nsIRDFResource>*/* aArguments);

    NS_IMETHOD BeginUpdateBatch() {
        return mInner->BeginUpdateBatch();
    }

    NS_IMETHOD EndUpdateBatch() {
        return mInner->EndUpdateBatch();
    }

    // nsIRDFRemoteDataSource
    NS_DECL_NSIRDFREMOTEDATASOURCE

    // nsIRDFPropagatableDataSource
    NS_DECL_NSIRDFPROPAGATABLEDATASOURCE

    // nsIRDFObserver
    NS_DECL_NSIRDFOBSERVER
};

#endif // bookmarksservice___h___
