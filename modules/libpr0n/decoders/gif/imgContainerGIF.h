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

/** @file
 * This file declares the imgContainerGIF class, which
 * handles animation of GIF frames.
 *
 * @author  Stuart Parmenter <pavlov@netscape.com>
 * @author  Chris Saari <saari@netscape.com>
 * @author  Arron Mogge <paper@animecity.nu>
 */

#ifndef _imgContainerGIF_h_
#define _imgContainerGIF_h_

#include "imgIContainerObserver.h"
#include "imgIContainer.h"
#include "nsCOMArray.h"
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


/**
 * Handles animation of GIF frames.
 *
 *
 * @par A Quick Walk Through
 * nsGIFDecoder initializes this class and calls AppendFrame() to add a frame.
 * Once imgContainerGIF detects more than one frame, it starts the animation
 * with StartAnimation().
 *
 * @par
 * StartAnimation() checks if animating is allowed, and creates a timer.  The
 * timer calls Notify when the specified frame delay time is up.
 *
 * @par
 * Notify() moves on to the next frame, sets up the new timer delay, destroys
 * the old frame, and forces a redraw via observer->FrameChanged().
 *
 * @par
 * Each GIF frame can have a different method of removing itself. These are
 * listed as an enum prefixed with DISPOSE_.  Notify() calls DoComposite() to
 * handle any special frame destruction.
 *
 * @par
 * The basic path through DoComposite() is:
 * 1) Calculate Area that needs updating, which is at least the area of
 *    aNextFrame.
 * 2) Dispose of previous frame.
 * 3) Draw new image onto mCompositingFrame.
 * See comments in DoComposite() for more information and optimizations.
 *
 * @par
 * The rest of the imgContainerGIF specific functions are used by DoComposite to
 * destroy the old frame and build the new one.
 *
 * @note
 * <li> "Mask", "Alpha", and "Alpha Level" are interchangable phrases in
 * respects to imgContainerGIF.
 *
 * @par
 * <li> GIFs never have more than a 1 bit alpha.
 *
 * @par
 * <li> GIFs are stored in a 24bit buffer.  Although one GIF frame can never
 * have more than 256 colors, due to frame disposal methods, one composited
 * frame could end up with far more than 256 colors.  (In the future each
 * frame in mFrames[..] may be 8bit, and the compositing frames 24)
 *
 * @par
 * <li> Background color specified in GIF is ignored by web browsers.
 *
 * @par
 * <li> If Frame 3 wants to dispose by restoring previous, what it wants is to
 * restore the composition up to and including Frame 2, as well as Frame 2s
 * disposal.  So, in the middle of DoComposite when composing Frame 3, right
 * after destroying Frame 2's area, we copy mCompositingFrame to
 * mPrevCompositingFrame.  When DoComposite get's called to do Frame 4, we
 * copy mPrevCompositingFrame back, and then draw Frame 4 on top.
 */
class imgContainerGIF : public imgIContainer,
                        public nsITimerCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_IMGICONTAINER
  NS_DECL_NSITIMERCALLBACK

  imgContainerGIF();
  virtual ~imgContainerGIF();

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

  inline gfxIImageFrame* inlinedGetCurrentFrame() {
    if (mLastCompositedFrameIndex == mCurrentAnimationFrameIndex)
      return mCompositingFrame;

    return mFrames.SafeObjectAt(mCurrentAnimationFrameIndex);
  }

  /** Function for doing the frame compositing of animations
   *
   * @param aFrameToUse Set by DoComposite
   *                   (aNextFrame, mCompositingFrame, or mCompositingPrevFrame)
   * @param aDirtyRect  Area that the display will need to update
   * @param aPrevFrame  Last Frame seen/processed
   * @param aNextFrame  Frame we need to incorperate/display
   * @param aNextFrameIndex Position of aNextFrame in mFrames list
   */
  nsresult DoComposite(gfxIImageFrame** aFrameToUse, nsRect* aDirtyRect,
                       gfxIImageFrame* aPrevFrame,
                       gfxIImageFrame* aNextFrame,
                       PRInt32 aNextFrameIndex);

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

  //! Copy one gfxIImageFrame's image and mask into another
  static PRBool CopyFrameImage(gfxIImageFrame *aSrcFrame,
                               gfxIImageFrame *aDstFrame);


  //! imgIContainerObserver; used for telling observers that the frame changed
  nsWeakPtr                  mObserver;
  //! All the <gfxIImageFrame>s of the GIF
  nsCOMArray<gfxIImageFrame> mFrames;

  //! Size of GIF (not necessarily the frame)
  nsSize                     mSize;
  //! Area of the first frame that needs to be redrawn on subsequent loops
  nsRect                     mFirstFrameRefreshArea;

  PRInt32                    mCurrentDecodingFrameIndex; // 0 to numFrames-1
  PRInt32                    mCurrentAnimationFrameIndex; // 0 to numFrames-1
  //! Track the last composited frame for Optimizations (See DoComposite code)
  PRInt32                    mLastCompositedFrameIndex;
  //! Whether we can assume there will be no more frames
  //! (and thus loop the animation)
  PRBool                     mDoneDecoding;


  //! Are we currently animating the GIF?
  PRBool                     mAnimating;
  //! See imgIContainer for mode constants
  PRUint16                   mAnimationMode;
  //! # loops remaining before animation stops (-1 no stop)
  PRInt32                    mLoopCount;

  //! Timer to animate multiframed images
  nsCOMPtr<nsITimer>         mTimer;

  /** For managing blending of frames
   *
   * Some GIF animations will use the mCompositingFrame to composite images
   * and just hand this back to the caller when it is time to draw the frame.
   * NOTE: When clearing mCompositingFrame, remember to set
   *       mLastCompositedFrameIndex to -1.  Code assume that if
   *       mLastCompositedFrameIndex >= 0 then mCompositingFrame exists.
   */
  nsCOMPtr<gfxIImageFrame>   mCompositingFrame;

  /** the previous composited frame, for DISPOSE_RESTORE_PREVIOUS
   *
   * The Previous Frame (all frames composited up to the current) needs to be
   * stored in cases where the GIF specifies it wants the last frame back
   * when it's done with the current frame.
   */
  nsCOMPtr<gfxIImageFrame>   mCompositingPrevFrame;
};

#endif /* __imgContainerGIF_h__ */
