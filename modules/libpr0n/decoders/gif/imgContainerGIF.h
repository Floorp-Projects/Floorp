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
 *   Arron Mogge <paper@animecity.nu>
 */

#ifndef _imgContainerGIF_h_
#define _imgContainerGIF_h_

#include "imgIContainerObserver.h"
#include "imgIContainer.h"
#include "nsSize.h"
#include "nsSupportsArray.h"
#include "nsCOMPtr.h"
#include "nsITimer.h"
#include "imgIDecoderObserver.h"
#include "gfxIImageFrame.h"
#include "nsWeakReference.h"

#define NS_GIFCONTAINER_CID \
{ /* da72e7ee-4821-4452-802d-5eb2d865dd3c */         \
     0xda72e7ee,                                     \
     0x4821,                                         \
     0x4452,                                         \
    {0x80, 0x2d, 0x5e, 0xb2, 0xd8, 0x65, 0xdd, 0x3c} \
}

class imgContainerGIF : public imgIContainer,
                        public nsITimerCallback,
                        public imgIDecoderObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_IMGICONTAINER
  NS_DECL_IMGIDECODEROBSERVER
  NS_DECL_IMGICONTAINEROBSERVER
  NS_DECL_NSITIMERCALLBACK


  imgContainerGIF();
  virtual ~imgContainerGIF();

private:

  inline PRUint32 inlinedGetNumFrames() {
    PRUint32 nframes;
    mFrames.Count(&nframes);
    return nframes;
  }

  inline nsresult inlinedGetFrameAt(PRUint32 index, gfxIImageFrame **_retval) {
    // callers DO try to go past the end
    nsISupports *_elem = mFrames.ElementAt(index);
    if (!_elem) return NS_ERROR_FAILURE;
    *_retval = NS_STATIC_CAST(gfxIImageFrame*, _elem);
    return NS_OK;
  }

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
  nsCOMPtr<gfxIImageFrame> mCompositingPrevFrame;

  // Private function for doing the frame compositing of animations and in cases
  // where there is a backgound color and single frame placed withing a larger
  // logical screen size. Smart GIF compressors may do this to save space.
  void DoComposite(gfxIImageFrame** aFrameToUse, nsRect* aDirtyRect,
                   PRInt32 aPrevFrame, PRInt32 aNextFrame);

  void BuildCompositeMask(gfxIImageFrame* aCompositingFrame, gfxIImageFrame* aOverlayFrame);

  /** Sets an area of the frame's mask.
   *
   * @param aFrame Target Frame
   * @param aVisible Turn on (PR_TRUE) or off (PR_FALSE) visibility
   *
   * @note Invisible area of frame's image will need to be set to 0
   */
  void SetMaskVisibility(gfxIImageFrame *aFrame, PRBool aVisible);
  //! @overload
  void SetMaskVisibility(gfxIImageFrame *aFrame,
                         PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight,
                         PRBool aVisible);
  //! @overload
  void SetMaskVisibility(gfxIImageFrame *aFrame, nsRect &aRect, PRBool aVisible) {
    SetMaskVisibility(aFrame, aRect.x, aRect.y, aRect.width, aRect.height, aVisible);
  }

  /** Fills an area of <aFrame> with black.
   *
   * @param aFrame Target Frame
   *
   * @note Does not set the mask
   */
  void BlackenFrame(gfxIImageFrame* aFrame);
  //! @overload
  void BlackenFrame(gfxIImageFrame *aFrame, PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight);
  //! @overload
  inline void BlackenFrame(gfxIImageFrame* aFrame, nsRect &aRect) {
    BlackenFrame(aFrame, aRect.x, aRect.y, aRect.width, aRect.height);
  }
};

#endif /* __imgContainerGIF_h__ */
