/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "Arm64.h"

namespace mozilla {
namespace interceptor {
namespace arm64 {

struct PCRelativeLoadTest {
  // Bitmask to be ANDed with the instruction to isolate the bits that this
  // instance is interested in
  uint32_t mTestMask;
  // The desired bits that we want to see after masking
  uint32_t mMatchBits;
  // If we match, mDecodeFn provide the code to decode the instruction.
  LoadInfo (*mDecodeFn)(const uintptr_t aPC, const uint32_t aInst);
};

static LoadInfo ADRPDecode(const uintptr_t aPC, const uint32_t aInst) {
  // Keep in mind that on Windows aarch64, uint32_t is little-endian
  const uint32_t kMaskDataProcImmPcRelativeImmLo = 0x60000000;
  const uint32_t kMaskDataProcImmPcRelativeImmHi = 0x00FFFFE0;

  uintptr_t base = aPC;
  intptr_t offset = SignExtend<intptr_t>(
      ((aInst & kMaskDataProcImmPcRelativeImmHi) >> 3) |
          ((aInst & kMaskDataProcImmPcRelativeImmLo) >> 29),
      21);
  base &= ~0xFFFULL;
  offset <<= 12;

  uint8_t reg = aInst & 0x1F;

  return LoadInfo(base + offset, reg);
}

// Order is important here; more specific encoding tests must be placed before
// less specific encoding tests.
static const PCRelativeLoadTest gPCRelTests[] = {
    {0x9FC00000, 0x10000000, nullptr},      // ADR
    {0x9FC00000, 0x90000000, &ADRPDecode},  // ADRP
    {0xFF000000, 0x58000000, nullptr},      // LDR (literal) 64-bit GPR
    {0x3B000000, 0x18000000, nullptr},      // LDR (literal) (remaining forms)
    {0x7C000000, 0x14000000, nullptr},      // B (unconditional immediate)
    {0xFE000000, 0x54000000, nullptr},      // B.Cond
    {0x7E000000, 0x34000000, nullptr},      // Compare and branch (imm)
    {0x7E000000, 0x36000000, nullptr},      // Test and branch (imm)
    {0xFE000000, 0xD6000000, nullptr}       // Unconditional branch (reg)
};

/**
 * In this function we interate through each entry in |gPCRelTests|, AND
 * |aInst| with |test.mTestMask| to isolate the bits that we're interested in,
 * then compare that result against |test.mMatchBits|. If we have a match,
 * then that particular entry is applicable to |aInst|. If |test.mDecodeFn| is
 * present, then we call it to decode the instruction. If it is not present,
 * then we assume that this particular instruction is unsupported.
 */
MFBT_API Result<LoadInfo, PCRelCheckError> CheckForPCRel(const uintptr_t aPC,
                                                         const uint32_t aInst) {
  for (auto&& test : gPCRelTests) {
    if ((aInst & test.mTestMask) == test.mMatchBits) {
      if (!test.mDecodeFn) {
        return Err(PCRelCheckError::NoDecoderAvailable);
      }

      return test.mDecodeFn(aPC, aInst);
    }
  }

  return Err(PCRelCheckError::InstructionNotPCRel);
}

}  // namespace arm64
}  // namespace interceptor
}  // namespace mozilla
