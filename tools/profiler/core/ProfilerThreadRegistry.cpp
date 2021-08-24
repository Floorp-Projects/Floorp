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

#if !defined(MOZ_GECKO_PROFILER)
// When MOZ_GECKO_PROFILER is not defined, the function definitions in
// platform.cpp are not built, causing link errors. So we keep these simple
// definitions here.

/* static */
void ThreadRegistry::Register(ThreadRegistration::OnThreadRef aOnThreadRef) {
  LockedRegistry lock;
  MOZ_RELEASE_ASSERT(sRegistryContainer.append(OffThreadRef{aOnThreadRef}));
}

/* static */
void ThreadRegistry::Unregister(ThreadRegistration::OnThreadRef aOnThreadRef) {
  LockedRegistry lock;
  for (OffThreadRef& thread : sRegistryContainer) {
    if (thread.IsPointingAt(*aOnThreadRef.mThreadRegistration)) {
      sRegistryContainer.erase(&thread);
      break;
    }
  }
}
#endif  // !defined(MOZ_GECKO_PROFILER)

}  // namespace mozilla::profiler
