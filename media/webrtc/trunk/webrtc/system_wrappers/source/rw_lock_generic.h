/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SYSTEM_WRAPPERS_SOURCE_RW_LOCK_GENERIC_H_
#define WEBRTC_SYSTEM_WRAPPERS_SOURCE_RW_LOCK_GENERIC_H_

#include "webrtc/system_wrappers/include/rw_lock_wrapper.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class CriticalSectionWrapper;
class ConditionVariableWrapper;

class RWLockGeneric : public RWLockWrapper {
 public:
  RWLockGeneric();
  ~RWLockGeneric() override;

  void AcquireLockExclusive() override;
  void ReleaseLockExclusive() override;

  void AcquireLockShared() override;
  void ReleaseLockShared() override;

 private:
  CriticalSectionWrapper* critical_section_;
  ConditionVariableWrapper* read_condition_;
  ConditionVariableWrapper* write_condition_;

  int readers_active_;
  bool writer_active_;
  int readers_waiting_;
  int writers_waiting_;
};

}  // namespace webrtc

#endif  // WEBRTC_SYSTEM_WRAPPERS_SOURCE_RW_LOCK_GENERIC_H_
