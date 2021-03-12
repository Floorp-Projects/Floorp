/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNativeBasicThemeWin_h
#define nsNativeBasicThemeWin_h

#include "nsNativeBasicTheme.h"

class nsNativeBasicThemeWin : public nsNativeBasicTheme {
 public:
  nsNativeBasicThemeWin() = default;

  Transparency GetWidgetTransparency(nsIFrame* aFrame,
                                     StyleAppearance aAppearance) override;

 protected:
  virtual ~nsNativeBasicThemeWin() = default;
};

#endif
