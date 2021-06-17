/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _MOZILLA_WIDGET_GTK_WAYLAND_SHM_BUFFER_H
#define _MOZILLA_WIDGET_GTK_WAYLAND_SHM_BUFFER_H

#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Types.h"
#include "mozilla/Mutex.h"
#include "nsTArray.h"
#include "nsWaylandDisplay.h"

namespace mozilla::widget {

// Allocates and owns shared memory for Wayland drawing surface
class WaylandShmPool {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WaylandShmPool);

  static RefPtr<WaylandShmPool> Create(
      const RefPtr<nsWaylandDisplay>& aWaylandDisplay, int aSize);

  wl_shm_pool* GetShmPool() { return mShmPool; };
  void* GetImageData() { return mImageData; };

 private:
  explicit WaylandShmPool(int aSize);
  ~WaylandShmPool();

  wl_shm_pool* mShmPool;
  int mShmPoolFd;
  int mAllocatedSize;
  void* mImageData;
};

// Holds actual graphics data for wl_surface
class WaylandShmBuffer {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WaylandShmBuffer);

  static RefPtr<WaylandShmBuffer> Create(
      const RefPtr<nsWaylandDisplay>& aWaylandDisplay,
      const LayoutDeviceIntSize& aSize);

  already_AddRefed<gfx::DrawTarget> Lock();

  void AttachAndCommit(wl_surface* aSurface);
  bool IsAttached() { return mAttached; }
  void Clear();

  static void BufferReleaseCallbackHandler(void* aData, wl_buffer* aBuffer);
  void SetBufferReleaseFunc(void (*aBufferReleaseFunc)(void* aData,
                                                       wl_buffer* aBuffer)) {
    mBufferReleaseFunc = aBufferReleaseFunc;
  }
  void SetBufferReleaseData(void* aBufferReleaseData) {
    mBufferReleaseData = aBufferReleaseData;
  }

  size_t GetBufferAge() { return mBufferAge; };
  RefPtr<WaylandShmPool> GetShmPool() { return mShmPool; }
  LayoutDeviceIntSize GetSize() { return mSize; };
  static gfx::SurfaceFormat GetSurfaceFormat() { return mFormat; }
  wl_buffer* GetWlBuffer() { return mWLBuffer; };
  bool IsMatchingSize(const LayoutDeviceIntSize& aSize) {
    return aSize == mSize;
  }

  void IncrementBufferAge() { mBufferAge++; };
  void ResetBufferAge() { mBufferAge = 0; };

#ifdef MOZ_LOGGING
  void DumpToFile(const char* aHint);
#endif

 private:
  explicit WaylandShmBuffer(const LayoutDeviceIntSize& aSize);
  ~WaylandShmBuffer();

  void BufferReleaseCallbackHandler(wl_buffer* aBuffer);

  // WaylandShmPoolMB provides actual shared memory we draw into
  RefPtr<WaylandShmPool> mShmPool;

  // wl_buffer is a wayland object that encapsulates the shared memory
  // and passes it to wayland compositor by wl_surface object.
  wl_buffer* mWLBuffer;

  void (*mBufferReleaseFunc)(void* aData, wl_buffer* aBuffer);
  void* mBufferReleaseData;

  LayoutDeviceIntSize mSize;
  size_t mBufferAge;
  bool mAttached;
  static gfx::SurfaceFormat mFormat;

#ifdef MOZ_LOGGING
  static int mDumpSerial;
  static char* mDumpDir;
#endif
};

}  // namespace mozilla::widget

#endif  // _MOZILLA_WIDGET_GTK_WAYLAND_SHM_BUFFER_H
