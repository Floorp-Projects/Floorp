/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PowerCounters.h"
#include "nsXULAppAPI.h"  // for XRE_IsParentProcess
#include <dlfcn.h>

#define ALOG(args...) \
  __android_log_print(ANDROID_LOG_INFO, "GeckoProfiler", ##args)

/*
 * The following declarations come from the dlext.h header (not in the ndk).
 * https://cs.android.com/android/platform/superproject/main/+/main:bionic/libc/include/android/dlext.h;drc=655e430b28d7404f763e7ebefe84fba5a387666d
 */
struct android_namespace_t;
typedef struct {
  /** A bitmask of `ANDROID_DLEXT_` enum values. */
  uint64_t flags;

  /** Used by `ANDROID_DLEXT_RESERVED_ADDRESS` and
   * `ANDROID_DLEXT_RESERVED_ADDRESS_HINT`. */
  void* _Nullable reserved_addr;
  /** Used by `ANDROID_DLEXT_RESERVED_ADDRESS` and
   * `ANDROID_DLEXT_RESERVED_ADDRESS_HINT`. */
  size_t reserved_size;

  /** Used by `ANDROID_DLEXT_WRITE_RELRO` and `ANDROID_DLEXT_USE_RELRO`. */
  int relro_fd;

  /** Used by `ANDROID_DLEXT_USE_LIBRARY_FD`. */
  int library_fd;
  /** Used by `ANDROID_DLEXT_USE_LIBRARY_FD_OFFSET` */
  off64_t library_fd_offset;

  /** Used by `ANDROID_DLEXT_USE_NAMESPACE`. */
  struct android_namespace_t* _Nullable library_namespace;
} android_dlextinfo;
enum { ANDROID_DLEXT_USE_NAMESPACE = 0x200 };
extern "C"
    __attribute__((visibility("default"))) void* _Nullable android_dlopen_ext(
        const char* _Nullable __filename, int __flags,
        const android_dlextinfo* _Nullable __info);

// See also documentation at
// https://developer.android.com/studio/profile/power-profiler#power-rails
bool GetAvailableRails(RailDescriptor*, size_t* size_of_arr);

class RailEnergy final : public BaseProfilerCount {
 public:
  explicit RailEnergy(RailEnergyData* data, const char* aRailName,
                      const char* aSubsystemName)
      : BaseProfilerCount(aSubsystemName, nullptr, nullptr, "power", aRailName),
        mDataPtr(data),
        mLastTimestamp(0) {}

  ~RailEnergy() {}

  RailEnergy(const RailEnergy&) = delete;
  RailEnergy& operator=(const RailEnergy&) = delete;

  CountSample Sample() override {
    CountSample result = {
        // RailEnergyData.energy is in microwatt-seconds (uWs)
        // we need to return values in picowatt-hour.
        .count = static_cast<int64_t>(mDataPtr->energy * 1e3 / 3.6),
        .number = 0,
        .isSampleNew = mDataPtr->timestamp != mLastTimestamp,
    };
    mLastTimestamp = mDataPtr->timestamp;
    return result;
  }

 private:
  RailEnergyData* mDataPtr;
  uint64_t mLastTimestamp;
};

PowerCounters::PowerCounters() {
  if (!XRE_IsParentProcess()) {
    // Energy meters are global, so only sample them on the parent.
    return;
  }

  // A direct dlopen call on libperfetto_android_internal.so fails with a
  // namespace error because libperfetto_android_internal.so is missing in
  // /etc/public.libraries.txt
  // Instead, use android_dlopen_ext with the "default" namespace.
  void* libcHandle = dlopen("libc.so", RTLD_LAZY);
  if (!libcHandle) {
    ALOG("failed to dlopen libc: %s", dlerror());
    return;
  }

  struct android_namespace_t* (*android_get_exported_namespace)(const char*) =
      reinterpret_cast<struct android_namespace_t* (*)(const char*)>(
          dlsym(libcHandle, "__loader_android_get_exported_namespace"));
  if (!android_get_exported_namespace) {
    ALOG("failed to get __loader_android_get_exported_namespace: %s",
         dlerror());
    return;
  }

  struct android_namespace_t* ns = android_get_exported_namespace("default");
  const android_dlextinfo dlextinfo = {
      .flags = ANDROID_DLEXT_USE_NAMESPACE,
      .library_namespace = ns,
  };

  mLibperfettoModule = android_dlopen_ext("libperfetto_android_internal.so",
                                          RTLD_LOCAL | RTLD_LAZY, &dlextinfo);
  MOZ_ASSERT(mLibperfettoModule);
  if (!mLibperfettoModule) {
    ALOG("failed to get libperfetto handle: %s", dlerror());
    return;
  }

  decltype(&GetAvailableRails) getAvailableRails =
      reinterpret_cast<decltype(&GetAvailableRails)>(
          dlsym(mLibperfettoModule, "GetAvailableRails"));
  if (!getAvailableRails) {
    ALOG("failed to get GetAvailableRails pointer: %s", dlerror());
    return;
  }

  constexpr size_t kMaxNumRails = 32;
  if (!mRailDescriptors.resize(kMaxNumRails)) {
    ALOG("failed to grow mRailDescriptors");
    return;
  }
  size_t numRails = mRailDescriptors.length();
  getAvailableRails(&mRailDescriptors[0], &numRails);
  mRailDescriptors.shrinkTo(numRails);
  ALOG("found %zu rails", numRails);
  if (numRails == 0) {
    // We will see 0 rails either if the device has no support for power
    // profiling or if the SELinux policy blocks access (ie. on a non-rooted
    // device).
    return;
  }

  if (!mRailEnergyData.resize(numRails)) {
    ALOG("failed to grow mRailEnergyData");
    return;
  }
  for (size_t i = 0; i < numRails; ++i) {
    RailDescriptor& rail = mRailDescriptors[i];
    ALOG("rail %zu, name: %s, subsystem: %s", i, rail.rail_name,
         rail.subsys_name);
    RailEnergy* railEnergy =
        new RailEnergy(&mRailEnergyData[i], rail.rail_name, rail.subsys_name);
    if (!mCounters.emplaceBack(railEnergy)) {
      delete railEnergy;
    }
  }

  mGetRailEnergyData = reinterpret_cast<decltype(&GetRailEnergyData)>(
      dlsym(mLibperfettoModule, "GetRailEnergyData"));
  if (!mGetRailEnergyData) {
    ALOG("failed to get GetRailEnergyData pointer");
    return;
  }
}
PowerCounters::~PowerCounters() {
  if (mLibperfettoModule) {
    dlclose(mLibperfettoModule);
  }
}
void PowerCounters::Sample() {
  // Energy meters are global, so only sample them on the parent.
  // Also return early if we failed to access the GetRailEnergyData symbol.
  if (!XRE_IsParentProcess() || !mGetRailEnergyData) {
    return;
  }

  size_t length = mRailEnergyData.length();
  mGetRailEnergyData(&mRailEnergyData[0], &length);
}
