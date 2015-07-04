/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PROFILERIOINTERPOSEOBSERVER_H
#define PROFILERIOINTERPOSEOBSERVER_H

#ifdef MOZ_ENABLE_PROFILER_SPS

#include "mozilla/IOInterposer.h"

namespace mozilla {

/**
 * This class is the observer that calls into the profiler whenever
 * main thread I/O occurs.
 */
class ProfilerIOInterposeObserver final : public IOInterposeObserver
{
public:
  virtual void Observe(Observation& aObservation);
};

} // namespace mozilla

#endif // MOZ_ENABLE_PROFILER_SPS

#endif // PROFILERIOINTERPOSEOBSERVER_H
