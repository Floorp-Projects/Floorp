
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


#include "common/arm_ex_reader.h"

#include <assert.h>
#include <stdio.h>

// This file, in conjunction with arm_ex_to_module.cc, translates
// EXIDX unwind information into the same format that Breakpad uses
// for CFI information.  Hence Breakpad's CFI unwinding abilities
// also become usable for EXIDX.
//
// See: "Exception Handling ABI for the ARM Architecture", ARM IHI 0038A
// http://infocenter.arm.com/help/topic/com.arm.doc.ihi0038a/IHI0038A_ehabi.pdf

// EXIDX data is presented in two parts:
//
// * an index table.  This contains two words per routine,
//   the first of which identifies the routine, and the second
//   of which is a reference to the unwind bytecode.  If the
//   bytecode is very compact -- 3 bytes or less -- it can be
//   stored directly in the second word.
//
// * an area containing the unwind bytecodes.

// General flow is: ExceptionTableInfo::Start iterates over all
// of the index table entries (pairs).  For each entry, it:
//
// * calls ExceptionTableInfo::ExtabEntryExtract to copy the bytecode
//   out into an intermediate buffer.

// * uses ExceptionTableInfo::ExtabEntryDecode to parse the intermediate
//   buffer.  Each bytecode instruction is bundled into a
//   arm_ex_to_module::extab_data structure, and handed to ..
//
// * .. ARMExToModule::ImproveStackFrame, which in turn hands it to
//   ARMExToModule::TranslateCmd, and that generates the pseudo-CFI
//   records that Breakpad stores.

#define ARM_EXIDX_CANT_UNWIND 0x00000001
#define ARM_EXIDX_COMPACT     0x80000000
#define ARM_EXTBL_OP_FINISH   0xb0
#define ARM_EXIDX_TABLE_LIMIT (255*4)

namespace arm_ex_reader {

using arm_ex_to_module::ARM_EXIDX_CMD_FINISH;
using arm_ex_to_module::ARM_EXIDX_CMD_SUB_FROM_VSP;
using arm_ex_to_module::ARM_EXIDX_CMD_ADD_TO_VSP;
using arm_ex_to_module::ARM_EXIDX_CMD_REG_POP;
using arm_ex_to_module::ARM_EXIDX_CMD_REG_TO_SP;
using arm_ex_to_module::ARM_EXIDX_CMD_VFP_POP;
using arm_ex_to_module::ARM_EXIDX_CMD_WREG_POP;
using arm_ex_to_module::ARM_EXIDX_CMD_WCGR_POP;
using arm_ex_to_module::ARM_EXIDX_CMD_RESERVED;
using arm_ex_to_module::ARM_EXIDX_CMD_REFUSED;
using arm_ex_to_module::exidx_entry;
using arm_ex_to_module::ARM_EXIDX_VFP_SHIFT_16;
using arm_ex_to_module::ARM_EXIDX_VFP_FSTMD;
using google_breakpad::MemoryRange;


static void* Prel31ToAddr(const void* addr) {
  uint32_t offset32 = *reinterpret_cast<const uint32_t*>(addr);
  // sign extend offset32[30:0] to 64 bits -- copy bit 30 to positions
  // 63:31 inclusive.
  uint64_t offset64 = offset32;
  if (offset64 & (1ULL << 30))
    offset64 |= 0xFFFFFFFF80000000ULL;
  else
    offset64 &= 0x000000007FFFFFFFULL;
  return ((char*)addr) + (uintptr_t)offset64;
}


// Extract unwind bytecode for the function denoted by |entry| into |buf|,
// and return the number of bytes of |buf| written, along with a code
// indicating the outcome.

ExceptionTableInfo::ExExtractResult ExceptionTableInfo::ExtabEntryExtract(
    const struct exidx_entry* entry,
    uint8_t* buf, size_t buf_size,
    size_t* buf_used) {
  MemoryRange mr_out(buf, buf_size);

  *buf_used = 0;

# define PUT_BUF_U8(_byte) \
  do { if (!mr_out.Covers(*buf_used, 1)) return ExOutBufOverflow; \
       buf[(*buf_used)++] = (_byte); } while (0)

# define GET_EX_U32(_lval, _addr, _sec_mr) \
  do { if (!(_sec_mr).Covers(reinterpret_cast<const uint8_t*>(_addr) \
                             - (_sec_mr).data(), 4)) \
         return ExInBufOverflow; \
       (_lval) = *(reinterpret_cast<const uint32_t*>(_addr)); } while (0)

# define GET_EXIDX_U32(_lval, _addr) \
            GET_EX_U32(_lval, _addr, mr_exidx_)
# define GET_EXTAB_U32(_lval, _addr) \
            GET_EX_U32(_lval, _addr, mr_extab_)

  uint32_t data;
  GET_EXIDX_U32(data, &entry->data);

  // A function can be marked CANT_UNWIND if (eg) it is known to be
  // at the bottom of the stack.
  if (data == ARM_EXIDX_CANT_UNWIND)
    return ExCantUnwind;

  uint32_t  pers;          // personality number
  uint32_t  extra;         // number of extra data words required
  uint32_t  extra_allowed; // number of extra data words allowed
  uint32_t* extbl_data;    // the handler entry, if not inlined

  if (data & ARM_EXIDX_COMPACT) {
    // The handler table entry has been inlined into the index table entry.
    // In this case it can only be an ARM-defined compact model, since
    // bit 31 is 1.  Only personalities 0, 1 and 2 are defined for the
    // ARM compact model, but 1 and 2 are "Long format" and may require
    // extra data words.  Hence the allowable personalities here are:
    //   personality 0, in which case 'extra' has no meaning
    //   personality 1, with zero extra words
    //   personality 2, with zero extra words
    extbl_data = NULL;
    pers  = (data >> 24) & 0x0F;
    extra = (data >> 16) & 0xFF;
    extra_allowed = 0;
  }
  else {
    // The index table entry is a pointer to the handler entry.  Note
    // that Prel31ToAddr will read the given address, but we already
    // range-checked above.
    extbl_data = reinterpret_cast<uint32_t*>(Prel31ToAddr(&entry->data));
    GET_EXTAB_U32(data, extbl_data);
    if (!(data & ARM_EXIDX_COMPACT)) {
      // This denotes a "generic model" handler.  That will involve
      // executing arbitary machine code, which is something we
      // can't represent here; hence reject it.
      return ExCantRepresent;
    }
    // So we have a compact model representation.  Again, 3 possible
    // personalities, but this time up to 255 allowable extra words.
    pers  = (data >> 24) & 0x0F;
    extra = (data >> 16) & 0xFF;
    extra_allowed = 255;
    extbl_data++;
  }

  // Now look at the the handler table entry.  The first word is
  // |data| and subsequent words start at |*extbl_data|.  The number
  // of extra words to use is |extra|, provided that the personality
  // allows extra words.  Even if it does, none may be available --
  // extra_allowed is the maximum number of extra words allowed. */
  if (pers == 0) {
    // "Su16" in the documentation -- 3 unwinding insn bytes
    // |extra| has no meaning here; instead that byte is an unwind-info byte
    PUT_BUF_U8(data >> 16);
    PUT_BUF_U8(data >> 8);
    PUT_BUF_U8(data);
  }
  else if ((pers == 1 || pers == 2) && extra <= extra_allowed) {
    // "Lu16" or "Lu32" respectively -- 2 unwinding insn bytes,
    // and up to 255 extra words.
    PUT_BUF_U8(data >> 8);
    PUT_BUF_U8(data);
    for (uint32_t j = 0; j < extra; j++) {
      GET_EXTAB_U32(data, extbl_data);
      extbl_data++;
      PUT_BUF_U8(data >> 24);
      PUT_BUF_U8(data >> 16);
      PUT_BUF_U8(data >> 8);
      PUT_BUF_U8(data >> 0);
    }
  }
  else {
    // The entry is invalid.
    return ExInvalid;
  }

  // Make sure the entry is terminated with "FINISH"
  if (*buf_used > 0 && buf[(*buf_used) - 1] != ARM_EXTBL_OP_FINISH)
    PUT_BUF_U8(ARM_EXTBL_OP_FINISH);

  return ExSuccess;

# undef GET_EXTAB_U32
# undef GET_EXIDX_U32
# undef GET_U32
# undef PUT_BUF_U8
}


// Take the unwind information extracted by ExtabEntryExtract
// and parse it into frame-unwind instructions.  These are as
// specified in "Table 4, ARM-defined frame-unwinding instructions"
// in the specification document detailed in comments at the top
// of this file.
//
// This reads from |buf[0, +data_size)|.  It checks for overruns of
// the input buffer and returns a negative value if that happens, or
// for any other failure cases.  It returns zero in case of success.
int ExceptionTableInfo::ExtabEntryDecode(const uint8_t* buf, size_t buf_size) {
  if (buf == NULL || buf_size == 0)
    return -1;

  MemoryRange mr_in(buf, buf_size);
  const uint8_t* buf_initially = buf;

# define GET_BUF_U8(_lval) \
  do { if (!mr_in.Covers(buf - buf_initially, 1)) return -1; \
       (_lval) = *(buf++); } while (0)

  const uint8_t* end = buf + buf_size;

  while (buf < end) {
    struct arm_ex_to_module::extab_data edata;
    memset(&edata, 0, sizeof(edata));

    uint8_t op;
    GET_BUF_U8(op);
    if ((op & 0xc0) == 0x00) {
      // vsp = vsp + (xxxxxx << 2) + 4
      edata.cmd = ARM_EXIDX_CMD_ADD_TO_VSP;
      edata.data = (((int)op & 0x3f) << 2) + 4;
    } else if ((op & 0xc0) == 0x40) {
      // vsp = vsp - (xxxxxx << 2) - 4
      edata.cmd = ARM_EXIDX_CMD_SUB_FROM_VSP;
      edata.data = (((int)op & 0x3f) << 2) + 4;
    } else if ((op & 0xf0) == 0x80) {
      uint8_t op2;
      GET_BUF_U8(op2);
      if (op == 0x80 && op2 == 0x00) {
        // Refuse to unwind
        edata.cmd = ARM_EXIDX_CMD_REFUSED;
      } else {
        // Pop up to 12 integer registers under masks {r15-r12},{r11-r4}
        edata.cmd = ARM_EXIDX_CMD_REG_POP;
        edata.data = ((op & 0xf) << 8) | op2;
        edata.data = edata.data << 4;
      }
    } else if ((op & 0xf0) == 0x90) {
      if (op == 0x9d || op == 0x9f) {
        // 9d: Reserved as prefix for ARM register to register moves
        // 9f: Reserved as perfix for Intel Wireless MMX reg to reg moves
        edata.cmd = ARM_EXIDX_CMD_RESERVED;
      } else {
        // Set vsp = r[nnnn]
        edata.cmd = ARM_EXIDX_CMD_REG_TO_SP;
        edata.data = op & 0x0f;
      }
    } else if ((op & 0xf0) == 0xa0) {
      // Pop r4 to r[4+nnn],          or
      // Pop r4 to r[4+nnn] and r14   or
      unsigned end = (op & 0x07);
      edata.data = (1 << (end + 1)) - 1;
      edata.data = edata.data << 4;
      if (op & 0x08) edata.data |= 1 << 14;
      edata.cmd = ARM_EXIDX_CMD_REG_POP;
    } else if (op == ARM_EXTBL_OP_FINISH) {
      // Finish
      edata.cmd = ARM_EXIDX_CMD_FINISH;
      buf = end;
    } else if (op == 0xb1) {
      uint8_t op2;
      GET_BUF_U8(op2);
      if (op2 == 0 || (op2 & 0xf0)) {
        // Spare
        edata.cmd = ARM_EXIDX_CMD_RESERVED;
      } else {
        // Pop integer registers under mask {r3,r2,r1,r0}
        edata.cmd = ARM_EXIDX_CMD_REG_POP;
        edata.data = op2 & 0x0f;
      }
    } else if (op == 0xb2) {
      // vsp = vsp + 0x204 + (uleb128 << 2)
      uint64_t offset = 0;
      uint8_t byte, shift = 0;
      do {
        GET_BUF_U8(byte);
        offset |= (byte & 0x7f) << shift;
        shift += 7;
      } while ((byte & 0x80) && buf < end);
      edata.data = offset * 4 + 0x204;
      edata.cmd = ARM_EXIDX_CMD_ADD_TO_VSP;
    } else if (op == 0xb3 || op == 0xc8 || op == 0xc9) {
      // b3: Pop VFP regs D[ssss]    to D[ssss+cccc],    FSTMFDX-ishly
      // c8: Pop VFP regs D[16+ssss] to D[16+ssss+cccc], FSTMFDD-ishly
      // c9: Pop VFP regs D[ssss]    to D[ssss+cccc],    FSTMFDD-ishly
      edata.cmd = ARM_EXIDX_CMD_VFP_POP;
      GET_BUF_U8(edata.data);
      if (op == 0xc8) edata.data |= ARM_EXIDX_VFP_SHIFT_16;
      if (op != 0xb3) edata.data |= ARM_EXIDX_VFP_FSTMD;
    } else if ((op & 0xf8) == 0xb8 || (op & 0xf8) == 0xd0) {
      // b8: Pop VFP regs D[8] to D[8+nnn], FSTMFDX-ishly
      // d0: Pop VFP regs D[8] to D[8+nnn], FSTMFDD-ishly
      edata.cmd = ARM_EXIDX_CMD_VFP_POP;
      edata.data = 0x80 | (op & 0x07);
      if ((op & 0xf8) == 0xd0) edata.data |= ARM_EXIDX_VFP_FSTMD;
    } else if (op >= 0xc0 && op <= 0xc5) {
      // Intel Wireless MMX pop wR[10]-wr[10+nnn], nnn != 6,7
      edata.cmd = ARM_EXIDX_CMD_WREG_POP;
      edata.data = 0xa0 | (op & 0x07);
    } else if (op == 0xc6) {
      // Intel Wireless MMX pop wR[ssss] to wR[ssss+cccc]
      edata.cmd = ARM_EXIDX_CMD_WREG_POP;
      GET_BUF_U8(edata.data);
    } else if (op == 0xc7) {
      uint8_t op2;
      GET_BUF_U8(op2);
      if (op2 == 0 || (op2 & 0xf0)) {
        // Spare
        edata.cmd = ARM_EXIDX_CMD_RESERVED;
      } else {
        // Intel Wireless MMX pop wCGR registers under mask {wCGR3,2,1,0}
        edata.cmd = ARM_EXIDX_CMD_WCGR_POP;
        edata.data = op2 & 0x0f;
      }
    } else {
      // Spare
      edata.cmd = ARM_EXIDX_CMD_RESERVED;
    }

    int ret = handler_->ImproveStackFrame(&edata);
    if (ret < 0)
      return ret;
  }
  return 0;

# undef GET_BUF_U8
}

void ExceptionTableInfo::Start() {
  const struct exidx_entry* start
    = reinterpret_cast<const struct exidx_entry*>(mr_exidx_.data());
  const struct exidx_entry* end
    = reinterpret_cast<const struct exidx_entry*>(mr_exidx_.data()
                                                  + mr_exidx_.length());

  // Iterate over each of the EXIDX entries (pairs of 32-bit words).
  // These occupy the entire .exidx section.
  for (const struct exidx_entry* entry = start; entry < end; ++entry) {
    // Figure out the code address range that this table entry is
    // associated with.
    uint32_t addr = (reinterpret_cast<char*>(Prel31ToAddr(&entry->addr))
                     - mapping_addr_ + loading_addr_) & 0x7fffffff;
    uint32_t next_addr;
    if (entry < end - 1) {
      next_addr = (reinterpret_cast<char*>(Prel31ToAddr(&((entry + 1)->addr)))
                   - mapping_addr_ + loading_addr_) & 0x7fffffff;
    } else {
      // This is the last EXIDX entry in the sequence, so we don't
      // have an address for the start of the next function, to limit
      // this one.  Instead use the address of the last byte of the
      // text section associated with this .exidx section, that we
      // have been given.  So as to avoid junking up the CFI unwind
      // tables with absurdly large address ranges in the case where
      // text_last_svma_ is wrong, only use the value if it is nonzero
      // and within one page of |addr|.  Otherwise assume a length of 1.
      //
      // In some cases, gcc has been observed to finish the exidx
      // section with an entry of length 1 marked CANT_UNWIND,
      // presumably exactly for the purpose of giving a definite
      // length for the last real entry, without having to look at
      // text segment boundaries.
      bool plausible = false;
      next_addr = addr + 1;
      if (text_last_svma_ != 0) {
        uint32_t maybe_next_addr = text_last_svma_ + 1;
        if (maybe_next_addr > addr && maybe_next_addr - addr <= 4096) {
          next_addr = maybe_next_addr;
          plausible = true;
        }
      }
      if (!plausible) {
        fprintf(stderr, "ExceptionTableInfo: implausible EXIDX last entry size "
                "%d, using 1 instead.", (int32_t)(text_last_svma_ - addr));
      }
    }

    // Extract the unwind info into |buf|.  This might fail for
    // various reasons.  It involves reading both the .exidx and
    // .extab sections.  All accesses to those sections are
    // bounds-checked.
    uint8_t buf[ARM_EXIDX_TABLE_LIMIT];
    size_t buf_used = 0;
    ExExtractResult res = ExtabEntryExtract(entry, buf, sizeof(buf), &buf_used);
    if (res != ExSuccess) {
      // Couldn't extract the unwind info, for some reason.  Move on.
      switch (res) {
        case ExInBufOverflow:
          fprintf(stderr, "ExtabEntryExtract: .exidx/.extab section overrun");
          break;
        case ExOutBufOverflow:
          fprintf(stderr, "ExtabEntryExtract: bytecode buffer overflow");
          break;
        case ExCantUnwind:
          fprintf(stderr, "ExtabEntryExtract: function is marked CANT_UNWIND");
          break;
        case ExCantRepresent:
          fprintf(stderr, "ExtabEntryExtract: bytecode can't be represented");
          break;
        case ExInvalid:
          fprintf(stderr, "ExtabEntryExtract: index table entry is invalid");
          break;
        default:
          fprintf(stderr, "ExtabEntryExtract: unknown error: %d", (int)res);
          break;
      }
      continue;
    }

    // Finally, work through the unwind instructions in |buf| and
    // create CFI entries that Breakpad can use.  This can also fail.
    // First, add a new stack frame entry, into which ExtabEntryDecode
    // will write the CFI entries.
    if (!handler_->HasStackFrame(addr, next_addr - addr)) {
      handler_->AddStackFrame(addr, next_addr - addr);
      int ret = ExtabEntryDecode(buf, buf_used);
      if (ret < 0) {
	handler_->DeleteStackFrame();
	fprintf(stderr, "ExtabEntryDecode: failed with error code: %d", ret);
	continue;
      }
      handler_->SubmitStackFrame();
    }

  } /* iterating over .exidx */
}

}  // namespace arm_ex_reader
