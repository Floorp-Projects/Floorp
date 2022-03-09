/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VAAPIUtils.h"

#include "va/va.h"
#include "prlink.h"
#include "mozilla/ScopeExit.h"
#include "DMABufLibWrapper.h"

namespace mozilla::widget {

static constexpr struct {
  VAProfile mVAProfile;
  nsLiteralCString mName;
} kVAAPiProfileName[] = {
#define MAP(v) \
  { VAProfile##v, nsLiteralCString(#v) }
    MAP(H264ConstrainedBaseline),
    MAP(H264Main),
    MAP(H264High),
    MAP(VP8Version0_3),
    MAP(VP9Profile0),
    MAP(VP9Profile2),
    MAP(AV1Profile0),
    MAP(AV1Profile1),
#undef MAP
};

static const char* VAProfileName(VAProfile aVAProfile) {
  for (const auto& profile : kVAAPiProfileName) {
    if (profile.mVAProfile == aVAProfile) {
      return profile.mName.get();
    }
  }
  return nullptr;
}

static nsTArray<VAProfile> VAAPIGetAcceleratedProfilesInternal() {
  LOGDMABUF(("VAAPIGetAcceleratedProfiles()"));
  VAProfile* profiles = nullptr;
  VAEntrypoint* entryPoints = nullptr;
  PRLibrary* libDrm = nullptr;
  VADisplay display = nullptr;
  nsTArray<VAProfile> supportedProfiles;

  auto autoRelease = mozilla::MakeScopeExit([&] {
    delete[] profiles;
    delete[] entryPoints;
    if (display) {
      vaTerminate(display);
    }
    if (libDrm) {
      PR_UnloadLibrary(libDrm);
    }
  });

  PRLibSpec lspec;
  lspec.type = PR_LibSpec_Pathname;
  const char* libName = "libva-drm.so.2";
  lspec.value.pathname = libName;
  libDrm = PR_LoadLibraryWithFlags(lspec, PR_LD_NOW | PR_LD_LOCAL);
  if (!libDrm) {
    LOGDMABUF(("  Missing or old %s library.", libName));
    return supportedProfiles;
  }

  static auto sVaGetDisplayDRM =
      (void* (*)(int fd))PR_FindSymbol(libDrm, "vaGetDisplayDRM");
  if (!sVaGetDisplayDRM) {
    LOGDMABUF(("  Missing vaGetDisplayDRM()"));
    return supportedProfiles;
  }
  display = sVaGetDisplayDRM(widget::GetDMABufDevice()->GetDRMFd());
  if (!display) {
    LOGDMABUF(("  Failed to get vaGetDisplayDRM()"));
    return supportedProfiles;
  }

  int major, minor;
  VAStatus status = vaInitialize(display, &major, &minor);
  if (status != VA_STATUS_SUCCESS) {
    LOGDMABUF(
        ("  Failed to initialise VAAPI connection: %s.", vaErrorStr(status)));
    return supportedProfiles;
  }
  LOGDMABUF(("  Initialised VAAPI connection: version %d.%d", major, minor));

  int maxProfiles = vaMaxNumProfiles(display);
  int maxEntryPoints = vaMaxNumEntrypoints(display);
  if (MOZ_UNLIKELY(maxProfiles <= 0 || maxEntryPoints <= 0)) {
    LOGDMABUF(("  Wrong profiles/entry point nums."));
    return supportedProfiles;
  }

  profiles = new VAProfile[maxProfiles];
  int numProfiles = 0;
  status = vaQueryConfigProfiles(display, profiles, &numProfiles);
  if (status != VA_STATUS_SUCCESS) {
    LOGDMABUF(("  vaQueryConfigProfiles() failed %s", vaErrorStr(status)));
    return supportedProfiles;
  }
  numProfiles = std::min(numProfiles, maxProfiles);

  entryPoints = new VAEntrypoint[maxEntryPoints];
  for (int p = 0; p < numProfiles; p++) {
    VAProfile profile = profiles[p];

    // Check only supported profiles
    if (!VAProfileName(profile)) {
      continue;
    }

    int numEntryPoints = 0;
    status = vaQueryConfigEntrypoints(display, profile, entryPoints,
                                      &numEntryPoints);
    if (status != VA_STATUS_SUCCESS) {
      LOGDMABUF(("  vaQueryConfigEntrypoints() failed: '%s' for profile %d",
                 vaErrorStr(status), (int)profile));
      continue;
    }
    numEntryPoints = std::min(numEntryPoints, maxEntryPoints);

    for (int entry = 0; entry < numEntryPoints; entry++) {
      if (entryPoints[entry] != VAEntrypointVLD) {
        continue;
      }
      VAConfigID config = VA_INVALID_ID;
      status = vaCreateConfig(display, profile, entryPoints[entry], nullptr, 0,
                              &config);
      if (status == VA_STATUS_SUCCESS) {
        LOGDMABUF(("  Supported profile %s:", VAProfileName(profile)));
        supportedProfiles.AppendElement(profile);
        vaDestroyConfig(display, config);
        break;
      }
    }
  }
  return supportedProfiles;
}

static const nsTArray<VAProfile>& VAAPIGetAcceleratedProfiles() {
  static const nsTArray<VAProfile> profiles =
      VAAPIGetAcceleratedProfilesInternal();
  return profiles;
}

bool VAAPIIsSupported() { return !VAAPIGetAcceleratedProfiles().IsEmpty(); }

}  // namespace mozilla::widget
