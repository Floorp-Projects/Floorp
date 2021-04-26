/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWaylandDisplay.h"
#include "WindowSurfaceWayland.h"

#include "nsPrintfCString.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Tools.h"
#include "gfx2DGlue.h"
#include "gfxPlatform.h"
#include "MozContainer.h"
#include "nsTArray.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/StaticPrefs_widget.h"
#include "mozilla/WidgetUtils.h"

#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>

#undef LOG
#ifdef MOZ_LOGGING
#  include "mozilla/Logging.h"
#  include "nsTArray.h"
#  include "Units.h"
extern mozilla::LazyLogModule gWidgetWaylandLog;
#  define LOGWAYLAND(args) \
    MOZ_LOG(gWidgetWaylandLog, mozilla::LogLevel::Debug, args)
#else
#  define LOGWAYLAND(args)
#endif /* MOZ_LOGGING */

// Maximal compositing timeout it miliseconds
#define COMPOSITING_TIMEOUT 200

namespace mozilla {
namespace widget {

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
        |       |  | WindowBackBuffer    |      |
        |       |  |                     |      |
        |       |  | ------------------- |      |
        |       |  | |  WaylandShmPool | |      |
        |       |  | ------------------- |      |
        |       |  -----------------------      |
        |       |                               |
        |       |  -----------------------      |
        |       |  | WindowBackBuffer    |      |
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
  |  | WindowBackBuffer    |      |
  |  |                     |      |
  |  | ------------------- |      |
  |  | |  WaylandShmPool | |      |
  |  | ------------------- |      |
  |  -----------------------      |
  |                               |
  |  -----------------------      |
  |  | WindowBackBuffer    |      |
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
wl_surface and two wl_buffer objects (owned by WindowBackBuffer)
as we use double buffering. When nsWindow drawing is finished to wl_buffer,
the wl_buffer is attached to wl_surface and it's sent to Wayland compositor.

When there's no wl_buffer available for drawing (all wl_buffers are locked in
compositor for instance) we store the drawing to WindowImageSurface object
and draw later when wl_buffer becomes available or discard the
WindowImageSurface cache when whole screen is invalidated.

WindowBackBuffer

Is a class which provides a wl_buffer for drawing.
Wl_buffer is a main Wayland object with actual graphics data.
Wl_buffer basically represent one complete window screen.
When double buffering is involved every window (GdkWindow for instance)
utilises two wl_buffers which are cycled. One is filed with data by application
and one is rendered by compositor.

WindowBackBuffer is implemented by shared memory (shm).
It owns wl_buffer object, owns WaylandShmPool
(which provides the shared memory) and ties them together.

WaylandShmPool

WaylandShmPool acts as a manager of shared memory for WindowBackBuffer.
Allocates it, holds reference to it and releases it.

We allocate shared memory (shm) by mmap(..., MAP_SHARED,...) as an interface
between us and wayland compositor. We draw our graphics data to the shm and
handle to wayland compositor by WindowBackBuffer/WindowSurfaceWayland
(wl_buffer/wl_surface).
*/

#define EVENT_LOOP_DELAY (1000 / 240)

#define BUFFER_BPP 4
gfx::SurfaceFormat WindowBackBuffer::mFormat = gfx::SurfaceFormat::B8G8R8A8;

int WindowBackBuffer::mDumpSerial =
    PR_GetEnv("MOZ_WAYLAND_DUMP_WL_BUFFERS") ? 1 : 0;
char* WindowBackBuffer::mDumpDir = PR_GetEnv("MOZ_WAYLAND_DUMP_DIR");

RefPtr<nsWaylandDisplay> WindowBackBuffer::GetWaylandDisplay() {
  return mWindowSurfaceWayland->GetWaylandDisplay();
}

static int WaylandAllocateShmMemory(int aSize) {
  int fd = -1;

  nsCString shmPrefix("/");
  const char* snapName = mozilla::widget::WidgetUtils::GetSnapInstanceName();
  if (snapName != nullptr) {
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
  if (ret != 0) {
    NS_WARNING(
        nsPrintfCString("posix_fallocate() fails to allocate shm memory: %s",
                        strerror(ret))
            .get());
    close(fd);
    return -1;
  }
#else
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
#endif

  return fd;
}

static bool WaylandReAllocateShmMemory(int aFd, int aSize) {
  if (ftruncate(aFd, aSize) < 0) {
    return false;
  }
#ifdef HAVE_POSIX_FALLOCATE
  do {
    errno = posix_fallocate(aFd, 0, aSize);
  } while (errno == EINTR);
  if (errno != 0) {
    return false;
  }
#endif
  return true;
}

WaylandShmPool::WaylandShmPool()
    : mShmPool(nullptr),
      mShmPoolFd(-1),
      mAllocatedSize(0),
      mImageData(MAP_FAILED){};

void WaylandShmPool::Release() {
  if (mImageData != MAP_FAILED) {
    munmap(mImageData, mAllocatedSize);
    mImageData = MAP_FAILED;
  }
  if (mShmPool) {
    wl_shm_pool_destroy(mShmPool);
    mShmPool = 0;
  }
  if (mShmPoolFd >= 0) {
    close(mShmPoolFd);
    mShmPoolFd = -1;
  }
}

bool WaylandShmPool::Create(RefPtr<nsWaylandDisplay> aWaylandDisplay,
                            int aSize) {
  // We do size increase only
  if (aSize <= mAllocatedSize) {
    return true;
  }

  if (mShmPoolFd < 0) {
    mShmPoolFd = WaylandAllocateShmMemory(aSize);
    if (mShmPoolFd < 0) {
      return false;
    }
  } else {
    if (!WaylandReAllocateShmMemory(mShmPoolFd, aSize)) {
      Release();
      return false;
    }
  }

  if (mImageData != MAP_FAILED) {
    munmap(mImageData, mAllocatedSize);
  }
  mImageData =
      mmap(nullptr, aSize, PROT_READ | PROT_WRITE, MAP_SHARED, mShmPoolFd, 0);
  if (mImageData == MAP_FAILED) {
    NS_WARNING("Unable to map drawing surface!");
    Release();
    return false;
  }

  if (mShmPool) {
    wl_shm_pool_resize(mShmPool, aSize);
  } else {
    mShmPool = wl_shm_create_pool(aWaylandDisplay->GetShm(), mShmPoolFd, aSize);
    // We set our queue to get mShmPool events at compositor thread.
    wl_proxy_set_queue((struct wl_proxy*)mShmPool,
                       aWaylandDisplay->GetEventQueue());
  }

  mAllocatedSize = aSize;
  return true;
}

void WaylandShmPool::SetImageDataFromPool(class WaylandShmPool* aSourcePool,
                                          int aImageDataSize) {
  MOZ_ASSERT(mAllocatedSize >= aImageDataSize, "WaylandShmPool overflows!");
  memcpy(mImageData, aSourcePool->GetImageData(), aImageDataSize);
}

WaylandShmPool::~WaylandShmPool() { Release(); }

static void buffer_release(void* data, wl_buffer* buffer) {
  auto surface = reinterpret_cast<WindowBackBuffer*>(data);
  surface->Detach(buffer);
}

static const struct wl_buffer_listener buffer_listener = {buffer_release};

bool WindowBackBuffer::Create(int aWidth, int aHeight) {
  MOZ_ASSERT(!IsAttached(), "We can't create attached buffers.");

  ReleaseWLBuffer();

  int size = aWidth * aHeight * BUFFER_BPP;
  if (!mShmPool.Create(GetWaylandDisplay(), size)) {
    return false;
  }

  mWLBuffer =
      wl_shm_pool_create_buffer(mShmPool.GetShmPool(), 0, aWidth, aHeight,
                                aWidth * BUFFER_BPP, WL_SHM_FORMAT_ARGB8888);
  wl_proxy_set_queue((struct wl_proxy*)mWLBuffer,
                     GetWaylandDisplay()->GetEventQueue());
  wl_buffer_add_listener(mWLBuffer, &buffer_listener, this);

  mWidth = aWidth;
  mHeight = aHeight;

  LOGWAYLAND(("WindowBackBuffer::Create [%p] wl_buffer %p ID %d\n", (void*)this,
              (void*)mWLBuffer,
              mWLBuffer ? wl_proxy_get_id((struct wl_proxy*)mWLBuffer) : -1));
  return true;
}

void WindowBackBuffer::ReleaseWLBuffer() {
  LOGWAYLAND(("WindowBackBuffer::Release [%p]\n", (void*)this));
  if (mWLBuffer) {
    wl_buffer_destroy(mWLBuffer);
    mWLBuffer = nullptr;
  }
  mWidth = mHeight = 0;
}

void WindowBackBuffer::Clear() {
  memset(mShmPool.GetImageData(), 0, mHeight * mWidth * BUFFER_BPP);
}

WindowBackBuffer::WindowBackBuffer(WindowSurfaceWayland* aWindowSurfaceWayland)
    : mWindowSurfaceWayland(aWindowSurfaceWayland),
      mShmPool(),
      mWLBuffer(nullptr),
      mWidth(0),
      mHeight(0),
      mAttached(false) {
  LOGWAYLAND(("WindowBackBuffer Created [%p] WindowSurfaceWayland [%p]\n",
              (void*)this, mWindowSurfaceWayland));
}

WindowBackBuffer::~WindowBackBuffer() { ReleaseWLBuffer(); }

bool WindowBackBuffer::Resize(int aWidth, int aHeight) {
  if (aWidth == mWidth && aHeight == mHeight) {
    return true;
  }
  LOGWAYLAND(
      ("WindowBackBuffer::Resize [%p] %d %d\n", (void*)this, aWidth, aHeight));
  Create(aWidth, aHeight);
  return (mWLBuffer != nullptr);
}

void WindowBackBuffer::Attach(wl_surface* aSurface) {
  LOGWAYLAND(
      ("WindowBackBuffer::Attach [%p] wl_surface %p ID %d wl_buffer %p ID %d "
       "WindowSurfaceWayland [%p]\n",
       (void*)this, (void*)aSurface,
       aSurface ? wl_proxy_get_id((struct wl_proxy*)aSurface) : -1,
       (void*)GetWlBuffer(),
       GetWlBuffer() ? wl_proxy_get_id((struct wl_proxy*)GetWlBuffer()) : -1,
       mWindowSurfaceWayland));

  wl_buffer* buffer = GetWlBuffer();
  if (buffer) {
    mAttached = true;
    wl_surface_attach(aSurface, buffer, 0, 0);
    wl_surface_commit(aSurface);
    wl_display_flush(WaylandDisplayGetWLDisplay());
  }
}

void WindowBackBuffer::Detach(wl_buffer* aBuffer) {
  LOGWAYLAND(
      ("WindowBackBuffer::Detach [%p] wl_buffer %p ID %d WindowSurfaceWayland "
       "[%p]\n",
       (void*)this, (void*)aBuffer,
       aBuffer ? wl_proxy_get_id((struct wl_proxy*)aBuffer) : -1,
       mWindowSurfaceWayland));
  mAttached = false;

  // Commit any potential cached drawings from latest Lock()/Commit() cycle.
  mWindowSurfaceWayland->FlushPendingCommits();
}

bool WindowBackBuffer::SetImageDataFromBuffer(
    class WindowBackBuffer* aSourceBuffer) {
  auto sourceBuffer = static_cast<class WindowBackBuffer*>(aSourceBuffer);
  if (!IsMatchingSize(sourceBuffer)) {
    if (!Resize(sourceBuffer->mWidth, sourceBuffer->mHeight)) {
      return false;
    }
  }

  mShmPool.SetImageDataFromPool(
      &sourceBuffer->mShmPool,
      sourceBuffer->mWidth * sourceBuffer->mHeight * BUFFER_BPP);
  return true;
}

already_AddRefed<gfx::DrawTarget> WindowBackBuffer::Lock() {
  LOGWAYLAND(("WindowBackBuffer::Lock [%p] [%d x %d] wl_buffer %p ID %d\n",
              (void*)this, mWidth, mHeight, (void*)mWLBuffer,
              mWLBuffer ? wl_proxy_get_id((struct wl_proxy*)mWLBuffer) : -1));

  gfx::IntSize lockSize(mWidth, mHeight);
  return gfxPlatform::CreateDrawTargetForData(
      static_cast<unsigned char*>(mShmPool.GetImageData()), lockSize,
      BUFFER_BPP * mWidth, GetSurfaceFormat());
}

#ifdef MOZ_LOGGING
void WindowBackBuffer::DumpToFile(const char* aHint) {
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
      (unsigned char*)mShmPool.GetImageData(), CAIRO_FORMAT_ARGB32, mWidth,
      mHeight, BUFFER_BPP * mWidth);
  if (cairo_surface_status(surface) == CAIRO_STATUS_SUCCESS) {
    nsCString filename;
    if (mDumpDir) {
      filename.Append(mDumpDir);
      filename.Append('/');
    }
    filename.Append(
        nsPrintfCString("firefox-wl-buffer-%.5d-%s.png", mDumpSerial++, aHint));
    cairo_surface_write_to_png(surface, filename.get());
    LOGWAYLAND(("Dumped wl_buffer to %s\n", filename.get()));
  }
}
#endif

static void frame_callback_handler(void* data, struct wl_callback* callback,
                                   uint32_t time) {
  auto surface = reinterpret_cast<WindowSurfaceWayland*>(data);
  surface->FrameCallbackHandler();
}

static const struct wl_callback_listener frame_listener = {
    frame_callback_handler};

WindowSurfaceWayland::WindowSurfaceWayland(nsWindow* aWindow)
    : mWindow(aWindow),
      mWaylandDisplay(WaylandDisplayGet()),
      mWaylandBuffer(nullptr),
      mWaylandFullscreenDamage(false),
      mFrameCallback(nullptr),
      mLastCommittedSurfaceID(-1),
      mLastCommitTime(0),
      mDrawToWaylandBufferDirectly(true),
      mCanSwitchWaylandBuffer(true),
      mWLBufferIsDirty(false),
      mBufferCommitAllowed(false),
      mBufferNeedsClear(false),
      mSmoothRendering(StaticPrefs::widget_wayland_smooth_rendering()),
      mSurfaceReadyTimerID(),
      mSurfaceLock("WindowSurfaceWayland lock") {
  for (int i = 0; i < BACK_BUFFER_NUM; i++) {
    mShmBackupBuffer[i] = nullptr;
  }
  // Use slow compositing on KDE only.
  const char* currentDesktop = getenv("XDG_CURRENT_DESKTOP");
  if (currentDesktop && strstr(currentDesktop, "KDE") != nullptr) {
    mSmoothRendering = CACHE_NONE;
  }
}

WindowSurfaceWayland::~WindowSurfaceWayland() {
  MutexAutoLock lock(mSurfaceLock);

  if (mSurfaceReadyTimerID) {
    g_source_remove(mSurfaceReadyTimerID);
    mSurfaceReadyTimerID = 0;
  }

  if (mWLBufferIsDirty) {
    NS_WARNING("Deleted WindowSurfaceWayland with a pending commit!");
  }

  if (mFrameCallback) {
    wl_callback_destroy(mFrameCallback);
  }

  mWaylandBuffer = nullptr;

  for (int i = 0; i < BACK_BUFFER_NUM; i++) {
    if (mShmBackupBuffer[i]) {
      delete mShmBackupBuffer[i];
    }
  }
}

WindowBackBuffer* WindowSurfaceWayland::CreateWaylandBuffer(int aWidth,
                                                            int aHeight) {
  int availableBuffer;

  LOGWAYLAND(
      ("WindowSurfaceWayland::CreateWaylandBuffer %d x %d\n", aWidth, aHeight));

  for (availableBuffer = 0; availableBuffer < BACK_BUFFER_NUM;
       availableBuffer++) {
    if (!mShmBackupBuffer[availableBuffer]) {
      break;
    }
  }

  // There isn't any free slot for additional buffer.
  if (availableBuffer == BACK_BUFFER_NUM) {
    LOGWAYLAND(("    no free buffer slot!\n"));
    return nullptr;
  }

  WindowBackBuffer* buffer = new WindowBackBuffer(this);
  if (!buffer->Create(aWidth, aHeight)) {
    delete buffer;
    LOGWAYLAND(("    failed to create back buffer!\n"));
    return nullptr;
  }

  mShmBackupBuffer[availableBuffer] = buffer;
  LOGWAYLAND(("    created new buffer %p at %d!\n", buffer, availableBuffer));
  return buffer;
}

WindowBackBuffer* WindowSurfaceWayland::WaylandBufferFindAvailable(
    int aWidth, int aHeight) {
  int availableBuffer;

  LOGWAYLAND(("WindowSurfaceWayland::WaylandBufferFindAvailable %d x %d\n",
              aWidth, aHeight));

  // Try to find a buffer which matches the size
  for (availableBuffer = 0; availableBuffer < BACK_BUFFER_NUM;
       availableBuffer++) {
    WindowBackBuffer* buffer = mShmBackupBuffer[availableBuffer];
    if (buffer && !buffer->IsAttached() &&
        buffer->IsMatchingSize(aWidth, aHeight)) {
      LOGWAYLAND(("    found match %d [%p]\n", availableBuffer, buffer));
      return buffer;
    }
  }

  // Try to find any buffer
  for (availableBuffer = 0; availableBuffer < BACK_BUFFER_NUM;
       availableBuffer++) {
    WindowBackBuffer* buffer = mShmBackupBuffer[availableBuffer];
    if (buffer && !buffer->IsAttached()) {
      LOGWAYLAND(
          ("    found any free buffer %d [%p]\n", availableBuffer, buffer));
      return buffer;
    }
  }

  LOGWAYLAND(("    no buffer available!\n"));
  return nullptr;
}

WindowBackBuffer* WindowSurfaceWayland::SetNewWaylandBuffer() {
  LOGWAYLAND(
      ("WindowSurfaceWayland::NewWaylandBuffer [%p] Requested buffer [%d "
       "x %d]\n",
       (void*)this, mWLBufferRect.width, mWLBufferRect.height));

  mWaylandBuffer =
      WaylandBufferFindAvailable(mWLBufferRect.width, mWLBufferRect.height);
  if (mWaylandBuffer) {
    if (!mWaylandBuffer->Resize(mWLBufferRect.width, mWLBufferRect.height)) {
      return nullptr;
    }
    return mWaylandBuffer;
  }

  mWaylandBuffer =
      CreateWaylandBuffer(mWLBufferRect.width, mWLBufferRect.height);
  return mWaylandBuffer;
}

// Recent
WindowBackBuffer* WindowSurfaceWayland::GetWaylandBuffer() {
  LOGWAYLAND(
      ("WindowSurfaceWayland::GetWaylandBuffer [%p] Requested buffer [%d "
       "x %d] can switch %d\n",
       (void*)this, mWLBufferRect.width, mWLBufferRect.height,
       mCanSwitchWaylandBuffer));

#if MOZ_LOGGING
  LOGWAYLAND(("    Recent WindowBackBuffer [%p]\n", mWaylandBuffer));
  for (int i = 0; i < BACK_BUFFER_NUM; i++) {
    if (!mShmBackupBuffer[i]) {
      LOGWAYLAND(("        WindowBackBuffer [%d] null\n", i));
    } else {
      LOGWAYLAND((
          "        WindowBackBuffer [%d][%p] width %d height %d attached %d\n",
          i, mShmBackupBuffer[i], mShmBackupBuffer[i]->GetWidth(),
          mShmBackupBuffer[i]->GetHeight(), mShmBackupBuffer[i]->IsAttached()));
    }
  }
#endif

  // There's no buffer created yet, create a new one for partial screen updates.
  if (!mWaylandBuffer) {
    return SetNewWaylandBuffer();
  }

  if (mWaylandBuffer->IsAttached()) {
    if (mCanSwitchWaylandBuffer) {
      return SetNewWaylandBuffer();
    }
    LOGWAYLAND(("    Buffer is attached and we can't switch, return null\n"));
    return nullptr;
  }

  if (mWaylandBuffer->IsMatchingSize(mWLBufferRect.width,
                                     mWLBufferRect.height)) {
    LOGWAYLAND(("    Size is ok, use the buffer [%d x %d]\n",
                mWLBufferRect.width, mWLBufferRect.height));
    return mWaylandBuffer;
  }

  if (mCanSwitchWaylandBuffer) {
    // Reuse existing buffer
    LOGWAYLAND(("    Reuse buffer with resize [%d x %d]\n", mWLBufferRect.width,
                mWLBufferRect.height));
    if (mWaylandBuffer->Resize(mWLBufferRect.width, mWLBufferRect.height)) {
      return mWaylandBuffer;
    }
    // OOM here, just return null to skip this frame.
    return nullptr;
  }

  LOGWAYLAND(
      ("    Buffer size does not match, requested %d x %d got %d x%d, return "
       "null.\n",
       mWaylandBuffer->GetWidth(), mWaylandBuffer->GetHeight(),
       mWLBufferRect.width, mWLBufferRect.height));
  return nullptr;
}

already_AddRefed<gfx::DrawTarget> WindowSurfaceWayland::LockWaylandBuffer() {
  // Allocated wayland buffer must match mozcontainer widget size.
  mWLBufferRect = mWindow->GetMozContainerSize();

  LOGWAYLAND(
      ("WindowSurfaceWayland::LockWaylandBuffer [%p] Requesting buffer %d x "
       "%d\n",
       (void*)this, mWLBufferRect.width, mWLBufferRect.height));

  WindowBackBuffer* buffer = GetWaylandBuffer();
  LOGWAYLAND(("WindowSurfaceWayland::LockWaylandBuffer [%p] Got buffer %p\n",
              (void*)this, (void*)buffer));

  if (!buffer) {
    if (mLastCommitTime && (g_get_monotonic_time() / 1000) - mLastCommitTime >
                               COMPOSITING_TIMEOUT) {
      NS_WARNING(
          "Slow response from Wayland compositor, visual glitches ahead.");
    }
    return nullptr;
  }

  mCanSwitchWaylandBuffer = false;

  if (mBufferNeedsClear) {
    buffer->Clear();
    mBufferNeedsClear = false;
  }

  return buffer->Lock();
}

already_AddRefed<gfx::DrawTarget> WindowSurfaceWayland::LockImageSurface(
    const gfx::IntSize& aLockSize) {
  if (!mImageSurface || !(aLockSize <= mImageSurface->GetSize())) {
    mImageSurface = gfx::Factory::CreateDataSourceSurface(
        aLockSize, WindowBackBuffer::GetSurfaceFormat());
  }
  gfx::DataSourceSurface::MappedSurface map = {nullptr, 0};
  if (!mImageSurface->Map(gfx::DataSourceSurface::READ_WRITE, &map)) {
    return nullptr;
  }
  return gfxPlatform::CreateDrawTargetForData(
      map.mData, mImageSurface->GetSize(), map.mStride,
      WindowBackBuffer::GetSurfaceFormat());
}

static bool IsWindowFullScreenUpdate(
    LayoutDeviceIntRect& aScreenRect,
    const LayoutDeviceIntRegion& aUpdatedRegion) {
  if (aUpdatedRegion.GetNumRects() > 1) return false;

  gfx::IntRect rect = aUpdatedRegion.RectIter().Get().ToUnknownRect();
  return (rect.x == 0 && rect.y == 0 && aScreenRect.width == rect.width &&
          aScreenRect.height == rect.height);
}

static bool IsPopupFullScreenUpdate(
    LayoutDeviceIntRect& aScreenRect,
    const LayoutDeviceIntRegion& aUpdatedRegion) {
  // We know that popups can be drawn from two parts; a panel and an arrow.
  // Assume we redraw whole popups when we have two rects and bounding
  // box is equal to window borders.
  if (aUpdatedRegion.GetNumRects() > 2) return false;

  gfx::IntRect lockSize = aUpdatedRegion.GetBounds().ToUnknownRect();
  return (lockSize.x == 0 && lockSize.y == 0 &&
          aScreenRect.width == lockSize.width &&
          aScreenRect.height == lockSize.height);
}

already_AddRefed<gfx::DrawTarget> WindowSurfaceWayland::Lock(
    const LayoutDeviceIntRegion& aRegion) {
  if (mWindow->WindowType() == eWindowType_invisible) {
    return nullptr;
  }

  // Lock the surface *after* WaitForSyncEnd() call as is can fire
  // FlushPendingCommits().
  MutexAutoLock lock(mSurfaceLock);

  // Disable all commits (from potential frame callback/delayed handlers)
  // until next WindowSurfaceWayland::Commit() call.
  mBufferCommitAllowed = false;

  LayoutDeviceIntRect mozContainerSize = mWindow->GetMozContainerSize();
  // The window bounds of popup windows contains relative position to
  // the transient window. We need to remove that effect because by changing
  // position of the popup window the buffer has not changed its size.
  mozContainerSize.x = mozContainerSize.y = 0;
  gfx::IntRect lockSize = aRegion.GetBounds().ToUnknownRect();

  bool isTransparentPopup =
      mWindow->IsWaylandPopup() &&
      (eTransparencyTransparent == mWindow->GetTransparencyMode());

  bool windowRedraw = isTransparentPopup
                          ? IsPopupFullScreenUpdate(mozContainerSize, aRegion)
                          : IsWindowFullScreenUpdate(mozContainerSize, aRegion);
  if (windowRedraw) {
    // Clear buffer when we (re)draw new transparent popup window,
    // otherwise leave it as-is, mBufferNeedsClear can be set from previous
    // (already pending) commits which are cached now.
    mBufferNeedsClear =
        mWindow->WaylandSurfaceNeedsClear() || isTransparentPopup;

    // We do full buffer repaint so clear our cached drawings.
    mDelayedImageCommits.Clear();
    mWaylandBufferDamage.SetEmpty();
    mCanSwitchWaylandBuffer = true;
    mWLBufferIsDirty = false;

    // Store info that we can safely invalidate whole screen.
    mWaylandFullscreenDamage = true;
  } else {
    // We can switch buffer if there isn't any content committed
    // to active buffer.
    mCanSwitchWaylandBuffer = !mWLBufferIsDirty;
  }

  LOGWAYLAND(
      ("WindowSurfaceWayland::Lock [%p] [%d,%d] -> [%d x %d] rects %d "
       "MozContainer size [%d x %d]\n",
       (void*)this, lockSize.x, lockSize.y, lockSize.width, lockSize.height,
       aRegion.GetNumRects(), mozContainerSize.width, mozContainerSize.height));
  LOGWAYLAND(("   nsWindow = %p\n", mWindow));
  LOGWAYLAND(("   isPopup = %d\n", mWindow->IsWaylandPopup()));
  LOGWAYLAND(("   isTransparentPopup = %d\n", isTransparentPopup));
  LOGWAYLAND(("   IsPopupFullScreenUpdate = %d\n",
              IsPopupFullScreenUpdate(mozContainerSize, aRegion)));
  LOGWAYLAND(("   IsWindowFullScreenUpdate = %d\n",
              IsWindowFullScreenUpdate(mozContainerSize, aRegion)));
  LOGWAYLAND(("   mBufferNeedsClear = %d\n", mBufferNeedsClear));
  LOGWAYLAND(("   mWLBufferIsDirty = %d\n", mWLBufferIsDirty));
  LOGWAYLAND(("   mCanSwitchWaylandBuffer = %d\n", mCanSwitchWaylandBuffer));
  LOGWAYLAND(("   windowRedraw = %d\n", windowRedraw));

  if (!(mMozContainerRect == mozContainerSize)) {
    LOGWAYLAND(("   screen size changed\n"));
    if (!windowRedraw) {
      LOGWAYLAND(("   screen size changed without redraw!\n"));
      // Screen (window) size changed and we still have some painting pending
      // for the last window size. That can happen when window is resized.
      // We won't draw it but wait for new content.
      mDelayedImageCommits.Clear();
      mWaylandBufferDamage.SetEmpty();
      mCanSwitchWaylandBuffer = true;
      mWLBufferIsDirty = false;
      mBufferNeedsClear = true;
    }
    mMozContainerRect = mozContainerSize;
  }

  mDrawToWaylandBufferDirectly = windowRedraw || mSmoothRendering == CACHE_NONE;
  if (!mDrawToWaylandBufferDirectly && mSmoothRendering == CACHE_SMALL) {
    mDrawToWaylandBufferDirectly =
        (lockSize.width * 2 > mozContainerSize.width &&
         lockSize.height * 2 > mozContainerSize.height);
  }

  if (!mDrawToWaylandBufferDirectly) {
    // Don't switch wl_buffers when we cache drawings.
    mCanSwitchWaylandBuffer = false;
    LOGWAYLAND(("   Indirect drawing, mCanSwitchWaylandBuffer = %d\n",
                mCanSwitchWaylandBuffer));
  }

  if (mDrawToWaylandBufferDirectly) {
    LOGWAYLAND(("   Direct drawing\n"));
    RefPtr<gfx::DrawTarget> dt = LockWaylandBuffer();
    if (dt) {
#if MOZ_LOGGING
      mWaylandBuffer->DumpToFile("Lock");
#endif
      if (!windowRedraw) {
        DrawDelayedImageCommits(dt, mWaylandBufferDamage);
#if MOZ_LOGGING
        mWaylandBuffer->DumpToFile("Lock-after-commit");
#endif
      }
      mWLBufferIsDirty = true;
      return dt.forget();
    }
  }

  // We do indirect drawing because there isn't any front buffer available.
  // Do indirect drawing to mImageSurface which is commited to wayland
  // wl_buffer by DrawDelayedImageCommits() later.
  mDrawToWaylandBufferDirectly = false;

  LOGWAYLAND(("   Indirect drawing.\n"));
  return LockImageSurface(gfx::IntSize(lockSize.XMost(), lockSize.YMost()));
}

bool WindowImageSurface::OverlapsSurface(
    class WindowImageSurface& aBottomSurface) {
  return mUpdateRegion.Contains(aBottomSurface.mUpdateRegion);
}

void WindowImageSurface::DrawToTarget(
    gfx::DrawTarget* aDest, LayoutDeviceIntRegion& aWaylandBufferDamage) {
#ifdef MOZ_LOGGING
  gfx::IntRect bounds = mUpdateRegion.GetBounds().ToUnknownRect();
  LOGWAYLAND(("WindowImageSurface::DrawToTarget\n"));
  LOGWAYLAND(("    rects num %d\n", mUpdateRegion.GetNumRects()));
  LOGWAYLAND(("    bounds [ %d, %d] -> [%d x %d]\n", bounds.x, bounds.y,
              bounds.width, bounds.height));
#endif
  for (auto iter = mUpdateRegion.RectIter(); !iter.Done(); iter.Next()) {
    gfx::IntRect r(iter.Get().ToUnknownRect());
    LOGWAYLAND(
        ("    draw rect [%d,%d] -> [%d x %d]\n", r.x, r.y, r.width, r.height));
    aDest->CopySurface(mImageSurface, r, gfx::IntPoint(r.x, r.y));
  }
  aWaylandBufferDamage.OrWith(mUpdateRegion);
}

WindowImageSurface::WindowImageSurface(
    gfx::DataSourceSurface* aImageSurface,
    const LayoutDeviceIntRegion& aUpdateRegion)
    : mImageSurface(aImageSurface), mUpdateRegion(aUpdateRegion) {}

bool WindowSurfaceWayland::DrawDelayedImageCommits(
    gfx::DrawTarget* aDrawTarget, LayoutDeviceIntRegion& aWaylandBufferDamage) {
  unsigned int imagesNum = mDelayedImageCommits.Length();
  LOGWAYLAND(("WindowSurfaceWayland::DrawDelayedImageCommits [%p] len %d\n",
              (void*)this, imagesNum));
  for (unsigned int i = 0; i < imagesNum; i++) {
    mDelayedImageCommits[i].DrawToTarget(aDrawTarget, aWaylandBufferDamage);
  }
  mDelayedImageCommits.Clear();

  return (imagesNum != 0);
}

void WindowSurfaceWayland::CacheImageSurface(
    const LayoutDeviceIntRegion& aRegion) {
#ifdef MOZ_LOGGING
  gfx::IntRect bounds = aRegion.GetBounds().ToUnknownRect();
  LOGWAYLAND(("WindowSurfaceWayland::CacheImageSurface [%p]\n", (void*)this));
  LOGWAYLAND(("    rects num %d\n", aRegion.GetNumRects()));
  LOGWAYLAND(("    bounds [ %d, %d] -> [%d x %d]\n", bounds.x, bounds.y,
              bounds.width, bounds.height));
#endif

  mImageSurface->Unmap();
  WindowImageSurface surf = WindowImageSurface(mImageSurface, aRegion);

  if (mDelayedImageCommits.Length()) {
    auto lastSurf = mDelayedImageCommits.PopLastElement();
    if (surf.OverlapsSurface(lastSurf)) {
#ifdef MOZ_LOGGING
      {
        gfx::IntRect size =
            lastSurf.GetUpdateRegion()->GetBounds().ToUnknownRect();
        LOGWAYLAND(("    removing [ %d, %d] -> [%d x %d]\n", size.x, size.y,
                    size.width, size.height));
      }
#endif
    } else {
      mDelayedImageCommits.AppendElement(lastSurf);
    }
  }

  mDelayedImageCommits.AppendElement(surf);
  // mImageSurface is owned by mDelayedImageCommits
  mImageSurface = nullptr;

  LOGWAYLAND(
      ("    There's %d cached images\n", int(mDelayedImageCommits.Length())));
}

bool WindowSurfaceWayland::CommitImageCacheToWaylandBuffer() {
  if (!mDelayedImageCommits.Length()) {
    return false;
  }

  MOZ_ASSERT(!mDrawToWaylandBufferDirectly);

  RefPtr<gfx::DrawTarget> dt = LockWaylandBuffer();
  if (!dt) {
    return false;
  }

  LOGWAYLAND(("   Flushing %ld cached WindowImageSurfaces to Wayland buffer\n",
              long(mDelayedImageCommits.Length())));

  return DrawDelayedImageCommits(dt, mWaylandBufferDamage);
}

void WindowSurfaceWayland::FlushPendingCommits() {
  MutexAutoLock lock(mSurfaceLock);
  if (FlushPendingCommitsLocked()) {
    mWaylandDisplay->QueueSyncBegin();
  }
}

// When a new window is created we may not have a valid wl_surface
// for drawing (Gtk haven't created it yet). All commits are queued
// and FlushPendingCommitsLocked() is called by timer when wl_surface is ready
// for drawing.
static int WaylandBufferFlushPendingCommits(void* data) {
  WindowSurfaceWayland* aSurface = static_cast<WindowSurfaceWayland*>(data);
  aSurface->FlushPendingCommits();
  return true;
}

bool WindowSurfaceWayland::FlushPendingCommitsLocked() {
  LOGWAYLAND(
      ("WindowSurfaceWayland::FlushPendingCommitsLocked [%p]\n", (void*)this));
  LOGWAYLAND(("    mDrawToWaylandBufferDirectly = %d\n",
              mDrawToWaylandBufferDirectly));
  LOGWAYLAND(("    mCanSwitchWaylandBuffer = %d\n", mCanSwitchWaylandBuffer));
  LOGWAYLAND(("    mFrameCallback = %p\n", mFrameCallback));
  LOGWAYLAND(("    mLastCommittedSurfaceID = %d\n", mLastCommittedSurfaceID));
  LOGWAYLAND(("    mWLBufferIsDirty = %d\n", mWLBufferIsDirty));
  LOGWAYLAND(("    mBufferCommitAllowed = %d\n", mBufferCommitAllowed));

  if (!mBufferCommitAllowed) {
    LOGWAYLAND(("    Quit - buffer commit is not allowed.\n"));
    return false;
  }

  if (CommitImageCacheToWaylandBuffer()) {
    mWLBufferIsDirty = true;
  }

  // There's nothing to do here
  if (!mWLBufferIsDirty) {
    LOGWAYLAND(("    Quit - no pending commit.\n"));
    return false;
  }

  MOZ_ASSERT(!mWaylandBuffer->IsAttached(),
             "We can't draw to attached wayland buffer!");

  LOGWAYLAND(("    Drawing pending commits.\n"));
  MozContainer* container = mWindow->GetMozContainer();
  wl_surface* waylandSurface = moz_container_wayland_surface_lock(container);
  if (!waylandSurface) {
    LOGWAYLAND(
        ("    moz_container_wayland_surface_lock() failed, delay commit.\n"));

    if (!mSurfaceReadyTimerID) {
      mSurfaceReadyTimerID = g_timeout_add(
          EVENT_LOOP_DELAY, &WaylandBufferFlushPendingCommits, this);
    }
    return true;
  }
  if (mSurfaceReadyTimerID) {
    g_source_remove(mSurfaceReadyTimerID);
    mSurfaceReadyTimerID = 0;
  }

  LOGWAYLAND(("    We have wl_surface %p ID [%d] to commit in.\n",
              waylandSurface,
              wl_proxy_get_id((struct wl_proxy*)waylandSurface)));

  auto unlockContainer = MakeScopeExit([&] {
    moz_container_wayland_surface_unlock(container, &waylandSurface);
  });

  wl_proxy_set_queue((struct wl_proxy*)waylandSurface,
                     mWaylandDisplay->GetEventQueue());

  // We have an active frame callback request so handle it.
  if (mFrameCallback) {
    int waylandSurfaceID = wl_proxy_get_id((struct wl_proxy*)waylandSurface);
    if (waylandSurfaceID == mLastCommittedSurfaceID) {
      LOGWAYLAND(("    [%p] wait for frame callback ID %d.\n", (void*)this,
                  waylandSurfaceID));
      // We have an active frame callback pending from our recent surface.
      // It means we should defer the commit to FrameCallbackHandler().
      return true;
    }
    LOGWAYLAND(("    Removing wrong frame callback [%p] ID %d.\n",
                mFrameCallback,
                wl_proxy_get_id((struct wl_proxy*)mFrameCallback)));
    // If our stored wl_surface does not match the actual one it means the frame
    // callback is no longer active and we should release it.
    wl_callback_destroy(mFrameCallback);
    mFrameCallback = nullptr;
    mLastCommittedSurfaceID = -1;
  }

  if (mWaylandFullscreenDamage) {
    LOGWAYLAND(("    wl_surface_damage full screen\n"));
    wl_surface_damage_buffer(waylandSurface, 0, 0, INT_MAX, INT_MAX);
  } else {
    for (auto iter = mWaylandBufferDamage.RectIter(); !iter.Done();
         iter.Next()) {
      mozilla::LayoutDeviceIntRect r = iter.Get();
      LOGWAYLAND(("   wl_surface_damage_buffer [%d, %d] -> [%d, %d]\n", r.x,
                  r.y, r.width, r.height));
      wl_surface_damage_buffer(waylandSurface, r.x, r.y, r.width, r.height);
    }
  }

#if MOZ_LOGGING
  mWaylandBuffer->DumpToFile("Commit");
#endif

  // Clear all back buffer damage as we're committing
  // all requested regions.
  mWaylandFullscreenDamage = false;
  mWaylandBufferDamage.SetEmpty();

  mFrameCallback = wl_surface_frame(waylandSurface);
  wl_callback_add_listener(mFrameCallback, &frame_listener, this);

  mWaylandBuffer->Attach(waylandSurface);
  mLastCommittedSurfaceID = wl_proxy_get_id((struct wl_proxy*)waylandSurface);
  mLastCommitTime = g_get_monotonic_time() / 1000;

  // There's no pending commit, all changes are sent to compositor.
  mWLBufferIsDirty = false;

  return true;
}

void WindowSurfaceWayland::Commit(const LayoutDeviceIntRegion& aInvalidRegion) {
#ifdef MOZ_LOGGING
  {
    gfx::IntRect lockSize = aInvalidRegion.GetBounds().ToUnknownRect();
    LOGWAYLAND(
        ("WindowSurfaceWayland::Commit [%p] damage size [%d, %d] -> [%d x %d] "
         "MozContainer [%d x %d]\n",
         (void*)this, lockSize.x, lockSize.y, lockSize.width, lockSize.height,
         mMozContainerRect.width, mMozContainerRect.height));
    LOGWAYLAND(("    mDrawToWaylandBufferDirectly = %d\n",
                mDrawToWaylandBufferDirectly));
  }
#endif

  MutexAutoLock lock(mSurfaceLock);

  if (mDrawToWaylandBufferDirectly) {
    mWaylandBufferDamage.OrWith(aInvalidRegion);
  } else {
    CacheImageSurface(aInvalidRegion);
  }

  mBufferCommitAllowed = true;
  if (FlushPendingCommitsLocked()) {
    mWaylandDisplay->QueueSyncBegin();
  }
}

void WindowSurfaceWayland::FrameCallbackHandler() {
  MOZ_ASSERT(mFrameCallback != nullptr,
             "FrameCallbackHandler() called without valid frame callback!");
  MOZ_ASSERT(mLastCommittedSurfaceID != -1,
             "FrameCallbackHandler() called without valid wl_surface!");
  LOGWAYLAND(
      ("WindowSurfaceWayland::FrameCallbackHandler [%p]\n", (void*)this));

  MutexAutoLock lock(mSurfaceLock);

  wl_callback_destroy(mFrameCallback);
  mFrameCallback = nullptr;

  if (FlushPendingCommitsLocked()) {
    mWaylandDisplay->QueueSyncBegin();
  }
}

}  // namespace widget
}  // namespace mozilla
