/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WaylandBuffer.h"

#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>

#include "gfx2DGlue.h"
#include "gfxPlatform.h"
#include "mozilla/WidgetUtilsGtk.h"
#include "mozilla/gfx/Tools.h"
#include "nsGtkUtils.h"
#include "nsPrintfCString.h"
#include "prenv.h"  // For PR_GetEnv

#ifdef MOZ_LOGGING
#  include "mozilla/Logging.h"
#  include "mozilla/ScopeExit.h"
#  include "Units.h"
extern mozilla::LazyLogModule gWidgetWaylandLog;
#  define LOGWAYLAND(...) \
    MOZ_LOG(gWidgetWaylandLog, mozilla::LogLevel::Debug, (__VA_ARGS__))
#else
#  define LOGWAYLAND(...)
#endif /* MOZ_LOGGING */

using namespace mozilla::gl;

namespace mozilla::widget {

#define BUFFER_BPP 4
gfx::SurfaceFormat WaylandBuffer::mFormat = gfx::SurfaceFormat::B8G8R8A8;

#ifdef MOZ_LOGGING
int WaylandBufferSHM::mDumpSerial =
    PR_GetEnv("MOZ_WAYLAND_DUMP_WL_BUFFERS") ? 1 : 0;
char* WaylandBufferSHM::mDumpDir = PR_GetEnv("MOZ_WAYLAND_DUMP_DIR");
#endif

/* static */
RefPtr<WaylandShmPool> WaylandShmPool::Create(nsWaylandDisplay* aWaylandDisplay,
                                              int aSize) {
  if (!aWaylandDisplay->GetShm()) {
    NS_WARNING("WaylandShmPool: Missing Wayland shm interface!");
    return nullptr;
  }

  RefPtr<WaylandShmPool> shmPool = new WaylandShmPool();

  shmPool->mShm = MakeUnique<base::SharedMemory>();
  if (!shmPool->mShm->Create(aSize)) {
    NS_WARNING("WaylandShmPool: Unable to allocate shared memory!");
    return nullptr;
  }

  shmPool->mSize = aSize;
  shmPool->mShmPool = wl_shm_create_pool(
      aWaylandDisplay->GetShm(), shmPool->mShm->CloneHandle().get(), aSize);
  if (!shmPool->mShmPool) {
    NS_WARNING("WaylandShmPool: Unable to allocate shared memory pool!");
    return nullptr;
  }

  return shmPool;
}

void* WaylandShmPool::GetImageData() {
  if (mImageData) {
    return mImageData;
  }
  if (!mShm->Map(mSize)) {
    NS_WARNING("WaylandShmPool: Failed to map Shm!");
    return nullptr;
  }
  mImageData = mShm->memory();
  return mImageData;
}

WaylandShmPool::~WaylandShmPool() {
  MozClearPointer(mShmPool, wl_shm_pool_destroy);
}

static const struct wl_buffer_listener sBufferListenerWaylandBuffer = {
    WaylandBuffer::BufferReleaseCallbackHandler};

WaylandBuffer::WaylandBuffer(const LayoutDeviceIntSize& aSize) : mSize(aSize) {}

void WaylandBuffer::AttachAndCommit(wl_surface* aSurface) {
  LOGWAYLAND(
      "WaylandBuffer::AttachAndCommit [%p] wl_surface %p ID %d wl_buffer "
      "%p ID %d\n",
      (void*)this, (void*)aSurface,
      aSurface ? wl_proxy_get_id((struct wl_proxy*)aSurface) : -1,
      (void*)GetWlBuffer(),
      GetWlBuffer() ? wl_proxy_get_id((struct wl_proxy*)GetWlBuffer()) : -1);

  wl_buffer* buffer = GetWlBuffer();
  if (buffer) {
    mAttached = true;
    wl_surface_attach(aSurface, buffer, 0, 0);
    wl_surface_commit(aSurface);
  }
}

void WaylandBuffer::BufferReleaseCallbackHandler(wl_buffer* aBuffer) {
  mAttached = false;

  if (mBufferReleaseFunc) {
    mBufferReleaseFunc(mBufferReleaseData, aBuffer);
  }
}

void WaylandBuffer::BufferReleaseCallbackHandler(void* aData,
                                                 wl_buffer* aBuffer) {
  auto* buffer = reinterpret_cast<WaylandBuffer*>(aData);
  buffer->BufferReleaseCallbackHandler(aBuffer);
}

/* static */
RefPtr<WaylandBufferSHM> WaylandBufferSHM::Create(
    const LayoutDeviceIntSize& aSize) {
  RefPtr<WaylandBufferSHM> buffer = new WaylandBufferSHM(aSize);
  nsWaylandDisplay* waylandDisplay = WaylandDisplayGet();

  int size = aSize.width * aSize.height * BUFFER_BPP;
  buffer->mShmPool = WaylandShmPool::Create(waylandDisplay, size);
  if (!buffer->mShmPool) {
    return nullptr;
  }

  buffer->mWLBuffer = wl_shm_pool_create_buffer(
      buffer->mShmPool->GetShmPool(), 0, aSize.width, aSize.height,
      aSize.width * BUFFER_BPP, WL_SHM_FORMAT_ARGB8888);
  if (!buffer->mWLBuffer) {
    return nullptr;
  }

  wl_buffer_add_listener(buffer->GetWlBuffer(), &sBufferListenerWaylandBuffer,
                         buffer.get());

  LOGWAYLAND("WaylandBufferSHM Created [%p] WaylandDisplay [%p]\n",
             buffer.get(), waylandDisplay);

  return buffer;
}

WaylandBufferSHM::WaylandBufferSHM(const LayoutDeviceIntSize& aSize)
    : WaylandBuffer(aSize) {}

WaylandBufferSHM::~WaylandBufferSHM() {
  MozClearPointer(mWLBuffer, wl_buffer_destroy);
}

already_AddRefed<gfx::DrawTarget> WaylandBufferSHM::Lock() {
  return gfxPlatform::CreateDrawTargetForData(
      static_cast<unsigned char*>(mShmPool->GetImageData()),
      mSize.ToUnknownSize(), BUFFER_BPP * mSize.width, GetSurfaceFormat());
}

void WaylandBufferSHM::Clear() {
  memset(mShmPool->GetImageData(), 0, mSize.height * mSize.width * BUFFER_BPP);
}

#ifdef MOZ_LOGGING
void WaylandBufferSHM::DumpToFile(const char* aHint) {
  if (!mDumpSerial) {
    return;
  }

  cairo_surface_t* surface = nullptr;
  auto unmap = MakeScopeExit([&] {
    if (surface) {
      cairo_surface_destroy(surface);
    }
  });
  surface = cairo_image_surface_create_for_data(
      (unsigned char*)mShmPool->GetImageData(), CAIRO_FORMAT_ARGB32,
      mSize.width, mSize.height, BUFFER_BPP * mSize.width);
  if (cairo_surface_status(surface) == CAIRO_STATUS_SUCCESS) {
    nsCString filename;
    if (mDumpDir) {
      filename.Append(mDumpDir);
      filename.Append('/');
    }
    filename.Append(
        nsPrintfCString("firefox-wl-buffer-%.5d-%s.png", mDumpSerial++, aHint));
    cairo_surface_write_to_png(surface, filename.get());
    LOGWAYLAND("Dumped wl_buffer to %s\n", filename.get());
  }
}
#endif

/* static */
RefPtr<WaylandBufferDMABUF> WaylandBufferDMABUF::Create(
    const LayoutDeviceIntSize& aSize, GLContext* aGL) {
  RefPtr<WaylandBufferDMABUF> buffer = new WaylandBufferDMABUF(aSize);

  const auto flags =
      static_cast<DMABufSurfaceFlags>(DMABUF_TEXTURE | DMABUF_ALPHA);
  buffer->mDMABufSurface =
      DMABufSurfaceRGBA::CreateDMABufSurface(aSize.width, aSize.height, flags);
  if (!buffer->mDMABufSurface || !buffer->mDMABufSurface->CreateTexture(aGL)) {
    return nullptr;
  }

  if (!buffer->mDMABufSurface->CreateWlBuffer()) {
    return nullptr;
  }

  wl_buffer_add_listener(buffer->GetWlBuffer(), &sBufferListenerWaylandBuffer,
                         buffer.get());

  return buffer;
}

WaylandBufferDMABUF::WaylandBufferDMABUF(const LayoutDeviceIntSize& aSize)
    : WaylandBuffer(aSize) {}

}  // namespace mozilla::widget
