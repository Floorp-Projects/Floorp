/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#include "nsCOMArray.h"
#include "nsTObserverArray.h"
#include "nsVoidArray.h"
#include "nsString.h"
#include "nsIChannel.h"
#include "nsIProgressEventSink.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIChannelEventSink.h"
#include "nsISecurityEventSink.h"
#include "nsISupportsPriority.h"
#include "nsCOMPtr.h"
#include "pldhash.h"
#include "nsAutoPtr.h"

#include "mozilla/LinkedList.h"

struct nsListenerInfo;

/****************************************************************************
 * nsDocLoader implementation...
 ****************************************************************************/

#define NS_THIS_DOCLOADER_IMPL_CID                    \
 { /* b4ec8387-98aa-4c08-93b6-6d23069c06f2 */         \
     0xb4ec8387,                                      \
     0x98aa,                                          \
     0x4c08,                                          \
     {0x93, 0xb6, 0x6d, 0x23, 0x06, 0x9c, 0x06, 0xf2} \
 }

class nsDocLoader : public nsIDocumentLoader, 
                    public nsIRequestObserver,
                    public nsSupportsWeakReference,
                    public nsIProgressEventSink,
                    public nsIWebProgress,
                    public nsIInterfaceRequestor,
                    public nsIChannelEventSink,
                    public nsISecurityEventSink,
                    public nsISupportsPriority
{
public:
    NS_DECLARE_STATIC_IID_ACCESSOR(NS_THIS_DOCLOADER_IMPL_CID)

    nsDocLoader();

    virtual nsresult Init();

    static already_AddRefed<nsDocLoader> GetAsDocLoader(nsISupports* aSupports);
    // Needed to deal with ambiguous inheritance from nsISupports...
    static nsISupports* GetAsSupports(nsDocLoader* aDocLoader) {
        return static_cast<nsIDocumentLoader*>(aDocLoader);
    }

    // Add aDocLoader as a child to the docloader service.
    static nsresult AddDocLoaderAsChildOfRoot(nsDocLoader* aDocLoader);

    NS_DECL_ISUPPORTS
    NS_DECL_NSIDOCUMENTLOADER
    
    // nsIProgressEventSink
    NS_DECL_NSIPROGRESSEVENTSINK

    NS_DECL_NSISECURITYEVENTSINK

    // nsIRequestObserver methods: (for observing the load group)
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSIWEBPROGRESS

    NS_DECL_NSIINTERFACEREQUESTOR
    NS_DECL_NSICHANNELEVENTSINK
    NS_DECL_NSISUPPORTSPRIORITY

    // Implementation specific methods...

    // Remove aChild from our childlist.  This nulls out the child's mParent
    // pointer.
    nsresult RemoveChildLoader(nsDocLoader *aChild);
    // Add aChild to our child list.  This will set aChild's mParent pointer to
    // |this|.
    nsresult AddChildLoader(nsDocLoader* aChild);
    nsDocLoader* GetParent() const { return mParent; }

protected:
    virtual ~nsDocLoader();

    virtual nsresult SetDocLoaderParent(nsDocLoader * aLoader);

    bool IsBusy();

    void Destroy();
    virtual void DestroyChildren();

    nsIDocumentLoader* ChildAt(int32_t i) {
        return mChildList.SafeElementAt(i, nullptr);
    }

    void FireOnProgressChange(nsDocLoader* aLoadInitiator,
                              nsIRequest *request,
                              int64_t aProgress,
                              int64_t aProgressMax,
                              int64_t aProgressDelta,
                              int64_t aTotalProgress,
                              int64_t aMaxTotalProgress);

    // This should be at least 2 long since we'll generally always
    // have the current page and the global docloader on the ancestor
    // list.  But to deal with frames it's better to make it a bit
    // longer, and it's always a stack temporary so there's no real
    // reason not to.
    typedef nsAutoTArray<nsRefPtr<nsDocLoader>, 8> WebProgressList;
    void GatherAncestorWebProgresses(WebProgressList& aList);

    void FireOnStateChange(nsIWebProgress *aProgress,
                           nsIRequest* request,
                           int32_t aStateFlags,
                           nsresult aStatus);

    // The guts of FireOnStateChange, but does not call itself on our ancestors.
    // The arguments that are const are const so that we can detect cases when
    // DoFireOnStateChange wants to propagate changes to the next web progress
    // at compile time.  The ones that are not, are references so that such
    // changes can be propagated.
    void DoFireOnStateChange(nsIWebProgress * const aProgress,
                             nsIRequest* const request,
                             int32_t &aStateFlags,
                             const nsresult aStatus);

    void FireOnStatusChange(nsIWebProgress *aWebProgress,
                            nsIRequest *aRequest,
                            nsresult aStatus,
                            const PRUnichar* aMessage);

    void FireOnLocationChange(nsIWebProgress* aWebProgress,
                              nsIRequest* aRequest,
                              nsIURI *aUri,
                              uint32_t aFlags);

    bool RefreshAttempted(nsIWebProgress* aWebProgress,
                            nsIURI *aURI,
                            int32_t aDelay,
                            bool aSameURI);

    // this function is overridden by the docshell, it is provided so that we
    // can pass more information about redirect state (the normal OnStateChange
    // doesn't get the new channel).
    // @param aRedirectFlags The flags being sent to OnStateChange that
    //                       indicate the type of redirect.
    // @param aStateFlags    The channel flags normally sent to OnStateChange.
    virtual void OnRedirectStateChange(nsIChannel* aOldChannel,
                                       nsIChannel* aNewChannel,
                                       uint32_t aRedirectFlags,
                                       uint32_t aStateFlags) {}

    void doStartDocumentLoad();
    void doStartURLLoad(nsIRequest *request);
    void doStopURLLoad(nsIRequest *request, nsresult aStatus);
    void doStopDocumentLoad(nsIRequest *request, nsresult aStatus);

    // Inform a parent docloader that aChild is about to call its onload
    // handler.
    bool ChildEnteringOnload(nsIDocumentLoader* aChild) {
        // It's ok if we're already in the list -- we'll just be in there twice
        // and then the RemoveObject calls from ChildDoneWithOnload will remove
        // us.
        return mChildrenInOnload.AppendObject(aChild);
    }

    // Inform a parent docloader that aChild is done calling its onload
    // handler.
    void ChildDoneWithOnload(nsIDocumentLoader* aChild) {
        mChildrenInOnload.RemoveObject(aChild);
        DocLoaderIsEmpty(true);
    }        

protected:
    struct nsStatusInfo : public mozilla::LinkedListElement<nsStatusInfo>
    {
        nsString mStatusMessage;
        nsresult mStatusCode;
        // Weak mRequest is ok; we'll be told if it decides to go away.
        nsIRequest * const mRequest;

        nsStatusInfo(nsIRequest* aRequest) :
            mRequest(aRequest)
        {
            MOZ_COUNT_CTOR(nsStatusInfo);
        }
        ~nsStatusInfo()
        {
            MOZ_COUNT_DTOR(nsStatusInfo);
        }
    };

    struct nsRequestInfo : public PLDHashEntryHdr
    {
        nsRequestInfo(const void* key)
            : mKey(key), mCurrentProgress(0), mMaxProgress(0), mUploading(false)
            , mLastStatus(nullptr)
        {
            MOZ_COUNT_CTOR(nsRequestInfo);
        }

        ~nsRequestInfo()
        {
            MOZ_COUNT_DTOR(nsRequestInfo);
        }

        nsIRequest* Request() {
            return static_cast<nsIRequest*>(const_cast<void*>(mKey));
        }

        const void* mKey; // Must be first for the pldhash stubs to work
        int64_t mCurrentProgress;
        int64_t mMaxProgress;
        bool mUploading;

        nsAutoPtr<nsStatusInfo> mLastStatus;
    };

    static bool RequestInfoHashInitEntry(PLDHashTable* table, PLDHashEntryHdr* entry,
                                         const void* key);
    static void RequestInfoHashClearEntry(PLDHashTable* table, PLDHashEntryHdr* entry);

    // IMPORTANT: The ownership implicit in the following member
    // variables has been explicitly checked and set using nsCOMPtr
    // for owning pointers and raw COM interface pointers for weak
    // (ie, non owning) references. If you add any members to this
    // class, please make the ownership explicit (pinkerton, scc).
  
    nsCOMPtr<nsIRequest>       mDocumentRequest;       // [OWNER] ???compare with document

    nsDocLoader*               mParent;                // [WEAK]

    nsVoidArray                mListenerInfoList;

    nsCOMPtr<nsILoadGroup>        mLoadGroup;
    // We hold weak refs to all our kids
    nsTObserverArray<nsDocLoader*> mChildList;

    // The following member variables are related to the new nsIWebProgress 
    // feedback interfaces that travis cooked up.
    int32_t mProgressStateFlags;

    int64_t mCurrentSelfProgress;
    int64_t mMaxSelfProgress;

    int64_t mCurrentTotalProgress;
    int64_t mMaxTotalProgress;

    PLDHashTable mRequestInfoHash;
    int64_t mCompletedTotalProgress;

    mozilla::LinkedList<nsStatusInfo> mStatusInfoList;

    /*
     * This flag indicates that the loader is loading a document.  It is set
     * from the call to LoadDocument(...) until the OnConnectionsComplete(...)
     * notification is fired...
     */
    bool mIsLoadingDocument;

    /* Flag to indicate that we're in the process of restoring a document. */
    bool mIsRestoringDocument;

    /* Flag to indicate that we're in the process of flushing layout
       under DocLoaderIsEmpty() and should not do another flush. */
    bool mDontFlushLayout;

    /* Flag to indicate whether we should consider ourselves as currently
       flushing layout for the purposes of IsBusy. For example, if Stop has
       been called then IsBusy should return false even if we are still
       flushing. */
    bool mIsFlushingLayout;

private:
    // A list of kids that are in the middle of their onload calls and will let
    // us know once they're done.  We don't want to fire onload for "normal"
    // DocLoaderIsEmpty calls (those coming from requests finishing in our
    // loadgroup) unless this is empty.
    nsCOMArray<nsIDocumentLoader> mChildrenInOnload;
    
    // DocLoaderIsEmpty should be called whenever the docloader may be empty.
    // This method is idempotent and does nothing if the docloader is not in
    // fact empty.  This method _does_ make sure that layout is flushed if our
    // loadgroup has no active requests before checking for "real" emptiness if
    // aFlushLayout is true.
    void DocLoaderIsEmpty(bool aFlushLayout);

    nsListenerInfo *GetListenerInfo(nsIWebProgressListener* aListener);

    int64_t GetMaxTotalProgress();

    nsresult AddRequestInfo(nsIRequest* aRequest);
    void RemoveRequestInfo(nsIRequest* aRequest);
    nsRequestInfo *GetRequestInfo(nsIRequest* aRequest);
    void ClearRequestInfoHash();
    int64_t CalculateMaxProgress();
    static PLDHashOperator CalcMaxProgressCallback(PLDHashTable* table,
                                                   PLDHashEntryHdr* hdr,
                                                   uint32_t number, void* arg);
///    void DumpChannelInfo(void);

    // used to clear our internal progress state between loads...
    void ClearInternalProgress(); 
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsDocLoader, NS_THIS_DOCLOADER_IMPL_CID)

#endif /* nsDocLoader_h__ */
