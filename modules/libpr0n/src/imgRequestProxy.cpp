/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
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
 * Copyright (C) 2001 Netscape Communications Corporation.
 * All Rights Reserved.
 * 
 * Contributor(s):
 *   Stuart Parmenter <pavlov@netscape.com>
 */

#include "imgRequestProxy.h"

#include "nsXPIDLString.h"

#include "nsIInputStream.h"
#include "imgILoader.h"
#include "nsIComponentManager.h"

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"

#include "imgRequest.h"

#include "nsString.h"

#include "DummyChannel.h"

#include "nspr.h"

#include "ImageLogging.h"

NS_IMPL_ISUPPORTS5(imgRequestProxy, imgIRequest, nsIRequest, imgIDecoderObserver, imgIContainerObserver, nsIStreamObserver)

imgRequestProxy::imgRequestProxy() :
  mCanceled(PR_FALSE)
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
}

imgRequestProxy::~imgRequestProxy()
{
  /* destructor code */

  // XXX pav
  // it isn't the job of the request proxy to cancel itself.
  // if your object goes away and you want to cancel the load, then do it yourself.

  // cancel here for now until i make this work right like the above comment
  Cancel(NS_ERROR_FAILURE);
}



nsresult imgRequestProxy::Init(imgRequest *request, nsILoadGroup *aLoadGroup, imgIDecoderObserver *aObserver, nsISupports *cx)
{
  PR_ASSERT(request);

  LOG_SCOPE_WITH_PARAM(gImgLog, "imgRequestProxy::Init", "request", request);

  mOwner = NS_STATIC_CAST(imgIRequest*, request);

  mObserver = aObserver;
  // XXX we should save off the thread we are getting called on here so that we can proxy all calls to mDecoder to it.

  mContext = cx;

  // XXX we should only create a channel, etc if the image isn't finished loading already.

  nsISupports *inst = nsnull;
  inst = new DummyChannel(this, aLoadGroup);
  NS_ADDREF(inst);
  nsresult res = inst->QueryInterface(NS_GET_IID(nsIChannel), getter_AddRefs(mDummyChannel));
  NS_RELEASE(inst);

  nsCOMPtr<nsILoadGroup> loadGroup;
  mDummyChannel->GetLoadGroup(getter_AddRefs(loadGroup));
  if (loadGroup) {
    loadGroup->AddRequest(mDummyChannel, cx);
  }

  request->AddObserver(this);

  return NS_OK;
}

/**  nsIRequest / imgIRequest methods **/

/* readonly attribute wstring name; */
NS_IMETHODIMP imgRequestProxy::GetName(PRUnichar * *aName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
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

  NS_ASSERTION(mOwner, "canceling request proxy twice");

  nsresult rv = NS_REINTERPRET_CAST(imgRequest*, mOwner.get())->RemoveObserver(this, status);

  mOwner = nsnull;

  return rv;
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

/**  imgIRequest methods **/

/* attribute imgIContainer image; */
NS_IMETHODIMP imgRequestProxy::GetImage(imgIContainer * *aImage)
{
  if (!mOwner)
    return NS_ERROR_FAILURE;

  return mOwner->GetImage(aImage);
}
NS_IMETHODIMP imgRequestProxy::SetImage(imgIContainer *aImage)
{
  return NS_ERROR_FAILURE;
}

/* readonly attribute unsigned long imageStatus; */
NS_IMETHODIMP imgRequestProxy::GetImageStatus(PRUint32 *aStatus)
{
  if (!mOwner) {
    *aStatus = imgIRequest::STATUS_ERROR;
    return NS_ERROR_FAILURE;
  }

  return mOwner->GetImageStatus(aStatus);
}

/* readonly attribute nsIURI URI; */
NS_IMETHODIMP imgRequestProxy::GetURI(nsIURI **aURI)
{
  if (!mOwner)
    return NS_ERROR_FAILURE;

  return mOwner->GetURI(aURI);
}

/* readonly attribute imgIDecoderObserver decoderObserver; */
NS_IMETHODIMP imgRequestProxy::GetDecoderObserver(imgIDecoderObserver **aDecoderObserver)
{
  *aDecoderObserver = mObserver;
  NS_IF_ADDREF(*aDecoderObserver);
  return NS_OK;
}


/** imgIContainerObserver methods **/

/* [noscript] void frameChanged (in imgIContainer container, in nsISupports cx, in gfxIImageFrame newframe, in nsRect dirtyRect); */
NS_IMETHODIMP imgRequestProxy::FrameChanged(imgIContainer *container, nsISupports *cx, gfxIImageFrame *newframe, nsRect * dirtyRect)
{
  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("[this=%p] imgRequestProxy::FrameChanged\n", this));

  if (mObserver)
    mObserver->FrameChanged(container, mContext, newframe, dirtyRect);

  return NS_OK;
}

/** imgIDecoderObserver methods **/

/* void onStartDecode (in imgIRequest request, in nsISupports cx); */
NS_IMETHODIMP imgRequestProxy::OnStartDecode(imgIRequest *request, nsISupports *cx)
{
  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("[this=%p] imgRequestProxy::OnStartDecode\n", this));

  if (mObserver)
    mObserver->OnStartDecode(this, mContext);

  return NS_OK;
}

/* void onStartContainer (in imgIRequest request, in nsISupports cx, in imgIContainer image); */
NS_IMETHODIMP imgRequestProxy::OnStartContainer(imgIRequest *request, nsISupports *cx, imgIContainer *image)
{
  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("[this=%p] imgRequestProxy::OnStartContainer\n", this));

  if (mObserver)
    mObserver->OnStartContainer(this, mContext, image);

  return NS_OK;
}

/* void onStartFrame (in imgIRequest request, in nsISupports cx, in gfxIImageFrame frame); */
NS_IMETHODIMP imgRequestProxy::OnStartFrame(imgIRequest *request, nsISupports *cx, gfxIImageFrame *frame)
{
  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("[this=%p] imgRequestProxy::OnStartFrame\n", this));

  if (mObserver)
    mObserver->OnStartFrame(this, mContext, frame);

  return NS_OK;
}

/* [noscript] void onDataAvailable (in imgIRequest request, in nsISupports cx, in gfxIImageFrame frame, [const] in nsRect rect); */
NS_IMETHODIMP imgRequestProxy::OnDataAvailable(imgIRequest *request, nsISupports *cx, gfxIImageFrame *frame, const nsRect * rect)
{
  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("[this=%p] imgRequestProxy::OnDataAvailable\n", this));

  if (mObserver)
    mObserver->OnDataAvailable(this, mContext, frame, rect);

  return NS_OK;
}

/* void onStopFrame (in imgIRequest request, in nsISupports cx, in gfxIImageFrame frame); */
NS_IMETHODIMP imgRequestProxy::OnStopFrame(imgIRequest *request, nsISupports *cx, gfxIImageFrame *frame)
{
  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("[this=%p] imgRequestProxy::OnStopFrame\n", this));

  if (mObserver)
    mObserver->OnStopFrame(this, mContext, frame);

  return NS_OK;
}

/* void onStopContainer (in imgIRequest request, in nsISupports cx, in imgIContainer image); */
NS_IMETHODIMP imgRequestProxy::OnStopContainer(imgIRequest *request, nsISupports *cx, imgIContainer *image)
{
  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("[this=%p] imgRequestProxy::OnStopContainer\n", this));

  if (mObserver)
    mObserver->OnStopContainer(this, mContext, image);

  return NS_OK;
}

/* void onStopDecode (in imgIRequest request, in nsISupports cx, in nsresult status, in wstring statusArg); */
NS_IMETHODIMP imgRequestProxy::OnStopDecode(imgIRequest *request, nsISupports *cx, nsresult status, const PRUnichar *statusArg)
{
  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("[this=%p] imgRequestProxy::OnStopDecode\n", this));

  if (mObserver)
    mObserver->OnStopDecode(this, mContext, status, statusArg);

  return NS_OK;
}





/* void onStartRequest (in nsIRequest request, in nsISupports ctxt); */
NS_IMETHODIMP imgRequestProxy::OnStartRequest(nsIRequest *request, nsISupports *ctxt)
{
  return NS_OK;
}

/* void onStopRequest (in nsIRequest request, in nsISupports ctxt, in nsresult statusCode, in wstring statusText); */
NS_IMETHODIMP imgRequestProxy::OnStopRequest(nsIRequest *request, nsISupports *ctxt, nsresult statusCode, const PRUnichar *statusText)
{
  if (!mDummyChannel)
    return NS_OK;

  nsCOMPtr<nsILoadGroup> loadGroup;
  mDummyChannel->GetLoadGroup(getter_AddRefs(loadGroup));
  if (loadGroup) {
    loadGroup->RemoveRequest(mDummyChannel, mContext, statusCode, statusText);
  }
  mDummyChannel = nsnull;

  return NS_OK;
}

