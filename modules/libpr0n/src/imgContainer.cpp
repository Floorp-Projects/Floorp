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
#include "nsIImage.h"
#include "imgContainer.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsAutoPtr.h"

#include "gfxContext.h"

NS_IMPL_ISUPPORTS3(imgContainer, imgIContainer, nsITimerCallback, nsIProperties)

//******************************************************************************
imgContainer::imgContainer() :
  mSize(0,0),
  mAnim(nsnull),
  mAnimationMode(kNormalAnimMode),
  mLoopCount(-1),
  mObserver(nsnull)
{
  mProperties = do_CreateInstance("@mozilla.org/properties;1");
}

//******************************************************************************
imgContainer::~imgContainer()
{
  if (mAnim)
    delete mAnim;
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
  
  mObserver = do_GetWeakReference(aObserver);
  
  return NS_OK;
}

//******************************************************************************
/* readonly attribute gfx_format preferredAlphaChannelFormat; */
NS_IMETHODIMP imgContainer::GetPreferredAlphaChannelFormat(gfx_format *aFormat)
{
  NS_ASSERTION(aFormat, "imgContainer::GetPreferredAlphaChannelFormat; Invalid Arg");
  if (!aFormat)
    return NS_ERROR_INVALID_ARG;

  /* default.. platforms should probably overwrite this */
  *aFormat = gfxIFormats::RGB_A8;
  return NS_OK;
}

//******************************************************************************
/* readonly attribute PRInt32 width; */
NS_IMETHODIMP imgContainer::GetWidth(PRInt32 *aWidth)
{
  NS_ASSERTION(aWidth, "imgContainer::GetWidth; Invalid Arg");
  if (!aWidth)
    return NS_ERROR_INVALID_ARG;

  *aWidth = mSize.width;
  return NS_OK;
}

//******************************************************************************
/* readonly attribute PRInt32 height; */
NS_IMETHODIMP imgContainer::GetHeight(PRInt32 *aHeight)
{
  NS_ASSERTION(aHeight, "imgContainer::GetHeight; Invalid Arg");
  if (!aHeight)
    return NS_ERROR_INVALID_ARG;

  *aHeight = mSize.height;
  return NS_OK;
}

//******************************************************************************
/* readonly attribute gfxIImageFrame currentFrame; */
NS_IMETHODIMP imgContainer::GetCurrentFrame(gfxIImageFrame **aCurrentFrame)
{
  NS_ASSERTION(aCurrentFrame, "imgContainer::GetCurrentFrame; Invalid Arg");
  if (!aCurrentFrame)
    return NS_ERROR_INVALID_POINTER;
  
  if (!(*aCurrentFrame = inlinedGetCurrentFrame()))
    return NS_ERROR_FAILURE;

  NS_ADDREF(*aCurrentFrame);
  
  return NS_OK;
}

//******************************************************************************
/* readonly attribute unsigned long numFrames; */
NS_IMETHODIMP imgContainer::GetNumFrames(PRUint32 *aNumFrames)
{
  NS_ASSERTION(aNumFrames, "imgContainer::GetNumFrames; Invalid Arg");
  if (!aNumFrames)
    return NS_ERROR_INVALID_ARG;

  *aNumFrames = mFrames.Count();
  
  return NS_OK;
}

//******************************************************************************
/* gfxIImageFrame getFrameAt (in unsigned long index); */
NS_IMETHODIMP imgContainer::GetFrameAt(PRUint32 index, gfxIImageFrame **_retval)
{
  NS_ENSURE_ARG(index < NS_STATIC_CAST(PRUint32, mFrames.Count()));
  
  NS_ASSERTION(_retval, "imgContainer::GetFrameAt; Invalid Arg");
  if (!_retval)
    return NS_ERROR_INVALID_POINTER;

  if (!(*_retval = mFrames[index]))
    return NS_ERROR_FAILURE;

  NS_ADDREF(*_retval);
  
  return NS_OK;
}

//******************************************************************************
/* void appendFrame (in gfxIImageFrame item); */
NS_IMETHODIMP imgContainer::AppendFrame(gfxIImageFrame *item)
{
  NS_ASSERTION(item, "imgContainer::AppendFrame; Invalid Arg");
  if (!item)
    return NS_ERROR_INVALID_ARG;
  
  PRInt32 numFrames = mFrames.Count();
  
  if (numFrames == 0) {
    // This may not be an animated image, don't do all the animation stuff.
    mFrames.AppendObject(item);
    return NS_OK;
  }
  
  if (numFrames == 1) {
    // Now that we got a second frame, initialize animation stuff.
    if (!ensureAnimExists())
      return NS_ERROR_OUT_OF_MEMORY;
    
    // If we dispose of the first frame by clearing it, then the
    // First Frame's refresh area is all of itself.
    // RESTORE_PREVIOUS is invalid (assumed to be DISPOSE_CLEAR)
    PRInt32 frameDisposalMethod;
    mFrames[0]->GetFrameDisposalMethod(&frameDisposalMethod);
    if (frameDisposalMethod == imgIContainer::kDisposeClear ||
        frameDisposalMethod == imgIContainer::kDisposeRestorePrevious)
      mFrames[0]->GetRect(mAnim->firstFrameRefreshArea);
  }
  
  // Calculate firstFrameRefreshArea
  // Some gifs are huge but only have a small area that they animate
  // We only need to refresh that small area when Frame 0 comes around again
  nsIntRect itemRect;
  item->GetRect(itemRect);
  mAnim->firstFrameRefreshArea.UnionRect(mAnim->firstFrameRefreshArea, 
                                         itemRect);
  
  mFrames.AppendObject(item);
  
  // If this is our second frame, start the animation.
  // Must be called after AppendObject because StartAnimation checks for > 1
  // frame
  if (numFrames == 1)
    StartAnimation();
  
  return NS_OK;
}

//******************************************************************************
/* void removeFrame (in gfxIImageFrame item); */
NS_IMETHODIMP imgContainer::RemoveFrame(gfxIImageFrame *item)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

//******************************************************************************
/* void endFrameDecode (in unsigned long framenumber, in unsigned long timeout); */
NS_IMETHODIMP imgContainer::EndFrameDecode(PRUint32 aFrameNum, PRUint32 aTimeout)
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
  // Optimizing animated images is not supported
  if (mFrames.Count() == 1)
    mFrames[0]->SetMutable(PR_FALSE);
  return NS_OK;
}

//******************************************************************************
/* void clear (); */
NS_IMETHODIMP imgContainer::Clear()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

//******************************************************************************
/* attribute unsigned short animationMode; */
NS_IMETHODIMP imgContainer::GetAnimationMode(PRUint16 *aAnimationMode)
{
  NS_ASSERTION(aAnimationMode, "imgContainer::GetAnimationMode; Invalid Arg");
  if (!aAnimationMode)
    return NS_ERROR_INVALID_ARG;
  
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
          (mAnim && (mAnim->currentAnimationFrameIndex + 1 < mFrames.Count())))
        StartAnimation();
      break;
    case kLoopOnceAnimMode:
      if (mAnim && (mAnim->currentAnimationFrameIndex + 1 < mFrames.Count()))
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
  
  if (mFrames.Count() > 1) {
    if (!ensureAnimExists())
      return NS_ERROR_OUT_OF_MEMORY;
    
    PRInt32 timeout;
    gfxIImageFrame *currentFrame = inlinedGetCurrentFrame();
    if (currentFrame) {
      currentFrame->GetTimeout(&timeout);
      if (timeout <= 0) // -1 means display this frame forever
        return NS_OK;
    } else
      timeout = 100; // XXX hack.. the timer notify code will do the right
                     //     thing, so just get that started
    
    mAnim->timer = do_CreateInstance("@mozilla.org/timer;1");
    if (!mAnim->timer)
      return NS_ERROR_OUT_OF_MEMORY;
    
    // The only way animating becomes true is if the timer is created
    mAnim->animating = PR_TRUE;
    mAnim->timer->InitWithCallback(NS_STATIC_CAST(nsITimerCallback*, this),
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
      !mAnim || mAnim->currentAnimationFrameIndex)
    return NS_OK;

  PRBool oldAnimating = mAnim->animating;

  if (mAnim->animating) {
    nsresult rv = StopAnimation();
    if (NS_FAILED(rv))
      return rv;
  }

  mAnim->lastCompositedFrameIndex = -1;
  mAnim->currentAnimationFrameIndex = 0;
  // Update display
  nsCOMPtr<imgIContainerObserver> observer(do_QueryReferent(mObserver));
  if (observer)
    observer->FrameChanged(this, mFrames[0], &(mAnim->firstFrameRefreshArea));

  if (oldAnimating)
    return StartAnimation();
  else
    return NS_OK;
}

//******************************************************************************
/* attribute long loopCount; */
NS_IMETHODIMP imgContainer::GetLoopCount(PRInt32 *aLoopCount)
{
  NS_ASSERTION(aLoopCount, "imgContainer::GetLoopCount() called with null ptr");
  if (!aLoopCount)
    return NS_ERROR_INVALID_ARG;
  
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

//******************************************************************************
/* void notify(in nsITimer timer); */
NS_IMETHODIMP imgContainer::Notify(nsITimer *timer)
{
  // This should never happen since the timer is only set up in StartAnimation()
  // after mAnim is checked to exist.
  NS_ASSERTION(mAnim, "imgContainer::Notify() called but mAnim is null");
  if (!mAnim)
    return NS_ERROR_UNEXPECTED;
  NS_ASSERTION(mAnim->timer == timer,
               "imgContainer::Notify() called with incorrect timer");

  if (!(mAnim->animating) || !(mAnim->timer))
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
  PRInt32 previousFrameIndex = mAnim->currentAnimationFrameIndex;
  PRInt32 nextFrameIndex = mAnim->currentAnimationFrameIndex + 1;
  PRInt32 timeout = 0;

  // If we're done decoding the next frame, go ahead and display it now and
  // reinit the timer with the next frame's delay time.
  // currentDecodingFrameIndex is not set until the second frame has
  // finished decoding (see EndFrameDecode)
  if (mAnim->doneDecoding || 
      (numFrames == 2 && nextFrameIndex < 2) ||
      (numFrames > 2 && nextFrameIndex < mAnim->currentDecodingFrameIndex)) {
    if (numFrames == nextFrameIndex) {
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
    nextFrame->GetTimeout(&timeout);

  } else if ((numFrames == 2 && nextFrameIndex == 2) ||
             (numFrames > 2 && nextFrameIndex == mAnim->currentDecodingFrameIndex)) {
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
    nextFrame->GetTimeout(&timeout);
  }

  if (timeout > 0)
    mAnim->timer->SetDelay(timeout);
  else
    StopAnimation();

  nsIntRect dirtyRect;
  gfxIImageFrame *frameToUse = nsnull;

  if (nextFrameIndex == 0) {
    frameToUse = nextFrame;
    dirtyRect = mAnim->firstFrameRefreshArea;
  } else {
    gfxIImageFrame *prevFrame = mFrames[previousFrameIndex];
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
  observer->FrameChanged(this, frameToUse, &dirtyRect);
  
  return NS_OK;
}

//******************************************************************************
// DoComposite gets called when the timer for animation get fired and we have to
// update the composited frame of the animation.
nsresult imgContainer::DoComposite(gfxIImageFrame** aFrameToUse,
                                   nsIntRect* aDirtyRect,
                                   gfxIImageFrame* aPrevFrame,
                                   gfxIImageFrame* aNextFrame,
                                   PRInt32 aNextFrameIndex)
{
  NS_ASSERTION(aDirtyRect, "imgContainer::DoComposite aDirtyRect is null");
  NS_ASSERTION(aPrevFrame, "imgContainer::DoComposite aPrevFrame is null");
  NS_ASSERTION(aNextFrame, "imgContainer::DoComposite aNextFrame is null");
  NS_ASSERTION(aFrameToUse, "imgContainer::DoComposite aFrameToUse is null");
  
  PRInt32 prevFrameDisposalMethod;
  aPrevFrame->GetFrameDisposalMethod(&prevFrameDisposalMethod);

  if (prevFrameDisposalMethod == imgIContainer::kDisposeRestorePrevious &&
      !mAnim->compositingPrevFrame)
    prevFrameDisposalMethod = imgIContainer::kDisposeClear;

  // Optimization: Skip compositing if the previous frame wants to clear the
  //               whole image
  if (prevFrameDisposalMethod == imgIContainer::kDisposeClearAll) {
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
  if (isFullPrevFrame && prevFrameDisposalMethod == imgIContainer::kDisposeClear) {
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
      (nextFrameDisposalMethod != imgIContainer::kDisposeRestorePrevious) &&
      !nextFrameHasAlpha) {

    aDirtyRect->SetRect(0, 0, mSize.width, mSize.height);
    *aFrameToUse = aNextFrame;
    return NS_OK;
  }

  // Calculate area that needs updating
  switch (prevFrameDisposalMethod) {
    default:
    case imgIContainer::kDisposeNotSpecified:
    case imgIContainer::kDisposeKeep:
      *aDirtyRect = nextFrameRect;
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
    nsresult rv;
    mAnim->compositingFrame = do_CreateInstance("@mozilla.org/gfx/image/frame;2", &rv);
    if (NS_FAILED(rv))
      return rv;
    rv = mAnim->compositingFrame->Init(0, 0, mSize.width, mSize.height,
                                       gfxIFormats::RGB_A1, 24);
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to init compositingFrame!\n");
      mAnim->compositingFrame = nsnull;
      return rv;
    }
    needToBlankComposite = PR_TRUE;
  }

  // Copy previous frame into compositingFrame before we put the new frame on top
  // Assumes that the previous frame represents a full frame (it could be
  // smaller in size than the container, as long as the frame before it erased
  // itself)
  // Note: Frame 1 never gets into DoComposite(), so (aNextFrameIndex - 1) will
  // always be a valid frame number.
  if (mAnim->lastCompositedFrameIndex != aNextFrameIndex - 1 &&
      prevFrameDisposalMethod != imgIContainer::kDisposeRestorePrevious) {

    // XXX If we had a method of drawing a section of a frame into another, we
    //     could optimize further:
    //     if aPrevFrameIndex == 1 && lastCompositedFrameIndex <> -1,
    //     only firstFrameRefreshArea needs to be drawn back to composite
    if (isFullPrevFrame) {
      CopyFrameImage(aPrevFrame, mAnim->compositingFrame);
    } else {
      ClearFrame(mAnim->compositingFrame);
      aPrevFrame->DrawTo(mAnim->compositingFrame, prevFrameRect.x, prevFrameRect.y,
                         prevFrameRect.width, prevFrameRect.height);
      needToBlankComposite = PR_FALSE;
    }
  }

  // Dispose of previous
  switch (prevFrameDisposalMethod) {
    case imgIContainer::kDisposeClear:
      if (needToBlankComposite) {
        // If we just created the composite, it could have anything in it's
        // buffers. Clear them
        ClearFrame(mAnim->compositingFrame);
        needToBlankComposite = PR_FALSE;
      } else {
        // Blank out previous frame area (both color & Mask/Alpha)
        ClearFrame(mAnim->compositingFrame, prevFrameRect);
      }
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
  }

  // Check if the frame we are composing wants the previous image restored afer
  // it is done. Don't store it (again) if last frame wanted it's image restored
  // too
  if ((nextFrameDisposalMethod == imgIContainer::kDisposeRestorePrevious) &&
      (prevFrameDisposalMethod != imgIContainer::kDisposeRestorePrevious)) {
    // We are storing the whole image.
    // It would be better if we just stored the area that nextFrame is going to
    // overwrite.
    if (!mAnim->compositingPrevFrame) {
      nsresult rv;
      mAnim->compositingPrevFrame = do_CreateInstance("@mozilla.org/gfx/image/frame;2",
                                                       &rv);
      if (NS_FAILED(rv))
        return rv;
      rv = mAnim->compositingPrevFrame->Init(0, 0, mSize.width, mSize.height,
                                              gfxIFormats::RGB_A1, 24);
      if (NS_FAILED(rv))
        return rv;
    }
    CopyFrameImage(mAnim->compositingFrame, mAnim->compositingPrevFrame);
  }

  // blit next frame into it's correct spot
  aNextFrame->DrawTo(mAnim->compositingFrame, nextFrameRect.x, nextFrameRect.y,
                     nextFrameRect.width, nextFrameRect.height);
  // Set timeout of CompositeFrame to timeout of frame we just composed
  // Bug 177948
  PRInt32 timeout;
  aNextFrame->GetTimeout(&timeout);
  mAnim->compositingFrame->SetTimeout(timeout);

  if (isFullNextFrame && mAnimationMode == kNormalAnimMode && mLoopCount != 0) {
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
void imgContainer::ClearFrame(gfxIImageFrame *aFrame)
{
  if (!aFrame)
    return;

  nsCOMPtr<nsIImage> img(do_GetInterface(aFrame));
  nsRefPtr<gfxASurface> surf;
  img->GetSurface(getter_AddRefs(surf));

  // Erase the surface to transparent
  nsRefPtr<gfxContext> ctx = new gfxContext(surf);
  ctx->SetOperator(gfxContext::OPERATOR_CLEAR);
  ctx->Paint();

  nsIntRect r;
  aFrame->GetRect(r);
  img->ImageUpdated(nsnull, nsImageUpdateFlags_kBitsChanged, &r);
}

//******************************************************************************
void imgContainer::ClearFrame(gfxIImageFrame *aFrame, nsIntRect &aRect)
{
  if (!aFrame || aRect.width <= 0 || aRect.height <= 0) {
    return;
  }

  nsCOMPtr<nsIImage> img(do_GetInterface(aFrame));
  nsRefPtr<gfxASurface> surf;
  img->GetSurface(getter_AddRefs(surf));

  // Erase the destination rectangle to transparent
  nsRefPtr<gfxContext> ctx = new gfxContext(surf);
  ctx->SetOperator(gfxContext::OPERATOR_CLEAR);
  ctx->Rectangle(gfxRect(aRect.x, aRect.y, aRect.width, aRect.height));
  ctx->Fill();

  img->ImageUpdated(nsnull, nsImageUpdateFlags_kBitsChanged, &aRect);
}


//******************************************************************************
// Whether we succeed or fail will not cause a crash, and there's not much
// we can do about a failure, so there we don't return a nsresult
PRBool imgContainer::CopyFrameImage(gfxIImageFrame *aSrcFrame,
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
