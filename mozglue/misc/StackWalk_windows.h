#ifndef mozilla_StackWalk_windows_h
#define mozilla_StackWalk_windows_h

#include "mozilla/Types.h"

#if defined(_M_AMD64) || defined(_M_ARM64)
/**
 * Allow stack walkers to work around the egregious win64 dynamic lookup table
 * list API by locking around SuspendThread to avoid deadlock.
 *
 * See comment in StackWalk.cpp
 */
struct MOZ_RAII AutoSuppressStackWalking {
  MFBT_API AutoSuppressStackWalking();
  MFBT_API ~AutoSuppressStackWalking();
};

MFBT_API void RegisterJitCodeRegion(uint8_t* aStart, size_t size);

MFBT_API void UnregisterJitCodeRegion(uint8_t* aStart, size_t size);
#endif  // _M_AMD64

#endif  // mozilla_StackWalk_windows_h
