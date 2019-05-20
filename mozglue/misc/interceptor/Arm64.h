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
#include "mozilla/TypeTraits.h"

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

/**
 * Casts |aValue| to a |ResultT| via sign extension.
 *
 * This function should be used when extracting signed immediate values from
 * an instruction.
 *
 * @param aValue The value to be sign extended. This value should already be
 *               isolated from the remainder of the instruction's bits and
 *               shifted all the way to the right.
 * @param aNumValidBits The number of bits in |aValue| that contain the
 *                      immediate signed value, including the sign bit.
 */
template <typename ResultT>
inline ResultT SignExtend(const uint32_t aValue, const uint8_t aNumValidBits) {
  static_assert(IsIntegral<ResultT>::value && IsSigned<ResultT>::value,
                "ResultT must be a signed integral type");
  MOZ_ASSERT(aNumValidBits < 32U && aNumValidBits > 1);

  using UnsignedResultT =
      typename Decay<typename MakeUnsigned<ResultT>::Type>::Type;

  const uint8_t kResultWidthBits = sizeof(ResultT) * 8;

  // Shift left unsigned
  const uint8_t shiftAmt = kResultWidthBits - aNumValidBits;
  UnsignedResultT shiftedLeft = static_cast<UnsignedResultT>(aValue)
                                << shiftAmt;

  // Now shift right signed
  auto result = static_cast<ResultT>(shiftedLeft);
  result >>= shiftAmt;

  return result;
}

inline static uint32_t BuildUnconditionalBranchToRegister(const uint32_t aReg) {
  MOZ_ASSERT(aReg < 32);
  // BR aReg
  return 0xD61F0000 | (aReg << 5);
}

}  // namespace arm64
}  // namespace interceptor
}  // namespace mozilla

#endif  // mozilla_interceptor_Arm64_h
