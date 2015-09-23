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

class nsWindowGfx {
public:
  enum IconSizeType {
    kSmallIcon,
    kRegularIcon
  };
  static mozilla::gfx::IntSize GetIconMetrics(IconSizeType aSizeType);
  static nsresult CreateIcon(imgIContainer *aContainer, bool aIsCursor,
                             uint32_t aHotspotX, uint32_t aHotspotY,
                             mozilla::gfx::IntSize aScaledSize, HICON *aIcon);

private:
  /**
   * Cursor helpers
   */
  static uint8_t*         Data32BitTo1Bit(uint8_t* aImageData, uint32_t aWidth, uint32_t aHeight);
  static HBITMAP          DataToBitmap(uint8_t* aImageData, uint32_t aWidth, uint32_t aHeight, uint32_t aDepth);
};

#endif // WindowGfx_h__
