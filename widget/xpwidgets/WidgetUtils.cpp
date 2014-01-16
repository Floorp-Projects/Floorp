/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/WidgetUtils.h"

namespace mozilla {

gfx::Matrix
ComputeTransformForRotation(const nsIntRect& aBounds,
                              ScreenRotation aRotation)
{
    gfx::Matrix transform;
    switch (aRotation) {
    case ROTATION_0:
        break;
    case ROTATION_90:
        transform.Translate(aBounds.width, 0);
        transform = gfx::Matrix::Rotation(M_PI / 2) * transform;
        break;
    case ROTATION_180:
        transform.Translate(aBounds.width, aBounds.height);
        transform = gfx::Matrix::Rotation(M_PI) * transform;
        break;
    case ROTATION_270:
        transform.Translate(0, aBounds.height);
        transform = gfx::Matrix::Rotation(M_PI * 3 / 2) * transform;
        break;
    default:
        MOZ_CRASH("Unknown rotation");
    }
    return transform;
}

} // namespace mozilla
