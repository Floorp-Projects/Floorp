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

// This file translates EXIDX unwind information into the same format
// that LUL uses for CFI information.  Hence LUL's CFI unwinding
// abilities also become usable for EXIDX.
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
//
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

// This file is derived from the following files in
// toolkit/crashreporter/google-breakpad:
//   src/common/arm_ex_to_module.cc
//   src/common/arm_ex_reader.cc

#include "mozilla/Assertions.h"
#include "mozilla/NullPtr.h"

#include "LulExidxExt.h"


#define ARM_EXBUF_START(x) (((x) >> 4) & 0x0f)
#define ARM_EXBUF_COUNT(x) ((x) & 0x0f)
#define ARM_EXBUF_END(x)   (ARM_EXBUF_START(x) + ARM_EXBUF_COUNT(x))

namespace lul {

// Translate command from extab_data to command for Module.
int ARMExToModule::TranslateCmd(const struct extab_data* edata,
                                LExpr& vsp) {
  int ret = 0;
  switch (edata->cmd) {
    case ARM_EXIDX_CMD_FINISH:
      /* Copy LR to PC if there isn't currently a rule for PC in force. */
      if (curr_rules_.mR15expr.mHow == LExpr::UNKNOWN) {
        if (curr_rules_.mR14expr.mHow == LExpr::UNKNOWN) {
          curr_rules_.mR15expr = LExpr(LExpr::NODEREF, DW_REG_ARM_R14, 0);
        } else {
          curr_rules_.mR15expr = curr_rules_.mR14expr;
        }
      }
      break;
    case ARM_EXIDX_CMD_SUB_FROM_VSP:
      vsp = vsp.add_delta(- static_cast<long>(edata->data));
      break;
    case ARM_EXIDX_CMD_ADD_TO_VSP:
      vsp = vsp.add_delta(static_cast<long>(edata->data));
      break;
    case ARM_EXIDX_CMD_REG_POP:
      for (unsigned int i = 0; i < 16; i++) {
        if (edata->data & (1 << i)) {
          // See if we're summarising for int register |i|.  If so,
          // describe how to pull it off the stack.  The cast of |i| is
          // a bit of a kludge but works because DW_REG_ARM_Rn has the
          // value |n|, for 0 <= |n| <= 15 -- that is, for the ARM 
          // general-purpose registers.
          LExpr* regI_exprP = curr_rules_.ExprForRegno((DW_REG_NUMBER)i);
          if (regI_exprP) {
            *regI_exprP = vsp.deref();
          }
          vsp = vsp.add_delta(4);
        }
      }
      /* Set cfa in case the SP got popped. */
      if (edata->data & (1 << 13)) {
        vsp = curr_rules_.mR13expr;
      }
      break;
    case ARM_EXIDX_CMD_REG_TO_SP: {
      MOZ_ASSERT (edata->data < 16);
      int    reg_no    = edata->data;
      // Same comment as above, re the casting of |reg_no|, applies.
      LExpr* reg_exprP = curr_rules_.ExprForRegno((DW_REG_NUMBER)reg_no);
      if (reg_exprP) {
        if (reg_exprP->mHow == LExpr::UNKNOWN) {
          curr_rules_.mR13expr = LExpr(LExpr::NODEREF, reg_no, 0);
        } else {
          curr_rules_.mR13expr = *reg_exprP;
        }
        vsp = curr_rules_.mR13expr;
      }
      break;
    }
    case ARM_EXIDX_CMD_VFP_POP:
      /* Don't recover VFP registers, but be sure to adjust the stack
         pointer. */
      for (unsigned int i = ARM_EXBUF_START(edata->data);
           i <= ARM_EXBUF_END(edata->data); i++) {
        vsp = vsp.add_delta(8);
      }
      if (!(edata->data & ARM_EXIDX_VFP_FSTMD)) {
        vsp = vsp.add_delta(4);
      }
      break;
    case ARM_EXIDX_CMD_WREG_POP:
      for (unsigned int i = ARM_EXBUF_START(edata->data);
           i <= ARM_EXBUF_END(edata->data); i++) {
        vsp = vsp.add_delta(8);
      }
      break;
    case ARM_EXIDX_CMD_WCGR_POP:
      // Pop wCGR registers under mask {wCGR3,2,1,0}, hence "i < 4"
      for (unsigned int i = 0; i < 4; i++) {
        if (edata->data & (1 << i)) {
          vsp = vsp.add_delta(4);
        }
      }
      break;
    case ARM_EXIDX_CMD_REFUSED:
    case ARM_EXIDX_CMD_RESERVED:
      ret = -1;
      break;
  }
  return ret;
}

void ARMExToModule::AddStackFrame(uintptr_t addr, size_t size) {
  // Here we are effectively reinitialising the EXIDX summariser for a
  // new code address range.  smap_ stays unchanged.  All other fields
  // are reinitialised.
  vsp_ = LExpr(LExpr::NODEREF, DW_REG_ARM_R13, 0);
  (void) new (&curr_rules_) RuleSet();
  curr_rules_.mAddr = (uintptr_t)addr;
  curr_rules_.mLen  = (uintptr_t)size;
  if (0) {
    char buf[100];
    sprintf(buf, "  AddStackFrame    %llx .. %llx",
            (uint64_t)addr, (uint64_t)(addr + size - 1));
    log_(buf);
  }
}

int ARMExToModule::ImproveStackFrame(const struct extab_data* edata) {
  return TranslateCmd(edata, vsp_) ;
}

void ARMExToModule::DeleteStackFrame() {
}

void ARMExToModule::SubmitStackFrame() {
  // JRS: I'm really not sure what this means, or if it is necessary
  // return address always winds up in pc
  //stack_frame_entry_->initial_rules[ustr__ZDra()] // ".ra"
  //  = stack_frame_entry_->initial_rules[ustr__pc()];
  // maybe don't need to do anything here?

  // the final value of vsp is the new value of sp
  curr_rules_.mR13expr = vsp_;

  // Finally, add the completed RuleSet to the SecMap
  if (curr_rules_.mLen > 0) {

    // Futz with the rules for r4 .. r11 in the same way as happens
    // with the CFI summariser:
    /* Mark callee-saved registers (r4 .. r11) as unchanged, if there is
       no other information about them.  FIXME: do this just once, at
       the point where the ruleset is committed. */
    if (curr_rules_.mR7expr.mHow == LExpr::UNKNOWN) {
      curr_rules_.mR7expr = LExpr(LExpr::NODEREF, DW_REG_ARM_R7, 0);
    }
    if (curr_rules_.mR11expr.mHow == LExpr::UNKNOWN) {
      curr_rules_.mR11expr = LExpr(LExpr::NODEREF, DW_REG_ARM_R11, 0);
    }
    if (curr_rules_.mR12expr.mHow == LExpr::UNKNOWN) {
      curr_rules_.mR12expr = LExpr(LExpr::NODEREF, DW_REG_ARM_R12, 0);
    }
    if (curr_rules_.mR14expr.mHow == LExpr::UNKNOWN) {
      curr_rules_.mR14expr = LExpr(LExpr::NODEREF, DW_REG_ARM_R14, 0);
    }

    // And add them
    smap_->AddRuleSet(&curr_rules_);

    if (0) {
      curr_rules_.Print(log_);
    }
    if (0) {
      char buf[100];
      sprintf(buf, "  SubmitStackFrame %llx .. %llx",
              (uint64_t)curr_rules_.mAddr,
              (uint64_t)(curr_rules_.mAddr + curr_rules_.mLen - 1));
      log_(buf);
    }
  }
}


#define ARM_EXIDX_CANT_UNWIND 0x00000001
#define ARM_EXIDX_COMPACT     0x80000000
#define ARM_EXTBL_OP_FINISH   0xb0
#define ARM_EXIDX_TABLE_LIMIT (255*4)

using lul::ARM_EXIDX_CMD_FINISH;
using lul::ARM_EXIDX_CMD_SUB_FROM_VSP;
using lul::ARM_EXIDX_CMD_ADD_TO_VSP;
using lul::ARM_EXIDX_CMD_REG_POP;
using lul::ARM_EXIDX_CMD_REG_TO_SP;
using lul::ARM_EXIDX_CMD_VFP_POP;
using lul::ARM_EXIDX_CMD_WREG_POP;
using lul::ARM_EXIDX_CMD_WCGR_POP;
using lul::ARM_EXIDX_CMD_RESERVED;
using lul::ARM_EXIDX_CMD_REFUSED;
using lul::exidx_entry;
using lul::ARM_EXIDX_VFP_SHIFT_16;
using lul::ARM_EXIDX_VFP_FSTMD;
using lul::MemoryRange;


static void* Prel31ToAddr(const void* addr)
{
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

ExceptionTableInfo::ExExtractResult
ExceptionTableInfo::ExtabEntryExtract(const struct exidx_entry* entry,
                                      uint8_t* buf, size_t buf_size,
                                      /*OUT*/size_t* buf_used)
{
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
            GET_EX_U32(_lval, _addr, mr_exidx_avma_)
# define GET_EXTAB_U32(_lval, _addr) \
            GET_EX_U32(_lval, _addr, mr_extab_avma_)

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
    extbl_data = nullptr;
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
int ExceptionTableInfo::ExtabEntryDecode(const uint8_t* buf, size_t buf_size)
{
  if (buf == nullptr || buf_size == 0)
    return -1;

  MemoryRange mr_in(buf, buf_size);
  const uint8_t* buf_initially = buf;

# define GET_BUF_U8(_lval) \
  do { if (!mr_in.Covers(buf - buf_initially, 1)) return -1; \
       (_lval) = *(buf++); } while (0)

  const uint8_t* end = buf + buf_size;

  while (buf < end) {
    struct lul::extab_data edata;
    memset(&edata, 0, sizeof(edata));

    uint8_t op;
    GET_BUF_U8(op);
    if ((op & 0xc0) == 0x00) {
      // vsp = vsp + (xxxxxx << 2) + 4
      edata.cmd = ARM_EXIDX_CMD_ADD_TO_VSP;
      edata.data = (((int)op & 0x3f) << 2) + 4;
    }
    else if ((op & 0xc0) == 0x40) {
      // vsp = vsp - (xxxxxx << 2) - 4
      edata.cmd = ARM_EXIDX_CMD_SUB_FROM_VSP;
      edata.data = (((int)op & 0x3f) << 2) + 4;
    }
    else if ((op & 0xf0) == 0x80) {
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
    }
    else if ((op & 0xf0) == 0x90) {
      if (op == 0x9d || op == 0x9f) {
        // 9d: Reserved as prefix for ARM register to register moves
        // 9f: Reserved as perfix for Intel Wireless MMX reg to reg moves
        edata.cmd = ARM_EXIDX_CMD_RESERVED;
      } else {
        // Set vsp = r[nnnn]
        edata.cmd = ARM_EXIDX_CMD_REG_TO_SP;
        edata.data = op & 0x0f;
      }
    }
    else if ((op & 0xf0) == 0xa0) {
      // Pop r4 to r[4+nnn],          or
      // Pop r4 to r[4+nnn] and r14   or
      unsigned end = (op & 0x07);
      edata.data = (1 << (end + 1)) - 1;
      edata.data = edata.data << 4;
      if (op & 0x08) edata.data |= 1 << 14;
      edata.cmd = ARM_EXIDX_CMD_REG_POP;
    }
    else if (op == ARM_EXTBL_OP_FINISH) {
      // Finish
      edata.cmd = ARM_EXIDX_CMD_FINISH;
      buf = end;
    }
    else if (op == 0xb1) {
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
    }
    else if (op == 0xb2) {
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
    }
    else if (op == 0xb3 || op == 0xc8 || op == 0xc9) {
      // b3: Pop VFP regs D[ssss]    to D[ssss+cccc],    FSTMFDX-ishly
      // c8: Pop VFP regs D[16+ssss] to D[16+ssss+cccc], FSTMFDD-ishly
      // c9: Pop VFP regs D[ssss]    to D[ssss+cccc],    FSTMFDD-ishly
      edata.cmd = ARM_EXIDX_CMD_VFP_POP;
      GET_BUF_U8(edata.data);
      if (op == 0xc8) edata.data |= ARM_EXIDX_VFP_SHIFT_16;
      if (op != 0xb3) edata.data |= ARM_EXIDX_VFP_FSTMD;
    }
    else if ((op & 0xf8) == 0xb8 || (op & 0xf8) == 0xd0) {
      // b8: Pop VFP regs D[8] to D[8+nnn], FSTMFDX-ishly
      // d0: Pop VFP regs D[8] to D[8+nnn], FSTMFDD-ishly
      edata.cmd = ARM_EXIDX_CMD_VFP_POP;
      edata.data = 0x80 | (op & 0x07);
      if ((op & 0xf8) == 0xd0) edata.data |= ARM_EXIDX_VFP_FSTMD;
    }
    else if (op >= 0xc0 && op <= 0xc5) {
      // Intel Wireless MMX pop wR[10]-wr[10+nnn], nnn != 6,7
      edata.cmd = ARM_EXIDX_CMD_WREG_POP;
      edata.data = 0xa0 | (op & 0x07);
    }
    else if (op == 0xc6) {
      // Intel Wireless MMX pop wR[ssss] to wR[ssss+cccc]
      edata.cmd = ARM_EXIDX_CMD_WREG_POP;
      GET_BUF_U8(edata.data);
    }
    else if (op == 0xc7) {
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
    }
    else {
      // Spare
      edata.cmd = ARM_EXIDX_CMD_RESERVED;
    }

    int ret = handler_->ImproveStackFrame(&edata);
    if (ret < 0) return ret;
  }
  return 0;

# undef GET_BUF_U8
}

void ExceptionTableInfo::Start()
{
  const struct exidx_entry* start
    = reinterpret_cast<const struct exidx_entry*>(mr_exidx_avma_.data());
  const struct exidx_entry* end
    = reinterpret_cast<const struct exidx_entry*>(mr_exidx_avma_.data()
                                                  + mr_exidx_avma_.length());

  // Iterate over each of the EXIDX entries (pairs of 32-bit words).
  // These occupy the entire .exidx section.
  for (const struct exidx_entry* entry = start; entry < end; ++entry) {

    // Figure out the code address range that this table entry is
    // associated with.
    uint32_t avma = reinterpret_cast<uint32_t>(Prel31ToAddr(&entry->addr));
    uint32_t next_avma;
    if (entry < end - 1) {
      next_avma
        = reinterpret_cast<uint32_t>(Prel31ToAddr(&((entry + 1)->addr)));
    } else {
      // This is the last EXIDX entry in the sequence, so we don't
      // have an address for the start of the next function, to limit
      // this one.  Instead use the address of the last byte of the
      // text section associated with this .exidx section, that we
      // have been given.  So as to avoid junking up the CFI unwind
      // tables with absurdly large address ranges in the case where
      // text_last_avma_ is wrong, only use the value if it is nonzero
      // and within one page of |avma|.  Otherwise assume a length of 1.
      //
      // In some cases, gcc has been observed to finish the exidx
      // section with an entry of length 1 marked CANT_UNWIND,
      // presumably exactly for the purpose of giving a definite
      // length for the last real entry, without having to look at
      // text segment boundaries.

      bool plausible;
      uint32_t maybe_next_avma = text_last_avma_ + 1;
      if (maybe_next_avma > avma && maybe_next_avma - avma <= 4096) {
        next_avma = maybe_next_avma;
        plausible = true;
      } else {
        next_avma = avma + 1;
        plausible = false;
      }

      if (!plausible && avma != text_last_avma_ + 1) {
        char buf[100];
        snprintf(buf, sizeof(buf),
                 "ExceptionTableInfo: implausible EXIDX last entry size %d"
                 "; using 1 instead.", (int32_t)(text_last_avma_ - avma));
        buf[sizeof(buf)-1] = 0;
        log_(buf);
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
          log_("ExtabEntryExtract: .exidx/.extab section overrun");
          break;
        case ExOutBufOverflow:
          log_("ExtabEntryExtract: bytecode buffer overflow");
          break;
        case ExCantUnwind:
          log_("ExtabEntryExtract: function is marked CANT_UNWIND");
          break;
        case ExCantRepresent:
          log_("ExtabEntryExtract: bytecode can't be represented");
          break;
        case ExInvalid:
          log_("ExtabEntryExtract: index table entry is invalid");
          break;
        default: {
          char buf[100];
          snprintf(buf, sizeof(buf),
                   "ExtabEntryExtract: unknown error: %d", (int)res);
          buf[sizeof(buf)-1] = 0;
          log_(buf);
          break;
        }
      }
      continue;
    }

    // Finally, work through the unwind instructions in |buf| and
    // create CFI entries that Breakpad can use.  This can also fail.
    // First, add a new stack frame entry, into which ExtabEntryDecode
    // will write the CFI entries.
    handler_->AddStackFrame(avma, next_avma - avma);
    int ret = ExtabEntryDecode(buf, buf_used);
    if (ret < 0) {
      handler_->DeleteStackFrame();
      char buf[100];
      snprintf(buf, sizeof(buf),
               "ExtabEntryDecode: failed with error code: %d", ret);
      buf[sizeof(buf)-1] = 0;
      log_(buf);
      continue;
    }
    handler_->SubmitStackFrame();
  } /* iterating over .exidx */
}

} // namespace lul
