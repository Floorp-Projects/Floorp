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

#include "gfxIImageFrame.h"

NS_IMPL_ISUPPORTS2(imgContainer, imgIContainer, nsITimerCallback)

imgContainer::imgContainer()
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
  mCurrentFrame = 0;
  mCurrentAnimationFrame = 0;
  mCurrentFrameIsFinishedDecoding = PR_FALSE;
  mDoneDecoding = PR_FALSE;
}

imgContainer::~imgContainer()
{
  /* destructor code */
  mFrames.Clear();

  if (mTimer)
    mTimer->Cancel();
}



/* void init (in nscoord aWidth, in nscoord aHeight, in imgIContainerObserver aObserver); */
NS_IMETHODIMP imgContainer::Init(nscoord aWidth, nscoord aHeight, imgIContainerObserver *aObserver)
{
  if (aWidth <= 0 || aHeight <= 0) {
    printf("error - negative image size\n");
    return NS_ERROR_FAILURE;
  }

  mSize.SizeTo(aWidth, aHeight);

  mObserver = aObserver;

  return NS_OK;
}

/* readonly attribute gfx_format preferredAlphaChannelFormat; */
NS_IMETHODIMP imgContainer::GetPreferredAlphaChannelFormat(gfx_format *aFormat)
{
  /* default.. platform's should probably overwrite this */
  *aFormat = gfxIFormats::RGB_A8;
  return NS_OK;
}

/* readonly attribute nscoord width; */
NS_IMETHODIMP imgContainer::GetWidth(nscoord *aWidth)
{
  *aWidth = mSize.width;
  return NS_OK;
}

/* readonly attribute nscoord height; */
NS_IMETHODIMP imgContainer::GetHeight(nscoord *aHeight)
{
  *aHeight = mSize.height;
  return NS_OK;
}


/* readonly attribute gfxIImageFrame currentFrame; */
NS_IMETHODIMP imgContainer::GetCurrentFrame(gfxIImageFrame * *aCurrentFrame)
{
  return this->GetFrameAt(mCurrentFrame, aCurrentFrame);
}

/* readonly attribute unsigned long numFrames; */
NS_IMETHODIMP imgContainer::GetNumFrames(PRUint32 *aNumFrames)
{
  return mFrames.Count(aNumFrames);
}

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

/* void appendFrame (in gfxIImageFrame item); */
NS_IMETHODIMP imgContainer::AppendFrame(gfxIImageFrame *item)
{
  // If this is our second frame, init a timer so we don't display
  // the next frame until the delay timer has expired for the current
  // frame.

  PRUint32 numFrames;
  this->GetNumFrames(&numFrames);

  if (!mTimer){
    if (numFrames > 1) {
      // Since we have more than one frame we need a timer
      mTimer = do_CreateInstance("@mozilla.org/timer;1");
      
      PRInt32 timeout;
      nsCOMPtr<gfxIImageFrame> currentFrame;
      this->GetFrameAt(mCurrentFrame, getter_AddRefs(currentFrame));
      currentFrame->GetTimeout(&timeout);
      if (timeout != -1 &&
         timeout >= 0) { // -1 means display this frame forever
        
        mTimer->Init(NS_STATIC_CAST(nsITimerCallback*, this),
                     //                     timeout, NS_PRIORITY_NORMAL, NS_TYPE_ONE_SHOT);
                     timeout, NS_PRIORITY_NORMAL, NS_TYPE_REPEATING_SLACK);
      }
    }
  }

  if (numFrames > 0) mCurrentFrame++;

  mCurrentFrameIsFinishedDecoding = PR_FALSE;

  return mFrames.AppendElement(NS_STATIC_CAST(nsISupports*, item));
}

/* void removeFrame (in gfxIImageFrame item); */
NS_IMETHODIMP imgContainer::RemoveFrame(gfxIImageFrame *item)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void endFrameDecode (in gfxIImageFrame item, in unsigned long timeout); */
NS_IMETHODIMP imgContainer::EndFrameDecode(PRUint32 aFrameNum, PRUint32 aTimeout)
{
  // It is now okay to start the timer for the next frame in the animation
  mCurrentFrameIsFinishedDecoding = PR_TRUE;
  return NS_OK;
}

/* void decodingComplete (); */
NS_IMETHODIMP imgContainer::DecodingComplete(void)
{
  mDoneDecoding = PR_TRUE;
  return NS_OK;
}

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

/* void startAnimation () */
NS_IMETHODIMP imgContainer::StartAnimation()
{
  if (mTimer)
    return NS_OK;

  printf("imgContainer::StartAnimation()\n");

  PRUint32 numFrames;
  this->GetNumFrames(&numFrames);

  if (numFrames > 1) {
  
    mTimer = do_CreateInstance("@mozilla.org/timer;1");

    PRInt32 timeout;
    nsCOMPtr<gfxIImageFrame> currentFrame;
    this->GetCurrentFrame(getter_AddRefs(currentFrame));
    if (currentFrame) {
      currentFrame->GetTimeout(&timeout);
      if (timeout != -1 &&
          timeout >= 0) { // -1 means display this frame forever

        mTimer->Init(NS_STATIC_CAST(nsITimerCallback*, this),
                     timeout, NS_PRIORITY_NORMAL, NS_TYPE_REPEATING_SLACK);
      }
    } else {
      // XXX hack.. the timer notify code will do the right thing, so just get that started
      mTimer->Init(NS_STATIC_CAST(nsITimerCallback*, this),
                   100, NS_PRIORITY_NORMAL, NS_TYPE_REPEATING_SLACK);
    }
  }

  return NS_OK;
}

/* void stopAnimation (); */
NS_IMETHODIMP imgContainer::StopAnimation()
{
  if (!mTimer)
    return NS_OK;

  printf("imgContainer::StopAnimation()\n");

  if (mTimer)
    mTimer->Cancel();
  
  mTimer = nsnull;

  // don't bother trying to change the frame (to 0, etc.) here.
  // No one is listening.

  return NS_OK;
}

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

  nsCOMPtr<gfxIImageFrame> nextFrame;
  PRInt32 timeout = 100;
      
  printf("timer callback\n");
  
  // If we're done decoding the next frame, go ahead and display it now and reinit
  // the timer with the next frame's delay time.
  if (mCurrentFrameIsFinishedDecoding && !mDoneDecoding) {
    // If we have the next frame in the sequence set the timer callback from it
    GetFrameAt(mCurrentAnimationFrame + 1, getter_AddRefs(nextFrame));
    if (nextFrame) {
      // Go to next frame in sequence
      nextFrame->GetTimeout(&timeout);
      mCurrentAnimationFrame++;
    } else if (mDoneDecoding) {
      // Go back to the beginning of the loop
      GetFrameAt(0, getter_AddRefs(nextFrame));
      nextFrame->GetTimeout(&timeout);
      mCurrentAnimationFrame = 0;
    } else {
      // twiddle our thumbs
      GetFrameAt(mCurrentAnimationFrame, getter_AddRefs(nextFrame));
      nextFrame->GetTimeout(&timeout);
    }
  } else if (mDoneDecoding){
    PRUint32 numFrames;
    GetNumFrames(&numFrames);
    if (numFrames == mCurrentAnimationFrame) {
      GetFrameAt(0, getter_AddRefs(nextFrame));
      mCurrentAnimationFrame = 0;
      nextFrame->GetTimeout(&timeout);
    } else {
      GetFrameAt(mCurrentAnimationFrame++, getter_AddRefs(nextFrame));
      nextFrame->GetTimeout(&timeout);
    }
    mCurrentFrame = mCurrentAnimationFrame;
  } else {
    GetFrameAt(mCurrentFrame, getter_AddRefs(nextFrame));
    nextFrame->GetTimeout(&timeout);
  }
  
  printf(" mCurrentAnimationFrame = %d\n", mCurrentAnimationFrame);
  
  // XXX do notification to FE to draw this frame
  nsRect* dirtyRect;
  nextFrame->GetRect(&dirtyRect);
  
  printf("x=%d, y=%d, w=%d, h=%d\n", dirtyRect->x, dirtyRect->y,
         dirtyRect->width, dirtyRect->height);

  if (mObserver)
    mObserver->FrameChanged(this, nsnull, nextFrame, dirtyRect);

#if 0
  mTimer->Cancel();

  mTimer->Init(NS_STATIC_CAST(nsITimerCallback*, this),
               timeout, NS_PRIORITY_NORMAL, NS_TYPE_REPEATING_PRECISE);
#endif

}
