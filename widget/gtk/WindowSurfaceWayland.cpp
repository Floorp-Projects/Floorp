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
#include "gfxPlatform.h"
#include "mozcontainer.h"
#include "nsTArray.h"
#include "base/message_loop.h"  // for MessageLoop
#include "base/task.h"          // for NewRunnableMethod, etc

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

bool WindowSurfaceWayland::mUseDMABuf = false;
bool WindowSurfaceWayland::mUseDMABufInitialized = false;

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

----------------------------------------------------------------
When WindowBackBufferDMABuf is used it's similar to
WindowBackBufferShm scheme:

    |
    |
    |
  -----------------------------------         ------------------
  | WindowSurfaceWayland             |<------>| nsWindow       |
  |                                  |        ------------------
  |  --------------------------      |
  |  |WindowBackBufferDMABuf  |      |
  |  |                        |      |
  |  | ---------------------- |      |
  |  | |WaylandDMABufSurface  |      |
  |  | ---------------------- |      |
  |  --------------------------      |
  |                                  |
  |  --------------------------      |
  |  |WindowBackBufferDMABuf  |      |
  |  |                        |      |
  |  | ---------------------- |      |
  |  | |WaylandDMABufSurface  |      |
  |  | ---------------------- |      |
  |  --------------------------      |
  -----------------------------------


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

WindowBackBufferDMABuf

It's WindowBackBuffer implementation based on DMA Buffer.
It owns wl_buffer object, owns WaylandDMABufSurface
(which provides the DMA Buffer) and ties them together.

WindowBackBufferDMABuf backend is used only when WaylandDMABufSurface is
available and widget.wayland_dmabuf_backend.enabled preference is set.

*/

#define EVENT_LOOP_DELAY (1000 / 240)

#define BUFFER_BPP 4
gfx::SurfaceFormat WindowBackBuffer::mFormat = gfx::SurfaceFormat::B8G8R8A8;

nsWaylandDisplay* WindowBackBuffer::GetWaylandDisplay() {
  return mWindowSurfaceWayland->GetWaylandDisplay();
}

int WaylandShmPool::CreateTemporaryFile(int aSize) {
  const char* tmppath = getenv("XDG_RUNTIME_DIR");
  MOZ_RELEASE_ASSERT(tmppath, "Missing XDG_RUNTIME_DIR env variable.");

  nsPrintfCString tmpname("%s/mozilla-shared-XXXXXX", tmppath);

  char* filename;
  int fd = -1;
  int ret = 0;

  if (tmpname.GetMutableData(&filename)) {
    fd = mkstemp(filename);
    if (fd >= 0) {
      int flags = fcntl(fd, F_GETFD);
      if (flags >= 0) {
        fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
      }
    }
  }

  if (fd >= 0) {
    unlink(tmpname.get());
  } else {
    printf_stderr("Unable to create mapping file %s\n", filename);
    MOZ_CRASH();
  }

#ifdef HAVE_POSIX_FALLOCATE
  do {
    ret = posix_fallocate(fd, 0, aSize);
  } while (ret == EINTR);
  if (ret != 0) {
    close(fd);
    MOZ_CRASH_UNSAFE_PRINTF(
        "posix_fallocate() fails on %s size %d error code %d\n", filename,
        aSize, ret);
  }
#else
  do {
    ret = ftruncate(fd, aSize);
  } while (ret < 0 && errno == EINTR);
  if (ret < 0) {
    close(fd);
    MOZ_CRASH_UNSAFE_PRINTF("ftruncate() fails on %s size %d error code %d\n",
                            filename, aSize, ret);
  }
#endif

  return fd;
}

WaylandShmPool::WaylandShmPool(nsWaylandDisplay* aWaylandDisplay, int aSize)
    : mAllocatedSize(aSize) {
  mShmPoolFd = CreateTemporaryFile(mAllocatedSize);
  mImageData = mmap(nullptr, mAllocatedSize, PROT_READ | PROT_WRITE, MAP_SHARED,
                    mShmPoolFd, 0);
  MOZ_RELEASE_ASSERT(mImageData != MAP_FAILED,
                     "Unable to map drawing surface!");

  mShmPool =
      wl_shm_create_pool(aWaylandDisplay->GetShm(), mShmPoolFd, mAllocatedSize);

  // We set our queue to get mShmPool events at compositor thread.
  wl_proxy_set_queue((struct wl_proxy*)mShmPool,
                     aWaylandDisplay->GetEventQueue());
}

bool WaylandShmPool::Resize(int aSize) {
  // We do size increase only
  if (aSize <= mAllocatedSize) return true;

  if (ftruncate(mShmPoolFd, aSize) < 0) return false;

#ifdef HAVE_POSIX_FALLOCATE
  do {
    errno = posix_fallocate(mShmPoolFd, 0, aSize);
  } while (errno == EINTR);
  if (errno != 0) return false;
#endif

  wl_shm_pool_resize(mShmPool, aSize);

  munmap(mImageData, mAllocatedSize);

  mImageData =
      mmap(nullptr, aSize, PROT_READ | PROT_WRITE, MAP_SHARED, mShmPoolFd, 0);
  if (mImageData == MAP_FAILED) return false;

  mAllocatedSize = aSize;
  return true;
}

void WaylandShmPool::SetImageDataFromPool(class WaylandShmPool* aSourcePool,
                                          int aImageDataSize) {
  MOZ_ASSERT(mAllocatedSize >= aImageDataSize, "WaylandShmPool overflows!");
  memcpy(mImageData, aSourcePool->GetImageData(), aImageDataSize);
}

WaylandShmPool::~WaylandShmPool() {
  munmap(mImageData, mAllocatedSize);
  wl_shm_pool_destroy(mShmPool);
  close(mShmPoolFd);
}

static void buffer_release(void* data, wl_buffer* buffer) {
  auto surface = reinterpret_cast<WindowBackBuffer*>(data);
  surface->Detach(buffer);
}

static const struct wl_buffer_listener buffer_listener = {buffer_release};

void WindowBackBufferShm::Create(int aWidth, int aHeight) {
  MOZ_ASSERT(!IsAttached(), "We can't resize attached buffers.");

  int newBufferSize = aWidth * aHeight * BUFFER_BPP;
  mShmPool.Resize(newBufferSize);

  mWaylandBuffer =
      wl_shm_pool_create_buffer(mShmPool.GetShmPool(), 0, aWidth, aHeight,
                                aWidth * BUFFER_BPP, WL_SHM_FORMAT_ARGB8888);
  wl_proxy_set_queue((struct wl_proxy*)mWaylandBuffer,
                     GetWaylandDisplay()->GetEventQueue());
  wl_buffer_add_listener(mWaylandBuffer, &buffer_listener, this);

  mWidth = aWidth;
  mHeight = aHeight;

  LOGWAYLAND((
      "WindowBackBufferShm::Create [%p] wl_buffer %p ID %d\n", (void*)this,
      (void*)mWaylandBuffer,
      mWaylandBuffer ? wl_proxy_get_id((struct wl_proxy*)mWaylandBuffer) : -1));
}

void WindowBackBufferShm::Release() {
  LOGWAYLAND(("WindowBackBufferShm::Release [%p]\n", (void*)this));

  wl_buffer_destroy(mWaylandBuffer);
  mWidth = mHeight = 0;
}

void WindowBackBufferShm::Clear() {
  memset(mShmPool.GetImageData(), 0, mHeight * mWidth * BUFFER_BPP);
}

WindowBackBufferShm::WindowBackBufferShm(
    WindowSurfaceWayland* aWindowSurfaceWayland, int aWidth, int aHeight)
    : WindowBackBuffer(aWindowSurfaceWayland),
      mShmPool(aWindowSurfaceWayland->GetWaylandDisplay(),
               aWidth * aHeight * BUFFER_BPP),
      mWaylandBuffer(nullptr),
      mWidth(aWidth),
      mHeight(aHeight),
      mAttached(false) {
  Create(aWidth, aHeight);
}

WindowBackBufferShm::~WindowBackBufferShm() { Release(); }

bool WindowBackBufferShm::Resize(int aWidth, int aHeight) {
  if (aWidth == mWidth && aHeight == mHeight) return true;

  LOGWAYLAND(("WindowBackBufferShm::Resize [%p] %d %d\n", (void*)this, aWidth,
              aHeight));

  Release();
  Create(aWidth, aHeight);

  return (mWaylandBuffer != nullptr);
}

void WindowBackBuffer::Attach(wl_surface* aSurface) {
  LOGWAYLAND(
      ("WindowBackBuffer::Attach [%p] wl_surface %p ID %d wl_buffer %p ID %d\n",
       (void*)this, (void*)aSurface,
       aSurface ? wl_proxy_get_id((struct wl_proxy*)aSurface) : -1,
       (void*)GetWlBuffer(),
       GetWlBuffer() ? wl_proxy_get_id((struct wl_proxy*)GetWlBuffer()) : -1));

  wl_surface_attach(aSurface, GetWlBuffer(), 0, 0);
  wl_surface_commit(aSurface);
  wl_display_flush(WaylandDisplayGetWLDisplay());
  SetAttached();
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
    Resize(sourceBuffer->mWidth, sourceBuffer->mHeight);
  }

  mShmPool.SetImageDataFromPool(
      &sourceBuffer->mShmPool,
      sourceBuffer->mWidth * sourceBuffer->mHeight * BUFFER_BPP);
  return true;
}

already_AddRefed<gfx::DrawTarget> WindowBackBufferShm::Lock() {
  LOGWAYLAND((
      "WindowBackBufferShm::Lock [%p] [%d x %d] wl_buffer %p ID %d\n",
      (void*)this, mWidth, mHeight, (void*)mWaylandBuffer,
      mWaylandBuffer ? wl_proxy_get_id((struct wl_proxy*)mWaylandBuffer) : -1));

  gfx::IntSize lockSize(mWidth, mHeight);
  mIsLocked = true;
  return gfxPlatform::CreateDrawTargetForData(
      static_cast<unsigned char*>(mShmPool.GetImageData()), lockSize,
      BUFFER_BPP * mWidth, GetSurfaceFormat());
}

WindowBackBufferDMABuf::WindowBackBufferDMABuf(
    WindowSurfaceWayland* aWindowSurfaceWayland, int aWidth, int aHeight)
    : WindowBackBuffer(aWindowSurfaceWayland) {
  mDMAbufSurface.Create(aWidth, aHeight,
                        DMABUF_ALPHA | DMABUF_CREATE_WL_BUFFER);

  LOGWAYLAND(
      ("WindowBackBufferDMABuf::WindowBackBufferDMABuf [%p] Created DMABuf "
       "buffer [%d x %d]\n",
       (void*)this, aWidth, aHeight));
}

WindowBackBufferDMABuf::~WindowBackBufferDMABuf() { mDMAbufSurface.Release(); }

already_AddRefed<gfx::DrawTarget> WindowBackBufferDMABuf::Lock() {
  LOGWAYLAND(
      ("WindowBackBufferDMABuf::Lock [%p] [%d x %d] wl_buffer %p ID %d\n",
       (void*)this, GetWidth(), GetHeight(), (void*)GetWlBuffer(),
       GetWlBuffer() ? wl_proxy_get_id((struct wl_proxy*)GetWlBuffer()) : -1));

  uint32_t stride;
  void* pixels = mDMAbufSurface.Map(&stride);
  gfx::IntSize lockSize(GetWidth(), GetHeight());
  return gfxPlatform::CreateDrawTargetForData(
      static_cast<unsigned char*>(pixels), lockSize, stride,
      GetSurfaceFormat());
}

void WindowBackBufferDMABuf::Unlock() { mDMAbufSurface.Unmap(); }

bool WindowBackBufferDMABuf::IsAttached() {
  return mDMAbufSurface.WLBufferIsAttached();
}

void WindowBackBufferDMABuf::SetAttached() {
  return mDMAbufSurface.WLBufferSetAttached();
}

int WindowBackBufferDMABuf::GetWidth() { return mDMAbufSurface.GetWidth(); }

int WindowBackBufferDMABuf::GetHeight() { return mDMAbufSurface.GetHeight(); }

wl_buffer* WindowBackBufferDMABuf::GetWlBuffer() {
  return mDMAbufSurface.GetWLBuffer();
}

bool WindowBackBufferDMABuf::IsLocked() { return mDMAbufSurface.IsMapped(); }

bool WindowBackBufferDMABuf::Resize(int aWidth, int aHeight) {
  return mDMAbufSurface.Resize(aWidth, aHeight);
}

bool WindowBackBufferDMABuf::SetImageDataFromBuffer(
    class WindowBackBuffer* aSourceBuffer) {
  WindowBackBufferDMABuf* source =
      static_cast<WindowBackBufferDMABuf*>(aSourceBuffer);
  mDMAbufSurface.CopyFrom(&source->mDMAbufSurface);
  return true;
}

void WindowBackBufferDMABuf::Detach(wl_buffer* aBuffer) {
  mDMAbufSurface.WLBufferDetach();

  // Commit any potential cached drawings from latest Lock()/Commit() cycle.
  mWindowSurfaceWayland->CommitWaylandBuffer();
}

void WindowBackBufferDMABuf::Clear() { mDMAbufSurface.Clear(); }

static void frame_callback_handler(void* data, struct wl_callback* callback,
                                   uint32_t time) {
  auto surface = reinterpret_cast<WindowSurfaceWayland*>(data);
  surface->FrameCallbackHandler();

  gfxPlatformGtk::GetPlatform()->SetWaylandLastVsync(time);
}

static const struct wl_callback_listener frame_listener = {
    frame_callback_handler};

WindowSurfaceWayland::WindowSurfaceWayland(nsWindow* aWindow)
    : mWindow(aWindow),
      mWaylandDisplay(WaylandDisplayGet()),
      mWaylandBuffer(nullptr),
      mFrameCallback(nullptr),
      mLastCommittedSurface(nullptr),
      mDelayedCommitHandle(nullptr),
      mLastCommitTime(0),
      mDrawToWaylandBufferDirectly(true),
      mBufferPendingCommit(false),
      mBufferCommitAllowed(false),
      mWholeWindowBufferDamage(false),
      mBufferNeedsClear(false),
      mIsMainThread(NS_IsMainThread()),
      mNeedScaleFactorUpdate(true) {
  for (int i = 0; i < BACK_BUFFER_NUM; i++) mBackupBuffer[i] = nullptr;
  mRenderingCacheMode = static_cast<RenderingCacheMode>(
      mWaylandDisplay->GetRenderingCacheModePref());
}

WindowSurfaceWayland::~WindowSurfaceWayland() {
  if (mBufferPendingCommit) {
    NS_WARNING("Deleted WindowSurfaceWayland with a pending commit!");
  }

  if (mDelayedCommitHandle) {
    // Delete reference to this to prevent WaylandBufferDelayCommitHandler()
    // operate on released this. mDelayedCommitHandle itself will
    // be released at WaylandBufferDelayCommitHandler().
    *mDelayedCommitHandle = nullptr;
  }

  if (mFrameCallback) {
    wl_callback_destroy(mFrameCallback);
  }

  delete mWaylandBuffer;

  for (int i = 0; i < BACK_BUFFER_NUM; i++) {
    if (mBackupBuffer[i]) {
      delete mBackupBuffer[i];
    }
  }
}

bool WindowSurfaceWayland::UseDMABufBackend() {
  if (!mUseDMABufInitialized) {
    mUseDMABuf = nsWaylandDisplay::IsDMABufEnabled();
    LOGWAYLAND(("WindowSurfaceWayland::UseDMABufBackend DMABuf state %d\n",
                mUseDMABuf));
    mUseDMABufInitialized = true;
  }
  return mUseDMABuf;
}

WindowBackBuffer* WindowSurfaceWayland::CreateWaylandBuffer(int aWidth,
                                                            int aHeight) {
  if (UseDMABufBackend()) {
    static bool sDMABufBufferCreated = false;
    WindowBackBuffer* buffer =
        new WindowBackBufferDMABuf(this, aWidth, aHeight);
    if (buffer) {
      sDMABufBufferCreated = true;
      return buffer;
    }
    // If this is the first failure and there's no dmabuf already active
    // we can safely fallback to Shm. Otherwise we can't mix DMAbuf and
    // SHM buffers so just fails now.
    if (sDMABufBufferCreated) {
      NS_WARNING("Failed to allocate DMABuf buffer!");
      return nullptr;
    } else {
      NS_WARNING("Wayland DMABuf failed, switched back to Shm backend!");
      mUseDMABuf = false;
    }
  }

  return new WindowBackBufferShm(this, aWidth, aHeight);
}

WindowBackBuffer* WindowSurfaceWayland::GetWaylandBufferToDraw(
    bool aCanSwitchBuffer) {
  LOGWAYLAND(
      ("WindowSurfaceWayland::GetWaylandBufferToDraw [%p] Requested buffer [%d "
       "x %d]\n",
       (void*)this, mBufferScreenRect.width, mBufferScreenRect.height));

  // There's no buffer created yet, create a new one.
  if (!mWaylandBuffer) {
    MOZ_ASSERT(aCanSwitchBuffer && mWholeWindowBufferDamage,
               "Created new buffer for partial drawing!");
    LOGWAYLAND(("    Created new buffer [%d x %d]\n", mBufferScreenRect.width,
                mBufferScreenRect.height));

    mWaylandBuffer =
        CreateWaylandBuffer(mBufferScreenRect.width, mBufferScreenRect.height);
    mNeedScaleFactorUpdate = true;
    return mWaylandBuffer;
  }

#ifdef DEBUG
  if (mWaylandBuffer->IsAttached()) {
    LOGWAYLAND(("    Buffer %p is attached, need to find a new one.\n",
                mWaylandBuffer));
  }
#endif

  // Reuse existing buffer
  if (!mWaylandBuffer->IsAttached()) {
    LOGWAYLAND(("    Use recent buffer.\n"));

    if (mWaylandBuffer->IsMatchingSize(mBufferScreenRect.width,
                                       mBufferScreenRect.height)) {
      LOGWAYLAND(("    Size is ok, use the buffer [%d x %d]\n",
                  mBufferScreenRect.width, mBufferScreenRect.height));
      return mWaylandBuffer;
    }

    if (!aCanSwitchBuffer) {
      NS_WARNING("We can't resize Wayland buffer for non-fullscreen updates!");
      return nullptr;
    }

    LOGWAYLAND(("    Reuse buffer with resize [%d x %d]\n",
                mBufferScreenRect.width, mBufferScreenRect.height));

    mWaylandBuffer->Resize(mBufferScreenRect.width, mBufferScreenRect.height);
    // There's a chance that scale factor has been changed
    // when buffer size changed
    mNeedScaleFactorUpdate = true;
    return mWaylandBuffer;
  }

  if (!aCanSwitchBuffer) {
    return nullptr;
  }

  // Front buffer is used by compositor, select or create a new back buffer
  int availableBuffer;
  for (availableBuffer = 0; availableBuffer < BACK_BUFFER_NUM;
       availableBuffer++) {
    if (!mBackupBuffer[availableBuffer]) {
      LOGWAYLAND(("    Created new buffer [%d x %d]\n", mBufferScreenRect.width,
                  mBufferScreenRect.height));
      mBackupBuffer[availableBuffer] = CreateWaylandBuffer(
          mBufferScreenRect.width, mBufferScreenRect.height);
      break;
    }

    if (!mBackupBuffer[availableBuffer]->IsAttached()) {
      break;
    }
  }

  if (MOZ_UNLIKELY(availableBuffer == BACK_BUFFER_NUM)) {
    LOGWAYLAND(("    No drawing buffer available!\n"));
    NS_WARNING("No drawing buffer available");
    return nullptr;
  }

  WindowBackBuffer* lastWaylandBuffer = mWaylandBuffer;
  mWaylandBuffer = mBackupBuffer[availableBuffer];
  mBackupBuffer[availableBuffer] = lastWaylandBuffer;

  LOGWAYLAND(("    Buffer flip new back %p new front %p \n",
              (void*)lastWaylandBuffer, (void*)mWaylandBuffer));

  mNeedScaleFactorUpdate = true;

  bool bufferNeedsResize = !mWaylandBuffer->IsMatchingSize(
      mBufferScreenRect.width, mBufferScreenRect.height);
  if (bufferNeedsResize) {
    LOGWAYLAND(("    Resize buffer to [%d x %d]\n", mBufferScreenRect.width,
                mBufferScreenRect.height));
    mWaylandBuffer->Resize(mBufferScreenRect.width, mBufferScreenRect.height);
  }

  return mWaylandBuffer;
}

already_AddRefed<gfx::DrawTarget> WindowSurfaceWayland::LockWaylandBuffer(
    bool aCanSwitchBuffer) {
  WindowBackBuffer* buffer = GetWaylandBufferToDraw(aCanSwitchBuffer);

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

  if (mBufferNeedsClear && mWholeWindowBufferDamage) {
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

  IntRect rect = aUpdatedRegion.RectIter().Get().ToUnknownRect();
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
  return (aScreenRect.width == lockSize.width &&
          aScreenRect.height == lockSize.height);
}

bool WindowSurfaceWayland::CanDrawToWaylandBufferDirectly(
    const LayoutDeviceIntRect& aScreenRect,
    const LayoutDeviceIntRegion& aUpdatedRegion) {
  // whole buffer damage or no cache - we can go direct rendering safely.
  if (mWholeWindowBufferDamage) {
    return true;
  }

  // Let's try to eliminate a buffer copy
  if (mRenderingCacheMode != CACHE_ALL) {
    // There's some cached rendering, we can't throw it away.
    if (mDelayedImageCommits.Length()) {
      return false;
    }

    // More than one regions can overlap and produce flickering/artifacts.
    if (aUpdatedRegion.GetNumRects() > 1) {
      return false;
    }

    gfx::IntRect lockSize = aUpdatedRegion.GetBounds().ToUnknownRect();

    // There's some heuristics here. Let's enable direct rendering for large
    // screen updates like video playback or page scrolling which is bigger
    // than 1/3 of screen.
    if (lockSize.width * 3 > aScreenRect.width &&
        lockSize.height * 3 > aScreenRect.height) {
      return true;
    }
  }
  return false;
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
  gfx::IntRect lockSize = aRegion.GetBounds().ToUnknownRect();

  bool isTransparentPopup =
      mWindow->IsWaylandPopup() &&
      (eTransparencyTransparent == mWindow->GetTransparencyMode());

  // We have request to lock whole buffer/window.
  mWholeWindowBufferDamage =
      isTransparentPopup ? IsPopupFullScreenUpdate(lockedScreenRect, aRegion)
                         : IsWindowFullScreenUpdate(lockedScreenRect, aRegion);

  // Clear buffer when we (re)draw new transparent popup window,
  // otherwise leave it as-is, mBufferNeedsClear can be set from previous
  // (already pending) commits which are cached now.
  if (mWholeWindowBufferDamage) {
    mBufferNeedsClear =
        mWindow->WaylandSurfaceNeedsClear() || isTransparentPopup;
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
  LOGWAYLAND(("   mWholeWindowBufferDamage = %d\n", mWholeWindowBufferDamage));

#if MOZ_LOGGING
  if (!(mBufferScreenRect == lockedScreenRect)) {
    LOGWAYLAND(("   screen size changed\n"));
  }
#endif

  if (!(mBufferScreenRect == lockedScreenRect)) {
    // Screen (window) size changed and we still have some painting pending
    // for the last window size. That can happen when window is resized.
    // We can't commit them any more as they're for former window size, so
    // scratch them.
    mDelayedImageCommits.Clear();

    if (!mWholeWindowBufferDamage) {
      NS_WARNING("Partial screen update when window is resized!");
      // This should not happen. Screen size changed but we got only
      // partal screen update instead of whole screen. Discard this painting
      // as it produces artifacts.
      return nullptr;
    }
    mBufferScreenRect = lockedScreenRect;
  }

  mDrawToWaylandBufferDirectly =
      CanDrawToWaylandBufferDirectly(mBufferScreenRect, aRegion);
  if (mDrawToWaylandBufferDirectly) {
    LOGWAYLAND(("   Direct drawing\n"));
    // If there's any pending image commit scratch them as we're going
    // to redraw the whole sceen anyway.
    if (mWholeWindowBufferDamage) {
      LOGWAYLAND(("   Whole buffer update, clear cache.\n"));
      mDelayedImageCommits.Clear();
    }

    RefPtr<gfx::DrawTarget> dt = LockWaylandBuffer(
        /* aCanSwitchBuffer */ mWholeWindowBufferDamage);
    if (dt) {
      if (!mWholeWindowBufferDamage) {
        DrawDelayedImageCommits(dt, mWaylandBufferDamage);
      }
      return dt.forget();
    }
  }

  // Any caching is disabled and we don't have any back buffer available.
  if (mRenderingCacheMode == CACHE_NONE) {
    return nullptr;
  }

  // We do indirect drawing due to:
  //
  // 1) We don't have any front buffer available. Try indirect drawing
  //    to mImageSurface which is mirrored to front buffer at commit.
  // 2) Only part of the screen is locked. We can't lock entire screen for
  //    such drawing as it produces visible artifacts.
  mDrawToWaylandBufferDirectly = false;

  LOGWAYLAND(("   Indirect drawing.\n"));
  return LockImageSurface(gfx::IntSize(lockSize.XMost(), lockSize.YMost()));
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
  LOGWAYLAND(
      ("WindowSurfaceWayland::DrawDelayedImageCommits [%p]\n", (void*)this));

  for (unsigned int i = 0; i < mDelayedImageCommits.Length(); i++) {
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

  mDelayedImageCommits.AppendElement(
      WindowImageSurface(mImageSurface, aRegion));
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

  RefPtr<gfx::DrawTarget> dt = LockWaylandBuffer(
      /* aCanSwitchBuffer */ mWholeWindowBufferDamage);
  if (!dt) {
    return false;
  }

  LOGWAYLAND(("   Flushing %ld cached WindowImageSurfaces to Wayland buffer\n",
              long(mDelayedImageCommits.Length())));

  DrawDelayedImageCommits(dt, mWaylandBufferDamage);
  UnlockWaylandBuffer();

  return true;
}

static void WaylandBufferDelayCommitHandler(WindowSurfaceWayland** aSurface) {
  if (*aSurface) {
    (*aSurface)->DelayedCommitHandler();
  } else {
    // Referenced WindowSurfaceWayland is already deleted.
    // Do nothing but just release the mDelayedCommitHandle allocated at
    // WindowSurfaceWayland::CommitWaylandBuffer().
    free(aSurface);
  }
}

void WindowSurfaceWayland::CommitWaylandBuffer() {
  MOZ_ASSERT(!mWaylandBuffer->IsAttached(),
             "We can't draw to attached wayland buffer!");

  LOGWAYLAND(("WindowSurfaceWayland::CommitWaylandBuffer [%p]\n", (void*)this));
  LOGWAYLAND(
      ("   mDrawToWaylandBufferDirectly = %d\n", mDrawToWaylandBufferDirectly));
  LOGWAYLAND(("   mWholeWindowBufferDamage = %d\n", mWholeWindowBufferDamage));
  LOGWAYLAND(("   mDelayedCommitHandle = %p\n", mDelayedCommitHandle));
  LOGWAYLAND(("   mFrameCallback = %p\n", mFrameCallback));
  LOGWAYLAND(("   mLastCommittedSurface = %p\n", mLastCommittedSurface));
  LOGWAYLAND(("   mBufferPendingCommit = %d\n", mBufferPendingCommit));
  LOGWAYLAND(("   mBufferCommitAllowed = %d\n", mBufferCommitAllowed));

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

  wl_surface* waylandSurface = mWindow->GetWaylandSurface();
  if (!waylandSurface) {
    LOGWAYLAND(("    [%p] mWindow->GetWaylandSurface() failed, delay commit.\n",
                (void*)this));

    // Target window is not created yet - delay the commit. This can happen only
    // when the window is newly created and there's no active
    // frame callback pending.
    MOZ_ASSERT(!mFrameCallback || waylandSurface != mLastCommittedSurface,
               "Missing wayland surface at frame callback!");

    // Do nothing if there's already mDelayedCommitHandle pending.
    if (!mDelayedCommitHandle) {
      mDelayedCommitHandle = static_cast<WindowSurfaceWayland**>(
          moz_xmalloc(sizeof(*mDelayedCommitHandle)));
      *mDelayedCommitHandle = this;

      MessageLoop::current()->PostDelayedTask(
          NewRunnableFunction("WaylandBackBufferCommit",
                              &WaylandBufferDelayCommitHandler,
                              mDelayedCommitHandle),
          EVENT_LOOP_DELAY);
    }
    return;
  }
  wl_proxy_set_queue((struct wl_proxy*)waylandSurface,
                     mWaylandDisplay->GetEventQueue());

  // We have an active frame callback request so handle it.
  if (mFrameCallback) {
    if (waylandSurface == mLastCommittedSurface) {
      LOGWAYLAND(("    [%p] wait for frame callback.\n", (void*)this));
      // We have an active frame callback pending from our recent surface.
      // It means we should defer the commit to FrameCallbackHandler().
      return;
    }
    // If our stored wl_surface does not match the actual one it means the frame
    // callback is no longer active and we should release it.
    wl_callback_destroy(mFrameCallback);
    mFrameCallback = nullptr;
    mLastCommittedSurface = nullptr;
  }

  if (mWholeWindowBufferDamage) {
    LOGWAYLAND(("   send whole screen damage\n"));
    wl_surface_damage(waylandSurface, 0, 0, mBufferScreenRect.width,
                      mBufferScreenRect.height);
    mWholeWindowBufferDamage = false;
    mNeedScaleFactorUpdate = true;
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
  mWaylandBufferDamage.SetEmpty();

  mFrameCallback = wl_surface_frame(waylandSurface);
  wl_callback_add_listener(mFrameCallback, &frame_listener, this);

  if (mNeedScaleFactorUpdate || mLastCommittedSurface != waylandSurface) {
    wl_surface_set_buffer_scale(waylandSurface, mWindow->GdkScaleFactor());
    mNeedScaleFactorUpdate = false;
  }

  mWaylandBuffer->Attach(waylandSurface);
  mLastCommittedSurface = waylandSurface;
  mLastCommitTime = g_get_monotonic_time() / 1000;

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
         mBufferScreenRect.width, mBufferScreenRect.height));
    LOGWAYLAND(("    mDrawToWaylandBufferDirectly = %d\n",
                mDrawToWaylandBufferDirectly));
    LOGWAYLAND(
        ("    mWholeWindowBufferDamage = %d\n", mWholeWindowBufferDamage));
  }
#endif

  if (mDrawToWaylandBufferDirectly) {
    MOZ_ASSERT(mWaylandBuffer->IsLocked());
    // If we're not at fullscreen damage add drawing area from aInvalidRegion
    if (!mWholeWindowBufferDamage) {
      mWaylandBufferDamage.OrWith(aInvalidRegion);
    }
    UnlockWaylandBuffer();
    mBufferPendingCommit = true;
  } else {
    MOZ_ASSERT(!mWaylandBuffer->IsLocked(),
               "Drawing to already locked buffer?");
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

void WindowSurfaceWayland::DelayedCommitHandler() {
  MOZ_ASSERT(mDelayedCommitHandle != nullptr, "Missing mDelayedCommitHandle!");

  LOGWAYLAND(
      ("WindowSurfaceWayland::DelayedCommitHandler [%p]\n", (void*)this));

  *mDelayedCommitHandle = nullptr;
  free(mDelayedCommitHandle);
  mDelayedCommitHandle = nullptr;

  CommitWaylandBuffer();
}

}  // namespace widget
}  // namespace mozilla
