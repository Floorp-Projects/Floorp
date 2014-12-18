/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VsyncDispatcher.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/layers/CompositorParent.h"
#include "gfxPrefs.h"
#include "gfxPlatform.h"
#include "VsyncSource.h"

#ifdef MOZ_ENABLE_PROFILER_SPS
#include "GeckoProfiler.h"
#include "ProfilerMarkers.h"
#endif

#ifdef MOZ_WIDGET_GONK
#include "GeckoTouchDispatcher.h"
#endif

using namespace mozilla::layers;

namespace mozilla {

VsyncDispatcher::VsyncDispatcher()
  : mCompositorObserverLock("CompositorObserverLock")
{
  MOZ_ASSERT(XRE_IsParentProcess());
  gfxPlatform::GetPlatform()->GetHardwareVsync()->AddVsyncDispatcher(this);
}

VsyncDispatcher::~VsyncDispatcher()
{
  // We auto remove this vsync dispatcher from the vsync source in the nsBaseWidget
  MOZ_ASSERT(NS_IsMainThread());
}

void
VsyncDispatcher::NotifyVsync(TimeStamp aVsyncTimestamp)
{
  // In hardware vsync thread
#ifdef MOZ_ENABLE_PROFILER_SPS
    if (profiler_is_active()) {
        CompositorParent::PostInsertVsyncProfilerMarker(aVsyncTimestamp);
    }
#endif

  MutexAutoLock lock(mCompositorObserverLock);
  if (gfxPrefs::VsyncAlignedCompositor() && mCompositorVsyncObserver) {
    mCompositorVsyncObserver->NotifyVsync(aVsyncTimestamp);
  }
}

void
VsyncDispatcher::SetCompositorVsyncObserver(VsyncObserver* aVsyncObserver)
{
  MOZ_ASSERT(CompositorParent::IsInCompositorThread());
  MutexAutoLock lock(mCompositorObserverLock);
  mCompositorVsyncObserver = aVsyncObserver;
}

void
VsyncDispatcher::Shutdown()
{
  // Need to explicitly remove VsyncDispatcher when the nsBaseWidget shuts down.
  // Otherwise, we would get dead vsync notifications between when the nsBaseWidget
  // shuts down and the CompositorParent shuts down.
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());
  gfxPlatform::GetPlatform()->GetHardwareVsync()->RemoveVsyncDispatcher(this);
}
} // namespace mozilla
