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

static int WaylandAllocateShmMemory(int aSize) {
  int fd = -1;

  nsAutoCString shmPrefix("/");
  if (const char* snapName = GetSnapInstanceName()) {
    shmPrefix.AppendPrintf("snap.%s.", snapName);
  }
  shmPrefix.Append("wayland.mozilla.ipc");

  do {
    static int counter = 0;
    nsPrintfCString shmName("%s.%d", shmPrefix.get(), counter++);
    fd = shm_open(shmName.get(), O_CREAT | O_RDWR | O_EXCL, 0600);
    if (fd >= 0) {
      // We don't want to use leaked file
      if (shm_unlink(shmName.get()) != 0) {
        NS_WARNING("shm_unlink failed");
        return -1;
      }
    }
  } while (fd < 0 && errno == EEXIST);

  if (fd < 0) {
    NS_WARNING(nsPrintfCString("shm_open failed: %s", strerror(errno)).get());
    return -1;
  }

  int ret = 0;
#ifdef HAVE_POSIX_FALLOCATE
  do {
    ret = posix_fallocate(fd, 0, aSize);
  } while (ret == EINTR);
  if (ret == 0) {
    return fd;
  }
  if (ret != ENODEV && ret != EINVAL && ret != EOPNOTSUPP) {
    NS_WARNING(
        nsPrintfCString("posix_fallocate() fails to allocate shm memory: %s",
                        strerror(ret))
            .get());
    close(fd);
    return -1;
  }
#endif
  do {
    ret = ftruncate(fd, aSize);
  } while (ret < 0 && errno == EINTR);
  if (ret < 0) {
    NS_WARNING(nsPrintfCString("ftruncate() fails to allocate shm memory: %s",
                               strerror(ret))
                   .get());
    close(fd);
    fd = -1;
  }

  return fd;
}

/* static */
RefPtr<WaylandShmPool> WaylandShmPool::Create(
    const RefPtr<nsWaylandDisplay>& aWaylandDisplay, int aSize) {
  RefPtr<WaylandShmPool> shmPool = new WaylandShmPool(aSize);

  shmPool->mShmPoolFd = WaylandAllocateShmMemory(aSize);
  if (shmPool->mShmPoolFd < 0) {
    return nullptr;
  }

  shmPool->mImageData = mmap(nullptr, aSize, PROT_READ | PROT_WRITE, MAP_SHARED,
                             shmPool->mShmPoolFd, 0);
  if (shmPool->mImageData == MAP_FAILED) {
    NS_WARNING("Unable to map drawing surface!");
    return nullptr;
  }

  shmPool->mShmPool =
      wl_shm_create_pool(aWaylandDisplay->GetShm(), shmPool->mShmPoolFd, aSize);
  if (!shmPool->mShmPool) {
    return nullptr;
  }

  // We set our queue to get mShmPool events at compositor thread.
  wl_proxy_set_queue((struct wl_proxy*)shmPool->mShmPool,
                     aWaylandDisplay->GetEventQueue());

  return shmPool;
}

WaylandShmPool::WaylandShmPool(int aSize)
    : mShmPool(nullptr),
      mShmPoolFd(-1),
      mAllocatedSize(aSize),
      mImageData(nullptr){};

WaylandShmPool::~WaylandShmPool() {
  if (mImageData != MAP_FAILED) {
    munmap(mImageData, mAllocatedSize);
    mImageData = MAP_FAILED;
  }
  MozClearPointer(mShmPool, wl_shm_pool_destroy);
  if (mShmPoolFd >= 0) {
    close(mShmPoolFd);
    mShmPoolFd = -1;
  }
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
  RefPtr<nsWaylandDisplay> waylandDisplay = WaylandDisplayGet();

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

  wl_proxy_set_queue((struct wl_proxy*)buffer->GetWlBuffer(),
                     waylandDisplay->GetEventQueue());
  wl_buffer_add_listener(buffer->GetWlBuffer(), &sBufferListenerWaylandBuffer,
                         buffer.get());

  LOGWAYLAND("WaylandBufferSHM Created [%p] WaylandDisplay [%p]\n",
             buffer.get(), waylandDisplay.get());

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

  RefPtr<nsWaylandDisplay> waylandDisplay = WaylandDisplayGet();
  wl_proxy_set_queue((struct wl_proxy*)buffer->GetWlBuffer(),
                     waylandDisplay->GetEventQueue());
  wl_buffer_add_listener(buffer->GetWlBuffer(), &sBufferListenerWaylandBuffer,
                         buffer.get());

  return buffer;
}

WaylandBufferDMABUF::WaylandBufferDMABUF(const LayoutDeviceIntSize& aSize)
    : WaylandBuffer(aSize) {}

}  // namespace mozilla::widget
