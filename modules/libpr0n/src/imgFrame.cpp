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
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Joe Drew <joe@drew.ca> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "imgFrame.h"

#include <limits.h>

#include "prmem.h"
#include "prenv.h"

#include "gfxPlatform.h"
#include "gfxUtils.h"

static bool gDisableOptimize = false;

#include "cairo.h"

#if defined(XP_WIN)

#include "gfxWindowsPlatform.h"

/* Whether to use the windows surface; only for desktop win32 */
#define USE_WIN_SURFACE 1

static PRUint32 gTotalDDBs = 0;
static PRUint32 gTotalDDBSize = 0;
// only use up a maximum of 64MB in DDBs
#define kMaxDDBSize (64*1024*1024)
// and don't let anything in that's bigger than 4MB
#define kMaxSingleDDBSize (4*1024*1024)

#endif

// Returns true if an image of aWidth x aHeight is allowed and legal.
static bool AllowedImageSize(PRInt32 aWidth, PRInt32 aHeight)
{
  // reject over-wide or over-tall images
  const PRInt32 k64KLimit = 0x0000FFFF;
  if (NS_UNLIKELY(aWidth > k64KLimit || aHeight > k64KLimit )) {
    NS_WARNING("image too big");
    return PR_FALSE;
  }

  // protect against invalid sizes
  if (NS_UNLIKELY(aHeight <= 0 || aWidth <= 0)) {
    return PR_FALSE;
  }

  // check to make sure we don't overflow a 32-bit
  PRInt32 tmp = aWidth * aHeight;
  if (NS_UNLIKELY(tmp / aHeight != aWidth)) {
    NS_WARNING("width or height too large");
    return PR_FALSE;
  }
  tmp = tmp * 4;
  if (NS_UNLIKELY(tmp / 4 != aWidth * aHeight)) {
    NS_WARNING("width or height too large");
    return PR_FALSE;
  }
#if defined(XP_MACOSX)
  // CoreGraphics is limited to images < 32K in *height*, so clamp all surfaces on the Mac to that height
  if (NS_UNLIKELY(aHeight > SHRT_MAX)) {
    NS_WARNING("image too big");
    return PR_FALSE;
  }
#endif
  return PR_TRUE;
}

// Returns whether we should, at this time, use image surfaces instead of
// optimized platform-specific surfaces.
static bool ShouldUseImageSurfaces()
{
#if defined(USE_WIN_SURFACE)
  static const DWORD kGDIObjectsHighWaterMark = 7000;

  if (gfxWindowsPlatform::GetPlatform()->GetRenderMode() ==
      gfxWindowsPlatform::RENDER_DIRECT2D) {
    return PR_TRUE;
  }

  // at 7000 GDI objects, stop allocating normal images to make sure
  // we never hit the 10k hard limit.
  // GetCurrentProcess() just returns (HANDLE)-1, it's inlined afaik
  DWORD count = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
  if (count == 0 ||
      count > kGDIObjectsHighWaterMark)
  {
    // either something's broken (count == 0),
    // or we hit our high water mark; disable
    // image allocations for a bit.
    return PR_TRUE;
  }
#endif

  return PR_FALSE;
}

imgFrame::imgFrame() :
  mDecoded(0, 0, 0, 0),
  mPalettedImageData(nsnull),
  mSinglePixelColor(0),
  mTimeout(100),
  mDisposalMethod(0), /* imgIContainer::kDisposeNotSpecified */
  mBlendMethod(1), /* imgIContainer::kBlendOver */
  mSinglePixel(PR_FALSE),
  mNeverUseDeviceSurface(PR_FALSE),
  mFormatChanged(PR_FALSE),
  mCompositingFailed(PR_FALSE)
#ifdef USE_WIN_SURFACE
  , mIsDDBSurface(PR_FALSE)
#endif
  , mLocked(PR_FALSE)
{
  static bool hasCheckedOptimize = false;
  if (!hasCheckedOptimize) {
    if (PR_GetEnv("MOZ_DISABLE_IMAGE_OPTIMIZE")) {
      gDisableOptimize = PR_TRUE;
    }
    hasCheckedOptimize = PR_TRUE;
  }
}

imgFrame::~imgFrame()
{
  PR_FREEIF(mPalettedImageData);
#ifdef USE_WIN_SURFACE
  if (mIsDDBSurface) {
      gTotalDDBs--;
      gTotalDDBSize -= mSize.width * mSize.height * 4;
  }
#endif
}

nsresult imgFrame::Init(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight, 
                        gfxASurface::gfxImageFormat aFormat, PRUint8 aPaletteDepth /* = 0 */)
{
  // assert for properties that should be verified by decoders, warn for properties related to bad content
  if (!AllowedImageSize(aWidth, aHeight))
    return NS_ERROR_FAILURE;

  mOffset.MoveTo(aX, aY);
  mSize.SizeTo(aWidth, aHeight);

  mFormat = aFormat;
  mPaletteDepth = aPaletteDepth;

  if (aPaletteDepth != 0) {
    // We're creating for a paletted image.
    if (aPaletteDepth > 8) {
      NS_ERROR("This Depth is not supported");
      return NS_ERROR_FAILURE;
    }

    // Use the fallible allocator here
    mPalettedImageData = (PRUint8*)moz_malloc(PaletteDataLength() + GetImageDataLength());
    NS_ENSURE_TRUE(mPalettedImageData, NS_ERROR_OUT_OF_MEMORY);
  } else {
    // For Windows, we must create the device surface first (if we're
    // going to) so that the image surface can wrap it.  Can't be done
    // the other way around.
#ifdef USE_WIN_SURFACE
    if (!mNeverUseDeviceSurface && !ShouldUseImageSurfaces()) {
      mWinSurface = new gfxWindowsSurface(gfxIntSize(mSize.width, mSize.height), mFormat);
      if (mWinSurface && mWinSurface->CairoStatus() == 0) {
        // no error
        mImageSurface = mWinSurface->GetAsImageSurface();
      } else {
        mWinSurface = nsnull;
      }
    }
#endif

    // For other platforms we create the image surface first and then
    // possibly wrap it in a device surface.  This branch is also used
    // on Windows if we're not using device surfaces or if we couldn't
    // create one.
    if (!mImageSurface)
      mImageSurface = new gfxImageSurface(gfxIntSize(mSize.width, mSize.height), mFormat);

    if (!mImageSurface || mImageSurface->CairoStatus()) {
      mImageSurface = nsnull;
      // guess
      return NS_ERROR_OUT_OF_MEMORY;
    }

#ifdef XP_MACOSX
    if (!mNeverUseDeviceSurface && !ShouldUseImageSurfaces()) {
      mQuartzSurface = new gfxQuartzImageSurface(mImageSurface);
    }
#endif
  }

  return NS_OK;
}

nsresult imgFrame::Optimize()
{
  if (gDisableOptimize)
    return NS_OK;

  if (mPalettedImageData || mOptSurface || mSinglePixel)
    return NS_OK;

  /* Figure out if the entire image is a constant color */

  // this should always be true
  if (mImageSurface->Stride() == mSize.width * 4) {
    PRUint32 *imgData = (PRUint32*) mImageSurface->Data();
    PRUint32 firstPixel = * (PRUint32*) imgData;
    PRUint32 pixelCount = mSize.width * mSize.height + 1;

    while (--pixelCount && *imgData++ == firstPixel)
      ;

    if (pixelCount == 0) {
      // all pixels were the same
      if (mFormat == gfxASurface::ImageFormatARGB32 ||
          mFormat == gfxASurface::ImageFormatRGB24)
      {
        mSinglePixelColor = gfxRGBA
          (firstPixel,
           (mFormat == gfxImageSurface::ImageFormatRGB24 ?
            gfxRGBA::PACKED_XRGB :
            gfxRGBA::PACKED_ARGB_PREMULTIPLIED));

        mSinglePixel = PR_TRUE;

        // blow away the older surfaces (if they exist), to release their memory
        mImageSurface = nsnull;
        mOptSurface = nsnull;
#ifdef USE_WIN_SURFACE
        mWinSurface = nsnull;
#endif
#ifdef XP_MACOSX
        mQuartzSurface = nsnull;
#endif
        return NS_OK;
      }
    }

    // if it's not RGB24/ARGB32, don't optimize, but we never hit this at the moment
  }

  // if we're being forced to use image surfaces due to
  // resource constraints, don't try to optimize beyond same-pixel.
  if (mNeverUseDeviceSurface || ShouldUseImageSurfaces())
    return NS_OK;

  mOptSurface = nsnull;

#ifdef USE_WIN_SURFACE
  // we need to special-case windows here, because windows has
  // a distinction between DIB and DDB and we want to use DDBs as much
  // as we can.
  if (mWinSurface) {
    // Don't do DDBs for large images; see bug 359147
    // Note that we bother with DDBs at all because they are much faster
    // on some systems; on others there isn't much of a speed difference
    // between DIBs and DDBs.
    //
    // Originally this just limited to 1024x1024; but that still
    // had us hitting overall total memory usage limits (which was
    // around 220MB on my intel shared memory system with 2GB RAM
    // and 16-128mb in use by the video card, so I can't make
    // heads or tails out of this limit).
    //
    // So instead, we clamp the max size to 64MB (this limit shuld
    // be made dynamic based on.. something.. as soon a we figure
    // out that something) and also limit each individual image to
    // be less than 4MB to keep very large images out of DDBs.

    // assume (almost -- we don't quadword-align) worst-case size
    PRUint32 ddbSize = mSize.width * mSize.height * 4;
    if (ddbSize <= kMaxSingleDDBSize &&
        ddbSize + gTotalDDBSize <= kMaxDDBSize)
    {
      nsRefPtr<gfxWindowsSurface> wsurf = mWinSurface->OptimizeToDDB(nsnull, gfxIntSize(mSize.width, mSize.height), mFormat);
      if (wsurf) {
        gTotalDDBs++;
        gTotalDDBSize += ddbSize;
        mIsDDBSurface = PR_TRUE;
        mOptSurface = wsurf;
      }
    }
    if (!mOptSurface && !mFormatChanged) {
      // just use the DIB if the format has not changed
      mOptSurface = mWinSurface;
    }
  }
#endif

#ifdef XP_MACOSX
  if (mQuartzSurface) {
    mQuartzSurface->Flush();
    mOptSurface = mQuartzSurface;
  }
#endif

  if (mOptSurface == nsnull)
    mOptSurface = gfxPlatform::GetPlatform()->OptimizeImage(mImageSurface, mFormat);

  if (mOptSurface) {
    mImageSurface = nsnull;
#ifdef USE_WIN_SURFACE
    mWinSurface = nsnull;
#endif
#ifdef XP_MACOSX
    mQuartzSurface = nsnull;
#endif
  }

  return NS_OK;
}

static void
DoSingleColorFastPath(gfxContext*    aContext,
                      const gfxRGBA& aSinglePixelColor,
                      const gfxRect& aFill)
{
  // if a == 0, it's a noop
  if (aSinglePixelColor.a == 0.0)
    return;

  gfxContext::GraphicsOperator op = aContext->CurrentOperator();
  if (op == gfxContext::OPERATOR_OVER && aSinglePixelColor.a == 1.0) {
    aContext->SetOperator(gfxContext::OPERATOR_SOURCE);
  }

  aContext->SetDeviceColor(aSinglePixelColor);
  aContext->NewPath();
  aContext->Rectangle(aFill);
  aContext->Fill();
  aContext->SetOperator(op);
  aContext->SetDeviceColor(gfxRGBA(0,0,0,0));
}

imgFrame::SurfaceWithFormat
imgFrame::SurfaceForDrawing(bool               aDoPadding,
                            bool               aDoPartialDecode,
                            bool               aDoTile,
                            const nsIntMargin& aPadding,
                            gfxMatrix&         aUserSpaceToImageSpace,
                            gfxRect&           aFill,
                            gfxRect&           aSubimage,
                            gfxRect&           aSourceRect,
                            gfxRect&           aImageRect)
{
  gfxIntSize size(PRInt32(aImageRect.Width()), PRInt32(aImageRect.Height()));
  if (!aDoPadding && !aDoPartialDecode) {
    NS_ASSERTION(!mSinglePixel, "This should already have been handled");
    return SurfaceWithFormat(new gfxSurfaceDrawable(ThebesSurface(), size), mFormat);
  }

  gfxRect available = gfxRect(mDecoded.x, mDecoded.y, mDecoded.width, mDecoded.height);

  if (aDoTile || mSinglePixel) {
    // Create a temporary surface.
    // Give this surface an alpha channel because there are
    // transparent pixels in the padding or undecoded area
    gfxImageSurface::gfxImageFormat format = gfxASurface::ImageFormatARGB32;
    nsRefPtr<gfxASurface> surface =
      gfxPlatform::GetPlatform()->CreateOffscreenSurface(size, gfxImageSurface::ContentFromFormat(format));
    if (!surface || surface->CairoStatus())
      return SurfaceWithFormat();

    // Fill 'available' with whatever we've got
    gfxContext tmpCtx(surface);
    tmpCtx.SetOperator(gfxContext::OPERATOR_SOURCE);
    if (mSinglePixel) {
      tmpCtx.SetDeviceColor(mSinglePixelColor);
    } else {
      tmpCtx.SetSource(ThebesSurface(), gfxPoint(aPadding.left, aPadding.top));
    }
    tmpCtx.Rectangle(available);
    tmpCtx.Fill();
    return SurfaceWithFormat(new gfxSurfaceDrawable(surface, size), format);
  }

  // Not tiling, and we have a surface, so we can account for
  // padding and/or a partial decode just by twiddling parameters.
  // First, update our user-space fill rect.
  aSourceRect = aSourceRect.Intersect(available);
  gfxMatrix imageSpaceToUserSpace = aUserSpaceToImageSpace;
  imageSpaceToUserSpace.Invert();
  aFill = imageSpaceToUserSpace.Transform(aSourceRect);

  aSubimage = aSubimage.Intersect(available) - gfxPoint(aPadding.left, aPadding.top);
  aUserSpaceToImageSpace.Multiply(gfxMatrix().Translate(-gfxPoint(aPadding.left, aPadding.top)));
  aSourceRect = aSourceRect - gfxPoint(aPadding.left, aPadding.top);
  aImageRect = gfxRect(0, 0, mSize.width, mSize.height);

  gfxIntSize availableSize(mDecoded.width, mDecoded.height);
  return SurfaceWithFormat(new gfxSurfaceDrawable(ThebesSurface(),
                                                  availableSize),
                           mFormat);
}

void imgFrame::Draw(gfxContext *aContext, gfxPattern::GraphicsFilter aFilter,
                    const gfxMatrix &aUserSpaceToImageSpace, const gfxRect& aFill,
                    const nsIntMargin &aPadding, const nsIntRect &aSubimage)
{
  NS_ASSERTION(!aFill.IsEmpty(), "zero dest size --- fix caller");
  NS_ASSERTION(!aSubimage.IsEmpty(), "zero source size --- fix caller");
  NS_ASSERTION(!mPalettedImageData, "Directly drawing a paletted image!");

  bool doPadding = aPadding != nsIntMargin(0,0,0,0);
  bool doPartialDecode = !ImageComplete();

  if (mSinglePixel && !doPadding && !doPartialDecode) {
    DoSingleColorFastPath(aContext, mSinglePixelColor, aFill);
    return;
  }

  gfxMatrix userSpaceToImageSpace = aUserSpaceToImageSpace;
  gfxRect sourceRect = userSpaceToImageSpace.Transform(aFill);
  gfxRect imageRect(0, 0, mSize.width + aPadding.LeftRight(),
                    mSize.height + aPadding.TopBottom());
  gfxRect subimage(aSubimage.x, aSubimage.y, aSubimage.width, aSubimage.height);
  gfxRect fill = aFill;

  NS_ASSERTION(!sourceRect.Intersect(subimage).IsEmpty(),
               "We must be allowed to sample *some* source pixels!");

  bool doTile = !imageRect.Contains(sourceRect);
  SurfaceWithFormat surfaceResult =
    SurfaceForDrawing(doPadding, doPartialDecode, doTile, aPadding,
                      userSpaceToImageSpace, fill, subimage, sourceRect,
                      imageRect);

  if (surfaceResult.IsValid()) {
    gfxUtils::DrawPixelSnapped(aContext, surfaceResult.mDrawable,
                               userSpaceToImageSpace,
                               subimage, sourceRect, imageRect, fill,
                               surfaceResult.mFormat, aFilter);
  }
}

nsresult imgFrame::Extract(const nsIntRect& aRegion, imgFrame** aResult)
{
  nsAutoPtr<imgFrame> subImage(new imgFrame());
  if (!subImage)
    return NS_ERROR_OUT_OF_MEMORY;

  // The scaling problems described in bug 468496 are especially
  // likely to be visible for the sub-image, as at present the only
  // user is the border-image code and border-images tend to get
  // stretched a lot.  At the same time, the performance concerns
  // that prevent us from just using Cairo's fallback scaler when
  // accelerated graphics won't cut it are less relevant to such
  // images, since they also tend to be small.  Thus, we forcibly
  // disable the use of anything other than a client-side image
  // surface for the sub-image; this ensures that the correct
  // (albeit slower) Cairo fallback scaler will be used.
  subImage->mNeverUseDeviceSurface = PR_TRUE;

  nsresult rv = subImage->Init(0, 0, aRegion.width, aRegion.height, 
                               mFormat, mPaletteDepth);
  NS_ENSURE_SUCCESS(rv, rv);

  // scope to destroy ctx
  {
    gfxContext ctx(subImage->ThebesSurface());
    ctx.SetOperator(gfxContext::OPERATOR_SOURCE);
    if (mSinglePixel) {
      ctx.SetDeviceColor(mSinglePixelColor);
    } else {
      // SetSource() places point (0,0) of its first argument at
      // the coordinages given by its second argument.  We want
      // (x,y) of the image to be (0,0) of source space, so we
      // put (0,0) of the image at (-x,-y).
      ctx.SetSource(this->ThebesSurface(), gfxPoint(-aRegion.x, -aRegion.y));
    }
    ctx.Rectangle(gfxRect(0, 0, aRegion.width, aRegion.height));
    ctx.Fill();
  }

  nsIntRect filled(0, 0, aRegion.width, aRegion.height);

  rv = subImage->ImageUpdated(filled);
  NS_ENSURE_SUCCESS(rv, rv);

  subImage->Optimize();

  *aResult = subImage.forget();

  return NS_OK;
}

nsresult imgFrame::ImageUpdated(const nsIntRect &aUpdateRect)
{
  mDecoded.UnionRect(mDecoded, aUpdateRect);

  // clamp to bounds, in case someone sends a bogus updateRect (I'm looking at
  // you, gif decoder)
  nsIntRect boundsRect(mOffset, mSize);
  mDecoded.IntersectRect(mDecoded, boundsRect);

#ifdef XP_MACOSX
  if (mQuartzSurface)
    mQuartzSurface->Flush();
#endif
  return NS_OK;
}

PRInt32 imgFrame::GetX() const
{
  return mOffset.x;
}

PRInt32 imgFrame::GetY() const
{
  return mOffset.y;
}

PRInt32 imgFrame::GetWidth() const
{
  return mSize.width;
}

PRInt32 imgFrame::GetHeight() const
{
  return mSize.height;
}

nsIntRect imgFrame::GetRect() const
{
  return nsIntRect(mOffset, mSize);
}

gfxASurface::gfxImageFormat imgFrame::GetFormat() const
{
  return mFormat;
}

bool imgFrame::GetNeedsBackground() const
{
  // We need a background painted if we have alpha or we're incomplete.
  return (mFormat == gfxASurface::ImageFormatARGB32 || !ImageComplete());
}

PRUint32 imgFrame::GetImageBytesPerRow() const
{
  if (mImageSurface)
    return mImageSurface->Stride();

  if (mPaletteDepth)
    return mSize.width;

  NS_ERROR("GetImageBytesPerRow called with mImageSurface == null and mPaletteDepth == 0");

  return 0;
}

PRUint32 imgFrame::GetImageDataLength() const
{
  return GetImageBytesPerRow() * mSize.height;
}

void imgFrame::GetImageData(PRUint8 **aData, PRUint32 *length) const
{
  if (mImageSurface)
    *aData = mImageSurface->Data();
  else if (mPalettedImageData)
    *aData = mPalettedImageData + PaletteDataLength();
  else
    *aData = nsnull;

  *length = GetImageDataLength();
}

bool imgFrame::GetIsPaletted() const
{
  return mPalettedImageData != nsnull;
}

bool imgFrame::GetHasAlpha() const
{
  return mFormat == gfxASurface::ImageFormatARGB32;
}

void imgFrame::GetPaletteData(PRUint32 **aPalette, PRUint32 *length) const
{
  if (!mPalettedImageData) {
    *aPalette = nsnull;
    *length = 0;
  } else {
    *aPalette = (PRUint32 *) mPalettedImageData;
    *length = PaletteDataLength();
  }
}

nsresult imgFrame::LockImageData()
{
  if (mPalettedImageData)
    return NS_ERROR_NOT_AVAILABLE;

  NS_ABORT_IF_FALSE(!mLocked, "Trying to lock already locked image data.");
  if (mLocked) {
    return NS_ERROR_FAILURE;
  }
  mLocked = PR_TRUE;

  if ((mOptSurface || mSinglePixel) && !mImageSurface) {
    // Recover the pixels
    mImageSurface = new gfxImageSurface(gfxIntSize(mSize.width, mSize.height),
                                        gfxImageSurface::ImageFormatARGB32);
    if (!mImageSurface || mImageSurface->CairoStatus())
      return NS_ERROR_OUT_OF_MEMORY;

    gfxContext context(mImageSurface);
    context.SetOperator(gfxContext::OPERATOR_SOURCE);
    if (mSinglePixel)
      context.SetDeviceColor(mSinglePixelColor);
    else
      context.SetSource(mOptSurface);
    context.Paint();

    mOptSurface = nsnull;
#ifdef USE_WIN_SURFACE
    mWinSurface = nsnull;
#endif
#ifdef XP_MACOSX
    mQuartzSurface = nsnull;
#endif
  }

  // We might write to the bits in this image surface, so we need to make the
  // surface ready for that.
  if (mImageSurface)
    mImageSurface->Flush();

#ifdef USE_WIN_SURFACE
  if (mWinSurface)
    mWinSurface->Flush();
#endif

  return NS_OK;
}

nsresult imgFrame::UnlockImageData()
{
  if (mPalettedImageData)
    return NS_ERROR_NOT_AVAILABLE;

  NS_ABORT_IF_FALSE(mLocked, "Unlocking an unlocked image!");
  if (!mLocked) {
    return NS_ERROR_FAILURE;
  }

  mLocked = PR_FALSE;

  // Assume we've been written to.
  if (mImageSurface)
    mImageSurface->MarkDirty();

#ifdef USE_WIN_SURFACE
  if (mWinSurface)
    mWinSurface->MarkDirty();
#endif

#ifdef XP_MACOSX
  // The quartz image surface (ab)uses the flush method to get the
  // cairo_image_surface data into a CGImage, so we have to call Flush() here.
  if (mQuartzSurface)
    mQuartzSurface->Flush();
#endif
  return NS_OK;
}

PRInt32 imgFrame::GetTimeout() const
{
  // Ensure a minimal time between updates so we don't throttle the UI thread.
  // consider 0 == unspecified and make it fast but not too fast.  See bug
  // 125137, bug 139677, and bug 207059.  The behavior of recent IE and Opera
  // versions seems to be:
  // IE 6/Win:
  //   10 - 50ms go 100ms
  //   >50ms go correct speed
  // Opera 7 final/Win:
  //   10ms goes 100ms
  //   >10ms go correct speed
  // It seems that there are broken tools out there that set a 0ms or 10ms
  // timeout when they really want a "default" one.  So munge values in that
  // range.
  if (mTimeout >= 0 && mTimeout <= 10)
    return 100;
  else
    return mTimeout;
}

void imgFrame::SetTimeout(PRInt32 aTimeout)
{
  mTimeout = aTimeout;
}

PRInt32 imgFrame::GetFrameDisposalMethod() const
{
  return mDisposalMethod;
}

void imgFrame::SetFrameDisposalMethod(PRInt32 aFrameDisposalMethod)
{
  mDisposalMethod = aFrameDisposalMethod;
}

PRInt32 imgFrame::GetBlendMethod() const
{
  return mBlendMethod;
}

void imgFrame::SetBlendMethod(PRInt32 aBlendMethod)
{
  mBlendMethod = (PRInt8)aBlendMethod;
}

bool imgFrame::ImageComplete() const
{
  return mDecoded.IsEqualInterior(nsIntRect(mOffset, mSize));
}

// A hint from the image decoders that this image has no alpha, even
// though we created is ARGB32.  This changes our format to RGB24,
// which in turn will cause us to Optimize() to RGB24.  Has no effect
// after Optimize() is called, though in all cases it will be just a
// performance win -- the pixels are still correct and have the A byte
// set to 0xff.
void imgFrame::SetHasNoAlpha()
{
  if (mFormat == gfxASurface::ImageFormatARGB32) {
      mFormat = gfxASurface::ImageFormatRGB24;
      mFormatChanged = PR_TRUE;
  }
}

bool imgFrame::GetCompositingFailed() const
{
  return mCompositingFailed;
}

void imgFrame::SetCompositingFailed(bool val)
{
  mCompositingFailed = val;
}

PRUint32
imgFrame::EstimateMemoryUsed(gfxASurface::MemoryLocation aLocation) const
{
  PRUint32 size = 0;

  if (mSinglePixel && aLocation == gfxASurface::MEMORY_IN_PROCESS_HEAP) {
    size += sizeof(gfxRGBA);
  }

  if (mPalettedImageData && aLocation == gfxASurface::MEMORY_IN_PROCESS_HEAP) {
    size += GetImageDataLength() + PaletteDataLength();
  }

#ifdef USE_WIN_SURFACE
  if (mWinSurface && aLocation == mWinSurface->GetMemoryLocation()) {
    size += mWinSurface->KnownMemoryUsed();
  } else
#endif
#ifdef XP_MACOSX
  if (mQuartzSurface && aLocation == gfxASurface::MEMORY_IN_PROCESS_HEAP) {
    size += mSize.width * mSize.height * 4;
  } else
#endif
  if (mImageSurface && aLocation == mImageSurface->GetMemoryLocation()) {
    size += mImageSurface->KnownMemoryUsed();
  }

  if (mOptSurface && aLocation == mOptSurface->GetMemoryLocation()) {
    size += mOptSurface->KnownMemoryUsed();
  }

  return size;
}
