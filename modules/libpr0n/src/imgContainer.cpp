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
 */

#include "imgContainer.h"

#include "nsIServiceManager.h"
#include "nsIInterfaceRequestor.h"
#include "gfxIImageFrame.h"
#include "nsIImage.h"

NS_IMPL_ISUPPORTS3(imgContainer, imgIContainer, nsITimerCallback,imgIDecoderObserver)

//******************************************************************************
imgContainer::imgContainer()
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
  mCurrentDecodingFrameIndex = 0;
  mCurrentAnimationFrameIndex = 0;
  mCurrentFrameIsFinishedDecoding = PR_FALSE;
  mDoneDecoding = PR_FALSE;
  mAnimating = PR_FALSE;
  mObserver = nsnull;
}

//******************************************************************************
imgContainer::~imgContainer()
{
  if (mTimer)
    mTimer->Cancel();
    
  /* destructor code */
  mFrames.Clear();
}

//******************************************************************************
/* void init (in nscoord aWidth, in nscoord aHeight, in imgIContainerObserver aObserver); */
NS_IMETHODIMP imgContainer::Init(nscoord aWidth, nscoord aHeight, imgIContainerObserver *aObserver)
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
NS_IMETHODIMP imgContainer::GetPreferredAlphaChannelFormat(gfx_format *aFormat)
{
  /* default.. platform's should probably overwrite this */
  *aFormat = gfxIFormats::RGB_A8;
  return NS_OK;
}

//******************************************************************************
/* readonly attribute nscoord width; */
NS_IMETHODIMP imgContainer::GetWidth(nscoord *aWidth)
{
  *aWidth = mSize.width;
  return NS_OK;
}

//******************************************************************************
/* readonly attribute nscoord height; */
NS_IMETHODIMP imgContainer::GetHeight(nscoord *aHeight)
{
  *aHeight = mSize.height;
  return NS_OK;
}

//******************************************************************************
/* readonly attribute gfxIImageFrame currentFrame; */
NS_IMETHODIMP imgContainer::GetCurrentFrame(gfxIImageFrame * *aCurrentFrame)
{
  if(mCompositingFrame)
    return mCompositingFrame->QueryInterface(NS_GET_IID(gfxIImageFrame), (void**)aCurrentFrame); // addrefs again
  else
    return this->GetFrameAt(mCurrentAnimationFrameIndex, aCurrentFrame);
}

//******************************************************************************
/* readonly attribute unsigned long numFrames; */
NS_IMETHODIMP imgContainer::GetNumFrames(PRUint32 *aNumFrames)
{
  return mFrames.Count(aNumFrames);
}

//******************************************************************************
/* gfxIImageFrame getFrameAt (in unsigned long index); */
NS_IMETHODIMP imgContainer::GetFrameAt(PRUint32 index, gfxIImageFrame **_retval)
{
  nsISupports *sup = mFrames.ElementAt(index); // addrefs
  if (!sup)
    return NS_ERROR_FAILURE;

  nsresult rv;
  rv = sup->QueryInterface(NS_GET_IID(gfxIImageFrame), (void**)_retval); // addrefs again

  NS_RELEASE(sup);

  return rv;
}

//******************************************************************************
/* void appendFrame (in gfxIImageFrame item); */
NS_IMETHODIMP imgContainer::AppendFrame(gfxIImageFrame *item)
{
  // If we don't have a composite frame already allocated, make sure that our container
  // size is the same the frame size. Otherwise, we'll either need the composite frame
  // for animation compositing (GIF) or for filling in with a background color.
  // XXX IMPORTANT: this means that the frame should be initialized BEFORE appending to container
  PRUint32 numFrames;
  this->GetNumFrames(&numFrames);
  
  if(!mCompositingFrame) {
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
      mCompositingFrame->Init(0, 0, mSize.width, mSize.height, gfxIFormats::RGB); 
      nsCOMPtr<nsIImage> img(do_GetInterface(mCompositingFrame));
      img->SetDecodedRect(0, 0, mSize.width, mSize.height);
      
      nsCOMPtr<gfxIImageFrame> firstFrame;
      this->GetFrameAt(0, getter_AddRefs(firstFrame));
      firstFrame->DrawTo(mCompositingFrame, 0, 0, mSize.width, mSize.height);
    }
  }
  // If this is our second frame, init a timer so we don't display
  // the next frame until the delay timer has expired for the current
  // frame.

  if (!mTimer && (numFrames >= 1)) {
      PRInt32 timeout;
      nsCOMPtr<gfxIImageFrame> currentFrame;
      this->GetFrameAt(mCurrentDecodingFrameIndex, getter_AddRefs(currentFrame));
      currentFrame->GetTimeout(&timeout);
      if (timeout != -1 &&
         timeout >= 0) { // -1 means display this frame forever
        
        if(mAnimating) {
          // Since we have more than one frame we need a timer
          mTimer = do_CreateInstance("@mozilla.org/timer;1");
          mTimer->Init(
            NS_STATIC_CAST(nsITimerCallback*, this), 
            timeout, NS_PRIORITY_NORMAL, NS_TYPE_REPEATING_SLACK);
        }
      }
  }
 
  if (numFrames > 0) mCurrentDecodingFrameIndex++;

  mCurrentFrameIsFinishedDecoding = PR_FALSE;

  return mFrames.AppendElement(NS_STATIC_CAST(nsISupports*, item));
}

//******************************************************************************
/* void removeFrame (in gfxIImageFrame item); */
NS_IMETHODIMP imgContainer::RemoveFrame(gfxIImageFrame *item)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

//******************************************************************************
/* void endFrameDecode (in gfxIImageFrame item, in unsigned long timeout); */
NS_IMETHODIMP imgContainer::EndFrameDecode(PRUint32 aFrameNum, PRUint32 aTimeout)
{
  // It is now okay to start the timer for the next frame in the animation
  mCurrentFrameIsFinishedDecoding = PR_TRUE;

  nsCOMPtr<gfxIImageFrame> currentFrame;
  this->GetFrameAt(aFrameNum-1, getter_AddRefs(currentFrame));
  currentFrame->SetTimeout(aTimeout);
      
  if (!mTimer && mAnimating){
    PRUint32 numFrames;
    this->GetNumFrames(&numFrames);
    if (numFrames > 1) {
        if (aTimeout != -1 &&
            aTimeout >= 0) { // -1 means display this frame forever

          mAnimating = PR_TRUE;
          mTimer = do_CreateInstance("@mozilla.org/timer;1");
      
          mTimer->Init(NS_STATIC_CAST(nsITimerCallback*, this),
                     aTimeout, NS_PRIORITY_NORMAL, NS_TYPE_REPEATING_SLACK);
        }
    }
  }
  return NS_OK;
}

//******************************************************************************
/* void decodingComplete (); */
NS_IMETHODIMP imgContainer::DecodingComplete(void)
{
  mDoneDecoding = PR_TRUE;
  return NS_OK;
}

//******************************************************************************
/* nsIEnumerator enumerate (); */
NS_IMETHODIMP imgContainer::Enumerate(nsIEnumerator **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void clear (); */
NS_IMETHODIMP imgContainer::Clear()
{
  return mFrames.Clear();
}

//******************************************************************************
/* void startAnimation () */
NS_IMETHODIMP imgContainer::StartAnimation()
{
  mAnimating = PR_TRUE;
        
  if (mTimer)
    return NS_OK;

  PRUint32 numFrames;
  this->GetNumFrames(&numFrames);

  if (numFrames > 1) {
    PRInt32 timeout;
    nsCOMPtr<gfxIImageFrame> currentFrame;
    this->GetCurrentFrame(getter_AddRefs(currentFrame));
    if (currentFrame) {
      currentFrame->GetTimeout(&timeout);
      if (timeout != -1 &&
          timeout >= 0) { // -1 means display this frame forever

        mAnimating = PR_TRUE;
        if(!mTimer) mTimer = do_CreateInstance("@mozilla.org/timer;1");
      
        mTimer->Init(NS_STATIC_CAST(nsITimerCallback*, this),
                     timeout, NS_PRIORITY_NORMAL, NS_TYPE_REPEATING_SLACK);
      }
    } else {
      // XXX hack.. the timer notify code will do the right thing, so just get that started
      mAnimating = PR_TRUE;
      if(!mTimer) mTimer = do_CreateInstance("@mozilla.org/timer;1");
      
      mTimer->Init(NS_STATIC_CAST(nsITimerCallback*, this),
                   100, NS_PRIORITY_NORMAL, NS_TYPE_REPEATING_SLACK);
    }
  }

  return NS_OK;
}

//******************************************************************************
/* void stopAnimation (); */
NS_IMETHODIMP imgContainer::StopAnimation()
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
/* attribute long loopCount; */
NS_IMETHODIMP imgContainer::GetLoopCount(PRInt32 *aLoopCount)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP imgContainer::SetLoopCount(PRInt32 aLoopCount)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP_(void) imgContainer::Notify(nsITimer *timer)
{
  NS_ASSERTION(mTimer == timer, "uh");

  if(!mAnimating || !mTimer)
    return;

  nsCOMPtr<imgIContainerObserver> observer(do_QueryReferent(mObserver));
  if (!observer) {
    // the imgRequest that owns us is dead, we should die now too.
    this->StopAnimation();
    return;
  }

  nsCOMPtr<gfxIImageFrame> nextFrame;
  PRInt32 timeout = 100;
  PRUint32 numFrames;
  GetNumFrames(&numFrames);
  if(!numFrames)
    return;
  
  // If we're done decoding the next frame, go ahead and display it now and reinit
  // the timer with the next frame's delay time.
  PRUint32 previousAnimationFrameIndex = mCurrentAnimationFrameIndex;
  if (mCurrentFrameIsFinishedDecoding && !mDoneDecoding) {
    // If we have the next frame in the sequence set the timer callback from it
    GetFrameAt(mCurrentAnimationFrameIndex+1, getter_AddRefs(nextFrame));
    if (nextFrame) {
      // Go to next frame in sequence
      nextFrame->GetTimeout(&timeout);
      mCurrentAnimationFrameIndex++;
    } else {
      // twiddle our thumbs
      GetFrameAt(mCurrentAnimationFrameIndex, getter_AddRefs(nextFrame));
      if(!nextFrame) return;
      
      nextFrame->GetTimeout(&timeout);
    }
  } else if (mDoneDecoding){
    if ((numFrames-1) == mCurrentAnimationFrameIndex) {
      // Go back to the beginning of the animation
      GetFrameAt(0, getter_AddRefs(nextFrame));
      if(!nextFrame) return;
      
      mCurrentAnimationFrameIndex = 0;
      nextFrame->GetTimeout(&timeout);
    } else {
      mCurrentAnimationFrameIndex++;
      GetFrameAt(mCurrentAnimationFrameIndex, getter_AddRefs(nextFrame));
      if(!nextFrame) return;
      
      nextFrame->GetTimeout(&timeout);
    }
  } else {
    GetFrameAt(mCurrentAnimationFrameIndex, getter_AddRefs(nextFrame));
    if(!nextFrame) return;
  }

  if(timeout >= 0)
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


}
//******************************************************************************
// DoComposite gets called when the timer for animation get fired and we have to
// update the composited frame of the animation.
void imgContainer::DoComposite(gfxIImageFrame** aFrameToUse, nsRect* aDirtyRect, PRInt32 aPrevFrame, PRInt32 aNextFrame)
{
  NS_ASSERTION(aDirtyRect, "DoComposite aDirtyRect is null");
  NS_ASSERTION(mCompositingFrame, "DoComposite mCompositingFrame is null");
  
  *aFrameToUse = nsnull;
  
  PRUint32 numFrames;
  this->GetNumFrames(&numFrames);
  PRInt32 nextFrameIndex = aNextFrame;
  PRInt32 prevFrameIndex = aPrevFrame;
  
  if(nextFrameIndex >= numFrames) nextFrameIndex = numFrames-1;
  if(prevFrameIndex >= numFrames) prevFrameIndex = numFrames-1;
  
  nsCOMPtr<gfxIImageFrame> prevFrame;
  this->GetFrameAt(prevFrameIndex, getter_AddRefs(prevFrame));
  PRInt32 prevFrameDisposalMethod;
  prevFrame->GetFrameDisposalMethod(&prevFrameDisposalMethod);
  
  nsCOMPtr<gfxIImageFrame> nextFrame;
  this->GetFrameAt(nextFrameIndex, getter_AddRefs(nextFrame));
  
  PRInt32 x;
  PRInt32 y;
  PRInt32 width;
  PRInt32 height;
  nextFrame->GetX(&x);
  nextFrame->GetY(&y);
  nextFrame->GetWidth(&width);
  nextFrame->GetHeight(&height);
  
  switch (prevFrameDisposalMethod) {
    default:
    case 0: // DISPOSE_NOT_SPECIFIED
    case 1: // DISPOSE_KEEP Leave previous frame in the framebuffer
      mCompositingFrame->QueryInterface(NS_GET_IID(gfxIImageFrame), (void**)aFrameToUse); // addrefs again
      //XXX blit into the composite frame too!!!
      nextFrame->DrawTo(mCompositingFrame, x, y, width, height);
          
      // we're drawing only the updated frame
      (*aDirtyRect).x = x;
      (*aDirtyRect).y = y;
      (*aDirtyRect).width = width;
      (*aDirtyRect).height = height;
    break;
    
    case 2: // DISPOSE_OVERWRITE_BGCOLOR Overwrite with background color
      //XXX overwrite mCompositeFrame with background color
      gfx_color backgroundColor;
      nextFrame->GetBackgroundColor(&backgroundColor);
      //XXX Do background color overwrite of mCompositeFrame here
      
      // blit next frame into this clean slate
      nextFrame->DrawTo(mCompositingFrame, x, y, width, height);
      
      // In this case we need to blit the whole composite frame  
      (*aDirtyRect).x = 0;
      (*aDirtyRect).y = 0;
      (*aDirtyRect).width = mSize.width;
      (*aDirtyRect).height = mSize.height;
      mCompositingFrame->QueryInterface(NS_GET_IID(gfxIImageFrame), (void**)aFrameToUse); // addrefs again
    break;
    
    case 4: // DISPOSE_OVERWRITE_PREVIOUS Save-under
      //XXX Reblit previous composite into frame buffer
      // 
      (*aDirtyRect).x = 0;
      (*aDirtyRect).y = 0;
      (*aDirtyRect).width = mSize.width;
      (*aDirtyRect).height = mSize.height;
    break;
  }
  
  // Get the next frame's disposal method, if it is it DISPOSE_OVER, save off
  // this mCompositeFrame for reblitting when this timer gets fired again and
  // we 
  PRInt32 nextFrameDisposalMethod;
  nextFrame->GetFrameDisposalMethod(&nextFrameDisposalMethod);
  //XXX if(nextFrameDisposalMethod == 4)
  // blit mPreviousCompositeFrame with this frame
}
//******************************************************************************
/* void onStartDecode (in imgIRequest aRequest, in nsISupports cx); */
NS_IMETHODIMP imgContainer::OnStartDecode(imgIRequest *aRequest, nsISupports *cx)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

//******************************************************************************
/* void onStartContainer (in imgIRequest aRequest, in nsISupports cx, in imgIContainer aContainer); */
NS_IMETHODIMP imgContainer::OnStartContainer(imgIRequest *aRequest, nsISupports *cx, imgIContainer *aContainer)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

//******************************************************************************
/* void onStartFrame (in imgIRequest aRequest, in nsISupports cx, in gfxIImageFrame aFrame); */
NS_IMETHODIMP imgContainer::OnStartFrame(imgIRequest *aRequest, nsISupports *cx, gfxIImageFrame *aFrame)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

//******************************************************************************
/* [noscript] void onDataAvailable (in imgIRequest aRequest, in nsISupports cx, in gfxIImageFrame aFrame, [const] in nsRect aRect); */
NS_IMETHODIMP imgContainer::OnDataAvailable(imgIRequest *aRequest, nsISupports *cx, gfxIImageFrame *aFrame, const nsRect * aRect)
{
  if(mCompositingFrame && !mCurrentDecodingFrameIndex) {
    // Update the composite frame
    PRInt32 x;
    aFrame->GetX(&x);
    aFrame->DrawTo(mCompositingFrame, x, aRect->y, aRect->width, aRect->height);
  }
    return NS_OK;
}

//******************************************************************************
/* void onStopFrame (in imgIRequest aRequest, in nsISupports cx, in gfxIImageFrame aFrame); */
NS_IMETHODIMP imgContainer::OnStopFrame(imgIRequest *aRequest, nsISupports *cx, gfxIImageFrame *aFrame)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

//******************************************************************************
/* void onStopContainer (in imgIRequest aRequest, in nsISupports cx, in imgIContainer aContainer); */
NS_IMETHODIMP imgContainer::OnStopContainer(imgIRequest *aRequest, nsISupports *cx, imgIContainer *aContainer)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

//******************************************************************************
/* void onStopDecode (in imgIRequest aRequest, in nsISupports cx, in nsresult status, in wstring statusArg); */
NS_IMETHODIMP imgContainer::OnStopDecode(imgIRequest *aRequest, nsISupports *cx, nsresult status, const PRUnichar *statusArg)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

//******************************************************************************
/* [noscript] void frameChanged (in imgIContainer aContainer, in nsISupports aCX, in gfxIImageFrame aFrame, in nsRect aDirtyRect); */
NS_IMETHODIMP imgContainer::FrameChanged(imgIContainer *aContainer, nsISupports *aCX, gfxIImageFrame *aFrame, nsRect * aDirtyRect)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

