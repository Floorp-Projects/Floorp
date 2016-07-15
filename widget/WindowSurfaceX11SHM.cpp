/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WindowSurfaceX11SHM.h"

namespace mozilla {
namespace widget {

WindowSurfaceX11SHM::WindowSurfaceX11SHM(Display* aDisplay, Drawable aWindow,
                                         Visual* aVisual, unsigned int aDepth)
{
  mFrontImage = new nsShmImage(aDisplay, aWindow, aVisual, aDepth);
  mBackImage = new nsShmImage(aDisplay, aWindow, aVisual, aDepth);
}

already_AddRefed<gfx::DrawTarget>
WindowSurfaceX11SHM::Lock(const LayoutDeviceIntRegion& aRegion)
{
  mBackImage.swap(mFrontImage);
  return mBackImage->CreateDrawTarget(aRegion);
}

void
WindowSurfaceX11SHM::Commit(const LayoutDeviceIntRegion& aInvalidRegion)
{
  mBackImage->Put(aInvalidRegion);
}

} // namespace widget
} // namespace mozilla
