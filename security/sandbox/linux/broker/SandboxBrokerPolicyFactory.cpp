/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SandboxBrokerPolicyFactory.h"
#include "SandboxInfo.h"
#include "SandboxLogging.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Preferences.h"
#include "mozilla/SandboxSettings.h"
#include "nsPrintfCString.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "SpecialSystemDirectory.h"

#ifdef ANDROID
#include "cutils/properties.h"
#endif

#ifdef MOZ_WIDGET_GTK
#include <glib.h>
#endif

namespace mozilla {

#if defined(MOZ_CONTENT_SANDBOX)
namespace {
static const int rdonly = SandboxBroker::MAY_READ;
static const int wronly = SandboxBroker::MAY_WRITE;
static const int rdwr = rdonly | wronly;
static const int rdwrcr = rdwr | SandboxBroker::MAY_CREATE;
}
#endif

SandboxBrokerPolicyFactory::SandboxBrokerPolicyFactory()
{
  // Policy entries that are the same in every process go here, and
  // are cached over the lifetime of the factory.
#if defined(MOZ_CONTENT_SANDBOX)
  SandboxBroker::Policy* policy = new SandboxBroker::Policy;
  policy->AddDir(rdonly, "/");
  policy->AddDir(rdwrcr, "/dev/shm");
  // Add write permissions on the temporary directory. This can come
  // from various environment variables (TMPDIR,TMP,TEMP,...) so
  // make sure to use the full logic.
  nsCOMPtr<nsIFile> tmpDir;
  nsresult rv = GetSpecialSystemDirectory(OS_TemporaryDirectory,
                                          getter_AddRefs(tmpDir));
  if (NS_SUCCEEDED(rv)) {
    nsAutoCString tmpPath;
    rv = tmpDir->GetNativePath(tmpPath);
    if (NS_SUCCEEDED(rv)) {
      policy->AddDir(rdwrcr, tmpPath.get());
    }
  }
  // If the above fails at any point, fall back to a very good guess.
  if (NS_FAILED(rv)) {
    policy->AddDir(rdwrcr, "/tmp");
  }

  // Bug 1308851: NVIDIA proprietary driver when using WebGL
  policy->AddFilePrefix(rdwr, "/dev", "nvidia");

  // Bug 1312678: radeonsi/Intel with DRI when using WebGL
  policy->AddDir(rdwr, "/dev/dri");

#ifdef MOZ_ALSA
  // Bug 1309098: ALSA support
  policy->AddDir(rdwr, "/dev/snd");
#endif

#ifdef MOZ_WIDGET_GTK
  // Bug 1321134: DConf's single bit of shared memory
  if (const auto userDir = g_get_user_runtime_dir()) {
    // The leaf filename is "user" by default, but is configurable.
    nsPrintfCString shmPath("%s/dconf/", userDir);
    policy->AddPrefix(rdwrcr, shmPath.get());
  }
#endif

  mCommonContentPolicy.reset(policy);
#endif
}

#ifdef MOZ_CONTENT_SANDBOX
UniquePtr<SandboxBroker::Policy>
SandboxBrokerPolicyFactory::GetContentPolicy(int aPid)
{
  // Policy entries that vary per-process (currently the only reason
  // that can happen is because they contain the pid) are added here.

  MOZ_ASSERT(NS_IsMainThread());
  // File broker usage is controlled through a pref.
  if (GetEffectiveContentSandboxLevel() <= 1) {
    return nullptr;
  }

  MOZ_ASSERT(mCommonContentPolicy);
  UniquePtr<SandboxBroker::Policy>
    policy(new SandboxBroker::Policy(*mCommonContentPolicy));

  // Now read any extra paths, this requires accessing user preferences
  // so we can only do it now. Our constructor is initialized before
  // user preferences are read in.
  nsAdoptingCString extraPathString =
    Preferences::GetCString("security.sandbox.content.write_path_whitelist");
  if (extraPathString) {
    for (const nsACString& path : extraPathString.Split(',')) {
      nsCString trimPath(path);
      trimPath.Trim(" ", true, true);
      policy->AddDynamic(rdwr, trimPath.get());
    }
  }

  // Return the common policy.
  return policy;
}

#endif // MOZ_CONTENT_SANDBOX
} // namespace mozilla
