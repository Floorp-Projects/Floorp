/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_windowsdllblocklist_h
#define mozilla_windowsdllblocklist_h

#if (defined(_MSC_VER) || defined(__MINGW32__)) && \
    (defined(_M_IX86) || defined(_M_X64) || defined(_M_ARM64))

#  include <windows.h>
#  include "mozilla/ProcessType.h"
#  include "mozilla/Types.h"
#  include <algorithm>

#  define HAS_DLL_BLOCKLIST

enum DllBlocklistInitFlags {
  eDllBlocklistInitFlagDefault = 0,
  eDllBlocklistInitFlagIsChildProcess = 1 << 0,
  eDllBlocklistInitFlagWasBootstrapped = 1 << 1,
  eDllBlocklistInitFlagIsUtilityProcess = 1 << 2,
  eDllBlocklistInitFlagIsSocketProcess = 1 << 3,
  eDllBlocklistInitFlagIsGPUProcess = 1 << 4,
  eDllBlocklistInitFlagIsGMPluginProcess = 1 << 5,
};

inline void SetDllBlocklistProcessTypeFlags(uint32_t& aFlags,
                                            GeckoProcessType aProcessType) {
  if (aProcessType == GeckoProcessType_Utility) {
    aFlags |= eDllBlocklistInitFlagIsUtilityProcess;
  } else if (aProcessType == GeckoProcessType_Socket) {
    aFlags |= eDllBlocklistInitFlagIsSocketProcess;
  } else if (aProcessType == GeckoProcessType_GPU) {
    aFlags |= eDllBlocklistInitFlagIsGPUProcess;
  } else if (aProcessType == GeckoProcessType_GMPlugin) {
    aFlags |= eDllBlocklistInitFlagIsGMPluginProcess;
  }
}

// Only available from within firefox.exe
#  if !defined(IMPL_MFBT) && !defined(MOZILLA_INTERNAL_API)
extern uint32_t gBlocklistInitFlags;
#  endif  // !defined(IMPL_MFBT) && !defined(MOZILLA_INTERNAL_API)

MFBT_API void DllBlocklist_Initialize(
    uint32_t aInitFlags = eDllBlocklistInitFlagDefault);
MFBT_API void DllBlocklist_WriteNotes();
MFBT_API bool DllBlocklist_CheckStatus();

MFBT_API bool* DllBlocklist_GetBlocklistInitFailedPointer();
MFBT_API bool* DllBlocklist_GetUser32BeforeBlocklistPointer();
MFBT_API const char* DllBlocklist_GetBlocklistWriterData();

// This export intends to clean up after DllBlocklist_Initialize().
// It's disabled in release builds for performance and to limit callers' ability
// to interfere with dll blocking.
#  ifdef DEBUG
MFBT_API void DllBlocklist_Shutdown();
#  endif  // DEBUG

namespace mozilla {
namespace glue {
namespace detail {
// Forward declaration
class DllServicesBase;

template <size_t N>
class WritableBuffer {
  char mBuffer[N];
  size_t mLen;

  size_t Available() const { return sizeof(mBuffer) - mLen; }

 public:
  WritableBuffer() : mBuffer{0}, mLen(0) {}

  void Write(const char* aData, size_t aLen) {
    size_t writable_len = std::min(aLen, Available());
    memcpy(mBuffer + mLen, aData, writable_len);
    mLen += writable_len;
  }

  size_t Length() const { return mLen; }
  const char* Data() const { return mBuffer; }
};
}  // namespace detail
}  // namespace glue
}  // namespace mozilla

MFBT_API void DllBlocklist_SetFullDllServices(
    mozilla::glue::detail::DllServicesBase* aSvc);
MFBT_API void DllBlocklist_SetBasicDllServices(
    mozilla::glue::detail::DllServicesBase* aSvc);

#endif  // defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
#endif  // mozilla_windowsdllblocklist_h
