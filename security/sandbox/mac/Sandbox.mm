/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Sandbox.h"

#include "nsCocoaFeatures.h"

// XXX There are currently problems with the /usr/include/sandbox.h file on
// some/all of the Macs in Mozilla's build system.  For the time being (until
// this problem is resolved), we refer directly to what we need from it,
// rather than including it here.
extern "C" int sandbox_init(const char *profile, uint64_t flags, char **errorbuf);
extern "C" void sandbox_free_error(char *errorbuf);

namespace mozilla {

static const char rules[] =
  "(version 1)\n"
  "(deny default)\n"
  "(allow signal (target self))\n"
  "(allow sysctl-read)\n"
  // Needed only on OS X 10.6
  "%s(allow file-read-data (literal \"%s\"))\n"
  "(allow mach-lookup\n"
  "    (global-name \"com.apple.cfprefsd.agent\")\n"
  "    (global-name \"com.apple.cfprefsd.daemon\")\n"
  "    (global-name \"com.apple.system.opendirectoryd.libinfo\")\n"
  "    (global-name \"com.apple.system.logger\")\n"
  "    (global-name \"com.apple.ls.boxd\"))\n"
  "(allow file-read*\n"
  "    (regex #\"^/etc$\")\n"
  "    (regex #\"^/dev/u?random$\")\n"
  "    (regex #\"^/(private/)?var($|/)\")\n"
  "    (literal \"/usr/share/icu/icudt51l.dat\")\n"
  "    (literal \"%s\")\n"
  "    (literal \"%s\")\n"
  "    (literal \"%s\"))\n";

bool StartMacSandbox(MacSandboxInfo aInfo, nsCString &aErrorMessage)
{
  if (aInfo.type != MacSandboxType_Plugin) {
    aErrorMessage.AppendPrintf("Unexpected sandbox type %u", aInfo.type);
    return false;
  }

  nsAutoCString profile;
  if (nsCocoaFeatures::OnLionOrLater()) {
    profile.AppendPrintf(rules, ";",
                         aInfo.pluginInfo.pluginPath.get(),
                         aInfo.pluginInfo.pluginBinaryPath.get(),
                         aInfo.appPath.get(),
                         aInfo.appBinaryPath.get());
  } else {
    profile.AppendPrintf(rules, "",
                         aInfo.pluginInfo.pluginPath.get(),
                         aInfo.pluginInfo.pluginBinaryPath.get(),
                         aInfo.appPath.get(),
                         aInfo.appBinaryPath.get());
  }

  char *errorbuf = NULL;
  if (sandbox_init(profile.get(), 0, &errorbuf)) {
    if (errorbuf) {
      aErrorMessage.AppendPrintf("sandbox_init() failed with error \"%s\"",
                                 errorbuf);
      printf("profile: %s\n", profile.get());
      sandbox_free_error(errorbuf);
    }
    return false;
  }

  return true;
}

} // namespace mozilla
