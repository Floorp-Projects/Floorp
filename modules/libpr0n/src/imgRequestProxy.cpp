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
 *   Stuart Parmenter <pavlov@netscape.com>
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

#include "nsAutoLock.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"

#include "ImageErrors.h"
#include "ImageLogging.h"

#include "nspr.h"


NS_IMPL_THREADSAFE_ISUPPORTS2(imgRequestProxy, imgIRequest, nsIRequest)

imgRequestProxy::imgRequestProxy() :
  mOwner(nsnull),
  mListener(nsnull),
  mLoadFlags(nsIRequest::LOAD_NORMAL),
  mCanceled(PR_FALSE),
  mIsInLoadGroup(PR_FALSE),
  mLock(nsnull)
{
  /* member initializers and constructor code */

  mLock = PR_NewLock();
}

imgRequestProxy::~imgRequestProxy()
{
  /* destructor code */
  NS_PRECONDITION(!mListener, "Someone forgot to properly cancel this request!");

  if (mOwner) {
    if (!mCanceled) {
      PR_Lock(mLock);

      mCanceled = PR_TRUE;

      /* set mListener to null so that we don't forward any callbacks that
         RemoveProxy might generate
       */
      mListener = nsnull;

      PR_Unlock(mLock);

      /* Call RemoveProxy with a successful status.  This will keep the
         channel, if still downloading data, from being canceled if 'this' is
         the last observer.  This allows the image to continue to download and
         be cached even if no one is using it currently.
       */
      mOwner->RemoveProxy(this, NS_OK, PR_FALSE);
    }

    NS_RELEASE(mOwner);
  }

  PR_DestroyLock(mLock);
}



nsresult imgRequestProxy::Init(imgRequest *request, nsILoadGroup *aLoadGroup, imgIDecoderObserver *aObserver)
{
  NS_PRECONDITION(request, "no request");
  if (!request)
    return NS_ERROR_NULL_POINTER;

  LOG_SCOPE_WITH_PARAM(gImgLog, "imgRequestProxy::Init", "request", request);

  PR_Lock(mLock);

  mOwner = request;
  NS_ADDREF(mOwner);

  mListener = aObserver;

  mLoadGroup = aLoadGroup;

  PR_Unlock(mLock);

  request->AddProxy(this, PR_FALSE); // Pass PR_FALSE here so that AddProxy doesn't send all the On* notifications immediatly

  return NS_OK;
}

nsresult imgRequestProxy::ChangeOwner(imgRequest *aNewOwner)
{
  if (mCanceled)
    return NS_OK;

  PR_Lock(mLock);

  mOwner->RemoveProxy(this, NS_IMAGELIB_CHANGING_OWNER, PR_FALSE);
  NS_RELEASE(mOwner);

  mOwner = aNewOwner;
  NS_ADDREF(mOwner);

  mOwner->AddProxy(this, PR_FALSE);

  PR_Unlock(mLock);

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

void imgRequestProxy::RemoveFromLoadGroup()
{
  if (!mIsInLoadGroup)
    return;

  /* calling RemoveFromLoadGroup may cause the document to finish
     loading, which could result in our death.  We need to make sure
     that we stay alive long enough to fight another battle... at
     least until we exit this function.
  */
  nsCOMPtr<imgIRequest> kungFuDeathGrip(this);

  mLoadGroup->RemoveRequest(this, NS_OK, nsnull);
  mIsInLoadGroup = PR_FALSE;

  // We're done with the loadgroup, release it.
  mLoadGroup = nsnull;
}


/**  nsIRequest / imgIRequest methods **/

/* readonly attribute wstring name; */
NS_IMETHODIMP imgRequestProxy::GetName(nsACString &aName)
{
  aName.Truncate();
  if (mOwner) {
    nsCOMPtr<nsIURI> uri;
    mOwner->GetURI(getter_AddRefs(uri));
    if (uri)
      uri->GetSpec(aName);
  }
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
  if (mCanceled || !mOwner)
    return NS_ERROR_FAILURE;

  LOG_SCOPE(gImgLog, "imgRequestProxy::Cancel");

  PR_Lock(mLock);

  mCanceled = PR_TRUE;
  mListener = nsnull;

  PR_Unlock(mLock);

  mOwner->RemoveProxy(this, status, PR_FALSE);

  return NS_OK;
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
  nsAutoLock lock(mLock);
  NS_IF_ADDREF(*loadGroup = mLoadGroup.get());
  return NS_OK;
}
NS_IMETHODIMP imgRequestProxy::SetLoadGroup(nsILoadGroup *loadGroup)
{
  nsAutoLock lock(mLock);
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
  if (!mOwner)
    return NS_ERROR_FAILURE;

  nsAutoLock lock(mLock);
  mOwner->GetImage(aImage);
  return NS_OK;
}

/* readonly attribute unsigned long imageStatus; */
NS_IMETHODIMP imgRequestProxy::GetImageStatus(PRUint32 *aStatus)
{
  if (!mOwner) {
    *aStatus = imgIRequest::STATUS_ERROR;
    return NS_ERROR_FAILURE;
  }

  nsAutoLock lock(mLock);
  *aStatus = mOwner->GetImageStatus();
  return NS_OK;
}

/* readonly attribute nsIURI URI; */
NS_IMETHODIMP imgRequestProxy::GetURI(nsIURI **aURI)
{
  if (!mOwner)
    return NS_ERROR_FAILURE;

  nsAutoLock lock(mLock);
  return mOwner->GetURI(aURI);
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

  *aMimeType = nsCRT::strdup(type);

  return NS_OK;
}

NS_IMETHODIMP imgRequestProxy::Clone(imgIDecoderObserver* aObserver,
                                     imgIRequest** aClone)
{
  NS_PRECONDITION(aClone, "Null out param");
  *aClone = nsnull;
  imgRequestProxy* clone = new imgRequestProxy();
  if (!clone) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  NS_ADDREF(clone);

  // It is important to call |SetLoadFlags()| before calling |Init()| because
  // |Init()| adds the request to the loadgroup.
  clone->SetLoadFlags(mLoadFlags);
  nsresult rv = clone->Init(mOwner, mLoadGroup, aObserver);
  if (NS_FAILED(rv)) {
    NS_RELEASE(clone);
    return rv;
  }

  // Assign to *aClone before calling NotifyProxyListener so that if
  // the caller expects to only be notified for requests it's already
  // holding pointers to it won't be surprised.
  *aClone = clone;

  // Send the notifications to the clone's observer
  mOwner->NotifyProxyListener(clone);

  return NS_OK;
}

/** imgIContainerObserver methods **/

void imgRequestProxy::FrameChanged(imgIContainer *container, gfxIImageFrame *newframe, nsRect * dirtyRect)
{
  LOG_FUNC(gImgLog, "imgRequestProxy::FrameChanged");

  if (mListener) {
    // Hold a ref to the listener while we call it, just in case.
    nsCOMPtr<imgIDecoderObserver> kungFuDeathGrip(mListener);
    mListener->FrameChanged(container, newframe, dirtyRect);
  }
}

/** imgIDecoderObserver methods **/

void imgRequestProxy::OnStartDecode()
{
  LOG_FUNC(gImgLog, "imgRequestProxy::OnStartDecode");

  if (mListener) {
    // Hold a ref to the listener while we call it, just in case.
    nsCOMPtr<imgIDecoderObserver> kungFuDeathGrip(mListener);
    mListener->OnStartDecode(this);
  }
}

void imgRequestProxy::OnStartContainer(imgIContainer *image)
{
  LOG_FUNC(gImgLog, "imgRequestProxy::OnStartContainer");

  if (mListener) {
    // Hold a ref to the listener while we call it, just in case.
    nsCOMPtr<imgIDecoderObserver> kungFuDeathGrip(mListener);
    mListener->OnStartContainer(this, image);
  }
}

void imgRequestProxy::OnStartFrame(gfxIImageFrame *frame)
{
  LOG_FUNC(gImgLog, "imgRequestProxy::OnStartFrame");

  if (mListener) {
    // Hold a ref to the listener while we call it, just in case.
    nsCOMPtr<imgIDecoderObserver> kungFuDeathGrip(mListener);
    mListener->OnStartFrame(this, frame);
  }
}

void imgRequestProxy::OnDataAvailable(gfxIImageFrame *frame, const nsRect * rect)
{
  LOG_FUNC(gImgLog, "imgRequestProxy::OnDataAvailable");

  if (mListener) {
    // Hold a ref to the listener while we call it, just in case.
    nsCOMPtr<imgIDecoderObserver> kungFuDeathGrip(mListener);
    mListener->OnDataAvailable(this, frame, rect);
  }
}

void imgRequestProxy::OnStopFrame(gfxIImageFrame *frame)
{
  LOG_FUNC(gImgLog, "imgRequestProxy::OnStopFrame");

  if (mListener) {
    // Hold a ref to the listener while we call it, just in case.
    nsCOMPtr<imgIDecoderObserver> kungFuDeathGrip(mListener);
    mListener->OnStopFrame(this, frame);
  }
}

void imgRequestProxy::OnStopContainer(imgIContainer *image)
{
  LOG_FUNC(gImgLog, "imgRequestProxy::OnStopContainer");

  if (mListener) {
    // Hold a ref to the listener while we call it, just in case.
    nsCOMPtr<imgIDecoderObserver> kungFuDeathGrip(mListener);
    mListener->OnStopContainer(this, image);
  }
}

void imgRequestProxy::OnStopDecode(nsresult status, const PRUnichar *statusArg)
{
  LOG_FUNC(gImgLog, "imgRequestProxy::OnStopDecode");

  if (mListener) {
    // Hold a ref to the listener while we call it, just in case.
    nsCOMPtr<imgIDecoderObserver> kungFuDeathGrip(mListener);
    mListener->OnStopDecode(this, status, statusArg);
  }
}



void imgRequestProxy::OnStartRequest(nsIRequest *request, nsISupports *ctxt)
{
#ifdef PR_LOGGING
  nsCAutoString name;
  GetName(name);
  LOG_FUNC_WITH_PARAM(gImgLog, "imgRequestProxy::OnStartRequest", "name", name.get());
#endif
}

void imgRequestProxy::OnStopRequest(nsIRequest *request, nsISupports *ctxt, nsresult statusCode)
{
#ifdef PR_LOGGING
  nsCAutoString name;
  GetName(name);
  LOG_FUNC_WITH_PARAM(gImgLog, "imgRequestProxy::OnStopRequest", "name", name.get());
#endif

  RemoveFromLoadGroup();
}

