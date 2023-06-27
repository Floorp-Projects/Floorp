/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SandboxLaunch_h
#define mozilla_SandboxLaunch_h

#include "base/process_util.h"
#include "mozilla/ipc/UtilityProcessSandboxing.h"
#include "nsXULAppAPI.h"
#include <vector>

namespace mozilla {

// Called in the parent process to set up launch-time aspects of
// sandboxing.  If aType is GeckoProcessType_Content, this must be
// called on the main thread in order to access prefs.
void SandboxLaunchPrepare(GeckoProcessType aType, base::LaunchOptions* aOptions,
                          ipc::SandboxingKind aKind);
#if defined(MOZ_ENABLE_FORKSERVER)
void SandboxLaunchForkServerPrepare(const std::vector<std::string>& aArgv,
                                    base::LaunchOptions& aOptions);
#endif
bool HasAtiDrivers();

}  // namespace mozilla

#endif  // mozilla_SandboxLaunch_h
