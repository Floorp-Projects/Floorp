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

#include "imgRequest.h"

#include "nsIAtom.h"
#include "nsIChannel.h"
#include "nsILoadGroup.h"
#include "nsIHTTPChannel.h"
#include "nsIInputStream.h"
#include "imgILoader.h"
#include "nsIComponentManager.h"

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"

#include "nsString.h"
#include "nsXPIDLString.h"

#include "gfxIImageFrame.h"

#include "nsICachingChannel.h"
#include "ImageCache.h"

#include "ImageLogging.h"

#if defined(PR_LOGGING)
PRLogModuleInfo *gImgLog = PR_NewLogModule("imgRequest");
#endif

NS_IMPL_ISUPPORTS6(imgRequest, imgIRequest, nsIRequest,
                   imgIDecoderObserver, imgIContainerObserver,
                   nsIStreamListener, nsIStreamObserver)

imgRequest::imgRequest() : 
  mObservers(0), mLoading(PR_FALSE), mProcessing(PR_FALSE), mStatus(imgIRequest::STATUS_NONE), mState(0)
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
}

imgRequest::~imgRequest()
{
  /* destructor code */
}


nsresult imgRequest::Init(nsIChannel *aChannel, nsICacheEntryDescriptor *aCacheEntry)
{
  // XXX we should save off the thread we are getting called on here so that we can proxy all calls to mDecoder to it.

  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("[this=%p] imgRequest::Init\n", this));

  NS_ASSERTION(!mImage, "imgRequest::Init -- Multiple calls to init");
  NS_ASSERTION(aChannel, "imgRequest::Init -- No channel");

  mChannel = aChannel;

#ifdef MOZ_NEW_CACHE
  mCacheEntry = aCacheEntry;
#endif

  // XXX do not init the image here.  this has to be done from the image decoder.
  mImage = do_CreateInstance("@mozilla.org/image/container;1");

  return NS_OK;
}

nsresult imgRequest::AddObserver(imgIDecoderObserver *observer)
{
  LOG_SCOPE_WITH_PARAM(gImgLog, "imgRequest::AddObserver", "observer", observer);

  mObservers.AppendElement(NS_STATIC_CAST(void*, observer));

  if (mState & onStartDecode)
    observer->OnStartDecode(nsnull, nsnull);
  if (mState & onStartContainer)
    observer->OnStartContainer(nsnull, nsnull, mImage);

  // XXX send the decoded rect in here

  if (mState & onStopContainer)
    observer->OnStopContainer(nsnull, nsnull, mImage);
  if (mState & onStopDecode)
    observer->OnStopDecode(nsnull, nsnull, NS_OK, nsnull);

  if (mObservers.Count() == 1) {
    PRUint32 nframes;
    mImage->GetNumFrames(&nframes);
  //if (nframes > 1) {
      PR_LOG(gImgLog, PR_LOG_DEBUG,
             ("[this=%p] imgRequest::AddObserver -- starting animation\n", this));

      mImage->StartAnimation();
  //}
  }

  if (mState & onStopRequest) {
    nsCOMPtr<nsIStreamObserver> ob(do_QueryInterface(observer));
    PR_ASSERT(observer);
    ob->OnStopRequest(nsnull, nsnull, NS_OK, nsnull);
  } 

  return NS_OK;
}

nsresult imgRequest::RemoveObserver(imgIDecoderObserver *observer, nsresult status)
{
  LOG_SCOPE_WITH_PARAM(gImgLog, "imgRequest::RemoveObserver", "observer", observer);

  mObservers.RemoveElement(NS_STATIC_CAST(void*, observer));

  if (mObservers.Count() == 0) {
    if (mImage) {
      PRUint32 nframes;
      mImage->GetNumFrames(&nframes);
      if (nframes > 1) {
        PR_LOG(gImgLog, PR_LOG_DEBUG,
               ("[this=%p] imgRequest::RemoveObserver -- stopping animation\n", this));

        mImage->StopAnimation();
      }
    }

    if (mChannel && mLoading) {
      PR_LOG(gImgLog, PR_LOG_DEBUG,
             ("[this=%p] imgRequest::RemoveObserver -- load in progress.  canceling\n", this));

      this->RemoveFromCache();
      this->Cancel(NS_BINDING_ABORTED);
    }
  }

  return NS_OK;
}

PRBool imgRequest::RemoveFromCache()
{
  LOG_SCOPE(gImgLog, "imgRequest::RemoveFromCache");

#ifdef MOZ_NEW_CACHE

  if (mCacheEntry)
    mCacheEntry->Doom();
  else
    NS_WARNING("imgRequest::RemoveFromCache -- no entry!");

  mCacheEntry = nsnull;
#endif

  return PR_TRUE;
}


/**  nsIRequest / imgIRequest methods **/

/* readonly attribute wstring name; */
NS_IMETHODIMP imgRequest::GetName(PRUnichar * *aName)
{
    NS_NOTYETIMPLEMENTED("imgRequest::GetName");
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean isPending (); */
NS_IMETHODIMP imgRequest::IsPending(PRBool *_retval)
{
    NS_NOTYETIMPLEMENTED("imgRequest::IsPending");
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute nsresult status; */
NS_IMETHODIMP imgRequest::GetStatus(nsresult *aStatus)
{
    NS_NOTYETIMPLEMENTED("imgRequest::GetStatus");
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void cancel (in nsresult status); */
NS_IMETHODIMP imgRequest::Cancel(nsresult status)
{
  LOG_SCOPE(gImgLog, "imgRequest::Cancel");

  if (mImage) {
    PRUint32 nframes;
    mImage->GetNumFrames(&nframes);
    if (nframes > 1) {
      PR_LOG(gImgLog, PR_LOG_DEBUG,
             ("[this=%p] imgRequest::RemoveObserver -- stopping animation\n", this));

      mImage->StopAnimation();
    }
  }

  if (mChannel && mLoading)
    mChannel->Cancel(NS_BINDING_ABORTED); // should prolly use status here

  return NS_OK;
}

/* void suspend (); */
NS_IMETHODIMP imgRequest::Suspend()
{
    NS_NOTYETIMPLEMENTED("imgRequest::Suspend");
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void resume (); */
NS_IMETHODIMP imgRequest::Resume()
{
    NS_NOTYETIMPLEMENTED("imgRequest::Resume");
    return NS_ERROR_NOT_IMPLEMENTED;
}

/** imgIRequest methods **/

/* readonly attribute imgIContainer image; */
NS_IMETHODIMP imgRequest::GetImage(imgIContainer * *aImage)
{
  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("[this=%p] imgRequest::GetImage\n", this));

  *aImage = mImage;
  NS_IF_ADDREF(*aImage);
  return NS_OK;
}

/* readonly attribute unsigned long imageStatus; */
NS_IMETHODIMP imgRequest::GetImageStatus(PRUint32 *aStatus)
{
  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("[this=%p] imgRequest::GetImageStatus\n", this));

  *aStatus = mStatus;
  return NS_OK;
}

/* readonly attribute nsIURI URI; */
NS_IMETHODIMP imgRequest::GetURI(nsIURI **aURI)
{
  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("[this=%p] imgRequest::GetURI\n", this));

  if (mChannel)
    mChannel->GetOriginalURI(aURI);
  else if (mURI) {
    *aURI = mURI;
    NS_ADDREF(*aURI);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}


/** imgIContainerObserver methods **/

/* [noscript] void frameChanged (in imgIContainer container, in nsISupports cx, in gfxIImageFrame newframe, in nsRect dirtyRect); */
NS_IMETHODIMP imgRequest::FrameChanged(imgIContainer *container, nsISupports *cx, gfxIImageFrame *newframe, nsRect * dirtyRect)
{
  LOG_SCOPE(gImgLog, "imgRequest::FrameChanged");

  PRInt32 i = -1;
  PRInt32 count = mObservers.Count();

  while (++i < count) {
    imgIDecoderObserver *ob = NS_STATIC_CAST(imgIDecoderObserver*, mObservers[i]);
    if (ob) ob->FrameChanged(container, cx, newframe, dirtyRect);
  }

  return NS_OK;
}

/** imgIDecoderObserver methods **/

/* void onStartDecode (in imgIRequest request, in nsISupports cx); */
NS_IMETHODIMP imgRequest::OnStartDecode(imgIRequest *request, nsISupports *cx)
{
  LOG_SCOPE(gImgLog, "imgRequest::OnStartDecode");

  mState |= onStartDecode;

  PRInt32 i = -1;
  PRInt32 count = mObservers.Count();

  while (++i < count) {
    imgIDecoderObserver *ob = NS_STATIC_CAST(imgIDecoderObserver*, mObservers[i]);
    if (ob) ob->OnStartDecode(request, cx);
  }

  return NS_OK;
}

/* void onStartContainer (in imgIRequest request, in nsISupports cx, in imgIContainer image); */
NS_IMETHODIMP imgRequest::OnStartContainer(imgIRequest *request, nsISupports *cx, imgIContainer *image)
{
  LOG_SCOPE(gImgLog, "imgRequest::OnStartContainer");

  mState |= onStartContainer;

  mStatus |= imgIRequest::STATUS_SIZE_AVAILABLE;

  PRInt32 i = -1;
  PRInt32 count = mObservers.Count();

  while (++i < count) {
    imgIDecoderObserver *ob = NS_STATIC_CAST(imgIDecoderObserver*, mObservers[i]);
    if (ob) ob->OnStartContainer(request, cx, image);
  }

  return NS_OK;
}

/* void onStartFrame (in imgIRequest request, in nsISupports cx, in gfxIImageFrame frame); */
NS_IMETHODIMP imgRequest::OnStartFrame(imgIRequest *request, nsISupports *cx, gfxIImageFrame *frame)
{
  LOG_SCOPE(gImgLog, "imgRequest::OnStartFrame");

  PRInt32 i = -1;
  PRInt32 count = mObservers.Count();

  while (++i < count) {
    imgIDecoderObserver *ob = NS_STATIC_CAST(imgIDecoderObserver*, mObservers[i]);
    if (ob) ob->OnStartFrame(request, cx, frame);
  }

  return NS_OK;
}

/* [noscript] void onDataAvailable (in imgIRequest request, in nsISupports cx, in gfxIImageFrame frame, [const] in nsRect rect); */
NS_IMETHODIMP imgRequest::OnDataAvailable(imgIRequest *request, nsISupports *cx, gfxIImageFrame *frame, const nsRect * rect)
{
  LOG_SCOPE(gImgLog, "imgRequest::OnDataAvailable");

  PRInt32 i = -1;
  PRInt32 count = mObservers.Count();

  while (++i < count) {
    imgIDecoderObserver *ob = NS_STATIC_CAST(imgIDecoderObserver*, mObservers[i]);
    if (ob) ob->OnDataAvailable(request, cx, frame, rect);
  }

  return NS_OK;
}

/* void onStopFrame (in imgIRequest request, in nsISupports cx, in gfxIImageFrame frame); */
NS_IMETHODIMP imgRequest::OnStopFrame(imgIRequest *request, nsISupports *cx, gfxIImageFrame *frame)
{
  NS_ASSERTION(frame, "imgRequest::OnStopFrame called with NULL frame");

  LOG_SCOPE(gImgLog, "imgRequest::OnStopFrame");

  PRInt32 i = -1;
  PRInt32 count = mObservers.Count();

#ifdef MOZ_NEW_CACHE
  if (mCacheEntry) {
    PRUint32 cacheSize = 0;

    mCacheEntry->GetDataSize(&cacheSize);

    PRUint32 imageSize = 0;
    PRUint32 alphaSize = 0;

    frame->GetImageDataLength(&imageSize);
    frame->GetAlphaDataLength(&alphaSize);

    mCacheEntry->SetDataSize(cacheSize + imageSize + alphaSize);
  }
#endif

  while (++i < count) {
    imgIDecoderObserver *ob = NS_STATIC_CAST(imgIDecoderObserver*, mObservers[i]);
    if (ob) ob->OnStopFrame(request, cx, frame);
  }

  return NS_OK;
}

/* void onStopContainer (in imgIRequest request, in nsISupports cx, in imgIContainer image); */
NS_IMETHODIMP imgRequest::OnStopContainer(imgIRequest *request, nsISupports *cx, imgIContainer *image)
{
  LOG_SCOPE(gImgLog, "imgRequest::OnStopContainer");

  mState |= onStopContainer;

  PRInt32 i = -1;
  PRInt32 count = mObservers.Count();

  while (++i < count) {
    imgIDecoderObserver *ob = NS_STATIC_CAST(imgIDecoderObserver*, mObservers[i]);
    if (ob) ob->OnStopContainer(request, cx, image);
  }

  return NS_OK;
}

/* void onStopDecode (in imgIRequest request, in nsISupports cx, in nsresult status, in wstring statusArg); */
NS_IMETHODIMP imgRequest::OnStopDecode(imgIRequest *request, nsISupports *cx, nsresult status, const PRUnichar *statusArg)
{
  LOG_SCOPE(gImgLog, "imgRequest::OnStopDecode");

  mState |= onStopDecode;

  if (NS_FAILED(status))
    mStatus |= imgIRequest::STATUS_ERROR;

  PRInt32 i = -1;
  PRInt32 count = mObservers.Count();

  while (++i < count) {
    imgIDecoderObserver *ob = NS_STATIC_CAST(imgIDecoderObserver*, mObservers[i]);
    if (ob) ob->OnStopDecode(request, cx, status, statusArg);
  }

  return NS_OK;
}






/** nsIStreamObserver methods **/

/* void onStartRequest (in nsIRequest request, in nsISupports ctxt); */
NS_IMETHODIMP imgRequest::OnStartRequest(nsIRequest *aRequest, nsISupports *ctxt)
{
  LOG_SCOPE(gImgLog, "imgRequest::OnStartRequest");

  NS_ASSERTION(!mDecoder, "imgRequest::OnStartRequest -- we already have a decoder");
  NS_ASSERTION(!mLoading, "imgRequest::OnStartRequest -- we are loading again?");

  /* set our loading flag to true */
  mLoading = PR_TRUE;

  /* notify our kids */
  PRInt32 i = -1;
  PRInt32 count = mObservers.Count();

  while (++i < count) {
    imgIDecoderObserver *iob = NS_STATIC_CAST(imgIDecoderObserver*, mObservers[i]);
    if (iob) {
      nsCOMPtr<nsIStreamObserver> ob(do_QueryInterface(iob));
      if (ob) ob->OnStartRequest(aRequest, ctxt);
    }
  }

  /* do our real work */
  nsCOMPtr<nsIChannel> chan(do_QueryInterface(aRequest));

  if (!mChannel) {
    PR_LOG(gImgLog, PR_LOG_ALWAYS,
           (" `->  Channel already stopped or no channel!?.\n"));

    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIHTTPChannel> httpChannel(do_QueryInterface(chan));
  if (httpChannel) {
    PRUint32 httpStatus;
    httpChannel->GetResponseStatus(&httpStatus);
    if (httpStatus == 404) {
      PR_LOG(gImgLog, PR_LOG_DEBUG,
             ("[this=%p] imgRequest::OnStartRequest -- http status = 404. canceling.\n", this));

      mStatus |= imgIRequest::STATUS_ERROR;
      // this->Cancel(NS_BINDING_ABORTED);
      this->RemoveFromCache();

      return NS_BINDING_ABORTED;
    }

  }

  /* get the expires info */
#if defined(MOZ_NEW_CACHE)
  if (mCacheEntry) {
    nsCOMPtr<nsICachingChannel> cacheChannel(do_QueryInterface(chan));
    if (cacheChannel) {
      nsCOMPtr<nsISupports> cacheToken;
      cacheChannel->GetCacheToken(getter_AddRefs(cacheToken));
      if (cacheToken) {
        nsCOMPtr<nsICacheEntryDescriptor> entryDesc(do_QueryInterface(cacheToken));
        if (entryDesc) {
          PRUint32 expiration;
          /* get the expiration time from the caching channel's token */
          entryDesc->GetExpirationTime(&expiration);

          /* set the expiration time on our entry */
          mCacheEntry->SetExpirationTime(expiration);
        }
      }
    }
  }
#endif

  return NS_OK;
}

/* void onStopRequest (in nsIRequest request, in nsISupports ctxt, in nsresult status, in wstring statusArg); */
NS_IMETHODIMP imgRequest::OnStopRequest(nsIRequest *aRequest, nsISupports *ctxt, nsresult status, const PRUnichar *statusArg)
{
  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("[this=%p] imgRequest::OnStopRequest\n", this));

  NS_ASSERTION(mChannel && mLoading, "imgRequest::OnStopRequest -- received multiple OnStopRequest");

  mState |= onStopRequest;

  /* set our loading flag to false */
  mLoading = PR_FALSE;

  /* set our processing flag to false */
  mProcessing = PR_FALSE;

  switch(status) {
  case NS_BINDING_ABORTED:
  case NS_BINDING_FAILED:
  case NS_ERROR_IMAGELIB_NO_DECODER:

    mStatus |= imgIRequest::STATUS_ERROR;

    this->RemoveFromCache();
    this->Cancel(status); // stops animations

    break;
  case NS_BINDING_SUCCEEDED:
    mStatus |= imgIRequest::STATUS_LOAD_COMPLETE;
    break;
  default:
    printf("weird status return %i\n", status);
    break;
  }

  mChannel->GetOriginalURI(getter_AddRefs(mURI));
  mChannel = nsnull; // we no longer need the channel

  if (mDecoder) {
    mDecoder->Flush();
    mDecoder->Close();
    mDecoder = nsnull; // release the decoder so that it can rest peacefully ;)
  }

  /* notify the kids */
  PRInt32 i = -1;
  PRInt32 count = mObservers.Count();

  while (++i < count) {
    void *item = NS_STATIC_CAST(void *, mObservers[i]);
    if (item) {
      imgIDecoderObserver *iob = NS_STATIC_CAST(imgIDecoderObserver*, item);
      if (iob) {
        nsCOMPtr<nsIStreamObserver> ob(do_QueryInterface(iob));
        if (ob) ob->OnStopRequest(aRequest, ctxt, status, statusArg);
      }
    } else {
      NS_NOTREACHED("why did we get a null item ?");
    }
  }

  return NS_OK;
}




/* prototype for this defined below */
static NS_METHOD sniff_mimetype_callback(nsIInputStream* in, void* closure, const char* fromRawSegment,
                                         PRUint32 toOffset, PRUint32 count, PRUint32 *writeCount);


/** nsIStreamListener methods **/

/* void onDataAvailable (in nsIRequest request, in nsISupports ctxt, in nsIInputStream inStr, in unsigned long sourceOffset, in unsigned long count); */
NS_IMETHODIMP imgRequest::OnDataAvailable(nsIRequest *aRequest, nsISupports *ctxt, nsIInputStream *inStr, PRUint32 sourceOffset, PRUint32 count)
{
  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("[this=%p] imgRequest::OnDataAvailable\n", this));

  NS_ASSERTION(mChannel, "imgRequest::OnDataAvailable -- no channel!");


  if (!mProcessing) {
    /* set our processing flag to true if this is the first OnDataAvailable() */
    mProcessing = PR_TRUE;

    /* look at the first few bytes and see if we can tell what the data is from that
     * since servers tend to lie. :(
     */
    PRUint32 out;
    inStr->ReadSegments(sniff_mimetype_callback, this, count, &out);

#ifdef NS_DEBUG
    /* NS_WARNING if the content type from the channel isn't the same if the sniffing */
#endif

    if (!mContentType.get()) {
      nsXPIDLCString contentType;
      nsresult rv = mChannel->GetContentType(getter_Copies(contentType));

      if (NS_FAILED(rv)) {
        PR_LOG(gImgLog, PR_LOG_ERROR,
               ("[this=%p] imgRequest::OnStartRequest -- Content type unavailable from the channel\n",
                this));

        this->RemoveFromCache();

        return NS_BINDING_ABORTED; //NS_BASE_STREAM_CLOSED;
      }

      mContentType = contentType;
    }

#if defined(PR_LOGGING)
    PR_LOG(gImgLog, PR_LOG_DEBUG,
           ("[this=%p] imgRequest::OnStartRequest -- Content type is %s\n", this, mContentType.get()));
#endif

    nsCAutoString conid("@mozilla.org/image/decoder;2?type=");
    conid += mContentType.get();

    mDecoder = do_CreateInstance(conid);

    if (!mDecoder) {
      PR_LOG(gImgLog, PR_LOG_WARNING,
             ("[this=%p] imgRequest::OnStartRequest -- Decoder not available\n", this));

      // no image decoder for this mimetype :(
      this->Cancel(NS_BINDING_ABORTED);
      this->RemoveFromCache();

      // XXX notify the person that owns us now that wants the imgIContainer off of us?
      return NS_ERROR_IMAGELIB_NO_DECODER;
    }

    mDecoder->Init(NS_STATIC_CAST(imgIRequest*, this));

  }

  if (!mDecoder) {
    PR_LOG(gImgLog, PR_LOG_WARNING,
           ("[this=%p] imgRequest::OnDataAvailable -- no decoder\n", this));

    return NS_BASE_STREAM_CLOSED;
  }

  PRUint32 wrote;
  nsresult rv = mDecoder->WriteFrom(inStr, count, &wrote);

  return NS_OK;
}

static NS_METHOD sniff_mimetype_callback(nsIInputStream* in,
                                         void* closure,
                                         const char* fromRawSegment,
                                         PRUint32 toOffset,
                                         PRUint32 count,
                                         PRUint32 *writeCount)
{
  imgRequest *request = NS_STATIC_CAST(imgRequest*, closure);

  NS_ASSERTION(request, "request is null!");

  if (count > 0)
    request->SniffMimeType(fromRawSegment, count);

  *writeCount = 0;
  return NS_ERROR_FAILURE;
}

void
imgRequest::SniffMimeType(const char *buf, PRUint32 len)
{
  /* Is it a GIF? */
  if (len >= 4 && !nsCRT::strncmp(buf, "GIF8", 4))  {
    mContentType = NS_LITERAL_CSTRING("image/gif");
    return;
  }

  /* or a PNG? */
  if (len >= 4 && ((unsigned char)buf[0]==0x89 &&
                   (unsigned char)buf[1]==0x50 &&
                   (unsigned char)buf[2]==0x4E &&
                   (unsigned char)buf[3]==0x47))
  { 
    mContentType = NS_LITERAL_CSTRING("image/png");
    return;
  }

  /* maybe a JPEG (JFIF)? */
  /* JFIF files start with SOI APP0 but older files can start with SOI DQT
   * so we test for SOI followed by any marker, i.e. FF D8 FF
   * this will also work for SPIFF JPEG files if they appear in the future.
   *
   * (JFIF is 0XFF 0XD8 0XFF 0XE0 <skip 2> 0X4A 0X46 0X49 0X46 0X00)
   */
  if (len >= 3 &&
     ((unsigned char)buf[0])==0xFF &&
     ((unsigned char)buf[1])==0xD8 &&
     ((unsigned char)buf[2])==0xFF)
  {
    mContentType = NS_LITERAL_CSTRING("image/jpeg");
    return;
  }

  /* or how about ART? */
  /* ART begins with JG (4A 47). Major version offset 2.
   * Minor version offset 3. Offset 4 must be NULL.
   */
  if (len >= 5 &&
   ((unsigned char) buf[0])==0x4a &&
   ((unsigned char) buf[1])==0x47 &&
   ((unsigned char) buf[4])==0x00 )
  {
    mContentType = NS_LITERAL_CSTRING("image/x-jg");
    return;
  }

  /* none of the above?  I give up */
}
