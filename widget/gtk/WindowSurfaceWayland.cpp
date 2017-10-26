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

bool
nsWaylandDisplay::Matches(wl_display *aDisplay)
{
  return mThreadId == PR_GetCurrentThread() && aDisplay == mDisplay;
}

NS_IMPL_ISUPPORTS(nsWaylandDisplay, nsISupports);

nsWaylandDisplay::nsWaylandDisplay(wl_display *aDisplay)
{
  mThreadId = PR_GetCurrentThread();
  mDisplay = aDisplay;

  // gfx::SurfaceFormat::B8G8R8A8 is a basic Wayland format
  // and is always present.
  mFormat = gfx::SurfaceFormat::B8G8R8A8;
}

nsWaylandDisplay::~nsWaylandDisplay()
{
  MOZ_ASSERT(mThreadId == PR_GetCurrentThread());
  // Owned by Gtk+, we don't need to release
  mDisplay = nullptr;
}

}  // namespace widget
}  // namespace mozilla
