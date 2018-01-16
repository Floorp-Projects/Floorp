/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nscore.h"
#include "nsAlgorithm.h"
#include "nsUTF8Utils.h"

#include <arm_neon.h>

void
LossyConvertEncoding16to8::write_neon(const char16_t* aSource,
                                      uint32_t aSourceLength)
{
  char* dest = mDestination;

  // Align source to a 16-byte boundary and destination to 8-bytes boundary.
  uint32_t i = 0;
  while (((reinterpret_cast<uintptr_t>(aSource + i) & 0xf) ||
          (reinterpret_cast<uintptr_t>(dest + i) & 0x7)) &&
         i < aSourceLength) {
    dest[i] = static_cast<unsigned char>(aSource[i]);
    i++;
  }

  while ((reinterpret_cast<uintptr_t>(dest + i) & 0xf) &&
         aSourceLength - i > 7) {
    // source is aligned, but destination isn't aligned by 16-byte yet
    uint16x8_t s =
      vld1q_u16(reinterpret_cast<const uint16_t*>(
                  __builtin_assume_aligned(aSource + i, 16)));
    vst1_u8(reinterpret_cast<uint8_t*>(
              __builtin_assume_aligned(dest + i, 8)),
            vmovn_u16(s));
    i += 8;
  }

  // Align source and destination to a 16-byte boundary.
  while (aSourceLength - i > 15) {
    uint16x8_t low =
      vld1q_u16(reinterpret_cast<const uint16_t*>(
                  __builtin_assume_aligned(aSource + i, 16)));
    uint16x8_t high =
      vld1q_u16(reinterpret_cast<const uint16_t*>(
                  __builtin_assume_aligned(aSource + i + 8, 16)));
    vst1q_u8(reinterpret_cast<uint8_t*>(
               __builtin_assume_aligned(dest + i, 16)),
             vcombine_u8(vmovn_u16(low), vmovn_u16(high)));
    i += 16;
  }

  if (aSourceLength - i > 7) {
    uint16x8_t s = vld1q_u16(reinterpret_cast<const uint16_t*>(
                               __builtin_assume_aligned(aSource + i, 16)));
    vst1_u8(reinterpret_cast<uint8_t*>(
              __builtin_assume_aligned(dest + i, 8)),
            vmovn_u16(s));
    i += 8;
  }

  // Finish up the rest.
  for (; i < aSourceLength; ++i) {
    dest[i] = static_cast<unsigned char>(aSource[i]);
  }

  mDestination += i;
}

void
LossyConvertEncoding8to16::write_neon(const char* aSource,
                                      uint32_t aSourceLength)
{
  char16_t* dest = mDestination;

  // Align source to a 8-byte boundary and destination to 16-bytes boundary.
  uint32_t i = 0;
  while (((reinterpret_cast<uintptr_t>(aSource + i) & 0x7) ||
          (reinterpret_cast<uintptr_t>(dest + i) & 0xf)) &&
         i < aSourceLength) {
    dest[i] = static_cast<unsigned char>(aSource[i]);
    i++;
  }

  if ((uintptr_t(aSource + i) & 0xf) && aSourceLength - i > 7) {
    // destination is aligned, but source isn't aligned by 16-byte yet
    uint8x8_t s =
      vld1_u8(reinterpret_cast<const uint8_t*>(
                __builtin_assume_aligned(aSource + i, 8)));
    vst1q_u16(reinterpret_cast<uint16_t*>(
                __builtin_assume_aligned(dest + i, 16)),
              vmovl_u8(s));
    i += 8;
  }

  // Align source and destination to a 16-byte boundary.
  while (aSourceLength - i > 15) {
    uint8x16_t s =
      vld1q_u8(reinterpret_cast<const uint8_t*>(
                 __builtin_assume_aligned(aSource + i, 16)));
    uint16x8_t low = vmovl_u8(vget_low_u8(s));
    uint16x8_t high = vmovl_u8(vget_high_u8(s));
    vst1q_u16(reinterpret_cast<uint16_t*>(
                __builtin_assume_aligned(dest + i, 16)),
              low);
    vst1q_u16(reinterpret_cast<uint16_t*>(
                __builtin_assume_aligned(dest + i + 8, 16)),
              high);
    i += 16;
  }

  if (aSourceLength - i > 7) {
    uint8x8_t s =
      vld1_u8(reinterpret_cast<const uint8_t*>(
                __builtin_assume_aligned(aSource + i, 8)));
    vst1q_u16(reinterpret_cast<uint16_t*>(
                __builtin_assume_aligned(dest + i, 16)),
              vmovl_u8(s));
    i += 8;
  }

  // Finish up whatever's left.
  for (; i < aSourceLength; ++i) {
    dest[i] = static_cast<unsigned char>(aSource[i]);
  }

  mDestination += i;
}
