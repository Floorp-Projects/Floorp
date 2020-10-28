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
#include "base/message_loop.h"  // for MessageLoop
#include "base/task.h"          // for NewRunnableMethod, etc
#include "mozilla/StaticPrefs_widget.h"

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

// Maximal timeout between frame callbacks
#define FRAME_CALLBACK_TIMEOUT 20

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
        |       |  | WindowBackBufferShm |      |
        |       |  |                     |      |
        |       |  | ------------------- |      |
        |       |  | |  WaylandShmPool | |      |
        |       |  | ------------------- |      |
        |       |  -----------------------      |
        |       |                               |
        |       |  -----------------------      |
        |       |  | WindowBackBufferShm |      |
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
  |  | WindowBackBufferShm |      |
  |  |                     |      |
  |  | ------------------- |      |
  |  | |  WaylandShmPool | |      |
  |  | ------------------- |      |
  |  -----------------------      |
  |                               |
  |  -----------------------      |
  |  | WindowBackBufferShm |      |
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
and draw later when wl_buffer becomes availabe or discard the
WindowImageSurface cache when whole screen is invalidated.

WindowBackBuffer

Is an abstraction class which provides a wl_buffer for drawing.
Wl_buffer is a main Wayland object with actual graphics data.
Wl_buffer basically represent one complete window screen.
When double buffering is involved every window (GdkWindow for instance)
utilises two wl_buffers which are cycled. One is filed with data by application
and one is rendered by compositor.


WindowBackBufferShm

It's WindowBackBuffer implementation by shared memory (shm).
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

static mozilla::Mutex* gDelayedCommitLock = nullptr;
static GList* gDelayedCommits = nullptr;

static void DelayedCommitsEnsureMutext() {
  if (!gDelayedCommitLock) {
    gDelayedCommitLock = new mozilla::Mutex("DelayedCommit lock");
  }
}

static bool DelayedCommitsCheckAndRemoveSurface(
    WindowSurfaceWayland* aSurface) {
  MutexAutoLock lock(*gDelayedCommitLock);
  GList* foundCommit = g_list_find(gDelayedCommits, aSurface);
  if (foundCommit) {
    gDelayedCommits = g_list_delete_link(gDelayedCommits, foundCommit);
  }
  return foundCommit != nullptr;
}

static bool DelayedCommitsCheckAndAddSurface(WindowSurfaceWayland* aSurface) {
  MutexAutoLock lock(*gDelayedCommitLock);
  GList* foundCommit = g_list_find(gDelayedCommits, aSurface);
  if (!foundCommit) {
    gDelayedCommits = g_list_prepend(gDelayedCommits, aSurface);
  }
  return foundCommit == nullptr;
}

// When a new window is created we may not have a valid wl_surface
// for drawing (Gtk haven't created it yet). All commits are queued
// and CommitWaylandBuffer() is called by timer when wl_surface is ready
// for drawing.
static void WaylandBufferDelayCommitHandler(WindowSurfaceWayland* aSurface) {
  if (DelayedCommitsCheckAndRemoveSurface(aSurface)) {
    aSurface->CommitWaylandBuffer();
  }
}

RefPtr<nsWaylandDisplay> WindowBackBuffer::GetWaylandDisplay() {
  return mWindowSurfaceWayland->GetWaylandDisplay();
}

static int WaylandAllocateShmMemory(int aSize) {
  int fd = -1;
  do {
    static int counter = 0;
    nsPrintfCString shmName("/wayland.mozilla.ipc.%d", counter++);
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

bool WindowBackBufferShm::Create(int aWidth, int aHeight) {
  MOZ_ASSERT(!IsAttached(), "We can't create attached buffers.");

  ReleaseShmSurface();

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

  LOGWAYLAND(("WindowBackBufferShm::Create [%p] wl_buffer %p ID %d\n",
              (void*)this, (void*)mWLBuffer,
              mWLBuffer ? wl_proxy_get_id((struct wl_proxy*)mWLBuffer) : -1));
  return true;
}

void WindowBackBufferShm::ReleaseShmSurface() {
  LOGWAYLAND(("WindowBackBufferShm::Release [%p]\n", (void*)this));
  if (mWLBuffer) {
    wl_buffer_destroy(mWLBuffer);
    mWLBuffer = nullptr;
  }
  mWidth = mHeight = 0;
}

void WindowBackBufferShm::Clear() {
  memset(mShmPool.GetImageData(), 0, mHeight * mWidth * BUFFER_BPP);
}

WindowBackBufferShm::WindowBackBufferShm(
    WindowSurfaceWayland* aWindowSurfaceWayland)
    : WindowBackBuffer(aWindowSurfaceWayland),
      mShmPool(),
      mWLBuffer(nullptr),
      mWidth(0),
      mHeight(0),
      mAttached(false) {}

WindowBackBufferShm::~WindowBackBufferShm() { ReleaseShmSurface(); }

bool WindowBackBufferShm::Resize(int aWidth, int aHeight) {
  if (aWidth == mWidth && aHeight == mHeight) {
    return true;
  }
  LOGWAYLAND(("WindowBackBufferShm::Resize [%p] %d %d\n", (void*)this, aWidth,
              aHeight));
  Create(aWidth, aHeight);
  return (mWLBuffer != nullptr);
}

void WindowBackBuffer::Attach(wl_surface* aSurface) {
  LOGWAYLAND(
      ("WindowBackBuffer::Attach [%p] wl_surface %p ID %d wl_buffer %p ID %d\n",
       (void*)this, (void*)aSurface,
       aSurface ? wl_proxy_get_id((struct wl_proxy*)aSurface) : -1,
       (void*)GetWlBuffer(),
       GetWlBuffer() ? wl_proxy_get_id((struct wl_proxy*)GetWlBuffer()) : -1));

  wl_buffer* buffer = GetWlBuffer();
  if (buffer) {
    wl_surface_attach(aSurface, buffer, 0, 0);
    wl_surface_commit(aSurface);
    wl_display_flush(WaylandDisplayGetWLDisplay());
    SetAttached();
  }
}

void WindowBackBufferShm::Detach(wl_buffer* aBuffer) {
  LOGWAYLAND(("WindowBackBufferShm::Detach [%p] wl_buffer %p ID %d\n",
              (void*)this, (void*)aBuffer,
              aBuffer ? wl_proxy_get_id((struct wl_proxy*)aBuffer) : -1));
  mAttached = false;

  // Commit any potential cached drawings from latest Lock()/Commit() cycle.
  mWindowSurfaceWayland->CommitWaylandBuffer();
}

bool WindowBackBufferShm::SetImageDataFromBuffer(
    class WindowBackBuffer* aSourceBuffer) {
  auto sourceBuffer = static_cast<class WindowBackBufferShm*>(aSourceBuffer);
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

already_AddRefed<gfx::DrawTarget> WindowBackBufferShm::Lock() {
  LOGWAYLAND(("WindowBackBufferShm::Lock [%p] [%d x %d] wl_buffer %p ID %d\n",
              (void*)this, mWidth, mHeight, (void*)mWLBuffer,
              mWLBuffer ? wl_proxy_get_id((struct wl_proxy*)mWLBuffer) : -1));

  gfx::IntSize lockSize(mWidth, mHeight);
  mIsLocked = true;
  return gfxPlatform::CreateDrawTargetForData(
      static_cast<unsigned char*>(mShmPool.GetImageData()), lockSize,
      BUFFER_BPP * mWidth, GetSurfaceFormat());
}

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
      mLastCommittedSurface(nullptr),
      mLastCommitTime(0),
      mDrawToWaylandBufferDirectly(true),
      mCanSwitchWaylandBuffer(true),
      mBufferPendingCommit(false),
      mBufferCommitAllowed(false),
      mBufferNeedsClear(false),
      mSmoothRendering(StaticPrefs::widget_wayland_smooth_rendering()),
      mIsMainThread(NS_IsMainThread()) {
  for (int i = 0; i < BACK_BUFFER_NUM; i++) {
    mShmBackupBuffer[i] = nullptr;
  }
  DelayedCommitsEnsureMutext();
}

WindowSurfaceWayland::~WindowSurfaceWayland() {
  if (mBufferPendingCommit) {
    NS_WARNING("Deleted WindowSurfaceWayland with a pending commit!");
  }

  // Delete reference to this to prevent WaylandBufferDelayCommitHandler()
  // operate on released this.
  DelayedCommitsCheckAndRemoveSurface(this);

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
  for (availableBuffer = 0; availableBuffer < BACK_BUFFER_NUM;
       availableBuffer++) {
    if (!mShmBackupBuffer[availableBuffer]) {
      break;
    }
  }

  // There isn't any free slot for additional buffer.
  if (availableBuffer == BACK_BUFFER_NUM) {
    return nullptr;
  }

  WindowBackBuffer* buffer = new WindowBackBufferShm(this);
  if (!buffer->Create(aWidth, aHeight)) {
    delete buffer;
    return nullptr;
  }

  mShmBackupBuffer[availableBuffer] = buffer;
  return buffer;
}

WindowBackBuffer* WindowSurfaceWayland::WaylandBufferFindAvailable(
    int aWidth, int aHeight) {
  int availableBuffer;
  // Try to find a buffer which matches the size
  for (availableBuffer = 0; availableBuffer < BACK_BUFFER_NUM;
       availableBuffer++) {
    WindowBackBuffer* buffer = mShmBackupBuffer[availableBuffer];
    if (buffer && !buffer->IsAttached() &&
        buffer->IsMatchingSize(aWidth, aHeight)) {
      return buffer;
    }
  }

  // Try to find any buffer
  for (availableBuffer = 0; availableBuffer < BACK_BUFFER_NUM;
       availableBuffer++) {
    WindowBackBuffer* buffer = mShmBackupBuffer[availableBuffer];
    if (buffer && !buffer->IsAttached()) {
      return buffer;
    }
  }

  return nullptr;
}

WindowBackBuffer* WindowSurfaceWayland::SetNewWaylandBuffer() {
  LOGWAYLAND(
      ("WindowSurfaceWayland::NewWaylandBuffer [%p] Requested buffer [%d "
       "x %d]\n",
       (void*)this, mWLBufferRect.width, mWLBufferRect.height));

  mWaylandBuffer =
      WaylandBufferFindAvailable(mWLBufferRect.width, mWLBufferRect.height);
  if (!mWaylandBuffer) {
    mWaylandBuffer =
        CreateWaylandBuffer(mWLBufferRect.width, mWLBufferRect.height);
  }
  return mWaylandBuffer;
}

WindowBackBuffer* WindowSurfaceWayland::GetWaylandBufferRecent() {
  LOGWAYLAND(
      ("WindowSurfaceWayland::GetWaylandBufferRecent [%p] Requested buffer [%d "
       "x %d]\n",
       (void*)this, mWLBufferRect.width, mWLBufferRect.height));

  // There's no buffer created yet, create a new one for partial screen updates.
  if (!mWaylandBuffer) {
    return SetNewWaylandBuffer();
  }

  if (mWaylandBuffer->IsAttached()) {
    LOGWAYLAND(("    Buffer is attached, return null\n"));
    return nullptr;
  }

  if (mWaylandBuffer->IsMatchingSize(mWLBufferRect.width,
                                     mWLBufferRect.height)) {
    LOGWAYLAND(("    Size is ok, use the buffer [%d x %d]\n",
                mWLBufferRect.width, mWLBufferRect.height));
    return mWaylandBuffer;
  }

  LOGWAYLAND(("    Buffer size does not match, return null.\n"));
  NS_WARNING(
      "We can't resize Wayland buffer when it contains "
      "unsubmitted drawings!");
  return nullptr;
}

WindowBackBuffer* WindowSurfaceWayland::GetWaylandBufferWithSwitch() {
  LOGWAYLAND(
      ("WindowSurfaceWayland::GetWaylandBufferWithSwitch [%p] Requested buffer "
       "[%d x %d]\n",
       (void*)this, mWLBufferRect.width, mWLBufferRect.height));

  // There's no buffer created yet or actual buffer is attached, get a new one.
  if (!mWaylandBuffer || mWaylandBuffer->IsAttached()) {
    return SetNewWaylandBuffer();
  }

  // Reuse existing buffer
  LOGWAYLAND(("    Reuse buffer with resize [%d x %d]\n", mWLBufferRect.width,
              mWLBufferRect.height));

  // OOM here, just return null to skip this frame.
  if (!mWaylandBuffer->Resize(mWLBufferRect.width, mWLBufferRect.height)) {
    return nullptr;
  }
  return mWaylandBuffer;
}

already_AddRefed<gfx::DrawTarget> WindowSurfaceWayland::LockWaylandBuffer() {
  // Allocated wayland buffer can't be bigger than mozilla widget size.
  LayoutDeviceIntRegion region;
  region.And(mLockedScreenRect, mWindow->GetMozContainerSize());
  mWLBufferRect = LayoutDeviceIntRect(region.GetBounds());

  // mCanSwitchWaylandBuffer set means we're getting buffer for fullscreen
  // update.
  WindowBackBuffer* buffer = mCanSwitchWaylandBuffer
                                 ? GetWaylandBufferWithSwitch()
                                 : GetWaylandBufferRecent();

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

void WindowSurfaceWayland::UnlockWaylandBuffer() {
  LOGWAYLAND(("WindowSurfaceWayland::UnlockWaylandBuffer [%p]\n", (void*)this));
  mWaylandBuffer->Unlock();
}

already_AddRefed<gfx::DrawTarget> WindowSurfaceWayland::LockImageSurface(
    const gfx::IntSize& aLockSize) {
  if (!mImageSurface || mImageSurface->CairoStatus() ||
      !(aLockSize <= mImageSurface->GetSize())) {
    mImageSurface = new gfxImageSurface(
        aLockSize,
        SurfaceFormatToImageFormat(WindowBackBuffer::GetSurfaceFormat()));
    if (mImageSurface->CairoStatus()) {
      return nullptr;
    }
  }

  return gfxPlatform::CreateDrawTargetForData(
      mImageSurface->Data(), mImageSurface->GetSize(), mImageSurface->Stride(),
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
  MOZ_ASSERT(mIsMainThread == NS_IsMainThread());

  // Wait until all pending events are processed. There may be queued
  // wl_buffer release event which releases our wl_buffer for further rendering.
  mWaylandDisplay->WaitForSyncEnd();

  // Disable all commits (from potential frame callback/delayed handlers)
  // until next WindowSurfaceWayland::Commit() call.
  mBufferCommitAllowed = false;

  LayoutDeviceIntRect lockedScreenRect = mWindow->GetBounds();
  // The window bounds of popup windows contains relative position to
  // the transient window. We need to remove that effect because by changing
  // position of the popup window the buffer has not changed its size.
  lockedScreenRect.x = lockedScreenRect.y = 0;
  gfx::IntRect lockSize = aRegion.GetBounds().ToUnknownRect();

  bool isTransparentPopup =
      mWindow->IsWaylandPopup() &&
      (eTransparencyTransparent == mWindow->GetTransparencyMode());

  bool windowRedraw = isTransparentPopup
                          ? IsPopupFullScreenUpdate(lockedScreenRect, aRegion)
                          : IsWindowFullScreenUpdate(lockedScreenRect, aRegion);
  if (windowRedraw) {
    // Clear buffer when we (re)draw new transparent popup window,
    // otherwise leave it as-is, mBufferNeedsClear can be set from previous
    // (already pending) commits which are cached now.
    mBufferNeedsClear =
        mWindow->WaylandSurfaceNeedsClear() || isTransparentPopup;

    // Store info that we can switch WaylandBuffer when we flush
    // mImageSurface / mDelayedImageCommits. Don't clear it - it's cleared
    // at LockWaylandBuffer() when we actualy switch the buffer.
    mCanSwitchWaylandBuffer = true;

    // We do full buffer repaint so clear our cached drawings.
    mDelayedImageCommits.Clear();
    mWaylandBufferDamage.SetEmpty();

    // Store info that we can safely invalidate whole screen.
    mWaylandFullscreenDamage = true;
  } else {
    // We can switch buffer if there isn't any content committed
    // to active buffer.
    mCanSwitchWaylandBuffer = !mBufferPendingCommit;
  }

  LOGWAYLAND(
      ("WindowSurfaceWayland::Lock [%p] [%d,%d] -> [%d x %d] rects %d "
       "windowSize [%d x %d]\n",
       (void*)this, lockSize.x, lockSize.y, lockSize.width, lockSize.height,
       aRegion.GetNumRects(), lockedScreenRect.width, lockedScreenRect.height));
  LOGWAYLAND(("   nsWindow = %p\n", mWindow));
  LOGWAYLAND(("   isPopup = %d\n", mWindow->IsWaylandPopup()));
  LOGWAYLAND(("   isTransparentPopup = %d\n", isTransparentPopup));
  LOGWAYLAND(("   IsPopupFullScreenUpdate = %d\n",
              IsPopupFullScreenUpdate(lockedScreenRect, aRegion)));
  LOGWAYLAND(("   IsWindowFullScreenUpdate = %d\n",
              IsWindowFullScreenUpdate(lockedScreenRect, aRegion)));
  LOGWAYLAND(("   mBufferNeedsClear = %d\n", mBufferNeedsClear));
  LOGWAYLAND(("   windowRedraw = %d\n", windowRedraw));

#if MOZ_LOGGING
  if (!(mLockedScreenRect == lockedScreenRect)) {
    LOGWAYLAND(("   screen size changed\n"));
  }
#endif

  if (!(mLockedScreenRect == lockedScreenRect)) {
    // Screen (window) size changed and we still have some painting pending
    // for the last window size. That can happen when window is resized.
    // We can't commit them any more as they're for former window size, so
    // scratch them.
    mDelayedImageCommits.Clear();
    mWaylandBufferDamage.SetEmpty();

    if (!windowRedraw) {
      NS_WARNING("Partial screen update when window is resized!");
      // This should not happen. Screen size changed but we got only
      // partal screen update instead of whole screen. Discard this painting
      // as it produces artifacts.
      return nullptr;
    }
    mLockedScreenRect = lockedScreenRect;
  }

  // We can draw directly only when widget has the same size as wl_buffer
  LayoutDeviceIntRect size = mWindow->GetMozContainerSize();
  mDrawToWaylandBufferDirectly = (size.width >= mLockedScreenRect.width &&
                                  size.height >= mLockedScreenRect.height);

  // We can draw directly only when we redraw significant part of the window
  // to avoid flickering or do only fullscreen updates in smooth mode.
  if (mDrawToWaylandBufferDirectly) {
    mDrawToWaylandBufferDirectly =
        mSmoothRendering
            ? windowRedraw
            : (windowRedraw || (lockSize.width * 2 > lockedScreenRect.width &&
                                lockSize.height * 2 > lockedScreenRect.height));
  }

  if (mDrawToWaylandBufferDirectly) {
    LOGWAYLAND(("   Direct drawing\n"));
    RefPtr<gfx::DrawTarget> dt = LockWaylandBuffer();
    if (dt) {
      if (!windowRedraw) {
        DrawDelayedImageCommits(dt, mWaylandBufferDamage);
      }
      mBufferPendingCommit = true;
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

void WindowImageSurface::Draw(gfx::SourceSurface* aSurface,
                              gfx::DrawTarget* aDest,
                              const LayoutDeviceIntRegion& aRegion) {
#ifdef MOZ_LOGGING
  gfx::IntRect bounds = aRegion.GetBounds().ToUnknownRect();
  LOGWAYLAND(("WindowImageSurface::Draw\n"));
  LOGWAYLAND(("    rects num %d\n", aRegion.GetNumRects()));
  LOGWAYLAND(("    bounds [ %d, %d] -> [%d x %d]\n", bounds.x, bounds.y,
              bounds.width, bounds.height));
#endif

  for (auto iter = aRegion.RectIter(); !iter.Done(); iter.Next()) {
    mozilla::LayoutDeviceIntRect r = iter.Get();
    gfx::Rect rect(r.ToUnknownRect());
    LOGWAYLAND(("    draw rect [%f,%f] -> [%f x %f]\n", rect.x, rect.y,
                rect.width, rect.height));
    aDest->DrawSurface(aSurface, rect, rect);
  }
}

void WindowImageSurface::Draw(gfx::DrawTarget* aDest,
                              LayoutDeviceIntRegion& aWaylandBufferDamage) {
  Draw(mSurface.get(), aDest, mUpdateRegion);
  aWaylandBufferDamage.OrWith(mUpdateRegion);
}

WindowImageSurface::WindowImageSurface(
    gfxImageSurface* aImageSurface, const LayoutDeviceIntRegion& aUpdateRegion)
    : mImageSurface(aImageSurface), mUpdateRegion(aUpdateRegion) {
  mSurface = gfx::Factory::CreateSourceSurfaceForCairoSurface(
      mImageSurface->CairoSurface(), mImageSurface->GetSize(),
      mImageSurface->Format());
}

void WindowSurfaceWayland::DrawDelayedImageCommits(
    gfx::DrawTarget* aDrawTarget, LayoutDeviceIntRegion& aWaylandBufferDamage) {
  unsigned int imagesNum = mDelayedImageCommits.Length();
  LOGWAYLAND(("WindowSurfaceWayland::DrawDelayedImageCommits [%p] len %d\n",
              (void*)this, imagesNum));

  for (unsigned int i = 0; i < imagesNum; i++) {
    mDelayedImageCommits[i].Draw(aDrawTarget, aWaylandBufferDamage);
  }
  mDelayedImageCommits.Clear();
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

  DrawDelayedImageCommits(dt, mWaylandBufferDamage);
  UnlockWaylandBuffer();

  return true;
}

void WindowSurfaceWayland::CommitWaylandBuffer() {
  LOGWAYLAND(("WindowSurfaceWayland::CommitWaylandBuffer [%p]\n", (void*)this));
  LOGWAYLAND(
      ("   mDrawToWaylandBufferDirectly = %d\n", mDrawToWaylandBufferDirectly));
  LOGWAYLAND(("   mCanSwitchWaylandBuffer = %d\n", mCanSwitchWaylandBuffer));
  LOGWAYLAND(("   mFrameCallback = %p\n", mFrameCallback));
  LOGWAYLAND(("   mLastCommittedSurface = %p\n", mLastCommittedSurface));
  LOGWAYLAND(("   mBufferPendingCommit = %d\n", mBufferPendingCommit));
  LOGWAYLAND(("   mBufferCommitAllowed = %d\n", mBufferCommitAllowed));

  MOZ_ASSERT(mIsMainThread == NS_IsMainThread());

  if (!mBufferCommitAllowed) {
    return;
  }

  if (CommitImageCacheToWaylandBuffer()) {
    mBufferPendingCommit = true;
  }

  // There's nothing to do here
  if (!mBufferPendingCommit) {
    return;
  }

  MOZ_ASSERT(!mWaylandBuffer->IsAttached(),
             "We can't draw to attached wayland buffer!");

  MozContainer* container = mWindow->GetMozContainer();
  wl_surface* waylandSurface = moz_container_wayland_surface_lock(container);
  if (!waylandSurface) {
    LOGWAYLAND(("    [%p] mWindow->GetWaylandSurface() failed, delay commit.\n",
                (void*)this));

    // Target window is not created yet - delay the commit. This can happen only
    // when the window is newly created and there's no active
    // frame callback pending.
    MOZ_ASSERT(!mFrameCallback || waylandSurface != mLastCommittedSurface,
               "Missing wayland surface at frame callback!");

    if (DelayedCommitsCheckAndAddSurface(this)) {
      MessageLoop::current()->PostDelayedTask(
          NewRunnableFunction("WaylandBackBufferCommit",
                              &WaylandBufferDelayCommitHandler, this),
          EVENT_LOOP_DELAY);
    }
    return;
  }

  auto unlockContainer = MakeScopeExit([&] {
    moz_container_wayland_surface_unlock(container, &waylandSurface);
  });

  wl_proxy_set_queue((struct wl_proxy*)waylandSurface,
                     mWaylandDisplay->GetEventQueue());

  // We have an active frame callback request so handle it.
  if (mFrameCallback) {
    if (waylandSurface == mLastCommittedSurface) {
      LOGWAYLAND(("    [%p] wait for frame callback.\n", (void*)this));
      // We have an active frame callback pending from our recent surface.
      // It means we should defer the commit to FrameCallbackHandler(),
      // but only if we're under frame callback timeout range.
      if (mLastCommitTime && (g_get_monotonic_time() / 1000) - mLastCommitTime <
                                 FRAME_CALLBACK_TIMEOUT) {
        return;
      }
    }
    // If our stored wl_surface does not match the actual one it means the frame
    // callback is no longer active and we should release it.
    wl_callback_destroy(mFrameCallback);
    mFrameCallback = nullptr;
    mLastCommittedSurface = nullptr;
  }

  if (mWaylandFullscreenDamage) {
    LOGWAYLAND(("   wl_surface_damage full screen\n"));
    wl_surface_damage(waylandSurface, 0, 0, INT_MAX, INT_MAX);
  } else {
    for (auto iter = mWaylandBufferDamage.RectIter(); !iter.Done();
         iter.Next()) {
      mozilla::LayoutDeviceIntRect r = iter.Get();
      LOGWAYLAND(("   wl_surface_damage_buffer [%d, %d] -> [%d, %d]\n", r.x,
                  r.y, r.width, r.height));
      wl_surface_damage_buffer(waylandSurface, r.x, r.y, r.width, r.height);
    }
  }

  // Clear all back buffer damage as we're committing
  // all requested regions.
  mWaylandFullscreenDamage = false;
  mWaylandBufferDamage.SetEmpty();

  mFrameCallback = wl_surface_frame(waylandSurface);
  wl_callback_add_listener(mFrameCallback, &frame_listener, this);

  mWaylandBuffer->Attach(waylandSurface);
  mLastCommittedSurface = waylandSurface;
  mLastCommitTime = g_get_monotonic_time() / 1000;

  // Unlock surface now as SyncBegin()
  moz_container_wayland_surface_unlock(container, &waylandSurface);

  // Ask wl_display to start events synchronization. We're going to wait
  // until all events are processed before next WindowSurfaceWayland::Lock()
  // as we hope for free wl_buffer there.
  mWaylandDisplay->SyncBegin();

  // There's no pending commit, all changes are sent to compositor.
  mBufferPendingCommit = false;
}

void WindowSurfaceWayland::Commit(const LayoutDeviceIntRegion& aInvalidRegion) {
  MOZ_ASSERT(mIsMainThread == NS_IsMainThread());

#ifdef MOZ_LOGGING
  {
    gfx::IntRect lockSize = aInvalidRegion.GetBounds().ToUnknownRect();
    LOGWAYLAND(
        ("WindowSurfaceWayland::Commit [%p] damage size [%d, %d] -> [%d x %d]"
         "screenSize [%d x %d]\n",
         (void*)this, lockSize.x, lockSize.y, lockSize.width, lockSize.height,
         mLockedScreenRect.width, mLockedScreenRect.height));
    LOGWAYLAND(("    mDrawToWaylandBufferDirectly = %d\n",
                mDrawToWaylandBufferDirectly));
  }
#endif

  if (mDrawToWaylandBufferDirectly) {
    MOZ_ASSERT(mWaylandBuffer->IsLocked());
    mWaylandBufferDamage.OrWith(aInvalidRegion);
    UnlockWaylandBuffer();
  } else {
    CacheImageSurface(aInvalidRegion);
  }

  mBufferCommitAllowed = true;
  CommitWaylandBuffer();
}

void WindowSurfaceWayland::FrameCallbackHandler() {
  MOZ_ASSERT(mIsMainThread == NS_IsMainThread());
  MOZ_ASSERT(mFrameCallback != nullptr,
             "FrameCallbackHandler() called without valid frame callback!");
  MOZ_ASSERT(mLastCommittedSurface != nullptr,
             "FrameCallbackHandler() called without valid wl_surface!");

  LOGWAYLAND(
      ("WindowSurfaceWayland::FrameCallbackHandler [%p]\n", (void*)this));

  wl_callback_destroy(mFrameCallback);
  mFrameCallback = nullptr;

  CommitWaylandBuffer();
}

}  // namespace widget
}  // namespace mozilla
