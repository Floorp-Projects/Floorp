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
#include "WaylandBuffer.h"
#include "WindowSurface.h"

namespace mozilla::widget {

using gfx::DrawTarget;

// WindowSurfaceWaylandMB is an abstraction for wl_surface
// and related management
class WindowSurfaceWaylandMB : public WindowSurface {
 public:
  explicit WindowSurfaceWaylandMB(RefPtr<nsWindow> aWindow);
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
  already_AddRefed<DrawTarget> Lock(
      const LayoutDeviceIntRegion& aInvalidRegion) override;
  void Commit(const LayoutDeviceIntRegion& aInvalidRegion) final;

 private:
  void Commit(const MutexAutoLock& aProofOfLock,
              const LayoutDeviceIntRegion& aInvalidRegion);
  RefPtr<WaylandBufferSHM> ObtainBufferFromPool(
      const MutexAutoLock& aProofOfLock, const LayoutDeviceIntSize& aSize);
  void ReturnBufferToPool(const MutexAutoLock& aProofOfLock,
                          const RefPtr<WaylandBufferSHM>& aBuffer);
  void EnforcePoolSizeLimit(const MutexAutoLock& aProofOfLock);
  void CollectPendingSurfaces(const MutexAutoLock& aProofOfLock);
  void HandlePartialUpdate(const MutexAutoLock& aProofOfLock,
                           const LayoutDeviceIntRegion& aInvalidRegion);
  void IncrementBufferAge(const MutexAutoLock& aProofOfLock);

  mozilla::Mutex mSurfaceLock;

  RefPtr<nsWindow> mWindow;
  LayoutDeviceIntSize mMozContainerSize;

  RefPtr<WaylandBufferSHM> mInProgressBuffer;
  RefPtr<WaylandBufferSHM> mFrontBuffer;
  LayoutDeviceIntRegion mFrontBufferInvalidRegion;

  // buffer pool
  nsTArray<RefPtr<WaylandBufferSHM>> mInUseBuffers;
  nsTArray<RefPtr<WaylandBufferSHM>> mPendingBuffers;
  nsTArray<RefPtr<WaylandBufferSHM>> mAvailableBuffers;

  // delayed commits
  bool mFrameInProcess;
  bool mCallbackRequested;
};

}  // namespace mozilla::widget

#endif  // _MOZILLA_WIDGET_GTK_WINDOW_SURFACE_WAYLAND_MULTI_BUFFER_H
