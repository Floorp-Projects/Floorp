/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WindowGfx_h__
#define WindowGfx_h__

/*
 * nsWindowGfx - Painting and aceleration.
 */

#include "nsWindow.h"
#include <imgIContainer.h>

// This isn't ideal, we should figure out how to export
// the #defines here; need this to figure out if we have
// the DirectDraw surface or not.
#include "cairo-features.h"

class nsWindowGfx {
public:
  static nsIntRect ToIntRect(const RECT& aRect)
  {
    return nsIntRect(aRect.left, aRect.top,
                     aRect.right - aRect.left, aRect.bottom - aRect.top);
  }

  static nsIntRegion ConvertHRGNToRegion(HRGN aRgn);

  enum IconSizeType {
    kSmallIcon,
    kRegularIcon
  };
  static gfxIntSize GetIconMetrics(IconSizeType aSizeType);
  static nsresult CreateIcon(imgIContainer *aContainer, bool aIsCursor, PRUint32 aHotspotX, PRUint32 aHotspotY, gfxIntSize aScaledSize, HICON *aIcon);

private:
  /**
   * Cursor helpers
   */
  static PRUint8*         Data32BitTo1Bit(PRUint8* aImageData, PRUint32 aWidth, PRUint32 aHeight);
  static HBITMAP          DataToBitmap(PRUint8* aImageData, PRUint32 aWidth, PRUint32 aHeight, PRUint32 aDepth);
};

#endif // WindowGfx_h__
