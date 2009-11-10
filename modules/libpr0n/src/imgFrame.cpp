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
 * The Initial Developer of the Original Code is Mozilla Corporation.
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

static PRBool gDisableOptimize = PR_FALSE;

/*XXX get CAIRO_HAS_DDRAW_SURFACE */
#include "cairo.h"

#ifdef CAIRO_HAS_DDRAW_SURFACE
#include "gfxDDrawSurface.h"
#endif

#if defined(XP_WIN) || defined(WINCE)
#include "gfxWindowsPlatform.h"
#endif

#if defined(XP_WIN) && !defined(WINCE)

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
static PRBool AllowedImageSize(PRInt32 aWidth, PRInt32 aHeight)
{
  NS_ASSERTION(aWidth > 0, "invalid image width");
  NS_ASSERTION(aHeight > 0, "invalid image height");

  // reject over-wide or over-tall images
  const PRInt32 k64KLimit = 0x0000FFFF;
  if (NS_UNLIKELY(aWidth > k64KLimit || aHeight > k64KLimit )) {
    NS_WARNING("image too big");
    return PR_FALSE;
  }

  // protect against division by zero - this really shouldn't happen
  // if our consumers were well behaved, but they aren't (bug 368427)
  if (NS_UNLIKELY(aHeight == 0)) {
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
static PRBool ShouldUseImageSurfaces()
{
#if defined(WINCE)
  // There is no test on windows mobile to check for Gui resources.
  // Allocate, until we run out of memory.
  gfxWindowsPlatform::RenderMode rmode = gfxWindowsPlatform::GetPlatform()->GetRenderMode();
  return rmode != gfxWindowsPlatform::RENDER_DDRAW &&
      rmode != gfxWindowsPlatform::RENDER_DDRAW_GL;

#elif defined(USE_WIN_SURFACE)
  static const DWORD kGDIObjectsHighWaterMark = 7000;

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
  mFormatChanged(PR_FALSE)
#ifdef USE_WIN_SURFACE
  , mIsDDBSurface(PR_FALSE)
#endif
{
  static PRBool hasCheckedOptimize = PR_FALSE;
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
                        gfxASurface::gfxImageFormat aFormat, PRInt8 aPaletteDepth /* = 0 */)
{
  // assert for properties that should be verified by decoders, warn for properties related to bad content
  if (!AllowedImageSize(aWidth, aHeight))
    return NS_ERROR_FAILURE;

  // Check to see if we are running OOM
  nsCOMPtr<nsIMemory> mem;
  NS_GetMemoryManager(getter_AddRefs(mem));
  if (!mem)
    return NS_ERROR_UNEXPECTED;

  PRBool lowMemory;
  mem->IsLowMemory(&lowMemory);
  if (lowMemory)
    return NS_ERROR_OUT_OF_MEMORY;

  mOffset.MoveTo(aX, aY);
  mSize.SizeTo(aWidth, aHeight);

  mFormat = aFormat;
  mPaletteDepth = aPaletteDepth;

  if (aPaletteDepth != 0) {
    // We're creating for a paletted image.
    if (aPaletteDepth > 8) {
      NS_ERROR("This Depth is not supported\n");
      return NS_ERROR_FAILURE;
    }

    mPalettedImageData = (PRUint8*)PR_MALLOC(PaletteDataLength() + GetImageDataLength());
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
        mImageSurface = mWinSurface->GetImageSurface();
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

static PRBool                                                                                                       
IsSafeImageTransformComponent(gfxFloat aValue)
{
  return aValue >= -32768 && aValue <= 32767;
}

void imgFrame::Draw(gfxContext *aContext, gfxPattern::GraphicsFilter aFilter,
                    const gfxMatrix &aUserSpaceToImageSpace, const gfxRect& aFill,
                    const nsIntMargin &aPadding, const nsIntRect &aSubimage)
{
  NS_ASSERTION(!aFill.IsEmpty(), "zero dest size --- fix caller");
  NS_ASSERTION(!aSubimage.IsEmpty(), "zero source size --- fix caller");
  NS_ASSERTION(!mPalettedImageData, "Directly drawing a paletted image!");

  PRBool doPadding = aPadding != nsIntMargin(0,0,0,0);
  PRBool doPartialDecode = !ImageComplete();
  gfxContext::GraphicsOperator op = aContext->CurrentOperator();

  if (mSinglePixel && !doPadding && ImageComplete()) {
    // Single-color fast path
    // if a == 0, it's a noop
    if (mSinglePixelColor.a == 0.0)
      return;

    if (op == gfxContext::OPERATOR_OVER && mSinglePixelColor.a == 1.0)
      aContext->SetOperator(gfxContext::OPERATOR_SOURCE);

    aContext->SetDeviceColor(mSinglePixelColor);
    aContext->NewPath();
    aContext->Rectangle(aFill);
    aContext->Fill();
    aContext->SetOperator(op);
    aContext->SetDeviceColor(gfxRGBA(0,0,0,0));
    return;
  }

  gfxMatrix userSpaceToImageSpace = aUserSpaceToImageSpace;
  gfxRect sourceRect = userSpaceToImageSpace.Transform(aFill);
  gfxRect imageRect(0, 0, mSize.width + aPadding.LeftRight(), mSize.height + aPadding.TopBottom());
  gfxRect subimage(aSubimage.x, aSubimage.y, aSubimage.width, aSubimage.height);
  gfxRect fill = aFill;
  nsRefPtr<gfxASurface> surface;
  gfxImageSurface::gfxImageFormat format;

  NS_ASSERTION(!sourceRect.Intersect(subimage).IsEmpty(),
               "We must be allowed to sample *some* source pixels!");

  PRBool doTile = !imageRect.Contains(sourceRect);
  if (doPadding || doPartialDecode) {
    gfxRect available = gfxRect(mDecoded.x, mDecoded.y, mDecoded.width, mDecoded.height) +
      gfxPoint(aPadding.left, aPadding.top);

    if (!doTile && !mSinglePixel) {
      // Not tiling, and we have a surface, so we can account for
      // padding and/or a partial decode just by twiddling parameters.
      // First, update our user-space fill rect.
      sourceRect = sourceRect.Intersect(available);
      gfxMatrix imageSpaceToUserSpace = userSpaceToImageSpace;
      imageSpaceToUserSpace.Invert();
      fill = imageSpaceToUserSpace.Transform(sourceRect);

      surface = ThebesSurface();
      format = mFormat;
      subimage = subimage.Intersect(available) - gfxPoint(aPadding.left, aPadding.top);
      userSpaceToImageSpace.Multiply(gfxMatrix().Translate(-gfxPoint(aPadding.left, aPadding.top)));
      sourceRect = sourceRect - gfxPoint(aPadding.left, aPadding.top);
      imageRect = gfxRect(0, 0, mSize.width, mSize.height);
    } else {
      // Create a temporary surface
      gfxIntSize size(PRInt32(imageRect.Width()), PRInt32(imageRect.Height()));
      // Give this surface an alpha channel because there are
      // transparent pixels in the padding or undecoded area
      format = gfxASurface::ImageFormatARGB32;
      surface = gfxPlatform::GetPlatform()->CreateOffscreenSurface(size, format);
      if (!surface || surface->CairoStatus() != 0)
        return;

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
    }
  } else {
    NS_ASSERTION(!mSinglePixel, "This should already have been handled");
    surface = ThebesSurface();
    format = mFormat;
  }
  // At this point, we've taken care of mSinglePixel images, images with
  // aPadding, and partially-decoded images.

  // Compute device-space-to-image-space transform. We need to sanity-
  // check it to work around a pixman bug :-(
  // XXX should we only do this for certain surface types?
  gfxFloat deviceX, deviceY;
  nsRefPtr<gfxASurface> currentTarget =
    aContext->CurrentSurface(&deviceX, &deviceY);
  gfxMatrix currentMatrix = aContext->CurrentMatrix();
  gfxMatrix deviceToUser = currentMatrix;
  deviceToUser.Invert();
  deviceToUser.Translate(-gfxPoint(-deviceX, -deviceY));
  gfxMatrix deviceToImage = deviceToUser;
  deviceToImage.Multiply(userSpaceToImageSpace);

  PRBool pushedGroup = PR_FALSE;
  if (currentTarget->GetType() != gfxASurface::SurfaceTypeQuartz) {
    // BEGIN working around cairo/pixman bug (bug 364968)
    // Quartz's limits for matrix are much larger than pixman
      
    // Our device-space-to-image-space transform may not be acceptable to pixman.
    if (!IsSafeImageTransformComponent(deviceToImage.xx) ||
        !IsSafeImageTransformComponent(deviceToImage.xy) ||
        !IsSafeImageTransformComponent(deviceToImage.yx) ||
        !IsSafeImageTransformComponent(deviceToImage.yy)) {
      NS_WARNING("Scaling up too much, bailing out");
      return;
    }

    if (!IsSafeImageTransformComponent(deviceToImage.x0) ||
        !IsSafeImageTransformComponent(deviceToImage.y0)) {
      // We'll push a group, which will hopefully reduce our transform's
      // translation so it's in bounds
      aContext->Save();

      // Clip the rounded-out-to-device-pixels bounds of the
      // transformed fill area. This is the area for the group we
      // want to push.
      aContext->IdentityMatrix();
      gfxRect bounds = currentMatrix.TransformBounds(fill);
      bounds.RoundOut();
      aContext->Clip(bounds);
      aContext->SetMatrix(currentMatrix);

      aContext->PushGroup(gfxASurface::CONTENT_COLOR_ALPHA);
      aContext->SetOperator(gfxContext::OPERATOR_OVER);
      pushedGroup = PR_TRUE;
    }
    // END working around cairo/pixman bug (bug 364968)
  }

  nsRefPtr<gfxPattern> pattern = new gfxPattern(surface);
  pattern->SetMatrix(userSpaceToImageSpace);

  // OK now, the hard part left is to account for the subimage sampling
  // restriction. If all the transforms involved are just integer
  // translations, then we assume no resampling will occur so there's
  // nothing to do.
  // XXX if only we had source-clipping in cairo!
  if (!currentMatrix.HasNonIntegerTranslation() &&
      !userSpaceToImageSpace.HasNonIntegerTranslation()) {
    if (doTile) {
      pattern->SetExtend(gfxPattern::EXTEND_REPEAT);
    }
  } else {
    if (doTile || !subimage.Contains(imageRect)) {
      // EXTEND_PAD won't help us here; we have to create a temporary
      // surface to hold the subimage of pixels we're allowed to
      // sample

      gfxRect userSpaceClipExtents = aContext->GetClipExtents();
      // This isn't optimal --- if aContext has a rotation then GetClipExtents
      // will have to do a bounding-box computation, and TransformBounds might
      // too, so we could get a better result if we computed image space clip
      // extents in one go --- but it doesn't really matter and this is easier
      // to understand.
      gfxRect imageSpaceClipExtents = userSpaceToImageSpace.TransformBounds(userSpaceClipExtents);
      // Inflate by one pixel because bilinear filtering will sample at most
      // one pixel beyond the computed image pixel coordinate.
      imageSpaceClipExtents.Outset(1.0);

      gfxRect needed = imageSpaceClipExtents.Intersect(sourceRect).Intersect(subimage);
      needed.RoundOut();

      // if 'needed' is empty, nothing will be drawn since aFill
      // must be entirely outside the clip region, so it doesn't
      // matter what we do here, but we should avoid trying to
      // create a zero-size surface.
      if (!needed.IsEmpty()) {
        gfxIntSize size(PRInt32(needed.Width()), PRInt32(needed.Height()));
        nsRefPtr<gfxASurface> temp =
          gfxPlatform::GetPlatform()->CreateOffscreenSurface(size, format);
        if (temp && temp->CairoStatus() == 0) {
          gfxContext tmpCtx(temp);
          tmpCtx.SetOperator(gfxContext::OPERATOR_SOURCE);
          nsRefPtr<gfxPattern> tmpPattern = new gfxPattern(surface);
          if (tmpPattern) {
            tmpPattern->SetExtend(gfxPattern::EXTEND_REPEAT);
            tmpPattern->SetMatrix(gfxMatrix().Translate(needed.pos));
            tmpCtx.SetPattern(tmpPattern);
            tmpCtx.Paint();
            tmpPattern = new gfxPattern(temp);
            if (tmpPattern) {
              pattern.swap(tmpPattern);
              pattern->SetMatrix(
                  gfxMatrix(userSpaceToImageSpace).Multiply(gfxMatrix().Translate(-needed.pos)));
            }
          }
        }
      }
    }

    // In theory we can handle this using cairo's EXTEND_PAD,
    // but implementation limitations mean we have to consult
    // the surface type.
    switch (currentTarget->GetType()) {
      case gfxASurface::SurfaceTypeXlib:
      case gfxASurface::SurfaceTypeXcb:
      {
        // See bug 324698.  This is a workaround for EXTEND_PAD not being
        // implemented correctly on linux in the X server.
        //
        // Set the filter to CAIRO_FILTER_FAST --- otherwise,
        // pixman's sampling will sample transparency for the outside edges and we'll
        // get blurry edges.  CAIRO_EXTEND_PAD would also work here, if
        // available
        //
        // But don't do this for simple downscales because it's horrible.
        // Downscaling means that device-space coordinates are
        // scaled *up* to find the image pixel coordinates.
        //
        // deviceToImage is slightly stale because up above we may
        // have adjusted the pattern's matrix ... but the adjustment
        // is only a translation so the scale factors in deviceToImage
        // are still valid.
        PRBool isDownscale =
          deviceToImage.xx >= 1.0 && deviceToImage.yy >= 1.0 &&
          deviceToImage.xy == 0.0 && deviceToImage.yx == 0.0;
        if (!isDownscale) {
          pattern->SetFilter(gfxPattern::FILTER_FAST);
        }
        break;
      }

      case gfxASurface::SurfaceTypeQuartz:
      case gfxASurface::SurfaceTypeQuartzImage:
        // Don't set EXTEND_PAD, Mac seems to be OK. Really?
        pattern->SetFilter(aFilter);
        break;

      default:
        // turn on EXTEND_PAD.
        // This is what we really want for all surface types, if the
        // implementation was universally good.
        pattern->SetExtend(gfxPattern::EXTEND_PAD);
        pattern->SetFilter(aFilter);
        break;
    }
  }

  if ((op == gfxContext::OPERATOR_OVER || pushedGroup) &&
      format == gfxASurface::ImageFormatRGB24) {
    aContext->SetOperator(gfxContext::OPERATOR_SOURCE);
  }

  // Phew! Now we can actually draw this image
  aContext->NewPath();
#ifdef MOZ_GFX_OPTIMIZE_MOBILE
  pattern->SetFilter(gfxPattern::FILTER_FAST); 
#endif
  aContext->SetPattern(pattern);
  aContext->Rectangle(fill);
  aContext->Fill();

  aContext->SetOperator(op);
  if (pushedGroup) {
    aContext->PopGroupToSource();
    aContext->Paint();
    aContext->Restore();
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
  // Check to see if we are running OOM
  nsCOMPtr<nsIMemory> mem;
  NS_GetMemoryManager(getter_AddRefs(mem));
  if (!mem)
    return NS_ERROR_UNEXPECTED;

  PRBool lowMemory;
  mem->IsLowMemory(&lowMemory);
  if (lowMemory)
    return NS_ERROR_OUT_OF_MEMORY;

  mDecoded.UnionRect(mDecoded, aUpdateRect);

  // clamp to bounds, in case someone sends a bogus updateRect (I'm looking at
  // you, gif decoder)
  nsIntRect boundsRect(0, 0, mSize.width, mSize.height);
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

PRBool imgFrame::GetNeedsBackground() const
{
  // We need a background painted if we have alpha or we're incomplete.
  return (mFormat == gfxASurface::ImageFormatARGB32 || !ImageComplete());
}

PRUint32 imgFrame::GetImageBytesPerRow() const
{
  if (mImageSurface)
    return mImageSurface->Stride();
  else
    return mSize.width;
}

PRUint32 imgFrame::GetImageDataLength() const
{
  if (mImageSurface)
    return mImageSurface->Stride() * mSize.height;
  else
    return mSize.width * mSize.height;
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

PRBool imgFrame::GetIsPaletted() const
{
  return mPalettedImageData != nsnull;
}

PRBool imgFrame::GetHasAlpha() const
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
    return NS_OK;

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

  return NS_OK;
}

nsresult imgFrame::UnlockImageData()
{
  if (mPalettedImageData)
    return NS_OK;

#ifdef XP_MACOSX
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

PRBool imgFrame::ImageComplete() const
{
  return mDecoded == nsIntRect(0, 0, mSize.width, mSize.height);
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
