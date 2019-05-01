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


WindowBackBuffer

Manages one wl_buffer. It owns wl_buffer object, owns WaylandShmPool
(which provides shared memory) and ties them together.

Wl_buffer is a main Wayland object with actual graphics data.
Wl_buffer basically represent one complete window screen.
When double buffering is involved every window (GdkWindow for instance)
utilises two wl_buffers which are cycled. One is filed with data by application
and one is rendered by compositor.


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

void WindowBackBuffer::Create(int aWidth, int aHeight) {
  MOZ_ASSERT(!IsAttached(), "We can't resize attached buffers.");

  int newBufferSize = aWidth * aHeight * BUFFER_BPP;
  mShmPool.Resize(newBufferSize);

  mWaylandBuffer =
      wl_shm_pool_create_buffer(mShmPool.GetShmPool(), 0, aWidth, aHeight,
                                aWidth * BUFFER_BPP, WL_SHM_FORMAT_ARGB8888);
  wl_proxy_set_queue((struct wl_proxy*)mWaylandBuffer,
                     mWaylandDisplay->GetEventQueue());
  wl_buffer_add_listener(mWaylandBuffer, &buffer_listener, this);

  mWidth = aWidth;
  mHeight = aHeight;

  LOGWAYLAND((
      "%s [%p] wl_buffer %p ID %d\n", __PRETTY_FUNCTION__, (void*)this,
      (void*)mWaylandBuffer,
      mWaylandBuffer ? wl_proxy_get_id((struct wl_proxy*)mWaylandBuffer) : -1));
}

void WindowBackBuffer::Release() {
  LOGWAYLAND(("%s [%p]\n", __PRETTY_FUNCTION__, (void*)this));

  wl_buffer_destroy(mWaylandBuffer);
  mWidth = mHeight = 0;
}

void WindowBackBuffer::Clear() {
  memset(mShmPool.GetImageData(), 0, mHeight * mWidth * BUFFER_BPP);
}

WindowBackBuffer::WindowBackBuffer(nsWaylandDisplay* aWaylandDisplay,
                                   int aWidth, int aHeight)
    : mShmPool(aWaylandDisplay, aWidth * aHeight * BUFFER_BPP),
      mWaylandBuffer(nullptr),
      mWidth(aWidth),
      mHeight(aHeight),
      mAttached(false),
      mWaylandDisplay(aWaylandDisplay) {
  Create(aWidth, aHeight);
}

WindowBackBuffer::~WindowBackBuffer() { Release(); }

bool WindowBackBuffer::Resize(int aWidth, int aHeight) {
  if (aWidth == mWidth && aHeight == mHeight) return true;

  LOGWAYLAND(
      ("%s [%p] %d %d\n", __PRETTY_FUNCTION__, (void*)this, aWidth, aHeight));

  Release();
  Create(aWidth, aHeight);

  return (mWaylandBuffer != nullptr);
}

void WindowBackBuffer::Attach(wl_surface* aSurface) {
  LOGWAYLAND((
      "%s [%p] wl_surface %p ID %d wl_buffer %p ID %d\n", __PRETTY_FUNCTION__,
      (void*)this, (void*)aSurface,
      aSurface ? wl_proxy_get_id((struct wl_proxy*)aSurface) : -1,
      (void*)mWaylandBuffer,
      mWaylandBuffer ? wl_proxy_get_id((struct wl_proxy*)mWaylandBuffer) : -1));

  wl_surface_attach(aSurface, mWaylandBuffer, 0, 0);
  wl_surface_commit(aSurface);
  wl_display_flush(mWaylandDisplay->GetDisplay());
  mAttached = true;
}

void WindowBackBuffer::Detach(wl_buffer* aBuffer) {
  LOGWAYLAND(("%s [%p] wl_buffer %p ID %d\n", __PRETTY_FUNCTION__, (void*)this,
              (void*)aBuffer,
              aBuffer ? wl_proxy_get_id((struct wl_proxy*)aBuffer) : -1));

  mAttached = false;
}

bool WindowBackBuffer::SetImageDataFromBuffer(
    class WindowBackBuffer* aSourceBuffer) {
  if (!IsMatchingSize(aSourceBuffer)) {
    Resize(aSourceBuffer->mWidth, aSourceBuffer->mHeight);
  }

  mShmPool.SetImageDataFromPool(
      &aSourceBuffer->mShmPool,
      aSourceBuffer->mWidth * aSourceBuffer->mHeight * BUFFER_BPP);
  return true;
}

already_AddRefed<gfx::DrawTarget> WindowBackBuffer::Lock() {
  LOGWAYLAND((
      "%s [%p] [%d x %d] wl_buffer %p ID %d\n", __PRETTY_FUNCTION__,
      (void*)this, mWidth, mHeight, (void*)mWaylandBuffer,
      mWaylandBuffer ? wl_proxy_get_id((struct wl_proxy*)mWaylandBuffer) : -1));

  gfx::IntSize lockSize(mWidth, mHeight);
  return gfxPlatform::CreateDrawTargetForData(
      static_cast<unsigned char*>(mShmPool.GetImageData()), lockSize,
      BUFFER_BPP * mWidth, mFormat);
}

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
      mDisplayThreadMessageLoop(MessageLoop::current()),
      mDelayedCommitHandle(nullptr),
      mDrawToWaylandBufferDirectly(true),
      mPendingCommit(false),
      mWaylandBufferFullScreenDamage(false),
      mIsMainThread(NS_IsMainThread()),
      mNeedScaleFactorUpdate(true),
      mWaitToFullScreenUpdate(true) {
  for (int i = 0; i < BACK_BUFFER_NUM; i++) mBackupBuffer[i] = nullptr;
}

WindowSurfaceWayland::~WindowSurfaceWayland() {
  if (mPendingCommit) {
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

WindowBackBuffer* WindowSurfaceWayland::GetWaylandBufferToDraw(int aWidth,
                                                               int aHeight) {
  if (!mWaylandBuffer) {
    LOGWAYLAND(("%s [%p] Create [%d x %d]\n", __PRETTY_FUNCTION__, (void*)this,
                aWidth, aHeight));

    mWaylandBuffer = new WindowBackBuffer(mWaylandDisplay, aWidth, aHeight);
    mWaitToFullScreenUpdate = true;
    return mWaylandBuffer;
  }

  if (!mWaylandBuffer->IsAttached()) {
    if (!mWaylandBuffer->IsMatchingSize(aWidth, aHeight)) {
      mWaylandBuffer->Resize(aWidth, aHeight);
      // There's a chance that scale factor has been changed
      // when buffer size changed
      mWaitToFullScreenUpdate = true;
    }
    LOGWAYLAND(("%s [%p] Reuse buffer [%d x %d]\n", __PRETTY_FUNCTION__,
                (void*)this, aWidth, aHeight));

    return mWaylandBuffer;
  }

  MOZ_ASSERT(!mPendingCommit,
             "Uncommitted buffer switch, screen artifacts ahead.");

  // Front buffer is used by compositor, select a back buffer
  int availableBuffer;
  for (availableBuffer = 0; availableBuffer < BACK_BUFFER_NUM;
       availableBuffer++) {
    if (!mBackupBuffer[availableBuffer]) {
      mBackupBuffer[availableBuffer] =
          new WindowBackBuffer(mWaylandDisplay, aWidth, aHeight);
      break;
    }

    if (!mBackupBuffer[availableBuffer]->IsAttached()) {
      break;
    }
  }

  if (MOZ_UNLIKELY(availableBuffer == BACK_BUFFER_NUM)) {
    LOGWAYLAND(("%s [%p] No drawing buffer available!\n", __PRETTY_FUNCTION__,
                (void*)this));
    NS_WARNING("No drawing buffer available");
    return nullptr;
  }

  WindowBackBuffer* lastWaylandBuffer = mWaylandBuffer;
  mWaylandBuffer = mBackupBuffer[availableBuffer];
  mBackupBuffer[availableBuffer] = lastWaylandBuffer;

  if (lastWaylandBuffer->IsMatchingSize(aWidth, aHeight)) {
    LOGWAYLAND(("%s [%p] Copy from old buffer [%d x %d]\n", __PRETTY_FUNCTION__,
                (void*)this, aWidth, aHeight));
    // Former front buffer has the same size as a requested one.
    // Gecko may expect a content already drawn on screen so copy
    // existing data to the new buffer.
    mWaylandBuffer->SetImageDataFromBuffer(lastWaylandBuffer);
    // When buffer switches we need to damage whole screen
    // (https://bugzilla.redhat.com/show_bug.cgi?id=1418260)
    mWaylandBufferFullScreenDamage = true;
  } else {
    LOGWAYLAND(("%s [%p] Resize to [%d x %d]\n", __PRETTY_FUNCTION__,
                (void*)this, aWidth, aHeight));
    // Former buffer has different size from the new request. Only resize
    // the new buffer and leave gecko to render new whole content.
    mWaylandBuffer->Resize(aWidth, aHeight);
    mWaitToFullScreenUpdate = true;
  }

  return mWaylandBuffer;
}

already_AddRefed<gfx::DrawTarget> WindowSurfaceWayland::LockWaylandBuffer(
    int aWidth, int aHeight, bool aClearBuffer) {
  WindowBackBuffer* buffer = GetWaylandBufferToDraw(aWidth, aHeight);
  if (!buffer) {
    NS_WARNING(
        "WindowSurfaceWayland::LockWaylandBuffer(): No buffer available");
    return nullptr;
  }

  if (aClearBuffer) {
    buffer->Clear();
  }

  return buffer->Lock();
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

/*
  There are some situations which can happen here:

  A) Lock() is called to whole surface. In that case we don't need
     to clip/buffer the drawing and we can return wl_buffer directly
     for drawing.
       - mWaylandBuffer is available - that's an ideal situation.
       - mWaylandBuffer is locked by compositor - flip buffers and draw.
          - if we can't flip buffers - go B)

  B) Lock() is requested for part(s) of screen. We need to provide temporary
     surface to draw into and copy result (clipped) to target wl_surface.
 */
already_AddRefed<gfx::DrawTarget> WindowSurfaceWayland::Lock(
    const LayoutDeviceIntRegion& aRegion) {
  MOZ_ASSERT(mIsMainThread == NS_IsMainThread());

  LayoutDeviceIntRect screenRect = mWindow->GetBounds();
  gfx::IntRect bounds = aRegion.GetBounds().ToUnknownRect();
  gfx::IntSize lockSize(bounds.XMost(), bounds.YMost());

  LOGWAYLAND(("%s [%p] lockSize [%d x %d] screenSize [%d x %d]\n",
              __PRETTY_FUNCTION__, (void*)this, lockSize.width, lockSize.height,
              screenRect.width, lockSize.height));

  // Are we asked for entire nsWindow to draw?
  mDrawToWaylandBufferDirectly =
      (aRegion.GetNumRects() == 1 && bounds.x == 0 && bounds.y == 0 &&
       lockSize.width == screenRect.width &&
       lockSize.height == screenRect.height);

  if (mDrawToWaylandBufferDirectly) {
    RefPtr<gfx::DrawTarget> dt =
        LockWaylandBuffer(screenRect.width, screenRect.height,
                          mWindow->WaylandSurfaceNeedsClear());
    if (dt) {
      // When we have a request to update whole screen at once
      // (surface was created, resized or changed somehow)
      // we also need update scale factor of the screen.
      if (mWaitToFullScreenUpdate) {
        mWaitToFullScreenUpdate = false;
        mNeedScaleFactorUpdate = true;
      }
      return dt.forget();
    }

    // We don't have any front buffer available. Try indirect drawing
    // to mImageSurface which is mirrored to front buffer at commit.
    mDrawToWaylandBufferDirectly = false;
  }

  return LockImageSurface(lockSize);
}

bool WindowSurfaceWayland::CommitImageSurfaceToWaylandBuffer(
    const LayoutDeviceIntRegion& aRegion) {
  MOZ_ASSERT(!mDrawToWaylandBufferDirectly);

  LayoutDeviceIntRect screenRect = mWindow->GetBounds();
  gfx::IntRect bounds = aRegion.GetBounds().ToUnknownRect();

  gfx::Rect rect(bounds);
  if (rect.IsEmpty()) {
    return false;
  }

  RefPtr<gfx::DrawTarget> dt = LockWaylandBuffer(
      screenRect.width, screenRect.height, mWindow->WaylandSurfaceNeedsClear());
  RefPtr<gfx::SourceSurface> surf =
      gfx::Factory::CreateSourceSurfaceForCairoSurface(
          mImageSurface->CairoSurface(), mImageSurface->GetSize(),
          mImageSurface->Format());
  if (!dt || !surf) {
    return false;
  }

  uint32_t numRects = aRegion.GetNumRects();
  if (numRects != 1) {
    AutoTArray<IntRect, 32> rects;
    rects.SetCapacity(numRects);
    for (auto iter = aRegion.RectIter(); !iter.Done(); iter.Next()) {
      rects.AppendElement(iter.Get().ToUnknownRect());
    }
    dt->PushDeviceSpaceClipRects(rects.Elements(), rects.Length());
  }

  dt->DrawSurface(surf, rect, rect);

  if (numRects != 1) {
    dt->PopClip();
  }

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

void WindowSurfaceWayland::CalcRectScale(LayoutDeviceIntRect& aRect,
                                         int aScale) {
  aRect.x = aRect.x / aScale;
  aRect.y = aRect.y / aScale;

  // We don't need exact damage size - just safely cover the round errors.
  aRect.width = (aRect.width / aScale) + 2;
  aRect.height = (aRect.height / aScale) + 2;
}

void WindowSurfaceWayland::CommitWaylandBuffer() {
  MOZ_ASSERT(mPendingCommit, "Committing empty surface!");

  if (mWaitToFullScreenUpdate) {
    return;
  }

  wl_surface* waylandSurface = mWindow->GetWaylandSurface();
  if (!waylandSurface) {
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

  if (mWaylandBufferFullScreenDamage) {
    LayoutDeviceIntRect rect = mWindow->GetBounds();
    wl_surface_damage(waylandSurface, 0, 0, rect.width, rect.height);
    mWaylandBufferFullScreenDamage = false;
    mNeedScaleFactorUpdate = true;
  } else {
    gint scaleFactor = mWindow->GdkScaleFactor();
    for (auto iter = mWaylandBufferDamage.RectIter(); !iter.Done();
         iter.Next()) {
      mozilla::LayoutDeviceIntRect r = iter.Get();
      // We need to remove the scale factor because the wl_surface_damage
      // also multiplies by current  scale factor.
      if (scaleFactor > 1) {
        CalcRectScale(r, scaleFactor);
      }
      wl_surface_damage(waylandSurface, r.x, r.y, r.width, r.height);
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

  // There's no pending commit, all changes are sent to compositor.
  mPendingCommit = false;
}

void WindowSurfaceWayland::Commit(const LayoutDeviceIntRegion& aInvalidRegion) {
  MOZ_ASSERT(mIsMainThread == NS_IsMainThread());

#ifdef DEBUG
  {
    LayoutDeviceIntRect screenRect = mWindow->GetBounds();
    gfx::IntRect bounds = aInvalidRegion.GetBounds().ToUnknownRect();
    gfx::IntSize lockSize(bounds.XMost(), bounds.YMost());

    LOGWAYLAND(("%s [%p] lockSize [%d x %d] screenSize [%d x %d]\n",
                __PRETTY_FUNCTION__, (void*)this, lockSize.width,
                lockSize.height, screenRect.width, lockSize.height));
  }
#endif

  // We have new content at mImageSurface - copy data to mWaylandBuffer first.
  if (!mDrawToWaylandBufferDirectly) {
    CommitImageSurfaceToWaylandBuffer(aInvalidRegion);
  }

  // If we're not at fullscreen damage add drawing area from aInvalidRegion
  if (!mWaylandBufferFullScreenDamage) {
    mWaylandBufferDamage.OrWith(aInvalidRegion);
  }

  // We're ready to commit.
  mPendingCommit = true;
  CommitWaylandBuffer();
}

void WindowSurfaceWayland::FrameCallbackHandler() {
  MOZ_ASSERT(mIsMainThread == NS_IsMainThread());
  MOZ_ASSERT(mFrameCallback != nullptr,
             "FrameCallbackHandler() called without valid frame callback!");
  MOZ_ASSERT(mLastCommittedSurface != nullptr,
             "FrameCallbackHandler() called without valid wl_surface!");

  wl_callback_destroy(mFrameCallback);
  mFrameCallback = nullptr;

  if (mPendingCommit) {
    CommitWaylandBuffer();
  }
}

void WindowSurfaceWayland::DelayedCommitHandler() {
  MOZ_ASSERT(mDelayedCommitHandle != nullptr, "Missing mDelayedCommitHandle!");

  *mDelayedCommitHandle = nullptr;
  free(mDelayedCommitHandle);
  mDelayedCommitHandle = nullptr;

  if (mPendingCommit) {
    CommitWaylandBuffer();
  }
}

}  // namespace widget
}  // namespace mozilla
