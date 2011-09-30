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
 *   Federico Mena-Quintero <federico@novell.com>
 *   Bobby Holley <bobbyholley@gmail.com>
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
 * This file declares the RasterImage class, which
 * handles static and animated rasterized images.
 *
 * @author  Stuart Parmenter <pavlov@netscape.com>
 * @author  Chris Saari <saari@netscape.com>
 * @author  Arron Mogge <paper@animecity.nu>
 * @author  Andrew Smith <asmith15@learn.senecac.on.ca>
 */

#ifndef mozilla_imagelib_RasterImage_h_
#define mozilla_imagelib_RasterImage_h_

#include "Image.h"
#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "imgIContainer.h"
#include "nsIProperties.h"
#include "nsITimer.h"
#include "nsWeakReference.h"
#include "nsTArray.h"
#include "imgFrame.h"
#include "nsThreadUtils.h"
#include "DiscardTracker.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Telemetry.h"
#ifdef DEBUG
  #include "imgIContainerDebug.h"
#endif

class imgIDecoder;
class nsIInputStream;

#define NS_RASTERIMAGE_CID \
{ /* 376ff2c1-9bf6-418a-b143-3340c00112f7 */         \
     0x376ff2c1,                                     \
     0x9bf6,                                         \
     0x418a,                                         \
    {0xb1, 0x43, 0x33, 0x40, 0xc0, 0x01, 0x12, 0xf7} \
}

/**
 * Handles static and animated image containers.
 *
 *
 * @par A Quick Walk Through
 * The decoder initializes this class and calls AppendFrame() to add a frame.
 * Once RasterImage detects more than one frame, it starts the animation
 * with StartAnimation().
 *
 * @par
 * StartAnimation() creates a timer.  The timer calls Notify when the
 * specified frame delay time is up.
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
 * The rest of the RasterImage specific functions are used by DoComposite to
 * destroy the old frame and build the new one.
 *
 * @note
 * <li> "Mask", "Alpha", and "Alpha Level" are interchangeable phrases in
 * respects to RasterImage.
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

namespace mozilla {
namespace imagelib {

class imgDecodeWorker;
class Decoder;

class RasterImage : public Image
                  , public nsITimerCallback
                  , public nsIProperties
                  , public nsSupportsWeakReference
#ifdef DEBUG
                  , public imgIContainerDebug
#endif
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSIPROPERTIES
#ifdef DEBUG
  NS_DECL_IMGICONTAINERDEBUG
#endif

  // BEGIN NS_DECL_IMGICONTAINER (minus GetAnimationMode/SetAnimationMode)
  // ** Don't edit this chunk except to mirror changes in imgIContainer.idl **
  NS_SCRIPTABLE NS_IMETHOD GetWidth(PRInt32 *aWidth);
  NS_SCRIPTABLE NS_IMETHOD GetHeight(PRInt32 *aHeight);
  NS_SCRIPTABLE NS_IMETHOD GetType(PRUint16 *aType);
  NS_IMETHOD_(PRUint16) GetType(void);
  NS_SCRIPTABLE NS_IMETHOD GetAnimated(bool *aAnimated);
  NS_SCRIPTABLE NS_IMETHOD GetCurrentFrameIsOpaque(bool *aCurrentFrameIsOpaque);
  NS_IMETHOD GetFrame(PRUint32 aWhichFrame, PRUint32 aFlags, gfxASurface **_retval NS_OUTPARAM);
  NS_IMETHOD CopyFrame(PRUint32 aWhichFrame, PRUint32 aFlags, gfxImageSurface **_retval NS_OUTPARAM);
  NS_IMETHOD ExtractFrame(PRUint32 aWhichFrame, const nsIntRect & aRect, PRUint32 aFlags, imgIContainer **_retval NS_OUTPARAM);
  NS_IMETHOD Draw(gfxContext *aContext, gfxPattern::GraphicsFilter aFilter, const gfxMatrix & aUserSpaceToImageSpace, const gfxRect & aFill, const nsIntRect & aSubimage, const nsIntSize & aViewportSize, PRUint32 aFlags);
  NS_IMETHOD_(nsIFrame *) GetRootLayoutFrame(void);
  NS_SCRIPTABLE NS_IMETHOD RequestDecode(void);
  NS_SCRIPTABLE NS_IMETHOD LockImage(void);
  NS_SCRIPTABLE NS_IMETHOD UnlockImage(void);
  NS_SCRIPTABLE NS_IMETHOD ResetAnimation(void);
  // END NS_DECL_IMGICONTAINER

  RasterImage(imgStatusTracker* aStatusTracker = nsnull);
  virtual ~RasterImage();

  virtual nsresult StartAnimation();
  virtual nsresult StopAnimation();

  // Methods inherited from Image
  nsresult Init(imgIDecoderObserver* aObserver,
                const char* aMimeType,
                const char* aURIString,
                PRUint32 aFlags);
  void     GetCurrentFrameRect(nsIntRect& aRect);

  // Raster-specific methods
  static NS_METHOD WriteToRasterImage(nsIInputStream* aIn, void* aClosure,
                                      const char* aFromRawSegment,
                                      PRUint32 aToOffset, PRUint32 aCount,
                                      PRUint32* aWriteCount);

  /* The index of the current frame that would be drawn if the image was to be
   * drawn now. */
  PRUint32 GetCurrentFrameIndex();

  /* The total number of frames in this image. */
  PRUint32 GetNumFrames();

  virtual PRUint32 GetDecodedHeapSize();
  virtual PRUint32 GetDecodedNonheapSize();
  virtual PRUint32 GetDecodedOutOfProcessSize();
  virtual PRUint32 GetSourceHeapSize();

  /* Triggers discarding. */
  void Discard(bool force = false);
  void ForceDiscard() { Discard(/* force = */ true); }

  /* Callbacks for decoders */
  nsresult SetFrameDisposalMethod(PRUint32 aFrameNum,
                                  PRInt32 aDisposalMethod);
  nsresult SetFrameTimeout(PRUint32 aFrameNum, PRInt32 aTimeout);
  nsresult SetFrameBlendMethod(PRUint32 aFrameNum, PRInt32 aBlendMethod);
  nsresult SetFrameHasNoAlpha(PRUint32 aFrameNum);

  /**
   * Sets the size of the container. This should only be called by the
   * decoder. This function may be called multiple times, but will throw an
   * error if subsequent calls do not match the first.
   */
  nsresult SetSize(PRInt32 aWidth, PRInt32 aHeight);


  /**
   * Ensures that a given frame number exists with the given parameters, and
   * returns pointers to the data storage for that frame.
   * It is not possible to create sparse frame arrays; you can only append
   * frames to the current frame array.
   */
  nsresult EnsureFrame(PRUint32 aFramenum, PRInt32 aX, PRInt32 aY,
                       PRInt32 aWidth, PRInt32 aHeight,
                       gfxASurface::gfxImageFormat aFormat,
                       PRUint8 aPaletteDepth,
                       PRUint8** imageData,
                       PRUint32* imageLength,
                       PRUint32** paletteData,
                       PRUint32* paletteLength);

  /**
   * A shorthand for EnsureFrame, above, with aPaletteDepth = 0 and paletteData
   * and paletteLength set to null.
   */
  nsresult EnsureFrame(PRUint32 aFramenum, PRInt32 aX, PRInt32 aY,
                       PRInt32 aWidth, PRInt32 aHeight,
                       gfxASurface::gfxImageFormat aFormat,
                       PRUint8** imageData,
                       PRUint32* imageLength);

  void FrameUpdated(PRUint32 aFrameNum, nsIntRect& aUpdatedRect);

  /* notification that the entire image has been decoded */
  nsresult DecodingComplete();

  /**
   * Number of times to loop the image.
   * @note -1 means forever.
   */
  void     SetLoopCount(PRInt32 aLoopCount);

  /* Add compressed source data to the imgContainer.
   *
   * The decoder will use this data, either immediately or at draw time, to
   * decode the image.
   *
   * XXX This method's only caller (WriteToContainer) ignores the return
   * value. Should this just return void?
   */
  nsresult AddSourceData(const char *aBuffer, PRUint32 aCount);

  /* Called after the all the source data has been added with addSourceData. */
  nsresult SourceDataComplete();

  /* Called for multipart images when there's a new source image to add. */
  nsresult NewSourceData();

  /**
   * A hint of the number of bytes of source data that the image contains. If
   * called early on, this can help reduce copying and reallocations by
   * appropriately preallocating the source data buffer.
   *
   * We take this approach rather than having the source data management code do
   * something more complicated (like chunklisting) because HTTP is by far the
   * dominant source of images, and the Content-Length header is quite reliable.
   * Thus, pre-allocation simplifies code and reduces the total number of
   * allocations.
   */
  nsresult SetSourceSizeHint(PRUint32 sizeHint);

  // "Blend" method indicates how the current image is combined with the
  // previous image.
  enum {
    // All color components of the frame, including alpha, overwrite the current
    // contents of the frame's output buffer region
    kBlendSource =  0,

    // The frame should be composited onto the output buffer based on its alpha,
    // using a simple OVER operation
    kBlendOver
  };

  enum {
    kDisposeClearAll         = -1, // Clear the whole image, revealing
                                   // what was there before the gif displayed
    kDisposeNotSpecified,   // Leave frame, let new frame draw on top
    kDisposeKeep,           // Leave frame, let new frame draw on top
    kDisposeClear,          // Clear the frame's area, revealing bg
    kDisposeRestorePrevious // Restore the previous (composited) frame
  };

  // Progressive decoding knobs
  static void SetDecodeBytesAtATime(PRUint32 aBytesAtATime);
  static void SetMaxMSBeforeYield(PRUint32 aMaxMS);
  static void SetMaxBytesForSyncDecode(PRUint32 aMaxBytes);

  const char* GetURIString() { return mURIString.get();}

private:
  struct Anim
  {
    //! Area of the first frame that needs to be redrawn on subsequent loops.
    nsIntRect                  firstFrameRefreshArea;
    PRUint32                   currentAnimationFrameIndex; // 0 to numFrames-1
    //! Track the last composited frame for Optimizations (See DoComposite code)
    PRInt32                    lastCompositedFrameIndex;
    /** For managing blending of frames
     *
     * Some animations will use the compositingFrame to composite images
     * and just hand this back to the caller when it is time to draw the frame.
     * NOTE: When clearing compositingFrame, remember to set
     *       lastCompositedFrameIndex to -1.  Code assume that if
     *       lastCompositedFrameIndex >= 0 then compositingFrame exists.
     */
    nsAutoPtr<imgFrame>        compositingFrame;
    /** the previous composited frame, for DISPOSE_RESTORE_PREVIOUS
     *
     * The Previous Frame (all frames composited up to the current) needs to be
     * stored in cases where the image specifies it wants the last frame back
     * when it's done with the current frame.
     */
    nsAutoPtr<imgFrame>        compositingPrevFrame;
    //! Timer to animate multiframed images
    nsCOMPtr<nsITimer>         timer;

    Anim() :
      firstFrameRefreshArea(),
      currentAnimationFrameIndex(0),
      lastCompositedFrameIndex(-1)
    {
      ;
    }
    ~Anim()
    {
      if (timer)
        timer->Cancel();
    }
  };

  /**
   * Deletes and nulls out the frame in mFrames[framenum].
   *
   * Does not change the size of mFrames.
   *
   * @param framenum The index of the frame to be deleted. 
   *                 Must lie in [0, mFrames.Length() )
   */
  void DeleteImgFrame(PRUint32 framenum);

  imgFrame* GetImgFrameNoDecode(PRUint32 framenum);
  imgFrame* GetImgFrame(PRUint32 framenum);
  imgFrame* GetDrawableImgFrame(PRUint32 framenum);
  imgFrame* GetCurrentImgFrame();
  imgFrame* GetCurrentDrawableImgFrame();
  PRUint32 GetCurrentImgFrameIndex() const;
  
  inline void EnsureAnimExists()
  {
    if (!mAnim) {

      // Create the animation context
      mAnim = new Anim();

      // We don't support discarding animated images (See bug 414259).
      // Lock the image and throw away the key.
      // 
      // Note that this is inefficient, since we could get rid of the source
      // data too. However, doing this is actually hard, because we're probably
      // calling ensureAnimExists mid-decode, and thus we're decoding out of
      // the source buffer. Since we're going to fix this anyway later, and
      // since we didn't kill the source data in the old world either, locking
      // is acceptable for the moment.
      LockImage();
    }
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
  nsresult DoComposite(imgFrame** aFrameToUse, nsIntRect* aDirtyRect,
                       imgFrame* aPrevFrame,
                       imgFrame* aNextFrame,
                       PRInt32 aNextFrameIndex);
  
  /** Clears an area of <aFrame> with transparent black.
   *
   * @param aFrame Target Frame
   *
   * @note Does also clears the transparancy mask
   */
  static void ClearFrame(imgFrame* aFrame);
  
  //! @overload
  static void ClearFrame(imgFrame* aFrame, nsIntRect &aRect);
  
  //! Copy one frames's image and mask into another
  static bool CopyFrameImage(imgFrame *aSrcFrame,
                               imgFrame *aDstFrame);
  
  /** Draws one frames's image to into another,
   * at the position specified by aRect
   *
   * @param aSrcFrame  Frame providing the source image
   * @param aDstFrame  Frame where the image is drawn into
   * @param aRect      The position and size to draw the image
   */
  static nsresult DrawFrameTo(imgFrame *aSrcFrame,
                              imgFrame *aDstFrame,
                              nsIntRect& aRect);

  nsresult InternalAddFrameHelper(PRUint32 framenum, imgFrame *frame,
                                  PRUint8 **imageData, PRUint32 *imageLength,
                                  PRUint32 **paletteData, PRUint32 *paletteLength);
  nsresult InternalAddFrame(PRUint32 framenum, PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight,
                            gfxASurface::gfxImageFormat aFormat, PRUint8 aPaletteDepth,
                            PRUint8 **imageData, PRUint32 *imageLength,
                            PRUint32 **paletteData, PRUint32 *paletteLength);

private: // data

  nsIntSize                  mSize;

  // Whether mFrames below were decoded using any special flags.
  // Some flags (e.g. unpremultiplied data) may not be compatible
  // with the browser's needs for displaying the image to the user.
  // As such, we may need to redecode if we're being asked for
  // a frame with different flags.  0 indicates default flags.
  //
  // Valid flag bits are imgIContainer::FLAG_DECODE_NO_PREMULTIPLY_ALPHA
  // and imgIContainer::FLAG_DECODE_NO_COLORSPACE_CONVERSION.
  PRUint32                   mFrameDecodeFlags;

  //! All the frames of the image
  // IMPORTANT: if you use mFrames in a method, call EnsureImageIsDecoded() first 
  // to ensure that the frames actually exist (they may have been discarded to save
  // memory, or we may be decoding on draw).
  nsTArray<imgFrame *>       mFrames;
  
  nsCOMPtr<nsIProperties>    mProperties;

  // IMPORTANT: if you use mAnim in a method, call EnsureImageIsDecoded() first to ensure
  // that the frames actually exist (they may have been discarded to save memory, or
  // we maybe decoding on draw).
  RasterImage::Anim*        mAnim;
  
  //! # loops remaining before animation stops (-1 no stop)
  PRInt32                    mLoopCount;
  
  //! imgIDecoderObserver
  nsWeakPtr                  mObserver;

  // Discard members
  PRUint32                   mLockCount;
  DiscardTrackerNode         mDiscardTrackerNode;

  // Source data members
  FallibleTArray<char>       mSourceData;
  nsCString                  mSourceDataMimeType;
  nsCString                  mURIString;

  friend class imgDecodeWorker;
  friend class DiscardTracker;

  // Decoder and friends
  nsRefPtr<Decoder>              mDecoder;
  nsRefPtr<imgDecodeWorker>      mWorker;
  PRUint32                       mBytesDecoded;

  // How many times we've decoded this image.
  // This is currently only used for statistics
  PRInt32                        mDecodeCount;

#ifdef DEBUG
  PRUint32                       mFramesNotified;
#endif

  // Boolean flags (clustered together to conserve space):
  bool                       mHasSize:1;       // Has SetSize() been called?
  bool                       mDecodeOnDraw:1;  // Decoding on draw?
  bool                       mMultipart:1;     // Multipart?
  bool                       mDiscardable:1;   // Is container discardable?
  bool                       mHasSourceData:1; // Do we have source data?

  // Do we have the frames in decoded form?
  bool                       mDecoded:1;
  bool                       mHasBeenDecoded:1;

  // Helpers for decoder
  bool                       mWorkerPending:1;
  bool                       mInDecoder:1;

  // Whether the animation can stop, due to running out
  // of frames, or no more owning request
  bool                       mAnimationFinished:1;

  // Decoding
  nsresult WantDecodedFrames();
  nsresult SyncDecode();
  nsresult InitDecoder(bool aDoSizeDecode);
  nsresult WriteToDecoder(const char *aBuffer, PRUint32 aCount);
  nsresult DecodeSomeData(PRUint32 aMaxBytes);
  bool     IsDecodeFinished();
  TimeStamp mDrawStartTime;

  // Decoder shutdown
  enum eShutdownIntent {
    eShutdownIntent_Done        = 0,
    eShutdownIntent_Interrupted = 1,
    eShutdownIntent_Error       = 2,
    eShutdownIntent_AllCount    = 3
  };
  nsresult ShutdownDecoder(eShutdownIntent aIntent);

  // Helpers
  void DoError();
  bool CanDiscard();
  bool CanForciblyDiscard();
  bool DiscardingActive();
  bool StoringSourceData();

protected:
  bool ShouldAnimate();
};

// XXXdholbert These helper classes should move to be inside the
// scope of the RasterImage class.
// Decoding Helper Class
//
// We use this class to mimic the interactivity benefits of threading
// in a single-threaded event loop. We want to progressively decode
// and keep a responsive UI while we're at it, so we have a runnable
// class that does a bit of decoding, and then "yields" by dispatching
// itself to the end of the event queue.
class imgDecodeWorker : public nsRunnable
{
  public:
    imgDecodeWorker(imgIContainer* aContainer) {
      mContainer = do_GetWeakReference(aContainer);
    }
    NS_IMETHOD Run();
    NS_METHOD  Dispatch();

  private:
    nsWeakPtr mContainer;
    TimeDuration mDecodeTime; // the default constructor initializes to 0
};

// Asynchronous Decode Requestor
//
// We use this class when someone calls requestDecode() from within a decode
// notification. Since requestDecode() involves modifying the decoder's state
// (for example, possibly shutting down a header-only decode and starting a
// full decode), we don't want to do this from inside a decoder.
class imgDecodeRequestor : public nsRunnable
{
  public:
    imgDecodeRequestor(imgIContainer *aContainer) {
      mContainer = do_GetWeakReference(aContainer);
    }
    NS_IMETHOD Run() {
      nsCOMPtr<imgIContainer> con = do_QueryReferent(mContainer);
      if (con)
        con->RequestDecode();
      return NS_OK;
    }

  private:
    nsWeakPtr mContainer;
};

} // namespace imagelib
} // namespace mozilla

#endif /* mozilla_imagelib_RasterImage_h_ */
