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
 *   Darin Fisher <darin@netscape.com>
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
    virtual ~nsBrowserStatusFilter();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBPROGRESS
    NS_DECL_NSIWEBPROGRESSLISTENER
    NS_DECL_NSIWEBPROGRESSLISTENER2

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
    nsCOMPtr<nsITimer>               mTimer;

    // delayed values
    nsString                         mStatusMsg;
    PRInt64                          mCurProgress;
    PRInt64                          mMaxProgress;

    nsString                         mCurrentStatusMsg;
    bool                             mStatusIsDirty;
    PRInt32                          mCurrentPercentage;

    // used to convert OnStart/OnStop notifications into progress notifications
    PRInt32                          mTotalRequests;
    PRInt32                          mFinishedRequests;
    bool                             mUseRealProgressFlag;

    // indicates whether a timeout is pending
    bool                             mDelayedStatus;
    bool                             mDelayedProgress;
};

#define NS_BROWSERSTATUSFILTER_CLASSNAME \
    "nsBrowserStatusFilter"
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
