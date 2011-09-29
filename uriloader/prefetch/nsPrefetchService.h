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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com> (original author)
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

#ifndef nsPrefetchService_h__
#define nsPrefetchService_h__

#include "nsCPrefetchService.h"
#include "nsIObserver.h"
#include "nsIInterfaceRequestor.h"
#include "nsIChannelEventSink.h"
#include "nsIWebProgressListener.h"
#include "nsIStreamListener.h"
#include "nsIChannel.h"
#include "nsIURI.h"
#include "nsIDOMDocument.h"
#include "nsIDOMLoadStatus.h"
#include "nsWeakReference.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"

class nsPrefetchService;
class nsPrefetchListener;
class nsPrefetchNode;

//-----------------------------------------------------------------------------
// nsPrefetchService
//-----------------------------------------------------------------------------

class nsPrefetchService : public nsIPrefetchService
                        , public nsIWebProgressListener
                        , public nsIObserver
                        , public nsSupportsWeakReference
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIPREFETCHSERVICE
    NS_DECL_NSIWEBPROGRESSLISTENER
    NS_DECL_NSIOBSERVER

    nsPrefetchService();

    nsresult Init();
    void     ProcessNextURI();

    nsPrefetchNode *GetCurrentNode() { return mCurrentNode.get(); }
    nsPrefetchNode *GetQueueHead() { return mQueueHead; }

    void NotifyLoadRequested(nsPrefetchNode *node);
    void NotifyLoadCompleted(nsPrefetchNode *node);

private:
    ~nsPrefetchService();

    nsresult Prefetch(nsIURI *aURI,
                      nsIURI *aReferrerURI,
                      nsIDOMNode *aSource,
                      bool aExplicit);

    void     AddProgressListener();
    void     RemoveProgressListener();
    nsresult EnqueueURI(nsIURI *aURI, nsIURI *aReferrerURI,
                        nsIDOMNode *aSource, nsPrefetchNode **node);
    nsresult EnqueueNode(nsPrefetchNode *node);
    nsresult DequeueNode(nsPrefetchNode **node);
    void     EmptyQueue();

    void     StartPrefetching();
    void     StopPrefetching();

    nsPrefetchNode                   *mQueueHead;
    nsPrefetchNode                   *mQueueTail;
    nsRefPtr<nsPrefetchNode>          mCurrentNode;
    PRInt32                           mStopCount;
    // true if pending document loads have ever reached zero.
    PRInt32                           mHaveProcessed;
    bool                              mDisabled;
};

//-----------------------------------------------------------------------------
// nsPrefetchNode
//-----------------------------------------------------------------------------

class nsPrefetchNode : public nsIDOMLoadStatus
                     , public nsIStreamListener
                     , public nsIInterfaceRequestor
                     , public nsIChannelEventSink
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIDOMLOADSTATUS
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSIINTERFACEREQUESTOR
    NS_DECL_NSICHANNELEVENTSINK

    nsPrefetchNode(nsPrefetchService *aPrefetchService,
                   nsIURI *aURI,
                   nsIURI *aReferrerURI,
                   nsIDOMNode *aSource);

    ~nsPrefetchNode() {}

    nsresult OpenChannel();
    nsresult CancelChannel(nsresult error);

    nsPrefetchNode             *mNext;
    nsCOMPtr<nsIURI>            mURI;
    nsCOMPtr<nsIURI>            mReferrerURI;
    nsCOMPtr<nsIWeakReference>  mSource;

private:
    nsRefPtr<nsPrefetchService> mService;
    nsCOMPtr<nsIChannel>        mChannel;
    PRUint16                    mState;
    PRInt32                     mBytesRead;
};

#endif // !nsPrefetchService_h__
