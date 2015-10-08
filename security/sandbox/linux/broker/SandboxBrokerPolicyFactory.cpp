/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SandboxBrokerPolicyFactory.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Preferences.h"
#include "nsPrintfCString.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"

#ifdef ANDROID
#include "cutils/properties.h"
#endif

namespace mozilla {

/* static */ bool
SandboxBrokerPolicyFactory::IsSystemSupported() {
#ifdef ANDROID
  char hardware[PROPERTY_VALUE_MAX];
  int length = property_get("ro.hardware", hardware, nullptr);
  // "goldfish" -> emulator.  Other devices can be added when we're
  // reasonably sure they work.  Eventually this won't be needed....
  if (length > 0 && strcmp(hardware, "goldfish") == 0) {
    return true;
  }
#endif
  return false;
}

#if defined(MOZ_CONTENT_SANDBOX) && defined(MOZ_WIDGET_GONK)
namespace {
static const int rdonly = SandboxBroker::MAY_READ;
static const int wronly = SandboxBroker::MAY_WRITE;
static const int rdwr = rdonly | wronly;
static const int wrlog = wronly | SandboxBroker::MAY_CREATE;
}
#endif

SandboxBrokerPolicyFactory::SandboxBrokerPolicyFactory()
{
  // Policy entries that are the same in every process go here, and
  // are cached over the lifetime of the factory.
#if defined(MOZ_CONTENT_SANDBOX) && defined(MOZ_WIDGET_GONK)
  SandboxBroker::Policy* policy = new SandboxBroker::Policy;

  // Devices that need write access:
  policy->AddPath(rdwr, "/dev/genlock");  // bug 980924
  policy->AddPath(rdwr, "/dev/ashmem");   // bug 980947
  policy->AddTree(wronly, "/dev/log"); // bug 1199857
  // Graphics devices are a significant source of attack surface, but
  // there's not much we can do about it without proxying (which is
  // very difficult and a perforamnce hit).
  policy->AddPrefix(rdwr, "/dev", "kgsl");  // bug 995072
  policy->AddPath(rdwr, "/dev/qemu_pipe"); // but 1198410: goldfish gralloc.

  // Bug 1198475: mochitest logs.  (This is actually passed in via URL
  // query param to the mochitest page, and is configurable, so this
  // isn't enough in general, but hopefully it's good enough for B2G.)
  // Conditional on tests being run, using the same check seen in
  // DirectoryProvider.js to set ProfD.
  if (access("/data/local/tests/profile", R_OK) == 0) {
    policy->AddPath(wrlog, "/data/local/tests/log/mochitest.log");
  }

  // Read-only items below this line.

  policy->AddPath(rdonly, "/dev/urandom");  // bug 964500, bug 995069
  policy->AddPath(rdonly, "/dev/ion");      // bug 980937
  policy->AddPath(rdonly, "/proc/cpuinfo"); // bug 995067
  policy->AddPath(rdonly, "/proc/meminfo"); // bug 1025333
  policy->AddPath(rdonly, "/sys/devices/system/cpu/present"); // bug 1025329
  policy->AddPath(rdonly, "/sys/devices/system/soc/soc0/id"); // bug 1025339
  policy->AddPath(rdonly, "/etc/media_profiles.xml"); // bug 1198419
  policy->AddPath(rdonly, "/etc/media_codecs.xml"); // bug 1198460
  policy->AddTree(rdonly, "/system/fonts"); // bug 1026063

  // Bug 1199051 (crossplatformly, this is NS_GRE_DIR).
  policy->AddTree(rdonly, "/system/b2g");

  // Bug 1026356: dynamic library loading from assorted frameworks we
  // don't control (media codecs, maybe others).
  //
  // Bug 1198515: Also, the profiler calls breakpad code to get info
  // on all loaded ELF objects, which opens those files.
  policy->AddTree(rdonly, "/system/lib");
  policy->AddTree(rdonly, "/vendor/lib");
  policy->AddPath(rdonly, "/system/bin/linker"); // (profiler only)

  // Bug 1199866: EGL/WebGL.
  policy->AddPath(rdonly, "/system/lib/egl");
  policy->AddPath(rdonly, "/vendor/lib/egl");

  // Bug 1198401: timezones.  Yes, we need both of these; see bug.
  policy->AddTree(rdonly, "/system/usr/share/zoneinfo");
  policy->AddTree(rdonly, "/system//usr/share/zoneinfo");

  policy->AddPath(rdonly, "/data/local/tmp/profiler.options",
                  SandboxBroker::Policy::AddAlways); // bug 1029337

  mCommonContentPolicy.reset(policy);
#endif
}

#ifdef MOZ_CONTENT_SANDBOX
UniquePtr<SandboxBroker::Policy>
SandboxBrokerPolicyFactory::GetContentPolicy(int aPid)
{
  // Allow overriding "unsupported"ness with a pref, for testing.
  if (!IsSystemSupported() &&
      Preferences::GetInt("security.sandbox.content.level") <= 0) {
    return nullptr;
  }

  // Policy entries that vary per-process (currently the only reason
  // that can happen is because they contain the pid) are added here.
#if defined(MOZ_WIDGET_GONK)
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mCommonContentPolicy);
  UniquePtr<SandboxBroker::Policy>
    policy(new SandboxBroker::Policy(*mCommonContentPolicy));

  // Bug 1029337: where the profiler writes the data.
  nsPrintfCString profilerLogPath("/data/local/tmp/profile_%d_%d.txt",
                                  GeckoProcessType_Content, aPid);
  policy->AddPath(wrlog, profilerLogPath.get());

  // Bug 1198550: the profiler's replacement for dl_iterate_phdr
  policy->AddPath(rdonly, nsPrintfCString("/proc/%d/maps", aPid).get());

  // Bug 1198552: memory reporting.
  policy->AddPath(rdonly, nsPrintfCString("/proc/%d/statm", aPid).get());
  policy->AddPath(rdonly, nsPrintfCString("/proc/%d/smaps", aPid).get());

  return policy;
#else // MOZ_WIDGET_GONK
  // Not implemented for desktop yet.
  return nullptr;
#endif
}

#endif // MOZ_CONTENT_SANDBOX
} // namespace mozilla
