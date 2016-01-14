/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsScreenWin_h___
#define nsScreenWin_h___

#include <windows.h>
#include "nsBaseScreen.h"

//------------------------------------------------------------------------

class nsScreenWin final : public nsBaseScreen
{
public:
  nsScreenWin ( HMONITOR inScreen );
  ~nsScreenWin();

  NS_IMETHOD GetId(uint32_t* aId);

  // These methods return the size in device (physical) pixels
  NS_IMETHOD GetRect(int32_t* aLeft, int32_t* aTop, int32_t* aWidth, int32_t* aHeight);
  NS_IMETHOD GetAvailRect(int32_t* aLeft, int32_t* aTop, int32_t* aWidth, int32_t* aHeight);

  // And these methods get the screen size in 'desktop pixels' (AKA 'logical pixels')
  // that are dependent on the logical DPI setting in windows
  NS_IMETHOD GetRectDisplayPix(int32_t *outLeft,  int32_t *outTop,
                               int32_t *outWidth, int32_t *outHeight);
  NS_IMETHOD GetAvailRectDisplayPix(int32_t *outLeft,  int32_t *outTop,
                                    int32_t *outWidth, int32_t *outHeight);

  NS_IMETHOD GetPixelDepth(int32_t* aPixelDepth);
  NS_IMETHOD GetColorDepth(int32_t* aColorDepth);

  NS_IMETHOD GetContentsScaleFactor(double *aContentsScaleFactor) override;

private:
  double GetDPIScale();

  HMONITOR mScreen;
  uint32_t mId;
};

#endif  // nsScreenWin_h___ 
