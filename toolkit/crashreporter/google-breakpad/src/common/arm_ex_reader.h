
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

// Copyright (c) 2010 Google Inc.
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

#ifndef COMMON_ARM_EX_READER_H__
#define COMMON_ARM_EX_READER_H__

#include "common/arm_ex_to_module.h"
#include "common/memory_range.h"

namespace arm_ex_reader {

// This class is a reader for ARM unwind information
// from .ARM.exidx and .ARM.extab sections.
class ExceptionTableInfo {
 public:
  ExceptionTableInfo(const char* exidx, size_t exidx_size,
                     const char* extab, size_t extab_size,
                     uint32_t text_last_svma,
                     arm_ex_to_module::ARMExToModule* handler,
                     const char* mapping_addr,
                     uint32_t loading_addr)
      : mr_exidx_(google_breakpad::MemoryRange(exidx, exidx_size)),
        mr_extab_(google_breakpad::MemoryRange(extab, extab_size)),
        text_last_svma_(text_last_svma),
        handler_(handler), mapping_addr_(mapping_addr),
        loading_addr_(loading_addr) { }

  ~ExceptionTableInfo() { }

  // Parses the entries in .ARM.exidx and possibly
  // in .ARM.extab tables, reports what we find to
  // arm_ex_to_module::ARMExToModule.
  void Start();

 private:
  google_breakpad::MemoryRange mr_exidx_;
  google_breakpad::MemoryRange mr_extab_;
  uint32_t text_last_svma_;
  arm_ex_to_module::ARMExToModule* handler_;
  const char* mapping_addr_;
  uint32_t loading_addr_;

  enum ExExtractResult {
    ExSuccess,        // success
    ExInBufOverflow,  // out-of-range while reading .exidx
    ExOutBufOverflow, // output buffer is too small
    ExCantUnwind,     // this function is marked CANT_UNWIND
    ExCantRepresent,  // entry valid, but we can't represent it
    ExInvalid         // entry is invalid
  };
  ExExtractResult
    ExtabEntryExtract(const struct arm_ex_to_module::exidx_entry* entry,
                      uint8_t* buf, size_t buf_size,
                      /*OUT*/size_t* buf_used);

  int ExtabEntryDecode(const uint8_t* buf, size_t buf_size);
};

} // namespace arm_ex_reader

#endif // COMMON_ARM_EX_READER_H__
