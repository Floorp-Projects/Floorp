/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _MOZILLA_WIDGET_WINDOW_SURFACE_H
#define _MOZILLA_WIDGET_WINDOW_SURFACE_H

#include "mozilla/gfx/2D.h"
#include "Units.h"

namespace mozilla {
namespace widget {

// A class for drawing to double-buffered windows.
class WindowSurface {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WindowSurface);

  // Locks a region of the window for drawing, returning a draw target
  // capturing the bounds of the provided region.
  // Implementations must permit invocation from any thread.
  virtual already_AddRefed<gfx::DrawTarget> Lock(
      const LayoutDeviceIntRegion& aRegion) = 0;

  // Swaps the provided invalid region from the back buffer to the window.
  // Implementations must permit invocation from any thread.
  virtual void Commit(const LayoutDeviceIntRegion& aInvalidRegion) = 0;

  // Whether the window surface represents a fallback method.
  virtual bool IsFallback() const { return false; }

 protected:
  virtual ~WindowSurface() = default;
};

}  // namespace widget
}  // namespace mozilla

#endif  // _MOZILLA_WIDGET_WINDOW_SURFACE_H
