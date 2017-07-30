/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsBrowserStatusFilter_h__
#define nsBrowserStatusFilter_h__

#include "nsIWebProgressListener.h"
#include "nsIWebProgressListener2.h"
#include "nsIWebProgress.h"
#include "nsWeakReference.h"
#include "nsITimer.h"
#include "nsCOMPtr.h"
#include "nsString.h"

//-----------------------------------------------------------------------------
// nsBrowserStatusFilter - a web progress listener implementation designed to
// sit between nsDocLoader and nsBrowserStatusHandler to filter out and limit
// the frequency of certain events to improve page load performance.
//-----------------------------------------------------------------------------

class nsBrowserStatusFilter : public nsIWebProgress
                            , public nsIWebProgressListener2
                            , public nsSupportsWeakReference
{
public:
    nsBrowserStatusFilter();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBPROGRESS
    NS_DECL_NSIWEBPROGRESSLISTENER
    NS_DECL_NSIWEBPROGRESSLISTENER2

protected:
    virtual ~nsBrowserStatusFilter();

private:
    nsresult StartDelayTimer();
    void ProcessTimeout();
    void MaybeSendProgress();
    void MaybeSendStatus();
    void ResetMembers();
    bool DelayInEffect() { return mDelayedStatus || mDelayedProgress; }

    static void TimeoutHandler(nsITimer *aTimer, void *aClosure);

private:
    nsCOMPtr<nsIWebProgressListener> mListener;
    nsCOMPtr<nsIEventTarget>         mTarget;
    nsCOMPtr<nsITimer>               mTimer;

    // delayed values
    nsString                         mStatusMsg;
    int64_t                          mCurProgress;
    int64_t                          mMaxProgress;

    nsString                         mCurrentStatusMsg;
    bool                             mStatusIsDirty;
    int32_t                          mCurrentPercentage;

    // used to convert OnStart/OnStop notifications into progress notifications
    int32_t                          mTotalRequests;
    int32_t                          mFinishedRequests;
    bool                             mUseRealProgressFlag;

    // indicates whether a timeout is pending
    bool                             mDelayedStatus;
    bool                             mDelayedProgress;
};

#define NS_BROWSERSTATUSFILTER_CONTRACTID \
    "@mozilla.org/appshell/component/browser-status-filter;1"
#define NS_BROWSERSTATUSFILTER_CID                   \
{ /* 6356aa16-7916-4215-a825-cbc2692ca87a */         \
    0x6356aa16,                                      \
    0x7916,                                          \
    0x4215,                                          \
    {0xa8, 0x25, 0xcb, 0xc2, 0x69, 0x2c, 0xa8, 0x7a} \
}

#endif // !nsBrowserStatusFilter_h__
