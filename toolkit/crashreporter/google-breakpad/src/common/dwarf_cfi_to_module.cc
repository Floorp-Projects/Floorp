// -*- mode: c++ -*-

// Copyright (c) 2010, Google Inc.
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

// Original author: Jim Blandy <jimb@mozilla.com> <jimb@red-bean.com>

// Implementation of google_breakpad::DwarfCFIToModule.
// See dwarf_cfi_to_module.h for details.

#include <sstream>
#include <iomanip>

#include "common/dwarf_cfi_to_module.h"
#include "common/logging.h"

namespace google_breakpad {

using std::ostringstream;

vector<const UniqueString*> DwarfCFIToModule::RegisterNames::MakeVector(
    const char* const* strings,
    size_t size) {
  vector<const UniqueString*> names(size, NULL);
  for (size_t i = 0; i < size; ++i) {
    names[i] = ToUniqueString(strings[i]);
  }

  return names;
}

vector<const UniqueString*> DwarfCFIToModule::RegisterNames::I386() {
  static const char *const names[] = {
    "$eax", "$ecx", "$edx", "$ebx", "$esp", "$ebp", "$esi", "$edi",
    "$eip", "$eflags", "$unused1",
    "$st0", "$st1", "$st2", "$st3", "$st4", "$st5", "$st6", "$st7",
    "$unused2", "$unused3",
    "$xmm0", "$xmm1", "$xmm2", "$xmm3", "$xmm4", "$xmm5", "$xmm6", "$xmm7",
    "$mm0", "$mm1", "$mm2", "$mm3", "$mm4", "$mm5", "$mm6", "$mm7",
    "$fcw", "$fsw", "$mxcsr",
    "$es", "$cs", "$ss", "$ds", "$fs", "$gs", "$unused4", "$unused5",
    "$tr", "$ldtr"
  };

  return MakeVector(names, sizeof(names) / sizeof(names[0]));
}

vector<const UniqueString*> DwarfCFIToModule::RegisterNames::X86_64() {
  static const char *const names[] = {
    "$rax", "$rdx", "$rcx", "$rbx", "$rsi", "$rdi", "$rbp", "$rsp",
    "$r8",  "$r9",  "$r10", "$r11", "$r12", "$r13", "$r14", "$r15",
    "$rip",
    "$xmm0","$xmm1","$xmm2", "$xmm3", "$xmm4", "$xmm5", "$xmm6", "$xmm7",
    "$xmm8","$xmm9","$xmm10","$xmm11","$xmm12","$xmm13","$xmm14","$xmm15",
    "$st0", "$st1", "$st2", "$st3", "$st4", "$st5", "$st6", "$st7",
    "$mm0", "$mm1", "$mm2", "$mm3", "$mm4", "$mm5", "$mm6", "$mm7",
    "$rflags",
    "$es", "$cs", "$ss", "$ds", "$fs", "$gs", "$unused1", "$unused2",
    "$fs.base", "$gs.base", "$unused3", "$unused4",
    "$tr", "$ldtr",
    "$mxcsr", "$fcw", "$fsw"
  };

  return MakeVector(names, sizeof(names) / sizeof(names[0]));
}

// Per ARM IHI 0040A, section 3.1
vector<const UniqueString*> DwarfCFIToModule::RegisterNames::ARM() {
  static const char *const names[] = {
    "r0",  "r1",  "r2",  "r3",  "r4",  "r5",  "r6",  "r7",
    "r8",  "r9",  "r10", "r11", "r12", "sp",  "lr",  "pc",
    "f0",  "f1",  "f2",  "f3",  "f4",  "f5",  "f6",  "f7",
    "fps", "cpsr", "",   "",    "",    "",    "",    "",
    "",    "",    "",    "",    "",    "",    "",    "",
    "",    "",    "",    "",    "",    "",    "",    "",
    "",    "",    "",    "",    "",    "",    "",    "",
    "",    "",    "",    "",    "",    "",    "",    "",
    "s0",  "s1",  "s2",  "s3",  "s4",  "s5",  "s6",  "s7",
    "s8",  "s9",  "s10", "s11", "s12", "s13", "s14", "s15",
    "s16", "s17", "s18", "s19", "s20", "s21", "s22", "s23",
    "s24", "s25", "s26", "s27", "s28", "s29", "s30", "s31",
    "f0",  "f1",  "f2",  "f3",  "f4",  "f5",  "f6",  "f7"
  };

  return MakeVector(names, sizeof(names) / sizeof(names[0]));
}

bool DwarfCFIToModule::Entry(size_t offset, uint64 address, uint64 length,
                             uint8 version, const string &augmentation,
                             unsigned return_address) {
  assert(!entry_);

  // If dwarf2reader::CallFrameInfo can handle this version and
  // augmentation, then we should be okay with that, so there's no
  // need to check them here.

  // Get ready to collect entries.
  entry_ = new Module::StackFrameEntry;
  entry_->address = address;
  entry_->size = length;
  entry_offset_ = offset;
  return_address_ = return_address;

  // Breakpad STACK CFI records must provide a .ra rule, but DWARF CFI
  // may not establish any rule for .ra if the return address column
  // is an ordinary register, and that register holds the return
  // address on entry to the function. So establish an initial .ra
  // rule citing the return address register.
  if (return_address_ < register_names_.size())
    entry_->initial_rules[ustr__ZDra()]
      = Module::Expr(register_names_[return_address_], 0, false);

  return true;
}

const UniqueString* DwarfCFIToModule::RegisterName(int i) {
  assert(entry_);
  if (i < 0) {
    assert(i == kCFARegister);
    return ustr__ZDcfa();
  }
  unsigned reg = i;
  if (reg == return_address_)
    return ustr__ZDra();

  // Ensure that a non-empty name exists for this register value.
  if (reg < register_names_.size() && register_names_[reg] != ustr__empty())
    return register_names_[reg];

  reporter_->UnnamedRegister(entry_offset_, reg);
  char buf[30];
  sprintf(buf, "unnamed_register%u", reg);
  return ToUniqueString(buf);
}

void DwarfCFIToModule::Record(Module::Address address, int reg,
                              const Module::Expr &rule) {
  assert(entry_);

  // Is this one of this entry's initial rules?
  if (address == entry_->address)
    entry_->initial_rules[RegisterName(reg)] = rule;
  // File it under the appropriate address.
  else
    entry_->rule_changes[address][RegisterName(reg)] = rule;
}

bool DwarfCFIToModule::UndefinedRule(uint64 address, int reg) {
  reporter_->UndefinedNotSupported(entry_offset_, RegisterName(reg));
  // Treat this as a non-fatal error.
  return true;
}

bool DwarfCFIToModule::SameValueRule(uint64 address, int reg) {
  // reg + 0
  Module::Expr rule
    = Module::Expr(RegisterName(reg), 0, false);
  Record(address, reg, rule);
  return true;
}

bool DwarfCFIToModule::OffsetRule(uint64 address, int reg,
                                  int base_register, long offset) {
  // *(base_register + offset)
  Module::Expr rule
    = Module::Expr(RegisterName(base_register), offset, true);
  Record(address, reg, rule);
  return true;
}

bool DwarfCFIToModule::ValOffsetRule(uint64 address, int reg,
                                     int base_register, long offset) {
  // base_register + offset
  Module::Expr rule
    = Module::Expr(RegisterName(base_register), offset, false);
  Record(address, reg, rule);
  return true;
}

bool DwarfCFIToModule::RegisterRule(uint64 address, int reg,
                                    int base_register) {
  // base_register + 0
  Module::Expr rule
    = Module::Expr(RegisterName(base_register), 0, false);
  Record(address, reg, rule);
  return true;
}

bool DwarfCFIToModule::ExpressionRule(uint64 address, int reg,
                                      const string &expression) {
  reporter_->ExpressionsNotSupported(entry_offset_, RegisterName(reg));
  // Treat this as a non-fatal error.
  return true;
}

bool DwarfCFIToModule::ValExpressionRule(uint64 address, int reg,
                                         const string &expression) {
  reporter_->ExpressionsNotSupported(entry_offset_, RegisterName(reg));
  // Treat this as a non-fatal error.
  return true;
}

bool DwarfCFIToModule::End() {
  module_->AddStackFrameEntry(entry_);
  entry_ = NULL;
  return true;
}

void DwarfCFIToModule::Reporter::UnnamedRegister(size_t offset, int reg) {
  BPLOG(INFO) << file_ << ", section '" << section_ 
    << "': the call frame entry at offset 0x" 
    << std::setbase(16) << offset << std::setbase(10)
    << " refers to register " << reg << ", whose name we don't know";
}

void DwarfCFIToModule::Reporter::UndefinedNotSupported(
    size_t offset,
    const UniqueString* reg) {
  BPLOG(INFO) << file_ << ", section '" << section_ 
    << "': the call frame entry at offset 0x" 
    << std::setbase(16) << offset << std::setbase(10)
    << " sets the rule for register '" << FromUniqueString(reg)
    << "' to 'undefined', but the Breakpad symbol file format cannot "
    << " express this";
}

void DwarfCFIToModule::Reporter::ExpressionsNotSupported(
    size_t offset,
    const UniqueString* reg) {
  static uint64_t n_complaints = 0; // This isn't threadsafe
  n_complaints++;
  if (!is_power_of_2(n_complaints))
    return;
  BPLOG(INFO) << file_ << ", section '" << section_ 
    << "': the call frame entry at offset 0x" 
    << std::setbase(16) << offset << std::setbase(10)
    << " uses a DWARF expression to describe how to recover register '"
    << FromUniqueString(reg) << "', but this translator cannot yet "
    << "translate DWARF expressions to Breakpad postfix expressions (shown "
    << n_complaints << " times)";
}

} // namespace google_breakpad
