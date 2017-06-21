/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PROFILERIOINTERPOSEOBSERVER_H
#define PROFILERIOINTERPOSEOBSERVER_H

#include "mozilla/IOInterposer.h"
#include "nsISupportsImpl.h"

namespace mozilla {

/**
 * This class is the observer that calls into the profiler whenever
 * main thread I/O occurs.
 */
class ProfilerIOInterposeObserver final : public IOInterposeObserver
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ProfilerIOInterposeObserver)

public:
  virtual void Observe(Observation& aObservation);

protected:
  virtual ~ProfilerIOInterposeObserver() {}
};

} // namespace mozilla

#endif // PROFILERIOINTERPOSEOBSERVER_H
