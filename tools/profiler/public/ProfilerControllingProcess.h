/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ProfilerControllingProcess_h
#define ProfilerControllingProcess_h

namespace mozilla {

class ProfilerInitParams;

// Top-level process actors should implement this to integrate with
// CrossProcessProfilerController.
class ProfilerControllingProcess {
public:
  virtual void SendStartProfiler(const ProfilerInitParams& aParams) = 0;
  virtual void SendPauseProfiler(const bool& aPause) = 0;
  virtual void SendStopProfiler() = 0;
  virtual void SendGatherProfile() = 0;

  virtual ~ProfilerControllingProcess() {}
};

} // namespace mozilla

#endif
