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
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Olli Pettay <Olli.Pettay@helsinki.fi> (original author)
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

#include "imgContainerRequest.h"
#include "ImageErrors.h"
#include "imgRequest.h"

NS_IMPL_ISUPPORTS2(imgContainerRequest, imgIRequest, nsIRequest)

imgContainerRequest::imgContainerRequest(imgIContainer* aImage,
                                         nsIURI* aURI,
                                         PRUint32 aImageStatus,
                                         PRUint32 aState,
                                         nsIPrincipal* aPrincipal)
: mImage(aImage), mURI(aURI), mPrincipal(aPrincipal), mImageStatus(aImageStatus),
  mState(aState), mLocksHeld(0)
{
#ifdef DEBUG
  PRUint32 numFrames = 0;
  if (aImage) {
    aImage->GetNumFrames(&numFrames);
  }
  NS_ABORT_IF_FALSE(!aImage || numFrames == 1,
                    "Shouldn't have image with more than one frame!");
#endif
}

imgContainerRequest::~imgContainerRequest()
{
  if (mImage) {
    while (mLocksHeld) {
      UnlockImage();
    }
  }
}

NS_IMETHODIMP
imgContainerRequest::GetName(nsACString& aName)
{
  aName.Truncate();
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
imgContainerRequest::IsPending(PRBool* _retval)
{
  *_retval = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
imgContainerRequest::GetStatus(nsresult* aStatus)
{
  *aStatus = NS_OK;
  return NS_OK;
}

NS_IMETHODIMP
imgContainerRequest::Cancel(nsresult aStatus)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
imgContainerRequest::Suspend()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
imgContainerRequest::Resume()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
imgContainerRequest::GetLoadGroup(nsILoadGroup** aLoadGroup)
{
  *aLoadGroup = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
imgContainerRequest::SetLoadGroup(nsILoadGroup* aLoadGroup)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
imgContainerRequest::GetLoadFlags(nsLoadFlags* aLoadFlags)
{
  *aLoadFlags = LOAD_NORMAL;
  return NS_OK;
}

NS_IMETHODIMP
imgContainerRequest::SetLoadFlags(nsLoadFlags aLoadFlags)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
imgContainerRequest::GetImage(imgIContainer** aImage)
{
  NS_IF_ADDREF(*aImage = mImage);
  return NS_OK;
}

NS_IMETHODIMP
imgContainerRequest::GetImageStatus(PRUint32* aImageStatus)
{
  *aImageStatus = mImageStatus;
  return NS_OK;
}

NS_IMETHODIMP
imgContainerRequest::GetURI(nsIURI** aURI)
{
  NS_IF_ADDREF(*aURI = mURI);
  return NS_OK;
}

NS_IMETHODIMP
imgContainerRequest::GetDecoderObserver(imgIDecoderObserver** aDecoderObserver)
{
  NS_IF_ADDREF(*aDecoderObserver = mObserver);
  return NS_OK;
}

NS_IMETHODIMP
imgContainerRequest::GetMimeType(char** aMimeType)
{
  *aMimeType = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
imgContainerRequest::Clone(imgIDecoderObserver* aObserver, imgIRequest** _retval)
{
  imgContainerRequest* req =
    new imgContainerRequest(mImage, mURI, mImageStatus, mState, mPrincipal);
  if (!req) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*_retval = req);
  req->mObserver = aObserver;

  nsCOMPtr<imgIDecoderObserver> kungFuDeathGrip(aObserver);

  // Keep these notifications in sync with imgRequest::NotifyProxyListener!

  // OnStartRequest
  if (req->mState & stateRequestStarted)
    aObserver->OnStartRequest(req);

  // OnStartContainer
  if (req->mState & stateHasSize)
    aObserver->OnStartContainer(req, req->mImage);

  // OnStartDecode
  if (req->mState & stateDecodeStarted)
    aObserver->OnStartDecode(req);

  // Send frame messages (OnStartFrame, OnDataAvailable, OnStopFrame)
  PRUint32 nframes = 0;
  if (req->mImage)
    req->mImage->GetNumFrames(&nframes);

  if (nframes > 0) {
    PRUint32 frame;
    req->mImage->GetCurrentFrameIndex(&frame);
    aObserver->OnStartFrame(req, frame);

    // OnDataAvailable
    // XXX - Should only send partial rects here, but that needs to
    // wait until we fix up the observer interface
    nsIntRect r;
    req->mImage->GetCurrentFrameRect(r);
    aObserver->OnDataAvailable(req, frame, &r);

    if (req->mState & stateRequestStopped)
      aObserver->OnStopFrame(req, frame);
  }

  // Reseting image animation isn't needed here.

  if (req->mState & stateRequestStopped) {
    aObserver->OnStopContainer(req, req->mImage);
    aObserver->OnStopDecode(req,
                            imgRequest::GetResultFromImageStatus(req->mImageStatus),
                            nsnull);
    aObserver->OnStopRequest(req, PR_TRUE);
  }

  return NS_OK;
}

NS_IMETHODIMP
imgContainerRequest::GetImagePrincipal(nsIPrincipal** aImagePrincipal)
{
  NS_IF_ADDREF(*aImagePrincipal = mPrincipal);
  return NS_OK;
}

NS_IMETHODIMP
imgContainerRequest::CancelAndForgetObserver(nsresult aStatus)
{
  return NS_OK;
}

NS_IMETHODIMP
imgContainerRequest::RequestDecode()
{
  return mImage ? mImage->RequestDecode() : NS_OK;
}

NS_IMETHODIMP
imgContainerRequest::LockImage()
{
  if (mImage) {
    ++mLocksHeld;
    return mImage->LockImage();
  }
  return NS_OK;
}

NS_IMETHODIMP
imgContainerRequest::UnlockImage()
{
  if (mImage) {
    NS_ABORT_IF_FALSE(mLocksHeld > 0, "calling unlock but no locks!");
    --mLocksHeld;
    return mImage->UnlockImage();
  }
  return NS_OK;
}

NS_IMETHODIMP
imgContainerRequest::GetStaticRequest(imgIRequest** aReturn)
{
  NS_ADDREF(*aReturn = this);
  return NS_OK;
}

