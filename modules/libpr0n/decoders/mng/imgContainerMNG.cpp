/*
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
 * The Initial Developer of the Original Code is Tim Rowley.
 * Portions created by Tim Rowley are 
 * Copyright (C) 2001 Tim Rowley.  Rights Reserved.
 * 
 * Contributor(s): 
 *   Tim Rowley <tor@cs.brown.edu>
 */

#include "imgContainerMNG.h"
#include "nsIServiceManager.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "gfxIImageFrame.h"
#include "nsIImage.h"
#include "nsIInputStream.h"
#include "imgIDecoderObserver.h"
#include "nsMemory.h"
#include "prinrval.h"
#include "nsColor.h"

static void il_mng_timeout_func(nsITimer *timer, void *data);

NS_IMPL_ISUPPORTS1(imgContainerMNG, imgIContainer)

//////////////////////////////////////////////////////////////////////////////
imgContainerMNG::imgContainerMNG() : 
  mObserver(0),
  mDecoder(0),
  mAnimationMode(0),
  mFrozen(PR_FALSE)
{
  NS_INIT_ISUPPORTS();
}

//////////////////////////////////////////////////////////////////////////////
imgContainerMNG::~imgContainerMNG()
{
  if (mTimer) {
    mTimer->Cancel();
    mTimer = nsnull; 
  }

  mng_display_freeze(mHandle);
  mng_cleanup(&mHandle);
  if (alpha)
    nsMemory::Free(alpha);
  if (image)
    nsMemory::Free(image);
  if (mBuffer)
    nsMemory::Free(mBuffer);
  
  mFrame = 0;
}

//****************************************************************************
/* void init (in nscoord aWidth, in nscoord aHeight, in imgIContainerObserver aObserver); */
NS_IMETHODIMP
imgContainerMNG::Init(nscoord aWidth,
		      nscoord aHeight,
		      imgIContainerObserver *aObserver)
{
  if (aWidth <= 0 || aHeight <= 0) {
    NS_WARNING("error - negative image size\n");
    return NS_ERROR_FAILURE;
  }

  mSize.SizeTo(aWidth, aHeight);

  mObserver = getter_AddRefs(NS_GetWeakReference(aObserver));

  return NS_OK;
}

//****************************************************************************
/* readonly attribute gfx_format preferredAlphaChannelFormat; */
NS_IMETHODIMP
imgContainerMNG::GetPreferredAlphaChannelFormat(gfx_format *aFormat)
{
  /* default.. platform's should probably overwrite this */
  *aFormat = gfxIFormats::RGB_A8;
  return NS_OK;
}

//****************************************************************************
/* readonly attribute nscoord width; */
NS_IMETHODIMP
imgContainerMNG::GetWidth(nscoord *aWidth)
{
  *aWidth = mSize.width;
  return NS_OK;
}

//****************************************************************************
/* readonly attribute nscoord height; */
NS_IMETHODIMP
imgContainerMNG::GetHeight(nscoord *aHeight)
{
  *aHeight = mSize.height;
  return NS_OK;
}

//****************************************************************************
/* readonly attribute gfxIImageFrame currentFrame; */
NS_IMETHODIMP
imgContainerMNG::GetCurrentFrame(gfxIImageFrame * *aCurrentFrame)
{
  if (mFrame) {
    *aCurrentFrame = mFrame;
    NS_ADDREF(*aCurrentFrame);
    return NS_OK;
  } else {
    *aCurrentFrame = 0;
    return NS_ERROR_FAILURE;
  }
}

//****************************************************************************
/* readonly attribute unsigned long numFrames; */
NS_IMETHODIMP
imgContainerMNG::GetNumFrames(PRUint32 *aNumFrames)
{
  if (mFrame)
    *aNumFrames = 1;
  else
    *aNumFrames = 0;
  return NS_OK;
}

//****************************************************************************
/* gfxIImageFrame getFrameAt (in unsigned long index); */
NS_IMETHODIMP
imgContainerMNG::GetFrameAt(PRUint32 index, gfxIImageFrame **_retval)
{
  return GetCurrentFrame(_retval);
}

//****************************************************************************
/* void appendFrame (in gfxIImageFrame item); */
NS_IMETHODIMP
imgContainerMNG::AppendFrame(gfxIImageFrame *item)
{
  mFrame = item;
  return NS_OK;
}

//****************************************************************************
/* void removeFrame (in gfxIImageFrame item); */
NS_IMETHODIMP
imgContainerMNG::RemoveFrame(gfxIImageFrame *item)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

//****************************************************************************
/* void endFrameDecode (in gfxIImageFrame item, in unsigned long timeout); */
NS_IMETHODIMP
imgContainerMNG::EndFrameDecode(PRUint32 aFrameNum, PRUint32 aTimeout)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

//****************************************************************************
/* void decodingComplete (); */
NS_IMETHODIMP
imgContainerMNG::DecodingComplete(void)
{
  return NS_OK;
}

//****************************************************************************
/* nsIEnumerator enumerate (); */
NS_IMETHODIMP
imgContainerMNG::Enumerate(nsIEnumerator **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void clear (); */
NS_IMETHODIMP
imgContainerMNG::Clear()
{
  mFrame = 0;
  return NS_OK;
}

//****************************************************************************
/* void startAnimation () */
NS_IMETHODIMP
imgContainerMNG::StartAnimation()
{
  if (mFrozen) {
    mFrozen = PR_FALSE;
    il_mng_timeout_func(0, (void *)mHandle);
  }

  return NS_OK;
}

//****************************************************************************
/* void stopAnimation (); */
NS_IMETHODIMP
imgContainerMNG::StopAnimation()
{ 
  if (!mTimer)
    return NS_OK;
  else {
    mTimer->Cancel();
    mTimer = nsnull;
    mFrozen = PR_TRUE;
  }

  return NS_OK;
}

//****************************************************************************
/* attribute long loopCount; */
NS_IMETHODIMP
imgContainerMNG::GetLoopCount(PRInt32 *aLoopCount)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
imgContainerMNG::SetLoopCount(PRInt32 aLoopCount)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

//****************************************************************************
NS_IMETHODIMP
imgContainerMNG::GetAnimationMode(PRUint16 *aAnimationMode)
{
  if (!aAnimationMode)
    return NS_ERROR_NULL_POINTER;
  *aAnimationMode = mAnimationMode;
  return NS_OK;
}

NS_IMETHODIMP
imgContainerMNG::SetAnimationMode(PRUint16 aAnimationMode)
{
  mAnimationMode = aAnimationMode;
  return NS_OK;
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
// -------------------------------------------------------------------

#define EXTRACT_CONTAINER \
  imgContainerMNG *container = (imgContainerMNG *)mng_get_userdata(handle)

//===========================================================
// Callbacks for libmng
//===========================================================

static mng_bool
il_mng_openstream(mng_handle handle)
{
  return MNG_TRUE;
}


static mng_bool
il_mng_closestream(mng_handle handle)
{
  return MNG_TRUE;
}

static mng_bool
il_mng_readdata(mng_handle handle, mng_ptr buf,
                mng_uint32 size, mng_uint32 *stored)
{
  EXTRACT_CONTAINER;

  size = PR_MIN(size, container->mBufferEnd - container->mBufferPtr);
  memcpy(buf, container->mBuffer + container->mBufferPtr, size);
  container->mBufferPtr += size;
  *stored = size;

  return MNG_TRUE;
}

static mng_bool
il_mng_processheader(mng_handle handle, mng_uint32 width, mng_uint32 height)
{
  EXTRACT_CONTAINER;

  gfx_format format;

#if defined(XP_PC) || defined(XP_BEOS) || defined(MOZ_WIDGET_PHOTON)
  if (mng_get_alphadepth(handle)) {
    format = gfxIFormats::BGR_A8;
    mng_set_canvasstyle(handle, MNG_CANVAS_RGB8_A8);
  } else {
    format = gfxIFormats::BGR;
    mng_set_canvasstyle(handle, MNG_CANVAS_RGB8);
  }
#else
  if (mng_get_alphadepth(handle)) {
    format = gfxIFormats::RGB_A8;
    mng_set_canvasstyle(handle, MNG_CANVAS_RGB8_A8);
  } else {
    format = gfxIFormats::RGB;
    mng_set_canvasstyle(handle, MNG_CANVAS_RGB8);
  }
#endif

  nsMNGDecoder* decoder = container->mDecoder;

  if (decoder->mObserver)
    decoder->mObserver->OnStartDecode(nsnull, nsnull);

  if(decoder->mImageContainer)
    decoder->mImageContainer->Init(width, 
                                   height,
                                   decoder->mObserver);

  if (decoder->mObserver)
    decoder->mObserver->OnStartContainer(nsnull, 
                                         nsnull, 
                                         decoder->mImageContainer);

  // initalize the frame and append it to the container
  decoder->mImageFrame = do_CreateInstance("@mozilla.org/gfx/image/frame;2");
  if (!decoder->mImageFrame)
    return MNG_FALSE;

  if (NS_FAILED(decoder->mImageFrame->Init(0, 0, width, height, format, 24)))
    return MNG_FALSE;

  decoder->mImageContainer->AppendFrame(decoder->mImageFrame);
    
  if (decoder->mObserver)
    decoder->mObserver->OnStartFrame(nsnull, nsnull, decoder->mImageFrame);

  container->mFrame->GetImageBytesPerRow(&container->mByteWidth);
  container->mFrame->GetAlphaBytesPerRow(&container->mByteWidthAlpha);

  if ((format!=gfxIFormats::RGB) && (format!=gfxIFormats::BGR)) {
    container->alpha = 
      (unsigned char*)nsMemory::Alloc(container->mByteWidthAlpha*height);
    memset(container->alpha, 0, container->mByteWidthAlpha*height);
  } else
    container->alpha = 0;

  container->image = 
    (unsigned char*)nsMemory::Alloc(container->mByteWidth*height);
  memset(container->image, 0, container->mByteWidth*height);

  return MNG_TRUE;
}

static mng_ptr
il_mng_getcanvasline(mng_handle handle, mng_uint32 iLine)
{
  EXTRACT_CONTAINER;

  return container->image+container->mByteWidth*iLine;
}

static mng_ptr
il_mng_getalphaline(mng_handle handle, mng_uint32 iLine)
{
  EXTRACT_CONTAINER;

  return container->alpha+container->mByteWidthAlpha*iLine;
}

static mng_bool
il_mng_refresh(mng_handle handle,
	       mng_uint32 left, mng_uint32 top,
	       mng_uint32 width, mng_uint32 height)
{
  EXTRACT_CONTAINER;

  PRUint32 bpr, abpr;
  container->mFrame->GetImageBytesPerRow(&bpr);
  container->mFrame->GetAlphaBytesPerRow(&abpr);

// stupid Mac code that shouldn't be in the image decoders...
#if defined(XP_MAC) || defined(XP_MACOSX) || defined(XP_PC) || defined(XP_BEOS) || defined(MOZ_WIDGET_PHOTON)
  PRInt32 iwidth;
  container->mFrame->GetWidth(&iwidth);
  PRUint8 *buf = (PRUint8 *)nsMemory::Alloc(bpr);
#endif

  for (mng_uint32 y=top; y<top+height; y++) {
    if (container->alpha)
      container->mFrame->SetAlphaData(container->alpha + 
                          y*container->mByteWidthAlpha,
                          container->mByteWidthAlpha,
                          abpr*y);
#if defined(XP_MAC) || defined(XP_MACOSX)
    PRUint8 *cptr = buf;
    PRUint8 *row = container->image+y*container->mByteWidth;
    for (PRUint32 x=0; x<iwidth; x++) {
      *cptr++ = 0;
      *cptr++ = *row++;
      *cptr++ = *row++;
      *cptr++ = *row++;
    }
    container->mFrame->SetImageData(buf, bpr, bpr*y);
#elif defined(XP_PC) || defined(XP_BEOS) || defined(MOZ_WIDGET_PHOTON)
    PRUint8 *cptr = buf;
    PRUint8 *row = container->image+y*container->mByteWidth;
    for (PRUint32 x=0; x<iwidth; x++) {
      *cptr++ = row[2];
      *cptr++ = row[1];
      *cptr++ = row[0];
      row += 3;
    }
    container->mFrame->SetImageData(buf, bpr, bpr*y);
#else
    container->mFrame->SetImageData(container->image + 
			 y*container->mByteWidth,
			 container->mByteWidth,
			 bpr*y);
#endif
  }
#if defined(XP_MAC) || defined(XP_MACOSX) || defined(XP_PC) || defined(XP_BEOS) || defined(MOZ_WIDGET_PHOTON)
  nsMemory::Free(buf);
#endif

  nsRect r(left, top, width, height);

  nsCOMPtr<imgIDecoderObserver>
    ob(do_QueryReferent(container->mObserver));
  if (ob)
    ob->OnDataAvailable(nsnull, nsnull, container->mFrame, &r);

  nsCOMPtr<imgIContainerObserver> 
    observer(do_QueryReferent(container->mObserver));

  if (observer) {
    nsRect dirtyRect;
    container->mFrame->GetRect(dirtyRect);

    // do notification to FE to draw this frame
    observer->FrameChanged(container, nsnull, container->mFrame, &dirtyRect);
  }

  return MNG_TRUE;
}


static mng_uint32
il_mng_gettickcount(mng_handle handle)
{
  return PR_IntervalToMilliseconds(PR_IntervalNow());
}

static void
il_mng_timeout_func(nsITimer *timer, void *data)
{
  mng_handle handle = (mng_handle)data;
  EXTRACT_CONTAINER;

  if (container->mTimer) {
    container->mTimer->Cancel();
    container->mTimer = 0;
  }

  int ret = mng_display_resume(handle);
  if (ret == MNG_NEEDMOREDATA)
    container->mResumeNeeded = PR_TRUE;
  else if ((ret != MNG_NOERROR) && 
           (ret != MNG_NEEDTIMERWAIT) &&
           (ret != MNG_NEEDSECTIONWAIT))
    container->mErrorPending = PR_TRUE;
}

static mng_bool
il_mng_settimer(mng_handle handle, mng_uint32 msec)
{
  EXTRACT_CONTAINER;

  container->mTimer = do_CreateInstance("@mozilla.org/timer;1");
  container->mTimer->InitWithFuncCallback(il_mng_timeout_func, (void *)handle, msec,
                                          nsITimer::TYPE_ONE_SHOT);

  return MNG_TRUE;
}  

static mng_ptr
il_mng_alloc(mng_size_t size)
{
  void *ptr = nsMemory::Alloc(size);
  memset(ptr, 0, size);
  return ptr;
}

static void
il_mng_free(mng_ptr ptr, mng_size_t size)
{
  nsMemory::Free(ptr);
}

/* -----------------------------
 * Setup an mng_struct for decoding.
 */
void
imgContainerMNG::InitMNG(nsMNGDecoder *decoder)
{
  mDecoder = decoder;

  // init data members
  image = alpha = 0;
  mBufferPtr = mBufferEnd = 0;
  mBuffer = 0;
  mResumeNeeded = PR_FALSE;
  mErrorPending = PR_FALSE;
  mTimer = 0;

  // pass mng container as user data 
  mHandle = mng_initialize(this, il_mng_alloc, il_mng_free, NULL);

  mng_set_dfltimggamma(mHandle, 0.45455);
  mng_set_displaygamma(mHandle, 2.2);

  mng_setcb_openstream(mHandle, il_mng_openstream);
  mng_setcb_closestream(mHandle, il_mng_closestream);
  mng_setcb_readdata(mHandle, il_mng_readdata);
  mng_setcb_processheader(mHandle, il_mng_processheader);
  mng_setcb_getcanvasline(mHandle, il_mng_getcanvasline);
  mng_setcb_getalphaline(mHandle, il_mng_getalphaline);
  mng_setcb_refresh(mHandle, il_mng_refresh);
  mng_setcb_gettickcount(mHandle, il_mng_gettickcount);
  mng_setcb_settimer(mHandle, il_mng_settimer);
  mng_setcb_memalloc(mHandle, il_mng_alloc);
  mng_setcb_memfree(mHandle, il_mng_free);
  mng_set_suspensionmode(mHandle, MNG_TRUE);
 
  int ret = mng_readdisplay(mHandle);
  if (ret == MNG_NEEDMOREDATA)
    mResumeNeeded = PR_TRUE;
  else if ((ret != MNG_NOERROR) && 
           (ret != MNG_NEEDTIMERWAIT) &&
           (ret != MNG_NEEDSECTIONWAIT))
    mErrorPending = PR_TRUE;
}


/* ----------------------------------------------------------
 * Process data arriving from the stream for the MNG decoder.
 */
NS_IMETHODIMP
imgContainerMNG::WriteMNG(nsIInputStream *inStr, 
			  PRInt32 count,
			  PRUint32 *_retval)
{
  mBuffer = (PRUint8 *) nsMemory::Realloc(mBuffer, mBufferEnd+count);
  inStr->Read((char *)mBuffer+mBufferEnd, count, _retval);
  mBufferEnd += count;

  if (mResumeNeeded) {
    mResumeNeeded = PR_FALSE;
    int ret = mng_display_resume(mHandle);
    if (ret == MNG_NEEDMOREDATA)
      mResumeNeeded = PR_TRUE;
    else if ((ret != MNG_NOERROR) && 
             (ret != MNG_NEEDTIMERWAIT) &&
             (ret != MNG_NEEDSECTIONWAIT))
      mErrorPending = PR_TRUE;
  }

  if (mErrorPending)
    return NS_ERROR_FAILURE;
  else
    return NS_OK;
}
