/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ProfilerThreadRegistry.h"

namespace mozilla::profiler {

/* static */
ThreadRegistry::RegistryContainer ThreadRegistry::sRegistryContainer;

/* static */
ThreadRegistry::RegistryMutex ThreadRegistry::sRegistryMutex;

/* static */
void ThreadRegistry::Register(ThreadRegistration::OnThreadRef aOnThreadRef) {
  // TODO in bug 1722261: Move to platform.cpp to interact with profiler code.
  LockedRegistry lock;
  MOZ_RELEASE_ASSERT(sRegistryContainer.append(OffThreadRef{aOnThreadRef}));
}

/* static */
void ThreadRegistry::Unregister(ThreadRegistration::OnThreadRef aOnThreadRef) {
  // TODO in bug 1722261: Move to platform.cpp to interact with profiler code.
  LockedRegistry lock;
  for (OffThreadRef& thread : sRegistryContainer) {
    if (thread.IsPointingAt(*aOnThreadRef.mThreadRegistration)) {
      sRegistryContainer.erase(&thread);
      break;
    }
  }
}

}  // namespace mozilla::profiler
