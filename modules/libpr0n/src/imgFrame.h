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

#ifndef imgFrame_h
#define imgFrame_h

#include "nsRect.h"
#include "nsPoint.h"
#include "nsSize.h"
#include "gfxTypes.h"
#include "nsID.h"
#include "gfxContext.h"
#include "gfxPattern.h"
#include "gfxDrawable.h"
#include "gfxImageSurface.h"
#if defined(XP_WIN)
#include "gfxWindowsSurface.h"
#elif defined(XP_MACOSX)
#include "gfxQuartzImageSurface.h"
#endif
#include "nsAutoPtr.h"

class imgFrame
{
public:
  imgFrame();
  ~imgFrame();

  nsresult Init(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight, gfxASurface::gfxImageFormat aFormat, PRUint8 aPaletteDepth = 0);
  nsresult Optimize();

  void Draw(gfxContext *aContext, gfxPattern::GraphicsFilter aFilter,
            const gfxMatrix &aUserSpaceToImageSpace, const gfxRect& aFill,
            const nsIntMargin &aPadding, const nsIntRect &aSubimage);

  nsresult Extract(const nsIntRect& aRegion, imgFrame** aResult);

  nsresult ImageUpdated(const nsIntRect &aUpdateRect);

  PRInt32 GetX() const;
  PRInt32 GetY() const;
  PRInt32 GetWidth() const;
  PRInt32 GetHeight() const;
  nsIntRect GetRect() const;
  gfxASurface::gfxImageFormat GetFormat() const;
  PRBool GetNeedsBackground() const;
  PRUint32 GetImageBytesPerRow() const;
  PRUint32 GetImageDataLength() const;
  PRBool GetIsPaletted() const;
  PRBool GetHasAlpha() const;
  void GetImageData(PRUint8 **aData, PRUint32 *length) const;
  void GetPaletteData(PRUint32 **aPalette, PRUint32 *length) const;

  PRInt32 GetTimeout() const;
  void SetTimeout(PRInt32 aTimeout);

  PRInt32 GetFrameDisposalMethod() const;
  void SetFrameDisposalMethod(PRInt32 aFrameDisposalMethod);
  PRInt32 GetBlendMethod() const;
  void SetBlendMethod(PRInt32 aBlendMethod);
  PRBool ImageComplete() const;

  void SetHasNoAlpha();

  PRBool GetCompositingFailed() const;
  void SetCompositingFailed(PRBool val);

  nsresult LockImageData();
  nsresult UnlockImageData();

  nsresult GetSurface(gfxASurface **aSurface) const
  {
    *aSurface = ThebesSurface();
    NS_IF_ADDREF(*aSurface);
    return NS_OK;
  }

  nsresult GetPattern(gfxPattern **aPattern) const
  {
    if (mSinglePixel)
      *aPattern = new gfxPattern(mSinglePixelColor);
    else
      *aPattern = new gfxPattern(ThebesSurface());
    NS_ADDREF(*aPattern);
    return NS_OK;
  }

  gfxASurface* ThebesSurface() const
  {
    if (mOptSurface)
      return mOptSurface;
#if defined(XP_WIN)
    if (mWinSurface)
      return mWinSurface;
#elif defined(XP_MACOSX)
    if (mQuartzSurface)
      return mQuartzSurface;
#endif
    return mImageSurface;
  }

  PRUint32 EstimateMemoryUsed(gfxASurface::MemoryLocation aLocation) const;

  PRUint8 GetPaletteDepth() const { return mPaletteDepth; }

private: // methods
  PRUint32 PaletteDataLength() const {
    return ((1 << mPaletteDepth) * sizeof(PRUint32));
  }

  struct SurfaceWithFormat {
    nsRefPtr<gfxDrawable> mDrawable;
    gfxImageSurface::gfxImageFormat mFormat;
    SurfaceWithFormat() {}
    SurfaceWithFormat(gfxDrawable* aDrawable, gfxImageSurface::gfxImageFormat aFormat)
     : mDrawable(aDrawable), mFormat(aFormat) {}
    PRBool IsValid() { return !!mDrawable; }
  };

  SurfaceWithFormat SurfaceForDrawing(PRBool             aDoPadding,
                                      PRBool             aDoPartialDecode,
                                      PRBool             aDoTile,
                                      const nsIntMargin& aPadding,
                                      gfxMatrix&         aUserSpaceToImageSpace,
                                      gfxRect&           aFill,
                                      gfxRect&           aSubimage,
                                      gfxRect&           aSourceRect,
                                      gfxRect&           aImageRect);

private: // data
  nsRefPtr<gfxImageSurface> mImageSurface;
  nsRefPtr<gfxASurface> mOptSurface;
#if defined(XP_WIN)
  nsRefPtr<gfxWindowsSurface> mWinSurface;
#elif defined(XP_MACOSX)
  nsRefPtr<gfxQuartzImageSurface> mQuartzSurface;
#endif

  nsIntSize    mSize;
  nsIntPoint   mOffset;

  nsIntRect    mDecoded;

  // The palette and image data for images that are paletted, since Cairo
  // doesn't support these images.
  // The paletted data comes first, then the image data itself.
  // Total length is PaletteDataLength() + GetImageDataLength().
  PRUint8*     mPalettedImageData;

  gfxRGBA      mSinglePixelColor;

  PRInt32      mTimeout; // -1 means display forever
  PRInt32      mDisposalMethod;

  gfxASurface::gfxImageFormat mFormat;
  PRUint8      mPaletteDepth;
  PRInt8       mBlendMethod;
  PRPackedBool mSinglePixel;
  PRPackedBool mNeverUseDeviceSurface;
  PRPackedBool mFormatChanged;
  PRPackedBool mCompositingFailed;
  /** Indicates if the image data is currently locked */
  PRPackedBool mLocked;

#ifdef XP_WIN
  PRPackedBool mIsDDBSurface;
#endif

};

#endif /* imgFrame_h */
