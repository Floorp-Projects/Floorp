/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_interceptor_Arm64_h
#define mozilla_interceptor_Arm64_h

#include "mozilla/Assertions.h"
#include "mozilla/Result.h"
#include "mozilla/Types.h"

namespace mozilla {
namespace interceptor {
namespace arm64 {

// This currently only handles loads, not branches
struct LoadInfo {
  LoadInfo(const uintptr_t aAbsAddress, const uint8_t aDestReg)
      : mAbsAddress(aAbsAddress), mDestReg(aDestReg) {
    MOZ_ASSERT(aDestReg < 32);
  }

  // The absolute address to be loaded into a register
  uintptr_t mAbsAddress;
  // The destination register for the load
  uint8_t mDestReg;
};

enum class PCRelCheckError {
  InstructionNotPCRel,
  NoDecoderAvailable,
};

MFBT_API Result<LoadInfo, PCRelCheckError> CheckForPCRel(const uintptr_t aPC,
                                                         const uint32_t aInst);

inline static uint32_t BuildUnconditionalBranchToRegister(const uint32_t aReg) {
  MOZ_ASSERT(aReg < 32);
  // BR aReg
  return 0xD61F0000 | (aReg << 5);
}

}  // namespace arm64
}  // namespace interceptor
}  // namespace mozilla

#endif  // mozilla_interceptor_Arm64_h
