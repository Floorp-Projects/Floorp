/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/* 
*/

#ifndef nsDocLoader_h__
#define nsDocLoader_h__

#include "nsIDocumentLoader.h"
#include "nsIWebProgress.h"
#include "nsIWebProgressListener.h"
#include "nsIStreamObserver.h"
#include "nsWeakReference.h"
#include "nsILoadGroup.h"
#include "nsISupportsArray.h"
#include "nsVoidArray.h"
#include "nsString.h"
#include "nsIChannel.h"
#include "nsIProgressEventSink.h"
#include "nsIInterfaceRequestor.h"
#include "nsIHTTPEventSink.h"
#include "nsCOMPtr.h"

struct nsChannelInfo;

/****************************************************************************
 * nsDocLoaderImpl implementation...
 ****************************************************************************/

class nsDocLoaderImpl : public nsIDocumentLoader, 
                        public nsIStreamObserver,
                        public nsSupportsWeakReference,
                        public nsIProgressEventSink,
                        public nsIWebProgress,
                        public nsIInterfaceRequestor,
                        public nsIHTTPEventSink
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

    // nsIStreamObserver methods: (for observing the load group)
    NS_DECL_NSISTREAMOBSERVER
    NS_DECL_NSIWEBPROGRESS

    NS_DECL_NSIINTERFACEREQUESTOR
    NS_DECL_NSIHTTPEVENTSINK

    // Implementation specific methods...
protected:
    virtual ~nsDocLoaderImpl();

    nsresult SetDocLoaderParent(nsDocLoaderImpl * aLoader);
    nsresult RemoveChildGroup(nsDocLoaderImpl *aLoader);
    void DocLoaderIsEmpty(nsresult aStatus);

    void FireOnStartDocumentLoad(nsDocLoaderImpl* aLoadInitiator,
                                 nsIChannel* aChannel);

    void FireOnEndDocumentLoad(nsDocLoaderImpl* aLoadInitiator,
                               nsIChannel *aDocChannel,
                               nsresult aStatus);
							   
    void FireOnStartURLLoad(nsDocLoaderImpl* aLoadInitiator,
                            nsIChannel* channel);

    void FireOnEndURLLoad(nsDocLoaderImpl* aLoadInitiator,
                          nsIChannel* channel, nsresult aStatus);

    void FireOnProgressChange(nsDocLoaderImpl* aLoadInitiator,
                              nsIChannel* aChannel,
                              PRInt32 aProgress,
                              PRInt32 aProgressMax,
                              PRInt32 aProgressDelta,
                              PRInt32 aTotalProgress,
                              PRInt32 aMaxTotalProgress);

    void FireOnStateChange(nsIWebProgress *aProgress,
                           nsIRequest* aChannel,
                           PRInt32 aStateFlags,
                           nsresult aStatus);

    void doStartDocumentLoad();
    void doStartURLLoad(nsIChannel *aChannel);
    void doStopURLLoad(nsIChannel *aChannel, nsresult aStatus);
    void doStopDocumentLoad(nsIChannel* aChannel, nsresult aStatus);

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
  
    nsCOMPtr<nsIChannel>       mDocumentChannel;       // [OWNER] ???compare with document
    nsVoidArray                mDocObservers;
    nsVoidArray                mListenerList;
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

    nsVoidArray mChannelInfoList;

private:
    nsresult AddChannelInfo(nsIChannel *aChannel);
    nsChannelInfo *GetChannelInfo(nsIChannel *aChannel);
    nsresult ClearChannelInfoList(void);
    void CalculateMaxProgress(PRInt32 *aMax);
///    void DumpChannelInfo(void);

    // used to clear our internal progress state between loads...
    void ClearInternalProgress(); 
};

#endif /* nsDocLoader_h__ */
