/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MacAutoreleasePool_h_
#define MacAutoreleasePool_h_

// This needs to be #include-able from non-ObjC code in nsAppRunner.cpp
#ifdef __OBJC__
@class NSAutoreleasePool;
#else
class NSAutoreleasePool;
#endif

namespace mozilla {

class MacAutoreleasePool {
public:
  MacAutoreleasePool();
  ~MacAutoreleasePool();

private:
  NSAutoreleasePool *mPool;

  MacAutoreleasePool(const MacAutoreleasePool&);
  void operator=(const MacAutoreleasePool&);
};

} // namespace mozilla

#endif // MacAutoreleasePool_h_
