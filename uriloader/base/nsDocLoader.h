/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1999
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

/* 
*/

#ifndef nsDocLoader_h__
#define nsDocLoader_h__

#include "nsIDocumentLoader.h"
#include "nsIWebProgress.h"
#include "nsIWebProgressListener.h"
#include "nsIRequestObserver.h"
#include "nsWeakReference.h"
#include "nsILoadGroup.h"
#include "nsISupportsArray.h"
#include "nsVoidArray.h"
#include "nsString.h"
#include "nsIChannel.h"
#include "nsIProgressEventSink.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIHttpEventSink.h"
#include "nsISecurityEventSink.h"
#include "nsCOMPtr.h"

struct nsRequestInfo;

/****************************************************************************
 * nsDocLoaderImpl implementation...
 ****************************************************************************/

class nsDocLoaderImpl : public nsIDocumentLoader, 
                        public nsIRequestObserver,
                        public nsSupportsWeakReference,
                        public nsIProgressEventSink,
                        public nsIWebProgress,
                        public nsIInterfaceRequestor,
                        public nsIHttpEventSink,
                        public nsISecurityEventSink
{
public:

    nsDocLoaderImpl();

    nsresult Init();

    // for nsIGenericFactory:
    static NS_IMETHODIMP Create(nsISupports *aOuter, const nsIID &aIID, void **aResult);

    NS_DECL_ISUPPORTS
    NS_DECL_NSIDOCUMENTLOADER
    
    // nsIProgressEventSink
    NS_DECL_NSIPROGRESSEVENTSINK

    NS_DECL_NSISECURITYEVENTSINK

    // nsIRequestObserver methods: (for observing the load group)
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSIWEBPROGRESS

    NS_DECL_NSIINTERFACEREQUESTOR
    NS_DECL_NSIHTTPEVENTSINK

    // Implementation specific methods...
protected:
    virtual ~nsDocLoaderImpl();

    nsresult SetDocLoaderParent(nsDocLoaderImpl * aLoader);
    nsresult RemoveChildGroup(nsDocLoaderImpl *aLoader);
    void DocLoaderIsEmpty();

    void FireOnProgressChange(nsDocLoaderImpl* aLoadInitiator,
                              nsIRequest *request,
                              PRInt32 aProgress,
                              PRInt32 aProgressMax,
                              PRInt32 aProgressDelta,
                              PRInt32 aTotalProgress,
                              PRInt32 aMaxTotalProgress);

    void FireOnStateChange(nsIWebProgress *aProgress,
                           nsIRequest* request,
                           PRInt32 aStateFlags,
                           nsresult aStatus);

    void doStartDocumentLoad();
    void doStartURLLoad(nsIRequest *request);
    void doStopURLLoad(nsIRequest *request, nsresult aStatus);
    void doStopDocumentLoad(nsIRequest *request, nsresult aStatus);

    // get web progress returns our web progress listener or if
    // we don't have one, it will look up the doc loader hierarchy
    // to see if one of our parent doc loaders has one.
    nsresult GetParentWebProgressListener(nsDocLoaderImpl * aDocLoader, nsIWebProgressListener ** aWebProgres);

protected:

    // IMPORTANT: The ownership implicit in the following member
    // variables has been explicitly checked and set using nsCOMPtr
    // for owning pointers and raw COM interface pointers for weak
    // (ie, non owning) references. If you add any members to this
    // class, please make the ownership explicit (pinkerton, scc).
  
    nsCOMPtr<nsIRequest>       mDocumentRequest;       // [OWNER] ???compare with document
    nsCOMPtr<nsISupportsArray> mListenerList;
    nsISupports*               mContainer;             // [WEAK] it owns me!

    nsDocLoaderImpl*           mParent;                // [WEAK]

    nsCString                  mCommand;

    /*
     * This flag indicates that the loader is loading a document.  It is set
     * from the call to LoadDocument(...) until the OnConnectionsComplete(...)
     * notification is fired...
     */
    PRBool mIsLoadingDocument;

    nsCOMPtr<nsILoadGroup>      mLoadGroup;
    nsCOMPtr<nsISupportsArray>  mChildList;

    // The following member variables are related to the new nsIWebProgress 
    // feedback interfaces that travis cooked up.
    nsCOMPtr<nsIWebProgressListener> mProgressListener;
    PRInt32 mProgressStateFlags;

    PRInt32 mCurrentSelfProgress;
    PRInt32 mMaxSelfProgress;

    PRInt32 mCurrentTotalProgress;
    PRInt32 mMaxTotalProgress;

    nsAutoVoidArray mRequestInfoList;

private:
    nsresult GetProgressStatusFlags(PRInt32* aProgressStatusFlags);
    nsresult GetCurSelfProgress(PRInt32* aCurSelfProgress);
    nsresult GetMaxSelfProgress(PRInt32* aMaxSelfProgress);
    nsresult GetCurTotalProgress(PRInt32* aCurTotalProgress);
    nsresult GetMaxTotalProgress(PRInt32* aMaxTotalProgress);

    nsresult AddRequestInfo(nsIRequest* aRequest);
    nsRequestInfo *GetRequestInfo(nsIRequest* aRequest);
    nsresult ClearRequestInfoList(void);
    void CalculateMaxProgress(PRInt32 *aMax);
///    void DumpChannelInfo(void);

    // used to clear our internal progress state between loads...
    void ClearInternalProgress(); 
};

#endif /* nsDocLoader_h__ */
