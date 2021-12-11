/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_interceptor_PatcherBase_h
#define mozilla_interceptor_PatcherBase_h

#include "mozilla/interceptor/TargetFunction.h"

namespace mozilla {
namespace interceptor {

template <typename MMPolicy>
struct GetProcAddressSelector;

template <>
struct GetProcAddressSelector<MMPolicyOutOfProcess> {
  FARPROC operator()(HMODULE aModule, const char* aName,
                     const MMPolicyOutOfProcess& aMMPolicy) const {
    auto exportSection =
        mozilla::nt::PEExportSection<MMPolicyOutOfProcess>::Get(aModule,
                                                                aMMPolicy);
    return exportSection.GetProcAddress(aName);
  }
};

template <>
struct GetProcAddressSelector<MMPolicyInProcess> {
  FARPROC operator()(HMODULE aModule, const char* aName,
                     const MMPolicyInProcess&) const {
    // PEExportSection works for MMPolicyInProcess, too, but the native
    // GetProcAddress is still better because PEExportSection does not
    // solve a forwarded entry.
    return ::GetProcAddress(aModule, aName);
  }
};

template <typename VMPolicy>
class WindowsDllPatcherBase {
 protected:
  typedef typename VMPolicy::MMPolicyT MMPolicyT;

  template <typename... Args>
  explicit WindowsDllPatcherBase(Args&&... aArgs)
      : mVMPolicy(std::forward<Args>(aArgs)...) {}

  ReadOnlyTargetFunction<MMPolicyT> ResolveRedirectedAddress(
      FARPROC aOriginalFunction) {
    uintptr_t currAddr = reinterpret_cast<uintptr_t>(aOriginalFunction);

#if defined(_M_IX86) || defined(_M_X64)
    uintptr_t prevAddr = 0;
    while (prevAddr != currAddr) {
      ReadOnlyTargetFunction<MMPolicyT> currFunc(mVMPolicy, currAddr);
      prevAddr = currAddr;

      // If function entry is jmp rel8 stub to the internal implementation, we
      // resolve redirected address from the jump target.
      uintptr_t nextAddr = 0;
      if (currFunc.IsRelativeShortJump(&nextAddr)) {
        int8_t offset = nextAddr - currFunc.GetAddress() - 2;

#  if defined(_M_X64)
        // We redirect to the target of a short jump backwards if the target
        // is another jump (only 32-bit displacement is currently supported).
        // This case is used by GetFileAttributesW in Win7 x64.
        if ((offset < 0) && (currFunc.IsValidAtOffset(2 + offset))) {
          ReadOnlyTargetFunction<MMPolicyT> redirectFn(mVMPolicy, nextAddr);
          if (redirectFn.IsIndirectNearJump(&nextAddr)) {
            return redirectFn;
          }
        }
#  endif

        // We check the downstream has enough nop-space only when the offset is
        // positive.  Otherwise we stop chasing redirects and let the caller
        // fail to hook.
        if (offset > 0) {
          bool isNopSpace = true;
          for (int8_t i = 0; i < offset; i++) {
            if (currFunc[2 + i] != 0x90) {
              isNopSpace = false;
              break;
            }
          }

          if (isNopSpace) {
            currAddr = nextAddr;
          }
        }
#  if defined(_M_X64)
      } else if (currFunc.IsIndirectNearJump(&nextAddr) ||
                 currFunc.IsRelativeNearJump(&nextAddr)) {
#  else
      } else if (currFunc.IsIndirectNearJump(&nextAddr)) {
#  endif
        // If function entry is jmp [disp32] such as used by kernel32, we
        // resolve redirected address from import table. For x64, we resolve
        // a relative near jump for TestDllInterceptor with --disable-optimize.
        currAddr = nextAddr;
      }
    }
#endif  // defined(_M_IX86) || defined(_M_X64)

    if (currAddr != reinterpret_cast<uintptr_t>(aOriginalFunction) &&
        !mVMPolicy.IsPageAccessible(currAddr)) {
      currAddr = reinterpret_cast<uintptr_t>(aOriginalFunction);
    }
    return ReadOnlyTargetFunction<MMPolicyT>(mVMPolicy, currAddr);
  }

 public:
  FARPROC GetProcAddress(HMODULE aModule, const char* aName) const {
    GetProcAddressSelector<MMPolicyT> selector;
    return selector(aModule, aName, mVMPolicy);
  }

  bool IsPageAccessible(uintptr_t aAddress) const {
    return mVMPolicy.IsPageAccessible(aAddress);
  }

#if defined(NIGHTLY_BUILD)
  const Maybe<DetourError>& GetLastDetourError() const {
    return mVMPolicy.GetLastDetourError();
  }
#endif  // defined(NIGHTLY_BUILD)
  template <typename... Args>
  void SetLastDetourError(Args&&... aArgs) {
    mVMPolicy.SetLastDetourError(std::forward<Args>(aArgs)...);
  }

 protected:
  VMPolicy mVMPolicy;
};

}  // namespace interceptor
}  // namespace mozilla

#endif  // mozilla_interceptor_PatcherBase_h
