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
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@netscape.com>
 *   Chris Saari <saari@netscape.com>
 *   Asko Tontti <atontti@cc.hut.fi>
 *   Arron Mogge <paper@animecity.nu>
 */

#include "imgContainerGIF.h"

#include "nsIServiceManager.h"
#include "nsIImage.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "gfxIImageFrame.h"
#include "nsMemory.h"
#include "nsITimer.h"

NS_IMPL_ISUPPORTS3(imgContainerGIF, imgIContainer, nsITimerCallback, imgIDecoderObserver)

//******************************************************************************
imgContainerGIF::imgContainerGIF() :
  mSize(0,0),
  mObserver(nsnull),
  mCurrentDecodingFrameIndex(0),
  mCurrentAnimationFrameIndex(0),
  mCurrentFrameIsFinishedDecoding(PR_FALSE),
  mDoneDecoding(PR_FALSE),
  mAnimating(PR_FALSE),
  mAnimationMode(kNormalAnimMode),
  mLoopCount(-1)
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
}

//******************************************************************************
imgContainerGIF::~imgContainerGIF()
{
  if (mTimer)
    mTimer->Cancel();

  /* destructor code */
  mFrames.Clear();
}

//******************************************************************************
/* void init (in nscoord aWidth, in nscoord aHeight, in imgIContainerObserver aObserver); */
NS_IMETHODIMP imgContainerGIF::Init(nscoord aWidth, nscoord aHeight, imgIContainerObserver *aObserver)
{
  if (aWidth <= 0 || aHeight <= 0) {
    NS_WARNING("error - negative image size\n");
    return NS_ERROR_FAILURE;
  }

  mSize.SizeTo(aWidth, aHeight);

  mObserver = getter_AddRefs(NS_GetWeakReference(aObserver));

  return NS_OK;
}

//******************************************************************************
/* readonly attribute gfx_format preferredAlphaChannelFormat; */
NS_IMETHODIMP imgContainerGIF::GetPreferredAlphaChannelFormat(gfx_format *aFormat)
{
  /* default.. platform's should probably overwrite this */
  *aFormat = gfxIFormats::RGB_A8;
  return NS_OK;
}

//******************************************************************************
/* readonly attribute nscoord width; */
NS_IMETHODIMP imgContainerGIF::GetWidth(nscoord *aWidth)
{
  *aWidth = mSize.width;
  return NS_OK;
}

//******************************************************************************
/* readonly attribute nscoord height; */
NS_IMETHODIMP imgContainerGIF::GetHeight(nscoord *aHeight)
{
  *aHeight = mSize.height;
  return NS_OK;
}

//******************************************************************************
/* readonly attribute gfxIImageFrame currentFrame; */
NS_IMETHODIMP imgContainerGIF::GetCurrentFrame(gfxIImageFrame * *aCurrentFrame)
{
  return inlinedGetCurrentFrame(aCurrentFrame);
}

//******************************************************************************
/* readonly attribute unsigned long numFrames; */
NS_IMETHODIMP imgContainerGIF::GetNumFrames(PRUint32 *aNumFrames)
{
  return mFrames.Count(aNumFrames);
}

//******************************************************************************
/* gfxIImageFrame getFrameAt (in unsigned long index); */
NS_IMETHODIMP imgContainerGIF::GetFrameAt(PRUint32 index, gfxIImageFrame **_retval)
{
  return inlinedGetFrameAt(index, _retval);
}

//******************************************************************************
/* void appendFrame (in gfxIImageFrame item); */
NS_IMETHODIMP imgContainerGIF::AppendFrame(gfxIImageFrame *item)
{
  // If we don't have a composite frame already allocated, make sure that our container
  // size is the same the frame size. Otherwise, we'll either need the composite frame
  // for animation compositing (GIF) or for filling in with a background color.
  // XXX IMPORTANT: this means that the frame should be initialized BEFORE appending to container
  PRUint32 numFrames = inlinedGetNumFrames();

  if (!mCompositingFrame) {
    nsRect frameRect;
    item->GetRect(frameRect);
    // We used to create a compositing frame if any frame was smaller than the logical
    // image size. You could create a single frame that was 10x10 in the middle of
    // an 20x20 logical screen and have the extra screen space filled by the image
    // background color. However, it turns out that neither NS4.x nor IE correctly
    // support this, and as a result there are many GIFs out there that look "wrong"
    // when this is correctly supported. So for now, we only create a compositing frame
    // if we have more than one frame in the image.
    if(/*(frameRect.x != 0) ||
       (frameRect.y != 0) ||
       (frameRect.width != mSize.width) ||
       (frameRect.height != mSize.height) ||*/
       (numFrames >= 1)) // Not sure if I want to create a composite frame for every anim. Could be smarter.
    {
      mCompositingFrame = do_CreateInstance("@mozilla.org/gfx/image/frame;2");
      mCompositingFrame->Init(0, 0, mSize.width, mSize.height, gfxIFormats::RGB_A1, 24);

      nsCOMPtr<nsIInterfaceRequestor> ireq(do_QueryInterface(mCompositingFrame));
      if (ireq) {
        nsCOMPtr<nsIImage> img(do_GetInterface(ireq));
        img->SetDecodedRect(0, 0, mSize.width, mSize.height);
      }

      nsCOMPtr<gfxIImageFrame> firstFrame;
      inlinedGetFrameAt(0, getter_AddRefs(firstFrame));

      gfx_color backgroundColor, transColor;
      if (NS_SUCCEEDED(firstFrame->GetTransparentColor(&transColor))) {
        mCompositingFrame->SetTransparentColor(transColor);
      }

      if (NS_SUCCEEDED(firstFrame->GetBackgroundColor(&backgroundColor))) {
        mCompositingFrame->SetBackgroundColor(backgroundColor);
      }

      PRInt32 timeout;
      // Set timeout because StartAnimation reads it
      if (NS_SUCCEEDED(firstFrame->GetTimeout(&timeout)))
        mCompositingFrame->SetTimeout(timeout);

      PRInt32 x;
      PRInt32 y;
      PRInt32 width;
      PRInt32 height;
      firstFrame->GetX(&x);
      firstFrame->GetY(&y);
      firstFrame->GetWidth(&width);
      firstFrame->GetHeight(&height);

      BlackenFrame(mCompositingFrame);
      firstFrame->DrawTo(mCompositingFrame, x, y, width, height);
      ZeroMask(mCompositingFrame);
      BuildCompositeMask(mCompositingFrame, firstFrame);
    }
  }

  mFrames.AppendElement(NS_STATIC_CAST(nsISupports*, item));
  if (numFrames > 0) {
    // If this is our second frame, init a timer so we don't display
    // the next frame until the delay timer has expired for the current
    // frame.
    if (!mAnimating)
      StartAnimation();
    mCurrentDecodingFrameIndex++;
  }

  mCurrentFrameIsFinishedDecoding = PR_FALSE;

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
NS_IMETHODIMP imgContainerGIF::EndFrameDecode(PRUint32 aFrameNum, PRUint32 aTimeout)
{
  // It is now okay to start the timer for the next frame in the animation
  mCurrentFrameIsFinishedDecoding = PR_TRUE;

  nsCOMPtr<gfxIImageFrame> currentFrame;
  inlinedGetFrameAt(aFrameNum-1, getter_AddRefs(currentFrame));
  NS_ASSERTION(currentFrame, "Received an EndFrameDecode call with an invalid frame number");
  if (!currentFrame) return NS_ERROR_UNEXPECTED;

  currentFrame->SetTimeout(aTimeout);

  return NS_OK;
}

//******************************************************************************
/* void decodingComplete (); */
NS_IMETHODIMP imgContainerGIF::DecodingComplete(void)
{
  mDoneDecoding = PR_TRUE;

  PRUint32 numFrames;
  mFrames.Count(&numFrames);
  if (numFrames == 1) {
    nsCOMPtr<gfxIImageFrame> currentFrame;
    inlinedGetFrameAt(0, getter_AddRefs(currentFrame));
    currentFrame->SetMutable(PR_FALSE);
  }
  return NS_OK;
}

//******************************************************************************
/* nsIEnumerator enumerate (); */
NS_IMETHODIMP imgContainerGIF::Enumerate(nsIEnumerator **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void clear (); */
NS_IMETHODIMP imgContainerGIF::Clear()
{
  return mFrames.Clear();
}

//******************************************************************************
NS_IMETHODIMP imgContainerGIF::GetAnimationMode(PRUint16 *aAnimationMode)
{
  if (!aAnimationMode) return NS_ERROR_NULL_POINTER;
  *aAnimationMode = mAnimationMode;
  return NS_OK;
}

NS_IMETHODIMP imgContainerGIF::SetAnimationMode(PRUint16 aAnimationMode)
{
  NS_ASSERTION(aAnimationMode == imgIContainer::kNormalAnimMode ||
               aAnimationMode == imgIContainer::kDontAnimMode ||
               aAnimationMode == imgIContainer::kLoopOnceAnimMode, "Wrong Animation Mode is being set!");

  if (mAnimationMode == kNormalAnimMode &&
      (aAnimationMode == kDontAnimMode || aAnimationMode == kLoopOnceAnimMode)) {
    StopAnimation();
  } else if (aAnimationMode == kNormalAnimMode &&
             (mAnimationMode == kDontAnimMode || mAnimationMode == kLoopOnceAnimMode)) {
    mAnimationMode = aAnimationMode;
    StartAnimation();
    return NS_OK;
  }
  mAnimationMode = aAnimationMode;

  return NS_OK;
}

//******************************************************************************
/* void startAnimation () */
NS_IMETHODIMP imgContainerGIF::StartAnimation()
{
  if (mAnimationMode == kDontAnimMode)
    return NS_OK;

  if (mAnimating || mTimer)
    return NS_OK;

  PRUint32 numFrames = inlinedGetNumFrames();
  if (numFrames > 1) {
    PRInt32 timeout;
    nsCOMPtr<gfxIImageFrame> currentFrame;

    inlinedGetCurrentFrame(getter_AddRefs(currentFrame));
    if (currentFrame)
      currentFrame->GetTimeout(&timeout);
    else
      timeout = 100; // XXX hack.. the timer notify code will do the right thing,
                     // so just get that started

    if (timeout > 0) { // -1 means display this frame forever
      mTimer = do_CreateInstance("@mozilla.org/timer;1");
      if (!mTimer)
        return NS_ERROR_OUT_OF_MEMORY;

      // The only way mAnimating becomes true is if the mTimer is created
      mAnimating = PR_TRUE;
      mTimer->InitWithCallback(NS_STATIC_CAST(nsITimerCallback*, this),
                               timeout, nsITimer::TYPE_REPEATING_SLACK);
    }
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

  // don't bother trying to change the frame (to 0, etc.) here.
  // No one is listening.

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
    nsresult rv;
    rv = StopAnimation();
    if (NS_FAILED(rv))
      return rv;
   }

  mCurrentAnimationFrameIndex = 0;
  if (mCompositingFrame) {
    nsRect dirtyRect;
    nsCOMPtr<gfxIImageFrame> firstFrame;
    PRInt32 timeout;

    // Get Frame 1
    inlinedGetFrameAt(0, getter_AddRefs(firstFrame));
    firstFrame->GetRect(dirtyRect);

    // Copy Frame 1 back into mCompositingFrame
    BlackenFrame(mCompositingFrame);
    firstFrame->DrawTo(mCompositingFrame, dirtyRect.x, dirtyRect.y, dirtyRect.width, dirtyRect.height);
    ZeroMask(mCompositingFrame);
    BuildCompositeMask(mCompositingFrame, firstFrame);
    // Set timeout because StartAnimation reads it
    firstFrame->GetTimeout(&timeout);
    mCompositingFrame->SetTimeout(timeout);
      
    // Update display
    nsCOMPtr<imgIContainerObserver> observer(do_QueryReferent(mObserver));
    if (observer)
      observer->FrameChanged(this, nsnull, mCompositingFrame, &dirtyRect);
   }


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
  NS_ASSERTION(mTimer == timer, "uh");

  if(!mAnimating || !mTimer)
    return NS_OK;

  nsCOMPtr<imgIContainerObserver> observer(do_QueryReferent(mObserver));
  if (!observer) {
    // the imgRequest that owns us is dead, we should die now too.
    this->StopAnimation();
    return NS_OK;
  }

  nsCOMPtr<gfxIImageFrame> nextFrame;
  PRInt32 timeout = 100;
  PRUint32 numFrames = inlinedGetNumFrames();
  if(!numFrames)
    return NS_OK;

  // If we're done decoding the next frame, go ahead and display it now and reinit
  // the timer with the next frame's delay time.
  PRUint32 previousAnimationFrameIndex = mCurrentAnimationFrameIndex;
  if (mCurrentFrameIsFinishedDecoding && !mDoneDecoding) {
    // If we have the next frame in the sequence set the timer callback from it
    inlinedGetFrameAt(mCurrentAnimationFrameIndex+1, getter_AddRefs(nextFrame));
    if (nextFrame) {
      // Go to next frame in sequence
      nextFrame->GetTimeout(&timeout);
      mCurrentAnimationFrameIndex++;
    } else {
      // twiddle our thumbs
      inlinedGetFrameAt(mCurrentAnimationFrameIndex, getter_AddRefs(nextFrame));
      if(!nextFrame) return NS_OK;

      nextFrame->GetTimeout(&timeout);
    }
  } else if (mDoneDecoding){
    if ((numFrames-1) == mCurrentAnimationFrameIndex) {
      // If animation mode is "loop once", it's time to stop animating
      if (mAnimationMode == kLoopOnceAnimMode || mLoopCount == 0) {
        this->StopAnimation();
        return NS_OK;
      }

      // Go back to the beginning of the animation
      inlinedGetFrameAt(0, getter_AddRefs(nextFrame));
      if(!nextFrame) return NS_OK;

      mCurrentAnimationFrameIndex = 0;
      nextFrame->GetTimeout(&timeout);
      if(mLoopCount > 0)
        mLoopCount--;
    } else {
      mCurrentAnimationFrameIndex++;
      inlinedGetFrameAt(mCurrentAnimationFrameIndex, getter_AddRefs(nextFrame));
      if(!nextFrame) return NS_OK;

      nextFrame->GetTimeout(&timeout);
    }
  } else {
    inlinedGetFrameAt(mCurrentAnimationFrameIndex, getter_AddRefs(nextFrame));
    if(!nextFrame) return NS_OK;
  }

  if(timeout > 0)
    mTimer->SetDelay(timeout);
  else
    this->StopAnimation();

  nsRect dirtyRect;

  // update the composited frame
  if(mCompositingFrame && (previousAnimationFrameIndex != mCurrentAnimationFrameIndex)) {
    nsCOMPtr<gfxIImageFrame> frameToUse;
    DoComposite(getter_AddRefs(frameToUse), &dirtyRect, previousAnimationFrameIndex, mCurrentAnimationFrameIndex);

    // do notification to FE to draw this frame, but hand it the compositing frame
    observer->FrameChanged(this, nsnull, mCompositingFrame, &dirtyRect);
  }
  else {
    nextFrame->GetRect(dirtyRect);

    // do notification to FE to draw this frame
    observer->FrameChanged(this, nsnull, nextFrame, &dirtyRect);
  }
  return NS_OK;
}
//******************************************************************************
// DoComposite gets called when the timer for animation get fired and we have to
// update the composited frame of the animation.
void imgContainerGIF::DoComposite(gfxIImageFrame** aFrameToUse, nsRect* aDirtyRect, PRInt32 aPrevFrame, PRInt32 aNextFrame)
{
  NS_ASSERTION(aDirtyRect, "DoComposite aDirtyRect is null");
  NS_ASSERTION(mCompositingFrame, "DoComposite mCompositingFrame is null");

  *aFrameToUse = nsnull;

  PRUint32 numFrames = inlinedGetNumFrames();
  PRInt32 nextFrameIndex = aNextFrame;
  PRInt32 prevFrameIndex = aPrevFrame;

  if (PRUint32(nextFrameIndex) >= numFrames) nextFrameIndex = numFrames - 1;
  if (PRUint32(prevFrameIndex) >= numFrames) prevFrameIndex = numFrames - 1;

  nsCOMPtr<gfxIImageFrame> prevFrame;
  inlinedGetFrameAt(prevFrameIndex, getter_AddRefs(prevFrame));

  nsCOMPtr<gfxIImageFrame> nextFrame;
  inlinedGetFrameAt(nextFrameIndex, getter_AddRefs(nextFrame));

  PRInt32 x;
  PRInt32 y;
  PRInt32 width;
  PRInt32 height;
  nextFrame->GetX(&x);
  nextFrame->GetY(&y);
  nextFrame->GetWidth(&width);
  nextFrame->GetHeight(&height);

  PRInt32 nextFrameDisposalMethod;
  nextFrame->GetFrameDisposalMethod(&nextFrameDisposalMethod);
  PRInt32 prevFrameDisposalMethod;
  prevFrame->GetFrameDisposalMethod(&prevFrameDisposalMethod);

  if (nextFrameIndex == 0) {
    // First Frame: Blank First, then Draw Normal
    BlackenFrame(mCompositingFrame);
    ZeroMask(mCompositingFrame);
    (*aDirtyRect).x = 0;
    (*aDirtyRect).y = 0;
    (*aDirtyRect).width = mSize.width;
    (*aDirtyRect).height = mSize.height;
  } else {
    switch (prevFrameDisposalMethod) {
      default:
      case 0: // DISPOSE_NOT_SPECIFIED
      case 1: // DISPOSE_KEEP Leave previous frame in the framebuffer
        *aFrameToUse = mCompositingFrame;
        NS_ADDREF(*aFrameToUse);

        (*aDirtyRect).x = x;
        (*aDirtyRect).y = y;
        (*aDirtyRect).width = width;
        (*aDirtyRect).height = height;
        break;
      case 2: // DISPOSE_OVERWRITE_BGCOLOR Overwrite with background color
        {
          PRInt32 xDispose;
          PRInt32 yDispose;
          PRInt32 widthDispose;
          PRInt32 heightDispose;
          prevFrame->GetX(&xDispose);
          prevFrame->GetY(&yDispose);
          prevFrame->GetWidth(&widthDispose);
          prevFrame->GetHeight(&heightDispose);

          *aFrameToUse = mCompositingFrame;
          NS_ADDREF(*aFrameToUse);

          // Blank out previous frame area (both color & Mask/Alpha)
          BlackenFrame(mCompositingFrame, xDispose, yDispose, widthDispose, heightDispose);
          ZeroMaskArea(mCompositingFrame, xDispose, yDispose, widthDispose, heightDispose);

          // Calculate area that we need to redraw
          // which is the combination of the previous frame and this one

          // This is essentially nsRect::UnionRect()
          nscoord xmost1 = x + width;
          nscoord xmost2 = xDispose + widthDispose;
          nscoord ymost1 = y + height;
          nscoord ymost2 = yDispose + heightDispose;

          (*aDirtyRect).x = PR_MIN(x, xDispose);
          (*aDirtyRect).y = PR_MIN(y, yDispose);
          (*aDirtyRect).width = PR_MAX(xmost1, xmost2) - (*aDirtyRect).x;
          (*aDirtyRect).height = PR_MAX(ymost1, ymost2) - (*aDirtyRect).y;
        }
        break;

      case 3:
      case 4:
      // Keep prev frame, but overwrite previous frame with this one? Let's just overwrite.
      // Gif Specs say bit 4 (our value 4), but all gif generators I've seen use bit2 & bit3 (our value 3)
        PRInt32 xDispose;
        PRInt32 yDispose;
        PRInt32 widthDispose;
        PRInt32 heightDispose;
        prevFrame->GetX(&xDispose);
        prevFrame->GetY(&yDispose);
        prevFrame->GetWidth(&widthDispose);
        prevFrame->GetHeight(&heightDispose);

        // Blank out previous frame area (both color & Mask/Alpha)
        BlackenFrame(mCompositingFrame, xDispose, yDispose, widthDispose, heightDispose);
        ZeroMaskArea(mCompositingFrame, xDispose, yDispose, widthDispose, heightDispose);

        if (mCompositingPrevFrame) {
          // It would be nice to just draw the area we need.
          // but for now, draw whole frame
          mCompositingPrevFrame->DrawTo(mCompositingFrame, 0, 0, mSize.width, mSize.height);
          BuildCompositeMask(mCompositingFrame, mCompositingPrevFrame);

          // destroy only if we don't need it for this one
          if (nextFrameDisposalMethod != 3 && nextFrameDisposalMethod != 4)
            mCompositingPrevFrame = nsnull;
        }

        (*aDirtyRect).x = 0;
        (*aDirtyRect).y = 0;
        (*aDirtyRect).width = mSize.width;
        (*aDirtyRect).height = mSize.height;
        break;
    }
  }


  // Check if the frame we are composing wants the previous image restored afer it is done
  // Don't store it (again) if last frame wanted it's image restored too
  if ((nextFrameDisposalMethod == 3 || nextFrameDisposalMethod == 4) && prevFrameDisposalMethod != 3 && prevFrameDisposalMethod != 4) {
    // We are storing the whole image.
    // It would be better if we just stored the area that nextFrame is going to overwrite.
    mCompositingPrevFrame = do_CreateInstance("@mozilla.org/gfx/image/frame;2");

    gfx_format format;
    mCompositingFrame->GetFormat(&format);

    mCompositingPrevFrame->Init(0, 0, mSize.width, mSize.height, format, 24);

    PRUint8* aDataSrc;
    PRUint8* aDataDest;
    PRUint32 aDataLengthSrc;
    PRUint32 aDataLengthDest;

    // Copy Image Over
    mCompositingPrevFrame->SetTransparentColor(0);
    if (NS_SUCCEEDED(mCompositingPrevFrame->LockImageData())) {
      mCompositingFrame->GetImageData(&aDataSrc, &aDataLengthSrc);
      mCompositingPrevFrame->GetImageData(&aDataDest, &aDataLengthDest);
      if (aDataLengthDest == aDataLengthSrc)
        memcpy(aDataDest, aDataSrc, aDataLengthSrc);

      // Tell the image that it's data has been updated
      nsCOMPtr<nsIInterfaceRequestor> ireq(do_QueryInterface(mCompositingPrevFrame));
      if (ireq) {
        nsCOMPtr<nsIImage> img(do_GetInterface(ireq));
        nsRect r(0, 0, mSize.width, mSize.height);
        img->ImageUpdated(nsnull, nsImageUpdateFlags_kBitsChanged, &r);
      }
      mCompositingPrevFrame->UnlockImageData();
    }

    // Copy Alpha/Mask Over
    if (NS_SUCCEEDED(mCompositingPrevFrame->LockAlphaData())) {
      mCompositingFrame->GetAlphaData(&aDataSrc, &aDataLengthSrc);
      mCompositingPrevFrame->GetAlphaData(&aDataDest, &aDataLengthDest);
      if (aDataLengthDest == aDataLengthSrc)
        memcpy(aDataDest, aDataSrc, aDataLengthSrc);

      mCompositingPrevFrame->UnlockAlphaData();
    }
  }

  // blit next frame into it's correct spot
  nextFrame->DrawTo(mCompositingFrame, x, y, width, height);
  // put the mask in
  BuildCompositeMask(mCompositingFrame, nextFrame);

}
//******************************************************************************
/* void onStartDecode (in imgIRequest aRequest, in nsISupports cx); */
NS_IMETHODIMP imgContainerGIF::OnStartDecode(imgIRequest *aRequest, nsISupports *cx)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

//******************************************************************************
/* void onStartContainer (in imgIRequest aRequest, in nsISupports cx, in imgIContainer aContainer); */
NS_IMETHODIMP imgContainerGIF::OnStartContainer(imgIRequest *aRequest, nsISupports *cx, imgIContainer *aContainer)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

//******************************************************************************
/* void onStartFrame (in imgIRequest aRequest, in nsISupports cx, in gfxIImageFrame aFrame); */
NS_IMETHODIMP imgContainerGIF::OnStartFrame(imgIRequest *aRequest, nsISupports *cx, gfxIImageFrame *aFrame)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

//******************************************************************************
/* [noscript] void onDataAvailable (in imgIRequest aRequest, in nsISupports cx, in gfxIImageFrame aFrame, [const] in nsRect aRect); */
NS_IMETHODIMP imgContainerGIF::OnDataAvailable(imgIRequest *aRequest, nsISupports *cx, gfxIImageFrame *aFrame, const nsRect * aRect)
{
  if(mCompositingFrame && !mCurrentDecodingFrameIndex) {
    // Update the composite frame
    PRInt32 x;
    aFrame->GetX(&x);
    aFrame->DrawTo(mCompositingFrame, x, aRect->y, aRect->width, aRect->height);
    BuildCompositeMask(mCompositingFrame, aFrame);
  }
  return NS_OK;
}

//******************************************************************************
/* void onStopFrame (in imgIRequest aRequest, in nsISupports cx, in gfxIImageFrame aFrame); */
NS_IMETHODIMP imgContainerGIF::OnStopFrame(imgIRequest *aRequest, nsISupports *cx, gfxIImageFrame *aFrame)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

//******************************************************************************
/* void onStopContainer (in imgIRequest aRequest, in nsISupports cx, in imgIContainer aContainer); */
NS_IMETHODIMP imgContainerGIF::OnStopContainer(imgIRequest *aRequest, nsISupports *cx, imgIContainer *aContainer)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

//******************************************************************************
/* void onStopDecode (in imgIRequest aRequest, in nsISupports cx, in nsresult status, in wstring statusArg); */
NS_IMETHODIMP imgContainerGIF::OnStopDecode(imgIRequest *aRequest, nsISupports *cx, nsresult status, const PRUnichar *statusArg)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

//******************************************************************************
/* [noscript] void frameChanged (in imgIContainer aContainer, in nsISupports aCX, in gfxIImageFrame aFrame, in nsRect aDirtyRect); */
NS_IMETHODIMP imgContainerGIF::FrameChanged(imgIContainer *aContainer, nsISupports *aCX, gfxIImageFrame *aFrame, nsRect * aDirtyRect)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

//******************************************************************************
// Yet another thing that should be in a GIF specific container.
// This takes the mask information from the passed in aOverlayFrame and inserts
// that information into the aCompositingFrame's mask at the proper offsets. It
// does *not* rebuild the entire mask.
void imgContainerGIF::BuildCompositeMask(gfxIImageFrame *aCompositingFrame, gfxIImageFrame *aOverlayFrame)
{
  if (!aCompositingFrame || !aOverlayFrame) return;

  nsresult res;
  PRUint8* compositingAlphaData;
  PRUint32 compositingAlphaDataLength;
  aCompositingFrame->LockAlphaData();
  res = aCompositingFrame->GetAlphaData(&compositingAlphaData, &compositingAlphaDataLength);
  if (!compositingAlphaData || !compositingAlphaDataLength || NS_FAILED(res)) {
    aCompositingFrame->UnlockAlphaData();
    return;
  }

  nscoord widthOverlay;
  nscoord heightOverlay;
  PRInt32 overlayXOffset, overlayYOffset;
  aOverlayFrame->GetWidth(&widthOverlay);
  aOverlayFrame->GetHeight(&heightOverlay);
  aOverlayFrame->GetX(&overlayXOffset);
  aOverlayFrame->GetY(&overlayYOffset);

  // The current frame of the animation (overlay frame) is what
  // determines the transparent color.
  gfx_color color;
  if (NS_FAILED(aOverlayFrame->GetTransparentColor(&color))) {
    // set the region of the overlay frame to 1
    OneMaskArea(aCompositingFrame, overlayXOffset, overlayYOffset, widthOverlay, heightOverlay);
    aCompositingFrame->UnlockAlphaData();
    return;
  }

  aOverlayFrame->LockAlphaData();

  PRUint32 abprComposite;
  aCompositingFrame->GetAlphaBytesPerRow(&abprComposite);

  PRUint32 abprOverlay;
  aOverlayFrame->GetAlphaBytesPerRow(&abprOverlay);

  nscoord widthComposite;
  nscoord heightComposite;
  aCompositingFrame->GetWidth(&widthComposite);
  aCompositingFrame->GetHeight(&heightComposite);

  PRUint8* overlayAlphaData;
  PRUint32 overlayAlphaDataLength;
  res = aOverlayFrame->GetAlphaData(&overlayAlphaData, &overlayAlphaDataLength);

  gfx_format format;
  aCompositingFrame->GetFormat(&format);

    switch (format) {
    case gfxIFormats::RGB_A1:
    case gfxIFormats::BGR_A1:
      {
        const PRUint32 width  = PR_MIN(widthOverlay,(widthComposite-overlayXOffset));
        const PRUint32 height = PR_MIN(heightOverlay,(heightComposite-overlayYOffset));
        PRInt32 offset;

// Windows and OS/2 have the funky bottom up data storage we need to account for
#if defined(XP_WIN) || defined(XP_OS2)
        offset = ((heightComposite - 1) - overlayYOffset) * abprComposite;
#else
        offset = overlayYOffset*abprComposite;
#endif
        PRUint8* alphaLine = compositingAlphaData + offset + (overlayXOffset>>3);

// Windows and OS/2 have the funky bottom up data storage we need to account for
#if defined(XP_WIN) || defined(XP_OS2)
        offset = (heightOverlay - 1) * abprOverlay;
#else
        offset = 0;
#endif
        PRUint8* overlayLine = overlayAlphaData + offset;

        /*
          This is the number of pixels of offset between alpha and overlay
          (the number of bits at the front of alpha to skip when starting
          a row).
          I.e:, for a mask_offset of 3:
          (these are representations of bits)
          overlay 'pixels':   76543210 hgfedcba
          alpha:              xxx76543 210hgfed ...
          where 'x' is data already in alpha
          the first 5 pixels of overlay are or'd into the low 5 bits of alpha
        */
        PRUint8 mask_offset = (overlayXOffset & 0x7);

        for(PRUint32 i=0; i < height; i++) {
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

            if (pixels == 0) {
              // no bits to set - iterate and bump output pointer
              localAlpha++;
            }
            else {
              // for the last few bits of a line, we need to special-case it
              if (mask_offset == 0) {
                // simple case, no offset
                *localAlpha++ |= pixels;
              }
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

/// Windows and OS/2 have the funky bottom up data storage we need to account for
#if defined(XP_WIN) || defined(XP_OS2)
          alphaLine   -= abprComposite;
          overlayLine -= abprOverlay;
#else
          alphaLine   += abprComposite;
          overlayLine += abprOverlay;
#endif
        }
      }
      break;
    default:
      break;

    }

  aCompositingFrame->UnlockAlphaData();
  aOverlayFrame->UnlockAlphaData();
  return;
}

//******************************************************************************
//Based on BuildCompositeMask
void imgContainerGIF::ZeroMaskArea(gfxIImageFrame *aCompositingFrame, PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  if (!aCompositingFrame) return;

  nsresult res;
  PRUint8* compositingAlphaData;
  PRUint32 compositingAlphaDataLength;
  aCompositingFrame->LockAlphaData();
  res = aCompositingFrame->GetAlphaData(&compositingAlphaData, &compositingAlphaDataLength);
  if (!compositingAlphaData || !compositingAlphaDataLength || NS_FAILED(res)) {
    aCompositingFrame->UnlockAlphaData();
    return;
  }

  PRUint32 abprComposite;
  aCompositingFrame->GetAlphaBytesPerRow(&abprComposite);

  nscoord widthComposite;
  nscoord heightComposite;
  aCompositingFrame->GetWidth(&widthComposite);
  aCompositingFrame->GetHeight(&heightComposite);

  const PRInt32 width  = PR_MIN(aWidth,(widthComposite-aX));
  const PRInt32 height = PR_MIN(aHeight,(heightComposite-aY));

  if (width <= 0 && height <= 0) {
    aCompositingFrame->UnlockAlphaData();
    return;
  }

  gfx_format format;
  aCompositingFrame->GetFormat(&format);

  switch (format) {
    case gfxIFormats::RGB_A1:
    case gfxIFormats::BGR_A1:
      {
        PRInt32 offset;

// Windows and OS/2 have the funky bottom up data storage we need to account for
#if defined(XP_WIN) || defined(XP_OS2)
        offset = ((heightComposite - 1) - aY) * abprComposite;
#else
        offset = aY*abprComposite;
#endif
        PRUint8* alphaLine = compositingAlphaData + offset + (aX>>3);
        PRUint8 mask_offset = (aX & 0x7);

        for (PRInt32 i=0; i < height; i++) {
          PRUint8 pixels;
          PRInt32 j;
          PRUint8 *localAlpha = alphaLine;

          for (j = width; j >= 8; j -= 8) {
            // for the last few bits of a line, we need to special-case it
            if (mask_offset == 0) {
              // simple case, no offset
              *localAlpha++ = 0;
            } else {
              // set our part to 0
              *localAlpha = (*localAlpha >> (8U-mask_offset)) << (8U-mask_offset);
              *localAlpha++;
              // Store in pixels to chop off bits
              pixels = (*localAlpha << mask_offset);
              *localAlpha = pixels >> mask_offset;
            }
          }
          if (j > 0) {
            // set bits we don't want to change to 1
            pixels = ((0xFF >> (8U-j)));
            pixels = ((pixels << (8U-j)));
            *localAlpha++ &= ~(pixels >> (mask_offset));
            if (j > (8 - mask_offset))
              *localAlpha &= ~(pixels << (8U-mask_offset));
          }

/// Windows and OS/2 have the funky bottom up data storage we need to account for
#if defined(XP_WIN) || defined(XP_OS2)
          alphaLine -= abprComposite;
#else
          alphaLine += abprComposite;
#endif
        }
      }
      break;

    case gfxIFormats::RGB_A8:
    case gfxIFormats::BGR_A8:
      {
        // Should never get run, since gifs are all 1 bit alpha
        // 0% Visible
        PRInt32 offset;
#if defined(XP_WIN) || defined(XP_OS2)
        offset = ((heightComposite - 1) - aY) * abprComposite;
#else
        offset = aY*abprComposite;
#endif

        PRUint8* alphaLine = compositingAlphaData + offset + aX;

        for (PRInt32 i=0; i < height; i++) {
          memset(alphaLine, 0, width);

#if defined(XP_WIN) || defined(XP_OS2)
          alphaLine -= abprComposite;
#else
          alphaLine += abprComposite;
#endif
        }
      }
      break;

    default:
      break;
  }

  aCompositingFrame->UnlockAlphaData();
  return;
}

//******************************************************************************
//Based on BuildCompositeMask
void imgContainerGIF::OneMaskArea(gfxIImageFrame *aCompositingFrame, PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  if (!aCompositingFrame) return;

  nsresult res;
  PRUint8* compositingAlphaData;
  PRUint32 compositingAlphaDataLength;
  aCompositingFrame->LockAlphaData();
  res = aCompositingFrame->GetAlphaData(&compositingAlphaData, &compositingAlphaDataLength);
  if (!compositingAlphaData || !compositingAlphaDataLength || NS_FAILED(res)) {
    aCompositingFrame->UnlockAlphaData();
    return;
  }

  PRUint32 abprComposite;
  aCompositingFrame->GetAlphaBytesPerRow(&abprComposite);

  nscoord widthComposite;
  nscoord heightComposite;
  aCompositingFrame->GetWidth(&widthComposite);
  aCompositingFrame->GetHeight(&heightComposite);

  const PRInt32 width  = PR_MIN(aWidth,(widthComposite-aX));
  const PRInt32 height = PR_MIN(aHeight,(heightComposite-aY));

  if (width <= 0 && height <= 0) {
    aCompositingFrame->UnlockAlphaData();
    return;
  }
  gfx_format format;
  aCompositingFrame->GetFormat(&format);

  switch (format) {
    case gfxIFormats::RGB_A1:
    case gfxIFormats::BGR_A1:
      {
        PRInt32 offset;

// Windows and OS/2 have the funky bottom up data storage we need to account for
#if defined(XP_WIN) || defined(XP_OS2)
        offset = ((heightComposite - 1) - aY) * abprComposite;
#else
        offset = aY*abprComposite;
#endif
        PRUint8* alphaLine = compositingAlphaData + offset + (aX>>3);
        PRUint8 mask_offset = (aX & 0x7);

        for (PRInt32 i=0; i < height; i++) {
          PRUint8 pixels;
          PRInt32 j;
          PRUint8 *localAlpha   = alphaLine;

          for (j = width; j >= 8; j -= 8) {
            // for the last few bits of a line, we need to special-case it
            if (mask_offset == 0) {
              // simple case, no offset
              *localAlpha++ = 0xFF;
            } else {
              *localAlpha++ |= (0xFF >> mask_offset);
              *localAlpha   |= (0xFF << (8U-mask_offset));
            }
          }
          if (j > 0) {
            // handle the end of the line, 1 to 7 pixels
            // last few bits have to be handled more carefully if
            // width is not a multiple of 8.

            // set bits we don't want to change to 0
            pixels = (0xFF >> (8U-j)) << (8U-j);
            *localAlpha++ |= (pixels >> mask_offset);
            // don't touch this byte unless we have bits for it
            if (j > (8 - mask_offset))
              *localAlpha |= (pixels << (8U-mask_offset));
          }

/// Windows and OS/2 have the funky bottom up data storage we need to account for
#if defined(XP_WIN) || defined(XP_OS2)
          alphaLine -= abprComposite;
#else
          alphaLine += abprComposite;
#endif
        }
      }
      break;

    case gfxIFormats::RGB_A8:
    case gfxIFormats::BGR_A8:
      {
        // Should never get run, since gifs are all 1 bit alpha
        // 100% Visible
        PRInt32 offset;
#if defined(XP_WIN) || defined(XP_OS2)
        offset = ((heightComposite - 1) - aY) * abprComposite;
#else
        offset = aY*abprComposite;
#endif

        PRUint8* alphaLine = compositingAlphaData + offset + aX;

        for (PRInt32 i=0; i < height; i++) {
          memset(alphaLine, 255, width);

#if defined(XP_WIN) || defined(XP_OS2)
          alphaLine -= abprComposite;
#else
          alphaLine += abprComposite;
#endif
        }
      }
      break;

    default:
      break;

  }

  aCompositingFrame->UnlockAlphaData();
  return;
}
//******************************************************************************
void imgContainerGIF::ZeroMask(gfxIImageFrame *aCompositingFrame)
{
  if (!aCompositingFrame) return;
  PRUint8* compositingAlphaData;
  PRUint32 compositingAlphaDataLength;
  aCompositingFrame->LockAlphaData();
  nsresult res = aCompositingFrame->GetAlphaData(&compositingAlphaData, &compositingAlphaDataLength);
  if (NS_SUCCEEDED(res) && compositingAlphaData && compositingAlphaDataLength)
    memset(compositingAlphaData, 0, compositingAlphaDataLength);

  aCompositingFrame->UnlockAlphaData();
  return;
 }

//******************************************************************************
// Yet another thing that should be in a GIF specific container.
// Fill aFrame with black. Does not change the mask.
void imgContainerGIF::BlackenFrame(gfxIImageFrame *aFrame)
{
  if (!aFrame) return;

  aFrame->LockImageData();

  nscoord width;
  nscoord height;
  aFrame->GetWidth(&width);
  aFrame->GetHeight(&height);

  PRUint8* aData;
  PRUint32 aDataLength;

  aFrame->GetImageData(&aData, &aDataLength);
  memset(aData, 0, aDataLength);

  nsCOMPtr<nsIInterfaceRequestor> ireq(do_QueryInterface(aFrame));
  if (ireq) {
    nsCOMPtr<nsIImage> img(do_GetInterface(ireq));
    nsRect r(0, 0, width, height);

    img->ImageUpdated(nsnull, nsImageUpdateFlags_kBitsChanged, &r);
  }

  aFrame->UnlockImageData();
}

//******************************************************************************
void imgContainerGIF::BlackenFrame(gfxIImageFrame *aFrame, PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  if (!aFrame) return;

  aFrame->LockImageData();

  nscoord widthFrame;
  nscoord heightFrame;
  aFrame->GetWidth(&widthFrame);
  aFrame->GetHeight(&heightFrame);

  const PRInt32 width  = PR_MIN(aWidth,(widthFrame-aX));
  const PRInt32 height = PR_MIN(aHeight,(heightFrame-aY));

  if (width <= 0 || height <= 0) {
    aFrame->UnlockImageData();
    return;
  }

  PRUint32 bpr; // Bytes Per Row
  aFrame->GetImageBytesPerRow(&bpr);

#if defined(XP_MAC) || defined(XP_MACOSX)
  const PRUint8 bpp = 4;
#else
  const PRUint8 bpp = 3;
#endif
  const PRUint32 bprToWrite = width * bpp;
  const PRUint32 xOffset = aX * bpp; // offset into row to start writing

  PRUint8* tmpRow = NS_STATIC_CAST(PRUint8*, nsMemory::Alloc(width * bpp));

  if (!tmpRow) {
    aFrame->UnlockImageData();
    return;
  }

  memset(tmpRow, 0, width * bpp);

  for (PRInt32 y=0; y<height; y++) {
    aFrame->SetImageData(tmpRow, bprToWrite, ((y+aY) * bpr) + xOffset);
  }
  nsMemory::Free(tmpRow);

  aFrame->UnlockImageData();
}

