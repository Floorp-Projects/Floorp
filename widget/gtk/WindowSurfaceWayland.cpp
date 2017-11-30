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
  , mDisplay(aDisplay)
  , mDisplay(aDisplay)
  // gfx::SurfaceFormat::B8G8R8A8 is a basic Wayland format
  // and is always present.
  , mFormat(gfx::SurfaceFormat::B8G8R8A8)
  , mShm(nullptr)
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

}  // namespace widget
}  // namespace mozilla
