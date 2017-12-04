/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WindowSurfaceWayland.h"

#include "base/message_loop.h"          // for MessageLoop
#include "base/task.h"                  // for NewRunnableMethod, etc
#include "nsPrintfCString.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Tools.h"
#include "gfxPlatform.h"
#include "mozcontainer.h"
#include "nsCOMArray.h"
#include "mozilla/StaticMutex.h"

#include <gdk/gdkwayland.h>
#include <sys/mman.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>

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

namespace mozilla {
namespace widget {

#define BUFFER_BPP 4

// TODO: How many rendering threads do we actualy handle?
static nsCOMArray<nsWaylandDisplay> gWaylandDisplays;
static StaticMutex gWaylandDisplaysMutex;

// Each thread which is using wayland connection (wl_display) has to operate
// its own wl_event_queue. Main Firefox thread wl_event_queue is handled
// by Gtk main loop, other threads/wl_event_queue has to be handled by us.
//
// nsWaylandDisplay is our interface to wayland compositor. It provides wayland
// global objects as we need (wl_display, wl_shm) and operates wl_event_queue on
// compositor (not the main) thread.
static nsWaylandDisplay* WaylandDisplayGet(wl_display *aDisplay);
static void WaylandDisplayRelease(wl_display *aDisplay);
static void WaylandDisplayLoop(wl_display *aDisplay);

// TODO: is the 60pfs loop correct?
#define EVENT_LOOP_DELAY (1000/60)

// Get WaylandDisplay for given wl_display and actual calling thread.
static nsWaylandDisplay*
WaylandDisplayGetLocked(wl_display *aDisplay, const StaticMutexAutoLock&)
{
  nsWaylandDisplay* waylandDisplay = nullptr;

  int len = gWaylandDisplays.Count();
  for (int i = 0; i < len; i++) {
    if (gWaylandDisplays[i]->Matches(aDisplay)) {
      waylandDisplay = gWaylandDisplays[i];
      break;
    }
  }

  if (!waylandDisplay) {
    waylandDisplay = new nsWaylandDisplay(aDisplay);
    gWaylandDisplays.AppendObject(waylandDisplay);
  }

  NS_ADDREF(waylandDisplay);
  return waylandDisplay;
}

static nsWaylandDisplay*
WaylandDisplayGet(wl_display *aDisplay)
{
  StaticMutexAutoLock lock(gWaylandDisplaysMutex);
  return WaylandDisplayGetLocked(aDisplay, lock);
}

static bool
WaylandDisplayReleaseLocked(wl_display *aDisplay,
                            const StaticMutexAutoLock&)
{
  int len = gWaylandDisplays.Count();
  for (int i = 0; i < len; i++) {
    if (gWaylandDisplays[i]->Matches(aDisplay)) {
      int rc = gWaylandDisplays[i]->Release();
      // nsCOMArray::AppendObject()/RemoveObjectAt() also call AddRef()/Release()
      // so remove WaylandDisplay when ref count is 1.
      if (rc == 1) {
        gWaylandDisplays.RemoveObjectAt(i);
      }
      return true;
    }
  }
  MOZ_ASSERT(false, "Missing nsWaylandDisplay for this thread!");
  return false;
}

static void
WaylandDisplayRelease(wl_display *aDisplay)
{
  StaticMutexAutoLock lock(gWaylandDisplaysMutex);
  WaylandDisplayReleaseLocked(aDisplay, lock);
}

static void
WaylandDisplayLoopLocked(wl_display* aDisplay,
                         const StaticMutexAutoLock&)
{
  int len = gWaylandDisplays.Count();
  for (int i = 0; i < len; i++) {
    if (gWaylandDisplays[i]->Matches(aDisplay)) {
      if (gWaylandDisplays[i]->DisplayLoop()) {
        MessageLoop::current()->PostDelayedTask(
            NewRunnableFunction(&WaylandDisplayLoop, aDisplay), EVENT_LOOP_DELAY);
      }
      break;
    }
  }
}

static void
WaylandDisplayLoop(wl_display* aDisplay)
{
  MOZ_ASSERT(!NS_IsMainThread());
  StaticMutexAutoLock lock(gWaylandDisplaysMutex);
  WaylandDisplayLoopLocked(aDisplay, lock);
}

static void
global_registry_handler(void *data, wl_registry *registry, uint32_t id,
                        const char *interface, uint32_t version)
{
  if (strcmp(interface, "wl_shm") == 0) {
    auto interface = reinterpret_cast<nsWaylandDisplay *>(data);
    auto shm = static_cast<wl_shm*>(
        wl_registry_bind(registry, id, &wl_shm_interface, 1));
    wl_proxy_set_queue((struct wl_proxy *)shm, interface->GetEventQueue());
    interface->SetShm(shm);
  }
}

static void
global_registry_remover(void *data, wl_registry *registry, uint32_t id)
{
}

static const struct wl_registry_listener registry_listener = {
  global_registry_handler,
  global_registry_remover
};

wl_shm*
nsWaylandDisplay::GetShm()
{
  MOZ_ASSERT(mThreadId == PR_GetCurrentThread());

  if (!mShm) {
    // wl_shm is not provided by Gtk so we need to query wayland directly
    // See weston/simple-shm.c and create_display() for reference.
    wl_registry* registry = wl_display_get_registry(mDisplay);
    wl_registry_add_listener(registry, &registry_listener, this);

    wl_proxy_set_queue((struct wl_proxy *)registry, mEventQueue);
    wl_display_roundtrip_queue(mDisplay, mEventQueue);
    MOZ_RELEASE_ASSERT(mShm, "Wayland registry query failed!");
  }

  return(mShm);
}

bool
nsWaylandDisplay::DisplayLoop()
{
  wl_display_dispatch_queue_pending(mDisplay, mEventQueue);
  return true;
}

bool
nsWaylandDisplay::Matches(wl_display *aDisplay)
{
  return mThreadId == PR_GetCurrentThread() && aDisplay == mDisplay;
}

NS_IMPL_ISUPPORTS(nsWaylandDisplay, nsISupports);

nsWaylandDisplay::nsWaylandDisplay(wl_display *aDisplay)
  : mThreadId(PR_GetCurrentThread())
  // gfx::SurfaceFormat::B8G8R8A8 is a basic Wayland format
  // and is always present.
  , mFormat(gfx::SurfaceFormat::B8G8R8A8)
  , mShm(nullptr)
  , mDisplay(aDisplay)
{
  if (NS_IsMainThread()) {
    // Use default event queue in main thread operated by Gtk+.
    mEventQueue = nullptr;
  } else {
    mEventQueue = wl_display_create_queue(mDisplay);
    MessageLoop::current()->PostTask(NewRunnableFunction(&WaylandDisplayLoop,
                                                         mDisplay));
  }
}

nsWaylandDisplay::~nsWaylandDisplay()
{
  MOZ_ASSERT(mThreadId == PR_GetCurrentThread());
  // Owned by Gtk+, we don't need to release
  mDisplay = nullptr;

  if (mEventQueue) {
    wl_event_queue_destroy(mEventQueue);
    mEventQueue = nullptr;
  }
}

int
WaylandShmPool::CreateTemporaryFile(int aSize)
{
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
  }
#else
  do {
    ret = ftruncate(fd, aSize);
  } while (ret < 0 && errno == EINTR);
  if (ret < 0) {
    close(fd);
  }
#endif
  MOZ_RELEASE_ASSERT(ret == 0, "Mapping file allocation failed.");

  return fd;
}

WaylandShmPool::WaylandShmPool(nsWaylandDisplay* aWaylandDisplay, int aSize)
  : mAllocatedSize(aSize)
{
  mShmPoolFd = CreateTemporaryFile(mAllocatedSize);
  mImageData = mmap(nullptr, mAllocatedSize,
                     PROT_READ | PROT_WRITE, MAP_SHARED, mShmPoolFd, 0);
  MOZ_RELEASE_ASSERT(mImageData != MAP_FAILED,
                     "Unable to map drawing surface!");

  mShmPool = wl_shm_create_pool(aWaylandDisplay->GetShm(),
                                mShmPoolFd, mAllocatedSize);

  // We set our queue to get mShmPool events at compositor thread.
  wl_proxy_set_queue((struct wl_proxy *)mShmPool,
                     aWaylandDisplay->GetEventQueue());
}

bool
WaylandShmPool::Resize(int aSize)
{
  // We do size increase only
  if (aSize <= mAllocatedSize)
    return true;

  if (ftruncate(mShmPoolFd, aSize) < 0)
    return false;

#ifdef HAVE_POSIX_FALLOCATE
  do {
    errno = posix_fallocate(mShmPoolFd, 0, aSize);
  } while (errno == EINTR);
  if (errno != 0)
    return false;
#endif

  wl_shm_pool_resize(mShmPool, aSize);

  munmap(mImageData, mAllocatedSize);

  mImageData = mmap(nullptr, aSize,
                    PROT_READ | PROT_WRITE, MAP_SHARED, mShmPoolFd, 0);
  if (mImageData == MAP_FAILED)
    return false;

  mAllocatedSize = aSize;
  return true;
}

void
WaylandShmPool::SetImageDataFromPool(class WaylandShmPool* aSourcePool,
                                     int aImageDataSize)
{
  MOZ_ASSERT(mAllocatedSize <= aImageDataSize, "WaylandShmPool overflows!");
  memcpy(mImageData, aSourcePool->GetImageData(), aImageDataSize);
}

WaylandShmPool::~WaylandShmPool()
{
  munmap(mImageData, mAllocatedSize);
  wl_shm_pool_destroy(mShmPool);
  close(mShmPoolFd);
}

static void
buffer_release(void *data, wl_buffer *buffer)
{
  auto surface = reinterpret_cast<WindowBackBuffer*>(data);
  surface->Detach();
}

static const struct wl_buffer_listener buffer_listener = {
  buffer_release
};

void WindowBackBuffer::Create(int aWidth, int aHeight)
{
  MOZ_ASSERT(!IsAttached(), "We can't resize attached buffers.");

  int newBufferSize = aWidth*aHeight*BUFFER_BPP;
  mShmPool.Resize(newBufferSize);

  mWaylandBuffer = wl_shm_pool_create_buffer(mShmPool.GetShmPool(), 0,
                                            aWidth, aHeight, aWidth*BUFFER_BPP,
                                            WL_SHM_FORMAT_ARGB8888);
  wl_proxy_set_queue((struct wl_proxy *)mWaylandBuffer,
                     mWaylandDisplay->GetEventQueue());
  wl_buffer_add_listener(mWaylandBuffer, &buffer_listener, this);

  mWidth = aWidth;
  mHeight = aHeight;
}

void WindowBackBuffer::Release()
{
  wl_buffer_destroy(mWaylandBuffer);
  mWidth = mHeight = 0;
}

WindowBackBuffer::WindowBackBuffer(nsWaylandDisplay* aWaylandDisplay,
                                   int aWidth, int aHeight)
 : mShmPool(aWaylandDisplay, aWidth*aHeight*BUFFER_BPP)
  ,mWaylandBuffer(nullptr)
  ,mWidth(aWidth)
  ,mHeight(aHeight)
  ,mAttached(false)
  ,mWaylandDisplay(aWaylandDisplay)
{
  Create(aWidth, aHeight);
}

WindowBackBuffer::~WindowBackBuffer()
{
  Release();
}

bool
WindowBackBuffer::Resize(int aWidth, int aHeight)
{
  if (aWidth == mWidth && aHeight == mHeight)
    return true;

  Release();
  Create(aWidth, aHeight);

  return (mWaylandBuffer != nullptr);
}

void
WindowBackBuffer::Attach(wl_surface* aSurface)
{
  wl_surface_attach(aSurface, mWaylandBuffer, 0, 0);
  wl_surface_commit(aSurface);
  wl_display_flush(mWaylandDisplay->GetDisplay());
  mAttached = true;
}

void
WindowBackBuffer::Detach()
{
  mAttached = false;
}

bool
WindowBackBuffer::SetImageDataFromBackBuffer(
  class WindowBackBuffer* aSourceBuffer)
{
  if (!IsMatchingSize(aSourceBuffer)) {
    Resize(aSourceBuffer->mWidth, aSourceBuffer->mHeight);
  }

  mShmPool.SetImageDataFromPool(aSourceBuffer->mShmPool,
    aSourceBuffer->mWidth * aSourceBuffer->mHeight * BUFFER_BPP);
  return true;
}

already_AddRefed<gfx::DrawTarget>
WindowBackBuffer::Lock(const LayoutDeviceIntRegion& aRegion)
{
  gfx::IntRect bounds = aRegion.GetBounds().ToUnknownRect();
  gfx::IntSize lockSize(bounds.XMost(), bounds.YMost());

  return gfxPlatform::CreateDrawTargetForData(static_cast<unsigned char*>(mShmPool.GetImageData()),
                                              lockSize,
                                              BUFFER_BPP * mWidth,
                                              mWaylandDisplay->GetSurfaceFormat());
}

static void
frame_callback_handler(void *data, struct wl_callback *callback, uint32_t time)
{
  auto surface = reinterpret_cast<WindowSurfaceWayland*>(data);
  surface->FrameCallbackHandler();
}

static const struct wl_callback_listener frame_listener = {
  frame_callback_handler
};

WindowSurfaceWayland::WindowSurfaceWayland(nsWindow *aWindow)
  : mWindow(aWindow)
  , mWaylandDisplay(WaylandDisplayGet(aWidget->GetWaylandDisplay()))
  , mFrontBuffer(nullptr)
  , mBackBuffer(nullptr)
  , mFrameCallback(nullptr)
  , mFrameCallbackSurface(nullptr)
  , mDelayedCommit(false)
  , mFullScreenDamage(false)
  , mIsMainThread(NS_IsMainThread())
{
}

WindowSurfaceWayland::~WindowSurfaceWayland()
{
  delete mFrontBuffer;
  delete mBackBuffer;

  if (mFrameCallback) {
    wl_callback_destroy(mFrameCallback);
  }

  if (!mIsMainThread) {
    // We can be destroyed from main thread even though we was created/used
    // in compositor thread. We have to unref/delete WaylandDisplay in compositor
    // thread then.
    MessageLoop::current()->PostTask(
      NewRunnableFunction(&WaylandDisplayRelease, mWaylandDisplay->GetDisplay()));
  } else {
    WaylandDisplayRelease(mWaylandDisplay->GetDisplay());
  }
}

WindowBackBuffer*
WindowSurfaceWayland::GetBufferToDraw(int aWidth, int aHeight)
{
  if (!mFrontBuffer) {
    mFrontBuffer = new WindowBackBuffer(mWaylandDisplay, aWidth, aHeight);
    mBackBuffer = new WindowBackBuffer(mWaylandDisplay, aWidth, aHeight);
    return mFrontBuffer;
  }

  if (!mFrontBuffer->IsAttached()) {
    if (!mFrontBuffer->IsMatchingSize(aWidth, aHeight)) {
      mFrontBuffer->Resize(aWidth, aHeight);
    }
    return mFrontBuffer;
  }

  // Front buffer is used by compositor, draw to back buffer
  if (mBackBuffer->IsAttached()) {
    NS_WARNING("No drawing buffer available");
    return nullptr;
  }

  MOZ_ASSERT(!mDelayedCommit,
             "Uncommitted buffer switch, screen artifacts ahead.");

  WindowBackBuffer *tmp = mFrontBuffer;
  mFrontBuffer = mBackBuffer;
  mBackBuffer = tmp;

  if (mBackBuffer->IsMatchingSize(aWidth, aHeight)) {
    // Former front buffer has the same size as a requested one.
    // Gecko may expect a content already drawn on screen so copy
    // existing data to the new buffer.
    mFrontBuffer->Sync(mBackBuffer);
    // When buffer switches we need to damage whole screen
    // (https://bugzilla.redhat.com/show_bug.cgi?id=1418260)
    mFullScreenDamage = true;
  } else {
    // Former buffer has different size from the new request. Only resize
    // the new buffer and leave gecko to render new whole content.
    mFrontBuffer->Resize(aWidth, aHeight);
  }

  return mFrontBuffer;
}

already_AddRefed<gfx::DrawTarget>
WindowSurfaceWayland::Lock(const LayoutDeviceIntRegion& aRegion)
{
  MOZ_ASSERT(mIsMainThread == NS_IsMainThread());

  // We allocate back buffer to widget size but return only
  // portion requested by aRegion.
  LayoutDeviceIntRect rect = mWindow->GetBounds();
  WindowBackBuffer* buffer = GetBufferToDraw(rect.width,
                                             rect.height);
  if (!buffer) {
    NS_WARNING("No drawing buffer available");
    return nullptr;
  }

  return buffer->Lock(aRegion);
}

void
WindowSurfaceWayland::Commit(const LayoutDeviceIntRegion& aInvalidRegion)
{
  MOZ_ASSERT(mIsMainThread == NS_IsMainThread());

  wl_surface* waylandSurface = mWindow->GetWaylandSurface();
  if (!waylandSurface) {
    // Target window is already destroyed - don't bother to render there.
    return;
  }
  wl_proxy_set_queue((struct wl_proxy *)waylandSurface,
                     mWaylandDisplay->GetEventQueue());

  if (mFullScreenDamage) {
    LayoutDeviceIntRect rect = mWindow->GetBounds();
    wl_surface_damage(waylandSurface, 0, 0, rect.width, rect.height);
    mFullScreenDamage = false;
  } else {
    for (auto iter = aInvalidRegion.RectIter(); !iter.Done(); iter.Next()) {
      const mozilla::LayoutDeviceIntRect &r = iter.Get();
      wl_surface_damage(waylandSurface, r.x, r.y, r.width, r.height);
    }
  }

  // Frame callback is always connected to actual wl_surface. When the surface
  // is unmapped/deleted the frame callback is never called. Unfortunatelly
  // we don't know if the frame callback is not going to be called.
  // But our mozcontainer code deletes wl_surface when the GdkWindow is hidden
  // creates a new one when is visible.
  if (mFrameCallback && mFrameCallbackSurface == waylandSurface) {
    // Do nothing here - we have a valid wl_surface and the buffer will be
    // commited to compositor in next frame callback event.
    mDelayedCommit = true;
    return;
  } else  {
    if (mFrameCallback) {
      // Delete frame callback connected to obsoleted wl_surface.
      wl_callback_destroy(mFrameCallback);
    }

    mFrameCallback = wl_surface_frame(waylandSurface);
    wl_callback_add_listener(mFrameCallback, &frame_listener, this);
    mFrameCallbackSurface = waylandSurface;

    // Let the wayland know of the current scaling factor for the hdpi
    // displays
    wl_surface_set_buffer_scale(waylandSurface, mWindow->GdkScaleFactor());

    // There's no pending frame callback so we can draw immediately
    // and create frame callback for possible subsequent drawing.
    mFrontBuffer->Attach(waylandSurface);
    mDelayedCommit = false;
  }
}

void
WindowSurfaceWayland::FrameCallbackHandler()
{
  MOZ_ASSERT(mIsMainThread == NS_IsMainThread());

  if (mFrameCallback) {
    wl_callback_destroy(mFrameCallback);
    mFrameCallback = nullptr;
    mFrameCallbackSurface = nullptr;
  }

  if (mDelayedCommit) {
    wl_surface* waylandSurface = mWindow->GetWaylandSurface();
    if (!waylandSurface) {
      // Target window is already destroyed - don't bother to render there.
      NS_WARNING("No drawing buffer available");
      return;
    }
    wl_proxy_set_queue((struct wl_proxy *)waylandSurface,
                       mWaylandDisplay->GetEventQueue());

    // Send pending surface to compositor and register frame callback
    // for possible subsequent drawing.
    mFrameCallback = wl_surface_frame(waylandSurface);
    wl_callback_add_listener(mFrameCallback, &frame_listener, this);
    mFrameCallbackSurface = waylandSurface;

    mFrontBuffer->Attach(waylandSurface);
    mDelayedCommit = false;
  }
}

}  // namespace widget
}  // namespace mozilla
