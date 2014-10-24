/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VsyncDispatcher.h"
#include "mozilla/ClearOnShutdown.h"
#include "CompositorParent.h"

using namespace mozilla::layers;

namespace mozilla {

StaticRefPtr<VsyncDispatcher> sVsyncDispatcher;

/*static*/ VsyncDispatcher*
VsyncDispatcher::GetInstance()
{
  if (!sVsyncDispatcher) {
    sVsyncDispatcher = new VsyncDispatcher();
    ClearOnShutdown(&sVsyncDispatcher);
  }

  return sVsyncDispatcher;
}

VsyncDispatcher::VsyncDispatcher()
{

}

VsyncDispatcher::~VsyncDispatcher()
{
}

void
VsyncDispatcher::AddCompositorVsyncObserver(VsyncObserver* aVsyncObserver)
{
  MOZ_ASSERT(CompositorParent::IsInCompositorThread());
}

void
VsyncDispatcher::RemoveCompositorVsyncObserver(VsyncObserver* aVsyncObserver)
{
  MOZ_ASSERT(CompositorParent::IsInCompositorThread());
}

} // namespace mozilla
