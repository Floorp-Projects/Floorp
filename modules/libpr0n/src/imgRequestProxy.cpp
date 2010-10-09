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
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <stuart@mozilla.com>
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

#include "imgRequestProxy.h"

#include "nsIInputStream.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIMultiPartChannel.h"

#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"

#include "Image.h"
#include "ImageErrors.h"
#include "ImageLogging.h"

#include "nspr.h"

using namespace mozilla::imagelib;

NS_IMPL_ISUPPORTS4(imgRequestProxy, imgIRequest, nsIRequest,
                   nsISupportsPriority, nsISecurityInfoProvider)

imgRequestProxy::imgRequestProxy() :
  mOwner(nsnull),
  mURI(nsnull),
  mImage(nsnull),
  mPrincipal(nsnull),
  mListener(nsnull),
  mLoadFlags(nsIRequest::LOAD_NORMAL),
  mLockCount(0),
  mAnimationConsumers(0),
  mCanceled(PR_FALSE),
  mIsInLoadGroup(PR_FALSE),
  mListenerIsStrongRef(PR_FALSE),
  mDecodeRequested(PR_FALSE),
  mDeferNotifications(PR_FALSE),
  mSentStartContainer(PR_FALSE)
{
  /* member initializers and constructor code */

}

imgRequestProxy::~imgRequestProxy()
{
  /* destructor code */
  NS_PRECONDITION(!mListener, "Someone forgot to properly cancel this request!");

  // Unlock the image the proper number of times if we're holding locks on it.
  // Note that UnlockImage() decrements mLockCount each time it's called.
  while (mLockCount)
    UnlockImage();

  ClearAnimationConsumers();

  // Explicitly set mListener to null to ensure that the RemoveProxy
  // call below can't send |this| to an arbitrary listener while |this|
  // is being destroyed.  This is all belt-and-suspenders in view of the
  // above assert.
  NullOutListener();

  if (mOwner) {
    if (!mCanceled) {
      mCanceled = PR_TRUE;

      /* Call RemoveProxy with a successful status.  This will keep the
         channel, if still downloading data, from being canceled if 'this' is
         the last observer.  This allows the image to continue to download and
         be cached even if no one is using it currently.
         
         Passing false to aNotify means that we will still get
         OnStopRequest, if needed.
       */
      mOwner->RemoveProxy(this, NS_OK, PR_FALSE);
    }
  }
}

nsresult imgRequestProxy::Init(imgRequest* request, nsILoadGroup* aLoadGroup, Image* aImage,
                               nsIURI* aURI, imgIDecoderObserver* aObserver)
{
  NS_PRECONDITION(!mOwner && !mListener, "imgRequestProxy is already initialized");

  LOG_SCOPE_WITH_PARAM(gImgLog, "imgRequestProxy::Init", "request", request);

  NS_ABORT_IF_FALSE(mAnimationConsumers == 0, "Cannot have animation before Init");

  mOwner = request;
  mListener = aObserver;
  // Make sure to addref mListener before the AddProxy call below, since
  // that call might well want to release it if the imgRequest has
  // already seen OnStopRequest.
  if (mListener) {
    mListenerIsStrongRef = PR_TRUE;
    NS_ADDREF(mListener);
  }
  mLoadGroup = aLoadGroup;
  mImage = aImage;
  mURI = aURI;

  // Note: AddProxy won't send all the On* notifications immediately
  if (mOwner)
    mOwner->AddProxy(this);

  return NS_OK;
}

nsresult imgRequestProxy::ChangeOwner(imgRequest *aNewOwner)
{
  NS_PRECONDITION(mOwner, "Cannot ChangeOwner on a proxy without an owner!");

  // If we're holding locks, unlock the old image.
  // Note that UnlockImage decrements mLockCount each time it's called.
  PRUint32 oldLockCount = mLockCount;
  while (mLockCount)
    UnlockImage();

  // If we're holding animation requests, undo them.
  PRUint32 oldAnimationConsumers = mAnimationConsumers;
  ClearAnimationConsumers();

  // Even if we are cancelled, we MUST change our image, because the image
  // holds our status, and the status must always be correct.
  mImage = aNewOwner->mImage;

  // If we were locked, apply the locks here
  for (PRUint32 i = 0; i < oldLockCount; i++)
    LockImage();

  // If we had animation requests, apply them here
  for (PRUint32 i = 0; i < oldAnimationConsumers; i++)
    IncrementAnimationConsumers();

  if (mCanceled)
    return NS_OK;

  // Were we decoded before?
  PRBool wasDecoded = PR_FALSE;
  if (mImage &&
      (mImage->GetStatusTracker().GetImageStatus() &
       imgIRequest::STATUS_FRAME_COMPLETE)) {
    wasDecoded = PR_TRUE;
  }

  // Passing false to aNotify means that mListener will still get
  // OnStopRequest, if needed.
  mOwner->RemoveProxy(this, NS_IMAGELIB_CHANGING_OWNER, PR_FALSE);

  mOwner = aNewOwner;

  mOwner->AddProxy(this);

  // If we were decoded, or if we'd previously requested a decode, request a
  // decode on the new image
  if (wasDecoded || mDecodeRequested)
    mOwner->RequestDecode();

  return NS_OK;
}

void imgRequestProxy::AddToLoadGroup()
{
  NS_ASSERTION(!mIsInLoadGroup, "Whaa, we're already in the loadgroup!");

  if (!mIsInLoadGroup && mLoadGroup) {
    mLoadGroup->AddRequest(this, nsnull);
    mIsInLoadGroup = PR_TRUE;
  }
}

void imgRequestProxy::RemoveFromLoadGroup(PRBool releaseLoadGroup)
{
  if (!mIsInLoadGroup)
    return;

  /* calling RemoveFromLoadGroup may cause the document to finish
     loading, which could result in our death.  We need to make sure
     that we stay alive long enough to fight another battle... at
     least until we exit this function.
  */
  nsCOMPtr<imgIRequest> kungFuDeathGrip(this);

  mLoadGroup->RemoveRequest(this, nsnull, NS_OK);
  mIsInLoadGroup = PR_FALSE;

  if (releaseLoadGroup) {
    // We're done with the loadgroup, release it.
    mLoadGroup = nsnull;
  }
}


/**  nsIRequest / imgIRequest methods **/

/* readonly attribute wstring name; */
NS_IMETHODIMP imgRequestProxy::GetName(nsACString &aName)
{
  aName.Truncate();

  if (mURI)
    mURI->GetSpec(aName);

  return NS_OK;
}

/* boolean isPending (); */
NS_IMETHODIMP imgRequestProxy::IsPending(PRBool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute nsresult status; */
NS_IMETHODIMP imgRequestProxy::GetStatus(nsresult *aStatus)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void cancel (in nsresult status); */
NS_IMETHODIMP imgRequestProxy::Cancel(nsresult status)
{
  if (mCanceled)
    return NS_ERROR_FAILURE;

  LOG_SCOPE(gImgLog, "imgRequestProxy::Cancel");

  mCanceled = PR_TRUE;

  nsCOMPtr<nsIRunnable> ev = new imgCancelRunnable(this, status);
  return NS_DispatchToCurrentThread(ev);
}

void
imgRequestProxy::DoCancel(nsresult status)
{
  // Passing false to aNotify means that mListener will still get
  // OnStopRequest, if needed.
  if (mOwner)
    mOwner->RemoveProxy(this, status, PR_FALSE);

  NullOutListener();
}

/* void cancelAndForgetObserver (in nsresult aStatus); */
NS_IMETHODIMP imgRequestProxy::CancelAndForgetObserver(nsresult aStatus)
{
  if (mCanceled)
    return NS_ERROR_FAILURE;

  LOG_SCOPE(gImgLog, "imgRequestProxy::CancelAndForgetObserver");

  mCanceled = PR_TRUE;

  // Now cheat and make sure our removal from loadgroup happens async
  PRBool oldIsInLoadGroup = mIsInLoadGroup;
  mIsInLoadGroup = PR_FALSE;

  // Passing false to aNotify means that mListener will still get
  // OnStopRequest, if needed.
  if (mOwner)
    mOwner->RemoveProxy(this, aStatus, PR_FALSE);

  mIsInLoadGroup = oldIsInLoadGroup;

  if (mIsInLoadGroup) {
    nsCOMPtr<nsIRunnable> ev =
      NS_NewRunnableMethod(this, &imgRequestProxy::DoRemoveFromLoadGroup);
    NS_DispatchToCurrentThread(ev);
  }

  NullOutListener();

  return NS_OK;
}

/* void requestDecode (); */
NS_IMETHODIMP
imgRequestProxy::RequestDecode()
{
  if (!mOwner)
    return NS_ERROR_FAILURE;

  // Flag this, so we know to transfer the request if our owner changes
  mDecodeRequested = PR_TRUE;

  // Forward the request
  return mOwner->RequestDecode();
}

/* void lockImage (); */
NS_IMETHODIMP
imgRequestProxy::LockImage()
{
  mLockCount++;
  if (mImage)
    return mImage->LockImage();
  return NS_OK;
}

/* void unlockImage (); */
NS_IMETHODIMP
imgRequestProxy::UnlockImage()
{
  NS_ABORT_IF_FALSE(mLockCount > 0, "calling unlock but no locks!");

  mLockCount--;
  if (mImage)
    return mImage->UnlockImage();
  return NS_OK;
}

NS_IMETHODIMP
imgRequestProxy::IncrementAnimationConsumers()
{
  mAnimationConsumers++;
  if (mImage)
    mImage->IncrementAnimationConsumers();
  return NS_OK;
}

NS_IMETHODIMP
imgRequestProxy::DecrementAnimationConsumers()
{
  // We may get here if some responsible code called Increment,
  // then called us, but we have meanwhile called ClearAnimationConsumers
  // because we needed to get rid of them earlier (see
  // imgRequest::RemoveProxy), and hence have nothing left to
  // decrement. (In such a case we got rid of the animation consumers
  // early, but not the observer.)
  if (mAnimationConsumers > 0) {
    mAnimationConsumers--;
    if (mImage)
      mImage->DecrementAnimationConsumers();
  }
  return NS_OK;
}

void
imgRequestProxy::ClearAnimationConsumers()
{
  while (mAnimationConsumers > 0)
    DecrementAnimationConsumers();
}

/* void suspend (); */
NS_IMETHODIMP imgRequestProxy::Suspend()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void resume (); */
NS_IMETHODIMP imgRequestProxy::Resume()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute nsILoadGroup loadGroup */
NS_IMETHODIMP imgRequestProxy::GetLoadGroup(nsILoadGroup **loadGroup)
{
  NS_IF_ADDREF(*loadGroup = mLoadGroup.get());
  return NS_OK;
}
NS_IMETHODIMP imgRequestProxy::SetLoadGroup(nsILoadGroup *loadGroup)
{
  mLoadGroup = loadGroup;
  return NS_OK;
}

/* attribute nsLoadFlags loadFlags */
NS_IMETHODIMP imgRequestProxy::GetLoadFlags(nsLoadFlags *flags)
{
  *flags = mLoadFlags;
  return NS_OK;
}
NS_IMETHODIMP imgRequestProxy::SetLoadFlags(nsLoadFlags flags)
{
  mLoadFlags = flags;
  return NS_OK;
}

/**  imgIRequest methods **/

/* attribute imgIContainer image; */
NS_IMETHODIMP imgRequestProxy::GetImage(imgIContainer * *aImage)
{
  // It's possible that our owner has an image but hasn't notified us of it -
  // that'll happen if we get Canceled before the owner instantiates its image
  // (because Canceling unregisters us as a listener on mOwner). If we're
  // in that situation, just grab the image off of mOwner.
  imgIContainer* imageToReturn = mImage ? mImage : mOwner->mImage;

  if (!imageToReturn)
    return NS_ERROR_FAILURE;

  NS_ADDREF(*aImage = imageToReturn);

  return NS_OK;
}

/* readonly attribute unsigned long imageStatus; */
NS_IMETHODIMP imgRequestProxy::GetImageStatus(PRUint32 *aStatus)
{
  *aStatus = GetStatusTracker().GetImageStatus();

  return NS_OK;
}

/* readonly attribute nsIURI URI; */
NS_IMETHODIMP imgRequestProxy::GetURI(nsIURI **aURI)
{
  if (!mURI)
    return NS_ERROR_FAILURE;

  NS_ADDREF(*aURI = mURI);

  return NS_OK;
}

/* readonly attribute imgIDecoderObserver decoderObserver; */
NS_IMETHODIMP imgRequestProxy::GetDecoderObserver(imgIDecoderObserver **aDecoderObserver)
{
  *aDecoderObserver = mListener;
  NS_IF_ADDREF(*aDecoderObserver);
  return NS_OK;
}

/* readonly attribute string mimeType; */
NS_IMETHODIMP imgRequestProxy::GetMimeType(char **aMimeType)
{
  if (!mOwner)
    return NS_ERROR_FAILURE;

  const char *type = mOwner->GetMimeType();
  if (!type)
    return NS_ERROR_FAILURE;

  *aMimeType = NS_strdup(type);

  return NS_OK;
}

NS_IMETHODIMP imgRequestProxy::Clone(imgIDecoderObserver* aObserver,
                                     imgIRequest** aClone)
{
  NS_PRECONDITION(aClone, "Null out param");

  LOG_SCOPE(gImgLog, "imgRequestProxy::Clone");

  *aClone = nsnull;
  nsRefPtr<imgRequestProxy> clone = new imgRequestProxy();

  // It is important to call |SetLoadFlags()| before calling |Init()| because
  // |Init()| adds the request to the loadgroup.
  // When a request is added to a loadgroup, its load flags are merged
  // with the load flags of the loadgroup.
  // XXXldb That's not true anymore.  Stuff from imgLoader adds the
  // request to the loadgroup.
  clone->SetLoadFlags(mLoadFlags);
  nsresult rv = clone->Init(mOwner, mLoadGroup,
                            mImage ? mImage : mOwner->mImage,
                            mURI, aObserver);
  if (NS_FAILED(rv))
    return rv;

  clone->SetPrincipal(mPrincipal);

  // Assign to *aClone before calling Notify so that if the caller expects to
  // only be notified for requests it's already holding pointers to it won't be
  // surprised.
  NS_ADDREF(*aClone = clone);

  // This is wrong!!! We need to notify asynchronously, but there's code that
  // assumes that we don't. This will be fixed in bug 580466.
  clone->SyncNotifyListener();

  return NS_OK;
}

/* readonly attribute nsIPrincipal imagePrincipal; */
NS_IMETHODIMP imgRequestProxy::GetImagePrincipal(nsIPrincipal **aPrincipal)
{
  if (!mPrincipal)
    return NS_ERROR_FAILURE;

  NS_ADDREF(*aPrincipal = mPrincipal);

  return NS_OK;
}

/** nsISupportsPriority methods **/

NS_IMETHODIMP imgRequestProxy::GetPriority(PRInt32 *priority)
{
  NS_ENSURE_STATE(mOwner);
  *priority = mOwner->Priority();
  return NS_OK;
}

NS_IMETHODIMP imgRequestProxy::SetPriority(PRInt32 priority)
{
  NS_ENSURE_STATE(mOwner && !mCanceled);
  mOwner->AdjustPriority(this, priority - mOwner->Priority());
  return NS_OK;
}

NS_IMETHODIMP imgRequestProxy::AdjustPriority(PRInt32 priority)
{
  NS_ENSURE_STATE(mOwner && !mCanceled);
  mOwner->AdjustPriority(this, priority);
  return NS_OK;
}

/** nsISecurityInfoProvider methods **/

NS_IMETHODIMP imgRequestProxy::GetSecurityInfo(nsISupports** _retval)
{
  if (mOwner)
    return mOwner->GetSecurityInfo(_retval);

  *_retval = nsnull;
  return NS_OK;
}

NS_IMETHODIMP imgRequestProxy::GetHasTransferredData(PRBool* hasData)
{
  if (mOwner) {
    *hasData = mOwner->HasTransferredData();
  } else {
    // The safe thing to do is to claim we have data
    *hasData = PR_TRUE;
  }
  return NS_OK;
}

/** imgIContainerObserver methods **/

void imgRequestProxy::FrameChanged(imgIContainer *container,
                                   const nsIntRect *dirtyRect)
{
  LOG_FUNC(gImgLog, "imgRequestProxy::FrameChanged");

  if (mListener && !mCanceled) {
    // Hold a ref to the listener while we call it, just in case.
    nsCOMPtr<imgIDecoderObserver> kungFuDeathGrip(mListener);
    mListener->FrameChanged(container, dirtyRect);
  }
}

/** imgIDecoderObserver methods **/

void imgRequestProxy::OnStartDecode()
{
  LOG_FUNC(gImgLog, "imgRequestProxy::OnStartDecode");

  if (mListener && !mCanceled) {
    // Hold a ref to the listener while we call it, just in case.
    nsCOMPtr<imgIDecoderObserver> kungFuDeathGrip(mListener);
    mListener->OnStartDecode(this);
  }
}

void imgRequestProxy::OnStartContainer(imgIContainer *image)
{
  LOG_FUNC(gImgLog, "imgRequestProxy::OnStartContainer");

  if (mListener && !mCanceled && !mSentStartContainer) {
    // Hold a ref to the listener while we call it, just in case.
    nsCOMPtr<imgIDecoderObserver> kungFuDeathGrip(mListener);
    mListener->OnStartContainer(this, image);
    mSentStartContainer = PR_TRUE;
  }
}

void imgRequestProxy::OnStartFrame(PRUint32 frame)
{
  LOG_FUNC(gImgLog, "imgRequestProxy::OnStartFrame");

  if (mListener && !mCanceled) {
    // Hold a ref to the listener while we call it, just in case.
    nsCOMPtr<imgIDecoderObserver> kungFuDeathGrip(mListener);
    mListener->OnStartFrame(this, frame);
  }
}

void imgRequestProxy::OnDataAvailable(PRBool aCurrentFrame, const nsIntRect * rect)
{
  LOG_FUNC(gImgLog, "imgRequestProxy::OnDataAvailable");

  if (mListener && !mCanceled) {
    // Hold a ref to the listener while we call it, just in case.
    nsCOMPtr<imgIDecoderObserver> kungFuDeathGrip(mListener);
    mListener->OnDataAvailable(this, aCurrentFrame, rect);
  }
}

void imgRequestProxy::OnStopFrame(PRUint32 frame)
{
  LOG_FUNC(gImgLog, "imgRequestProxy::OnStopFrame");

  if (mListener && !mCanceled) {
    // Hold a ref to the listener while we call it, just in case.
    nsCOMPtr<imgIDecoderObserver> kungFuDeathGrip(mListener);
    mListener->OnStopFrame(this, frame);
  }
}

void imgRequestProxy::OnStopContainer(imgIContainer *image)
{
  LOG_FUNC(gImgLog, "imgRequestProxy::OnStopContainer");

  if (mListener && !mCanceled) {
    // Hold a ref to the listener while we call it, just in case.
    nsCOMPtr<imgIDecoderObserver> kungFuDeathGrip(mListener);
    mListener->OnStopContainer(this, image);
  }
}

void imgRequestProxy::OnStopDecode(nsresult status, const PRUnichar *statusArg)
{
  LOG_FUNC(gImgLog, "imgRequestProxy::OnStopDecode");

  if (mListener && !mCanceled) {
    // Hold a ref to the listener while we call it, just in case.
    nsCOMPtr<imgIDecoderObserver> kungFuDeathGrip(mListener);
    mListener->OnStopDecode(this, status, statusArg);
  }
}

void imgRequestProxy::OnDiscard()
{
  LOG_FUNC(gImgLog, "imgRequestProxy::OnDiscard");

  if (mListener && !mCanceled) {
    // Hold a ref to the listener while we call it, just in case.
    nsCOMPtr<imgIDecoderObserver> kungFuDeathGrip(mListener);
    mListener->OnDiscard(this);
  }
}

void imgRequestProxy::OnStartRequest()
{
#ifdef PR_LOGGING
  nsCAutoString name;
  GetName(name);
  LOG_FUNC_WITH_PARAM(gImgLog, "imgRequestProxy::OnStartRequest", "name", name.get());
#endif

  // Notify even if mCanceled, since OnStartRequest is guaranteed by the
  // nsIStreamListener contract so it makes sense to do the same here.
  if (mListener) {
    // Hold a ref to the listener while we call it, just in case.
    nsCOMPtr<imgIDecoderObserver> kungFuDeathGrip(mListener);
    mListener->OnStartRequest(this);
  }
}

void imgRequestProxy::OnStopRequest(PRBool lastPart)
{
#ifdef PR_LOGGING
  nsCAutoString name;
  GetName(name);
  LOG_FUNC_WITH_PARAM(gImgLog, "imgRequestProxy::OnStopRequest", "name", name.get());
#endif
  // There's all sorts of stuff here that could kill us (the OnStopRequest call
  // on the listener, the removal from the loadgroup, the release of the
  // listener, etc).  Don't let them do it.
  nsCOMPtr<imgIRequest> kungFuDeathGrip(this);

  if (mListener) {
    // Hold a ref to the listener while we call it, just in case.
    nsCOMPtr<imgIDecoderObserver> kungFuDeathGrip(mListener);
    mListener->OnStopRequest(this, lastPart);
  }

  // If we're expecting more data from a multipart channel, re-add ourself
  // to the loadgroup so that the document doesn't lose track of the load.
  // If the request is already a background request and there's more data
  // coming, we can just leave the request in the loadgroup as-is.
  if (lastPart || (mLoadFlags & nsIRequest::LOAD_BACKGROUND) == 0) {
    RemoveFromLoadGroup(lastPart);
    // More data is coming, so change the request to be a background request
    // and put it back in the loadgroup.
    if (!lastPart) {
      mLoadFlags |= nsIRequest::LOAD_BACKGROUND;
      AddToLoadGroup();
    }
  }

  if (mListenerIsStrongRef) {
    NS_PRECONDITION(mListener, "How did that happen?");
    // Drop our strong ref to the listener now that we're done with
    // everything.  Note that this can cancel us and other fun things
    // like that.  Don't add anything in this method after this point.
    imgIDecoderObserver* obs = mListener;
    mListenerIsStrongRef = PR_FALSE;
    NS_RELEASE(obs);
  }
}

void imgRequestProxy::NullOutListener()
{
  // If we have animation consumers, then they don't matter anymore
  if (mListener)
    ClearAnimationConsumers();

  if (mListenerIsStrongRef) {
    // Releasing could do weird reentery stuff, so just play it super-safe
    nsCOMPtr<imgIDecoderObserver> obs;
    obs.swap(mListener);
    mListenerIsStrongRef = PR_FALSE;
  } else {
    mListener = nsnull;
  }
}

NS_IMETHODIMP
imgRequestProxy::GetStaticRequest(imgIRequest** aReturn)
{
  *aReturn = nsnull;

  PRBool animated;
  if (!mImage || (NS_SUCCEEDED(mImage->GetAnimated(&animated)) && !animated)) {
    // Early exit - we're not animated, so we don't have to do anything.
    NS_ADDREF(*aReturn = this);
    return NS_OK;
  }

  // We are animated. We need to extract the current frame from this image.
  PRInt32 w = 0;
  PRInt32 h = 0;
  mImage->GetWidth(&w);
  mImage->GetHeight(&h);
  nsIntRect rect(0, 0, w, h);
  nsCOMPtr<imgIContainer> currentFrame;
  nsresult rv = mImage->ExtractFrame(imgIContainer::FRAME_CURRENT, rect,
                                     imgIContainer::FLAG_SYNC_DECODE,
                                     getter_AddRefs(currentFrame));
  if (NS_FAILED(rv))
    return rv;

  nsRefPtr<Image> frame = static_cast<Image*>(currentFrame.get());

  // Create a static imgRequestProxy with our new extracted frame.
  nsRefPtr<imgRequestProxy> req = new imgRequestProxy();
  req->Init(nsnull, nsnull, frame, mURI, nsnull);
  req->SetPrincipal(mPrincipal);

  NS_ADDREF(*aReturn = req);

  return NS_OK;
}

void imgRequestProxy::SetPrincipal(nsIPrincipal *aPrincipal)
{
  mPrincipal = aPrincipal;
}

void imgRequestProxy::NotifyListener()
{
  // It would be nice to notify the observer directly in the status tracker
  // instead of through the proxy, but there are several places we do extra
  // processing when we receive notifications (like OnStopRequest()), and we
  // need to check mCanceled everywhere too.

  if (mOwner) {
    // Send the notifications to our listener asynchronously.
    GetStatusTracker().Notify(mOwner, this);
  } else {
    // We don't have an imgRequest, so we can only notify the clone of our
    // current state, but we still have to do that asynchronously.
    NS_ABORT_IF_FALSE(mImage,
                      "if we have no imgRequest, we should have an Image");
    mImage->GetStatusTracker().NotifyCurrentState(this);
  }
}

void imgRequestProxy::SyncNotifyListener()
{
  // It would be nice to notify the observer directly in the status tracker
  // instead of through the proxy, but there are several places we do extra
  // processing when we receive notifications (like OnStopRequest()), and we
  // need to check mCanceled everywhere too.

  GetStatusTracker().SyncNotify(this);
}

void
imgRequestProxy::SetImage(Image* aImage)
{
  NS_ABORT_IF_FALSE(aImage,  "Setting null image");
  NS_ABORT_IF_FALSE(!mImage, "Setting image when we already have one");

  mImage = aImage;

  // Apply any locks we have
  for (PRUint32 i = 0; i < mLockCount; ++i)
    mImage->LockImage();

  // Apply any animation consumers we have
  for (PRUint32 i = 0; i < mAnimationConsumers; i++)
    mImage->IncrementAnimationConsumers();
}

imgStatusTracker&
imgRequestProxy::GetStatusTracker()
{
  // NOTE: It's possible that our mOwner has an Image that it didn't notify
  // us about, if we were Canceled before its Image was constructed.
  // (Canceling removes us as an observer, so mOwner has no way to notify us).
  // That's why this method uses mOwner->GetStatusTracker() instead of just
  // mOwner->mStatusTracker -- we might have a null mImage and yet have an
  // mOwner with a non-null mImage (and a null mStatusTracker pointer).
  return mImage ? mImage->GetStatusTracker() : mOwner->GetStatusTracker();
}
