/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsBaseScreen_h
#define nsBaseScreen_h

#include "mozilla/Attributes.h"
#include "nsIScreen.h"

class nsBaseScreen : public nsIScreen {
 public:
  nsBaseScreen();

  NS_DECL_ISUPPORTS

  // nsIScreen interface

  // These simply forward to the device-pixel versions;
  // implementations where desktop pixels may not correspond
  // to per-screen device pixels must override.
  NS_IMETHOD GetRectDisplayPix(int32_t* outLeft, int32_t* outTop,
                               int32_t* outWidth, int32_t* outHeight) override;
  NS_IMETHOD GetAvailRectDisplayPix(int32_t* outLeft, int32_t* outTop,
                                    int32_t* outWidth,
                                    int32_t* outHeight) override;

  NS_IMETHOD GetContentsScaleFactor(double* aContentsScaleFactor) override;

  NS_IMETHOD GetDefaultCSSScaleFactor(double* aScaleFactor) override;

  NS_IMETHOD GetDpi(float* aDPI) override;

 protected:
  virtual ~nsBaseScreen();
};

#endif  // nsBaseScreen_h
