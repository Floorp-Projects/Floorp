/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_WindowsDllBlocklistCommon_h
#define mozilla_WindowsDllBlocklistCommon_h

#include <stdint.h>

#include "mozilla/ArrayUtils.h"

namespace mozilla {

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
  // If the CHILD_PROCESSES_ONLY flag is set, then the dll is blocked
  // only when we are a child process.
  enum Flags {
    FLAGS_DEFAULT = 0,
    BLOCK_WIN7_AND_OLDER = 1 << 0,
    BLOCK_WIN8_AND_OLDER = 1 << 1,
    USE_TIMESTAMP = 1 << 2,
    CHILD_PROCESSES_ONLY = 1 << 3,
    BROWSER_PROCESS_ONLY = 1 << 4,
    REDIRECT_TO_NOOP_ENTRYPOINT = 1 << 5,
  } mFlags;

  bool IsVersionBlocked(const uint64_t aOther) const {
    if (mMaxVersion == ALL_VERSIONS) {
      return true;
    }

    return aOther <= mMaxVersion;
  }

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

#if !defined(DLL_BLOCKLIST_STRING_TYPE)
#  error "You must define DLL_BLOCKLIST_STRING_TYPE"
#endif  // !defined(DLL_BLOCKLIST_STRING_TYPE)

#define DLL_BLOCKLIST_DEFINITIONS_BEGIN_NAMED(name)                       \
  using DllBlockInfo = mozilla::DllBlockInfoT<DLL_BLOCKLIST_STRING_TYPE>; \
  static const DllBlockInfo name[] = {
#define DLL_BLOCKLIST_DEFINITIONS_BEGIN \
  DLL_BLOCKLIST_DEFINITIONS_BEGIN_NAMED(gWindowsDllBlocklist)

#define DLL_BLOCKLIST_DEFINITIONS_END \
  {}                                  \
  }                                   \
  ;

#define DECLARE_POINTER_TO_FIRST_DLL_BLOCKLIST_ENTRY_FOR(name, list) \
  const DllBlockInfo* name = &list[0]

#define DECLARE_POINTER_TO_FIRST_DLL_BLOCKLIST_ENTRY(name) \
  DECLARE_POINTER_TO_FIRST_DLL_BLOCKLIST_ENTRY_FOR(name, gWindowsDllBlocklist)

#define DECLARE_POINTER_TO_LAST_DLL_BLOCKLIST_ENTRY_FOR(name, list) \
  const DllBlockInfo* name = &list[mozilla::ArrayLength(list) - 1]

#define DECLARE_POINTER_TO_LAST_DLL_BLOCKLIST_ENTRY(name) \
  DECLARE_POINTER_TO_LAST_DLL_BLOCKLIST_ENTRY_FOR(name, gWindowsDllBlocklist)

#define DECLARE_DLL_BLOCKLIST_NUM_ENTRIES_FOR(name, list) \
  const size_t name = mozilla::ArrayLength(list) - 1

#define DECLARE_DLL_BLOCKLIST_NUM_ENTRIES(name) \
  DECLARE_DLL_BLOCKLIST_NUM_ENTRIES_FOR(name, gWindowsDllBlocklist)

#endif  // mozilla_WindowsDllBlocklistCommon_h
