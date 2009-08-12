/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 *   Chris Saari <saari@netscape.com>
 *   Asko Tontti <atontti@cc.hut.fi>
 *   Arron Mogge <paper@animecity.nu>
 *   Andrew Smith
 *   Federico Mena-Quintero <federico@novell.com>
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

#include "nsComponentManagerUtils.h"
#include "imgIContainerObserver.h"
#include "ImageErrors.h"
#include "imgILoad.h"
#include "imgIDecoder.h"
#include "imgIDecoderObserver.h"
#include "imgContainer.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsAutoPtr.h"
#include "nsStringStream.h"
#include "prmem.h"
#include "prlog.h"
#include "prenv.h"

#include "gfxContext.h"

/* Accounting for compressed data */
#if defined(PR_LOGGING)
static PRLogModuleInfo *gCompressedImageAccountingLog = PR_NewLogModule ("CompressedImageAccounting");
#else
#define gCompressedImageAccountingLog
#endif

static int num_containers_with_discardable_data;
static PRInt64 num_compressed_image_bytes;


NS_IMPL_ISUPPORTS3(imgContainer, imgIContainer, nsITimerCallback, nsIProperties)

//******************************************************************************
imgContainer::imgContainer() :
  mSize(0,0),
  mNumFrames(0),
  mAnim(nsnull),
  mAnimationMode(kNormalAnimMode),
  mLoopCount(-1),
  mObserver(nsnull),
  mDiscardable(PR_FALSE),
  mDiscarded(PR_FALSE),
  mRestoreDataDone(PR_FALSE),
  mDiscardTimer(nsnull)
{
}

//******************************************************************************
imgContainer::~imgContainer()
{
  if (mAnim)
    delete mAnim;

  for (unsigned int i = 0; i < mFrames.Length(); ++i)
    delete mFrames[i];

  if (!mRestoreData.IsEmpty()) {
    num_containers_with_discardable_data--;
    num_compressed_image_bytes -= mRestoreData.Length();

    PR_LOG (gCompressedImageAccountingLog, PR_LOG_DEBUG,
            ("CompressedImageAccounting: destroying imgContainer %p.  "
             "Compressed containers: %d, Compressed data bytes: %lld",
             this,
             num_containers_with_discardable_data,
             num_compressed_image_bytes));
  }

  if (mDiscardTimer) {
    mDiscardTimer->Cancel ();
    mDiscardTimer = nsnull;
  }
}

//******************************************************************************
/* void init (in PRInt32 aWidth, in PRInt32 aHeight, 
              in imgIContainerObserver aObserver); */
NS_IMETHODIMP imgContainer::Init(PRInt32 aWidth, PRInt32 aHeight,
                                 imgIContainerObserver *aObserver)
{
  if (aWidth <= 0 || aHeight <= 0) {
    NS_WARNING("error - negative image size\n");
    return NS_ERROR_FAILURE;
  }

  mSize.SizeTo(aWidth, aHeight);
  
  // As we are reloading it means we are no longer in 'discarded' state
  mDiscarded = PR_FALSE;

  mObserver = do_GetWeakReference(aObserver);
  
  return NS_OK;
}

//******************************************************************************
/* [noscript] imgIContainer extractCurrentFrame([const] in nsIntRect aRegion); */
NS_IMETHODIMP imgContainer::ExtractCurrentFrame(const nsIntRect &aRegion, imgIContainer **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsRefPtr<imgContainer> img(new imgContainer());
  NS_ENSURE_TRUE(img, NS_ERROR_OUT_OF_MEMORY);

  img->Init(aRegion.width, aRegion.height, nsnull);

  imgFrame *frame = GetCurrentImgFrame();
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);

  // The frame can be smaller than the image. We want to extract only the part
  // of the frame that actually exists.
  nsIntRect framerect = frame->GetRect();
  framerect.IntersectRect(framerect, aRegion);

  if (framerect.IsEmpty())
    return NS_ERROR_NOT_AVAILABLE;

  nsAutoPtr<imgFrame> subframe;
  nsresult rv = frame->Extract(framerect, getter_Transfers(subframe));
  if (NS_FAILED(rv))
    return rv;

  img->mFrames.AppendElement(subframe.forget());
  img->mNumFrames++;

  *_retval = img.forget().get();

  return NS_OK;
}

//******************************************************************************
/* readonly attribute PRInt32 width; */
NS_IMETHODIMP imgContainer::GetWidth(PRInt32 *aWidth)
{
  NS_ENSURE_ARG_POINTER(aWidth);

  *aWidth = mSize.width;
  return NS_OK;
}

//******************************************************************************
/* readonly attribute PRInt32 height; */
NS_IMETHODIMP imgContainer::GetHeight(PRInt32 *aHeight)
{
  NS_ENSURE_ARG_POINTER(aHeight);

  *aHeight = mSize.height;
  return NS_OK;
}

imgFrame *imgContainer::GetImgFrame(PRUint32 framenum)
{
  nsresult rv = RestoreDiscardedData();
  NS_ENSURE_SUCCESS(rv, nsnull);

  if (!mAnim) {
    NS_ASSERTION(framenum == 0, "Don't ask for a frame > 0 if we're not animated!");
    return mFrames.SafeElementAt(0, nsnull);
  }
  if (mAnim->lastCompositedFrameIndex == PRInt32(framenum))
    return mAnim->compositingFrame;
  return mFrames.SafeElementAt(framenum, nsnull);
}

PRInt32 imgContainer::GetCurrentImgFrameIndex() const
{
  if (mAnim)
    return mAnim->currentAnimationFrameIndex;

  return 0;
}

imgFrame *imgContainer::GetCurrentImgFrame()
{
  return GetImgFrame(GetCurrentImgFrameIndex());
}

//******************************************************************************
/* readonly attribute boolean currentFrameIsOpaque; */
NS_IMETHODIMP imgContainer::GetCurrentFrameIsOpaque(PRBool *aIsOpaque)
{
  NS_ENSURE_ARG_POINTER(aIsOpaque);

  imgFrame *curframe = GetCurrentImgFrame();
  NS_ENSURE_TRUE(curframe, NS_ERROR_FAILURE);

  *aIsOpaque = !curframe->GetNeedsBackground();

  // We are also transparent if the current frame's size doesn't cover our
  // entire area.
  nsIntRect framerect = curframe->GetRect();
  *aIsOpaque = *aIsOpaque && (framerect != nsIntRect(0, 0, mSize.width, mSize.height));

  return NS_OK;
}

//******************************************************************************
/* [noscript] void getCurrentFrameRect(nsIntRect rect); */
NS_IMETHODIMP imgContainer::GetCurrentFrameRect(nsIntRect &aRect)
{
  imgFrame *curframe = GetCurrentImgFrame();
  NS_ENSURE_TRUE(curframe, NS_ERROR_FAILURE);

  aRect = curframe->GetRect();

  return NS_OK;
}

//******************************************************************************
/* readonly attribute unsigned long currentFrameIndex; */
NS_IMETHODIMP imgContainer::GetCurrentFrameIndex(PRUint32 *aCurrentFrameIdx)
{
  NS_ENSURE_ARG_POINTER(aCurrentFrameIdx);
  
  *aCurrentFrameIdx = GetCurrentImgFrameIndex();

  return NS_OK;
}

//******************************************************************************
/* readonly attribute unsigned long numFrames; */
NS_IMETHODIMP imgContainer::GetNumFrames(PRUint32 *aNumFrames)
{
  NS_ENSURE_ARG_POINTER(aNumFrames);

  *aNumFrames = mNumFrames;
  
  return NS_OK;
}

//******************************************************************************
/* readonly attribute boolean animated; */
NS_IMETHODIMP imgContainer::GetAnimated(PRBool *aAnimated)
{
  NS_ENSURE_ARG_POINTER(aAnimated);

  *aAnimated = (mNumFrames > 1);
  
  return NS_OK;
}


//******************************************************************************
/* [noscript] gfxImageSurface copyCurrentFrame(); */
NS_IMETHODIMP imgContainer::CopyCurrentFrame(gfxImageSurface **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  imgFrame *frame = GetImgFrame(GetCurrentImgFrameIndex());
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);

  nsRefPtr<gfxPattern> pattern;
  frame->GetPattern(getter_AddRefs(pattern));
  nsIntRect intframerect = frame->GetRect();
  gfxRect framerect(intframerect.x, intframerect.y, intframerect.width, intframerect.height);

  // Create a 32-bit image surface of our size, but draw using the frame's
  // rect, implicitly padding the frame out to the image's size.
  nsRefPtr<gfxImageSurface> imgsurface = new gfxImageSurface(gfxIntSize(mSize.width, mSize.height),
                                                             gfxASurface::ImageFormatARGB32);
  gfxContext ctx(imgsurface);
  ctx.SetOperator(gfxContext::OPERATOR_SOURCE);
  ctx.SetPattern(pattern);
  ctx.Rectangle(framerect);
  ctx.Fill();

  *_retval = imgsurface.forget().get();
  return NS_OK;
}

//******************************************************************************
/* [noscript] readonly attribute gfxASurface currentFrame; */
NS_IMETHODIMP imgContainer::GetCurrentFrame(gfxASurface **_retval)
{
  imgFrame *frame = GetImgFrame(GetCurrentImgFrameIndex());
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);

  nsRefPtr<gfxASurface> framesurf;
  nsresult rv = NS_OK;

  // If this frame covers the entire image, we can just reuse its existing
  // surface.
  nsIntRect framerect = frame->GetRect();
  if (framerect.x == 0 && framerect.y == 0 &&
      framerect.width == mSize.width &&
      framerect.height == mSize.height)
    rv = frame->GetSurface(getter_AddRefs(framesurf));

  // The image doesn't have a surface because it's been optimized away. Create
  // one.
  if (!framesurf) {
    nsRefPtr<gfxImageSurface> imgsurf;
    rv = CopyCurrentFrame(getter_AddRefs(imgsurf));
    framesurf = imgsurf;
  }

  *_retval = framesurf.forget().get();

  return rv;
}

//******************************************************************************
/* unsigned long getFrameDataLength(in unsigned long framenum); */
NS_IMETHODIMP imgContainer::GetFrameImageDataLength(PRUint32 framenum, PRUint32 *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  if (framenum >= PRUint32(mNumFrames))
    return NS_ERROR_INVALID_ARG;

  imgFrame *frame = GetImgFrame(framenum);
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);

  *_retval = frame->GetImageDataLength();

  return NS_OK;
}

//******************************************************************************
/* unsigned long getFrameColormap(unsigned long framenumber, 
 *                         [array, size_is(paletteLength)] out PRUint32 paletteData,
 *                         out unsigned long paletteLength); */
NS_IMETHODIMP imgContainer::GetFrameColormap(PRUint32 framenum, PRUint32 **aPaletteData,
                                             PRUint32 *aPaletteLength)
{
  NS_ENSURE_ARG_POINTER(aPaletteData);
  NS_ENSURE_ARG_POINTER(aPaletteLength);

  if (framenum >= PRUint32(mNumFrames))
    return NS_ERROR_INVALID_ARG;

  imgFrame *frame = GetImgFrame(framenum);
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);

  if (!frame->GetIsPaletted())
    return NS_ERROR_FAILURE;

  frame->GetPaletteData(aPaletteData, aPaletteLength);

  return NS_OK;
}

nsresult imgContainer::InternalAddFrameHelper(PRUint32 framenum, imgFrame *aFrame,
                                              PRUint8 **imageData, PRUint32 *imageLength,
                                              PRUint32 **paletteData, PRUint32 *paletteLength)
{
  if (framenum > PRUint32(mNumFrames))
    return NS_ERROR_INVALID_ARG;

  nsAutoPtr<imgFrame> frame(aFrame);

  if (paletteData && paletteLength)
    frame->GetPaletteData(paletteData, paletteLength);

  frame->GetImageData(imageData, imageLength);

  mFrames.InsertElementAt(framenum, frame.forget());
  mNumFrames++;

  return NS_OK;
}
                                  
nsresult imgContainer::InternalAddFrame(PRUint32 framenum,
                                        PRInt32 aX, PRInt32 aY,
                                        PRInt32 aWidth, PRInt32 aHeight,
                                        gfxASurface::gfxImageFormat aFormat,
                                        PRUint8 aPaletteDepth,
                                        PRUint8 **imageData,
                                        PRUint32 *imageLength,
                                        PRUint32 **paletteData,
                                        PRUint32 *paletteLength)
{
  if (framenum > PRUint32(mNumFrames))
    return NS_ERROR_INVALID_ARG;

  nsAutoPtr<imgFrame> frame(new imgFrame());
  NS_ENSURE_TRUE(frame, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = frame->Init(aX, aY, aWidth, aHeight, aFormat, aPaletteDepth);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mFrames.Length() == 0) {
    return InternalAddFrameHelper(framenum, frame.forget(), imageData, imageLength, 
                                  paletteData, paletteLength);
  }

  if (mFrames.Length() == 1) {
    // Since we're about to add our second frame, initialize animation stuff
    if (!ensureAnimExists())
      return NS_ERROR_OUT_OF_MEMORY;
    
    // If we dispose of the first frame by clearing it, then the
    // First Frame's refresh area is all of itself.
    // RESTORE_PREVIOUS is invalid (assumed to be DISPOSE_CLEAR)
    PRInt32 frameDisposalMethod = mFrames[0]->GetFrameDisposalMethod();
    if (frameDisposalMethod == imgIContainer::kDisposeClear ||
        frameDisposalMethod == imgIContainer::kDisposeRestorePrevious)
      mAnim->firstFrameRefreshArea = mFrames[0]->GetRect();
  }

  // Calculate firstFrameRefreshArea
  // Some gifs are huge but only have a small area that they animate
  // We only need to refresh that small area when Frame 0 comes around again
  nsIntRect frameRect = frame->GetRect();
  mAnim->firstFrameRefreshArea.UnionRect(mAnim->firstFrameRefreshArea, 
                                         frameRect);
  
  rv = InternalAddFrameHelper(framenum, frame.forget(), imageData, imageLength,
                              paletteData, paletteLength);
  
  // If this is our second frame (We've just added our second frame above),
  // count should now be 2.  This must be called after we AppendObject 
  // because StartAnimation checks for > 1 frames
  if (mFrames.Length() == 2)
    StartAnimation();
  
  return rv;
}

/* [noscript] void appendFrame (in PRInt32 aX, in PRInt32 aY, in PRInt32 aWidth, in PRInt32 aHeight, in gfxImageFormat aFormat, [array, size_is (imageLength)] out PRUint8 imageData, out unsigned long imageLength); */
NS_IMETHODIMP imgContainer::AppendFrame(PRInt32 aX, PRInt32 aY, PRInt32 aWidth,
                                        PRInt32 aHeight, 
                                        gfxASurface::gfxImageFormat aFormat,
                                        PRUint8 **imageData,
                                        PRUint32 *imageLength)
{
  NS_ENSURE_ARG_POINTER(imageData);
  NS_ENSURE_ARG_POINTER(imageLength);

  return InternalAddFrame(mNumFrames, aX, aY, aWidth, aHeight, aFormat, 
                          /* aPaletteDepth = */ 0, imageData, imageLength,
                          /* aPaletteData = */ nsnull, 
                          /* aPaletteLength = */ nsnull);
}

/* [noscript] void appendPalettedFrame (in PRInt32 aX, in PRInt32 aY, in PRInt32 aWidth, in PRInt32 aHeight, in gfxImageFormat aFormat, in PRUint8 aPaletteDepth, [array, size_is (imageLength)] out PRUint8 imageData, out unsigned long imageLength, [array, size_is (paletteLength)] out PRUint32 paletteData, out unsigned long paletteLength); */
NS_IMETHODIMP imgContainer::AppendPalettedFrame(PRInt32 aX, PRInt32 aY,
                                                PRInt32 aWidth, PRInt32 aHeight,
                                                gfxASurface::gfxImageFormat aFormat,
                                                PRUint8 aPaletteDepth,
                                                PRUint8 **imageData,
                                                PRUint32 *imageLength,
                                                PRUint32 **paletteData,
                                                PRUint32 *paletteLength)
{
  NS_ENSURE_ARG_POINTER(imageData);
  NS_ENSURE_ARG_POINTER(imageLength);
  NS_ENSURE_ARG_POINTER(paletteData);
  NS_ENSURE_ARG_POINTER(paletteLength);

  return InternalAddFrame(mNumFrames, aX, aY, aWidth, aHeight, aFormat, 
                          aPaletteDepth, imageData, imageLength,
                          paletteData, paletteLength);
}

/*  [noscript] void ensureCleanFrame(in unsigned long aFramenum, in PRInt32 aX, in PRInt32 aY, in PRInt32 aWidth, in PRInt32 aHeight, in gfxImageFormat aFormat, [array, size_is(imageLength)] out PRUint8 imageData, out unsigned long imageLength); */
NS_IMETHODIMP imgContainer::EnsureCleanFrame(PRUint32 aFrameNum, PRInt32 aX, PRInt32 aY,
                                             PRInt32 aWidth, PRInt32 aHeight, 
                                             gfxASurface::gfxImageFormat aFormat,
                                             PRUint8 **imageData, PRUint32 *imageLength)
{
  NS_ENSURE_ARG_POINTER(imageData);
  NS_ENSURE_ARG_POINTER(imageLength);
  if (aFrameNum > PRUint32(mNumFrames))
    return NS_ERROR_INVALID_ARG;

  // Adding a frame that doesn't already exist.
  if (aFrameNum == PRUint32(mNumFrames))
    return InternalAddFrame(aFrameNum, aX, aY, aWidth, aHeight, aFormat, 
                            /* aPaletteDepth = */ 0, imageData, imageLength,
                            /* aPaletteData = */ nsnull, 
                            /* aPaletteLength = */ nsnull);

  imgFrame *frame = GetImgFrame(aFrameNum);
  if (!frame)
    return InternalAddFrame(aFrameNum, aX, aY, aWidth, aHeight, aFormat, 
                            /* aPaletteDepth = */ 0, imageData, imageLength,
                            /* aPaletteData = */ nsnull, 
                            /* aPaletteLength = */ nsnull);

  // See if we can re-use the frame that already exists.
  nsIntRect rect = frame->GetRect();
  if (rect.x != aX || rect.y != aY || rect.width != aWidth || rect.height != aHeight ||
      frame->GetFormat() != aFormat) {
    delete frame;
    return InternalAddFrame(aFrameNum, aX, aY, aWidth, aHeight, aFormat, 
                            /* aPaletteDepth = */ 0, imageData, imageLength,
                            /* aPaletteData = */ nsnull, 
                            /* aPaletteLength = */ nsnull);
  }

  // We can re-use the frame.
  frame->GetImageData(imageData, imageLength);

  return NS_OK;
}


//******************************************************************************
/* void frameUpdated (in unsigned long framenumber, in nsIntRect rect); */
NS_IMETHODIMP imgContainer::FrameUpdated(PRUint32 aFrameNum, nsIntRect &aUpdatedRect)
{
  if (aFrameNum >= PRUint32(mNumFrames))
    return NS_ERROR_INVALID_ARG;

  imgFrame *frame = GetImgFrame(aFrameNum);
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);

  frame->ImageUpdated(aUpdatedRect);

  return NS_OK;
}

//******************************************************************************
/* void setFrameDisposalMethod (in unsigned long framenumber, in PRInt32 aDisposalMethod); */
NS_IMETHODIMP imgContainer::SetFrameDisposalMethod(PRUint32 aFrameNum, PRInt32 aDisposalMethod)
{
  if (aFrameNum >= PRUint32(mNumFrames))
    return NS_ERROR_INVALID_ARG;

  imgFrame *frame = GetImgFrame(aFrameNum);
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);

  frame->SetFrameDisposalMethod(aDisposalMethod);

  return NS_OK;
}

//******************************************************************************
/* void setFrameTimeout (in unsigned long framenumber, in PRInt32 aTimeout); */
NS_IMETHODIMP imgContainer::SetFrameTimeout(PRUint32 aFrameNum, PRInt32 aTimeout)
{
  if (aFrameNum >= PRUint32(mNumFrames))
    return NS_ERROR_INVALID_ARG;

  imgFrame *frame = GetImgFrame(aFrameNum);
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);

  frame->SetTimeout(aTimeout);

  return NS_OK;
}

//******************************************************************************
/* void setFrameBlendMethod (in unsigned long framenumber, in PRInt32 aBlendMethod); */
NS_IMETHODIMP imgContainer::SetFrameBlendMethod(PRUint32 aFrameNum, PRInt32 aBlendMethod)
{
  if (aFrameNum >= PRUint32(mNumFrames))
    return NS_ERROR_INVALID_ARG;

  imgFrame *frame = GetImgFrame(aFrameNum);
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);

  frame->SetBlendMethod(aBlendMethod);

  return NS_OK;
}


//******************************************************************************
/* void setFrameHasNoAlpha (in unsigned long framenumber); */
NS_IMETHODIMP imgContainer::SetFrameHasNoAlpha(PRUint32 aFrameNum)
{
  if (aFrameNum >= PRUint32(mNumFrames))
    return NS_ERROR_INVALID_ARG;

  imgFrame *frame = GetImgFrame(aFrameNum);
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);

  frame->SetHasNoAlpha();

  return NS_OK;
}

//******************************************************************************
/* void endFrameDecode (in unsigned long framenumber); */
NS_IMETHODIMP imgContainer::EndFrameDecode(PRUint32 aFrameNum)
{
  // Assume there's another frame.
  // currentDecodingFrameIndex is 0 based, aFrameNum is 1 based
  if (mAnim)
    mAnim->currentDecodingFrameIndex = aFrameNum;
  
  return NS_OK;
}

//******************************************************************************
/* void decodingComplete (); */
NS_IMETHODIMP imgContainer::DecodingComplete(void)
{
  if (mAnim)
    mAnim->doneDecoding = PR_TRUE;

  // If there's only 1 frame, optimize it.
  // Optimizing animated images is not supported.
  if (mNumFrames == 1)
    return mFrames[0]->Optimize();

  return NS_OK;
}

//******************************************************************************
/* attribute unsigned short animationMode; */
NS_IMETHODIMP imgContainer::GetAnimationMode(PRUint16 *aAnimationMode)
{
  NS_ENSURE_ARG_POINTER(aAnimationMode);
  
  *aAnimationMode = mAnimationMode;
  return NS_OK;
}

//******************************************************************************
/* attribute unsigned short animationMode; */
NS_IMETHODIMP imgContainer::SetAnimationMode(PRUint16 aAnimationMode)
{
  NS_ASSERTION(aAnimationMode == imgIContainer::kNormalAnimMode ||
               aAnimationMode == imgIContainer::kDontAnimMode ||
               aAnimationMode == imgIContainer::kLoopOnceAnimMode,
               "Wrong Animation Mode is being set!");
  
  switch (mAnimationMode = aAnimationMode) {
    case kDontAnimMode:
      StopAnimation();
      break;
    case kNormalAnimMode:
      if (mLoopCount != 0 || 
          (mAnim && (mAnim->currentAnimationFrameIndex + 1 < mNumFrames)))
        StartAnimation();
      break;
    case kLoopOnceAnimMode:
      if (mAnim && (mAnim->currentAnimationFrameIndex + 1 < mNumFrames))
        StartAnimation();
      break;
  }
  
  return NS_OK;
}

//******************************************************************************
/* void startAnimation () */
NS_IMETHODIMP imgContainer::StartAnimation()
{
  if (mAnimationMode == kDontAnimMode || 
      (mAnim && (mAnim->timer || mAnim->animating)))
    return NS_OK;
  
  if (mNumFrames > 1) {
    if (!ensureAnimExists())
      return NS_ERROR_OUT_OF_MEMORY;
    
    // Default timeout to 100: the timer notify code will do the right
    // thing, so just get that started.
    PRInt32 timeout = 100;
    imgFrame *currentFrame = GetCurrentImgFrame();
    if (currentFrame) {
      timeout = currentFrame->GetTimeout();
      if (timeout <= 0) // -1 means display this frame forever
        return NS_OK;
    }
    
    mAnim->timer = do_CreateInstance("@mozilla.org/timer;1");
    NS_ENSURE_TRUE(mAnim->timer, NS_ERROR_OUT_OF_MEMORY);
    
    // The only way animating becomes true is if the timer is created
    mAnim->animating = PR_TRUE;
    mAnim->timer->InitWithCallback(static_cast<nsITimerCallback*>(this),
                                   timeout, nsITimer::TYPE_REPEATING_SLACK);
  }
  
  return NS_OK;
}

//******************************************************************************
/* void stopAnimation (); */
NS_IMETHODIMP imgContainer::StopAnimation()
{
  if (mAnim) {
    mAnim->animating = PR_FALSE;

    if (!mAnim->timer)
      return NS_OK;

    mAnim->timer->Cancel();
    mAnim->timer = nsnull;
  }

  return NS_OK;
}

//******************************************************************************
/* void resetAnimation (); */
NS_IMETHODIMP imgContainer::ResetAnimation()
{
  if (mAnimationMode == kDontAnimMode || 
      !mAnim || mAnim->currentAnimationFrameIndex == 0)
    return NS_OK;

  PRBool oldAnimating = mAnim->animating;

  if (mAnim->animating) {
    nsresult rv = StopAnimation();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mAnim->lastCompositedFrameIndex = -1;
  mAnim->currentAnimationFrameIndex = 0;
  // Update display
  nsCOMPtr<imgIContainerObserver> observer(do_QueryReferent(mObserver));
  if (observer) {
    nsresult rv = RestoreDiscardedData();
    NS_ENSURE_SUCCESS(rv, rv);
    observer->FrameChanged(this, &(mAnim->firstFrameRefreshArea));
  }

  if (oldAnimating)
    return StartAnimation();
  return NS_OK;
}

//******************************************************************************
/* attribute long loopCount; */
NS_IMETHODIMP imgContainer::GetLoopCount(PRInt32 *aLoopCount)
{
  NS_ENSURE_ARG_POINTER(aLoopCount);
  
  *aLoopCount = mLoopCount;
  
  return NS_OK;
}

//******************************************************************************
/* attribute long loopCount; */
NS_IMETHODIMP imgContainer::SetLoopCount(PRInt32 aLoopCount)
{
  // -1  infinite
  //  0  no looping, one iteration
  //  1  one loop, two iterations
  //  ...
  mLoopCount = aLoopCount;

  return NS_OK;
}

static PRBool
DiscardingEnabled(void)
{
  static PRBool inited;
  static PRBool enabled;

  if (!inited) {
    inited = PR_TRUE;

    enabled = (PR_GetEnv("MOZ_DISABLE_IMAGE_DISCARD") == nsnull);
  }

  return enabled;
}

//******************************************************************************
/* void setDiscardable(in string mime_type); */
NS_IMETHODIMP imgContainer::SetDiscardable(const char* aMimeType)
{
  NS_ENSURE_ARG_POINTER(aMimeType);

  if (!DiscardingEnabled())
    return NS_OK;

  if (mDiscardable) {
    NS_WARNING ("imgContainer::SetDiscardable(): cannot change an imgContainer which is already discardable");
    return NS_ERROR_FAILURE;
  }

  mDiscardableMimeType.Assign(aMimeType);
  mDiscardable = PR_TRUE;

  num_containers_with_discardable_data++;
  PR_LOG (gCompressedImageAccountingLog, PR_LOG_DEBUG,
          ("CompressedImageAccounting: Making imgContainer %p (%s) discardable.  "
           "Compressed containers: %d, Compressed data bytes: %lld",
           this,
           aMimeType,
           num_containers_with_discardable_data,
           num_compressed_image_bytes));

  return NS_OK;
}

//******************************************************************************
/* void addRestoreData(in nsIInputStream aInputStream, in unsigned long aCount); */
NS_IMETHODIMP imgContainer::AddRestoreData(const char *aBuffer, PRUint32 aCount)
{
  NS_ENSURE_ARG_POINTER(aBuffer);

  if (!mDiscardable)
    return NS_OK;

  if (mRestoreDataDone) {
    /* We are being called from the decoder while the data is being restored
     * (i.e. we were fully loaded once, then we discarded the image data, then
     * we are being restored).  We don't want to save the compressed data again,
     * since we already have it.
     */
    return NS_OK;
  }

  if (!mRestoreData.AppendElements(aBuffer, aCount))
    return NS_ERROR_OUT_OF_MEMORY;

  num_compressed_image_bytes += aCount;

  PR_LOG (gCompressedImageAccountingLog, PR_LOG_DEBUG,
          ("CompressedImageAccounting: Added compressed data to imgContainer %p (%s).  "
           "Compressed containers: %d, Compressed data bytes: %lld",
           this,
           mDiscardableMimeType.get(),
           num_containers_with_discardable_data,
           num_compressed_image_bytes));

  return NS_OK;
}

/* Note!  buf must be declared as char buf[9]; */
// just used for logging and hashing the header
static void
get_header_str (char *buf, char *data, PRSize data_len)
{
  int i;
  int n;
  static char hex[] = "0123456789abcdef";

  n = data_len < 4 ? data_len : 4;

  for (i = 0; i < n; i++) {
    buf[i * 2]     = hex[(data[i] >> 4) & 0x0f];
    buf[i * 2 + 1] = hex[data[i] & 0x0f];
  }

  buf[i * 2] = 0;
}

//******************************************************************************
/* void restoreDataDone(); */
NS_IMETHODIMP imgContainer::RestoreDataDone (void)
{
  // If image is not discardable, don't start discard timer
  if (!mDiscardable)
    return NS_OK;

  if (mRestoreDataDone)
    return NS_OK;

  mRestoreData.Compact();

  mRestoreDataDone = PR_TRUE;

  if (PR_LOG_TEST(gCompressedImageAccountingLog, PR_LOG_DEBUG)) {
    char buf[9];
    get_header_str(buf, mRestoreData.Elements(), mRestoreData.Length());
    PR_LOG (gCompressedImageAccountingLog, PR_LOG_DEBUG,
            ("CompressedImageAccounting: imgContainer::RestoreDataDone() - data is done for container %p (%s), %d real frames (cached as %d frames) - header %p is 0x%s (length %d)",
             this,
             mDiscardableMimeType.get(),
             mFrames.Length (),
             mNumFrames,
             mRestoreData.Elements(),
             buf,
             mRestoreData.Length()));
  }

  return ResetDiscardTimer();
}

//******************************************************************************
/* void notify(in nsITimer timer); */
NS_IMETHODIMP imgContainer::Notify(nsITimer *timer)
{
  // Note that as long as the image is animated, it will not be discarded, 
  // so this should never happen...
  nsresult rv = RestoreDiscardedData();
  NS_ENSURE_SUCCESS(rv, rv);

  // This should never happen since the timer is only set up in StartAnimation()
  // after mAnim is checked to exist.
  NS_ENSURE_TRUE(mAnim, NS_ERROR_UNEXPECTED);
  NS_ASSERTION(mAnim->timer == timer,
               "imgContainer::Notify() called with incorrect timer");

  if (!mAnim->animating || !mAnim->timer)
    return NS_OK;

  nsCOMPtr<imgIContainerObserver> observer(do_QueryReferent(mObserver));
  if (!observer) {
    // the imgRequest that owns us is dead, we should die now too.
    StopAnimation();
    return NS_OK;
  }

  if (mNumFrames == 0)
    return NS_OK;
  
  imgFrame *nextFrame = nsnull;
  PRInt32 previousFrameIndex = mAnim->currentAnimationFrameIndex;
  PRInt32 nextFrameIndex = mAnim->currentAnimationFrameIndex + 1;
  PRInt32 timeout = 0;

  // If we're done decoding the next frame, go ahead and display it now and
  // reinit the timer with the next frame's delay time.
  // currentDecodingFrameIndex is not set until the second frame has
  // finished decoding (see EndFrameDecode)
  if (mAnim->doneDecoding || 
      (nextFrameIndex < mAnim->currentDecodingFrameIndex)) {
    if (mNumFrames == nextFrameIndex) {
      // End of Animation

      // If animation mode is "loop once", it's time to stop animating
      if (mAnimationMode == kLoopOnceAnimMode || mLoopCount == 0) {
        StopAnimation();
        return NS_OK;
      } else {
        // We may have used compositingFrame to build a frame, and then copied
        // it back into mFrames[..].  If so, delete composite to save memory
        if (mAnim->compositingFrame && mAnim->lastCompositedFrameIndex == -1)
          mAnim->compositingFrame = nsnull;
      }

      nextFrameIndex = 0;
      if (mLoopCount > 0)
        mLoopCount--;
    }

    if (!(nextFrame = mFrames[nextFrameIndex])) {
      // something wrong with the next frame, skip it
      mAnim->currentAnimationFrameIndex = nextFrameIndex;
      mAnim->timer->SetDelay(100);
      return NS_OK;
    }
    timeout = nextFrame->GetTimeout();

  } else if (nextFrameIndex == mAnim->currentDecodingFrameIndex) {
    // Uh oh, the frame we want to show is currently being decoded (partial)
    // Wait a bit and try again
    mAnim->timer->SetDelay(100);
    return NS_OK;
  } else { //  (nextFrameIndex > currentDecodingFrameIndex)
    // We shouldn't get here. However, if we are requesting a frame
    // that hasn't been decoded yet, go back to the last frame decoded
    NS_WARNING("imgContainer::Notify()  Frame is passed decoded frame");
    nextFrameIndex = mAnim->currentDecodingFrameIndex;
    if (!(nextFrame = mFrames[nextFrameIndex])) {
      // something wrong with the next frame, skip it
      mAnim->currentAnimationFrameIndex = nextFrameIndex;
      mAnim->timer->SetDelay(100);
      return NS_OK;
    }
    timeout = nextFrame->GetTimeout();
  }

  if (timeout > 0)
    mAnim->timer->SetDelay(timeout);
  else
    StopAnimation();

  nsIntRect dirtyRect;
  imgFrame *frameToUse = nsnull;

  if (nextFrameIndex == 0) {
    frameToUse = nextFrame;
    dirtyRect = mAnim->firstFrameRefreshArea;
  } else {
    imgFrame *prevFrame = mFrames[previousFrameIndex];
    if (!prevFrame)
      return NS_OK;

    // Change frame and announce it
    if (NS_FAILED(DoComposite(&frameToUse, &dirtyRect, prevFrame,
                              nextFrame, nextFrameIndex))) {
      // something went wrong, move on to next
      NS_WARNING("imgContainer::Notify(): Composing Frame Failed\n");
      mAnim->currentAnimationFrameIndex = nextFrameIndex;
      return NS_OK;
    }
  }
  // Set currentAnimationFrameIndex at the last possible moment
  mAnim->currentAnimationFrameIndex = nextFrameIndex;
  // Refreshes the screen
  observer->FrameChanged(this, &dirtyRect);
  
  return NS_OK;
}

//******************************************************************************
// DoComposite gets called when the timer for animation get fired and we have to
// update the composited frame of the animation.
nsresult imgContainer::DoComposite(imgFrame** aFrameToUse,
                                   nsIntRect* aDirtyRect,
                                   imgFrame* aPrevFrame,
                                   imgFrame* aNextFrame,
                                   PRInt32 aNextFrameIndex)
{
  NS_ENSURE_ARG_POINTER(aDirtyRect);
  NS_ENSURE_ARG_POINTER(aPrevFrame);
  NS_ENSURE_ARG_POINTER(aNextFrame);
  NS_ENSURE_ARG_POINTER(aFrameToUse);

  PRInt32 prevFrameDisposalMethod = aPrevFrame->GetFrameDisposalMethod();
  if (prevFrameDisposalMethod == imgIContainer::kDisposeRestorePrevious &&
      !mAnim->compositingPrevFrame)
    prevFrameDisposalMethod = imgIContainer::kDisposeClear;

  nsIntRect prevFrameRect = aPrevFrame->GetRect();
  PRBool isFullPrevFrame = (prevFrameRect.x == 0 && prevFrameRect.y == 0 &&
                            prevFrameRect.width == mSize.width &&
                            prevFrameRect.height == mSize.height);

  // Optimization: DisposeClearAll if the previous frame is the same size as
  //               container and it's clearing itself
  if (isFullPrevFrame && 
      (prevFrameDisposalMethod == imgIContainer::kDisposeClear))
    prevFrameDisposalMethod = imgIContainer::kDisposeClearAll;

  PRInt32 nextFrameDisposalMethod = aNextFrame->GetFrameDisposalMethod();
  nsIntRect nextFrameRect = aNextFrame->GetRect();
  PRBool isFullNextFrame = (nextFrameRect.x == 0 && nextFrameRect.y == 0 &&
                            nextFrameRect.width == mSize.width &&
                            nextFrameRect.height == mSize.height);

  if (!aNextFrame->GetIsPaletted()) {
    // Optimization: Skip compositing if the previous frame wants to clear the
    //               whole image
    if (prevFrameDisposalMethod == imgIContainer::kDisposeClearAll) {
      aDirtyRect->SetRect(0, 0, mSize.width, mSize.height);
      *aFrameToUse = aNextFrame;
      return NS_OK;
    }
  
    // Optimization: Skip compositing if this frame is the same size as the
    //               container and it's fully drawing over prev frame (no alpha)
    if (isFullNextFrame &&
        (nextFrameDisposalMethod != imgIContainer::kDisposeRestorePrevious) &&
        !aNextFrame->GetHasAlpha()) {
      aDirtyRect->SetRect(0, 0, mSize.width, mSize.height);
      *aFrameToUse = aNextFrame;
      return NS_OK;
    }
  }

  // Calculate area that needs updating
  switch (prevFrameDisposalMethod) {
    default:
    case imgIContainer::kDisposeNotSpecified:
    case imgIContainer::kDisposeKeep:
      *aDirtyRect = nextFrameRect;
      break;

    case imgIContainer::kDisposeClearAll:
      // Whole image container is cleared
      aDirtyRect->SetRect(0, 0, mSize.width, mSize.height);
      break;

    case imgIContainer::kDisposeClear:
      // Calc area that needs to be redrawn (the combination of previous and
      // this frame)
      // XXX - This could be done with multiple framechanged calls
      //       Having prevFrame way at the top of the image, and nextFrame
      //       way at the bottom, and both frames being small, we'd be
      //       telling framechanged to refresh the whole image when only two
      //       small areas are needed.
      aDirtyRect->UnionRect(nextFrameRect, prevFrameRect);
      break;

    case imgIContainer::kDisposeRestorePrevious:
      aDirtyRect->SetRect(0, 0, mSize.width, mSize.height);
      break;
  }

  // Optimization:
  //   Skip compositing if the last composited frame is this frame
  //   (Only one composited frame was made for this animation.  Example:
  //    Only Frame 3 of a 10 frame image required us to build a composite frame
  //    On the second loop, we do not need to rebuild the frame
  //    since it's still sitting in compositingFrame)
  if (mAnim->lastCompositedFrameIndex == aNextFrameIndex) {
    *aFrameToUse = mAnim->compositingFrame;
    return NS_OK;
  }

  PRBool needToBlankComposite = PR_FALSE;

  // Create the Compositing Frame
  if (!mAnim->compositingFrame) {
    mAnim->compositingFrame = new imgFrame();
    if (!mAnim->compositingFrame) {
      NS_WARNING("Failed to init compositingFrame!\n");
      return NS_ERROR_OUT_OF_MEMORY;
    }
    nsresult rv = mAnim->compositingFrame->Init(0, 0, mSize.width, mSize.height,
                                                gfxASurface::ImageFormatARGB32);
    NS_ENSURE_SUCCESS(rv, rv);
    needToBlankComposite = PR_TRUE;
  } else if (aNextFrameIndex == 1) {
    // When we are looping the compositing frame needs to be cleared.
    needToBlankComposite = PR_TRUE;
  }

  // More optimizations possible when next frame is not transparent
  PRBool doDisposal = PR_TRUE;
  if (!aNextFrame->GetHasAlpha()) {
    if (isFullNextFrame) {
      // Optimization: No need to dispose prev.frame when 
      // next frame is full frame and not transparent.
      doDisposal = PR_FALSE;
      // No need to blank the composite frame
      needToBlankComposite = PR_FALSE;
    } else {
      if ((prevFrameRect.x >= nextFrameRect.x) &&
          (prevFrameRect.y >= nextFrameRect.y) &&
          (prevFrameRect.x + prevFrameRect.width <= nextFrameRect.x + nextFrameRect.width) &&
          (prevFrameRect.y + prevFrameRect.height <= nextFrameRect.y + nextFrameRect.height)) {
        // Optimization: No need to dispose prev.frame when 
        // next frame fully overlaps previous frame.
        doDisposal = PR_FALSE;  
      }
    }      
  }

  if (doDisposal) {
    // Dispose of previous: clear, restore, or keep (copy)
    switch (prevFrameDisposalMethod) {
      case imgIContainer::kDisposeClear:
        if (needToBlankComposite) {
          // If we just created the composite, it could have anything in it's
          // buffer. Clear whole frame
          ClearFrame(mAnim->compositingFrame);
        } else {
          // Only blank out previous frame area (both color & Mask/Alpha)
          ClearFrame(mAnim->compositingFrame, prevFrameRect);
        }
        break;
  
      case imgIContainer::kDisposeClearAll:
        ClearFrame(mAnim->compositingFrame);
        break;
  
      case imgIContainer::kDisposeRestorePrevious:
        // It would be better to copy only the area changed back to
        // compositingFrame.
        if (mAnim->compositingPrevFrame) {
          CopyFrameImage(mAnim->compositingPrevFrame, mAnim->compositingFrame);
  
          // destroy only if we don't need it for this frame's disposal
          if (nextFrameDisposalMethod != imgIContainer::kDisposeRestorePrevious)
            mAnim->compositingPrevFrame = nsnull;
        } else {
          ClearFrame(mAnim->compositingFrame);
        }
        break;
      
      default:
        // Copy previous frame into compositingFrame before we put the new frame on top
        // Assumes that the previous frame represents a full frame (it could be
        // smaller in size than the container, as long as the frame before it erased
        // itself)
        // Note: Frame 1 never gets into DoComposite(), so (aNextFrameIndex - 1) will
        // always be a valid frame number.
        if (mAnim->lastCompositedFrameIndex != aNextFrameIndex - 1) {
          if (isFullPrevFrame && !aPrevFrame->GetIsPaletted())
            // Just copy the bits
            CopyFrameImage(aPrevFrame, mAnim->compositingFrame);
          } else {
            if (needToBlankComposite) {
              // Only blank composite when prev is transparent or not full.
              if (aPrevFrame->GetHasAlpha() || !isFullPrevFrame) {
                ClearFrame(mAnim->compositingFrame);
              }
            }
            DrawFrameTo(aPrevFrame, mAnim->compositingFrame, prevFrameRect);
          }
        }
  } else if (needToBlankComposite) {
    // If we just created the composite, it could have anything in it's
    // buffers. Clear them
    ClearFrame(mAnim->compositingFrame);
  }

  // Check if the frame we are composing wants the previous image restored afer
  // it is done. Don't store it (again) if last frame wanted its image restored
  // too
  if ((nextFrameDisposalMethod == imgIContainer::kDisposeRestorePrevious) &&
      (prevFrameDisposalMethod != imgIContainer::kDisposeRestorePrevious)) {
    // We are storing the whole image.
    // It would be better if we just stored the area that nextFrame is going to
    // overwrite.
    if (!mAnim->compositingPrevFrame) {
      mAnim->compositingPrevFrame = new imgFrame();
      if (!mAnim->compositingPrevFrame) {
        NS_WARNING("Failed to init compositingFrame!\n");
        return NS_ERROR_OUT_OF_MEMORY;
      }
      nsresult rv = mAnim->compositingPrevFrame->Init(0, 0, mSize.width, mSize.height,
                                                      gfxASurface::ImageFormatARGB32);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    CopyFrameImage(mAnim->compositingFrame, mAnim->compositingPrevFrame);
  }

  // blit next frame into it's correct spot
  DrawFrameTo(aNextFrame, mAnim->compositingFrame, nextFrameRect);

  // Set timeout of CompositeFrame to timeout of frame we just composed
  // Bug 177948
  PRInt32 timeout = aNextFrame->GetTimeout();
  mAnim->compositingFrame->SetTimeout(timeout);

  // Tell the image that it is fully 'downloaded'.
  nsresult rv = mAnim->compositingFrame->ImageUpdated(mAnim->compositingFrame->GetRect());
  if (NS_FAILED(rv)) {
    return rv;
  }

  // We don't want to keep composite images for 8bit frames...
  if (isFullNextFrame && mAnimationMode == kNormalAnimMode && mLoopCount != 0 &&
      !aNextFrame->GetIsPaletted()) {
    // We have a composited full frame
    // Store the composited frame into the mFrames[..] so we don't have to
    // continuously re-build it
    // Then set the previous frame's disposal to CLEAR_ALL so we just draw the
    // frame next time around
    if (CopyFrameImage(mAnim->compositingFrame, aNextFrame)) {
      aPrevFrame->SetFrameDisposalMethod(imgIContainer::kDisposeClearAll);
      mAnim->lastCompositedFrameIndex = -1;
      *aFrameToUse = aNextFrame;
      return NS_OK;
    }
  }

  mAnim->lastCompositedFrameIndex = aNextFrameIndex;
  *aFrameToUse = mAnim->compositingFrame;

  return NS_OK;
}

//******************************************************************************
// Fill aFrame with black. Does also clears the mask.
void imgContainer::ClearFrame(imgFrame *aFrame)
{
  if (!aFrame)
    return;

  aFrame->LockImageData();

  nsRefPtr<gfxASurface> surf;
  aFrame->GetSurface(getter_AddRefs(surf));

  // Erase the surface to transparent
  gfxContext ctx(surf);
  ctx.SetOperator(gfxContext::OPERATOR_CLEAR);
  ctx.Paint();

  aFrame->UnlockImageData();
}

//******************************************************************************
void imgContainer::ClearFrame(imgFrame *aFrame, nsIntRect &aRect)
{
  if (!aFrame || aRect.width <= 0 || aRect.height <= 0)
    return;

  aFrame->LockImageData();

  nsRefPtr<gfxASurface> surf;
  aFrame->GetSurface(getter_AddRefs(surf));

  // Erase the destination rectangle to transparent
  gfxContext ctx(surf);
  ctx.SetOperator(gfxContext::OPERATOR_CLEAR);
  ctx.Rectangle(gfxRect(aRect.x, aRect.y, aRect.width, aRect.height));
  ctx.Fill();

  aFrame->UnlockImageData();
}


//******************************************************************************
// Whether we succeed or fail will not cause a crash, and there's not much
// we can do about a failure, so there we don't return a nsresult
PRBool imgContainer::CopyFrameImage(imgFrame *aSrcFrame,
                                    imgFrame *aDstFrame)
{
  PRUint8* aDataSrc;
  PRUint8* aDataDest;
  PRUint32 aDataLengthSrc;
  PRUint32 aDataLengthDest;

  if (!aSrcFrame || !aDstFrame)
    return PR_FALSE;

  if (NS_FAILED(aDstFrame->LockImageData()))
    return PR_FALSE;

  // Copy Image Over
  aSrcFrame->GetImageData(&aDataSrc, &aDataLengthSrc);
  aDstFrame->GetImageData(&aDataDest, &aDataLengthDest);
  if (!aDataDest || !aDataSrc || aDataLengthDest != aDataLengthSrc) {
    aDstFrame->UnlockImageData();
    return PR_FALSE;
  }
  memcpy(aDataDest, aDataSrc, aDataLengthSrc);
  aDstFrame->UnlockImageData();

  return PR_TRUE;
}

//******************************************************************************
/* 
 * aSrc is the current frame being drawn,
 * aDst is the composition frame where the current frame is drawn into.
 * aSrcRect is the size of the current frame, and the position of that frame
 *          in the composition frame.
 */
nsresult imgContainer::DrawFrameTo(imgFrame *aSrc,
                                   imgFrame *aDst, 
                                   nsIntRect& aSrcRect)
{
  NS_ENSURE_ARG_POINTER(aSrc);
  NS_ENSURE_ARG_POINTER(aDst);

  nsIntRect dstRect = aDst->GetRect();

  // According to both AGIF and APNG specs, offsets are unsigned
  if (aSrcRect.x < 0 || aSrcRect.y < 0) {
    NS_WARNING("imgContainer::DrawFrameTo: negative offsets not allowed");
    return NS_ERROR_FAILURE;
  }
  // Outside the destination frame, skip it
  if ((aSrcRect.x > dstRect.width) || (aSrcRect.y > dstRect.height)) {
    return NS_OK;
  }

  if (aSrc->GetIsPaletted()) {
    // Larger than the destination frame, clip it
    PRInt32 width = PR_MIN(aSrcRect.width, dstRect.width - aSrcRect.x);
    PRInt32 height = PR_MIN(aSrcRect.height, dstRect.height - aSrcRect.y);

    // The clipped image must now fully fit within destination image frame
    NS_ASSERTION((aSrcRect.x >= 0) && (aSrcRect.y >= 0) &&
                 (aSrcRect.x + width <= dstRect.width) &&
                 (aSrcRect.y + height <= dstRect.height),
                "imgContainer::DrawFrameTo: Invalid aSrcRect");

    // clipped image size may be smaller than source, but not larger
    NS_ASSERTION((width <= aSrcRect.width) && (height <= aSrcRect.height),
                 "imgContainer::DrawFrameTo: source must be smaller than dest");

    if (NS_FAILED(aDst->LockImageData()))
      return NS_ERROR_FAILURE;

    // Get pointers to image data
    PRUint32 size;
    PRUint8 *srcPixels;
    PRUint32 *colormap;
    PRUint32 *dstPixels;

    aSrc->GetImageData(&srcPixels, &size);
    aSrc->GetPaletteData(&colormap, &size);
    aDst->GetImageData((PRUint8 **)&dstPixels, &size);
    if (!srcPixels || !dstPixels || !colormap) {
      aDst->UnlockImageData();
      return NS_ERROR_FAILURE;
    }

    // Skip to the right offset
    dstPixels += aSrcRect.x + (aSrcRect.y * dstRect.width);
    if (!aSrc->GetHasAlpha()) {
      for (PRInt32 r = height; r > 0; --r) {
        for (PRInt32 c = 0; c < width; c++) {
          dstPixels[c] = colormap[srcPixels[c]];
        }
        // Go to the next row in the source resp. destination image
        srcPixels += aSrcRect.width;
        dstPixels += dstRect.width;
      }
    } else {
      for (PRInt32 r = height; r > 0; --r) {
        for (PRInt32 c = 0; c < width; c++) {
          const PRUint32 color = colormap[srcPixels[c]];
          if (color)
            dstPixels[c] = color;
        }
        // Go to the next row in the source resp. destination image
        srcPixels += aSrcRect.width;
        dstPixels += dstRect.width;
      }
    }

    aDst->UnlockImageData();
    return NS_OK;
  }

  nsRefPtr<gfxPattern> srcPatt;
  aSrc->GetPattern(getter_AddRefs(srcPatt));

  aDst->LockImageData();
  nsRefPtr<gfxASurface> dstSurf;
  aDst->GetSurface(getter_AddRefs(dstSurf));

  gfxContext dst(dstSurf);
  dst.Translate(gfxPoint(aSrcRect.x, aSrcRect.y));
  dst.Rectangle(gfxRect(0, 0, aSrcRect.width, aSrcRect.height), PR_TRUE);
  
  // first clear the surface if the blend flag says so
  PRInt32 blendMethod = aSrc->GetBlendMethod();
  if (blendMethod == imgIContainer::kBlendSource) {
    gfxContext::GraphicsOperator defaultOperator = dst.CurrentOperator();
    dst.SetOperator(gfxContext::OPERATOR_CLEAR);
    dst.Fill();
    dst.SetOperator(defaultOperator);
  }
  dst.SetPattern(srcPatt);
  dst.Paint();

  aDst->UnlockImageData();

  return NS_OK;
}


/********* Methods to implement lazy allocation of nsIProperties object *************/
NS_IMETHODIMP imgContainer::Get(const char *prop, const nsIID & iid, void * *result)
{
  if (!mProperties)
    return NS_ERROR_FAILURE;
  return mProperties->Get(prop, iid, result);
}

NS_IMETHODIMP imgContainer::Set(const char *prop, nsISupports *value)
{
  if (!mProperties)
    mProperties = do_CreateInstance("@mozilla.org/properties;1");
  if (!mProperties)
    return NS_ERROR_OUT_OF_MEMORY;
  return mProperties->Set(prop, value);
}

NS_IMETHODIMP imgContainer::Has(const char *prop, PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  if (!mProperties) {
    *_retval = PR_FALSE;
    return NS_OK;
  }
  return mProperties->Has(prop, _retval);
}

NS_IMETHODIMP imgContainer::Undefine(const char *prop)
{
  if (!mProperties)
    return NS_ERROR_FAILURE;
  return mProperties->Undefine(prop);
}

NS_IMETHODIMP imgContainer::GetKeys(PRUint32 *count, char ***keys)
{
  if (!mProperties) {
    *count = 0;
    *keys = nsnull;
    return NS_OK;
  }
  return mProperties->GetKeys(count, keys);
}

static int
get_discard_timer_ms (void)
{
  /* FIXME: don't hardcode this */
  return 15000; /* 15 seconds */
}

void
imgContainer::sDiscardTimerCallback(nsITimer *aTimer, void *aClosure)
{
  imgContainer *self = (imgContainer *) aClosure;

  NS_ASSERTION(aTimer == self->mDiscardTimer,
               "imgContainer::DiscardTimerCallback() got a callback for an unknown timer");

  self->mDiscardTimer = nsnull;

  int old_frame_count = self->mFrames.Length();

  // Don't discard animated images, because we don't handle that very well. (See bug 414259.)
  if (self->mAnim) {
    return;
  }

  for (int i = 0; i < old_frame_count; ++i)
    delete self->mFrames[i];
  self->mFrames.Clear();

  self->mDiscarded = PR_TRUE;

  PR_LOG(gCompressedImageAccountingLog, PR_LOG_DEBUG,
         ("CompressedImageAccounting: discarded uncompressed image data from imgContainer %p (%s) - %d frames (cached count: %d); "
          "Compressed containers: %d, Compressed data bytes: %lld",
          self,
          self->mDiscardableMimeType.get(),
          old_frame_count,
          self->mNumFrames,
          num_containers_with_discardable_data,
          num_compressed_image_bytes));
}

nsresult
imgContainer::ResetDiscardTimer (void)
{
  if (!mRestoreDataDone)
    return NS_OK;

  if (mDiscardTimer) {
    /* Cancel current timer */
    nsresult rv = mDiscardTimer->Cancel();
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
    mDiscardTimer = nsnull;
  }

  /* Don't activate timer when we are animating... */
  if (mAnim && mAnim->animating)
    return NS_OK;

  if (!mDiscardTimer) {
    mDiscardTimer = do_CreateInstance("@mozilla.org/timer;1");
    NS_ENSURE_TRUE(mDiscardTimer, NS_ERROR_OUT_OF_MEMORY);
  }

  return mDiscardTimer->InitWithFuncCallback(sDiscardTimerCallback,
                                             (void *) this,
                                             get_discard_timer_ms (),
                                             nsITimer::TYPE_ONE_SHOT);
}

nsresult
imgContainer::RestoreDiscardedData(void)
{
  // mRestoreDataDone = PR_TRUE means that we want to timeout and then discard the image frames
  // So, we only need to restore, if mRestoreDataDone is true, and then only when the frames are discarded...
  if (!mRestoreDataDone) 
    return NS_OK;

  // Reset timer, as the frames are accessed
  nsresult rv = ResetDiscardTimer();
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mDiscarded)
    return NS_OK;

  int num_expected_frames = mNumFrames;

  // To prevent that ReloadImages is called multiple times, reset the flag before reloading
  mDiscarded = PR_FALSE;

  rv = ReloadImages();
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ASSERTION (mNumFrames == PRInt32(mFrames.Length()),
                "number of restored image frames doesn't match");
  NS_ASSERTION (num_expected_frames == mNumFrames,
                "number of restored image frames doesn't match the original number of frames!");
  
  PR_LOG (gCompressedImageAccountingLog, PR_LOG_DEBUG,
          ("CompressedImageAccounting: imgContainer::RestoreDiscardedData() restored discarded data "
           "for imgContainer %p (%s) - %d image frames.  "
           "Compressed containers: %d, Compressed data bytes: %lld",
           this,
           mDiscardableMimeType.get(),
           mNumFrames,
           num_containers_with_discardable_data,
           num_compressed_image_bytes));

  return NS_OK;
}

//******************************************************************************
/* [noscript] void draw(in gfxContext aContext, in gfxGraphicsFilter aFilter, in gfxMatrix aUserSpaceToImageSpace, in gfxRect aFill, in nsIntRect aSubimage); */ 
NS_IMETHODIMP imgContainer::Draw(gfxContext *aContext, gfxPattern::GraphicsFilter aFilter, 
                                 gfxMatrix &aUserSpaceToImageSpace, gfxRect &aFill,
                                 nsIntRect &aSubimage)
{
  NS_ENSURE_ARG_POINTER(aContext);

  imgFrame *frame = GetCurrentImgFrame();
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);

  nsIntRect framerect = frame->GetRect();
  nsIntMargin padding(framerect.x, framerect.y, 
                      mSize.width - framerect.XMost(),
                      mSize.height - framerect.YMost());

  frame->Draw(aContext, aFilter, aUserSpaceToImageSpace, aFill, padding, aSubimage);

  return NS_OK;
}

class ContainerLoader : public imgILoad,
                        public imgIDecoderObserver,
                        public nsSupportsWeakReference
{
public:

  NS_DECL_ISUPPORTS
  NS_DECL_IMGILOAD
  NS_DECL_IMGIDECODEROBSERVER
  NS_DECL_IMGICONTAINEROBSERVER

  ContainerLoader(void);

private:

  nsCOMPtr<imgIContainer> mContainer;
};

NS_IMPL_ISUPPORTS4 (ContainerLoader, imgILoad, imgIDecoderObserver, imgIContainerObserver, nsISupportsWeakReference)

ContainerLoader::ContainerLoader (void)
{
}

/* Implement imgILoad::image getter */
NS_IMETHODIMP
ContainerLoader::GetImage(imgIContainer **aImage)
{
  *aImage = mContainer;
  NS_IF_ADDREF (*aImage);
  return NS_OK;
}

/* Implement imgILoad::image setter */
NS_IMETHODIMP
ContainerLoader::SetImage(imgIContainer *aImage)
{
  mContainer = aImage;
  return NS_OK;
}

/* Implement imgILoad::isMultiPartChannel getter */
NS_IMETHODIMP
ContainerLoader::GetIsMultiPartChannel(PRBool *aIsMultiPartChannel)
{
  *aIsMultiPartChannel = PR_FALSE; /* FIXME: is this always right? */
  return NS_OK;
}

/* Implement imgIDecoderObserver::onStartRequest() */
NS_IMETHODIMP
ContainerLoader::OnStartRequest(imgIRequest *aRequest)
{
  return NS_OK;
}

/* Implement imgIDecoderObserver::onStartDecode() */
NS_IMETHODIMP
ContainerLoader::OnStartDecode(imgIRequest *aRequest)
{
  return NS_OK;
}

/* Implement imgIDecoderObserver::onStartContainer() */
NS_IMETHODIMP
ContainerLoader::OnStartContainer(imgIRequest *aRequest, imgIContainer *aContainer)
{
  return NS_OK;
}

/* Implement imgIDecoderObserver::onStartFrame() */
NS_IMETHODIMP
ContainerLoader::OnStartFrame(imgIRequest *aRequest, PRUint32 aFrame)
{
  return NS_OK;
}

/* Implement imgIDecoderObserver::onDataAvailable() */
NS_IMETHODIMP
ContainerLoader::OnDataAvailable(imgIRequest *aRequest, PRBool aCurrentFrame, const nsIntRect * aRect)
{
  return NS_OK;
}

/* Implement imgIDecoderObserver::onStopFrame() */
NS_IMETHODIMP
ContainerLoader::OnStopFrame(imgIRequest *aRequest, PRUint32 aFrame)
{
  return NS_OK;
}

/* Implement imgIDecoderObserver::onStopContainer() */
NS_IMETHODIMP
ContainerLoader::OnStopContainer(imgIRequest *aRequest, imgIContainer *aContainer)
{
  return NS_OK;
}

/* Implement imgIDecoderObserver::onStopDecode() */
NS_IMETHODIMP
ContainerLoader::OnStopDecode(imgIRequest *aRequest, nsresult status, const PRUnichar *statusArg)
{
  return NS_OK;
}

/* Implement imgIDecoderObserver::onStopRequest() */
NS_IMETHODIMP
ContainerLoader::OnStopRequest(imgIRequest *aRequest, PRBool aIsLastPart)
{
  return NS_OK;
}

/* implement imgIContainerObserver::frameChanged() */
NS_IMETHODIMP
ContainerLoader::FrameChanged(imgIContainer *aContainer, nsIntRect * aDirtyRect)
{
  return NS_OK;
}

nsresult
imgContainer::ReloadImages(void)
{
  NS_ASSERTION(!mRestoreData.IsEmpty(),
               "imgContainer::ReloadImages(): mRestoreData should not be empty");
  NS_ASSERTION(mRestoreDataDone,
               "imgContainer::ReloadImages(): mRestoreDataDone shoudl be true!");

  mNumFrames = 0;
  NS_ASSERTION(mFrames.Length() == 0,
               "imgContainer::ReloadImages(): mFrames should be empty");

  nsCAutoString decoderCID(NS_LITERAL_CSTRING("@mozilla.org/image/decoder;2?type=") + mDiscardableMimeType);

  nsCOMPtr<imgIDecoder> decoder = do_CreateInstance(decoderCID.get());
  if (!decoder) {
    PR_LOG(gCompressedImageAccountingLog, PR_LOG_WARNING,
           ("CompressedImageAccounting: imgContainer::ReloadImages() could not create decoder for %s",
            mDiscardableMimeType.get()));
    return NS_IMAGELIB_ERROR_NO_DECODER;
  }

  nsCOMPtr<imgILoad> loader = new ContainerLoader();
  if (!loader) {
    PR_LOG(gCompressedImageAccountingLog, PR_LOG_WARNING,
           ("CompressedImageAccounting: imgContainer::ReloadImages() could not allocate ContainerLoader "
            "when reloading the images for container %p",
            this));
    return NS_ERROR_OUT_OF_MEMORY;
  }

  loader->SetImage(this);

  nsresult result = decoder->Init(loader);
  if (NS_FAILED(result)) {
    PR_LOG(gCompressedImageAccountingLog, PR_LOG_WARNING,
           ("CompressedImageAccounting: imgContainer::ReloadImages() image container %p "
            "failed to initialize the decoder (%s)",
            this,
            mDiscardableMimeType.get()));
    return result;
  }

  nsCOMPtr<nsIInputStream> stream;
  result = NS_NewByteInputStream(getter_AddRefs(stream), mRestoreData.Elements(), mRestoreData.Length(), NS_ASSIGNMENT_DEPEND);
  NS_ENSURE_SUCCESS(result, result);

  if (PR_LOG_TEST(gCompressedImageAccountingLog, PR_LOG_DEBUG)) {
    char buf[9];
    get_header_str(buf, mRestoreData.Elements(), mRestoreData.Length());
    PR_LOG(gCompressedImageAccountingLog, PR_LOG_WARNING,
           ("CompressedImageAccounting: imgContainer::ReloadImages() starting to restore images for container %p (%s) - "
            "header %p is 0x%s (length %d)",
            this,
            mDiscardableMimeType.get(),
            mRestoreData.Elements(),
            buf,
            mRestoreData.Length()));
  }

  // |WriteFrom()| may fail if the original data is broken.
  PRUint32 written;
  (void)decoder->WriteFrom(stream, mRestoreData.Length(), &written);

  result = decoder->Flush();
  NS_ENSURE_SUCCESS(result, result);

  result = decoder->Close();
  NS_ENSURE_SUCCESS(result, result);

  NS_ASSERTION(PRInt32(mFrames.Length()) == mNumFrames,
               "imgContainer::ReloadImages(): the restored mFrames.Length() doesn't match mNumFrames!");

  return result;
}
