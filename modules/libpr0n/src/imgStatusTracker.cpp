/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Initial Developer of the Original Code is Mozilla Foundation.
 *
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Joe Drew <joe@drew.ca> (original author)
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

#include "imgStatusTracker.h"

#include "imgRequest.h"
#include "imgIContainer.h"
#include "imgRequestProxy.h"

static nsresult
GetResultFromImageStatus(PRUint32 aStatus)
{
  if (aStatus & imgIRequest::STATUS_ERROR)
    return NS_IMAGELIB_ERROR_FAILURE;
  if (aStatus & imgIRequest::STATUS_LOAD_COMPLETE)
    return NS_IMAGELIB_SUCCESS_LOAD_FINISHED;
  return NS_OK;
}

imgStatusTracker::imgStatusTracker(imgIContainer* aImage)
  : mImage(aImage),
    mState(0),
    mImageStatus(imgIRequest::STATUS_NONE),
    mHadLastPart(PR_FALSE)
{}

PRBool
imgStatusTracker::IsLoading() const
{
  // Checking for whether OnStopRequest has fired allows us to say we're
  // loading before OnStartRequest gets called, letting the request properly
  // get removed from the cache in certain cases.
  return !(mState & stateRequestStopped);
}

PRUint32
imgStatusTracker::GetImageStatus() const
{
  return mImageStatus;
}

// A helper class to allow us to call SyncNotify asynchronously.
class imgRequestNotifyRunnable : public nsRunnable
{
  public:
    imgRequestNotifyRunnable(imgRequest* request, imgRequestProxy* requestproxy)
      : mRequest(request), mProxy(requestproxy)
    {}

    NS_IMETHOD Run()
    {
      mProxy->SetNotificationsDeferred(PR_FALSE);

      mRequest->mImage->GetStatusTracker().SyncNotify(mProxy);
      return NS_OK;
    }

  private:
    nsRefPtr<imgRequest> mRequest;
    nsRefPtr<imgRequestProxy> mProxy;
};

void
imgStatusTracker::Notify(imgRequest* request, imgRequestProxy* proxy)
{
  proxy->SetNotificationsDeferred(PR_TRUE);

  nsCOMPtr<nsIRunnable> ev = new imgRequestNotifyRunnable(request, proxy);
  NS_DispatchToCurrentThread(ev);
}

// A helper class to allow us to call SyncNotify asynchronously for a given,
// fixed, state.
class imgStatusNotifyRunnable : public nsRunnable
{
  public:
    imgStatusNotifyRunnable(imgStatusTracker status,
                            imgRequestProxy* requestproxy)
      : mStatus(status), mImage(status.mImage), mProxy(requestproxy)
    {}

    NS_IMETHOD Run()
    {
      mProxy->SetNotificationsDeferred(PR_FALSE);

      mStatus.SyncNotify(mProxy);
      return NS_OK;
    }

  private:
    imgStatusTracker mStatus;
    // We have to hold on to a reference to the tracker's image, just in case
    // it goes away while we're in the event queue.
    nsRefPtr<imgIContainer> mImage;
    nsRefPtr<imgRequestProxy> mProxy;
};

void
imgStatusTracker::NotifyCurrentState(imgRequestProxy* proxy)
{
  proxy->SetNotificationsDeferred(PR_TRUE);

  nsCOMPtr<nsIRunnable> ev = new imgStatusNotifyRunnable(*this, proxy);
  NS_DispatchToCurrentThread(ev);
}

void
imgStatusTracker::SyncNotify(imgRequestProxy* proxy)
{
  NS_ABORT_IF_FALSE(!proxy->NotificationsDeferred(),
    "Calling imgStatusTracker::Notify() on a proxy that doesn't want notifications!");

  nsCOMPtr<imgIRequest> kungFuDeathGrip(proxy);

  // OnStartRequest
  if (mState & stateRequestStarted)
    proxy->OnStartRequest();

  // OnStartContainer
  if (mState & stateHasSize)
    proxy->OnStartContainer(mImage);

  // OnStartDecode
  if (mState & stateDecodeStarted)
    proxy->OnStartDecode();

  // Send frame messages (OnStartFrame, OnDataAvailable, OnStopFrame)
  PRUint32 nframes = 0;
  mImage->GetNumFrames(&nframes);

  if (nframes > 0) {
    PRUint32 frame;
    mImage->GetCurrentFrameIndex(&frame);
    proxy->OnStartFrame(frame);

    // OnDataAvailable
    // XXX - Should only send partial rects here, but that needs to
    // wait until we fix up the observer interface
    nsIntRect r;
    mImage->GetCurrentFrameRect(r);
    proxy->OnDataAvailable(frame, &r);

    if (mState & stateFrameStopped)
      proxy->OnStopFrame(frame);
  }

  // See bug 505385 and imgRequest::OnStopDecode for more information on why we
  // call OnStopContainer based on stateDecodeStopped, and why OnStopDecode is
  // called with OnStopRequest.
  if (mState & stateDecodeStopped)
    proxy->OnStopContainer(mImage);

  if (mState & stateRequestStopped) {
    proxy->OnStopDecode(GetResultFromImageStatus(mImageStatus), nsnull);
    proxy->OnStopRequest(mHadLastPart);
  }
}

void
imgStatusTracker::EmulateRequestFinished(imgRequestProxy* aProxy, nsresult aStatus,
                                                PRBool aOnlySendStopRequest)
{
  nsCOMPtr<imgIRequest> kungFuDeathGrip(aProxy);

  if (!aOnlySendStopRequest) {
    // The "real" OnStopDecode - fix this with bug 505385.
    if (!(mState & stateDecodeStopped)) {
      aProxy->OnStopContainer(mImage);
    }

    if (!(mState & stateRequestStopped)) {
      aProxy->OnStopDecode(aStatus, nsnull);
    }
  }

  if (!(mState & stateRequestStopped)) {
    aProxy->OnStopRequest(PR_TRUE);
  }
}

void
imgStatusTracker::RecordCancel()
{
  if (!(mImageStatus & imgIRequest::STATUS_LOAD_PARTIAL))
    mImageStatus |= imgIRequest::STATUS_ERROR;
}

void
imgStatusTracker::RecordLoaded()
{
  mState |= stateRequestStarted | stateHasSize | stateRequestStopped;
  mImageStatus |= imgIRequest::STATUS_SIZE_AVAILABLE | imgIRequest::STATUS_LOAD_COMPLETE;
  mHadLastPart = PR_TRUE;
}

void
imgStatusTracker::RecordDecoded()
{
  mState |= stateDecodeStarted | stateDecodeStopped | stateFrameStopped;
  mImageStatus |= imgIRequest::STATUS_FRAME_COMPLETE | imgIRequest::STATUS_DECODE_COMPLETE;
}

/* non-virtual imgIDecoderObserver methods */
void
imgStatusTracker::RecordStartDecode()
{
  mState |= stateDecodeStarted;
}

void
imgStatusTracker::SendStartDecode(imgRequestProxy* aProxy)
{
  if (!aProxy->NotificationsDeferred())
    aProxy->OnStartDecode();
}

void
imgStatusTracker::RecordStartContainer(imgIContainer* aContainer)
{
  mState |= stateHasSize;
  mImageStatus |= imgIRequest::STATUS_SIZE_AVAILABLE;
}

void
imgStatusTracker::SendStartContainer(imgRequestProxy* aProxy, imgIContainer* aContainer)
{
  // We only want to send onStartContainer once, but we might get multiple
  // OnStartContainer calls (e.g. from multipart/x-mixed-replace).
  PRBool alreadySent = (mState & stateHasSize) != 0;
  if (!alreadySent && !aProxy->NotificationsDeferred())
    aProxy->OnStartContainer(aContainer);
}

void
imgStatusTracker::RecordStartFrame(PRUint32 aFrame)
{
  // no bookkeeping necessary here - this is implied by imgIContainer's number
  // of frames
}

void
imgStatusTracker::SendStartFrame(imgRequestProxy* aProxy, PRUint32 aFrame)
{
  if (!aProxy->NotificationsDeferred())
    aProxy->OnStartFrame(aFrame);
}

void
imgStatusTracker::RecordDataAvailable(PRBool aCurrentFrame, const nsIntRect* aRect)
{
  // no bookkeeping necessary here - this is implied by imgIContainer's
  // number of frames and frame rect
}

void
imgStatusTracker::SendDataAvailable(imgRequestProxy* aProxy, PRBool aCurrentFrame,
                                         const nsIntRect* aRect)
{
  if (!aProxy->NotificationsDeferred())
    aProxy->OnDataAvailable(aCurrentFrame, aRect);
}


void
imgStatusTracker::RecordStopFrame(PRUint32 aFrame)
{
  mState |= stateFrameStopped;
  mImageStatus |= imgIRequest::STATUS_FRAME_COMPLETE;
}

void
imgStatusTracker::SendStopFrame(imgRequestProxy* aProxy, PRUint32 aFrame)
{
  if (!aProxy->NotificationsDeferred())
    aProxy->OnStopFrame(aFrame);
}

void
imgStatusTracker::RecordStopContainer(imgIContainer* aContainer)
{
  // No-op: see imgRequest::OnStopDecode for more information
}

void
imgStatusTracker::SendStopContainer(imgRequestProxy* aProxy, imgIContainer* aContainer)
{
  // No-op: see imgRequest::OnStopDecode for more information
}

void
imgStatusTracker::RecordStopDecode(nsresult aStatus, const PRUnichar* statusArg)
{
  mState |= stateDecodeStopped;

  if (NS_SUCCEEDED(aStatus))
    mImageStatus |= imgIRequest::STATUS_DECODE_COMPLETE;
  // If we weren't successful, clear all success status bits and set error.
  else
    mImageStatus = imgIRequest::STATUS_ERROR;
}

void
imgStatusTracker::SendStopDecode(imgRequestProxy* aProxy, nsresult aStatus,
                                 const PRUnichar* statusArg)
{
  // See imgRequest::OnStopDecode for more information on why we call
  // OnStopContainer from here this, and why imgRequestProxy::OnStopDecode() is
  // called from OnStopRequest() and SyncNotify().
  if (!aProxy->NotificationsDeferred())
    aProxy->OnStopContainer(mImage);
}

void
imgStatusTracker::RecordDiscard()
{
  // Clear the state bits we no longer deserve.
  PRUint32 stateBitsToClear = stateDecodeStarted | stateDecodeStopped;
  mState &= ~stateBitsToClear;

  // Clear the status bits we no longer deserve.
  PRUint32 statusBitsToClear = imgIRequest::STATUS_FRAME_COMPLETE
                               | imgIRequest::STATUS_DECODE_COMPLETE;
  mImageStatus &= ~statusBitsToClear;
}

void
imgStatusTracker::SendDiscard(imgRequestProxy* aProxy)
{
  if (!aProxy->NotificationsDeferred())
    aProxy->OnDiscard();
}

/* non-virtual imgIContainerObserver methods */
void
imgStatusTracker::RecordFrameChanged(imgIContainer* aContainer, nsIntRect* aDirtyRect)
{
  // no bookkeeping necessary here - this is only for in-frame updates, which we
  // don't fire while we're recording
}

void
imgStatusTracker::SendFrameChanged(imgRequestProxy* aProxy, imgIContainer* aContainer,
                                   nsIntRect* aDirtyRect)
{
  if (!aProxy->NotificationsDeferred())
    aProxy->FrameChanged(aContainer, aDirtyRect);
}

/* non-virtual sort-of-nsIRequestObserver methods */
void
imgStatusTracker::RecordStartRequest()
{
  // We're starting a new load, so clear any status and state bits indicating
  // load/decode
  mImageStatus &= ~imgIRequest::STATUS_LOAD_PARTIAL;
  mImageStatus &= ~imgIRequest::STATUS_LOAD_COMPLETE;
  mImageStatus &= ~imgIRequest::STATUS_FRAME_COMPLETE;
  mState &= ~stateRequestStarted;
  mState &= ~stateDecodeStarted;
  mState &= ~stateDecodeStopped;
  mState &= ~stateRequestStopped;

  mState |= stateRequestStarted;
}

void
imgStatusTracker::SendStartRequest(imgRequestProxy* aProxy)
{
  if (!aProxy->NotificationsDeferred())
    aProxy->OnStartRequest();
}

void
imgStatusTracker::RecordStopRequest(PRBool aLastPart, nsresult aStatus)
{
  mHadLastPart = aLastPart;
  mState |= stateRequestStopped;

  // If we were successful in loading, note that the image is complete.
  if (NS_SUCCEEDED(aStatus))
    mImageStatus |= imgIRequest::STATUS_LOAD_COMPLETE;
}

void
imgStatusTracker::SendStopRequest(imgRequestProxy* aProxy, PRBool aLastPart, nsresult aStatus)
{
  // See bug 505385 and imgRequest::OnStopDecode for more information on why
  // OnStopDecode is called with OnStopRequest.
  if (!aProxy->NotificationsDeferred()) {
    aProxy->OnStopDecode(GetResultFromImageStatus(mImageStatus), nsnull);
    aProxy->OnStopRequest(aLastPart);
  }
}
