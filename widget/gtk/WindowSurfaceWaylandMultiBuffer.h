/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _MOZILLA_WIDGET_GTK_WINDOW_SURFACE_WAYLAND_MULTI_BUFFER_H
#define _MOZILLA_WIDGET_GTK_WINDOW_SURFACE_WAYLAND_MULTI_BUFFER_H

#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Types.h"
#include "mozilla/Mutex.h"
#include "nsTArray.h"
#include "nsWaylandDisplay.h"
#include "nsWindow.h"
#include "WaylandShmBuffer.h"
#include "WindowSurface.h"

namespace mozilla::widget {

// WindowSurfaceWaylandMB is an abstraction for wl_surface
// and related management
class WindowSurfaceWaylandMB : public WindowSurface {
 public:
  explicit WindowSurfaceWaylandMB(nsWindow* aWindow);
  ~WindowSurfaceWaylandMB() = default;

  // Lock() / Commit() are called by gecko when Firefox
  // wants to display something. Lock() returns a DrawTarget
  // where gecko paints. When gecko is done it calls Commit()
  // and we try to send the DrawTarget (backed by wl_buffer)
  // to wayland compositor.
  //
  // If we fail (wayland compositor is busy,
  // wl_surface is not created yet) we queue the painting
  // and we send it to wayland compositor in FrameCallbackHandler()/
  // FlushPendingCommits().
  already_AddRefed<gfx::DrawTarget> Lock(
      const LayoutDeviceIntRegion& aRegion) override;
  void Commit(const LayoutDeviceIntRegion& aInvalidRegion) final;
  RefPtr<nsWaylandDisplay> GetWaylandDisplay() { return mWaylandDisplay; };

  void PrepareBufferForFrame() override;
  size_t GetBufferAge() const override;
  bool ShouldDrawPreviousPartialPresentRegions() override { return true; };

  static void BufferReleaseCallbackHandler(void* aData, wl_buffer* aBuffer);

 private:
  RefPtr<WaylandShmBuffer> ObtainBufferFromPool();
  void ReturnBufferToPool(const RefPtr<WaylandShmBuffer>& aBuffer);
  void EnforcePoolSizeLimitLocked();
  void PrepareBufferForFrameLocked();
  void IncrementBufferAge();
  void BufferReleaseCallbackHandler(wl_buffer* aBuffer);

  mozilla::Mutex mSurfaceLock;

  nsWindow* mWindow;
  RefPtr<nsWaylandDisplay> mWaylandDisplay;
  RefPtr<WaylandShmBuffer> mWaylandBuffer;
  nsTArray<RefPtr<WaylandShmBuffer>> mInUseBuffers;
  nsTArray<RefPtr<WaylandShmBuffer>> mAvailableBuffers;
  LayoutDeviceIntSize mMozContainerSize;
};

}  // namespace mozilla::widget

#endif  // _MOZILLA_WIDGET_GTK_WINDOW_SURFACE_WAYLAND_MULTI_BUFFER_H
