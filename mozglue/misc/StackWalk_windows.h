#ifndef mozilla_StackWalk_windows_h
#define mozilla_StackWalk_windows_h

#include "mozilla/Types.h"

/**
 * Allow stack walkers to work around the egregious win64 dynamic lookup table
 * list API by locking around SuspendThread to avoid deadlock.
 *
 * See comment in StackWalk.cpp
 */
MFBT_API void
AcquireStackWalkWorkaroundLock();

MFBT_API bool
TryAcquireStackWalkWorkaroundLock();

MFBT_API void
ReleaseStackWalkWorkaroundLock();

MFBT_API void
RegisterJitCodeRegion(uint8_t* aStart, size_t size);

MFBT_API void
UnregisterJitCodeRegion(uint8_t* aStart, size_t size);

#endif // mozilla_StackWalk_windows_h
