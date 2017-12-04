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

WaylandShmPool::~WaylandShmPool()
{
  munmap(mImageData, mAllocatedSize);
  wl_shm_pool_destroy(mShmPool);
  close(mShmPoolFd);
}

}  // namespace widget
}  // namespace mozilla
