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

#include "nspr.h"

#include "ImageLogging.h"

#include "nsAutoLock.h"

NS_IMPL_THREADSAFE_ISUPPORTS5(imgRequestProxy, imgIRequest, nsIRequest,
                              imgIDecoderObserver, imgIContainerObserver,
                              nsIRequestObserver)

imgRequestProxy::imgRequestProxy() :
  mLoadFlags(nsIRequest::LOAD_NORMAL),
  mCanceled(PR_FALSE),
  mLock(nsnull)
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */

  mLock = PR_NewLock();
}

imgRequestProxy::~imgRequestProxy()
{
  /* destructor code */

  if (!mCanceled && mOwner) {
    PR_Lock(mLock);

    mCanceled = PR_TRUE;

    /* set mListener to null so that we don't forward any callbacks that RemoveObserver might generate */
    mListener = nsnull;

    PR_Unlock(mLock);

    /* Call RemoveObserver with a successful status.  This will keep the channel, if still downloading data,
       from being canceled if 'this' is the last observer.  This allows the image to continue to download and
       be cached even if no one is using it currently.
     */
    NS_REINTERPRET_CAST(imgRequest*, mOwner.get())->RemoveProxy(this, NS_OK);
  }

  PR_DestroyLock(mLock);
}



nsresult imgRequestProxy::Init(imgRequest *request, nsILoadGroup *aLoadGroup, imgIDecoderObserver *aObserver, nsISupports *cx)
{
  NS_PRECONDITION(request, "no request");
  if (!request)
    return NS_ERROR_NULL_POINTER;

  LOG_SCOPE_WITH_PARAM(gImgLog, "imgRequestProxy::Init", "request", request);

  PR_Lock(mLock);

  mOwner = NS_STATIC_CAST(imgIRequest*, request);
  mListener = aObserver;
  mContext = cx;

  if (aLoadGroup) {
    PRUint32 imageStatus;
    nsresult rv = mOwner->GetImageStatus(&imageStatus);
    if (NS_FAILED(rv)) return rv;
    if (!(imageStatus & STATUS_LOAD_COMPLETE)) {
      aLoadGroup->AddRequest(this, cx);
      mLoadGroup = aLoadGroup;
    }
  }

  PR_Unlock(mLock);

  request->AddProxy(this);

  return NS_OK;
}


/**  nsIRequest / imgIRequest methods **/

/* readonly attribute wstring name; */
NS_IMETHODIMP imgRequestProxy::GetName(PRUnichar * *aName)
{
  nsAutoString name;
  if (mOwner) {
    nsCOMPtr<nsIURI> uri;
    mOwner->GetURI(getter_AddRefs(uri));
    if (uri) {
      nsXPIDLCString spec;
      uri->GetSpec(getter_Copies(spec));
      if (spec)
        name.Append(NS_ConvertUTF8toUCS2(spec));
    }
  }

  *aName = nsCRT::strdup(name.get());
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

  NS_REINTERPRET_CAST(imgRequest*, mOwner.get())->RemoveProxy(this, status);

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

  nsAutoLock lock(mLock);
  return mOwner->GetImageStatus(aStatus);
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


/** imgIContainerObserver methods **/

/* [noscript] void frameChanged (in imgIContainer container, in nsISupports cx, in gfxIImageFrame newframe, in nsRect dirtyRect); */
NS_IMETHODIMP imgRequestProxy::FrameChanged(imgIContainer *container, nsISupports *cx, gfxIImageFrame *newframe, nsRect * dirtyRect)
{
  LOG_FUNC(gImgLog, "imgRequestProxy::FrameChanged");

  if (mListener)
    mListener->FrameChanged(container, mContext, newframe, dirtyRect);

  return NS_OK;
}

/** imgIDecoderObserver methods **/

/* void onStartDecode (in imgIRequest request, in nsISupports cx); */
NS_IMETHODIMP imgRequestProxy::OnStartDecode(imgIRequest *request, nsISupports *cx)
{
  LOG_FUNC(gImgLog, "imgRequestProxy::OnStartDecode");

  if (mListener)
    mListener->OnStartDecode(this, mContext);

  return NS_OK;
}

/* void onStartContainer (in imgIRequest request, in nsISupports cx, in imgIContainer image); */
NS_IMETHODIMP imgRequestProxy::OnStartContainer(imgIRequest *request, nsISupports *cx, imgIContainer *image)
{
  LOG_FUNC(gImgLog, "imgRequestProxy::OnStartContainer");

  if (mListener)
    mListener->OnStartContainer(this, mContext, image);

  return NS_OK;
}

/* void onStartFrame (in imgIRequest request, in nsISupports cx, in gfxIImageFrame frame); */
NS_IMETHODIMP imgRequestProxy::OnStartFrame(imgIRequest *request, nsISupports *cx, gfxIImageFrame *frame)
{
  LOG_FUNC(gImgLog, "imgRequestProxy::OnStartFrame");

  if (mListener)
    mListener->OnStartFrame(this, mContext, frame);

  return NS_OK;
}

/* [noscript] void onDataAvailable (in imgIRequest request, in nsISupports cx, in gfxIImageFrame frame, [const] in nsRect rect); */
NS_IMETHODIMP imgRequestProxy::OnDataAvailable(imgIRequest *request, nsISupports *cx, gfxIImageFrame *frame, const nsRect * rect)
{
  LOG_FUNC(gImgLog, "imgRequestProxy::OnDataAvailable");

  if (mListener)
    mListener->OnDataAvailable(this, mContext, frame, rect);

  return NS_OK;
}

/* void onStopFrame (in imgIRequest request, in nsISupports cx, in gfxIImageFrame frame); */
NS_IMETHODIMP imgRequestProxy::OnStopFrame(imgIRequest *request, nsISupports *cx, gfxIImageFrame *frame)
{
  LOG_FUNC(gImgLog, "imgRequestProxy::OnStopFrame");

  if (mListener)
    mListener->OnStopFrame(this, mContext, frame);

  return NS_OK;
}

/* void onStopContainer (in imgIRequest request, in nsISupports cx, in imgIContainer image); */
NS_IMETHODIMP imgRequestProxy::OnStopContainer(imgIRequest *request, nsISupports *cx, imgIContainer *image)
{
  LOG_FUNC(gImgLog, "imgRequestProxy::OnStopContainer");

  if (mListener)
    mListener->OnStopContainer(this, mContext, image);

  return NS_OK;
}

/* void onStopDecode (in imgIRequest request, in nsISupports cx, in nsresult status, in wstring statusArg); */
NS_IMETHODIMP imgRequestProxy::OnStopDecode(imgIRequest *request, nsISupports *cx, nsresult status, const PRUnichar *statusArg)
{
  LOG_FUNC(gImgLog, "imgRequestProxy::OnStopDecode");

  if (mListener)
    mListener->OnStopDecode(this, mContext, status, statusArg);

  return NS_OK;
}





/* void onStartRequest (in nsIRequest request, in nsISupports ctxt); */
NS_IMETHODIMP imgRequestProxy::OnStartRequest(nsIRequest *request, nsISupports *ctxt)
{
#ifdef PR_LOGGING
  nsXPIDLString name;
  GetName(getter_Copies(name));
  LOG_FUNC_WITH_PARAM(gImgLog, "imgRequestProxy::OnStartRequest", "name", NS_ConvertUCS2toUTF8(name).get());
#endif

  return NS_OK;
}

/* void onStopRequest (in nsIRequest request, in nsISupports ctxt, in nsresult statusCode, in wstring statusText); */
NS_IMETHODIMP imgRequestProxy::OnStopRequest(nsIRequest *request, nsISupports *ctxt, nsresult statusCode)
{
  /* it is ok to get multiple OnStopRequest messages */
  if (!mLoadGroup)
    return NS_OK;

#ifdef PR_LOGGING
  nsXPIDLString name;
  GetName(getter_Copies(name));
  LOG_FUNC_WITH_PARAM(gImgLog, "imgRequestProxy::OnStopRequest", "name", NS_ConvertUCS2toUTF8(name).get());
#endif

  /* calling RemoveRequest may cause the document to finish loading,
     which could result in our death.  We need to make sure that we stay
     alive long enough to fight another battle... at least until we exit
     this function.
   */
  nsCOMPtr<imgIRequest> kungFuDeathGrip(this);

  mLoadGroup->RemoveRequest(this, mContext, statusCode);
  mLoadGroup = nsnull;
  return NS_OK;
}

