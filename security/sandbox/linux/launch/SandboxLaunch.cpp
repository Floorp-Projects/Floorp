/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SandboxLaunch.h"

#include "mozilla/Assertions.h"
#include "mozilla/SandboxReporter.h"
#include "nsString.h"
#include "prenv.h"

namespace mozilla {

static void
PreloadSandboxLib(base::environment_map* aEnv)
{
  // Preload libmozsandbox.so so that sandbox-related interpositions
  // can be defined there instead of in the executable.
  // (This could be made conditional on intent to use sandboxing, but
  // it's harmless for non-sandboxed processes.)
  nsAutoCString preload;
  // Prepend this, because people can and do preload libpthread.
  // (See bug 1222500.)
  preload.AssignLiteral("libmozsandbox.so");
  if (const char* oldPreload = PR_GetEnv("LD_PRELOAD")) {
    // Doesn't matter if oldPreload is ""; extra separators are ignored.
    preload.Append(' ');
    preload.Append(oldPreload);
  }
  MOZ_ASSERT(aEnv->count("LD_PRELOAD") == 0);
  (*aEnv)["LD_PRELOAD"] = preload.get();
}

static void
AttachSandboxReporter(base::file_handle_mapping_vector* aFdMap)
{
  int srcFd, dstFd;
  SandboxReporter::Singleton()
    ->GetClientFileDescriptorMapping(&srcFd, &dstFd);
  aFdMap->push_back({srcFd, dstFd});
}

void
SandboxLaunchPrepare(GeckoProcessType aType,
		     base::LaunchOptions* aOptions)
{
  PreloadSandboxLib(&aOptions->env_map);
  AttachSandboxReporter(&aOptions->fds_to_remap);

  // aType will be used in bug 1401062 to take over the functionality
  // of SandboxEarlyInit
}


} // namespace mozilla
