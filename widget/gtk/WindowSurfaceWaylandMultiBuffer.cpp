/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WindowSurfaceWaylandMultiBuffer.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "gfx2DGlue.h"
#include "gfxPlatform.h"
#include "MozContainer.h"
#include "mozilla/gfx/DataSurfaceHelpers.h"
#include "mozilla/gfx/Tools.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/StaticPrefs_widget.h"
#include "mozilla/WidgetUtils.h"

#undef LOG
#ifdef MOZ_LOGGING
#  include "mozilla/Logging.h"
#  include "Units.h"
extern mozilla::LazyLogModule gWidgetWaylandLog;
#  define LOGWAYLAND(args) \
    MOZ_LOG(gWidgetWaylandLog, mozilla::LogLevel::Debug, args)
#else
#  define LOGWAYLAND(args)
#endif /* MOZ_LOGGING */

namespace mozilla::widget {

/*
  Wayland multi-thread rendering scheme

  Every rendering thread (main thread, compositor thread) contains its own
  nsWaylandDisplay object connected to Wayland compositor (Mutter, Weston, etc.)

  WindowSurfaceWayland implements WindowSurface class and draws nsWindow by
  WindowSurface interface (Lock, Commit) to screen through nsWaylandDisplay.

  ----------------------
  | Wayland compositor |
  ----------------------
             ^
             |
  ----------------------
  |  nsWaylandDisplay  |
  ----------------------
        ^          ^
        |          |
        |          |
        |       ---------------------------------        ------------------
        |       | WindowSurfaceWayland          |<------>| nsWindow       |
        |       |                               |        ------------------
        |       |  -----------------------      |
        |       |  | WaylandShmBuffer    |      |
        |       |  |                     |      |
        |       |  | ------------------- |      |
        |       |  | |  WaylandShmPool | |      |
        |       |  | ------------------- |      |
        |       |  -----------------------      |
        |       |                               |
        |       |  -----------------------      |
        |       |  | WaylandShmBuffer    |      |
        |       |  |                     |      |
        |       |  | ------------------- |      |
        |       |  | |  WaylandShmPool | |      |
        |       |  | ------------------- |      |
        |       |  -----------------------      |
        |       ---------------------------------
        |
        |
  ---------------------------------        ------------------
  | WindowSurfaceWayland          |<------>| nsWindow       |
  |                               |        ------------------
  |  -----------------------      |
  |  | WaylandShmBuffer    |      |
  |  |                     |      |
  |  | ------------------- |      |
  |  | |  WaylandShmPool | |      |
  |  | ------------------- |      |
  |  -----------------------      |
  |                               |
  |  -----------------------      |
  |  | WaylandShmBuffer    |      |
  |  |                     |      |
  |  | ------------------- |      |
  |  | |  WaylandShmPool | |      |
  |  | ------------------- |      |
  |  -----------------------      |
  ---------------------------------


nsWaylandDisplay

Is our connection to Wayland display server,
holds our display connection (wl_display) and event queue (wl_event_queue).

nsWaylandDisplay is created for every thread which sends data to Wayland
compositor. Wayland events for main thread is served by default Gtk+ loop,
for other threads (compositor) we must create wl_event_queue and run event loop.


WindowSurfaceWayland

Is a Wayland implementation of WindowSurface class for WindowSurfaceProvider,
we implement Lock() and Commit() interfaces from WindowSurface
for actual drawing.

One WindowSurfaceWayland draws one nsWindow so those are tied 1:1.
At Wayland level it holds one wl_surface object.

To perform visualiation of nsWindow, WindowSurfaceWayland contains one
wl_surface and two wl_buffer objects (owned by WaylandShmBuffer)
as we use double buffering. When nsWindow drawing is finished to wl_buffer,
the wl_buffer is attached to wl_surface and it's sent to Wayland compositor.

When there's no wl_buffer available for drawing (all wl_buffers are locked in
compositor for instance) we store the drawing to WindowImageSurface object
and draw later when wl_buffer becomes available or discard the
WindowImageSurface cache when whole screen is invalidated.

WaylandShmBuffer

Is a class which provides a wl_buffer for drawing.
Wl_buffer is a main Wayland object with actual graphics data.
Wl_buffer basically represent one complete window screen.
When double buffering is involved every window (GdkWindow for instance)
utilises two wl_buffers which are cycled. One is filed with data by application
and one is rendered by compositor.

WaylandShmBuffer is implemented by shared memory (shm).
It owns wl_buffer object, owns WaylandShmPool
(which provides the shared memory) and ties them together.

WaylandShmPool

WaylandShmPool acts as a manager of shared memory for WaylandShmBuffer.
Allocates it, holds reference to it and releases it.

We allocate shared memory (shm) by mmap(..., MAP_SHARED,...) as an interface
between us and wayland compositor. We draw our graphics data to the shm and
handle to wayland compositor by WaylandShmBuffer/WindowSurfaceWayland
(wl_buffer/wl_surface).
*/

using gfx::DataSourceSurface;

#define BACK_BUFFER_NUM 3

WindowSurfaceWaylandMB::WindowSurfaceWaylandMB(nsWindow* aWindow)
    : mSurfaceLock("WindowSurfaceWayland lock"),
      mWindow(aWindow),
      mWaylandDisplay(WaylandDisplayGet()),
      mWaylandBuffer(nullptr) {}

RefPtr<WaylandShmBuffer> WindowSurfaceWaylandMB::ObtainBufferFromPool(
    const LayoutDeviceIntSize& aSize) {
  if (!mAvailableBuffers.IsEmpty()) {
    RefPtr<WaylandShmBuffer> buffer = mAvailableBuffers.PopLastElement();
    mInUseBuffers.AppendElement(buffer);
    return buffer;
  }

  RefPtr<WaylandShmBuffer> buffer =
      WaylandShmBuffer::Create(GetWaylandDisplay(), aSize);
  mInUseBuffers.AppendElement(buffer);

  buffer->SetBufferReleaseFunc(
      &WindowSurfaceWaylandMB::BufferReleaseCallbackHandler);
  buffer->SetBufferReleaseData(this);

  return buffer;
}

void WindowSurfaceWaylandMB::ReturnBufferToPool(
    const RefPtr<WaylandShmBuffer>& aBuffer) {
  for (const RefPtr<WaylandShmBuffer>& buffer : mInUseBuffers) {
    if (buffer == aBuffer) {
      if (buffer->IsMatchingSize(mMozContainerSize)) {
        mAvailableBuffers.AppendElement(buffer);
      }
      mInUseBuffers.RemoveElement(buffer);
      return;
    }
  }
  MOZ_RELEASE_ASSERT(false, "Returned buffer not in use");
}

void WindowSurfaceWaylandMB::EnforcePoolSizeLimit(const MutexAutoLock& aLock) {
  // Enforce the pool size limit, removing least-recently-used entries as
  // necessary.
  while (mAvailableBuffers.Length() > BACK_BUFFER_NUM) {
    mAvailableBuffers.RemoveElementAt(0);
  }

  NS_WARNING_ASSERTION(mInUseBuffers.Length() < 10, "We are leaking buffers");
}

void WindowSurfaceWaylandMB::PrepareBufferForFrame(const MutexAutoLock& aLock) {
  if (mWindow->WindowType() == eWindowType_invisible) {
    return;
  }

  LayoutDeviceIntSize newMozContainerSize = mWindow->GetMozContainerSize();
  if (mMozContainerSize != newMozContainerSize) {
    mMozContainerSize = newMozContainerSize;
    mPreviousWaylandBuffer = nullptr;
    mPreviousInvalidRegion.SetEmpty();
    mAvailableBuffers.Clear();
  }

  LOGWAYLAND(
      ("WindowSurfaceWaylandMB::PrepareBufferForFrame [%p] MozContainer "
       "size [%d x %d]\n",
       (void*)this, mMozContainerSize.width, mMozContainerSize.height));

  MOZ_ASSERT(!mWaylandBuffer);
  mWaylandBuffer = ObtainBufferFromPool(mMozContainerSize);
}

already_AddRefed<DrawTarget> WindowSurfaceWaylandMB::Lock(
    const LayoutDeviceIntRegion& aRegion) {
  MutexAutoLock lock(mSurfaceLock);

#ifdef MOZ_LOGGING
  gfx::IntRect lockRect = aRegion.GetBounds().ToUnknownRect();
  LOGWAYLAND(
      ("WindowSurfaceWaylandMB::Lock [%p] [%d,%d] -> [%d x %d] rects %d "
       "MozContainer size [%d x %d]\n",
       (void*)this, lockRect.x, lockRect.y, lockRect.width, lockRect.height,
       aRegion.GetNumRects(), mMozContainerSize.width,
       mMozContainerSize.height));
#endif

  PrepareBufferForFrame(lock);
  if (!mWaylandBuffer) {
    return nullptr;
  }

  RefPtr<DrawTarget> dt = mWaylandBuffer->Lock();
  return dt.forget();
}

void WindowSurfaceWaylandMB::HandlePartialUpdate(
    const MutexAutoLock& aLock, const LayoutDeviceIntRegion& aInvalidRegion) {
  if (!mPreviousWaylandBuffer || mPreviousWaylandBuffer == mWaylandBuffer) {
    mPreviousWaylandBuffer = mWaylandBuffer;
    mPreviousInvalidRegion = aInvalidRegion;
    return;
  }

  LayoutDeviceIntRegion copyRegion;
  if (mWaylandBuffer->GetBufferAge() == 2) {
    copyRegion.Sub(mPreviousInvalidRegion, aInvalidRegion);
  } else {
    LayoutDeviceIntSize size = mPreviousWaylandBuffer->GetSize();
    copyRegion = LayoutDeviceIntRegion(
        LayoutDeviceIntRect(0, 0, size.width, size.height));
    copyRegion.SubOut(aInvalidRegion);
  }

  if (!copyRegion.IsEmpty()) {
    RefPtr<DataSourceSurface> dataSourceSurface =
        mozilla::gfx::CreateDataSourceSurfaceFromData(
            mPreviousWaylandBuffer->GetSize().ToUnknownSize(),
            mPreviousWaylandBuffer->GetSurfaceFormat(),
            (const uint8_t*)mPreviousWaylandBuffer->GetShmPool()
                ->GetImageData(),
            mPreviousWaylandBuffer->GetSize().width *
                BytesPerPixel(mPreviousWaylandBuffer->GetSurfaceFormat()));
    RefPtr<DrawTarget> dt = mWaylandBuffer->Lock();

    for (auto iter = copyRegion.RectIter(); !iter.Done(); iter.Next()) {
      LayoutDeviceIntRect r = iter.Get();
      dt->CopySurface(dataSourceSurface, r.ToUnknownRect(), IntPoint(r.x, r.y));
    }
  }

  mPreviousWaylandBuffer = mWaylandBuffer;
  mPreviousInvalidRegion = aInvalidRegion;
}

void WindowSurfaceWaylandMB::Commit(
    const LayoutDeviceIntRegion& aInvalidRegion) {
  MutexAutoLock lock(mSurfaceLock);

#ifdef MOZ_LOGGING
  gfx::IntRect invalidRect = aInvalidRegion.GetBounds().ToUnknownRect();
  LOGWAYLAND(
      ("WindowSurfaceWaylandMB::Commit [%p] damage rect [%d, %d] -> [%d x %d] "
       "MozContainer [%d x %d]\n",
       (void*)this, invalidRect.x, invalidRect.y, invalidRect.width,
       invalidRect.height, mMozContainerSize.width, mMozContainerSize.height));
#endif

  if (!mWaylandBuffer) {
    LOGWAYLAND(
        ("WindowSurfaceWaylandMB::Commit [%p] frame skipped: no buffer\n",
         (void*)this));
    IncrementBufferAge();
    return;
  }

  MozContainer* container = mWindow->GetMozContainer();
  wl_surface* waylandSurface = moz_container_wayland_surface_lock(container);
  if (!waylandSurface) {
    LOGWAYLAND(
        ("WindowSurfaceWaylandMB::Commit [%p] frame skipped: can't lock "
         "wl_surface\n",
         (void*)this));
    ReturnBufferToPool(mWaylandBuffer);
    mWaylandBuffer = nullptr;
    IncrementBufferAge();
    return;
  }

  HandlePartialUpdate(lock, aInvalidRegion);

  for (auto iter = aInvalidRegion.RectIter(); !iter.Done(); iter.Next()) {
    LayoutDeviceIntRect r = iter.Get();
    wl_surface_damage_buffer(waylandSurface, r.x, r.y, r.width, r.height);
  }

  mWaylandBuffer->AttachAndCommit(waylandSurface);

  moz_container_wayland_surface_unlock(container, &waylandSurface);

  mWaylandBuffer->ResetBufferAge();
  mWaylandBuffer = nullptr;

  EnforcePoolSizeLimit(lock);

  IncrementBufferAge();

  if (wl_display_flush(GetWaylandDisplay()->GetDisplay()) == -1) {
    LOGWAYLAND(
        ("WindowSurfaceWaylandMB::Commit [%p] flush failed\n", (void*)this));
  }
}

void WindowSurfaceWaylandMB::IncrementBufferAge() {
  for (const RefPtr<WaylandShmBuffer>& buffer : mInUseBuffers) {
    buffer->IncrementBufferAge();
  }
  for (const RefPtr<WaylandShmBuffer>& buffer : mAvailableBuffers) {
    buffer->IncrementBufferAge();
  }
}

void WindowSurfaceWaylandMB::BufferReleaseCallbackHandler(wl_buffer* aBuffer) {
  MutexAutoLock lock(mSurfaceLock);

  for (const RefPtr<WaylandShmBuffer>& buffer : mInUseBuffers) {
    if (buffer->GetWlBuffer() == aBuffer) {
      ReturnBufferToPool(buffer);
      break;
    }
  }
}

void WindowSurfaceWaylandMB::BufferReleaseCallbackHandler(void* aData,
                                                          wl_buffer* aBuffer) {
  auto* surface = reinterpret_cast<WindowSurfaceWaylandMB*>(aData);
  surface->BufferReleaseCallbackHandler(aBuffer);
}

}  // namespace mozilla::widget
