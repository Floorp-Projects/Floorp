/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_WindowsUnwindInfo_h
#define mozilla_WindowsUnwindInfo_h

#ifdef _M_X64

#  include <cstdint>

namespace mozilla {

union UnwindCode {
  struct {
    uint8_t offset_in_prolog;
    uint8_t unwind_operation_code : 4;
    uint8_t operation_info : 4;
  };
  uint16_t frame_offset;
};

enum UnwindOperationCodes {
  UWOP_PUSH_NONVOL = 0,      // info == register number
  UWOP_ALLOC_LARGE = 1,      // no info, alloc size in next 2 slots
  UWOP_ALLOC_SMALL = 2,      // info == size of allocation / 8 - 1
  UWOP_SET_FPREG = 3,        // no info, FP = RSP + UNWIND_INFO.FPRegOffset*16
  UWOP_SAVE_NONVOL = 4,      // info == register number, offset in next slot
  UWOP_SAVE_NONVOL_FAR = 5,  // info == register number, offset in next 2 slots
  UWOP_SAVE_XMM = 6,         // Version 1; undocumented
  UWOP_EPILOG = 6,           // Version 2; undocumented
  UWOP_SAVE_XMM_FAR = 7,     // Version 1; undocumented
  UWOP_SPARE = 7,            // Version 2; undocumented
  UWOP_SAVE_XMM128 = 8,      // info == XMM reg number, offset in next slot
  UWOP_SAVE_XMM128_FAR = 9,  // info == XMM reg number, offset in next 2 slots
  UWOP_PUSH_MACHFRAME = 10   // info == 0: no error-code, 1: error-code
};

struct UnwindInfo {
  uint8_t version : 3;
  uint8_t flags : 5;
  uint8_t size_of_prolog;
  uint8_t count_of_codes;
  uint8_t frame_register : 4;
  uint8_t frame_offset : 4;
  UnwindCode unwind_code[1];
};

}  // namespace mozilla

#endif  // _M_X64

#endif  // mozilla_WindowsUnwindInfo_h
