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
                        public nsITimerCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_IMGICONTAINER
  NS_DECL_NSITIMERCALLBACK


  imgContainerGIF();
  virtual ~imgContainerGIF();

  /**
   *  New Image Data has been decoded, update the composite frame as needed
   */
  NS_IMETHOD NewFrameData(gfxIImageFrame *aFrame, const nsRect * aRect);

private:
  /** "Disposal" method indicates how the image should be handled before the 
   *   subsequent image is displayed. 
   */
  enum {
    DISPOSE_CLEAR_ALL       = -1, //!< Clear the whole image, revealing
                                  //!  what was there before the gif displayed
    DISPOSE_NOT_SPECIFIED    = 0, //!< Leave frame, let new frame draw on top
    DISPOSE_KEEP             = 1, //!< Leave frame, let new frame draw on top
    DISPOSE_CLEAR            = 2, //!< Clear the frame's area, revealing bg
    DISPOSE_RESTORE_PREVIOUS = 3  //!< Restore the previous (composited) frame
  };

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
    if (mLastCompositedFrameIndex == mCurrentAnimationFrameIndex) {
      *_retval = mCompositingFrame;
      NS_ADDREF(*_retval);
      return NS_OK;
    }
    return inlinedGetFrameAt(mCurrentAnimationFrameIndex, _retval);
  }

  // Private function for doing the frame compositing of animations and in cases
  // where there is a backgound color and single frame placed withing a larger
  // logical screen size. Smart GIF compressors may do this to save space.
  void DoComposite(gfxIImageFrame** aFrameToUse, nsRect* aDirtyRect,
                   PRInt32 aPrevFrame, PRInt32 aNextFrame);

  /** 
   * Combine aOverlayFrame's mask into aCompositingFrame's mask.
   *
   * This takes the mask information from the passed in aOverlayFrame and 
   * inserts that information into the aCompositingFrame's mask at the proper 
   * offsets. It does *not* rebuild the entire mask.
   *
   * @param aCompositingFrame Target frame
   * @param aOverlayFrame     This frame's mask is being copied
   */
  void BuildCompositeMask(gfxIImageFrame* aCompositingFrame, 
                          gfxIImageFrame* aOverlayFrame);

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
                         PRInt32 aX, PRInt32 aY, 
                         PRInt32 aWidth, PRInt32 aHeight, 
                         PRBool aVisible);
  //! @overload
  void SetMaskVisibility(gfxIImageFrame *aFrame, 
                         nsRect &aRect, PRBool aVisible) {
    SetMaskVisibility(aFrame, aRect.x, aRect.y, 
                      aRect.width, aRect.height, aVisible);
  }

  /** Fills an area of <aFrame> with black.
   *
   * @param aFrame Target Frame
   *
   * @note Does not set the mask
   */
  void BlackenFrame(gfxIImageFrame* aFrame);
  //! @overload
  void BlackenFrame(gfxIImageFrame* aFrame, 
                    PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight);
  //! @overload
  inline void BlackenFrame(gfxIImageFrame* aFrame, nsRect &aRect) {
    BlackenFrame(aFrame, aRect.x, aRect.y, aRect.width, aRect.height);
  }

  //! imgIContainerObserver; used for telling observers that the frame changed
  nsWeakPtr            mObserver;
  //! All the <gfxIImageFrame>s of the GIF
  nsSupportsArray      mFrames;
  //! Size of GIF (not necessarily the frame)
  nsSize               mSize;
  //! Area of the first frame that needs to be redrawn on subsequent loops
  nsRect               mFirstFrameRefreshArea;

  PRInt32              mCurrentDecodingFrameIndex; // 0 to numFrames-1
  PRInt32              mCurrentAnimationFrameIndex; // 0 to numFrames-1
  //! Track the last composited frame for Optimizations (See DoComposite code)
  PRInt32              mLastCompositedFrameIndex;
  PRBool               mCurrentFrameIsFinishedDecoding;
  //! Whether we can assume there will be no more frames
  //! (and thus loop the animation)
  PRBool               mDoneDecoding;


  //! Are we currently animating the GIF?
  PRBool               mAnimating;
  //! See imgIContainer for mode constants
  PRUint16             mAnimationMode;
  //! # loops remaining before animation stops (-1 no stop)
  PRInt32              mLoopCount;

  nsCOMPtr<nsITimer>   mTimer;

  /** For managing blending of frames
   *
   * Some GIF animations will use the mCompositeFrame to composite images
   * and just hand this back to the caller when it is time to draw the frame.
   * NOTE: When clearing mCompositingFrame, remember to set 
   *       mLastCompositedFrameIndex to -1.  Code assume that if
   *       mLastCompositedFrameIndex >= 0 then mCompositingFrame exists.
   */
  nsCOMPtr<gfxIImageFrame> mCompositingFrame;

  /** the previous composited frame, for DISPOSE_RESTORE_PREVIOUS
   *
   * The Previous Frame (all frames composited up to the current) needs to be
   * stored in cases where the GIF specifies it wants the last frame back 
   * when it's done with the current frame.
   */
  nsCOMPtr<gfxIImageFrame> mCompositingPrevFrame;
};

#endif /* __imgContainerGIF_h__ */
