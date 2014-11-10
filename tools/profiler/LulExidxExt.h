/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */

/* libunwind - a platform-independent unwind library
   Copyright 2011 Linaro Limited

This file is part of libunwind.

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.  */


// Copyright (c) 2010, 2011 Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


// Derived from libunwind, with extensive modifications.
// This file is derived from the following files in
// toolkit/crashreporter/google-breakpad:
//   src/common/arm_ex_to_module.h
//   src/common/memory_range.h
//   src/common/arm_ex_reader.h

#ifndef LulExidxExt_h
#define LulExidxExt_h

#include "LulMainInt.h"

using lul::LExpr;
using lul::RuleSet;
using lul::SecMap;

namespace lul {

typedef enum extab_cmd {
  ARM_EXIDX_CMD_FINISH,
  ARM_EXIDX_CMD_SUB_FROM_VSP,
  ARM_EXIDX_CMD_ADD_TO_VSP,
  ARM_EXIDX_CMD_REG_POP,
  ARM_EXIDX_CMD_REG_TO_SP,
  ARM_EXIDX_CMD_VFP_POP,
  ARM_EXIDX_CMD_WREG_POP,
  ARM_EXIDX_CMD_WCGR_POP,
  ARM_EXIDX_CMD_RESERVED,
  ARM_EXIDX_CMD_REFUSED,
} extab_cmd_t;

struct exidx_entry {
  uint32_t addr;
  uint32_t data;
};

struct extab_data {
  extab_cmd_t cmd;
  uint32_t data;
};

enum extab_cmd_flags {
  ARM_EXIDX_VFP_SHIFT_16 = 1 << 16,
  ARM_EXIDX_VFP_FSTMD = 1 << 17, // distinguishes FSTMxxD from FSTMxxX
};

// Receives information from arm_ex_reader::ExceptionTableInfo
// and adds it to the SecMap object
// This is in effect the EXIDX summariser.
class ARMExToModule {
 public:
   ARMExToModule(SecMap* smap, void(*log)(const char*)) : smap_(smap)
                                                        , log_(log) { }
  ~ARMExToModule() { }
  void AddStackFrame(uintptr_t addr, size_t size);
  int  ImproveStackFrame(const struct extab_data* edata);
  void DeleteStackFrame();
  void SubmitStackFrame();
 private:
  SecMap* smap_;
  LExpr   vsp_;        // Always appears to be of the form "sp + offset"
  RuleSet curr_rules_; // Also holds the address range being summarised
  // debugging message sink
  void (*log_)(const char*);
  int TranslateCmd(const struct extab_data* edata, LExpr& vsp);
};


// (derived from)
// memory_range.h: Define the google_breakpad::MemoryRange class, which
// is a lightweight wrapper with a pointer and a length to encapsulate
// a contiguous range of memory.

// A lightweight wrapper with a pointer and a length to encapsulate a
// contiguous range of memory. It provides helper methods for checked
// access of a subrange of the memory. Its implemementation does not
// allocate memory or call into libc functions, and is thus safer to use
// in a crashed environment.
class MemoryRange {
 public:

  MemoryRange(const void* data, size_t length) {
    Set(data, length);
  }

  // Sets this memory range to point to |data| and its length to |length|.
  void Set(const void* data, size_t length) {
    data_ = reinterpret_cast<const uint8_t*>(data);
    // Always set |length_| to zero if |data_| is NULL.
    length_ = data ? length : 0;
  }

  // Returns true if this range covers a subrange of |sub_length| bytes
  // at |sub_offset| bytes of this memory range, or false otherwise.
  bool Covers(size_t sub_offset, size_t sub_length) const {
    // The following checks verify that:
    // 1. sub_offset is within [ 0 .. length_ - 1 ]
    // 2. sub_offset + sub_length is within
    //    [ sub_offset .. length_ ]
    return sub_offset < length_ &&
           sub_offset + sub_length >= sub_offset &&
           sub_offset + sub_length <= length_;
  }

  // Returns a pointer to the beginning of this memory range.
  const uint8_t* data() const { return data_; }

  // Returns the length, in bytes, of this memory range.
  size_t length() const { return length_; }

 private:
  // Pointer to the beginning of this memory range.
  const uint8_t* data_;

  // Length, in bytes, of this memory range.
  size_t length_;
};


// This class is a reader for ARM unwind information
// from .ARM.exidx and .ARM.extab sections.
class ExceptionTableInfo {
 public:
  ExceptionTableInfo(const char* exidx_avma, size_t exidx_size,
                     const char* extab_avma, size_t extab_size,
                     uint32_t text_last_avma,
                     lul::ARMExToModule* handler,
                     void (*log)(const char*))
      : mr_exidx_avma_(lul::MemoryRange(exidx_avma, exidx_size)),
        mr_extab_avma_(lul::MemoryRange(extab_avma, extab_size)),
        text_last_avma_(text_last_avma),
        handler_(handler),
        log_(log) { }

  ~ExceptionTableInfo() { }

  // Parses the entries in .ARM.exidx and possibly
  // in .ARM.extab tables, reports what we find to
  // arm_ex_to_module::ARMExToModule.
  void Start();

 private:
  // Memory ranges for the exidx and extab sections in the executing image
  lul::MemoryRange mr_exidx_avma_;
  lul::MemoryRange mr_extab_avma_;
  // Address of the last byte of the text segment in the executing image
  uint32_t text_last_avma_;
  lul::ARMExToModule* handler_;
  // debugging message sink
  void (*log_)(const char*);
  enum ExExtractResult {
    ExSuccess,        // success
    ExInBufOverflow,  // out-of-range while reading .exidx
    ExOutBufOverflow, // output buffer is too small
    ExCantUnwind,     // this function is marked CANT_UNWIND
    ExCantRepresent,  // entry valid, but we can't represent it
    ExInvalid         // entry is invalid
  };
  ExExtractResult
    ExtabEntryExtract(const struct lul::exidx_entry* entry,
                      uint8_t* buf, size_t buf_size,
                      /*OUT*/size_t* buf_used);

  int ExtabEntryDecode(const uint8_t* buf, size_t buf_size);
};

} // namespace lul

#endif // LulExidxExt_h
