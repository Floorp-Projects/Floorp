/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_WidgetUtils_h
#define mozilla_WidgetUtils_h

#include "gfxMatrix.h"

namespace mozilla {

// NB: these must match up with pseudo-enum in nsIScreen.idl.
enum ScreenRotation {
  ROTATION_0 = 0,
  ROTATION_90,
  ROTATION_180,
  ROTATION_270,

  ROTATION_COUNT
};

gfxMatrix ComputeGLTransformForRotation(const nsIntRect& aBounds,
                                        ScreenRotation aRotation);

} // namespace mozilla

#endif // mozilla_WidgetUtils_h
