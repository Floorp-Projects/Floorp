/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_WindowsDllBlocklistInfo_h
#define mozilla_WindowsDllBlocklistInfo_h

#include <stdint.h>

namespace mozilla {

// If the USE_TIMESTAMP flag is set, then we use the timestamp from
// the IMAGE_FILE_HEADER in lieu of a version number.
//
// If the UTILITY_PROCESSES_ONLY, SOCKET_PROCESSES_ONLY, GPU_PROCESSES_ONLY,
// or GMPLUGIN_PROCESSES_ONLY flags are set, the dll will only be blocked for
// these specific child processes. Note that these are a subset of
// CHILD_PROCESSES_ONLY.
enum DllBlockInfoFlags : uint32_t {
  FLAGS_DEFAULT = 0,
  BLOCK_WIN7_AND_OLDER = 1 << 0,
  BLOCK_WIN8_AND_OLDER = 1 << 1,
  USE_TIMESTAMP = 1 << 2,
  CHILD_PROCESSES_ONLY = 1 << 3,
  BROWSER_PROCESS_ONLY = 1 << 4,
  REDIRECT_TO_NOOP_ENTRYPOINT = 1 << 5,
  UTILITY_PROCESSES_ONLY = 1 << 6,
  SOCKET_PROCESSES_ONLY = 1 << 7,
  GPU_PROCESSES_ONLY = 1 << 8,
  GMPLUGIN_PROCESSES_ONLY = 1 << 9,
};

constexpr DllBlockInfoFlags operator|(const DllBlockInfoFlags a,
                                      const DllBlockInfoFlags b) {
  return static_cast<DllBlockInfoFlags>(static_cast<uint32_t>(a) |
                                        static_cast<uint32_t>(b));
}

template <typename StrType>
struct DllBlockInfoT {
  // The name of the DLL -- in LOWERCASE!  It will be compared to
  // a lowercase version of the DLL name only.
  StrType mName;

  // If mMaxVersion is ALL_VERSIONS, we'll block all versions of this
  // dll.  Otherwise, we'll block all versions less than or equal to
  // the given version, as queried by GetFileVersionInfo and
  // VS_FIXEDFILEINFO's dwFileVersionMS and dwFileVersionLS fields.
  //
  // Note that the version is usually 4 components, which is A.B.C.D
  // encoded as 0x AAAA BBBB CCCC DDDD ULL (spaces added for clarity),
  // but it's not required to be of that format.
  uint64_t mMaxVersion;

  // If the USE_TIMESTAMP flag is set, then we use the timestamp from
  // the IMAGE_FILE_HEADER in lieu of a version number.
  //
  // If the UTILITY_PROCESSES_ONLY, SOCKET_PROCESSES_ONLY, GPU_PROCESSES_ONLY,
  // or GMPLUGIN_PROCESSES_ONLY flags are set, the dll will only be blocked for
  // these specific child processes. Note that these are a subset of
  // CHILD_PROCESSES_ONLY. These flags cannot be combined.
  DllBlockInfoFlags mFlags;

  bool IsVersionBlocked(const uint64_t aOther) const {
    if (mMaxVersion == ALL_VERSIONS) {
      return true;
    }

    return aOther <= mMaxVersion;
  }

  bool IsValidDynamicBlocklistEntry() const;

  static const uint64_t ALL_VERSIONS = (uint64_t)-1LL;

  // DLLs sometimes ship without a version number, particularly early
  // releases. Blocking "version <= 0" has the effect of blocking unversioned
  // DLLs (since the call to get version info fails), but not blocking
  // any versioned instance.
  static const uint64_t UNVERSIONED = 0ULL;
};

}  // namespace mozilla

// Convert the 4 (decimal) components of a DLL version number into a
// single unsigned long long, as needed by the blocklist
#if defined(_MSC_VER) && !defined(__clang__)

// MSVC does not properly handle the constexpr MAKE_VERSION, so we use a macro
// instead (ugh).
#  define MAKE_VERSION(a, b, c, d) \
    ((a##ULL << 48) + (b##ULL << 32) + (c##ULL << 16) + d##ULL)

#else

static inline constexpr uint64_t MAKE_VERSION(uint16_t a, uint16_t b,
                                              uint16_t c, uint16_t d) {
  return static_cast<uint64_t>(a) << 48 | static_cast<uint64_t>(b) << 32 |
         static_cast<uint64_t>(c) << 16 | static_cast<uint64_t>(d);
}

#endif

#endif  // mozilla_WindowsDllBlocklistInfo_h
