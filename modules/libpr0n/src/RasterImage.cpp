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

#include "nsComponentManagerUtils.h"
#include "imgIContainerObserver.h"
#include "ImageErrors.h"
#include "Decoder.h"
#include "imgIDecoderObserver.h"
#include "RasterImage.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsAutoPtr.h"
#include "nsStringStream.h"
#include "prmem.h"
#include "prlog.h"
#include "prenv.h"
#include "nsTime.h"
#include "ImageLogging.h"

#include "nsPNGDecoder.h"
#include "nsGIFDecoder2.h"
#include "nsJPEGDecoder.h"
#include "nsBMPDecoder.h"
#include "nsICODecoder.h"
#include "nsIconDecoder.h"

#include "gfxContext.h"

using namespace mozilla::imagelib;

/* Accounting for compressed data */
#if defined(PR_LOGGING)
static PRLogModuleInfo *gCompressedImageAccountingLog = PR_NewLogModule ("CompressedImageAccounting");
#else
#define gCompressedImageAccountingLog
#endif

// Tweakable progressive decoding parameters
static PRUint32 gDecodeBytesAtATime = 200000;
static PRUint32 gMaxMSBeforeYield = 400;
static PRUint32 gMaxBytesForSyncDecode = 150000;

void
RasterImage::SetDecodeBytesAtATime(PRUint32 aBytesAtATime)
{
  gDecodeBytesAtATime = aBytesAtATime;
}
void
RasterImage::SetMaxMSBeforeYield(PRUint32 aMaxMS)
{
  gMaxMSBeforeYield = aMaxMS;
}
void
RasterImage::SetMaxBytesForSyncDecode(PRUint32 aMaxBytes)
{
  gMaxBytesForSyncDecode = aMaxBytes;
}

/* We define our own error checking macros here for 2 reasons:
 *
 * 1) Most of the failures we encounter here will (hopefully) be
 * the result of decoding failures (ie, bad data) and not code
 * failures. As such, we don't want to clutter up debug consoles
 * with spurious messages about NS_ENSURE_SUCCESS failures.
 *
 * 2) We want to set the internal error flag, shutdown properly,
 * and end up in an error state.
 *
 * So this macro should be called when the desired failure behavior
 * is to put the container into an error state and return failure.
 * It goes without saying that macro won't compile outside of a
 * non-static RasterImage method.
 */
#define LOG_CONTAINER_ERROR                      \
  PR_BEGIN_MACRO                                 \
  PR_LOG (gImgLog, PR_LOG_ERROR,                 \
          ("RasterImage: [this=%p] Error "      \
           "detected at line %u for image of "   \
           "type %s\n", this, __LINE__,          \
           mSourceDataMimeType.get()));          \
  PR_END_MACRO

#define CONTAINER_ENSURE_SUCCESS(status)      \
  PR_BEGIN_MACRO                              \
  nsresult _status = status; /* eval once */  \
  if (_status) {                              \
    LOG_CONTAINER_ERROR;                      \
    DoError();                                \
    return _status;                           \
  }                                           \
 PR_END_MACRO

#define CONTAINER_ENSURE_TRUE(arg, rv)  \
  PR_BEGIN_MACRO                        \
  if (!(arg)) {                         \
    LOG_CONTAINER_ERROR;                \
    DoError();                          \
    return rv;                          \
  }                                     \
  PR_END_MACRO



static int num_containers;
static int num_discardable_containers;
static PRInt64 total_source_bytes;
static PRInt64 discardable_source_bytes;

/* Are we globally disabling image discarding? */
static PRBool
DiscardingEnabled()
{
  static PRBool inited;
  static PRBool enabled;

  if (!inited) {
    inited = PR_TRUE;

    enabled = (PR_GetEnv("MOZ_DISABLE_IMAGE_DISCARD") == nsnull);
  }

  return enabled;
}

namespace mozilla {
namespace imagelib {

#ifndef DEBUG
NS_IMPL_ISUPPORTS4(RasterImage, imgIContainer, nsITimerCallback, nsIProperties,
                   nsISupportsWeakReference)
#else
NS_IMPL_ISUPPORTS5(RasterImage, imgIContainer, nsITimerCallback, nsIProperties,
                   imgIContainerDebug, nsISupportsWeakReference)
#endif

//******************************************************************************
RasterImage::RasterImage(imgStatusTracker* aStatusTracker) :
  Image(aStatusTracker), // invoke superclass's constructor
  mSize(0,0),
  mAnim(nsnull),
  mAnimationMode(kNormalAnimMode),
  mLoopCount(-1),
  mObserver(nsnull),
  mLockCount(0),
  mDecoder(nsnull),
  mWorker(nsnull),
  mBytesDecoded(0),
#ifdef DEBUG
  mFramesNotified(0),
#endif
  mHasSize(PR_FALSE),
  mDecodeOnDraw(PR_FALSE),
  mMultipart(PR_FALSE),
  mDiscardable(PR_FALSE),
  mHasSourceData(PR_FALSE),
  mDecoded(PR_FALSE),
  mHasBeenDecoded(PR_FALSE),
  mWorkerPending(PR_FALSE),
  mInDecoder(PR_FALSE),
  mError(PR_FALSE),
  mAnimationFinished(PR_FALSE)
{
  // Set up the discard tracker node.
  mDiscardTrackerNode.curr = this;
  mDiscardTrackerNode.prev = mDiscardTrackerNode.next = nsnull;

  // Statistics
  num_containers++;
}

//******************************************************************************
RasterImage::~RasterImage()
{
  if (mAnim)
    delete mAnim;

  for (unsigned int i = 0; i < mFrames.Length(); ++i)
    delete mFrames[i];

  // Discardable statistics
  if (mDiscardable) {
    num_discardable_containers--;
    discardable_source_bytes -= mSourceData.Length();

    PR_LOG (gCompressedImageAccountingLog, PR_LOG_DEBUG,
            ("CompressedImageAccounting: destroying RasterImage %p.  "
             "Total Containers: %d, Discardable containers: %d, "
             "Total source bytes: %lld, Source bytes for discardable containers %lld",
             this,
             num_containers,
             num_discardable_containers,
             total_source_bytes,
             discardable_source_bytes));
  }

  DiscardTracker::Remove(&mDiscardTrackerNode);

  // If we have a decoder open, shut it down
  if (mDecoder) {
    nsresult rv = ShutdownDecoder(eShutdownIntent_Interrupted);
    if (NS_FAILED(rv))
      NS_WARNING("Failed to shut down decoder in destructor!");
  }

  // Total statistics
  num_containers--;
  total_source_bytes -= mSourceData.Length();
}

nsresult
RasterImage::Init(imgIDecoderObserver *aObserver,
                  const char* aMimeType,
                  const char* aURIString,
                  PRUint32 aFlags)
{
  // We don't support re-initialization
  if (mInitialized)
    return NS_ERROR_ILLEGAL_VALUE;

  // Not sure an error can happen before init, but be safe
  if (mError)
    return NS_ERROR_FAILURE;

  NS_ENSURE_ARG_POINTER(aMimeType);

  // We must be non-discardable and non-decode-on-draw for
  // multipart channels
  NS_ABORT_IF_FALSE(!(aFlags & INIT_FLAG_MULTIPART) ||
                    (!(aFlags & INIT_FLAG_DISCARDABLE) &&
                     !(aFlags & INIT_FLAG_DECODE_ON_DRAW)),
                    "Can't be discardable or decode-on-draw for multipart");

  // Store initialization data
  mObserver = do_GetWeakReference(aObserver);
  mSourceDataMimeType.Assign(aMimeType);
  mURIString.Assign(aURIString);
  mDiscardable = !!(aFlags & INIT_FLAG_DISCARDABLE);
  mDecodeOnDraw = !!(aFlags & INIT_FLAG_DECODE_ON_DRAW);
  mMultipart = !!(aFlags & INIT_FLAG_MULTIPART);

  // Statistics
  if (mDiscardable) {
    num_discardable_containers++;
    discardable_source_bytes += mSourceData.Length();
  }

  // If we're being called from ExtractFrame (used by borderimage),
  // we don't actually do any decoding. Bail early.
  // XXX - This should be removed when we fix borderimage
  if (mSourceDataMimeType.Length() == 0) {
    mInitialized = PR_TRUE;
    return NS_OK;
  }

  // Instantiate the decoder
  //
  // If we're doing decode-on-draw, we want to do a quick first pass to get
  // the size but nothing else. We instantiate another decoder later to do
  // the full decoding.
  nsresult rv = InitDecoder(/* aDoSizeDecode = */ mDecodeOnDraw);
  CONTAINER_ENSURE_SUCCESS(rv);

  // Mark us as initialized
  mInitialized = PR_TRUE;

  return NS_OK;
}

//******************************************************************************
/* [noscript] imgIContainer extractFrame(PRUint32 aWhichFrame,
 *                                       [const] in nsIntRect aRegion,
 *                                       in PRUint32 aFlags); */
NS_IMETHODIMP
RasterImage::ExtractFrame(PRUint32 aWhichFrame,
                          const nsIntRect &aRegion,
                          PRUint32 aFlags,
                          imgIContainer **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsresult rv;

  if (aWhichFrame > FRAME_MAX_VALUE)
    return NS_ERROR_INVALID_ARG;

  if (mError)
    return NS_ERROR_FAILURE;

  // Disallowed in the API
  if (mInDecoder && (aFlags & imgIContainer::FLAG_SYNC_DECODE))
    return NS_ERROR_FAILURE;

  // Make a new container. This should switch to another class with bug 505959.
  nsRefPtr<RasterImage> img(new RasterImage());
  NS_ENSURE_TRUE(img, NS_ERROR_OUT_OF_MEMORY);

  // We don't actually have a mimetype in this case. The empty string tells the
  // init routine not to try to instantiate a decoder. This should be fixed in
  // bug 505959.
  img->Init(nsnull, "", "", INIT_FLAG_NONE);
  img->SetSize(aRegion.width, aRegion.height);
  img->mDecoded = PR_TRUE; // Also, we need to mark the image as decoded
  img->mHasBeenDecoded = PR_TRUE;

  // If a synchronous decode was requested, do it
  if (aFlags & FLAG_SYNC_DECODE) {
    rv = SyncDecode();
    CONTAINER_ENSURE_SUCCESS(rv);
  }

  // Get the frame. If it's not there, it's probably the caller's fault for
  // not waiting for the data to be loaded from the network or not passing
  // FLAG_SYNC_DECODE
  PRUint32 frameIndex = (aWhichFrame == FRAME_FIRST) ?
                        0 : GetCurrentImgFrameIndex();
  imgFrame *frame = GetDrawableImgFrame(frameIndex);
  if (!frame) {
    *_retval = nsnull;
    return NS_ERROR_FAILURE;
  }

  // The frame can be smaller than the image. We want to extract only the part
  // of the frame that actually exists.
  nsIntRect framerect = frame->GetRect();
  framerect.IntersectRect(framerect, aRegion);

  if (framerect.IsEmpty())
    return NS_ERROR_NOT_AVAILABLE;

  nsAutoPtr<imgFrame> subframe;
  rv = frame->Extract(framerect, getter_Transfers(subframe));
  if (NS_FAILED(rv))
    return rv;

  img->mFrames.AppendElement(subframe.forget());

  img->mStatusTracker->RecordLoaded();
  img->mStatusTracker->RecordDecoded();

  *_retval = img.forget().get();

  return NS_OK;
}

//******************************************************************************
/* readonly attribute PRInt32 width; */
NS_IMETHODIMP
RasterImage::GetWidth(PRInt32 *aWidth)
{
  NS_ENSURE_ARG_POINTER(aWidth);

  if (mError)
    return NS_ERROR_FAILURE;

  *aWidth = mSize.width;
  return NS_OK;
}

//******************************************************************************
/* readonly attribute PRInt32 height; */
NS_IMETHODIMP
RasterImage::GetHeight(PRInt32 *aHeight)
{
  NS_ENSURE_ARG_POINTER(aHeight);

  if (mError)
    return NS_ERROR_FAILURE;

  *aHeight = mSize.height;
  return NS_OK;
}

//******************************************************************************
/* unsigned short GetType(); */
NS_IMETHODIMP
RasterImage::GetType(PRUint16 *aType)
{
  NS_ENSURE_ARG_POINTER(aType);

  *aType = imgIContainer::TYPE_RASTER;
  return NS_OK;
}

imgFrame*
RasterImage::GetImgFrame(PRUint32 framenum)
{
  nsresult rv = WantDecodedFrames();
  CONTAINER_ENSURE_TRUE(NS_SUCCEEDED(rv), nsnull);

  if (!mAnim) {
    NS_ASSERTION(framenum == 0, "Don't ask for a frame > 0 if we're not animated!");
    return mFrames.SafeElementAt(0, nsnull);
  }
  if (mAnim->lastCompositedFrameIndex == PRInt32(framenum))
    return mAnim->compositingFrame;
  return mFrames.SafeElementAt(framenum, nsnull);
}

imgFrame*
RasterImage::GetDrawableImgFrame(PRUint32 framenum)
{
  imgFrame *frame = GetImgFrame(framenum);

  // We will return a paletted frame if it's not marked as compositing failed
  // so we can catch crashes for reasons we haven't investigated.
  if (frame && frame->GetCompositingFailed())
    return nsnull;
  return frame;
}

PRUint32
RasterImage::GetCurrentImgFrameIndex() const
{
  if (mAnim)
    return mAnim->currentAnimationFrameIndex;

  return 0;
}

imgFrame*
RasterImage::GetCurrentImgFrame()
{
  return GetImgFrame(GetCurrentImgFrameIndex());
}

imgFrame*
RasterImage::GetCurrentDrawableImgFrame()
{
  return GetDrawableImgFrame(GetCurrentImgFrameIndex());
}

//******************************************************************************
/* readonly attribute boolean currentFrameIsOpaque; */
NS_IMETHODIMP
RasterImage::GetCurrentFrameIsOpaque(PRBool *aIsOpaque)
{
  NS_ENSURE_ARG_POINTER(aIsOpaque);

  if (mError)
    return NS_ERROR_FAILURE;

  // See if we can get an image frame
  imgFrame *curframe = GetCurrentImgFrame();

  // If we don't get a frame, the safe answer is "not opaque"
  if (!curframe)
    *aIsOpaque = PR_FALSE;

  // Otherwise, we can make a more intelligent decision
  else {
    *aIsOpaque = !curframe->GetNeedsBackground();

    // We are also transparent if the current frame's size doesn't cover our
    // entire area.
    nsIntRect framerect = curframe->GetRect();
    *aIsOpaque = *aIsOpaque && (framerect != nsIntRect(0, 0, mSize.width, mSize.height));
  }

  return NS_OK;
}

void
RasterImage::GetCurrentFrameRect(nsIntRect& aRect)
{
  // Get the current frame
  imgFrame* curframe = GetCurrentImgFrame();

  // If we have the frame, use that rectangle
  if (curframe) {
    aRect = curframe->GetRect();
  } else {
    // If the frame doesn't exist, we pass the empty rectangle. It's not clear
    // whether this is appropriate in general, but at the moment the only
    // consumer of this method is imgStatusTracker (when it wants to figure out
    // dirty rectangles to send out batched observer updates). This should
    // probably be revisited when we fix bug 503973.
    aRect.MoveTo(0, 0);
    aRect.SizeTo(0, 0);
  }
}

PRUint32
RasterImage::GetCurrentFrameIndex()
{
  return GetCurrentImgFrameIndex();
}

PRUint32
RasterImage::GetNumFrames()
{
  return mFrames.Length();
}

//******************************************************************************
/* readonly attribute boolean animated; */
NS_IMETHODIMP
RasterImage::GetAnimated(PRBool *aAnimated)
{
  if (mError)
    return NS_ERROR_FAILURE;

  NS_ENSURE_ARG_POINTER(aAnimated);

  // If we have mAnim, we can know for sure
  if (mAnim) {
    *aAnimated = PR_TRUE;
    return NS_OK;
  }

  // Otherwise, we need to have been decoded to know for sure, since if we were
  // decoded at least once mAnim would have been created for animated images
  if (!mHasBeenDecoded)
    return NS_ERROR_NOT_AVAILABLE;

  // We know for sure
  *aAnimated = PR_FALSE;

  return NS_OK;
}


//******************************************************************************
/* [noscript] gfxImageSurface copyFrame(in PRUint32 aWhichFrame,
 *                                      in PRUint32 aFlags); */
NS_IMETHODIMP
RasterImage::CopyFrame(PRUint32 aWhichFrame,
                       PRUint32 aFlags,
                       gfxImageSurface **_retval)
{
  if (aWhichFrame > FRAME_MAX_VALUE)
    return NS_ERROR_INVALID_ARG;

  if (mError)
    return NS_ERROR_FAILURE;

  // Disallowed in the API
  if (mInDecoder && (aFlags & imgIContainer::FLAG_SYNC_DECODE))
    return NS_ERROR_FAILURE;

  nsresult rv;

  // If requested, synchronously flush any data we have lying around to the decoder
  if (aFlags & FLAG_SYNC_DECODE) {
    rv = SyncDecode();
    CONTAINER_ENSURE_SUCCESS(rv);
  }

  NS_ENSURE_ARG_POINTER(_retval);

  // Get the frame. If it's not there, it's probably the caller's fault for
  // not waiting for the data to be loaded from the network or not passing
  // FLAG_SYNC_DECODE
  PRUint32 frameIndex = (aWhichFrame == FRAME_FIRST) ?
                        0 : GetCurrentImgFrameIndex();
  imgFrame *frame = GetDrawableImgFrame(frameIndex);
  if (!frame) {
    *_retval = nsnull;
    return NS_ERROR_FAILURE;
  }

  nsRefPtr<gfxPattern> pattern;
  frame->GetPattern(getter_AddRefs(pattern));
  nsIntRect intframerect = frame->GetRect();
  gfxRect framerect(intframerect.x, intframerect.y, intframerect.width, intframerect.height);

  // Create a 32-bit image surface of our size, but draw using the frame's
  // rect, implicitly padding the frame out to the image's size.
  nsRefPtr<gfxImageSurface> imgsurface = new gfxImageSurface(gfxIntSize(mSize.width, mSize.height),
                                                             gfxASurface::ImageFormatARGB32);
  gfxContext ctx(imgsurface);
  ctx.SetOperator(gfxContext::OPERATOR_SOURCE);
  ctx.SetPattern(pattern);
  ctx.Rectangle(framerect);
  ctx.Fill();

  *_retval = imgsurface.forget().get();
  return NS_OK;
}

//******************************************************************************
/* [noscript] gfxASurface getFrame(in PRUint32 aWhichFrame,
 *                                 in PRUint32 aFlags); */
NS_IMETHODIMP
RasterImage::GetFrame(PRUint32 aWhichFrame,
                      PRUint32 aFlags,
                      gfxASurface **_retval)
{
  if (aWhichFrame > FRAME_MAX_VALUE)
    return NS_ERROR_INVALID_ARG;

  if (mError)
    return NS_ERROR_FAILURE;

  // Disallowed in the API
  if (mInDecoder && (aFlags & imgIContainer::FLAG_SYNC_DECODE))
    return NS_ERROR_FAILURE;

  nsresult rv = NS_OK;

  // If the caller requested a synchronous decode, do it
  if (aFlags & FLAG_SYNC_DECODE) {
    rv = SyncDecode();
    CONTAINER_ENSURE_SUCCESS(rv);
  }

  // Get the frame. If it's not there, it's probably the caller's fault for
  // not waiting for the data to be loaded from the network or not passing
  // FLAG_SYNC_DECODE
  PRUint32 frameIndex = (aWhichFrame == FRAME_FIRST) ?
                          0 : GetCurrentImgFrameIndex();
  imgFrame *frame = GetDrawableImgFrame(frameIndex);
  if (!frame) {
    *_retval = nsnull;
    return NS_ERROR_FAILURE;
  }

  nsRefPtr<gfxASurface> framesurf;

  // If this frame covers the entire image, we can just reuse its existing
  // surface.
  nsIntRect framerect = frame->GetRect();
  if (framerect.x == 0 && framerect.y == 0 &&
      framerect.width == mSize.width &&
      framerect.height == mSize.height)
    rv = frame->GetSurface(getter_AddRefs(framesurf));

  // The image doesn't have a surface because it's been optimized away. Create
  // one.
  if (!framesurf) {
    nsRefPtr<gfxImageSurface> imgsurf;
    rv = CopyFrame(aWhichFrame, aFlags, getter_AddRefs(imgsurf));
    framesurf = imgsurf;
  }

  *_retval = framesurf.forget().get();

  return rv;
}

PRUint32
RasterImage::GetDataSize()
{
  if (mError)
    return 0;

  // Start with 0
  PRUint32 size = 0;

  // Account for any compressed source data
  size += GetSourceDataSize();
  NS_ABORT_IF_FALSE(StoringSourceData() || (size == 0),
                    "Non-zero source data size when we aren't storing it?");

  // Account for any uncompressed frames
  size += GetDecodedDataSize();

  return size;
}

PRUint32
RasterImage::GetDecodedDataSize()
{
  PRUint32 val = 0;
  for (PRUint32 i = 0; i < mFrames.Length(); ++i) {
    imgFrame *frame = mFrames.SafeElementAt(i, nsnull);
    NS_ABORT_IF_FALSE(frame, "Null frame in frame array!");
    val += frame->EstimateMemoryUsed();
  }

  return val;
}

PRUint32
RasterImage::GetSourceDataSize()
{
  return mSourceData.Length();
}

void
RasterImage::DeleteImgFrame(PRUint32 framenum)
{
  NS_ABORT_IF_FALSE(framenum < mFrames.Length(), "Deleting invalid frame!");

  delete mFrames[framenum];
  mFrames[framenum] = nsnull;
}

nsresult
RasterImage::InternalAddFrameHelper(PRUint32 framenum, imgFrame *aFrame,
                                    PRUint8 **imageData, PRUint32 *imageLength,
                                    PRUint32 **paletteData, PRUint32 *paletteLength)
{
  NS_ABORT_IF_FALSE(framenum <= mFrames.Length(), "Invalid frame index!");
  if (framenum > mFrames.Length())
    return NS_ERROR_INVALID_ARG;

  nsAutoPtr<imgFrame> frame(aFrame);

  if (paletteData && paletteLength)
    frame->GetPaletteData(paletteData, paletteLength);

  frame->GetImageData(imageData, imageLength);

  // We are in the middle of decoding. This will be unlocked when we finish the
  // decoder->Write() call.
  frame->LockImageData();

  mFrames.InsertElementAt(framenum, frame.forget());

  return NS_OK;
}
                                  
nsresult
RasterImage::InternalAddFrame(PRUint32 framenum,
                              PRInt32 aX, PRInt32 aY,
                              PRInt32 aWidth, PRInt32 aHeight,
                              gfxASurface::gfxImageFormat aFormat,
                              PRUint8 aPaletteDepth,
                              PRUint8 **imageData,
                              PRUint32 *imageLength,
                              PRUint32 **paletteData,
                              PRUint32 *paletteLength)
{
  // We assume that we're in the middle of decoding because we unlock the
  // previous frame when we create a new frame, and only when decoding do we
  // lock frames.
  NS_ABORT_IF_FALSE(mInDecoder, "Only decoders may add frames!");

  NS_ABORT_IF_FALSE(framenum <= mFrames.Length(), "Invalid frame index!");
  if (framenum > mFrames.Length())
    return NS_ERROR_INVALID_ARG;

  nsAutoPtr<imgFrame> frame(new imgFrame());
  NS_ENSURE_TRUE(frame, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = frame->Init(aX, aY, aWidth, aHeight, aFormat, aPaletteDepth);
  NS_ENSURE_SUCCESS(rv, rv);

  // We know we are in a decoder. Therefore, we must unlock the previous frame
  // when we move on to decoding into the next frame.
  if (mFrames.Length() > 0) {
    imgFrame *prevframe = mFrames.ElementAt(mFrames.Length() - 1);
    prevframe->UnlockImageData();
  }

  if (mFrames.Length() == 0) {
    return InternalAddFrameHelper(framenum, frame.forget(), imageData, imageLength, 
                                  paletteData, paletteLength);
  }

  if (mFrames.Length() == 1) {
    // Since we're about to add our second frame, initialize animation stuff
    if (!ensureAnimExists())
      return NS_ERROR_OUT_OF_MEMORY;
    
    // If we dispose of the first frame by clearing it, then the
    // First Frame's refresh area is all of itself.
    // RESTORE_PREVIOUS is invalid (assumed to be DISPOSE_CLEAR)
    PRInt32 frameDisposalMethod = mFrames[0]->GetFrameDisposalMethod();
    if (frameDisposalMethod == kDisposeClear ||
        frameDisposalMethod == kDisposeRestorePrevious)
      mAnim->firstFrameRefreshArea = mFrames[0]->GetRect();
  }

  // Calculate firstFrameRefreshArea
  // Some gifs are huge but only have a small area that they animate
  // We only need to refresh that small area when Frame 0 comes around again
  nsIntRect frameRect = frame->GetRect();
  mAnim->firstFrameRefreshArea.UnionRect(mAnim->firstFrameRefreshArea, 
                                         frameRect);
  
  rv = InternalAddFrameHelper(framenum, frame.forget(), imageData, imageLength,
                              paletteData, paletteLength);
  
  // We may be able to start animating, if we now have enough frames
  EvaluateAnimation();
  
  return rv;
}

nsresult
RasterImage::AppendFrame(PRInt32 aX, PRInt32 aY, PRInt32 aWidth,
                         PRInt32 aHeight,
                         gfxASurface::gfxImageFormat aFormat,
                         PRUint8 **imageData,
                         PRUint32 *imageLength)
{
  if (mError)
    return NS_ERROR_FAILURE;

  NS_ENSURE_ARG_POINTER(imageData);
  NS_ENSURE_ARG_POINTER(imageLength);

  return InternalAddFrame(mFrames.Length(), aX, aY, aWidth, aHeight, aFormat, 
                          /* aPaletteDepth = */ 0, imageData, imageLength,
                          /* aPaletteData = */ nsnull, 
                          /* aPaletteLength = */ nsnull);
}

nsresult
RasterImage::AppendPalettedFrame(PRInt32 aX, PRInt32 aY,
                                 PRInt32 aWidth, PRInt32 aHeight,
                                 gfxASurface::gfxImageFormat aFormat,
                                 PRUint8 aPaletteDepth,
                                 PRUint8 **imageData,
                                 PRUint32 *imageLength,
                                 PRUint32 **paletteData,
                                 PRUint32 *paletteLength)
{
  if (mError)
    return NS_ERROR_FAILURE;

  NS_ENSURE_ARG_POINTER(imageData);
  NS_ENSURE_ARG_POINTER(imageLength);
  NS_ENSURE_ARG_POINTER(paletteData);
  NS_ENSURE_ARG_POINTER(paletteLength);

  return InternalAddFrame(mFrames.Length(), aX, aY, aWidth, aHeight, aFormat, 
                          aPaletteDepth, imageData, imageLength,
                          paletteData, paletteLength);
}

nsresult
RasterImage::SetSize(PRInt32 aWidth, PRInt32 aHeight)
{
  if (mError)
    return NS_ERROR_FAILURE;

  // Ensure that we have positive values
  // XXX - Why isn't the size unsigned? Should this be changed?
  if ((aWidth < 0) || (aHeight < 0))
    return NS_ERROR_INVALID_ARG;

  // if we already have a size, check the new size against the old one
  if (mHasSize &&
      ((aWidth != mSize.width) || (aHeight != mSize.height))) {

    // Alter the warning depending on whether the channel is multipart
    if (!mMultipart)
      NS_WARNING("Image changed size on redecode! This should not happen!");
    else
      NS_WARNING("Multipart channel sent an image of a different size");

    DoError();
    return NS_ERROR_UNEXPECTED;
  }

  // Set the size and flag that we have it
  mSize.SizeTo(aWidth, aHeight);
  mHasSize = PR_TRUE;

  return NS_OK;
}

nsresult
RasterImage::EnsureCleanFrame(PRUint32 aFrameNum, PRInt32 aX, PRInt32 aY,
                              PRInt32 aWidth, PRInt32 aHeight,
                              gfxASurface::gfxImageFormat aFormat,
                              PRUint8 **imageData, PRUint32 *imageLength)
{
  if (mError)
    return NS_ERROR_FAILURE;

  NS_ENSURE_ARG_POINTER(imageData);
  NS_ENSURE_ARG_POINTER(imageLength);
  NS_ABORT_IF_FALSE(aFrameNum <= mFrames.Length(), "Invalid frame index!");
  if (aFrameNum > mFrames.Length())
    return NS_ERROR_INVALID_ARG;

  // Adding a frame that doesn't already exist.
  if (aFrameNum == mFrames.Length())
    return InternalAddFrame(aFrameNum, aX, aY, aWidth, aHeight, aFormat, 
                            /* aPaletteDepth = */ 0, imageData, imageLength,
                            /* aPaletteData = */ nsnull, 
                            /* aPaletteLength = */ nsnull);

  imgFrame *frame = GetImgFrame(aFrameNum);
  if (!frame)
    return InternalAddFrame(aFrameNum, aX, aY, aWidth, aHeight, aFormat, 
                            /* aPaletteDepth = */ 0, imageData, imageLength,
                            /* aPaletteData = */ nsnull, 
                            /* aPaletteLength = */ nsnull);

  // See if we can re-use the frame that already exists.
  nsIntRect rect = frame->GetRect();
  if (rect.x != aX || rect.y != aY || rect.width != aWidth || rect.height != aHeight ||
      frame->GetFormat() != aFormat) {
    DeleteImgFrame(aFrameNum);
    return InternalAddFrame(aFrameNum, aX, aY, aWidth, aHeight, aFormat, 
                            /* aPaletteDepth = */ 0, imageData, imageLength,
                            /* aPaletteData = */ nsnull, 
                            /* aPaletteLength = */ nsnull);
  }

  // We can re-use the frame.
  frame->GetImageData(imageData, imageLength);

  return NS_OK;
}


void
RasterImage::FrameUpdated(PRUint32 aFrameNum, nsIntRect &aUpdatedRect)
{
  NS_ABORT_IF_FALSE(aFrameNum < mFrames.Length(), "Invalid frame index!");

  imgFrame *frame = GetImgFrame(aFrameNum);
  NS_ABORT_IF_FALSE(frame, "Calling FrameUpdated on frame that doesn't exist!");

  frame->ImageUpdated(aUpdatedRect);
}

nsresult
RasterImage::SetFrameDisposalMethod(PRUint32 aFrameNum,
                                    PRInt32 aDisposalMethod)
{
  if (mError)
    return NS_ERROR_FAILURE;

  NS_ABORT_IF_FALSE(aFrameNum < mFrames.Length(), "Invalid frame index!");
  if (aFrameNum >= mFrames.Length())
    return NS_ERROR_INVALID_ARG;

  imgFrame *frame = GetImgFrame(aFrameNum);
  NS_ABORT_IF_FALSE(frame,
                    "Calling SetFrameDisposalMethod on frame that doesn't exist!");
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);

  frame->SetFrameDisposalMethod(aDisposalMethod);

  return NS_OK;
}

nsresult
RasterImage::SetFrameTimeout(PRUint32 aFrameNum, PRInt32 aTimeout)
{
  if (mError)
    return NS_ERROR_FAILURE;

  NS_ABORT_IF_FALSE(aFrameNum < mFrames.Length(), "Invalid frame index!");
  if (aFrameNum >= mFrames.Length())
    return NS_ERROR_INVALID_ARG;

  imgFrame *frame = GetImgFrame(aFrameNum);
  NS_ABORT_IF_FALSE(frame, "Calling SetFrameTimeout on frame that doesn't exist!");
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);

  frame->SetTimeout(aTimeout);

  return NS_OK;
}

nsresult
RasterImage::SetFrameBlendMethod(PRUint32 aFrameNum, PRInt32 aBlendMethod)
{
  if (mError)
    return NS_ERROR_FAILURE;

  NS_ABORT_IF_FALSE(aFrameNum < mFrames.Length(), "Invalid frame index!");
  if (aFrameNum >= mFrames.Length())
    return NS_ERROR_INVALID_ARG;

  imgFrame *frame = GetImgFrame(aFrameNum);
  NS_ABORT_IF_FALSE(frame, "Calling SetFrameBlendMethod on frame that doesn't exist!");
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);

  frame->SetBlendMethod(aBlendMethod);

  return NS_OK;
}

nsresult
RasterImage::SetFrameHasNoAlpha(PRUint32 aFrameNum)
{
  if (mError)
    return NS_ERROR_FAILURE;

  NS_ABORT_IF_FALSE(aFrameNum < mFrames.Length(), "Invalid frame index!");
  if (aFrameNum >= mFrames.Length())
    return NS_ERROR_INVALID_ARG;

  imgFrame *frame = GetImgFrame(aFrameNum);
  NS_ABORT_IF_FALSE(frame, "Calling SetFrameHasNoAlpha on frame that doesn't exist!");
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);

  frame->SetHasNoAlpha();

  return NS_OK;
}

nsresult
RasterImage::DecodingComplete()
{
  if (mError)
    return NS_ERROR_FAILURE;

  // Flag that we're done decoding.
  // XXX - these should probably be combined when we fix animated image
  // discarding with bug 500402.
  mDecoded = PR_TRUE;
  mHasBeenDecoded = PR_TRUE;

  nsresult rv;

  // We now have one of the qualifications for discarding. Re-evaluate.
  if (CanDiscard()) {
    NS_ABORT_IF_FALSE(!DiscardingActive(),
                      "We shouldn't have been discardable before this");
    rv = DiscardTracker::Reset(&mDiscardTrackerNode);
    CONTAINER_ENSURE_SUCCESS(rv);
  }

  // If there's only 1 frame, optimize it. Optimizing animated images
  // is not supported.
  //
  // We don't optimize the frame for multipart images because we reuse
  // the frame.
  if ((mFrames.Length() == 1) && !mMultipart) {
    rv = mFrames[0]->Optimize();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

//******************************************************************************
/* attribute unsigned short animationMode; */
NS_IMETHODIMP
RasterImage::GetAnimationMode(PRUint16 *aAnimationMode)
{
  if (mError)
    return NS_ERROR_FAILURE;

  NS_ENSURE_ARG_POINTER(aAnimationMode);
  
  *aAnimationMode = mAnimationMode;
  return NS_OK;
}

//******************************************************************************
/* attribute unsigned short animationMode; */
NS_IMETHODIMP
RasterImage::SetAnimationMode(PRUint16 aAnimationMode)
{
  if (mError)
    return NS_ERROR_FAILURE;

  NS_ASSERTION(aAnimationMode == kNormalAnimMode ||
               aAnimationMode == kDontAnimMode ||
               aAnimationMode == kLoopOnceAnimMode,
               "Wrong Animation Mode is being set!");
  
  mAnimationMode = aAnimationMode;

  EvaluateAnimation();

  return NS_OK;
}

//******************************************************************************
/* void StartAnimation () */
nsresult
RasterImage::StartAnimation()
{
  if (mError)
    return NS_ERROR_FAILURE;

  NS_ABORT_IF_FALSE(ShouldAnimate(), "Should not animate!");

  if (!ensureAnimExists())
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ABORT_IF_FALSE(mAnim && !mAnim->timer, "Anim must exist and not have a timer yet");
  
  // Default timeout to 100: the timer notify code will do the right
  // thing, so just get that started.
  PRInt32 timeout = 100;
  imgFrame *currentFrame = GetCurrentImgFrame();
  if (currentFrame) {
    timeout = currentFrame->GetTimeout();
    if (timeout < 0) { // -1 means display this frame forever
      mAnimationFinished = PR_TRUE;
      return NS_ERROR_ABORT;
    }
  }
  
  mAnim->timer = do_CreateInstance("@mozilla.org/timer;1");
  NS_ENSURE_TRUE(mAnim->timer, NS_ERROR_OUT_OF_MEMORY);
  mAnim->timer->InitWithCallback(static_cast<nsITimerCallback*>(this),
                                 timeout, nsITimer::TYPE_REPEATING_SLACK);
  
  return NS_OK;
}

//******************************************************************************
/* void stopAnimation (); */
nsresult
RasterImage::StopAnimation()
{
  NS_ABORT_IF_FALSE(mAnimating, "Should be animating!");

  if (mError)
    return NS_ERROR_FAILURE;

  if (mAnim->timer) {
    mAnim->timer->Cancel();
    mAnim->timer = nsnull;
  }

  return NS_OK;
}

//******************************************************************************
/* void resetAnimation (); */
NS_IMETHODIMP
RasterImage::ResetAnimation()
{
  if (mError)
    return NS_ERROR_FAILURE;

  if (mAnimationMode == kDontAnimMode || 
      !mAnim || mAnim->currentAnimationFrameIndex == 0)
    return NS_OK;

  if (mAnimating)
    StopAnimation();

  mAnim->lastCompositedFrameIndex = -1;
  mAnim->currentAnimationFrameIndex = 0;

  // Note - We probably want to kick off a redecode somewhere around here when
  // we fix bug 500402.

  // Update display if we were animating before
  nsCOMPtr<imgIContainerObserver> observer(do_QueryReferent(mObserver));
  if (mAnimating && observer)
    observer->FrameChanged(this, &(mAnim->firstFrameRefreshArea));

  if (ShouldAnimate())
    StartAnimation();

  return NS_OK;
}

void
RasterImage::SetLoopCount(PRInt32 aLoopCount)
{
  if (mError)
    return;

  // -1  infinite
  //  0  no looping, one iteration
  //  1  one loop, two iterations
  //  ...
  mLoopCount = aLoopCount;
}

nsresult
RasterImage::AddSourceData(const char *aBuffer, PRUint32 aCount)
{
  if (mError)
    return NS_ERROR_FAILURE;

  NS_ENSURE_ARG_POINTER(aBuffer);
  nsresult rv = NS_OK;

  // We should not call this if we're not initialized
  NS_ABORT_IF_FALSE(mInitialized, "Calling AddSourceData() on uninitialized "
                                  "RasterImage!");

  // We should not call this if we're already finished adding source data
  NS_ABORT_IF_FALSE(!mHasSourceData, "Calling AddSourceData() after calling "
                                     "sourceDataComplete()!");

  // This call should come straight from necko - no reentrancy allowed
  NS_ABORT_IF_FALSE(!mInDecoder, "Re-entrant call to AddSourceData!");

  // If we're not storing source data, write it directly to the decoder
  if (!StoringSourceData()) {
    rv = WriteToDecoder(aBuffer, aCount);
    CONTAINER_ENSURE_SUCCESS(rv);

    // We're not storing source data, so this data is probably coming straight
    // from the network. In this case, we want to display data as soon as we
    // get it, so we want to flush invalidations after every write.
    mInDecoder = PR_TRUE;
    mDecoder->FlushInvalidations();
    mInDecoder = PR_FALSE;
  }

  // Otherwise, we're storing data in the source buffer
  else {

    // Store the data
    char *newElem = mSourceData.AppendElements(aBuffer, aCount);
    if (!newElem)
      return NS_ERROR_OUT_OF_MEMORY;

    // If there's a decoder open, that means we want to do more decoding.
    // Wake up the worker if it's not up already
    if (mDecoder && !mWorkerPending) {
      NS_ABORT_IF_FALSE(mWorker, "We should have a worker here!");
      rv = mWorker->Run();
      CONTAINER_ENSURE_SUCCESS(rv);
    }
  }

  // Statistics
  total_source_bytes += aCount;
  if (mDiscardable)
    discardable_source_bytes += aCount;
  PR_LOG (gCompressedImageAccountingLog, PR_LOG_DEBUG,
          ("CompressedImageAccounting: Added compressed data to RasterImage %p (%s). "
           "Total Containers: %d, Discardable containers: %d, "
           "Total source bytes: %lld, Source bytes for discardable containers %lld",
           this,
           mSourceDataMimeType.get(),
           num_containers,
           num_discardable_containers,
           total_source_bytes,
           discardable_source_bytes));

  return NS_OK;
}

/* Note!  buf must be declared as char buf[9]; */
// just used for logging and hashing the header
static void
get_header_str (char *buf, char *data, PRSize data_len)
{
  int i;
  int n;
  static char hex[] = "0123456789abcdef";

  n = data_len < 4 ? data_len : 4;

  for (i = 0; i < n; i++) {
    buf[i * 2]     = hex[(data[i] >> 4) & 0x0f];
    buf[i * 2 + 1] = hex[data[i] & 0x0f];
  }

  buf[i * 2] = 0;
}

nsresult
RasterImage::SourceDataComplete()
{
  if (mError)
    return NS_ERROR_FAILURE;

  // If we've been called before, ignore. Otherwise, flag that we have everything
  if (mHasSourceData)
    return NS_OK;
  mHasSourceData = PR_TRUE;

  // This call should come straight from necko - no reentrancy allowed
  NS_ABORT_IF_FALSE(!mInDecoder, "Re-entrant call to AddSourceData!");

  // If we're not storing any source data, then all the data was written
  // directly to the decoder in the AddSourceData() calls. This means we're
  // done, so we can shut down the decoder.
  if (!StoringSourceData()) {
    nsresult rv = ShutdownDecoder(eShutdownIntent_Done);
    CONTAINER_ENSURE_SUCCESS(rv);
  }

  // If there's a decoder open, we need to wake up the worker if it's not
  // already. This is so the worker can account for the fact that the source
  // data is complete. For some decoders, DecodingComplete() is only called
  // when the decoder is Close()-ed, and thus the SourceDataComplete() call
  // is the only way we can transition to a 'decoded' state. Furthermore,
  // it's always possible for any image type to have the data stream stop
  // abruptly at any point, in which case we need to trigger an error.
  if (mDecoder && !mWorkerPending) {
    NS_ABORT_IF_FALSE(mWorker, "We should have a worker here!");
    nsresult rv = mWorker->Run();
    CONTAINER_ENSURE_SUCCESS(rv);
  }

  // Free up any extra space in the backing buffer
  mSourceData.Compact();

  // Log header information
  if (PR_LOG_TEST(gCompressedImageAccountingLog, PR_LOG_DEBUG)) {
    char buf[9];
    get_header_str(buf, mSourceData.Elements(), mSourceData.Length());
    PR_LOG (gCompressedImageAccountingLog, PR_LOG_DEBUG,
            ("CompressedImageAccounting: RasterImage::SourceDataComplete() - data "
             "is done for container %p (%s) - header %p is 0x%s (length %d)",
             this,
             mSourceDataMimeType.get(),
             mSourceData.Elements(),
             buf,
             mSourceData.Length()));
  }

  // We now have one of the qualifications for discarding. Re-evaluate.
  if (CanDiscard()) {
    nsresult rv = DiscardTracker::Reset(&mDiscardTrackerNode);
    CONTAINER_ENSURE_SUCCESS(rv);
  }
  return NS_OK;
}

nsresult
RasterImage::NewSourceData()
{
  nsresult rv;

  if (mError)
    return NS_ERROR_FAILURE;

  // The source data should be complete before calling this
  NS_ABORT_IF_FALSE(mHasSourceData,
                    "Calling NewSourceData before SourceDataComplete!");
  if (!mHasSourceData)
    return NS_ERROR_ILLEGAL_VALUE;

  // Only supported for multipart channels. It wouldn't be too hard to change this,
  // but it would involve making sure that things worked for decode-on-draw and
  // discarding. Presently there's no need for this, so we don't.
  NS_ABORT_IF_FALSE(mMultipart, "NewSourceData not supported for multipart");
  if (!mMultipart)
    return NS_ERROR_ILLEGAL_VALUE;

  // We're multipart, so we shouldn't be storing source data
  NS_ABORT_IF_FALSE(!StoringSourceData(),
                    "Shouldn't be storing source data for multipart");

  // We're not storing the source data and we got SourceDataComplete. We should
  // have shut down the previous decoder
  NS_ABORT_IF_FALSE(!mDecoder, "Shouldn't have a decoder in NewSourceData");

  // The decoder was shut down and we didn't flag an error, so we should be decoded
  NS_ABORT_IF_FALSE(mDecoded, "Should be decoded in NewSourceData");

  // Reset some flags
  mDecoded = PR_FALSE;
  mHasSourceData = PR_FALSE;

  // We're decode-on-load here. Open up a new decoder just like what happens when
  // we call Init() for decode-on-load images.
  rv = InitDecoder(/* aDoSizeDecode = */ false);
  CONTAINER_ENSURE_SUCCESS(rv);

  return NS_OK;
}

nsresult
RasterImage::SetSourceSizeHint(PRUint32 sizeHint)
{
  if (sizeHint && StoringSourceData())
    mSourceData.SetCapacity(sizeHint);
  return NS_OK;
}

//******************************************************************************
/* void notify(in nsITimer timer); */
NS_IMETHODIMP
RasterImage::Notify(nsITimer *timer)
{
#ifdef DEBUG
  mFramesNotified++;
#endif

  // This should never happen since the timer is only set up in StartAnimation()
  // after mAnim is checked to exist.
  NS_ABORT_IF_FALSE(mAnim, "Need anim for Notify()");
  NS_ABORT_IF_FALSE(timer, "Need timer for Notify()");
  NS_ABORT_IF_FALSE(mAnim->timer == timer,
                    "RasterImage::Notify() called with incorrect timer");

  if (!mAnimating || !ShouldAnimate())
    return NS_OK;

  nsCOMPtr<imgIContainerObserver> observer(do_QueryReferent(mObserver));
  if (!observer) {
    // the imgRequest that owns us is dead, we should die now too.
    NS_ABORT_IF_FALSE(mAnimationConsumers == 0,
                      "If no observer, should have no consumers");
    if (mAnimating)
      StopAnimation();
    return NS_OK;
  }

  if (mFrames.Length() == 0)
    return NS_OK;
  
  imgFrame *nextFrame = nsnull;
  PRInt32 previousFrameIndex = mAnim->currentAnimationFrameIndex;
  PRUint32 nextFrameIndex = mAnim->currentAnimationFrameIndex + 1;
  PRInt32 timeout = 0;

  // Figure out if we have the next full frame. This is more complicated than
  // just checking for mFrames.Length() because decoders append their frames
  // before they're filled in.
  NS_ABORT_IF_FALSE(mDecoder || nextFrameIndex <= mFrames.Length(),
                    "How did we get 2 indicies too far by incrementing?");
  bool haveFullNextFrame = !mDecoder || nextFrameIndex < mDecoder->GetCompleteFrameCount();

  // If we don't have the next full frame, it had better be in the pipe.
  NS_ABORT_IF_FALSE(haveFullNextFrame ||
                    (mDecoder && mFrames.Length() > mDecoder->GetCompleteFrameCount()),
                    "What is the next frame supposed to be?");

  // If we're done decoding the next frame, go ahead and display it now and
  // reinit the timer with the next frame's delay time.
  if (haveFullNextFrame) {
    if (mFrames.Length() == nextFrameIndex) {
      // End of Animation

      // If animation mode is "loop once", it's time to stop animating
      if (mAnimationMode == kLoopOnceAnimMode || mLoopCount == 0) {
        mAnimationFinished = PR_TRUE;
        EvaluateAnimation();
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
    timeout = nextFrame->GetTimeout();

  } else {
    // Uh oh, the frame we want to show is currently being decoded (partial)
    // Wait a bit and try again
    mAnim->timer->SetDelay(100);
    return NS_OK;
  }

  if (timeout > 0)
    mAnim->timer->SetDelay(timeout);
  else {
    mAnimationFinished = PR_TRUE;
    EvaluateAnimation();
  }

  nsIntRect dirtyRect;
  imgFrame *frameToUse = nsnull;

  if (nextFrameIndex == 0) {
    frameToUse = nextFrame;
    dirtyRect = mAnim->firstFrameRefreshArea;
  } else {
    imgFrame *prevFrame = mFrames[previousFrameIndex];
    if (!prevFrame)
      return NS_OK;

    // Change frame and announce it
    if (NS_FAILED(DoComposite(&frameToUse, &dirtyRect, prevFrame,
                              nextFrame, nextFrameIndex))) {
      // something went wrong, move on to next
      NS_WARNING("RasterImage::Notify(): Composing Frame Failed\n");
      nextFrame->SetCompositingFailed(PR_TRUE);
      mAnim->currentAnimationFrameIndex = nextFrameIndex;
      return NS_OK;
    } else {
      nextFrame->SetCompositingFailed(PR_FALSE);
    }
  }
  // Set currentAnimationFrameIndex at the last possible moment
  mAnim->currentAnimationFrameIndex = nextFrameIndex;
  // Refreshes the screen
  observer->FrameChanged(this, &dirtyRect);
  
  return NS_OK;
}

//******************************************************************************
// DoComposite gets called when the timer for animation get fired and we have to
// update the composited frame of the animation.
nsresult
RasterImage::DoComposite(imgFrame** aFrameToUse,
                         nsIntRect* aDirtyRect,
                         imgFrame* aPrevFrame,
                         imgFrame* aNextFrame,
                         PRInt32 aNextFrameIndex)
{
  NS_ENSURE_ARG_POINTER(aDirtyRect);
  NS_ENSURE_ARG_POINTER(aPrevFrame);
  NS_ENSURE_ARG_POINTER(aNextFrame);
  NS_ENSURE_ARG_POINTER(aFrameToUse);

  PRInt32 prevFrameDisposalMethod = aPrevFrame->GetFrameDisposalMethod();
  if (prevFrameDisposalMethod == kDisposeRestorePrevious &&
      !mAnim->compositingPrevFrame)
    prevFrameDisposalMethod = kDisposeClear;

  nsIntRect prevFrameRect = aPrevFrame->GetRect();
  PRBool isFullPrevFrame = (prevFrameRect.x == 0 && prevFrameRect.y == 0 &&
                            prevFrameRect.width == mSize.width &&
                            prevFrameRect.height == mSize.height);

  // Optimization: DisposeClearAll if the previous frame is the same size as
  //               container and it's clearing itself
  if (isFullPrevFrame && 
      (prevFrameDisposalMethod == kDisposeClear))
    prevFrameDisposalMethod = kDisposeClearAll;

  PRInt32 nextFrameDisposalMethod = aNextFrame->GetFrameDisposalMethod();
  nsIntRect nextFrameRect = aNextFrame->GetRect();
  PRBool isFullNextFrame = (nextFrameRect.x == 0 && nextFrameRect.y == 0 &&
                            nextFrameRect.width == mSize.width &&
                            nextFrameRect.height == mSize.height);

  if (!aNextFrame->GetIsPaletted()) {
    // Optimization: Skip compositing if the previous frame wants to clear the
    //               whole image
    if (prevFrameDisposalMethod == kDisposeClearAll) {
      aDirtyRect->SetRect(0, 0, mSize.width, mSize.height);
      *aFrameToUse = aNextFrame;
      return NS_OK;
    }
  
    // Optimization: Skip compositing if this frame is the same size as the
    //               container and it's fully drawing over prev frame (no alpha)
    if (isFullNextFrame &&
        (nextFrameDisposalMethod != kDisposeRestorePrevious) &&
        !aNextFrame->GetHasAlpha()) {
      aDirtyRect->SetRect(0, 0, mSize.width, mSize.height);
      *aFrameToUse = aNextFrame;
      return NS_OK;
    }
  }

  // Calculate area that needs updating
  switch (prevFrameDisposalMethod) {
    default:
    case kDisposeNotSpecified:
    case kDisposeKeep:
      *aDirtyRect = nextFrameRect;
      break;

    case kDisposeClearAll:
      // Whole image container is cleared
      aDirtyRect->SetRect(0, 0, mSize.width, mSize.height);
      break;

    case kDisposeClear:
      // Calc area that needs to be redrawn (the combination of previous and
      // this frame)
      // XXX - This could be done with multiple framechanged calls
      //       Having prevFrame way at the top of the image, and nextFrame
      //       way at the bottom, and both frames being small, we'd be
      //       telling framechanged to refresh the whole image when only two
      //       small areas are needed.
      aDirtyRect->UnionRect(nextFrameRect, prevFrameRect);
      break;

    case kDisposeRestorePrevious:
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
    mAnim->compositingFrame = new imgFrame();
    if (!mAnim->compositingFrame) {
      NS_WARNING("Failed to init compositingFrame!\n");
      return NS_ERROR_OUT_OF_MEMORY;
    }
    nsresult rv = mAnim->compositingFrame->Init(0, 0, mSize.width, mSize.height,
                                                gfxASurface::ImageFormatARGB32);
    if (NS_FAILED(rv)) {
      mAnim->compositingFrame = nsnull;
      return rv;
    }
    needToBlankComposite = PR_TRUE;
  } else if (aNextFrameIndex != mAnim->lastCompositedFrameIndex+1) {

    // If we are not drawing on top of last composited frame, 
    // then we are building a new composite frame, so let's clear it first.
    needToBlankComposite = PR_TRUE;
  }

  // More optimizations possible when next frame is not transparent
  // But if the next frame has kDisposeRestorePrevious,
  // this "no disposal" optimization is not possible, 
  // because the frame in "after disposal operation" state 
  // needs to be stored in compositingFrame, so it can be 
  // copied into compositingPrevFrame later.
  PRBool doDisposal = PR_TRUE;
  if (!aNextFrame->GetHasAlpha() &&
      nextFrameDisposalMethod != kDisposeRestorePrevious) {
    if (isFullNextFrame) {
      // Optimization: No need to dispose prev.frame when 
      // next frame is full frame and not transparent.
      doDisposal = PR_FALSE;
      // No need to blank the composite frame
      needToBlankComposite = PR_FALSE;
    } else {
      if ((prevFrameRect.x >= nextFrameRect.x) &&
          (prevFrameRect.y >= nextFrameRect.y) &&
          (prevFrameRect.x + prevFrameRect.width <= nextFrameRect.x + nextFrameRect.width) &&
          (prevFrameRect.y + prevFrameRect.height <= nextFrameRect.y + nextFrameRect.height)) {
        // Optimization: No need to dispose prev.frame when 
        // next frame fully overlaps previous frame.
        doDisposal = PR_FALSE;  
      }
    }      
  }

  if (doDisposal) {
    // Dispose of previous: clear, restore, or keep (copy)
    switch (prevFrameDisposalMethod) {
      case kDisposeClear:
        if (needToBlankComposite) {
          // If we just created the composite, it could have anything in it's
          // buffer. Clear whole frame
          ClearFrame(mAnim->compositingFrame);
        } else {
          // Only blank out previous frame area (both color & Mask/Alpha)
          ClearFrame(mAnim->compositingFrame, prevFrameRect);
        }
        break;
  
      case kDisposeClearAll:
        ClearFrame(mAnim->compositingFrame);
        break;
  
      case kDisposeRestorePrevious:
        // It would be better to copy only the area changed back to
        // compositingFrame.
        if (mAnim->compositingPrevFrame) {
          CopyFrameImage(mAnim->compositingPrevFrame, mAnim->compositingFrame);
  
          // destroy only if we don't need it for this frame's disposal
          if (nextFrameDisposalMethod != kDisposeRestorePrevious)
            mAnim->compositingPrevFrame = nsnull;
        } else {
          ClearFrame(mAnim->compositingFrame);
        }
        break;
      
      default:
        // Copy previous frame into compositingFrame before we put the new frame on top
        // Assumes that the previous frame represents a full frame (it could be
        // smaller in size than the container, as long as the frame before it erased
        // itself)
        // Note: Frame 1 never gets into DoComposite(), so (aNextFrameIndex - 1) will
        // always be a valid frame number.
        if (mAnim->lastCompositedFrameIndex != aNextFrameIndex - 1) {
          if (isFullPrevFrame && !aPrevFrame->GetIsPaletted()) {
            // Just copy the bits
            CopyFrameImage(aPrevFrame, mAnim->compositingFrame);
          } else {
            if (needToBlankComposite) {
              // Only blank composite when prev is transparent or not full.
              if (aPrevFrame->GetHasAlpha() || !isFullPrevFrame) {
                ClearFrame(mAnim->compositingFrame);
              }
            }
            DrawFrameTo(aPrevFrame, mAnim->compositingFrame, prevFrameRect);
          }
        }
    }
  } else if (needToBlankComposite) {
    // If we just created the composite, it could have anything in it's
    // buffers. Clear them
    ClearFrame(mAnim->compositingFrame);
  }

  // Check if the frame we are composing wants the previous image restored afer
  // it is done. Don't store it (again) if last frame wanted its image restored
  // too
  if ((nextFrameDisposalMethod == kDisposeRestorePrevious) &&
      (prevFrameDisposalMethod != kDisposeRestorePrevious)) {
    // We are storing the whole image.
    // It would be better if we just stored the area that nextFrame is going to
    // overwrite.
    if (!mAnim->compositingPrevFrame) {
      mAnim->compositingPrevFrame = new imgFrame();
      if (!mAnim->compositingPrevFrame) {
        NS_WARNING("Failed to init compositingPrevFrame!\n");
        return NS_ERROR_OUT_OF_MEMORY;
      }
      nsresult rv = mAnim->compositingPrevFrame->Init(0, 0, mSize.width, mSize.height,
                                                      gfxASurface::ImageFormatARGB32);
      if (NS_FAILED(rv)) {
        mAnim->compositingPrevFrame = nsnull;
        return rv;
      }
    }

    CopyFrameImage(mAnim->compositingFrame, mAnim->compositingPrevFrame);
  }

  // blit next frame into it's correct spot
  DrawFrameTo(aNextFrame, mAnim->compositingFrame, nextFrameRect);

  // Set timeout of CompositeFrame to timeout of frame we just composed
  // Bug 177948
  PRInt32 timeout = aNextFrame->GetTimeout();
  mAnim->compositingFrame->SetTimeout(timeout);

  // Tell the image that it is fully 'downloaded'.
  nsresult rv = mAnim->compositingFrame->ImageUpdated(mAnim->compositingFrame->GetRect());
  if (NS_FAILED(rv)) {
    return rv;
  }

  // We don't want to keep composite images for 8bit frames.
  // Also this optimization won't work if the next frame has 
  // kDisposeRestorePrevious, because it would need to be restored 
  // into "after prev disposal but before next blend" state, 
  // not into empty frame.
  if (isFullNextFrame && mAnimationMode == kNormalAnimMode && mLoopCount != 0 &&
      nextFrameDisposalMethod != kDisposeRestorePrevious &&
      !aNextFrame->GetIsPaletted()) {
    // We have a composited full frame
    // Store the composited frame into the mFrames[..] so we don't have to
    // continuously re-build it
    // Then set the previous frame's disposal to CLEAR_ALL so we just draw the
    // frame next time around
    if (CopyFrameImage(mAnim->compositingFrame, aNextFrame)) {
      aPrevFrame->SetFrameDisposalMethod(kDisposeClearAll);
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
void
RasterImage::ClearFrame(imgFrame *aFrame)
{
  if (!aFrame)
    return;

  nsresult rv = aFrame->LockImageData();
  if (NS_FAILED(rv))
    return;

  nsRefPtr<gfxASurface> surf;
  aFrame->GetSurface(getter_AddRefs(surf));

  // Erase the surface to transparent
  gfxContext ctx(surf);
  ctx.SetOperator(gfxContext::OPERATOR_CLEAR);
  ctx.Paint();

  aFrame->UnlockImageData();
}

//******************************************************************************
void
RasterImage::ClearFrame(imgFrame *aFrame, nsIntRect &aRect)
{
  if (!aFrame || aRect.width <= 0 || aRect.height <= 0)
    return;

  nsresult rv = aFrame->LockImageData();
  if (NS_FAILED(rv))
    return;

  nsRefPtr<gfxASurface> surf;
  aFrame->GetSurface(getter_AddRefs(surf));

  // Erase the destination rectangle to transparent
  gfxContext ctx(surf);
  ctx.SetOperator(gfxContext::OPERATOR_CLEAR);
  ctx.Rectangle(gfxRect(aRect.x, aRect.y, aRect.width, aRect.height));
  ctx.Fill();

  aFrame->UnlockImageData();
}


//******************************************************************************
// Whether we succeed or fail will not cause a crash, and there's not much
// we can do about a failure, so there we don't return a nsresult
PRBool
RasterImage::CopyFrameImage(imgFrame *aSrcFrame,
                            imgFrame *aDstFrame)
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

  return PR_TRUE;
}

//******************************************************************************
/* 
 * aSrc is the current frame being drawn,
 * aDst is the composition frame where the current frame is drawn into.
 * aSrcRect is the size of the current frame, and the position of that frame
 *          in the composition frame.
 */
nsresult
RasterImage::DrawFrameTo(imgFrame *aSrc,
                         imgFrame *aDst,
                         nsIntRect& aSrcRect)
{
  NS_ENSURE_ARG_POINTER(aSrc);
  NS_ENSURE_ARG_POINTER(aDst);

  nsIntRect dstRect = aDst->GetRect();

  // According to both AGIF and APNG specs, offsets are unsigned
  if (aSrcRect.x < 0 || aSrcRect.y < 0) {
    NS_WARNING("RasterImage::DrawFrameTo: negative offsets not allowed");
    return NS_ERROR_FAILURE;
  }
  // Outside the destination frame, skip it
  if ((aSrcRect.x > dstRect.width) || (aSrcRect.y > dstRect.height)) {
    return NS_OK;
  }

  if (aSrc->GetIsPaletted()) {
    // Larger than the destination frame, clip it
    PRInt32 width = PR_MIN(aSrcRect.width, dstRect.width - aSrcRect.x);
    PRInt32 height = PR_MIN(aSrcRect.height, dstRect.height - aSrcRect.y);

    // The clipped image must now fully fit within destination image frame
    NS_ASSERTION((aSrcRect.x >= 0) && (aSrcRect.y >= 0) &&
                 (aSrcRect.x + width <= dstRect.width) &&
                 (aSrcRect.y + height <= dstRect.height),
                "RasterImage::DrawFrameTo: Invalid aSrcRect");

    // clipped image size may be smaller than source, but not larger
    NS_ASSERTION((width <= aSrcRect.width) && (height <= aSrcRect.height),
                 "RasterImage::DrawFrameTo: source must be smaller than dest");

    if (NS_FAILED(aDst->LockImageData()))
      return NS_ERROR_FAILURE;

    // Get pointers to image data
    PRUint32 size;
    PRUint8 *srcPixels;
    PRUint32 *colormap;
    PRUint32 *dstPixels;

    aSrc->GetImageData(&srcPixels, &size);
    aSrc->GetPaletteData(&colormap, &size);
    aDst->GetImageData((PRUint8 **)&dstPixels, &size);
    if (!srcPixels || !dstPixels || !colormap) {
      aDst->UnlockImageData();
      return NS_ERROR_FAILURE;
    }

    // Skip to the right offset
    dstPixels += aSrcRect.x + (aSrcRect.y * dstRect.width);
    if (!aSrc->GetHasAlpha()) {
      for (PRInt32 r = height; r > 0; --r) {
        for (PRInt32 c = 0; c < width; c++) {
          dstPixels[c] = colormap[srcPixels[c]];
        }
        // Go to the next row in the source resp. destination image
        srcPixels += aSrcRect.width;
        dstPixels += dstRect.width;
      }
    } else {
      for (PRInt32 r = height; r > 0; --r) {
        for (PRInt32 c = 0; c < width; c++) {
          const PRUint32 color = colormap[srcPixels[c]];
          if (color)
            dstPixels[c] = color;
        }
        // Go to the next row in the source resp. destination image
        srcPixels += aSrcRect.width;
        dstPixels += dstRect.width;
      }
    }

    aDst->UnlockImageData();
    return NS_OK;
  }

  nsRefPtr<gfxPattern> srcPatt;
  aSrc->GetPattern(getter_AddRefs(srcPatt));

  aDst->LockImageData();
  nsRefPtr<gfxASurface> dstSurf;
  aDst->GetSurface(getter_AddRefs(dstSurf));

  gfxContext dst(dstSurf);
  dst.Translate(gfxPoint(aSrcRect.x, aSrcRect.y));
  dst.Rectangle(gfxRect(0, 0, aSrcRect.width, aSrcRect.height), PR_TRUE);
  
  // first clear the surface if the blend flag says so
  PRInt32 blendMethod = aSrc->GetBlendMethod();
  if (blendMethod == kBlendSource) {
    gfxContext::GraphicsOperator defaultOperator = dst.CurrentOperator();
    dst.SetOperator(gfxContext::OPERATOR_CLEAR);
    dst.Fill();
    dst.SetOperator(defaultOperator);
  }
  dst.SetPattern(srcPatt);
  dst.Paint();

  aDst->UnlockImageData();

  return NS_OK;
}


/********* Methods to implement lazy allocation of nsIProperties object *************/
NS_IMETHODIMP
RasterImage::Get(const char *prop, const nsIID & iid, void * *result)
{
  if (!mProperties)
    return NS_ERROR_FAILURE;
  return mProperties->Get(prop, iid, result);
}

NS_IMETHODIMP
RasterImage::Set(const char *prop, nsISupports *value)
{
  if (!mProperties)
    mProperties = do_CreateInstance("@mozilla.org/properties;1");
  if (!mProperties)
    return NS_ERROR_OUT_OF_MEMORY;
  return mProperties->Set(prop, value);
}

NS_IMETHODIMP
RasterImage::Has(const char *prop, PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  if (!mProperties) {
    *_retval = PR_FALSE;
    return NS_OK;
  }
  return mProperties->Has(prop, _retval);
}

NS_IMETHODIMP
RasterImage::Undefine(const char *prop)
{
  if (!mProperties)
    return NS_ERROR_FAILURE;
  return mProperties->Undefine(prop);
}

NS_IMETHODIMP
RasterImage::GetKeys(PRUint32 *count, char ***keys)
{
  if (!mProperties) {
    *count = 0;
    *keys = nsnull;
    return NS_OK;
  }
  return mProperties->GetKeys(count, keys);
}

void
RasterImage::Discard()
{
  // We should be ok for discard
  NS_ABORT_IF_FALSE(CanDiscard(), "Asked to discard but can't!");

  // We should never discard when we have an active decoder
  NS_ABORT_IF_FALSE(!mDecoder, "Asked to discard with open decoder!");

  // As soon as an image becomes animated, it becomes non-discardable and any
  // timers are cancelled.
  NS_ABORT_IF_FALSE(!mAnim, "Asked to discard for animated image!");

  // For post-operation logging
  int old_frame_count = mFrames.Length();

  // Delete all the decoded frames, then clear the array.
  for (int i = 0; i < old_frame_count; ++i)
    delete mFrames[i];
  mFrames.Clear();

  // Flag that we no longer have decoded frames for this image
  mDecoded = PR_FALSE;

  // Notify that we discarded
  nsCOMPtr<imgIDecoderObserver> observer(do_QueryReferent(mObserver));
  if (observer)
    observer->OnDiscard(nsnull);

  // Log
  PR_LOG(gCompressedImageAccountingLog, PR_LOG_DEBUG,
         ("CompressedImageAccounting: discarded uncompressed image "
          "data from RasterImage %p (%s) - %d frames (cached count: %d); "
          "Total Containers: %d, Discardable containers: %d, "
          "Total source bytes: %lld, Source bytes for discardable containers %lld",
          this,
          mSourceDataMimeType.get(),
          old_frame_count,
          mFrames.Length(),
          num_containers,
          num_discardable_containers,
          total_source_bytes,
          discardable_source_bytes));
}

// Helper method to determine if we can discard an image
PRBool
RasterImage::CanDiscard() {
  return (DiscardingEnabled() && // Globally enabled...
          mDiscardable &&        // ...Enabled at creation time...
          (mLockCount == 0) &&   // ...not temporarily disabled...
          mHasSourceData &&      // ...have the source data...
          mDecoded);             // ...and have something to discard.
}

// Helper method to tell us whether the clock is currently running for
// discarding this image. Mainly for assertions.
PRBool
RasterImage::DiscardingActive() {
  return !!(mDiscardTrackerNode.prev || mDiscardTrackerNode.next);
}

// Helper method to determine if we're storing the source data in a buffer
// or just writing it directly to the decoder
PRBool
RasterImage::StoringSourceData() {
  return (mDecodeOnDraw || mDiscardable);
}


// Sets up a decoder for this image. It is an error to call this function
// when decoding is already in process (ie - when mDecoder is non-null).
nsresult
RasterImage::InitDecoder(bool aDoSizeDecode)
{
  // Ensure that the decoder is not already initialized
  NS_ABORT_IF_FALSE(!mDecoder, "Calling InitDecoder() while already decoding!");
  
  // We shouldn't be firing up a decoder if we already have the frames decoded
  NS_ABORT_IF_FALSE(!mDecoded, "Calling InitDecoder() but already decoded!");

  // Since we're not decoded, we should not have a discard timer active
  NS_ABORT_IF_FALSE(!DiscardingActive(), "Discard Timer active in InitDecoder()!");

  // Figure out which decoder we want
  eDecoderType type = GetDecoderType(mSourceDataMimeType.get());
  CONTAINER_ENSURE_TRUE(type != eDecoderType_unknown, NS_IMAGELIB_ERROR_NO_DECODER);

  // Instantiate the appropriate decoder
  switch (type) {
    case eDecoderType_png:
      mDecoder = new nsPNGDecoder();
      break;
    case eDecoderType_gif:
      mDecoder = new nsGIFDecoder2();
      break;
    case eDecoderType_jpeg:
      mDecoder = new nsJPEGDecoder();
      break;
    case eDecoderType_bmp:
      mDecoder = new nsBMPDecoder();
      break;
    case eDecoderType_ico:
      mDecoder = new nsICODecoder();
      break;
    case eDecoderType_icon:
      mDecoder = new nsIconDecoder();
      break;
    default:
      NS_ABORT_IF_FALSE(0, "Shouldn't get here!");
  }

  // Initialize the decoder
  nsCOMPtr<imgIDecoderObserver> observer(do_QueryReferent(mObserver));
  mDecoder->SetSizeDecode(aDoSizeDecode);
  nsresult result = mDecoder->Init(this, observer);
  CONTAINER_ENSURE_SUCCESS(result);

  // Create a decode worker
  mWorker = new imgDecodeWorker(this);
  CONTAINER_ENSURE_TRUE(mWorker, NS_ERROR_OUT_OF_MEMORY);

  return NS_OK;
}

// Flushes, closes, and nulls-out a decoder. Cleans up any related decoding
// state. It is an error to call this function when there is no initialized
// decoder.
// 
// aIntent specifies the intent of the shutdown. If aIntent is
// eShutdownIntent_Done, an error is flagged if we didn't get what we should
// have out of the decode. If aIntent is eShutdownIntent_Interrupted, we don't
// check this. If aIntent is eShutdownIntent_Error, we shut down in error mode.
nsresult
RasterImage::ShutdownDecoder(eShutdownIntent aIntent)
{
  // Ensure that our intent is valid
  NS_ABORT_IF_FALSE((aIntent >= 0) || (aIntent < eShutdownIntent_AllCount),
                    "Invalid shutdown intent");

  // Ensure that the decoder is initialized
  NS_ABORT_IF_FALSE(mDecoder, "Calling ShutdownDecoder() with no active decoder!");

  // Figure out what kind of decode we were doing before we get rid of our decoder
  bool wasSizeDecode = mDecoder->IsSizeDecode();

  // If we're not in error mode, finalize the decoder
  nsresult rv = NS_OK;
  if (aIntent != eShutdownIntent_Error) {
    mInDecoder = PR_TRUE;
    rv = mDecoder->Finish();
    mInDecoder = PR_FALSE;
  }

  // null out the decoder, _then_ check for errors on the close (otherwise the
  // error routine might re-invoke ShutdownDecoder)
  mDecoder = nsnull;
  if (NS_FAILED(rv)) {
    DoError();
    return rv;
  }

  // Kill off the worker
  mWorker = nsnull;

  // We just shut down the decoder. If we didn't get what we want, but expected
  // to, flag an error
  PRBool failed = PR_FALSE;
  if (wasSizeDecode && !mHasSize)
    failed = PR_TRUE;
  if (!wasSizeDecode && !mDecoded)
    failed = PR_TRUE;
  if ((aIntent == eShutdownIntent_Done) && failed) {
    DoError();
    return NS_ERROR_FAILURE;
  }

  // Reset number of decoded bytes
  mBytesDecoded = 0;

  return NS_OK;
}

// Writes the data to the decoder, updating the total number of bytes written.
nsresult
RasterImage::WriteToDecoder(const char *aBuffer, PRUint32 aCount)
{
  // We should have a decoder
  NS_ABORT_IF_FALSE(mDecoder, "Trying to write to null decoder!");

  // The decoder will start decoding into the current frame (if we have one).
  // When it needs to add another frame, we will unlock this frame and lock the
  // new frame.
  // Our invariant is that, while in the decoder, the last frame is always
  // locked, and all others are unlocked.
  if (mFrames.Length() > 0) {
    imgFrame *curframe = mFrames.ElementAt(mFrames.Length() - 1);
    curframe->LockImageData();
  }

  // Write
  mInDecoder = PR_TRUE;
  nsresult rv = mDecoder->Write(aBuffer, aCount);
  mInDecoder = PR_FALSE;

  // We unlock the current frame, even if that frame is different from the
  // frame we entered the decoder with. (See above.)
  if (mFrames.Length() > 0) {
    imgFrame *curframe = mFrames.ElementAt(mFrames.Length() - 1);
    curframe->UnlockImageData();
  }

  CONTAINER_ENSURE_SUCCESS(rv);

  // Keep track of the total number of bytes written over the lifetime of the
  // decoder
  mBytesDecoded += aCount;

  return NS_OK;
}

// This function is called in situations where it's clear that we want the
// frames in decoded form (Draw, GetFrame, CopyFrame, ExtractFrame, etc).
// If we're completely decoded, this method resets the discard timer (if
// we're discardable), since wanting the frames now is a good indicator of
// wanting them again soon. If we're not decoded, this method kicks off
// asynchronous decoding to generate the frames.
nsresult
RasterImage::WantDecodedFrames()
{
  nsresult rv;

  // If we can discard, the clock should be running. Reset it.
  if (CanDiscard()) {
    NS_ABORT_IF_FALSE(DiscardingActive(),
                      "Decoded and discardable but discarding not activated!");
    rv = DiscardTracker::Reset(&mDiscardTrackerNode);
    CONTAINER_ENSURE_SUCCESS(rv);
  }

  // Request a decode (no-op if we're decoded)
  return RequestDecode();
}

//******************************************************************************
/* void requestDecode() */
NS_IMETHODIMP
RasterImage::RequestDecode()
{
  nsresult rv;

  if (mError)
    return NS_ERROR_FAILURE;

  // If we're not storing source data, we have nothing to do
  if (!StoringSourceData())
    return NS_OK;

  // If we're fully decoded, we have nothing to do
  if (mDecoded)
    return NS_OK;

  // If we've already got a full decoder running, we have nothing to do
  if (mDecoder && !mDecoder->IsSizeDecode())
    return NS_OK;

  // If our callstack goes through a size decoder, we have a problem.
  // We need to shutdown the size decode and replace it with  a full
  // decoder, but can't do that from within the decoder itself. Thus, we post
  // an asynchronous event to the event loop to do it later. Since
  // RequestDecode() is an asynchronous function this works fine (though it's
  // a little slower).
  if (mInDecoder) {
    nsRefPtr<imgDecodeRequestor> requestor = new imgDecodeRequestor(this);
    if (!requestor)
      return NS_ERROR_OUT_OF_MEMORY;
    return NS_DispatchToCurrentThread(requestor);
  }


  // If we have a size decode open, interrupt it and shut it down
  if (mDecoder && mDecoder->IsSizeDecode()) {
    rv = ShutdownDecoder(eShutdownIntent_Interrupted);
    CONTAINER_ENSURE_SUCCESS(rv);
  }

  // If we don't have a decoder, create one 
  if (!mDecoder) {
    NS_ABORT_IF_FALSE(mFrames.IsEmpty(), "Trying to decode to non-empty frame-array");
    rv = InitDecoder(/* aDoSizeDecode = */ false);
    CONTAINER_ENSURE_SUCCESS(rv);
  }

  // If we already have a pending worker, we're done
  if (mWorkerPending)
    return NS_OK;

  // If we've read all the data we have, we're done
  if (mBytesDecoded == mSourceData.Length())
    return NS_OK;

  // If it's a smallish image, it's not worth it to do things async
  if (!mDecoded && !mInDecoder && mHasSourceData && (mSourceData.Length() < gMaxBytesForSyncDecode))
    return SyncDecode();

  // If we get this far, dispatch the worker. We do this instead of starting
  // any immediate decoding to guarantee that all our decode notifications are
  // dispatched asynchronously, and to ensure we stay responsive.
  return mWorker->Dispatch();
}

// Synchronously decodes as much data as possible
nsresult
RasterImage::SyncDecode()
{
  nsresult rv;

  // If we're decoded already, no worries
  if (mDecoded)
    return NS_OK;

  // If we're not storing source data, there isn't much to do here
  if (!StoringSourceData())
    return NS_OK;

  // We really have no good way of forcing a synchronous decode if we're being
  // called in a re-entrant manner (ie, from an event listener fired by a
  // decoder), because the decoding machinery is already tied up. We thus explicitly
  // disallow this type of call in the API, and check for it in API methods.
  NS_ABORT_IF_FALSE(!mInDecoder, "Yikes, forcing sync in reentrant call!");

  // If we have a size decode open, shut it down
  if (mDecoder && mDecoder->IsSizeDecode()) {
    rv = ShutdownDecoder(eShutdownIntent_Interrupted);
    CONTAINER_ENSURE_SUCCESS(rv);
  }

  // If we don't have a decoder, create one 
  if (!mDecoder) {
    NS_ABORT_IF_FALSE(mFrames.IsEmpty(), "Trying to decode to non-empty frame-array");
    rv = InitDecoder(/* aDoSizeDecode = */ false);
    CONTAINER_ENSURE_SUCCESS(rv);
  }

  // Write everything we have
  rv = WriteToDecoder(mSourceData.Elements() + mBytesDecoded,
                      mSourceData.Length() - mBytesDecoded);
  CONTAINER_ENSURE_SUCCESS(rv);

  // When we're doing a sync decode, we want to get as much information from the
  // image as possible. We've send the decoder all of our data, so now's a good
  // time  to flush any invalidations (in case we don't have all the data and what
  // we got left us mid-frame).
  mInDecoder = PR_TRUE;
  mDecoder->FlushInvalidations();
  mInDecoder = PR_FALSE;

  // If we finished the decode, shutdown the decoder
  if (IsDecodeFinished()) {
    rv = ShutdownDecoder(eShutdownIntent_Done);
    CONTAINER_ENSURE_SUCCESS(rv);
  }

  // All good!
  return NS_OK;
}

//******************************************************************************
/* [noscript] void draw(in gfxContext aContext,
 *                      in gfxGraphicsFilter aFilter,
 *                      [const] in gfxMatrix aUserSpaceToImageSpace,
 *                      [const] in gfxRect aFill,
 *                      [const] in nsIntRect aSubimage,
 *                      [const] in nsIntSize aViewportSize,
 *                      in PRUint32 aFlags); */
NS_IMETHODIMP
RasterImage::Draw(gfxContext *aContext,
                  gfxPattern::GraphicsFilter aFilter,
                  const gfxMatrix &aUserSpaceToImageSpace,
                  const gfxRect &aFill,
                  const nsIntRect &aSubimage,
                  const nsIntSize& /*aViewportSize - ignored*/,
                  PRUint32 aFlags)
{
  if (mError)
    return NS_ERROR_FAILURE;

  // Disallowed in the API
  if (mInDecoder && (aFlags & imgIContainer::FLAG_SYNC_DECODE))
    return NS_ERROR_FAILURE;

  NS_ENSURE_ARG_POINTER(aContext);

  // If a synchronous draw is requested, flush anything that might be sitting around
  if (aFlags & FLAG_SYNC_DECODE) {
    nsresult rv = SyncDecode();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  imgFrame *frame = GetCurrentDrawableImgFrame();
  if (!frame) {
    return NS_OK; // Getting the frame (above) touches the image and kicks off decoding
  }

  nsIntRect framerect = frame->GetRect();
  nsIntMargin padding(framerect.x, framerect.y, 
                      mSize.width - framerect.XMost(),
                      mSize.height - framerect.YMost());

  frame->Draw(aContext, aFilter, aUserSpaceToImageSpace, aFill, padding, aSubimage);

  return NS_OK;
}

//******************************************************************************
/* [notxpcom] nsIFrame GetRootLayoutFrame() */
nsIFrame*
RasterImage::GetRootLayoutFrame()
{
  return nsnull;
}

//******************************************************************************
/* void lockImage() */
NS_IMETHODIMP
RasterImage::LockImage()
{
  if (mError)
    return NS_ERROR_FAILURE;

  // Cancel the discard timer if it's there
  DiscardTracker::Remove(&mDiscardTrackerNode);

  // Increment the lock count
  mLockCount++;

  return NS_OK;
}

//******************************************************************************
/* void unlockImage() */
NS_IMETHODIMP
RasterImage::UnlockImage()
{
  if (mError)
    return NS_ERROR_FAILURE;

  // It's an error to call this function if the lock count is 0
  NS_ABORT_IF_FALSE(mLockCount > 0,
                    "Calling UnlockImage with mLockCount == 0!");
  if (mLockCount == 0)
    return NS_ERROR_ABORT;

  // We're locked, so discarding should not be active
  NS_ABORT_IF_FALSE(!DiscardingActive(), "Locked, but discarding activated");

  // Decrement our lock count
  mLockCount--;

  // We now _might_ have one of the qualifications for discarding. Re-evaluate.
  if (CanDiscard()) {
    nsresult rv = DiscardTracker::Reset(&mDiscardTrackerNode);
    CONTAINER_ENSURE_SUCCESS(rv);
  }

  return NS_OK;
}

// Flushes up to aMaxBytes to the decoder.
nsresult
RasterImage::DecodeSomeData(PRUint32 aMaxBytes)
{
  // We should have a decoder if we get here
  NS_ABORT_IF_FALSE(mDecoder, "trying to decode without decoder!");

  // If we have nothing to decode, return
  if (mBytesDecoded == mSourceData.Length())
    return NS_OK;


  // write the proper amount of data
  PRUint32 bytesToDecode = PR_MIN(aMaxBytes,
                                  mSourceData.Length() - mBytesDecoded);
  nsresult rv = WriteToDecoder(mSourceData.Elements() + mBytesDecoded,
                               bytesToDecode);

  return rv;
}

// There are various indicators that tell us we're finished with the decode
// task at hand and can shut down the decoder.
//
// This method may not be called if there is no decoder.
PRBool
RasterImage::IsDecodeFinished()
{
  // Precondition
  NS_ABORT_IF_FALSE(mDecoder, "Can't call IsDecodeFinished() without decoder!");

  // Assume it's not finished
  PRBool decodeFinished = PR_FALSE;

  // There shouldn't be any reason to call this if we're not storing
  // source data
  NS_ABORT_IF_FALSE(StoringSourceData(),
                    "just shut down on SourceDataComplete!");

  // The decode is complete if we got what we wanted...
  if (mDecoder->IsSizeDecode()) {
    if (mHasSize)
      decodeFinished = PR_TRUE;
  }
  else {
    if (mDecoded)
      decodeFinished = PR_TRUE;
  }

  // ...or if we have all the source data and wrote all the source data.
  //
  // (NB - This can be distinct from the above case even for non-erroneous
  // images because the decoder might not call DecodingComplete() until we
  // call Close() in ShutdownDecoder())
  if (mHasSourceData && (mBytesDecoded == mSourceData.Length()))
    decodeFinished = PR_TRUE;

  return decodeFinished;
}

// Indempotent error flagging routine. If a decoder is open,
// sends OnStopContainer and OnStopDecode and shuts down the decoder
void
RasterImage::DoError()
{
  // If we've flagged an error before, we have nothing to do
  if (mError)
    return;

  // If we're mid-decode
  if (mDecoder) {

    // grab the observer and give an OnStopContainer and an OnStopDecode
    nsCOMPtr<imgIDecoderObserver> observer = do_QueryReferent(mObserver);
    if (observer) {
      observer->OnStopContainer(nsnull, this);
      observer->OnStopDecode(nsnull, NS_ERROR_FAILURE, nsnull);
    }

    // Shutdown the decoder in error mode. We don't care if this flags other
    // errors.
    (void) ShutdownDecoder(eShutdownIntent_Error);
  }

  // Put the container in an error state
  mError = PR_TRUE;

  // Log our error
  LOG_CONTAINER_ERROR;
}

// Decodes some data, then re-posts itself to the end of the event queue if
// there's more processing to be done
NS_IMETHODIMP
imgDecodeWorker::Run()
{
  nsresult rv;

  // If we shutdown the decoder in this function, we could lose ourselves
  nsCOMPtr<nsIRunnable> kungFuDeathGrip(this);

  // The container holds a strong reference to us. Cycles are bad.
  nsCOMPtr<imgIContainer> iContainer(do_QueryReferent(mContainer));
  if (!iContainer)
    return NS_OK;
  RasterImage* image = static_cast<RasterImage*>(iContainer.get());

  NS_ABORT_IF_FALSE(image->mInitialized,
                    "Worker active for uninitialized container!");

  // If we were pending, we're not anymore
  image->mWorkerPending = PR_FALSE;

  // If an error is flagged, it probably happened while we were waiting
  // in the event queue. Bail early, but no need to bother the run queue
  // by returning an error.
  if (image->mError)
    return NS_OK;

  // If we don't have a decoder, we must have finished already (for example,
  // a synchronous decode request came while the worker was pending).
  if (!image->mDecoder)
    return NS_OK;

  // Size decodes are cheap and we more or less want them to be
  // synchronous. Write all the data in that case, otherwise write a
  // chunk
  PRUint32 maxBytes = image->mDecoder->IsSizeDecode()
    ? image->mSourceData.Length() : gDecodeBytesAtATime;

  // Loop control
  PRBool haveMoreData = PR_TRUE;
  nsTime deadline(PR_Now() + 1000 * gMaxMSBeforeYield);

  // We keep decoding chunks until one of three possible events occur:
  // 1) We don't have any data left to decode
  // 2) The decode completes
  // 3) We hit the deadline and need to yield to keep the UI snappy
  while (haveMoreData && !image->IsDecodeFinished() &&
         (nsTime(PR_Now()) < deadline)) {

    // Decode a chunk of data
    rv = image->DecodeSomeData(maxBytes);
    if (NS_FAILED(rv)) {
      image->DoError();
      return rv;
    }

    // Figure out if we still have more data
    haveMoreData =
      image->mSourceData.Length() > image->mBytesDecoded;
  }

  // Flush invalidations _after_ we've written everything we're going to.
  // Furthermore, if this is a redecode, we don't want to do progressive
  // display at all. In that case, let Decoder::PostFrameStop() do the
  // flush once the whole frame is ready.
  if (!image->mHasBeenDecoded) {
    image->mInDecoder = PR_TRUE;
    image->mDecoder->FlushInvalidations();
    image->mInDecoder = PR_FALSE;
  }

  // If the decode finished, shutdown the decoder
  if (image->IsDecodeFinished()) {
    rv = image->ShutdownDecoder(RasterImage::eShutdownIntent_Done);
    if (NS_FAILED(rv)) {
      image->DoError();
      return rv;
    }
  }

  // If Conditions 1 & 2 are still true, then the only reason we bailed was
  // because we hit the deadline. Repost ourselves to the end of the event
  // queue.
  if (image->mDecoder && !image->IsDecodeFinished() && haveMoreData)
    return this->Dispatch();

  // Otherwise, return success
  return NS_OK;
}

// Queues the worker up at the end of the event queue
NS_METHOD imgDecodeWorker::Dispatch()
{
  // The container holds a strong reference to us. Cycles are bad.
  nsCOMPtr<imgIContainer> iContainer(do_QueryReferent(mContainer));
  if (!iContainer)
    return NS_OK;
  RasterImage* image = static_cast<RasterImage*>(iContainer.get());

  // We should not be called if there's already a pending worker
  NS_ABORT_IF_FALSE(!image->mWorkerPending,
                    "Trying to queue up worker with one already pending!");

  // Flag that we're pending
  image->mWorkerPending = PR_TRUE;

  // Dispatch
  return NS_DispatchToCurrentThread(this);
}

// nsIInputStream callback to copy the incoming image data directly to the 
// RasterImage without processing. The RasterImage is passed as the closure.
// Always reads everything it gets, even if the data is erroneous.
NS_METHOD
RasterImage::WriteToRasterImage(nsIInputStream* /* unused */,
                                void*          aClosure,
                                const char*    aFromRawSegment,
                                PRUint32       /* unused */,
                                PRUint32       aCount,
                                PRUint32*      aWriteCount)
{
  // Retrieve the RasterImage
  RasterImage* image = static_cast<RasterImage*>(aClosure);

  // Copy the source data. We squelch the return value here, because returning
  // an error means that ReadSegments stops reading data, violating our
  // invariant that we read everything we get.
  (void) image->AddSourceData(aFromRawSegment, aCount);

  // We wrote everything we got
  *aWriteCount = aCount;

  return NS_OK;
}

PRBool
RasterImage::ShouldAnimate()
{
  return Image::ShouldAnimate() && mFrames.Length() >= 2 &&
         mAnimationMode != kDontAnimMode && !mAnimationFinished;
}

//******************************************************************************
/* readonly attribute PRUint32 framesNotified; */
#ifdef DEBUG
NS_IMETHODIMP
RasterImage::GetFramesNotified(PRUint32 *aFramesNotified)
{
  NS_ENSURE_ARG_POINTER(aFramesNotified);

  *aFramesNotified = mFramesNotified;

  return NS_OK;
}
#endif

} // namespace imagelib
} // namespace mozilla
