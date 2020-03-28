/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_interceptor_Arm64_h
#define mozilla_interceptor_Arm64_h

#include <type_traits>

#include "mozilla/Assertions.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Maybe.h"
#include "mozilla/Result.h"
#include "mozilla/Saturate.h"
#include "mozilla/Types.h"
#include "mozilla/TypeTraits.h"

namespace mozilla {
namespace interceptor {
namespace arm64 {

// clang-format off
enum class IntegerConditionCode : uint8_t {
  // From the ARMv8 Architectural Reference Manual, Section C1.2.4
  //               Description           Condition Flags
  EQ = 0b0000,  // ==                    Z == 1
  NE = 0b0001,  // !=                    Z == 0
  CS = 0b0010,  // carry set             C == 1
  HS = 0b0010,  // carry set (alias)     C == 1
  CC = 0b0011,  // carry clear           C == 0
  LO = 0b0011,  // carry clear (alias)   C == 0
  MI = 0b0100,  // < 0                   N == 1
  PL = 0b0101,  // >= 0                  N == 0
  VS = 0b0110,  // overflow              V == 1
  VC = 0b0111,  // no overflow           V == 0
  HI = 0b1000,  // unsigned >            C == 1 && Z == 0
  LS = 0b1001,  // unsigned <=           !(C == 1 && Z == 0)
  GE = 0b1010,  // signed >=             N == V
  LT = 0b1011,  // signed <              N != V
  GT = 0b1100,  // signed >              Z == 0 && N == V
  LE = 0b1101,  // signed <=             !(Z == 0 && N == V)
  AL = 0b1110,  // unconditional         <Any>
  NV = 0b1111   // unconditional (but AL is the preferred encoding)
};
// clang-format on

struct LoadOrBranch {
  enum class Type {
    Load,
    Branch,
  };

  // Load constructor
  LoadOrBranch(const uintptr_t aAbsAddress, const uint8_t aDestReg)
      : mType(Type::Load), mAbsAddress(aAbsAddress), mDestReg(aDestReg) {
    MOZ_ASSERT(aDestReg < 32);
  }

  // Unconditional branch constructor
  explicit LoadOrBranch(const uintptr_t aAbsAddress)
      : mType(Type::Branch),
        mAbsAddress(aAbsAddress),
        mCond(IntegerConditionCode::AL) {}

  // Conditional branch constructor
  LoadOrBranch(const uintptr_t aAbsAddress, const IntegerConditionCode aCond)
      : mType(Type::Branch), mAbsAddress(aAbsAddress), mCond(aCond) {}

  Type mType;

  // The absolute address to be loaded into a register, or branched to
  uintptr_t mAbsAddress;

  union {
    // The destination register for the load
    uint8_t mDestReg;

    // The condition code for the branch
    IntegerConditionCode mCond;
  };
};

enum class PCRelCheckError {
  InstructionNotPCRel,
  NoDecoderAvailable,
};

MFBT_API Result<LoadOrBranch, PCRelCheckError> CheckForPCRel(
    const uintptr_t aPC, const uint32_t aInst);

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
  static_assert(std::is_integral_v<ResultT> && std::is_signed_v<ResultT>,
                "ResultT must be a signed integral type");
  MOZ_ASSERT(aNumValidBits < 32U && aNumValidBits > 1);

  using UnsignedResultT = std::decay_t<std::make_unsigned_t<ResultT>>;

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

MFBT_API LoadOrBranch BUncondImmDecode(const uintptr_t aPC,
                                       const uint32_t aInst);

/**
 * If |aTarget| is more than 128MB away from |aPC|, we need to use a veneer.
 */
inline static bool IsVeneerRequired(const uintptr_t aPC,
                                    const uintptr_t aTarget) {
  detail::Saturate<intptr_t> saturated(aTarget);
  saturated -= aPC;

  uintptr_t absDiff = Abs(saturated.value());

  return absDiff >= 0x08000000U;
}

inline static bool IsUnconditionalBranchImm(const uint32_t aInst) {
  return (aInst & 0xFC000000U) == 0x14000000U;
}

inline static Maybe<uint32_t> BuildUnconditionalBranchImm(
    const uintptr_t aPC, const uintptr_t aTarget) {
  detail::Saturate<intptr_t> saturated(aTarget);
  saturated -= aPC;

  CheckedInt<int32_t> offset(saturated.value());
  if (!offset.isValid()) {
    return Nothing();
  }

  // offset should be a multiple of 4
  MOZ_ASSERT(offset.value() % 4 == 0);
  if (offset.value() % 4) {
    return Nothing();
  }

  offset /= 4;
  if (!offset.isValid()) {
    return Nothing();
  }

  int32_t signbits = offset.value() & 0xFE000000;
  // Ensure that offset is small enough to fit into the 26 bit region.
  // We check that the sign bits are either all ones or all zeros.
  MOZ_ASSERT(signbits == 0xFE000000 || !signbits);
  if (signbits && signbits != 0xFE000000) {
    return Nothing();
  }

  int32_t masked = offset.value() & 0x03FFFFFF;

  // B imm26
  return Some(0x14000000U | masked);
}

/**
 * Allocate and construct a veneer that provides an absolute 64-bit branch to
 * the hook function.
 */
template <typename TrampPoolT>
inline static uintptr_t MakeVeneer(TrampPoolT& aTrampPool, void* aPrimaryTramp,
                                   const uintptr_t aDestAddress) {
  auto maybeVeneer = aTrampPool.GetNextTrampoline();
  if (!maybeVeneer) {
    return 0;
  }

  Trampoline<typename TrampPoolT::MMPolicyT> veneer(
      std::move(maybeVeneer.ref()));

  // Write the same header information that is used for trampolines
  veneer.WriteEncodedPointer(nullptr);
  veneer.WriteEncodedPointer(aPrimaryTramp);

  veneer.StartExecutableCode();

  // Register 16 is explicitly intended for veneers in ARM64, so we use that
  // register without fear of clobbering anything important.
  veneer.WriteLoadLiteral(aDestAddress, 16);
  veneer.WriteInstruction(BuildUnconditionalBranchToRegister(16));

  return reinterpret_cast<uintptr_t>(veneer.EndExecutableCode());
}

}  // namespace arm64
}  // namespace interceptor
}  // namespace mozilla

#endif  // mozilla_interceptor_Arm64_h
