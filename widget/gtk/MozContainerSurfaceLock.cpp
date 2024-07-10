/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MozContainerSurfaceLock.h"
#include "MozContainer.h"
#include "WidgetUtilsGtk.h"

using namespace mozilla::widget;

MozContainerSurfaceLock::MozContainerSurfaceLock(MozContainer* aContainer) {
#ifdef MOZ_WAYLAND
  mContainer = aContainer;
  if (GdkIsWaylandDisplay()) {
    // mSurface can be nullptr if we lock hidden MozContainer and
    // that's correct, MozContainer is still locked.
    mSurface = moz_container_wayland_surface_lock(aContainer);
  }
#endif
}

MozContainerSurfaceLock::~MozContainerSurfaceLock() {
#ifdef MOZ_WAYLAND
  if (GdkIsWaylandDisplay()) {
    moz_container_wayland_surface_unlock(mContainer, &mSurface);
  }
#endif
}

struct wl_surface* MozContainerSurfaceLock::GetSurface() { return mSurface; }
