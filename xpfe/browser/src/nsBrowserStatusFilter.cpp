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

#include "nsBrowserStatusFilter.h"
#include "nsIChannel.h"
#include "nsITimer.h"
#include "nsIServiceManager.h"
#include "nsString.h"

// XXX
// XXX  DO NOT TOUCH THIS CODE UNLESS YOU KNOW WHAT YOU'RE DOING !!!
// XXX

//-----------------------------------------------------------------------------
// nsBrowserStatusFilter <public>
//-----------------------------------------------------------------------------

nsBrowserStatusFilter::nsBrowserStatusFilter()
    : mTotalRequests(0)
    , mFinishedRequests(0)
    , mUseRealProgressFlag(PR_FALSE)
    , mDelayedStatus(PR_FALSE)
    , mDelayedProgress(PR_FALSE)
{
}

nsBrowserStatusFilter::~nsBrowserStatusFilter()
{
    if (mTimer) {
        mTimer->Cancel();
    }
}

//-----------------------------------------------------------------------------
// nsBrowserStatusFilter::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS3(nsBrowserStatusFilter,
                   nsIWebProgress,
                   nsIWebProgressListener,
                   nsISupportsWeakReference)

//-----------------------------------------------------------------------------
// nsBrowserStatusFilter::nsIWebProgress
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsBrowserStatusFilter::AddProgressListener(nsIWebProgressListener *aListener,
                                           PRUint32 aNotifyMask)
{
    mListener = aListener;
    return NS_OK;
}

NS_IMETHODIMP
nsBrowserStatusFilter::RemoveProgressListener(nsIWebProgressListener *aListener)
{
    if (aListener == mListener)
        mListener = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsBrowserStatusFilter::GetDOMWindow(nsIDOMWindow **aResult)
{
    NS_NOTREACHED("nsBrowserStatusFilter::GetDOMWindow");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsBrowserStatusFilter::GetIsLoadingDocument(PRBool *aIsLoadingDocument)
{
    NS_NOTREACHED("nsBrowserStatusFilter::GetIsLoadingDocument");
    return NS_ERROR_NOT_IMPLEMENTED;
}


//-----------------------------------------------------------------------------
// nsBrowserStatusFilter::nsIWebProgressListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsBrowserStatusFilter::OnStateChange(nsIWebProgress *aWebProgress,
                                     nsIRequest *aRequest,
                                     PRUint32 aStateFlags,
                                     nsresult aStatus)
{
    if (!mListener)
        return NS_OK;

    if (aStateFlags & STATE_START) {
        if (aStateFlags & STATE_IS_NETWORK) {
            mTotalRequests = 0;
            mFinishedRequests = 0;
            mUseRealProgressFlag = PR_FALSE;
        }
        if (aStateFlags & STATE_IS_REQUEST) {
            ++mTotalRequests;
        }
    }
    else if (aStateFlags & STATE_STOP) {
        if (aStateFlags & STATE_IS_REQUEST) {
            ++mFinishedRequests;
            if (!mUseRealProgressFlag && mTotalRequests)
                return OnProgressChange(nsnull, nsnull, 0, 0, mFinishedRequests, mTotalRequests);
        }
        if (aStateFlags & STATE_IS_NETWORK) {
            if (mTimer) {
                ProcessTimeout();
                mTimer->Cancel();
                mTimer = nsnull;
            }
        }
    }
    else if (aStateFlags & STATE_TRANSFERRING) {
        if (aStateFlags & STATE_IS_DOCUMENT) {
            nsCOMPtr<nsIChannel> chan(do_QueryInterface(aRequest));
            if (chan) {
                nsCAutoString contentType;
                chan->GetContentType(contentType);
                if (!contentType.Equals(NS_LITERAL_CSTRING("text/html")))
                    mUseRealProgressFlag = PR_TRUE;
            }
        }
        if (aStateFlags & STATE_IS_REQUEST) {
            if (!mUseRealProgressFlag && mTotalRequests)
                return OnProgressChange(nsnull, nsnull, 0, 0, mFinishedRequests, mTotalRequests);
        }

        // no need to forward this state change
        return NS_OK;
    } else {
        // no need to forward this state change
        return NS_OK;
    }

    // If we're here, we have either STATE_START or STATE_STOP.  The
    // listener only cares about these in certain conditions.
    PRBool isLoadingDocument = PR_FALSE;
    if (! (aStateFlags & nsIWebProgressListener::STATE_IS_NETWORK ||
           (aStateFlags & nsIWebProgressListener::STATE_IS_REQUEST &&
            mTotalRequests == mFinishedRequests &&
            (aWebProgress->GetIsLoadingDocument(&isLoadingDocument),
             !isLoadingDocument))))
        return NS_OK;


    return mListener->OnStateChange(aWebProgress, aRequest, aStateFlags, aStatus);
}

NS_IMETHODIMP
nsBrowserStatusFilter::OnProgressChange(nsIWebProgress *aWebProgress,
                                        nsIRequest *aRequest,
                                        PRInt32 aCurSelfProgress,
                                        PRInt32 aMaxSelfProgress,
                                        PRInt32 aCurTotalProgress,
                                        PRInt32 aMaxTotalProgress)
{
    if (!mListener)
        return NS_OK;

    if (!mUseRealProgressFlag && aRequest)
        return NS_OK;

    //
    // limit frequency of calls to OnProgressChange
    //

    mCurProgress = aCurTotalProgress;
    mMaxProgress = aMaxTotalProgress;

    if (mDelayedProgress)
        return NS_OK;

    if (!mDelayedStatus) {
        mListener->OnProgressChange(nsnull, nsnull, 0, 0, mCurProgress, mMaxProgress);
        StartDelayTimer();
    }

    mDelayedProgress = PR_TRUE;

    return NS_OK;
}

NS_IMETHODIMP
nsBrowserStatusFilter::OnLocationChange(nsIWebProgress *aWebProgress,
                                        nsIRequest *aRequest,
                                        nsIURI *aLocation)
{
    if (!mListener)
        return NS_OK;

    return mListener->OnLocationChange(aWebProgress, aRequest, aLocation);
}

NS_IMETHODIMP
nsBrowserStatusFilter::OnStatusChange(nsIWebProgress *aWebProgress,
                                      nsIRequest *aRequest,
                                      nsresult aStatus,
                                      const PRUnichar *aMessage)
{
    if (!mListener)
        return NS_OK;

    //
    // limit frequency of calls to OnStatusChange
    //

    mStatusMsg = aMessage;

    if (mDelayedStatus)
        return NS_OK;

    if (!mDelayedProgress) {
        mListener->OnStatusChange(nsnull, nsnull, 0, aMessage);
        StartDelayTimer();
    }

    mDelayedStatus = PR_TRUE;

    return NS_OK;
}

NS_IMETHODIMP
nsBrowserStatusFilter::OnSecurityChange(nsIWebProgress *aWebProgress,
                                        nsIRequest *aRequest,
                                        PRUint32 aState)
{
    if (!mListener)
        return NS_OK;

    return mListener->OnSecurityChange(aWebProgress, aRequest, aState);
}

//-----------------------------------------------------------------------------
// nsBrowserStatusFilter <private>
//-----------------------------------------------------------------------------

nsresult
nsBrowserStatusFilter::StartDelayTimer()
{
    NS_ASSERTION(!DelayInEffect(), "delay should not be in effect");

    mTimer = do_CreateInstance("@mozilla.org/timer;1");
    if (!mTimer)
      return NS_ERROR_FAILURE;

    return mTimer->InitWithFuncCallback(TimeoutHandler, this, 40, 
                                        nsITimer::TYPE_ONE_SHOT);
}

void
nsBrowserStatusFilter::ProcessTimeout()
{
    if (!mListener)
        return;

    if (mDelayedStatus) {
        mDelayedStatus = PR_FALSE;
        mListener->OnStatusChange(nsnull, nsnull, 0, mStatusMsg.get());
    }

    if (mDelayedProgress) {
        mDelayedProgress = PR_FALSE;
        mListener->OnProgressChange(nsnull, nsnull, 0, 0, mCurProgress, mMaxProgress);
    }
}

void
nsBrowserStatusFilter::TimeoutHandler(nsITimer *aTimer, void *aClosure)
{
    nsBrowserStatusFilter *self = NS_REINTERPRET_CAST(nsBrowserStatusFilter *, aClosure);
    if (!self) {
        NS_ERROR("no self");
        return;
    }

    self->ProcessTimeout();
}
