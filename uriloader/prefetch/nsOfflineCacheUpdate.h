/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dave Camp <dcamp@mozilla.com>
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
 * ***** END LICENSE BLOCK ***** */

#ifndef nsOfflineCacheUpdate_h__
#define nsOfflineCacheUpdate_h__

#include "nsIOfflineCacheUpdate.h"

#include "nsAutoPtr.h"
#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsICacheService.h"
#include "nsIChannelEventSink.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNode.h"
#include "nsIDOMLoadStatus.h"
#include "nsIInterfaceRequestor.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIOfflineCacheSession.h"
#include "nsIPrefetchService.h"
#include "nsIRequestObserver.h"
#include "nsIRunnable.h"
#include "nsIStreamListener.h"
#include "nsIURI.h"
#include "nsIWebProgressListener.h"
#include "nsClassHashtable.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsWeakReference.h"

class nsOfflineCacheUpdate;

class nsOfflineCacheUpdateItem : public nsIDOMLoadStatus
                               , public nsIStreamListener
                               , public nsIRunnable
                               , public nsIInterfaceRequestor
                               , public nsIChannelEventSink
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIDOMLOADSTATUS
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSIRUNNABLE
    NS_DECL_NSIINTERFACEREQUESTOR
    NS_DECL_NSICHANNELEVENTSINK

    nsOfflineCacheUpdateItem(nsOfflineCacheUpdate *aUpdate,
                             nsIURI *aURI,
                             nsIURI *aReferrerURI,
                             const nsACString &aClientID);
    virtual ~nsOfflineCacheUpdateItem();

    nsCOMPtr<nsIURI>           mURI;
    nsCOMPtr<nsIURI>           mReferrerURI;
    nsCString                  mClientID;

    nsresult OpenChannel();
    nsresult Cancel();

private:
    nsOfflineCacheUpdate*          mUpdate;
    nsCOMPtr<nsIChannel>           mChannel;
    PRUint16                       mState;

protected:
    PRInt32                        mBytesRead;
};


class nsOfflineManifestItem : public nsOfflineCacheUpdateItem
{
public:
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSIREQUESTOBSERVER

    nsOfflineManifestItem(nsOfflineCacheUpdate *aUpdate,
                          nsIURI *aURI,
                          nsIURI *aReferrerURI,
                          const nsACString &aClientID);
    virtual ~nsOfflineManifestItem();

    nsCOMArray<nsIURI> &GetExplicitURIs() { return mExplicitURIs; }

    PRBool ParseSucceeded()
        { return (mParserState != PARSE_INIT && mParserState != PARSE_ERROR); }
    PRBool NeedsUpdate() { return mParserState != PARSE_INIT && mNeedsUpdate; }

private:
    static NS_METHOD ReadManifest(nsIInputStream *aInputStream,
                                  void *aClosure,
                                  const char *aFromSegment,
                                  PRUint32 aOffset,
                                  PRUint32 aCount,
                                  PRUint32 *aBytesConsumed);

    nsresult HandleManifestLine(const nsCString::const_iterator &aBegin,
                                const nsCString::const_iterator &aEnd);

    enum {
        PARSE_INIT,
        PARSE_CACHE_ENTRIES,
        PARSE_FALLBACK_ENTRIES,
        PARSE_NETWORK_ENTRIES,
        PARSE_ERROR
    } mParserState;

    nsCString mReadBuf;
    nsCOMArray<nsIURI> mExplicitURIs;
    PRBool mNeedsUpdate;
};

class nsOfflineCacheUpdate : public nsIOfflineCacheUpdate
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOFFLINECACHEUPDATE

    nsOfflineCacheUpdate();
    ~nsOfflineCacheUpdate();

    nsresult Init();

    nsresult Begin();
    nsresult Cancel();

    void LoadCompleted();
private:
    nsresult HandleManifest(PRBool *aDoUpdate);
    nsresult AddURI(nsIURI *aURI, const nsACString &aOwnerSpec);

    nsresult ProcessNextURI();

    nsresult AddOwnedItems(const nsACString &aOwnerURI);

    nsresult GatherObservers(nsCOMArray<nsIOfflineCacheUpdateObserver> &aObservers);
    nsresult NotifyError();
    nsresult NotifyChecking();
    nsresult NotifyNoUpdate();
    nsresult NotifyDownloading();
    nsresult NotifyStarted(nsOfflineCacheUpdateItem *aItem);
    nsresult NotifyCompleted(nsOfflineCacheUpdateItem *aItem);
    nsresult Finish();

    enum {
        STATE_UNINITIALIZED,
        STATE_INITIALIZED,
        STATE_CHECKING,
        STATE_DOWNLOADING,
        STATE_CANCELLED,
        STATE_FINISHED
    } mState;

    PRBool mAddedItems;
    PRBool mPartialUpdate;
    PRBool mSucceeded;
    nsCString mUpdateDomain;
    nsCOMPtr<nsIURI> mManifestURI;
    nsCString mManifestOwnerSpec;
    nsCString mDynamicOwnerSpec;

    nsCOMPtr<nsIURI> mDocumentURI;

    nsCString mClientID;
    nsCOMPtr<nsIOfflineCacheSession> mCacheSession;
    nsCOMPtr<nsIOfflineCacheSession> mMainCacheSession;

    nsCOMPtr<nsIObserverService> mObserverService;

    nsRefPtr<nsOfflineManifestItem> mManifestItem;

    /* Items being updated */
    PRInt32 mCurrentItem;
    nsTArray<nsRefPtr<nsOfflineCacheUpdateItem> > mItems;

    /* Clients watching this update for changes */
    nsCOMArray<nsIWeakReference> mWeakObservers;
    nsCOMArray<nsIOfflineCacheUpdateObserver> mObservers;
};

class nsOfflineCacheUpdateService : public nsIOfflineCacheUpdateService
                                  , public nsIWebProgressListener
                                  , public nsIObserver
                                  , public nsSupportsWeakReference
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOFFLINECACHEUPDATESERVICE
    NS_DECL_NSIWEBPROGRESSLISTENER
    NS_DECL_NSIOBSERVER

    nsOfflineCacheUpdateService();
    ~nsOfflineCacheUpdateService();

    nsresult Init();

    nsresult Schedule(nsOfflineCacheUpdate *aUpdate);
    nsresult UpdateFinished(nsOfflineCacheUpdate *aUpdate);

    /**
     * Returns the singleton nsOfflineCacheUpdateService without an addref, or
     * nsnull if the service couldn't be created.
     */
    static nsOfflineCacheUpdateService *EnsureService();

    /** Addrefs and returns the singleton nsOfflineCacheUpdateService. */
    static nsOfflineCacheUpdateService *GetInstance();
    
private:
    nsresult ProcessNextUpdate();

    nsTArray<nsRefPtr<nsOfflineCacheUpdate> > mUpdates;

    struct PendingUpdate {
        nsCOMPtr<nsIURI> mManifestURI;
        nsCOMPtr<nsIURI> mDocumentURI;
    };
    nsClassHashtable<nsVoidPtrHashKey, PendingUpdate> mDocUpdates;

    PRBool mDisabled;
    PRBool mUpdateRunning;
};

#endif
