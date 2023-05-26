/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _MOZILLA_WIDGET_GTK_WAYLAND_BUFFER_H
#define _MOZILLA_WIDGET_GTK_WAYLAND_BUFFER_H

#include "DMABufSurface.h"
#include "GLContext.h"
#include "MozFramebuffer.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Types.h"
#include "mozilla/Mutex.h"
#include "nsTArray.h"
#include "nsWaylandDisplay.h"
#include "base/shared_memory.h"

namespace mozilla::widget {

// Allocates and owns shared memory for Wayland drawing surface
class WaylandShmPool {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WaylandShmPool);

  static RefPtr<WaylandShmPool> Create(nsWaylandDisplay* aWaylandDisplay,
                                       int aSize);

  wl_shm_pool* GetShmPool() { return mShmPool; };
  void* GetImageData();

 private:
  WaylandShmPool() = default;
  ~WaylandShmPool();

  wl_shm_pool* mShmPool = nullptr;
  void* mImageData = nullptr;
  UniquePtr<base::SharedMemory> mShm;
  int mSize = 0;
};

class WaylandBuffer {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WaylandBuffer);

  void AttachAndCommit(wl_surface* aSurface);
  virtual wl_buffer* GetWlBuffer() = 0;
  virtual already_AddRefed<gfx::DrawTarget> Lock() { return nullptr; };
  virtual void* GetImageData() { return nullptr; }
  virtual GLuint GetTexture() { return 0; }
  virtual void DestroyGLResources(){};

  LayoutDeviceIntSize GetSize() { return mSize; };
  bool IsMatchingSize(const LayoutDeviceIntSize& aSize) {
    return aSize == mSize;
  }
  bool IsAttached() { return mAttached; }
  static gfx::SurfaceFormat GetSurfaceFormat() { return mFormat; }

  static void BufferReleaseCallbackHandler(void* aData, wl_buffer* aBuffer);
  void SetBufferReleaseFunc(void (*aBufferReleaseFunc)(void* aData,
                                                       wl_buffer* aBuffer)) {
    mBufferReleaseFunc = aBufferReleaseFunc;
  }
  void SetBufferReleaseData(void* aBufferReleaseData) {
    mBufferReleaseData = aBufferReleaseData;
  }

 protected:
  explicit WaylandBuffer(const LayoutDeviceIntSize& aSize);
  virtual ~WaylandBuffer() = default;

  void BufferReleaseCallbackHandler(wl_buffer* aBuffer);

  LayoutDeviceIntSize mSize;
  bool mAttached = false;
  void (*mBufferReleaseFunc)(void* aData, wl_buffer* aBuffer) = nullptr;
  void* mBufferReleaseData = nullptr;
  static gfx::SurfaceFormat mFormat;
};

// Holds actual graphics data for wl_surface
class WaylandBufferSHM final : public WaylandBuffer {
 public:
  static RefPtr<WaylandBufferSHM> Create(const LayoutDeviceIntSize& aSize);

  wl_buffer* GetWlBuffer() override { return mWLBuffer; };
  already_AddRefed<gfx::DrawTarget> Lock() override;
  void* GetImageData() override { return mShmPool->GetImageData(); }

  void Clear();
  size_t GetBufferAge() { return mBufferAge; };
  RefPtr<WaylandShmPool> GetShmPool() { return mShmPool; }

  void IncrementBufferAge() { mBufferAge++; };
  void ResetBufferAge() { mBufferAge = 0; };

#ifdef MOZ_LOGGING
  void DumpToFile(const char* aHint);
#endif

 private:
  explicit WaylandBufferSHM(const LayoutDeviceIntSize& aSize);
  ~WaylandBufferSHM() override;

  // WaylandShmPoolMB provides actual shared memory we draw into
  RefPtr<WaylandShmPool> mShmPool;

  // wl_buffer is a wayland object that encapsulates the shared memory
  // and passes it to wayland compositor by wl_surface object.
  wl_buffer* mWLBuffer = nullptr;

  size_t mBufferAge = 0;

#ifdef MOZ_LOGGING
  static int mDumpSerial;
  static char* mDumpDir;
#endif
};

class WaylandBufferDMABUF final : public WaylandBuffer {
 public:
  static RefPtr<WaylandBufferDMABUF> Create(const LayoutDeviceIntSize& aSize,
                                            gl::GLContext* aGL);

  wl_buffer* GetWlBuffer() override { return mDMABufSurface->GetWlBuffer(); };
  GLuint GetTexture() override { return mDMABufSurface->GetTexture(); };
  void DestroyGLResources() override { mDMABufSurface->ReleaseTextures(); };

 private:
  explicit WaylandBufferDMABUF(const LayoutDeviceIntSize& aSize);
  ~WaylandBufferDMABUF() = default;

  RefPtr<DMABufSurfaceRGBA> mDMABufSurface;
};

}  // namespace mozilla::widget

#endif  // _MOZILLA_WIDGET_GTK_WAYLAND_BUFFER_H
