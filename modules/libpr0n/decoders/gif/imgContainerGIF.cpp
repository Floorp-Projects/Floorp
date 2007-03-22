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
 *   Chris Saari <saari@netscape.com>
 *   Asko Tontti <atontti@cc.hut.fi>
 *   Arron Mogge <paper@animecity.nu>
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

#include "imgContainerGIF.h"

#include "nsIServiceManager.h"
#include "nsIImage.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsMemory.h"
#include "gfxContext.h"

NS_IMPL_ISUPPORTS2(imgContainerGIF, imgIContainer, nsITimerCallback)

//******************************************************************************
imgContainerGIF::imgContainerGIF()
  : mObserver(nsnull)
  , mSize(0,0)
  , mFirstFrameRefreshArea()
  , mCurrentDecodingFrameIndex(0)
  , mCurrentAnimationFrameIndex(0)
  , mLastCompositedFrameIndex(-1)
  , mDoneDecoding(PR_FALSE)
  , mAnimating(PR_FALSE)
  , mAnimationMode(kNormalAnimMode)
  , mLoopCount(-1)
{
  /* member initializers and constructor code */
}

//******************************************************************************
imgContainerGIF::~imgContainerGIF()
{
  if (mTimer)
    mTimer->Cancel();
}

//******************************************************************************
/* void init (in PRInt32 aWidth, in PRInt32 aHeight,
              in imgIContainerObserver aObserver); */
NS_IMETHODIMP imgContainerGIF::Init(PRInt32 aWidth, PRInt32 aHeight,
                                    imgIContainerObserver *aObserver)
{
  if (aWidth <= 0 || aHeight <= 0) {
    NS_WARNING("error - negative image size\n");
    return NS_ERROR_FAILURE;
  }

  mSize.SizeTo(aWidth, aHeight);

  mObserver = do_GetWeakReference(aObserver);

  return NS_OK;
}

//******************************************************************************
/* readonly attribute gfx_format preferredAlphaChannelFormat; */
NS_IMETHODIMP imgContainerGIF::GetPreferredAlphaChannelFormat(gfx_format *aFormat)
{
  *aFormat = gfxIFormats::RGB_A1;
  return NS_OK;
}

//******************************************************************************
/* readonly attribute PRInt32 width; */
NS_IMETHODIMP imgContainerGIF::GetWidth(PRInt32 *aWidth)
{
  *aWidth = mSize.width;
  return NS_OK;
}

//******************************************************************************
/* readonly attribute PRInt32 height; */
NS_IMETHODIMP imgContainerGIF::GetHeight(PRInt32 *aHeight)
{
  *aHeight = mSize.height;
  return NS_OK;
}

//******************************************************************************
/* readonly attribute gfxIImageFrame currentFrame; */
NS_IMETHODIMP imgContainerGIF::GetCurrentFrame(gfxIImageFrame * *aCurrentFrame)
{
  if (!(*aCurrentFrame = inlinedGetCurrentFrame()))
    return NS_ERROR_FAILURE;

  NS_ADDREF(*aCurrentFrame);
  return NS_OK;
}

//******************************************************************************
/* readonly attribute unsigned long numFrames; */
NS_IMETHODIMP imgContainerGIF::GetNumFrames(PRUint32 *aNumFrames)
{
  *aNumFrames = mFrames.Count();
  return NS_OK;
}

//******************************************************************************
/* gfxIImageFrame getFrameAt (in unsigned long index); */
NS_IMETHODIMP imgContainerGIF::GetFrameAt(PRUint32 index,
                                          gfxIImageFrame **_retval)
{
  NS_ENSURE_ARG(index < mFrames.Count());

  if (!(*_retval = mFrames[index]))
    return NS_ERROR_FAILURE;

  NS_ADDREF(*_retval);
  return NS_OK;
}

//******************************************************************************
/* void appendFrame (in gfxIImageFrame item); */
NS_IMETHODIMP imgContainerGIF::AppendFrame(gfxIImageFrame *item)
{
  NS_ASSERTION(item, "imgContainerGIF::AppendFrame: item is null");
  if (!item)
    return NS_ERROR_NULL_POINTER;

  PRInt32 numFrames = mFrames.Count();
  if (numFrames == 0) {
    // First Frame
    // If we dispose of the first frame by clearing it, then the
    // First Frame's refresh area is all of itself.
    // RESTORE_PREVIOUS is invalid (assumed to be DISPOSE_CLEAR)
    PRInt32 frameDisposalMethod;
    item->GetFrameDisposalMethod(&frameDisposalMethod);
    if (frameDisposalMethod == DISPOSE_CLEAR ||
        frameDisposalMethod == DISPOSE_RESTORE_PREVIOUS)
      item->GetRect(mFirstFrameRefreshArea);
  } else {
    // Calculate mFirstFrameRefreshArea
    // Some gifs are huge but only have a small area that they animate
    // We only need to refresh that small area when Frame 0 comes around again
    nsIntRect itemRect;
    item->GetRect(itemRect);
    mFirstFrameRefreshArea.UnionRect(mFirstFrameRefreshArea, itemRect);
  }

  mFrames.AppendObject(item);

  // If this is our second frame, start the animation.
  // Must be called after AppendElement because StartAnimation checks for > 1
  // frame
  if (numFrames == 1)
    StartAnimation();

  return NS_OK;
}

//******************************************************************************
/* void removeFrame (in gfxIImageFrame item); */
NS_IMETHODIMP imgContainerGIF::RemoveFrame(gfxIImageFrame *item)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

//******************************************************************************
/* void endFrameDecode (in gfxIImageFrame item, in unsigned long timeout); */
NS_IMETHODIMP imgContainerGIF::EndFrameDecode(PRUint32 aFrameNum,
                                              PRUint32 aTimeout)
{
  // Assume there's another frame.
  // aFrameNum is 1 based, mCurrentDecodingFrameIndex is 0 based.
  mCurrentDecodingFrameIndex = aFrameNum;
  return NS_OK;
}

//******************************************************************************
/* void decodingComplete (); */
NS_IMETHODIMP imgContainerGIF::DecodingComplete(void)
{
  mDoneDecoding = PR_TRUE;
  // If there's only 1 frame, optimize it.
  // Optimizing animated gifs is not supported
  if (mFrames.Count() == 1)
    mFrames[0]->SetMutable(PR_FALSE);
  return NS_OK;
}

/* void clear (); */
NS_IMETHODIMP imgContainerGIF::Clear()
{
  mFrames.Clear();
  return NS_OK;
}

//******************************************************************************
NS_IMETHODIMP imgContainerGIF::GetAnimationMode(PRUint16 *aAnimationMode)
{
  if (!aAnimationMode)
    return NS_ERROR_NULL_POINTER;
  *aAnimationMode = mAnimationMode;
  return NS_OK;
}

//******************************************************************************
NS_IMETHODIMP imgContainerGIF::SetAnimationMode(PRUint16 aAnimationMode)
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
      if (mLoopCount != 0 || mCurrentAnimationFrameIndex + 1 < mFrames.Count())
        StartAnimation();
      break;
    case kLoopOnceAnimMode:
      if (mCurrentAnimationFrameIndex + 1 < mFrames.Count())
        StartAnimation();
      break;
  }

  return NS_OK;
}

//******************************************************************************
/* void startAnimation () */
NS_IMETHODIMP imgContainerGIF::StartAnimation()
{
  if (mAnimationMode == kDontAnimMode || mAnimating || mTimer)
    return NS_OK;

  if (mFrames.Count() > 1) {
    PRInt32 timeout;
    gfxIImageFrame *currentFrame = inlinedGetCurrentFrame();
    if (currentFrame) {
      currentFrame->GetTimeout(&timeout);
      if (timeout <= 0) // -1 means display this frame forever
        return NS_OK;
    } else
      timeout = 100; // XXX hack.. the timer notify code will do the right
                     //     thing, so just get that started

    mTimer = do_CreateInstance("@mozilla.org/timer;1");
    if (!mTimer)
      return NS_ERROR_OUT_OF_MEMORY;

    // The only way mAnimating becomes true is if the mTimer is created
    mAnimating = PR_TRUE;
    mTimer->InitWithCallback(NS_STATIC_CAST(nsITimerCallback*, this),
                             timeout, nsITimer::TYPE_REPEATING_SLACK);
  }

  return NS_OK;
}

//******************************************************************************
/* void stopAnimation (); */
NS_IMETHODIMP imgContainerGIF::StopAnimation()
{
  mAnimating = PR_FALSE;

  if (!mTimer)
    return NS_OK;

  mTimer->Cancel();
  mTimer = nsnull;

  return NS_OK;
}

//******************************************************************************
/* void ResetAnimation (); */
NS_IMETHODIMP imgContainerGIF::ResetAnimation()
{
  if (mCurrentAnimationFrameIndex == 0 || mAnimationMode == kDontAnimMode)
    return NS_OK;

  PRBool oldAnimating = mAnimating;

  if (oldAnimating) {
    nsresult rv = StopAnimation();
    if (NS_FAILED(rv))
      return rv;
   }

  mLastCompositedFrameIndex = -1;
  mCurrentAnimationFrameIndex = 0;
  // Update display
  nsCOMPtr<imgIContainerObserver> observer(do_QueryReferent(mObserver));
  if (observer)
    observer->FrameChanged(this, mFrames[0], &mFirstFrameRefreshArea);

  if (oldAnimating)
    return StartAnimation();
  else
    return NS_OK;
}

//******************************************************************************
/* attribute long loopCount; */
NS_IMETHODIMP imgContainerGIF::GetLoopCount(PRInt32 *aLoopCount)
{
  NS_ASSERTION(aLoopCount, "ptr is null");
  *aLoopCount = mLoopCount;
  return NS_OK;
}

NS_IMETHODIMP imgContainerGIF::SetLoopCount(PRInt32 aLoopCount)
{
  // -1  infinite
  //  0  no looping, one iteration
  //  1  one loop, two iterations
  //  ...
  mLoopCount = aLoopCount;

  return NS_OK;
}



NS_IMETHODIMP imgContainerGIF::Notify(nsITimer *timer)
{
  NS_ASSERTION(mTimer == timer,
               "imgContainerGIF::Notify called with incorrect timer");

  if (!mAnimating || !mTimer)
    return NS_OK;

  nsCOMPtr<imgIContainerObserver> observer(do_QueryReferent(mObserver));
  if (!observer) {
    // the imgRequest that owns us is dead, we should die now too.
    StopAnimation();
    return NS_OK;
  }

  PRInt32 numFrames = mFrames.Count();
  if (!numFrames)
    return NS_OK;

  gfxIImageFrame *nextFrame = nsnull;
  PRInt32 previousFrameIndex = mCurrentAnimationFrameIndex;
  PRInt32 nextFrameIndex = mCurrentAnimationFrameIndex + 1;
  PRInt32 timeout = 0;

  // If we're done decoding the next frame, go ahead and display it now and
  // reinit the timer with the next frame's delay time.
  if (mDoneDecoding || (nextFrameIndex < mCurrentDecodingFrameIndex)) {
    if (numFrames == nextFrameIndex) {
      // End of Animation

      // If animation mode is "loop once", it's time to stop animating
      if (mAnimationMode == kLoopOnceAnimMode || mLoopCount == 0) {
        StopAnimation();
        return NS_OK;
      } else {
        // We may have used mCompositingFrame to build a frame, and then copied
        // it back into mFrames[..].  If so, delete composite to save memory
        if (mCompositingFrame && mLastCompositedFrameIndex == -1)
          mCompositingFrame = nsnull;
      }

      nextFrameIndex = 0;
      if (mLoopCount > 0)
        mLoopCount--;
    }

    if (!(nextFrame = mFrames[nextFrameIndex])) {
      // something wrong with the next frame, skip it
      mCurrentAnimationFrameIndex = nextFrameIndex;
      mTimer->SetDelay(100);
      return NS_OK;
    }
    nextFrame->GetTimeout(&timeout);

  } else if (nextFrameIndex == mCurrentDecodingFrameIndex) {
    // Uh oh, the frame we want to show is currently being decoded (partial)
    // Wait a bit and try again
    mTimer->SetDelay(100);
    return NS_OK;
  } else { //  (nextFrameIndex > mCurrentDecodingFrameIndex)
    // We shouldn't get here. However, if we are requesting a frame
    // that hasn't been decoded yet, go back to the last frame decoded
    NS_WARNING("imgContainerGIF::Notify()  Frame is passed decoded frame");
    nextFrameIndex = mCurrentDecodingFrameIndex;
    if (!(nextFrame = mFrames[nextFrameIndex])) {
      // something wrong with the next frame, skip it
      mCurrentAnimationFrameIndex = nextFrameIndex;
      mTimer->SetDelay(100);
      return NS_OK;
    }
    nextFrame->GetTimeout(&timeout);
  }

  if (timeout > 0)
    mTimer->SetDelay(timeout);
  else
    StopAnimation();

  nsIntRect dirtyRect;
  gfxIImageFrame *frameToUse = nsnull;

  if (nextFrameIndex == 0) {
    frameToUse = nextFrame;
    dirtyRect = mFirstFrameRefreshArea;
  } else {
    gfxIImageFrame *prevFrame = mFrames[previousFrameIndex];
    if (!prevFrame)
      return NS_OK;

    // Change frame and announce it
    if (NS_FAILED(DoComposite(&frameToUse, &dirtyRect, prevFrame,
                              nextFrame, nextFrameIndex))) {
      // something went wrong, move on to next
      NS_WARNING("imgContainerGIF: Composing Frame Failed\n");
      mCurrentAnimationFrameIndex = nextFrameIndex;
      return NS_OK;
    }
  }
  // Set mCurrentAnimationFrameIndex at the last possible moment
  mCurrentAnimationFrameIndex = nextFrameIndex;
  // Refreshes the screen
  observer->FrameChanged(this, frameToUse, &dirtyRect);
  return NS_OK;
}

//******************************************************************************
// DoComposite gets called when the timer for animation get fired and we have to
// update the composited frame of the animation.
nsresult imgContainerGIF::DoComposite(gfxIImageFrame** aFrameToUse,
                                      nsIntRect* aDirtyRect,
                                      gfxIImageFrame* aPrevFrame,
                                      gfxIImageFrame* aNextFrame,
                                      PRInt32 aNextFrameIndex)
{
  NS_ASSERTION(aDirtyRect, "imgContainerGIF::DoComposite aDirtyRect is null");
  NS_ASSERTION(aPrevFrame, "imgContainerGIF::DoComposite aPrevFrame is null");
  NS_ASSERTION(aNextFrame, "imgContainerGIF::DoComposite aNextFrame is null");
  NS_ASSERTION(aFrameToUse, "imgContainerGIF::DoComposite aFrameToUse is null");

  PRInt32 prevFrameDisposalMethod;
  aPrevFrame->GetFrameDisposalMethod(&prevFrameDisposalMethod);

  if (prevFrameDisposalMethod == DISPOSE_RESTORE_PREVIOUS &&
      !mCompositingPrevFrame)
    prevFrameDisposalMethod = DISPOSE_CLEAR;

  // Optimization: Skip compositing if the previous frame wants to clear the
  //               whole image
  if (prevFrameDisposalMethod == DISPOSE_CLEAR_ALL) {
    aDirtyRect->SetRect(0, 0, mSize.width, mSize.height);
    *aFrameToUse = aNextFrame;
    return NS_OK;
  }

  nsIntRect prevFrameRect;
  aPrevFrame->GetRect(prevFrameRect);
  PRBool isFullPrevFrame = (prevFrameRect.x == 0 && prevFrameRect.y == 0 &&
                            prevFrameRect.width == mSize.width &&
                            prevFrameRect.height == mSize.height);

  // Optimization: Skip compositing if the previous frame is the same size as
  //               container and it's clearing itself
  if (isFullPrevFrame && prevFrameDisposalMethod == DISPOSE_CLEAR) {
    aDirtyRect->SetRect(0, 0, mSize.width, mSize.height);
    *aFrameToUse = aNextFrame;
    return NS_OK;
  }

  PRInt32 nextFrameDisposalMethod;
  nsIntRect nextFrameRect;
  aNextFrame->GetFrameDisposalMethod(&nextFrameDisposalMethod);
  aNextFrame->GetRect(nextFrameRect);
  PRBool isFullNextFrame = (nextFrameRect.x == 0 && nextFrameRect.y == 0 &&
                            nextFrameRect.width == mSize.width &&
                            nextFrameRect.height == mSize.height);

  PRBool nextFrameHasAlpha;
  PRUint32 aBPR;
  nextFrameHasAlpha = NS_SUCCEEDED(aNextFrame->GetAlphaBytesPerRow(&aBPR));

  // Optimization: Skip compositing if this frame is the same size as the
  //               container and it's fully drawing over prev frame (no alpha)
  if (isFullNextFrame &&
      (nextFrameDisposalMethod != DISPOSE_RESTORE_PREVIOUS) &&
      !nextFrameHasAlpha) {

    aDirtyRect->SetRect(0, 0, mSize.width, mSize.height);
    *aFrameToUse = aNextFrame;
    return NS_OK;
  }

  // Calculate area that needs updating
  switch (prevFrameDisposalMethod) {
    default:
    case DISPOSE_NOT_SPECIFIED:
    case DISPOSE_KEEP:
      *aDirtyRect = nextFrameRect;
      break;

    case DISPOSE_CLEAR:
      // Calc area that needs to be redrawn (the combination of previous and
      // this frame)
      // XXX - This could be done with multiple framechanged calls
      //       Having prevFrame way at the top of the image, and nextFrame
      //       way at the bottom, and both frames being small, we'd be
      //       telling framechanged to refresh the whole image when only two
      //       small areas are needed.
      aDirtyRect->UnionRect(nextFrameRect, prevFrameRect);
      break;

    case DISPOSE_RESTORE_PREVIOUS:
      aDirtyRect->SetRect(0, 0, mSize.width, mSize.height);
      break;
  }

  // Optimization:
  //   Skip compositing if the last composited frame is this frame
  //   (Only one composited frame was made for this animation.  Example:
  //    Only Frame 3 of a 10 frame GIF required us to build a composite frame
  //    On the second loop of the GIF, we do not need to rebuild the frame
  //    since it's still sitting in mCompositingFrame)
  if (mLastCompositedFrameIndex == aNextFrameIndex) {
    *aFrameToUse = mCompositingFrame;
    return NS_OK;
  }

  PRBool needToBlankComposite = PR_FALSE;

  // Create the Compositing Frame
  if (!mCompositingFrame) {
    nsresult rv;
    mCompositingFrame = do_CreateInstance("@mozilla.org/gfx/image/frame;2", &rv);
    if (NS_FAILED(rv))
      return rv;
    rv = mCompositingFrame->Init(0, 0, mSize.width, mSize.height,
                                 gfxIFormats::RGB_A1, 24);
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to init mCompositingFrame!\n");
      mCompositingFrame = nsnull;
      return rv;
    }
    needToBlankComposite = PR_TRUE;
  }

  // Copy previous frame into mCompositingFrame before we put the new frame on top
  // Assumes that the previous frame represents a full frame (it could be
  // smaller in size than the container, as long as the frame before it erased
  // itself)
  // Note: Frame 1 never gets into DoComposite(), so (aNextFrameIndex - 1) will
  // always be a valid frame number.
  if (mLastCompositedFrameIndex != aNextFrameIndex - 1 &&
      prevFrameDisposalMethod != DISPOSE_RESTORE_PREVIOUS) {

    // XXX If we had a method of drawing a section of a frame into another, we
    //     could optimize further:
    //     if aPrevFrameIndex == 1 && mLastCompositedFrameIndex <> -1,
    //     only mFirstFrameRefreshArea needs to be drawn back to composite
    if (isFullPrevFrame) {
      CopyFrameImage(aPrevFrame, mCompositingFrame);
    } else {
      BlackenFrame(mCompositingFrame);
      SetMaskVisibility(mCompositingFrame, PR_FALSE);
      aPrevFrame->DrawTo(mCompositingFrame, prevFrameRect.x, prevFrameRect.y,
                         prevFrameRect.width, prevFrameRect.height);

      BuildCompositeMask(mCompositingFrame, aPrevFrame);
      needToBlankComposite = PR_FALSE;
    }
  }

  // Dispose of previous
  switch (prevFrameDisposalMethod) {
    case DISPOSE_CLEAR:
      if (needToBlankComposite) {
        // If we just created the composite, it could have anything in it's
        // buffers. Clear them
        BlackenFrame(mCompositingFrame);
        SetMaskVisibility(mCompositingFrame, PR_FALSE);
        needToBlankComposite = PR_FALSE;
      } else {
        // Blank out previous frame area (both color & Mask/Alpha)
        BlackenFrame(mCompositingFrame, prevFrameRect);
        SetMaskVisibility(mCompositingFrame, prevFrameRect, PR_FALSE);
      }
      break;

    case DISPOSE_RESTORE_PREVIOUS:
      // It would be better to copy only the area changed back to
      // mCompositingFrame.
      if (mCompositingPrevFrame) {
        CopyFrameImage(mCompositingPrevFrame, mCompositingFrame);

        // destroy only if we don't need it for this frame's disposal
        if (nextFrameDisposalMethod != DISPOSE_RESTORE_PREVIOUS)
          mCompositingPrevFrame = nsnull;
      } else {
        BlackenFrame(mCompositingFrame);
        SetMaskVisibility(mCompositingFrame, PR_FALSE);
      }
      break;
  }

  // Check if the frame we are composing wants the previous image restored afer
  // it is done. Don't store it (again) if last frame wanted it's image restored
  // too
  if ((nextFrameDisposalMethod == DISPOSE_RESTORE_PREVIOUS) &&
      (prevFrameDisposalMethod != DISPOSE_RESTORE_PREVIOUS)) {
    // We are storing the whole image.
    // It would be better if we just stored the area that nextFrame is going to
    // overwrite.
    if (!mCompositingPrevFrame) {
      nsresult rv;
      mCompositingPrevFrame = do_CreateInstance("@mozilla.org/gfx/image/frame;2",
                                                &rv);
      if (NS_FAILED(rv))
        return rv;
      rv = mCompositingPrevFrame->Init(0, 0, mSize.width, mSize.height,
                                       gfxIFormats::RGB_A1, 24);
      if (NS_FAILED(rv))
        return rv;
    }
    CopyFrameImage(mCompositingFrame, mCompositingPrevFrame);
  }

  // blit next frame into it's correct spot
  aNextFrame->DrawTo(mCompositingFrame, nextFrameRect.x, nextFrameRect.y,
                     nextFrameRect.width, nextFrameRect.height);
  // put the mask in
  BuildCompositeMask(mCompositingFrame, aNextFrame);
  // Set timeout of CompositeFrame to timeout of frame we just composed
  // Bug 177948
  PRInt32 timeout;
  aNextFrame->GetTimeout(&timeout);
  mCompositingFrame->SetTimeout(timeout);

  if (isFullNextFrame && mAnimationMode == kNormalAnimMode && mLoopCount != 0) {
    // We have a composited full frame
    // Store the composited frame into the mFrames[..] so we don't have to
    // continuously re-build it
    // Then set the previous frame's disposal to CLEAR_ALL so we just draw the
    // frame next time around
    if (CopyFrameImage(mCompositingFrame, aNextFrame)) {
      aPrevFrame->SetFrameDisposalMethod(DISPOSE_CLEAR_ALL);
      mLastCompositedFrameIndex = -1;
      *aFrameToUse = aNextFrame;
      return NS_OK;
    }
  }

  mLastCompositedFrameIndex = aNextFrameIndex;
  *aFrameToUse = mCompositingFrame;

  return NS_OK;
}

//******************************************************************************
void imgContainerGIF::BuildCompositeMask(gfxIImageFrame *aCompositingFrame,
                                         gfxIImageFrame *aOverlayFrame)
{
  if (!aCompositingFrame || !aOverlayFrame) return;

  nsresult res;
  PRUint8* compositingAlphaData;
  PRUint32 compositingAlphaDataLength;
  aCompositingFrame->LockAlphaData();
  res = aCompositingFrame->GetAlphaData(&compositingAlphaData,
                                        &compositingAlphaDataLength);
  if (!compositingAlphaData || !compositingAlphaDataLength || NS_FAILED(res)) {
    aCompositingFrame->UnlockAlphaData();
    return;
  }

  PRInt32 widthOverlay, heightOverlay;
  PRInt32 overlayXOffset, overlayYOffset;
  aOverlayFrame->GetWidth(&widthOverlay);
  aOverlayFrame->GetHeight(&heightOverlay);
  aOverlayFrame->GetX(&overlayXOffset);
  aOverlayFrame->GetY(&overlayYOffset);

  if (NS_FAILED(aOverlayFrame->LockAlphaData())) {
    // set the region of the overlay frame to visible in compositingFrame
    SetMaskVisibility(aCompositingFrame, overlayXOffset, overlayYOffset,
                      widthOverlay, heightOverlay, PR_TRUE);
    aCompositingFrame->UnlockAlphaData();
    return;
  }

  PRUint32 abprComposite;
  aCompositingFrame->GetAlphaBytesPerRow(&abprComposite);

  PRUint32 abprOverlay;
  aOverlayFrame->GetAlphaBytesPerRow(&abprOverlay);

  // Only the composite's width & height are needed.  x & y should always be 0.
  PRInt32 widthComposite, heightComposite;
  aCompositingFrame->GetWidth(&widthComposite);
  aCompositingFrame->GetHeight(&heightComposite);

  PRUint8* overlayAlphaData;
  PRUint32 overlayAlphaDataLength;
  res = aOverlayFrame->GetAlphaData(&overlayAlphaData, &overlayAlphaDataLength);

  gfx_format format;
  aCompositingFrame->GetFormat(&format);
  if (format != gfxIFormats::RGB_A1 && format != gfxIFormats::BGR_A1) {
    NS_NOTREACHED("GIFs only support 1 bit alpha");
    aCompositingFrame->UnlockAlphaData();
    aOverlayFrame->UnlockAlphaData();
    return;
  }

  // Exit if overlay is beyond the area of the composite
  if (widthComposite <= overlayXOffset || heightComposite <= overlayYOffset)
    return;

  const PRUint32 width  = PR_MIN(widthOverlay,
                                 widthComposite - overlayXOffset);
  const PRUint32 height = PR_MIN(heightOverlay,
                                 heightComposite - overlayYOffset);

#ifdef MOZ_PLATFORM_IMAGES_BOTTOM_TO_TOP
  // Account for bottom-up storage
  PRInt32 offset = ((heightComposite - 1) - overlayYOffset) * abprComposite;
#else
  PRInt32 offset = overlayYOffset * abprComposite;
#endif
  PRUint8* alphaLine = compositingAlphaData + offset + (overlayXOffset >> 3);

#ifdef MOZ_PLATFORM_IMAGES_BOTTOM_TO_TOP
  offset = (heightOverlay - 1) * abprOverlay;
#else
  offset = 0;
#endif
  PRUint8* overlayLine = overlayAlphaData + offset;

  /*
    This is the number of pixels of offset between alpha and overlay
    (the number of bits at the front of alpha to skip when starting a row).
    I.e:, for a mask_offset of 3:
    (these are representations of bits)
    overlay 'pixels':   76543210 hgfedcba
    alpha:              xxx76543 210hgfed ...
    where 'x' is data already in alpha
    the first 5 pixels of overlay are or'd into the low 5 bits of alpha
  */
  PRUint8 mask_offset = (overlayXOffset & 0x7);

  for(PRUint32 i = 0; i < height; i++) {
    PRUint8 pixels;
    PRUint32 j;
    // use locals to avoid keeping track of how much we need to add
    // at the end of a line.  we don't really need this since we may
    // be able to calculate the ending offsets, but it's simpler and
    // cheap.
    PRUint8 *localOverlay = overlayLine;
    PRUint8 *localAlpha   = alphaLine;

    for (j = width; j >= 8; j -= 8) {
      // don't do in for(...) to avoid reference past end of buffer
      pixels = *localOverlay++;

      if (pixels == 0) // no bits to set - iterate and bump output pointer
        localAlpha++;
      else {
        // for the last few bits of a line, we need to special-case it
        if (mask_offset == 0) // simple case, no offset
          *localAlpha++ |= pixels;
        else {
          *localAlpha++ |= (pixels >> mask_offset);
          *localAlpha   |= (pixels << (8U-mask_offset));
        }
      }
    }
    if (j != 0) {
      // handle the end of the line, 1 to 7 pixels
      pixels = *localOverlay++;
      if (pixels != 0) {
        // last few bits have to be handled more carefully if
        // width is not a multiple of 8.

        // set bits we don't want to change to 0
        pixels = (pixels >> (8U-j)) << (8U-j);
        *localAlpha++ |= (pixels >> mask_offset);
        // don't touch this byte unless we have bits for it
        if (j > (8U - mask_offset))
          *localAlpha |= (pixels << (8U-mask_offset));
      }
    }

#ifdef MOZ_PLATFORM_IMAGES_BOTTOM_TO_TOP
    alphaLine   -= abprComposite;
    overlayLine -= abprOverlay;
#else
    alphaLine   += abprComposite;
    overlayLine += abprOverlay;
#endif
  }

  aCompositingFrame->UnlockAlphaData();
  aOverlayFrame->UnlockAlphaData();
  return;
}

//******************************************************************************
void imgContainerGIF::SetMaskVisibility(gfxIImageFrame *aFrame,
                                        PRInt32 aX, PRInt32 aY,
                                        PRInt32 aWidth, PRInt32 aHeight,
                                        PRBool aVisible)
{
  if (!aFrame)
    return;

  PRInt32 frameWidth;
  PRInt32 frameHeight;
  aFrame->GetWidth(&frameWidth);
  aFrame->GetHeight(&frameHeight);

  const PRInt32 width  = PR_MIN(aWidth, frameWidth - aX);
  const PRInt32 height = PR_MIN(aHeight, frameHeight - aY);

  if (width <= 0 || height <= 0) {
    return;
  }

  PRUint8* alphaData;
  PRUint32 alphaDataLength;
  const PRUint8 setMaskTo = aVisible ? 0xFF : 0x00;

  aFrame->LockImageData();
  nsresult res = aFrame->GetImageData(&alphaData, &alphaDataLength);
  if (NS_SUCCEEDED(res)) {
#ifdef IS_LITTLE_ENDIAN
    alphaData += aY*frameWidth*4 + 3;
#else
    alphaData += aY*frameWidth*4;
#endif
    for (PRInt32 j = height; j > 0; --j) {
      for (PRInt32 i = (aX+width-1)*4; i >= aX; i -= 4) {
        alphaData[i] = setMaskTo;
      }
      alphaData += frameWidth*4;
    }
  }
  aFrame->UnlockImageData();
}

//******************************************************************************
void imgContainerGIF::SetMaskVisibility(gfxIImageFrame *aFrame, PRBool aVisible)
{
  if (!aFrame)
    return;

  PRUint8* alphaData;
  PRUint32 alphaDataLength;
  const PRUint8 setMaskTo = aVisible ? 0xFF : 0x00;

  aFrame->LockImageData();
  nsresult res = aFrame->GetImageData(&alphaData, &alphaDataLength);
  if (NS_SUCCEEDED(res)) {
    for (PRUint32 i = 0; i < alphaDataLength; i+=4) {
#ifdef IS_LITTLE_ENDIAN
      alphaData[i+3] = setMaskTo;
#else
      alphaData[i] = setMaskTo;
#endif
    }
  }
  aFrame->UnlockImageData();
}

//******************************************************************************
// Fill aFrame with black. Does not change the mask.
void imgContainerGIF::BlackenFrame(gfxIImageFrame *aFrame)
{
  if (!aFrame)
    return;

  PRInt32 widthFrame;
  PRInt32 heightFrame;
  aFrame->GetWidth(&widthFrame);
  aFrame->GetHeight(&heightFrame);

  BlackenFrame(aFrame, 0, 0, widthFrame, heightFrame);
}

//******************************************************************************
void imgContainerGIF::BlackenFrame(gfxIImageFrame *aFrame,
                                   PRInt32 aX, PRInt32 aY,
                                   PRInt32 aWidth, PRInt32 aHeight)
{
  if (!aFrame)
    return;

  nsCOMPtr<nsIImage> img(do_GetInterface(aFrame));
  if (!img)
    return;

  nsRefPtr<gfxASurface> surf;
  img->GetSurface(getter_AddRefs(surf));

  nsRefPtr<gfxContext> ctx = new gfxContext(surf);
  ctx->SetColor(gfxRGBA(0, 0, 0));
  ctx->Rectangle(gfxRect(aX, aY, aWidth, aHeight));
  ctx->Fill();

  nsIntRect r(aX, aY, aWidth, aHeight);
  img->ImageUpdated(nsnull, nsImageUpdateFlags_kBitsChanged, &r);
}


//******************************************************************************
// Whether we succeed or fail will not cause a crash, and there's not much
// we can do about a failure, so there we don't return a nsresult
PRBool imgContainerGIF::CopyFrameImage(gfxIImageFrame *aSrcFrame,
                                       gfxIImageFrame *aDstFrame)
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

  // Tell the image that it's data has been updated
  nsCOMPtr<nsIInterfaceRequestor> ireq(do_QueryInterface(aDstFrame));
  if (!ireq)
    return PR_FALSE;
  nsCOMPtr<nsIImage> img(do_GetInterface(ireq));
  if (!img)
    return PR_FALSE;
  nsIntRect r;
  aDstFrame->GetRect(r);
  img->ImageUpdated(nsnull, nsImageUpdateFlags_kBitsChanged, &r);

  return PR_TRUE;
}
