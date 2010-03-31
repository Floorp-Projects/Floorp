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

#include "common/linux/dwarf_cfi_to_module.h"

namespace google_breakpad {

using std::ostringstream;

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
    entry_->initial_rules[".ra"] = register_names_[return_address_];

  return true;
}

string DwarfCFIToModule::RegisterName(int i) {
  assert(entry_);
  if (i < 0) {
    assert(i == kCFARegister);
    return ".cfa";
  }
  unsigned reg = i;
  if (reg == return_address_)
    return ".ra";

  if (0 <= reg && reg < register_names_.size())
    return register_names_[reg];

  reporter_->UnnamedRegister(entry_offset_, reg);
  char buf[30];
  sprintf(buf, "unnamed_register%u", reg);
  return buf;
}

void DwarfCFIToModule::Record(Module::Address address, int reg,
                              const string &rule) {
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
  ostringstream s;
  s << RegisterName(reg);
  Record(address, reg, s.str());
  return true;
}

bool DwarfCFIToModule::OffsetRule(uint64 address, int reg,
                                  int base_register, long offset) {
  ostringstream s;
  s << RegisterName(base_register) << " " << offset << " + ^";
  Record(address, reg, s.str());
  return true;
}

bool DwarfCFIToModule::ValOffsetRule(uint64 address, int reg,
                                     int base_register, long offset) {
  ostringstream s;
  s << RegisterName(base_register) << " " << offset << " +";
  Record(address, reg, s.str());
  return true;
}

bool DwarfCFIToModule::RegisterRule(uint64 address, int reg,
                                    int base_register) {
  ostringstream s;
  s << RegisterName(base_register);
  Record(address, reg, s.str());
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
  fprintf(stderr, "%s, section '%s': "
          "the call frame entry at offset 0x%zx refers to register %d,"
          " whose name we don't know\n",
          file_.c_str(), section_.c_str(), offset, reg);
}

void DwarfCFIToModule::Reporter::UndefinedNotSupported(size_t offset,
                                                       const string &reg) {
  fprintf(stderr, "%s, section '%s': "
          "the call frame entry at offset 0x%zx sets the rule for "
          "register '%s' to 'undefined', but the Breakpad symbol file format"
          " cannot express this\n",
          file_.c_str(), section_.c_str(), offset, reg.c_str());
}

void DwarfCFIToModule::Reporter::ExpressionsNotSupported(size_t offset,
                                                         const string &reg) {
  fprintf(stderr, "%s, section '%s': "
          "the call frame entry at offset 0x%zx uses a DWARF expression to"
          " describe how to recover register '%s', "
          " but this translator cannot yet translate DWARF expressions to"
          " Breakpad postfix expressions\n",
          file_.c_str(), section_.c_str(), offset, reg.c_str());
}

} // namespace google_breakpad
