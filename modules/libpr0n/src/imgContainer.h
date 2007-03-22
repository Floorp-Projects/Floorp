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
 * This file declares the imgContainer class, which
 * handles static and animated image containers.
 *
 * @author  Stuart Parmenter <pavlov@netscape.com>
 * @author  Chris Saari <saari@netscape.com>
 * @author  Arron Mogge <paper@animecity.nu>
 * @author  Andrew Smith <asmith15@learn.senecac.on.ca>
 */

#ifndef __imgContainer_h__
#define __imgContainer_h__

#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "imgIContainer.h"
#include "gfxIImageFrame.h"
#include "nsIProperties.h"
#include "nsITimer.h"
#include "nsWeakReference.h"

#define NS_IMGCONTAINER_CID \
{ /* 27f0682c-ff64-4dd2-ae7a-668e59f2fd38 */         \
     0x27f0682c,                                     \
     0xff64,                                         \
     0x4dd2,                                         \
    {0xae, 0x7a, 0x66, 0x8e, 0x59, 0xf2, 0xfd, 0x38} \
}

/**
 * Handles static and animated image containers.
 *
 *
 * @par A Quick Walk Through
 * The decoder initializes this class and calls AppendFrame() to add a frame.
 * Once imgContainer detects more than one frame, it starts the animation
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
 * Each frame can have a different method of removing itself. These are
 * listed as imgIContainer::cDispose... constants.  Notify() calls 
 * DoComposite() to handle any special frame destruction.
 *
 * @par
 * The basic path through DoComposite() is:
 * 1) Calculate Area that needs updating, which is at least the area of
 *    aNextFrame.
 * 2) Dispose of previous frame.
 * 3) Draw new image onto compositingFrame.
 * See comments in DoComposite() for more information and optimizations.
 *
 * @par
 * The rest of the imgContainer specific functions are used by DoComposite to
 * destroy the old frame and build the new one.
 *
 * @note
 * <li> "Mask", "Alpha", and "Alpha Level" are interchangable phrases in
 * respects to imgContainerGIF.
 *
 * @par
 * <li> GIFs never have more than a 1 bit alpha.
 * <li> APNGs may have a full alpha channel.
 *
 * @par
 * <li> Background color specified in GIF is ignored by web browsers.
 *
 * @par
 * <li> If Frame 3 wants to dispose by restoring previous, what it wants is to
 * restore the composition up to and including Frame 2, as well as Frame 2s
 * disposal.  So, in the middle of DoComposite when composing Frame 3, right
 * after destroying Frame 2's area, we copy compositingFrame to
 * prevCompositingFrame.  When DoComposite gets called to do Frame 4, we
 * copy prevCompositingFrame back, and then draw Frame 4 on top.
 *
 * @par
 * The mAnim structure has members only needed for animated images, so
 * it's not allocated until the second frame is added.
 *
 * @note
 * mAnimationMode, mLoopCount and mObserver are not in the mAnim structure
 * because the first two have public setters and the observer we only get
 * in Init().
 */
class imgContainer : public imgIContainer, 
                     public nsITimerCallback, 
                     public nsIProperties
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_IMGICONTAINER
  NS_DECL_NSITIMERCALLBACK
  NS_FORWARD_SAFE_NSIPROPERTIES(mProperties)

  imgContainer();
  virtual ~imgContainer();

private:
  friend class nsGIFDecoder2;
  
  struct Anim
  {
    //! Area of the first frame that needs to be redrawn on subsequent loops.
    nsIntRect                  firstFrameRefreshArea;
    // Note this doesn't hold a proper value until frame 2 finished decoding.
    PRInt32                    currentDecodingFrameIndex; // 0 to numFrames-1
    PRInt32                    currentAnimationFrameIndex; // 0 to numFrames-1
    //! Track the last composited frame for Optimizations (See DoComposite code)
    PRInt32                    lastCompositedFrameIndex;
    //! Whether we can assume there will be no more frames
    //! (and thus loop the animation)
    PRBool                     doneDecoding;
    //! Are we currently animating the image?
    PRBool                     animating;
    /** For managing blending of frames
     *
     * Some animations will use the compositingFrame to composite images
     * and just hand this back to the caller when it is time to draw the frame.
     * NOTE: When clearing compositingFrame, remember to set
     *       lastCompositedFrameIndex to -1.  Code assume that if
     *       lastCompositedFrameIndex >= 0 then compositingFrame exists.
     */
    nsCOMPtr<gfxIImageFrame>   compositingFrame;
    /** the previous composited frame, for DISPOSE_RESTORE_PREVIOUS
     *
     * The Previous Frame (all frames composited up to the current) needs to be
     * stored in cases where the image specifies it wants the last frame back
     * when it's done with the current frame.
     */
    nsCOMPtr<gfxIImageFrame>   compositingPrevFrame;
    //! Timer to animate multiframed images
    nsCOMPtr<nsITimer>         timer;
    
    Anim() :
      firstFrameRefreshArea(),
      currentDecodingFrameIndex(0),
      currentAnimationFrameIndex(0),
      lastCompositedFrameIndex(-1),
      doneDecoding(PR_FALSE),
      animating(PR_FALSE)
    {
      ;
    }
    ~Anim()
    {
      if (timer)
        timer->Cancel();
    }
  };
  
  inline gfxIImageFrame* inlinedGetCurrentFrame() {
    if (!mAnim)
      return mFrames.SafeObjectAt(0);
    if (mAnim->lastCompositedFrameIndex == mAnim->currentAnimationFrameIndex)
      return mAnim->compositingFrame;
    return mFrames.SafeObjectAt(mAnim->currentAnimationFrameIndex);
  }
  
  inline Anim* ensureAnimExists() {
    if (!mAnim)
      mAnim = new Anim();
    return mAnim;
  }
  
  /** Function for doing the frame compositing of animations
   *
   * @param aFrameToUse Set by DoComposite
   *                   (aNextFrame, compositingFrame, or compositingPrevFrame)
   * @param aDirtyRect  Area that the display will need to update
   * @param aPrevFrame  Last Frame seen/processed
   * @param aNextFrame  Frame we need to incorperate/display
   * @param aNextFrameIndex Position of aNextFrame in mFrames list
   */
  nsresult DoComposite(gfxIImageFrame** aFrameToUse, nsIntRect* aDirtyRect,
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
                         nsIntRect &aRect, PRBool aVisible) {
    SetMaskVisibility(aFrame, aRect.x, aRect.y,
                      aRect.width, aRect.height, aVisible);
  }
  
  /** Fills an area of <aFrame> with black.
   *
   * @param aFrame Target Frame
   *
   * @note Does not set the mask
   */
  static void BlackenFrame(gfxIImageFrame* aFrame);
  //! @overload
  static void BlackenFrame(gfxIImageFrame* aFrame,
                    PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight);
  //! @overload
  static inline void BlackenFrame(gfxIImageFrame* aFrame, nsIntRect &aRect) {
    BlackenFrame(aFrame, aRect.x, aRect.y, aRect.width, aRect.height);
  }
  
  //! Copy one gfxIImageFrame's image and mask into another
  static PRBool CopyFrameImage(gfxIImageFrame *aSrcFrame,
                               gfxIImageFrame *aDstFrame);
  
  nsIntSize                  mSize;
  
  //! All the <gfxIImageFrame>s of the PNG
  nsCOMArray<gfxIImageFrame> mFrames;
  
  nsCOMPtr<nsIProperties>    mProperties;
  
  imgContainer::Anim*        mAnim;
  
  //! See imgIContainer for mode constants
  PRUint16                   mAnimationMode;
  
  //! # loops remaining before animation stops (-1 no stop)
  PRInt32                    mLoopCount;
  
  //! imgIContainerObserver
  nsWeakPtr                  mObserver;
};

#endif /* __imgContainer_h__ */
