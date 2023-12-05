/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <stdint.h>
#include <stdarg.h>

#if defined(MOZ_ASAN) || defined(FUZZING)
#  include <signal.h>
#endif

#include "mozilla/ScopeExit.h"

#ifdef __SUNPRO_CC
#  include <stdio.h>
#endif

#include "prlink.h"
#include "va/va.h"

#include "mozilla/GfxInfoUtils.h"

// Print VA-API test results to stdout and logging to stderr
#define OUTPUT_PIPE 1

#define MIN(a, b) ((a) < (b) ? (a) : (b))

// bits to use decoding vaapitest() return values.
constexpr int CODEC_HW_H264 = 1 << 4;
constexpr int CODEC_HW_VP8 = 1 << 5;
constexpr int CODEC_HW_VP9 = 1 << 6;
constexpr int CODEC_HW_AV1 = 1 << 7;

// childgltest is declared inside extern "C" so that the name is not mangled.
// The name is used in build/valgrind/x86_64-pc-linux-gnu.sup to suppress
// memory leak errors because we run it inside a short lived fork and we don't
// care about leaking memory
extern "C" {

static constexpr struct {
  VAProfile mVAProfile;
  const char* mName;
} kVAAPiProfileName[] = {
#define MAP(v) \
  { VAProfile##v, #v }
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
      return profile.mName;
    }
  }
  return nullptr;
}

static void vaapitest(const char* aRenderDevicePath) {
  int renderDeviceFD = -1;
  VAProfile* profiles = nullptr;
  VAEntrypoint* entryPoints = nullptr;
  VADisplay display = nullptr;
  void* libDrm = nullptr;

  log("vaapitest start, device %s\n", aRenderDevicePath);

  auto autoRelease = mozilla::MakeScopeExit([&] {
    free(profiles);
    free(entryPoints);
    if (display) {
      vaTerminate(display);
    }
    if (libDrm) {
      dlclose(libDrm);
    }
    if (renderDeviceFD > -1) {
      close(renderDeviceFD);
    }
  });

  renderDeviceFD = open(aRenderDevicePath, O_RDWR);
  if (renderDeviceFD == -1) {
    record_error("VA-API test failed: failed to open renderDeviceFD.");
    return;
  }

  libDrm = dlopen("libva-drm.so.2", RTLD_LAZY);
  if (!libDrm) {
    record_error("VA-API test failed: libva-drm.so.2 is missing.");
    return;
  }

  static auto sVaGetDisplayDRM =
      (void* (*)(int fd))dlsym(libDrm, "vaGetDisplayDRM");
  if (!sVaGetDisplayDRM) {
    record_error("VA-API test failed: sVaGetDisplayDRM is missing.");
    return;
  }

  display = sVaGetDisplayDRM(renderDeviceFD);
  if (!display) {
    record_error("VA-API test failed: sVaGetDisplayDRM failed.");
    return;
  }

  int major, minor;
  VAStatus status = vaInitialize(display, &major, &minor);
  if (status != VA_STATUS_SUCCESS) {
    log("vaInitialize failed %d\n", status);
    record_error("VA-API test failed: failed to initialise VAAPI connection.");
    return;
  } else {
    log("vaInitialize finished\n");
  }

  int maxProfiles = vaMaxNumProfiles(display);
  int maxEntryPoints = vaMaxNumEntrypoints(display);
  if (maxProfiles <= 0 || maxEntryPoints <= 0) {
    record_error("VA-API test failed: wrong VAAPI profiles/entry point nums.");
    return;
  }

  profiles = (VAProfile*)malloc(sizeof(VAProfile) * maxProfiles);
  int numProfiles = 0;
  status = vaQueryConfigProfiles(display, profiles, &numProfiles);
  if (status != VA_STATUS_SUCCESS) {
    record_error("VA-API test failed: vaQueryConfigProfiles() failed.");
    return;
  }
  numProfiles = MIN(numProfiles, maxProfiles);

  entryPoints = (VAEntrypoint*)malloc(sizeof(VAEntrypoint) * maxEntryPoints);
  int codecs = 0;
  bool foundProfile = false;
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
      continue;
    }
    numEntryPoints = MIN(numEntryPoints, maxEntryPoints);
    for (int entry = 0; entry < numEntryPoints; entry++) {
      if (entryPoints[entry] != VAEntrypointVLD) {
        continue;
      }
      VAConfigID config = VA_INVALID_ID;
      status = vaCreateConfig(display, profile, entryPoints[entry], nullptr, 0,
                              &config);
      if (status == VA_STATUS_SUCCESS) {
        const char* profstr = VAProfileName(profile);
        log("Profile: %s\n", profstr);
        // VAProfileName returns null on failure, making the below calls safe
        if (!strncmp(profstr, "H264", 4)) {
          codecs |= CODEC_HW_H264;
        } else if (!strncmp(profstr, "VP8", 3)) {
          codecs |= CODEC_HW_VP8;
        } else if (!strncmp(profstr, "VP9", 3)) {
          codecs |= CODEC_HW_VP9;
        } else if (!strncmp(profstr, "AV1", 3)) {
          codecs |= CODEC_HW_AV1;
        } else {
          record_warning("VA-API test unknown profile.");
        }
        vaDestroyConfig(display, config);
        foundProfile = true;
      }
    }
  }
  if (foundProfile) {
    record_value("VAAPI_SUPPORTED\nTRUE\n");
    record_value("VAAPI_HWCODECS\n%d\n", codecs);
  } else {
    record_value("VAAPI_SUPPORTED\nFALSE\n");
  }
  log("vaapitest finished\n");
}

}  // extern "C"

static void PrintUsage() {
  printf(
      "Firefox VA-API probe utility\n"
      "\n"
      "usage: vaapitest [options]\n"
      "\n"
      "Options:\n"
      "\n"
      "  -h --help                 show this message\n"
      "  -d --drm drm_device       probe VA-API on drm_device (may be "
      "/dev/dri/renderD128)\n"
      "\n");
}

int main(int argc, char** argv) {
  struct option longOptions[] = {{"help", no_argument, NULL, 'h'},
                                 {"drm", required_argument, NULL, 'd'},
                                 {NULL, 0, NULL, 0}};
  const char* shortOptions = "hd:";
  int c;
  const char* drmDevice = nullptr;
  while ((c = getopt_long(argc, argv, shortOptions, longOptions, NULL)) != -1) {
    switch (c) {
      case 'd':
        drmDevice = optarg;
        break;
      case 'h':
      default:
        break;
    }
  }
  if (drmDevice) {
#if defined(MOZ_ASAN) || defined(FUZZING)
    // If handle_segv=1 (default), then glxtest crash will print a sanitizer
    // report which can confuse the harness in fuzzing automation.
    signal(SIGSEGV, SIG_DFL);
#endif
    const char* env = getenv("MOZ_GFX_DEBUG");
    enable_logging = env && *env == '1';
    output_pipe = OUTPUT_PIPE;
    if (!enable_logging) {
      close_logging();
    }
    vaapitest(drmDevice);
    record_flush();
    return EXIT_SUCCESS;
  }
  PrintUsage();
  return 0;
}
