/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/WidgetUtils.h"

namespace mozilla {

gfxMatrix
ComputeTransformForRotation(const nsIntRect& aBounds,
                              ScreenRotation aRotation)
{
    gfxMatrix transform;
    switch (aRotation) {
    case ROTATION_0:
        break;
    case ROTATION_90:
        transform.Translate(gfxPoint(aBounds.width, 0));
        transform.Rotate(M_PI / 2);
        break;
    case ROTATION_180:
        transform.Translate(gfxPoint(aBounds.width, aBounds.height));
        transform.Rotate(M_PI);
        break;
    case ROTATION_270:
        transform.Translate(gfxPoint(0, aBounds.height));
        transform.Rotate(M_PI * 3 / 2);
        break;
    default:
        MOZ_CRASH("Unknown rotation");
    }
    return transform;
}

} // namespace mozilla
