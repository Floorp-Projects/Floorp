/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCocoaFeatures_h_
#define nsCocoaFeatures_h_

#include "mozilla/StandardInteger.h"

class nsCocoaFeatures {
public:
  static int32_t OSXVersion();
  static bool OnLionOrLater();
  static bool OnMountainLionOrLater();
  static bool SupportCoreAnimationPlugins();

private:
  static int32_t mOSXVersion;
};
#endif // nsCocoaFeatures_h_
