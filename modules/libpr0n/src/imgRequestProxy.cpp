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

NS_IMPL_ISUPPORTS5(imgRequestProxy, imgIRequest, nsIRequest, imgIDecoderObserver, imgIContainerObserver, nsIRequestObserver)

imgRequestProxy::imgRequestProxy() :
  mCanceled(PR_FALSE)
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
}

imgRequestProxy::~imgRequestProxy()
{
  /* destructor code */

  if (!mCanceled) {
    mCanceled = PR_TRUE;

    /* set mListener to null so that we don't forward any callbacks that RemoveObserver might generate */
    mListener = nsnull;

    /* Call RemoveObserver with a successful status.  This will keep the channel, if still downloading data,
       from being canceled if 'this' is the last observer.  This allows the image to continue to download and
       be cached even if no one is using it currently.
     */
    NS_REINTERPRET_CAST(imgRequest*, mOwner.get())->RemoveProxy(this, NS_OK);

    mOwner = nsnull;
  }
}



nsresult imgRequestProxy::Init(imgRequest *request, nsILoadGroup *aLoadGroup, imgIDecoderObserver *aObserver, nsISupports *cx)
{
  NS_PRECONDITION(aLoadGroup, "no loadgroup");
  if (!aLoadGroup)
    return NS_ERROR_NULL_POINTER;

  NS_PRECONDITION(request, "no request");
  if (!request)
    return NS_ERROR_NULL_POINTER;

  LOG_SCOPE_WITH_PARAM(gImgLog, "imgRequestProxy::Init", "request", request);

  mOwner = NS_STATIC_CAST(imgIRequest*, request);
  mListener = aObserver;
  mContext = cx;

  aLoadGroup->AddRequest(this, cx);
  mLoadGroup = aLoadGroup;

  request->AddProxy(this);

  return NS_OK;
}

/**  nsIRequest / imgIRequest methods **/

/* readonly attribute wstring name; */
NS_IMETHODIMP imgRequestProxy::GetName(PRUnichar * *aName)
{
  nsAutoString name(NS_LITERAL_STRING("imgRequestProxy["));
  if (mOwner) {
    nsCOMPtr<nsIURI> uri;
    mOwner->GetURI(getter_AddRefs(uri));
    if (uri) {
      nsXPIDLCString spec;
      uri->GetSpec(getter_Copies(spec));
      if (spec)
        name.Append(NS_ConvertUTF8toUCS2(spec));
    }
  } else {
    name.Append(NS_LITERAL_STRING("(null)"));
  }
  name.Append(PRUnichar(']'));

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
  if (mCanceled)
    return NS_ERROR_FAILURE;

  LOG_SCOPE(gImgLog, "imgRequestProxy::Cancel");

  mCanceled = PR_TRUE;

  nsresult rv = NS_REINTERPRET_CAST(imgRequest*, mOwner.get())->RemoveProxy(this, status);

  mOwner = nsnull;

  mListener = nsnull;

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
  *flags = nsIRequest::LOAD_NORMAL;
  return NS_OK;
}
NS_IMETHODIMP imgRequestProxy::SetLoadFlags(nsLoadFlags flags)
{
    NS_NOTYETIMPLEMENTED("imgRequestProxy::SetLoadFlags");
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
  *aDecoderObserver = mListener;
  NS_IF_ADDREF(*aDecoderObserver);
  return NS_OK;
}


/** imgIContainerObserver methods **/

/* [noscript] void frameChanged (in imgIContainer container, in nsISupports cx, in gfxIImageFrame newframe, in nsRect dirtyRect); */
NS_IMETHODIMP imgRequestProxy::FrameChanged(imgIContainer *container, nsISupports *cx, gfxIImageFrame *newframe, nsRect * dirtyRect)
{
  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("[this=%p] imgRequestProxy::FrameChanged\n", this));

  if (mListener)
    mListener->FrameChanged(container, mContext, newframe, dirtyRect);

  return NS_OK;
}

/** imgIDecoderObserver methods **/

/* void onStartDecode (in imgIRequest request, in nsISupports cx); */
NS_IMETHODIMP imgRequestProxy::OnStartDecode(imgIRequest *request, nsISupports *cx)
{
  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("[this=%p] imgRequestProxy::OnStartDecode\n", this));

  if (mListener)
    mListener->OnStartDecode(this, mContext);

  return NS_OK;
}

/* void onStartContainer (in imgIRequest request, in nsISupports cx, in imgIContainer image); */
NS_IMETHODIMP imgRequestProxy::OnStartContainer(imgIRequest *request, nsISupports *cx, imgIContainer *image)
{
  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("[this=%p] imgRequestProxy::OnStartContainer\n", this));

  if (mListener)
    mListener->OnStartContainer(this, mContext, image);

  return NS_OK;
}

/* void onStartFrame (in imgIRequest request, in nsISupports cx, in gfxIImageFrame frame); */
NS_IMETHODIMP imgRequestProxy::OnStartFrame(imgIRequest *request, nsISupports *cx, gfxIImageFrame *frame)
{
  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("[this=%p] imgRequestProxy::OnStartFrame\n", this));

  if (mListener)
    mListener->OnStartFrame(this, mContext, frame);

  return NS_OK;
}

/* [noscript] void onDataAvailable (in imgIRequest request, in nsISupports cx, in gfxIImageFrame frame, [const] in nsRect rect); */
NS_IMETHODIMP imgRequestProxy::OnDataAvailable(imgIRequest *request, nsISupports *cx, gfxIImageFrame *frame, const nsRect * rect)
{
  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("[this=%p] imgRequestProxy::OnDataAvailable\n", this));

  if (mListener)
    mListener->OnDataAvailable(this, mContext, frame, rect);

  return NS_OK;
}

/* void onStopFrame (in imgIRequest request, in nsISupports cx, in gfxIImageFrame frame); */
NS_IMETHODIMP imgRequestProxy::OnStopFrame(imgIRequest *request, nsISupports *cx, gfxIImageFrame *frame)
{
  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("[this=%p] imgRequestProxy::OnStopFrame\n", this));

  if (mListener)
    mListener->OnStopFrame(this, mContext, frame);

  return NS_OK;
}

/* void onStopContainer (in imgIRequest request, in nsISupports cx, in imgIContainer image); */
NS_IMETHODIMP imgRequestProxy::OnStopContainer(imgIRequest *request, nsISupports *cx, imgIContainer *image)
{
  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("[this=%p] imgRequestProxy::OnStopContainer\n", this));

  if (mListener)
    mListener->OnStopContainer(this, mContext, image);

  return NS_OK;
}

/* void onStopDecode (in imgIRequest request, in nsISupports cx, in nsresult status, in wstring statusArg); */
NS_IMETHODIMP imgRequestProxy::OnStopDecode(imgIRequest *request, nsISupports *cx, nsresult status, const PRUnichar *statusArg)
{
  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("[this=%p] imgRequestProxy::OnStopDecode\n", this));

  if (mListener)
    mListener->OnStopDecode(this, mContext, status, statusArg);

  return NS_OK;
}





/* void onStartRequest (in nsIRequest request, in nsISupports ctxt); */
NS_IMETHODIMP imgRequestProxy::OnStartRequest(nsIRequest *request, nsISupports *ctxt)
{
#ifdef PR_LOGGING
  if (PR_LOG_TEST(gImgLog, PR_LOG_DEBUG)) {
    nsXPIDLString name;
    GetName(getter_Copies(name));
    PR_LOG(gImgLog, PR_LOG_DEBUG,
           ("[this=%p] imgRequestProxy::OnStartRequest(%s)",
            this, NS_ConvertUCS2toUTF8(name).get()));
  }
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
  if (PR_LOG_TEST(gImgLog, PR_LOG_DEBUG)) {
    nsXPIDLString name;
    GetName(getter_Copies(name));
    PR_LOG(gImgLog, PR_LOG_DEBUG,
           ("[this=%p] imgRequestProxy::OnStopRequest(%s)",
            this, NS_ConvertUCS2toUTF8(name).get()));
  }
#endif

  mLoadGroup->RemoveRequest(this, mContext, statusCode);
  mLoadGroup = nsnull;
  return NS_OK;
}

