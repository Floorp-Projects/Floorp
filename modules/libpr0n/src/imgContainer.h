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

#ifndef __imgContainer_h__
#define __imgContainer_h__

#include "imgIContainer.h"

#include "imgIContainerObserver.h"

#include "nsSize.h"

#include "nsSupportsArray.h"

#include "nsCOMPtr.h"
#include "nsITimer.h"
#include "nsITimerCallback.h"
#include "imgIDecoderObserver.h"

#include "gfxIImageFrame.h"

#include "nsWeakReference.h"

#ifdef __GNUC__
#define CANT_INLINE_GETTER
#endif

#define NS_IMGCONTAINER_CID \
{ /* 5e04ec5e-1dd2-11b2-8fda-c4db5fb666e0 */         \
     0x5e04ec5e,                                     \
     0x1dd2,                                         \
     0x11b2,                                         \
    {0x8f, 0xda, 0xc4, 0xdb, 0x5f, 0xb6, 0x66, 0xe0} \
}

class imgContainer : public imgIContainer,
                     public nsITimerCallback,
                     public imgIDecoderObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_IMGICONTAINER
  NS_DECL_IMGIDECODEROBSERVER
  NS_DECL_IMGICONTAINEROBSERVER

  NS_IMETHOD_(void) Notify(nsITimer *timer);

  imgContainer();
  virtual ~imgContainer();

private:

  inline PRUint32 inlinedGetNumFrames() {
    PRUint32 nframes;
    mFrames.Count(&nframes);
    return nframes;
  }

#ifdef CANT_INLINE_GETTER
  nsresult inlinedGetFrameAt(PRUint32 index, gfxIImageFrame **_retval);
#else
  inline nsresult inlinedGetFrameAt(PRUint32 index, gfxIImageFrame **_retval) {
    *_retval = NS_STATIC_CAST(gfxIImageFrame*, mFrames.ElementAt(index));
    if (!*_retval) return NS_ERROR_FAILURE;
    return NS_OK;
  }
#endif

  inline nsresult inlinedGetCurrentFrame(gfxIImageFrame **_retval) {
    if (mCompositingFrame) {
      *_retval = mCompositingFrame;
      NS_ADDREF(*_retval);
      return NS_OK;
    }

    return inlinedGetFrameAt(mCurrentAnimationFrameIndex, _retval);
  }

  /* additional members */
  nsSupportsArray mFrames;
  nsSize mSize;
  nsWeakPtr mObserver;

  PRUint32 mCurrentDecodingFrameIndex; // 0 to numFrames-1
  PRUint32 mCurrentAnimationFrameIndex; // 0 to numFrames-1
  PRBool   mCurrentFrameIsFinishedDecoding;
  PRBool   mDoneDecoding;
  PRBool   mAnimating;
  PRUint16 mAnimationMode;
  PRInt32  mLoopCount;

private:

  // GIF specific bits
  nsCOMPtr<nsITimer> mTimer;

  // GIF animations will use the mCompositingFrame to composite images
  // and just hand this back to the caller when it is time to draw the frame.
  nsCOMPtr<gfxIImageFrame> mCompositingFrame;
  
  // Private function for doing the frame compositing of animations and in cases
  // where there is a backgound color and single frame placed withing a larger
  // logical screen size. Smart GIF compressors may do this to save space.
  void DoComposite(gfxIImageFrame** aFrameToUse, nsRect* aDirtyRect,
                   PRInt32 aPrevFrame, PRInt32 aNextFrame);

  void BuildCompositeMask(gfxIImageFrame* aCompositingFrame, gfxIImageFrame* aOverlayFrame);
  void ZeroMask(gfxIImageFrame *aCompositingFrame);
  void FillWithColor(gfxIImageFrame* aFrame, gfx_color color);

};

#endif /* __imgContainer_h__ */

