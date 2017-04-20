/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */

// Copyright (c) 2010 Google Inc. All Rights Reserved.
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

// CFI reader author: Jim Blandy <jimb@mozilla.com> <jimb@red-bean.com>
// Original author: Jim Blandy <jimb@mozilla.com> <jimb@red-bean.com>

// Implementation of dwarf2reader::LineInfo, dwarf2reader::CompilationUnit,
// and dwarf2reader::CallFrameInfo. See dwarf2reader.h for details.

// This file is derived from the following files in
// toolkit/crashreporter/google-breakpad:
//   src/common/dwarf/bytereader.cc
//   src/common/dwarf/dwarf2reader.cc
//   src/common/dwarf_cfi_to_module.cc

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <map>
#include <stack>
#include <string>

#include "mozilla/Assertions.h"
#include "mozilla/Sprintf.h"

#include "LulCommonExt.h"
#include "LulDwarfInt.h"


// Set this to 1 for verbose logging
#define DEBUG_DWARF 0


namespace lul {

using std::string;

ByteReader::ByteReader(enum Endianness endian)
    :offset_reader_(NULL), address_reader_(NULL), endian_(endian),
     address_size_(0), offset_size_(0),
     have_section_base_(), have_text_base_(), have_data_base_(),
     have_function_base_() { }

ByteReader::~ByteReader() { }

void ByteReader::SetOffsetSize(uint8 size) {
  offset_size_ = size;
  MOZ_ASSERT(size == 4 || size == 8);
  if (size == 4) {
    this->offset_reader_ = &ByteReader::ReadFourBytes;
  } else {
    this->offset_reader_ = &ByteReader::ReadEightBytes;
  }
}

void ByteReader::SetAddressSize(uint8 size) {
  address_size_ = size;
  MOZ_ASSERT(size == 4 || size == 8);
  if (size == 4) {
    this->address_reader_ = &ByteReader::ReadFourBytes;
  } else {
    this->address_reader_ = &ByteReader::ReadEightBytes;
  }
}

uint64 ByteReader::ReadInitialLength(const char* start, size_t* len) {
  const uint64 initial_length = ReadFourBytes(start);
  start += 4;

  // In DWARF2/3, if the initial length is all 1 bits, then the offset
  // size is 8 and we need to read the next 8 bytes for the real length.
  if (initial_length == 0xffffffff) {
    SetOffsetSize(8);
    *len = 12;
    return ReadOffset(start);
  } else {
    SetOffsetSize(4);
    *len = 4;
  }
  return initial_length;
}

bool ByteReader::ValidEncoding(DwarfPointerEncoding encoding) const {
  if (encoding == DW_EH_PE_omit) return true;
  if (encoding == DW_EH_PE_aligned) return true;
  if ((encoding & 0x7) > DW_EH_PE_udata8)
    return false;
  if ((encoding & 0x70) > DW_EH_PE_funcrel)
    return false;
  return true;
}

bool ByteReader::UsableEncoding(DwarfPointerEncoding encoding) const {
  switch (encoding & 0x70) {
    case DW_EH_PE_absptr:  return true;
    case DW_EH_PE_pcrel:   return have_section_base_;
    case DW_EH_PE_textrel: return have_text_base_;
    case DW_EH_PE_datarel: return have_data_base_;
    case DW_EH_PE_funcrel: return have_function_base_;
    default:               return false;
  }
}

uint64 ByteReader::ReadEncodedPointer(const char *buffer,
                                      DwarfPointerEncoding encoding,
                                      size_t *len) const {
  // UsableEncoding doesn't approve of DW_EH_PE_omit, so we shouldn't
  // see it here.
  MOZ_ASSERT(encoding != DW_EH_PE_omit);

  // The Linux Standards Base 4.0 does not make this clear, but the
  // GNU tools (gcc/unwind-pe.h; readelf/dwarf.c; gdb/dwarf2-frame.c)
  // agree that aligned pointers are always absolute, machine-sized,
  // machine-signed pointers.
  if (encoding == DW_EH_PE_aligned) {
    MOZ_ASSERT(have_section_base_);

    // We don't need to align BUFFER in *our* address space. Rather, we
    // need to find the next position in our buffer that would be aligned
    // when the .eh_frame section the buffer contains is loaded into the
    // program's memory. So align assuming that buffer_base_ gets loaded at
    // address section_base_, where section_base_ itself may or may not be
    // aligned.

    // First, find the offset to START from the closest prior aligned
    // address.
    uint64 skew = section_base_ & (AddressSize() - 1);
    // Now find the offset from that aligned address to buffer.
    uint64 offset = skew + (buffer - buffer_base_);
    // Round up to the next boundary.
    uint64 aligned = (offset + AddressSize() - 1) & -AddressSize();
    // Convert back to a pointer.
    const char *aligned_buffer = buffer_base_ + (aligned - skew);
    // Finally, store the length and actually fetch the pointer.
    *len = aligned_buffer - buffer + AddressSize();
    return ReadAddress(aligned_buffer);
  }

  // Extract the value first, ignoring whether it's a pointer or an
  // offset relative to some base.
  uint64 offset;
  switch (encoding & 0x0f) {
    case DW_EH_PE_absptr:
      // DW_EH_PE_absptr is weird, as it is used as a meaningful value for
      // both the high and low nybble of encoding bytes. When it appears in
      // the high nybble, it means that the pointer is absolute, not an
      // offset from some base address. When it appears in the low nybble,
      // as here, it means that the pointer is stored as a normal
      // machine-sized and machine-signed address. A low nybble of
      // DW_EH_PE_absptr does not imply that the pointer is absolute; it is
      // correct for us to treat the value as an offset from a base address
      // if the upper nybble is not DW_EH_PE_absptr.
      offset = ReadAddress(buffer);
      *len = AddressSize();
      break;

    case DW_EH_PE_uleb128:
      offset = ReadUnsignedLEB128(buffer, len);
      break;

    case DW_EH_PE_udata2:
      offset = ReadTwoBytes(buffer);
      *len = 2;
      break;

    case DW_EH_PE_udata4:
      offset = ReadFourBytes(buffer);
      *len = 4;
      break;

    case DW_EH_PE_udata8:
      offset = ReadEightBytes(buffer);
      *len = 8;
      break;

    case DW_EH_PE_sleb128:
      offset = ReadSignedLEB128(buffer, len);
      break;

    case DW_EH_PE_sdata2:
      offset = ReadTwoBytes(buffer);
      // Sign-extend from 16 bits.
      offset = (offset ^ 0x8000) - 0x8000;
      *len = 2;
      break;

    case DW_EH_PE_sdata4:
      offset = ReadFourBytes(buffer);
      // Sign-extend from 32 bits.
      offset = (offset ^ 0x80000000ULL) - 0x80000000ULL;
      *len = 4;
      break;

    case DW_EH_PE_sdata8:
      // No need to sign-extend; this is the full width of our type.
      offset = ReadEightBytes(buffer);
      *len = 8;
      break;

    default:
      abort();
  }

  // Find the appropriate base address.
  uint64 base;
  switch (encoding & 0x70) {
    case DW_EH_PE_absptr:
      base = 0;
      break;

    case DW_EH_PE_pcrel:
      MOZ_ASSERT(have_section_base_);
      base = section_base_ + (buffer - buffer_base_);
      break;

    case DW_EH_PE_textrel:
      MOZ_ASSERT(have_text_base_);
      base = text_base_;
      break;

    case DW_EH_PE_datarel:
      MOZ_ASSERT(have_data_base_);
      base = data_base_;
      break;

    case DW_EH_PE_funcrel:
      MOZ_ASSERT(have_function_base_);
      base = function_base_;
      break;

    default:
      abort();
  }

  uint64 pointer = base + offset;

  // Remove inappropriate upper bits.
  if (AddressSize() == 4)
    pointer = pointer & 0xffffffff;
  else
    MOZ_ASSERT(AddressSize() == sizeof(uint64));

  return pointer;
}


// A DWARF rule for recovering the address or value of a register, or
// computing the canonical frame address. There is one subclass of this for
// each '*Rule' member function in CallFrameInfo::Handler.
//
// It's annoying that we have to handle Rules using pointers (because
// the concrete instances can have an arbitrary size). They're small,
// so it would be much nicer if we could just handle them by value
// instead of fretting about ownership and destruction.
//
// It seems like all these could simply be instances of std::tr1::bind,
// except that we need instances to be EqualityComparable, too.
//
// This could logically be nested within State, but then the qualified names
// get horrendous.
class CallFrameInfo::Rule {
 public:
  virtual ~Rule() { }

  // Tell HANDLER that, at ADDRESS in the program, REGISTER can be
  // recovered using this rule. If REGISTER is kCFARegister, then this rule
  // describes how to compute the canonical frame address. Return what the
  // HANDLER member function returned.
  virtual bool Handle(Handler *handler, uint64 address, int register) const = 0;

  // Equality on rules. We use these to decide which rules we need
  // to report after a DW_CFA_restore_state instruction.
  virtual bool operator==(const Rule &rhs) const = 0;

  bool operator!=(const Rule &rhs) const { return ! (*this == rhs); }

  // Return a pointer to a copy of this rule.
  virtual Rule *Copy() const = 0;

  // If this is a base+offset rule, change its base register to REG.
  // Otherwise, do nothing. (Ugly, but required for DW_CFA_def_cfa_register.)
  virtual void SetBaseRegister(unsigned reg) { }

  // If this is a base+offset rule, change its offset to OFFSET. Otherwise,
  // do nothing. (Ugly, but required for DW_CFA_def_cfa_offset.)
  virtual void SetOffset(long long offset) { }

  // A RTTI workaround, to make it possible to implement equality
  // comparisons on classes derived from this one.
  enum CFIRTag {
    CFIR_UNDEFINED_RULE,
    CFIR_SAME_VALUE_RULE,
    CFIR_OFFSET_RULE,
    CFIR_VAL_OFFSET_RULE,
    CFIR_REGISTER_RULE,
    CFIR_EXPRESSION_RULE,
    CFIR_VAL_EXPRESSION_RULE
  };

  // Produce the tag that identifies the child class of this object.
  virtual CFIRTag getTag() const = 0;
};

// Rule: the value the register had in the caller cannot be recovered.
class CallFrameInfo::UndefinedRule: public CallFrameInfo::Rule {
 public:
  UndefinedRule() { }
  ~UndefinedRule() { }
  CFIRTag getTag() const { return CFIR_UNDEFINED_RULE; }
  bool Handle(Handler *handler, uint64 address, int reg) const {
    return handler->UndefinedRule(address, reg);
  }
  bool operator==(const Rule &rhs) const {
    if (rhs.getTag() != CFIR_UNDEFINED_RULE) return false;
    return true;
  }
  Rule *Copy() const { return new UndefinedRule(*this); }
};

// Rule: the register's value is the same as that it had in the caller.
class CallFrameInfo::SameValueRule: public CallFrameInfo::Rule {
 public:
  SameValueRule() { }
  ~SameValueRule() { }
  CFIRTag getTag() const { return CFIR_SAME_VALUE_RULE; }
  bool Handle(Handler *handler, uint64 address, int reg) const {
    return handler->SameValueRule(address, reg);
  }
  bool operator==(const Rule &rhs) const {
    if (rhs.getTag() != CFIR_SAME_VALUE_RULE) return false;
    return true;
  }
  Rule *Copy() const { return new SameValueRule(*this); }
};

// Rule: the register is saved at OFFSET from BASE_REGISTER.  BASE_REGISTER
// may be CallFrameInfo::Handler::kCFARegister.
class CallFrameInfo::OffsetRule: public CallFrameInfo::Rule {
 public:
  OffsetRule(int base_register, long offset)
      : base_register_(base_register), offset_(offset) { }
  ~OffsetRule() { }
  CFIRTag getTag() const { return CFIR_OFFSET_RULE; }
  bool Handle(Handler *handler, uint64 address, int reg) const {
    return handler->OffsetRule(address, reg, base_register_, offset_);
  }
  bool operator==(const Rule &rhs) const {
    if (rhs.getTag() != CFIR_OFFSET_RULE) return false;
    const OffsetRule *our_rhs = static_cast<const OffsetRule *>(&rhs);
    return (base_register_ == our_rhs->base_register_ &&
            offset_ == our_rhs->offset_);
  }
  Rule *Copy() const { return new OffsetRule(*this); }
  // We don't actually need SetBaseRegister or SetOffset here, since they
  // are only ever applied to CFA rules, for DW_CFA_def_cfa_offset, and it
  // doesn't make sense to use OffsetRule for computing the CFA: it
  // computes the address at which a register is saved, not a value.
 private:
  int base_register_;
  long offset_;
};

// Rule: the value the register had in the caller is the value of
// BASE_REGISTER plus offset. BASE_REGISTER may be
// CallFrameInfo::Handler::kCFARegister.
class CallFrameInfo::ValOffsetRule: public CallFrameInfo::Rule {
 public:
  ValOffsetRule(int base_register, long offset)
      : base_register_(base_register), offset_(offset) { }
  ~ValOffsetRule() { }
  CFIRTag getTag() const { return CFIR_VAL_OFFSET_RULE; }
  bool Handle(Handler *handler, uint64 address, int reg) const {
    return handler->ValOffsetRule(address, reg, base_register_, offset_);
  }
  bool operator==(const Rule &rhs) const {
    if (rhs.getTag() != CFIR_VAL_OFFSET_RULE) return false;
    const ValOffsetRule *our_rhs = static_cast<const ValOffsetRule *>(&rhs);
    return (base_register_ == our_rhs->base_register_ &&
            offset_ == our_rhs->offset_);
  }
  Rule *Copy() const { return new ValOffsetRule(*this); }
  void SetBaseRegister(unsigned reg) { base_register_ = reg; }
  void SetOffset(long long offset) { offset_ = offset; }
 private:
  int base_register_;
  long offset_;
};

// Rule: the register has been saved in another register REGISTER_NUMBER_.
class CallFrameInfo::RegisterRule: public CallFrameInfo::Rule {
 public:
  explicit RegisterRule(int register_number)
      : register_number_(register_number) { }
  ~RegisterRule() { }
  CFIRTag getTag() const { return CFIR_REGISTER_RULE; }
  bool Handle(Handler *handler, uint64 address, int reg) const {
    return handler->RegisterRule(address, reg, register_number_);
  }
  bool operator==(const Rule &rhs) const {
    if (rhs.getTag() != CFIR_REGISTER_RULE) return false;
    const RegisterRule *our_rhs = static_cast<const RegisterRule *>(&rhs);
    return (register_number_ == our_rhs->register_number_);
  }
  Rule *Copy() const { return new RegisterRule(*this); }
 private:
  int register_number_;
};

// Rule: EXPRESSION evaluates to the address at which the register is saved.
class CallFrameInfo::ExpressionRule: public CallFrameInfo::Rule {
 public:
  explicit ExpressionRule(const string &expression)
      : expression_(expression) { }
  ~ExpressionRule() { }
  CFIRTag getTag() const { return CFIR_EXPRESSION_RULE; }
  bool Handle(Handler *handler, uint64 address, int reg) const {
    return handler->ExpressionRule(address, reg, expression_);
  }
  bool operator==(const Rule &rhs) const {
    if (rhs.getTag() != CFIR_EXPRESSION_RULE) return false;
    const ExpressionRule *our_rhs = static_cast<const ExpressionRule *>(&rhs);
    return (expression_ == our_rhs->expression_);
  }
  Rule *Copy() const { return new ExpressionRule(*this); }
 private:
  string expression_;
};

// Rule: EXPRESSION evaluates to the previous value of the register.
class CallFrameInfo::ValExpressionRule: public CallFrameInfo::Rule {
 public:
  explicit ValExpressionRule(const string &expression)
      : expression_(expression) { }
  ~ValExpressionRule() { }
  CFIRTag getTag() const { return CFIR_VAL_EXPRESSION_RULE; }
  bool Handle(Handler *handler, uint64 address, int reg) const {
    return handler->ValExpressionRule(address, reg, expression_);
  }
  bool operator==(const Rule &rhs) const {
    if (rhs.getTag() != CFIR_VAL_EXPRESSION_RULE) return false;
    const ValExpressionRule *our_rhs =
        static_cast<const ValExpressionRule *>(&rhs);
    return (expression_ == our_rhs->expression_);
  }
  Rule *Copy() const { return new ValExpressionRule(*this); }
 private:
  string expression_;
};

// A map from register numbers to rules.
class CallFrameInfo::RuleMap {
 public:
  RuleMap() : cfa_rule_(NULL) { }
  RuleMap(const RuleMap &rhs) : cfa_rule_(NULL) { *this = rhs; }
  ~RuleMap() { Clear(); }

  RuleMap &operator=(const RuleMap &rhs);

  // Set the rule for computing the CFA to RULE. Take ownership of RULE.
  void SetCFARule(Rule *rule) { delete cfa_rule_; cfa_rule_ = rule; }

  // Return the current CFA rule. Unlike RegisterRule, this RuleMap retains
  // ownership of the rule. We use this for DW_CFA_def_cfa_offset and
  // DW_CFA_def_cfa_register, and for detecting references to the CFA before
  // a rule for it has been established.
  Rule *CFARule() const { return cfa_rule_; }

  // Return the rule for REG, or NULL if there is none. The caller takes
  // ownership of the result.
  Rule *RegisterRule(int reg) const;

  // Set the rule for computing REG to RULE. Take ownership of RULE.
  void SetRegisterRule(int reg, Rule *rule);

  // Make all the appropriate calls to HANDLER as if we were changing from
  // this RuleMap to NEW_RULES at ADDRESS. We use this to implement
  // DW_CFA_restore_state, where lots of rules can change simultaneously.
  // Return true if all handlers returned true; otherwise, return false.
  bool HandleTransitionTo(Handler *handler, uint64 address,
                          const RuleMap &new_rules) const;

 private:
  // A map from register numbers to Rules.
  typedef std::map<int, Rule *> RuleByNumber;

  // Remove all register rules and clear cfa_rule_.
  void Clear();

  // The rule for computing the canonical frame address. This RuleMap owns
  // this rule.
  Rule *cfa_rule_;

  // A map from register numbers to postfix expressions to recover
  // their values. This RuleMap owns the Rules the map refers to.
  RuleByNumber registers_;
};

CallFrameInfo::RuleMap &CallFrameInfo::RuleMap::operator=(const RuleMap &rhs) {
  Clear();
  // Since each map owns the rules it refers to, assignment must copy them.
  if (rhs.cfa_rule_) cfa_rule_ = rhs.cfa_rule_->Copy();
  for (RuleByNumber::const_iterator it = rhs.registers_.begin();
       it != rhs.registers_.end(); it++)
    registers_[it->first] = it->second->Copy();
  return *this;
}

CallFrameInfo::Rule *CallFrameInfo::RuleMap::RegisterRule(int reg) const {
  MOZ_ASSERT(reg != Handler::kCFARegister);
  RuleByNumber::const_iterator it = registers_.find(reg);
  if (it != registers_.end())
    return it->second->Copy();
  else
    return NULL;
}

void CallFrameInfo::RuleMap::SetRegisterRule(int reg, Rule *rule) {
  MOZ_ASSERT(reg != Handler::kCFARegister);
  MOZ_ASSERT(rule);
  Rule **slot = &registers_[reg];
  delete *slot;
  *slot = rule;
}

bool CallFrameInfo::RuleMap::HandleTransitionTo(
    Handler *handler,
    uint64 address,
    const RuleMap &new_rules) const {
  // Transition from cfa_rule_ to new_rules.cfa_rule_.
  if (cfa_rule_ && new_rules.cfa_rule_) {
    if (*cfa_rule_ != *new_rules.cfa_rule_ &&
        !new_rules.cfa_rule_->Handle(handler, address, Handler::kCFARegister))
      return false;
  } else if (cfa_rule_) {
    // this RuleMap has a CFA rule but new_rules doesn't.
    // CallFrameInfo::Handler has no way to handle this --- and shouldn't;
    // it's garbage input. The instruction interpreter should have
    // detected this and warned, so take no action here.
  } else if (new_rules.cfa_rule_) {
    // This shouldn't be possible: NEW_RULES is some prior state, and
    // there's no way to remove entries.
    MOZ_ASSERT(0);
  } else {
    // Both CFA rules are empty.  No action needed.
  }

  // Traverse the two maps in order by register number, and report
  // whatever differences we find.
  RuleByNumber::const_iterator old_it = registers_.begin();
  RuleByNumber::const_iterator new_it = new_rules.registers_.begin();
  while (old_it != registers_.end() && new_it != new_rules.registers_.end()) {
    if (old_it->first < new_it->first) {
      // This RuleMap has an entry for old_it->first, but NEW_RULES
      // doesn't.
      //
      // This isn't really the right thing to do, but since CFI generally
      // only mentions callee-saves registers, and GCC's convention for
      // callee-saves registers is that they are unchanged, it's a good
      // approximation.
      if (!handler->SameValueRule(address, old_it->first))
        return false;
      old_it++;
    } else if (old_it->first > new_it->first) {
      // NEW_RULES has entry for new_it->first, but this RuleMap
      // doesn't. This shouldn't be possible: NEW_RULES is some prior
      // state, and there's no way to remove entries.
      MOZ_ASSERT(0);
    } else {
      // Both maps have an entry for this register. Report the new
      // rule if it is different.
      if (*old_it->second != *new_it->second &&
          !new_it->second->Handle(handler, address, new_it->first))
        return false;
      new_it++; old_it++;
    }
  }
  // Finish off entries from this RuleMap with no counterparts in new_rules.
  while (old_it != registers_.end()) {
    if (!handler->SameValueRule(address, old_it->first))
      return false;
    old_it++;
  }
  // Since we only make transitions from a rule set to some previously
  // saved rule set, and we can only add rules to the map, NEW_RULES
  // must have fewer rules than *this.
  MOZ_ASSERT(new_it == new_rules.registers_.end());

  return true;
}

// Remove all register rules and clear cfa_rule_.
void CallFrameInfo::RuleMap::Clear() {
  delete cfa_rule_;
  cfa_rule_ = NULL;
  for (RuleByNumber::iterator it = registers_.begin();
       it != registers_.end(); it++)
    delete it->second;
  registers_.clear();
}

// The state of the call frame information interpreter as it processes
// instructions from a CIE and FDE.
class CallFrameInfo::State {
 public:
  // Create a call frame information interpreter state with the given
  // reporter, reader, handler, and initial call frame info address.
  State(ByteReader *reader, Handler *handler, Reporter *reporter,
        uint64 address)
      : reader_(reader), handler_(handler), reporter_(reporter),
        address_(address), entry_(NULL), cursor_(NULL),
        saved_rules_(NULL) { }

  ~State() {
    if (saved_rules_)
      delete saved_rules_;
  }

  // Interpret instructions from CIE, save the resulting rule set for
  // DW_CFA_restore instructions, and return true. On error, report
  // the problem to reporter_ and return false.
  bool InterpretCIE(const CIE &cie);

  // Interpret instructions from FDE, and return true. On error,
  // report the problem to reporter_ and return false.
  bool InterpretFDE(const FDE &fde);

 private:
  // The operands of a CFI instruction, for ParseOperands.
  struct Operands {
    unsigned register_number;  // A register number.
    uint64 offset;             // An offset or address.
    long signed_offset;        // A signed offset.
    string expression;         // A DWARF expression.
  };

  // Parse CFI instruction operands from STATE's instruction stream as
  // described by FORMAT. On success, populate OPERANDS with the
  // results, and return true. On failure, report the problem and
  // return false.
  //
  // Each character of FORMAT should be one of the following:
  //
  //   'r'  unsigned LEB128 register number (OPERANDS->register_number)
  //   'o'  unsigned LEB128 offset          (OPERANDS->offset)
  //   's'  signed LEB128 offset            (OPERANDS->signed_offset)
  //   'a'  machine-size address            (OPERANDS->offset)
  //        (If the CIE has a 'z' augmentation string, 'a' uses the
  //        encoding specified by the 'R' argument.)
  //   '1'  a one-byte offset               (OPERANDS->offset)
  //   '2'  a two-byte offset               (OPERANDS->offset)
  //   '4'  a four-byte offset              (OPERANDS->offset)
  //   '8'  an eight-byte offset            (OPERANDS->offset)
  //   'e'  a DW_FORM_block holding a       (OPERANDS->expression)
  //        DWARF expression
  bool ParseOperands(const char *format, Operands *operands);

  // Interpret one CFI instruction from STATE's instruction stream, update
  // STATE, report any rule changes to handler_, and return true. On
  // failure, report the problem and return false.
  bool DoInstruction();

  // The following Do* member functions are subroutines of DoInstruction,
  // factoring out the actual work of operations that have several
  // different encodings.

  // Set the CFA rule to be the value of BASE_REGISTER plus OFFSET, and
  // return true. On failure, report and return false. (Used for
  // DW_CFA_def_cfa and DW_CFA_def_cfa_sf.)
  bool DoDefCFA(unsigned base_register, long offset);

  // Change the offset of the CFA rule to OFFSET, and return true. On
  // failure, report and return false. (Subroutine for
  // DW_CFA_def_cfa_offset and DW_CFA_def_cfa_offset_sf.)
  bool DoDefCFAOffset(long offset);

  // Specify that REG can be recovered using RULE, and return true. On
  // failure, report and return false.
  bool DoRule(unsigned reg, Rule *rule);

  // Specify that REG can be found at OFFSET from the CFA, and return true.
  // On failure, report and return false. (Subroutine for DW_CFA_offset,
  // DW_CFA_offset_extended, and DW_CFA_offset_extended_sf.)
  bool DoOffset(unsigned reg, long offset);

  // Specify that the caller's value for REG is the CFA plus OFFSET,
  // and return true. On failure, report and return false. (Subroutine
  // for DW_CFA_val_offset and DW_CFA_val_offset_sf.)
  bool DoValOffset(unsigned reg, long offset);

  // Restore REG to the rule established in the CIE, and return true. On
  // failure, report and return false. (Subroutine for DW_CFA_restore and
  // DW_CFA_restore_extended.)
  bool DoRestore(unsigned reg);

  // Return the section offset of the instruction at cursor. For use
  // in error messages.
  uint64 CursorOffset() { return entry_->offset + (cursor_ - entry_->start); }

  // Report that entry_ is incomplete, and return false. For brevity.
  bool ReportIncomplete() {
    reporter_->Incomplete(entry_->offset, entry_->kind);
    return false;
  }

  // For reading multi-byte values with the appropriate endianness.
  ByteReader *reader_;

  // The handler to which we should report the data we find.
  Handler *handler_;

  // For reporting problems in the info we're parsing.
  Reporter *reporter_;

  // The code address to which the next instruction in the stream applies.
  uint64 address_;

  // The entry whose instructions we are currently processing. This is
  // first a CIE, and then an FDE.
  const Entry *entry_;

  // The next instruction to process.
  const char *cursor_;

  // The current set of rules.
  RuleMap rules_;

  // The set of rules established by the CIE, used by DW_CFA_restore
  // and DW_CFA_restore_extended. We set this after interpreting the
  // CIE's instructions.
  RuleMap cie_rules_;

  // A stack of saved states, for DW_CFA_remember_state and
  // DW_CFA_restore_state.
  std::stack<RuleMap>* saved_rules_;
};

bool CallFrameInfo::State::InterpretCIE(const CIE &cie) {
  entry_ = &cie;
  cursor_ = entry_->instructions;
  while (cursor_ < entry_->end)
    if (!DoInstruction())
      return false;
  // Note the rules established by the CIE, for use by DW_CFA_restore
  // and DW_CFA_restore_extended.
  cie_rules_ = rules_;
  return true;
}

bool CallFrameInfo::State::InterpretFDE(const FDE &fde) {
  entry_ = &fde;
  cursor_ = entry_->instructions;
  while (cursor_ < entry_->end)
    if (!DoInstruction())
      return false;
  return true;
}

bool CallFrameInfo::State::ParseOperands(const char *format,
                                         Operands *operands) {
  size_t len;
  const char *operand;

  for (operand = format; *operand; operand++) {
    size_t bytes_left = entry_->end - cursor_;
    switch (*operand) {
      case 'r':
        operands->register_number = reader_->ReadUnsignedLEB128(cursor_, &len);
        if (len > bytes_left) return ReportIncomplete();
        cursor_ += len;
        break;

      case 'o':
        operands->offset = reader_->ReadUnsignedLEB128(cursor_, &len);
        if (len > bytes_left) return ReportIncomplete();
        cursor_ += len;
        break;

      case 's':
        operands->signed_offset = reader_->ReadSignedLEB128(cursor_, &len);
        if (len > bytes_left) return ReportIncomplete();
        cursor_ += len;
        break;

      case 'a':
        operands->offset =
          reader_->ReadEncodedPointer(cursor_, entry_->cie->pointer_encoding,
                                      &len);
        if (len > bytes_left) return ReportIncomplete();
        cursor_ += len;
        break;

      case '1':
        if (1 > bytes_left) return ReportIncomplete();
        operands->offset = static_cast<unsigned char>(*cursor_++);
        break;

      case '2':
        if (2 > bytes_left) return ReportIncomplete();
        operands->offset = reader_->ReadTwoBytes(cursor_);
        cursor_ += 2;
        break;

      case '4':
        if (4 > bytes_left) return ReportIncomplete();
        operands->offset = reader_->ReadFourBytes(cursor_);
        cursor_ += 4;
        break;

      case '8':
        if (8 > bytes_left) return ReportIncomplete();
        operands->offset = reader_->ReadEightBytes(cursor_);
        cursor_ += 8;
        break;

      case 'e': {
        size_t expression_length = reader_->ReadUnsignedLEB128(cursor_, &len);
        if (len > bytes_left || expression_length > bytes_left - len)
          return ReportIncomplete();
        cursor_ += len;
        operands->expression = string(cursor_, expression_length);
        cursor_ += expression_length;
        break;
      }

      default:
        MOZ_ASSERT(0);
    }
  }

  return true;
}

bool CallFrameInfo::State::DoInstruction() {
  CIE *cie = entry_->cie;
  Operands ops;

  // Our entry's kind should have been set by now.
  MOZ_ASSERT(entry_->kind != kUnknown);

  // We shouldn't have been invoked unless there were more
  // instructions to parse.
  MOZ_ASSERT(cursor_ < entry_->end);

  unsigned opcode = *cursor_++;
  if ((opcode & 0xc0) != 0) {
    switch (opcode & 0xc0) {
      // Advance the address.
      case DW_CFA_advance_loc: {
        size_t code_offset = opcode & 0x3f;
        address_ += code_offset * cie->code_alignment_factor;
        break;
      }

      // Find a register at an offset from the CFA.
      case DW_CFA_offset:
        if (!ParseOperands("o", &ops) ||
            !DoOffset(opcode & 0x3f, ops.offset * cie->data_alignment_factor))
          return false;
        break;

      // Restore the rule established for a register by the CIE.
      case DW_CFA_restore:
        if (!DoRestore(opcode & 0x3f)) return false;
        break;

      // The 'if' above should have excluded this possibility.
      default:
        MOZ_ASSERT(0);
    }

    // Return here, so the big switch below won't be indented.
    return true;
  }

  switch (opcode) {
    // Set the address.
    case DW_CFA_set_loc:
      if (!ParseOperands("a", &ops)) return false;
      address_ = ops.offset;
      break;

    // Advance the address.
    case DW_CFA_advance_loc1:
      if (!ParseOperands("1", &ops)) return false;
      address_ += ops.offset * cie->code_alignment_factor;
      break;

    // Advance the address.
    case DW_CFA_advance_loc2:
      if (!ParseOperands("2", &ops)) return false;
      address_ += ops.offset * cie->code_alignment_factor;
      break;

    // Advance the address.
    case DW_CFA_advance_loc4:
      if (!ParseOperands("4", &ops)) return false;
      address_ += ops.offset * cie->code_alignment_factor;
      break;

    // Advance the address.
    case DW_CFA_MIPS_advance_loc8:
      if (!ParseOperands("8", &ops)) return false;
      address_ += ops.offset * cie->code_alignment_factor;
      break;

    // Compute the CFA by adding an offset to a register.
    case DW_CFA_def_cfa:
      if (!ParseOperands("ro", &ops) ||
          !DoDefCFA(ops.register_number, ops.offset))
        return false;
      break;

    // Compute the CFA by adding an offset to a register.
    case DW_CFA_def_cfa_sf:
      if (!ParseOperands("rs", &ops) ||
          !DoDefCFA(ops.register_number,
                    ops.signed_offset * cie->data_alignment_factor))
        return false;
      break;

    // Change the base register used to compute the CFA.
    case DW_CFA_def_cfa_register: {
      Rule *cfa_rule = rules_.CFARule();
      if (!cfa_rule) {
        reporter_->NoCFARule(entry_->offset, entry_->kind, CursorOffset());
        return false;
      }
      if (!ParseOperands("r", &ops)) return false;
      cfa_rule->SetBaseRegister(ops.register_number);
      if (!cfa_rule->Handle(handler_, address_, Handler::kCFARegister))
        return false;
      break;
    }

    // Change the offset used to compute the CFA.
    case DW_CFA_def_cfa_offset:
      if (!ParseOperands("o", &ops) ||
          !DoDefCFAOffset(ops.offset))
        return false;
      break;

    // Change the offset used to compute the CFA.
    case DW_CFA_def_cfa_offset_sf:
      if (!ParseOperands("s", &ops) ||
          !DoDefCFAOffset(ops.signed_offset * cie->data_alignment_factor))
        return false;
      break;

    // Specify an expression whose value is the CFA.
    case DW_CFA_def_cfa_expression: {
      if (!ParseOperands("e", &ops))
        return false;
      Rule *rule = new ValExpressionRule(ops.expression);
      rules_.SetCFARule(rule);
      if (!rule->Handle(handler_, address_, Handler::kCFARegister))
        return false;
      break;
    }

    // The register's value cannot be recovered.
    case DW_CFA_undefined: {
      if (!ParseOperands("r", &ops) ||
          !DoRule(ops.register_number, new UndefinedRule()))
        return false;
      break;
    }

    // The register's value is unchanged from its value in the caller.
    case DW_CFA_same_value: {
      if (!ParseOperands("r", &ops) ||
          !DoRule(ops.register_number, new SameValueRule()))
        return false;
      break;
    }

    // Find a register at an offset from the CFA.
    case DW_CFA_offset_extended:
      if (!ParseOperands("ro", &ops) ||
          !DoOffset(ops.register_number,
                    ops.offset * cie->data_alignment_factor))
        return false;
      break;

    // The register is saved at an offset from the CFA.
    case DW_CFA_offset_extended_sf:
      if (!ParseOperands("rs", &ops) ||
          !DoOffset(ops.register_number,
                    ops.signed_offset * cie->data_alignment_factor))
        return false;
      break;

    // The register is saved at an offset from the CFA.
    case DW_CFA_GNU_negative_offset_extended:
      if (!ParseOperands("ro", &ops) ||
          !DoOffset(ops.register_number,
                    -ops.offset * cie->data_alignment_factor))
        return false;
      break;

    // The register's value is the sum of the CFA plus an offset.
    case DW_CFA_val_offset:
      if (!ParseOperands("ro", &ops) ||
          !DoValOffset(ops.register_number,
                       ops.offset * cie->data_alignment_factor))
        return false;
      break;

    // The register's value is the sum of the CFA plus an offset.
    case DW_CFA_val_offset_sf:
      if (!ParseOperands("rs", &ops) ||
          !DoValOffset(ops.register_number,
                       ops.signed_offset * cie->data_alignment_factor))
        return false;
      break;

    // The register has been saved in another register.
    case DW_CFA_register: {
      if (!ParseOperands("ro", &ops) ||
          !DoRule(ops.register_number, new RegisterRule(ops.offset)))
        return false;
      break;
    }

    // An expression yields the address at which the register is saved.
    case DW_CFA_expression: {
      if (!ParseOperands("re", &ops) ||
          !DoRule(ops.register_number, new ExpressionRule(ops.expression)))
        return false;
      break;
    }

    // An expression yields the caller's value for the register.
    case DW_CFA_val_expression: {
      if (!ParseOperands("re", &ops) ||
          !DoRule(ops.register_number, new ValExpressionRule(ops.expression)))
        return false;
      break;
    }

    // Restore the rule established for a register by the CIE.
    case DW_CFA_restore_extended:
      if (!ParseOperands("r", &ops) ||
          !DoRestore( ops.register_number))
        return false;
      break;

    // Save the current set of rules on a stack.
    case DW_CFA_remember_state:
      if (!saved_rules_) {
        saved_rules_ = new std::stack<RuleMap>();
      }
      saved_rules_->push(rules_);
      break;

    // Pop the current set of rules off the stack.
    case DW_CFA_restore_state: {
      if (!saved_rules_ || saved_rules_->empty()) {
        reporter_->EmptyStateStack(entry_->offset, entry_->kind,
                                   CursorOffset());
        return false;
      }
      const RuleMap &new_rules = saved_rules_->top();
      if (rules_.CFARule() && !new_rules.CFARule()) {
        reporter_->ClearingCFARule(entry_->offset, entry_->kind,
                                   CursorOffset());
        return false;
      }
      rules_.HandleTransitionTo(handler_, address_, new_rules);
      rules_ = new_rules;
      saved_rules_->pop();
      break;
    }

    // No operation.  (Padding instruction.)
    case DW_CFA_nop:
      break;

    // A SPARC register window save: Registers 8 through 15 (%o0-%o7)
    // are saved in registers 24 through 31 (%i0-%i7), and registers
    // 16 through 31 (%l0-%l7 and %i0-%i7) are saved at CFA offsets
    // (0-15 * the register size). The register numbers must be
    // hard-coded. A GNU extension, and not a pretty one.
    case DW_CFA_GNU_window_save: {
      // Save %o0-%o7 in %i0-%i7.
      for (int i = 8; i < 16; i++)
        if (!DoRule(i, new RegisterRule(i + 16)))
          return false;
      // Save %l0-%l7 and %i0-%i7 at the CFA.
      for (int i = 16; i < 32; i++)
        // Assume that the byte reader's address size is the same as
        // the architecture's register size. !@#%*^ hilarious.
        if (!DoRule(i, new OffsetRule(Handler::kCFARegister,
                                      (i - 16) * reader_->AddressSize())))
          return false;
      break;
    }

    // I'm not sure what this is. GDB doesn't use it for unwinding.
    case DW_CFA_GNU_args_size:
      if (!ParseOperands("o", &ops)) return false;
      break;

    // An opcode we don't recognize.
    default: {
      reporter_->BadInstruction(entry_->offset, entry_->kind, CursorOffset());
      return false;
    }
  }

  return true;
}

bool CallFrameInfo::State::DoDefCFA(unsigned base_register, long offset) {
  Rule *rule = new ValOffsetRule(base_register, offset);
  rules_.SetCFARule(rule);
  return rule->Handle(handler_, address_, Handler::kCFARegister);
}

bool CallFrameInfo::State::DoDefCFAOffset(long offset) {
  Rule *cfa_rule = rules_.CFARule();
  if (!cfa_rule) {
    reporter_->NoCFARule(entry_->offset, entry_->kind, CursorOffset());
    return false;
  }
  cfa_rule->SetOffset(offset);
  return cfa_rule->Handle(handler_, address_, Handler::kCFARegister);
}

bool CallFrameInfo::State::DoRule(unsigned reg, Rule *rule) {
  rules_.SetRegisterRule(reg, rule);
  return rule->Handle(handler_, address_, reg);
}

bool CallFrameInfo::State::DoOffset(unsigned reg, long offset) {
  if (!rules_.CFARule()) {
    reporter_->NoCFARule(entry_->offset, entry_->kind, CursorOffset());
    return false;
  }
  return DoRule(reg,
                new OffsetRule(Handler::kCFARegister, offset));
}

bool CallFrameInfo::State::DoValOffset(unsigned reg, long offset) {
  if (!rules_.CFARule()) {
    reporter_->NoCFARule(entry_->offset, entry_->kind, CursorOffset());
    return false;
  }
  return DoRule(reg,
                new ValOffsetRule(Handler::kCFARegister, offset));
}

bool CallFrameInfo::State::DoRestore(unsigned reg) {
  // DW_CFA_restore and DW_CFA_restore_extended don't make sense in a CIE.
  if (entry_->kind == kCIE) {
    reporter_->RestoreInCIE(entry_->offset, CursorOffset());
    return false;
  }
  Rule *rule = cie_rules_.RegisterRule(reg);
  if (!rule) {
    // This isn't really the right thing to do, but since CFI generally
    // only mentions callee-saves registers, and GCC's convention for
    // callee-saves registers is that they are unchanged, it's a good
    // approximation.
    rule = new SameValueRule();
  }
  return DoRule(reg, rule);
}

bool CallFrameInfo::ReadEntryPrologue(const char *cursor, Entry *entry) {
  const char *buffer_end = buffer_ + buffer_length_;

  // Initialize enough of ENTRY for use in error reporting.
  entry->offset = cursor - buffer_;
  entry->start = cursor;
  entry->kind = kUnknown;
  entry->end = NULL;

  // Read the initial length. This sets reader_'s offset size.
  size_t length_size;
  uint64 length = reader_->ReadInitialLength(cursor, &length_size);
  if (length_size > size_t(buffer_end - cursor))
    return ReportIncomplete(entry);
  cursor += length_size;

  // In a .eh_frame section, a length of zero marks the end of the series
  // of entries.
  if (length == 0 && eh_frame_) {
    entry->kind = kTerminator;
    entry->end = cursor;
    return true;
  }

  // Validate the length.
  if (length > size_t(buffer_end - cursor))
    return ReportIncomplete(entry);

  // The length is the number of bytes after the initial length field;
  // we have that position handy at this point, so compute the end
  // now. (If we're parsing 64-bit-offset DWARF on a 32-bit machine,
  // and the length didn't fit in a size_t, we would have rejected it
  // above.)
  entry->end = cursor + length;

  // Parse the next field: either the offset of a CIE or a CIE id.
  size_t offset_size = reader_->OffsetSize();
  if (offset_size > size_t(entry->end - cursor)) return ReportIncomplete(entry);
  entry->id = reader_->ReadOffset(cursor);

  // Don't advance cursor past id field yet; in .eh_frame data we need
  // the id's position to compute the section offset of an FDE's CIE.

  // Now we can decide what kind of entry this is.
  if (eh_frame_) {
    // In .eh_frame data, an ID of zero marks the entry as a CIE, and
    // anything else is an offset from the id field of the FDE to the start
    // of the CIE.
    if (entry->id == 0) {
      entry->kind = kCIE;
    } else {
      entry->kind = kFDE;
      // Turn the offset from the id into an offset from the buffer's start.
      entry->id = (cursor - buffer_) - entry->id;
    }
  } else {
    // In DWARF CFI data, an ID of ~0 (of the appropriate width, given the
    // offset size for the entry) marks the entry as a CIE, and anything
    // else is the offset of the CIE from the beginning of the section.
    if (offset_size == 4)
      entry->kind = (entry->id == 0xffffffff) ? kCIE : kFDE;
    else {
      MOZ_ASSERT(offset_size == 8);
      entry->kind = (entry->id == 0xffffffffffffffffULL) ? kCIE : kFDE;
    }
  }

  // Now advance cursor past the id.
   cursor += offset_size;

  // The fields specific to this kind of entry start here.
  entry->fields = cursor;

  entry->cie = NULL;

  return true;
}

bool CallFrameInfo::ReadCIEFields(CIE *cie) {
  const char *cursor = cie->fields;
  size_t len;

  MOZ_ASSERT(cie->kind == kCIE);

  // Prepare for early exit.
  cie->version = 0;
  cie->augmentation.clear();
  cie->code_alignment_factor = 0;
  cie->data_alignment_factor = 0;
  cie->return_address_register = 0;
  cie->has_z_augmentation = false;
  cie->pointer_encoding = DW_EH_PE_absptr;
  cie->instructions = 0;

  // Parse the version number.
  if (cie->end - cursor < 1)
    return ReportIncomplete(cie);
  cie->version = reader_->ReadOneByte(cursor);
  cursor++;

  // If we don't recognize the version, we can't parse any more fields of the
  // CIE. For DWARF CFI, we handle versions 1 through 3 (there was never a
  // version 2 of CFI data). For .eh_frame, we handle versions 1 and 3 as well;
  // the difference between those versions seems to be the same as for
  // .debug_frame.
  if (cie->version < 1 || cie->version > 3) {
    reporter_->UnrecognizedVersion(cie->offset, cie->version);
    return false;
  }

  const char *augmentation_start = cursor;
  const void *augmentation_end =
      memchr(augmentation_start, '\0', cie->end - augmentation_start);
  if (! augmentation_end) return ReportIncomplete(cie);
  cursor = static_cast<const char *>(augmentation_end);
  cie->augmentation = string(augmentation_start,
                                  cursor - augmentation_start);
  // Skip the terminating '\0'.
  cursor++;

  // Is this CFI augmented?
  if (!cie->augmentation.empty()) {
    // Is it an augmentation we recognize?
    if (cie->augmentation[0] == DW_Z_augmentation_start) {
      // Linux C++ ABI 'z' augmentation, used for exception handling data.
      cie->has_z_augmentation = true;
    } else {
      // Not an augmentation we recognize. Augmentations can have arbitrary
      // effects on the form of rest of the content, so we have to give up.
      reporter_->UnrecognizedAugmentation(cie->offset, cie->augmentation);
      return false;
    }
  }

  // Parse the code alignment factor.
  cie->code_alignment_factor = reader_->ReadUnsignedLEB128(cursor, &len);
  if (size_t(cie->end - cursor) < len) return ReportIncomplete(cie);
  cursor += len;

  // Parse the data alignment factor.
  cie->data_alignment_factor = reader_->ReadSignedLEB128(cursor, &len);
  if (size_t(cie->end - cursor) < len) return ReportIncomplete(cie);
  cursor += len;

  // Parse the return address register. This is a ubyte in version 1, and
  // a ULEB128 in version 3.
  if (cie->version == 1) {
    if (cursor >= cie->end) return ReportIncomplete(cie);
    cie->return_address_register = uint8(*cursor++);
  } else {
    cie->return_address_register = reader_->ReadUnsignedLEB128(cursor, &len);
    if (size_t(cie->end - cursor) < len) return ReportIncomplete(cie);
    cursor += len;
  }

  // If we have a 'z' augmentation string, find the augmentation data and
  // use the augmentation string to parse it.
  if (cie->has_z_augmentation) {
    uint64_t data_size = reader_->ReadUnsignedLEB128(cursor, &len);
    if (size_t(cie->end - cursor) < len + data_size)
      return ReportIncomplete(cie);
    cursor += len;
    const char *data = cursor;
    cursor += data_size;
    const char *data_end = cursor;

    cie->has_z_lsda = false;
    cie->has_z_personality = false;
    cie->has_z_signal_frame = false;

    // Walk the augmentation string, and extract values from the
    // augmentation data as the string directs.
    for (size_t i = 1; i < cie->augmentation.size(); i++) {
      switch (cie->augmentation[i]) {
        case DW_Z_has_LSDA:
          // The CIE's augmentation data holds the language-specific data
          // area pointer's encoding, and the FDE's augmentation data holds
          // the pointer itself.
          cie->has_z_lsda = true;
          // Fetch the LSDA encoding from the augmentation data.
          if (data >= data_end) return ReportIncomplete(cie);
          cie->lsda_encoding = DwarfPointerEncoding(*data++);
          if (!reader_->ValidEncoding(cie->lsda_encoding)) {
            reporter_->InvalidPointerEncoding(cie->offset, cie->lsda_encoding);
            return false;
          }
          // Don't check if the encoding is usable here --- we haven't
          // read the FDE's fields yet, so we're not prepared for
          // DW_EH_PE_funcrel, although that's a fine encoding for the
          // LSDA to use, since it appears in the FDE.
          break;

        case DW_Z_has_personality_routine:
          // The CIE's augmentation data holds the personality routine
          // pointer's encoding, followed by the pointer itself.
          cie->has_z_personality = true;
          // Fetch the personality routine pointer's encoding from the
          // augmentation data.
          if (data >= data_end) return ReportIncomplete(cie);
          cie->personality_encoding = DwarfPointerEncoding(*data++);
          if (!reader_->ValidEncoding(cie->personality_encoding)) {
            reporter_->InvalidPointerEncoding(cie->offset,
                                              cie->personality_encoding);
            return false;
          }
          if (!reader_->UsableEncoding(cie->personality_encoding)) {
            reporter_->UnusablePointerEncoding(cie->offset,
                                               cie->personality_encoding);
            return false;
          }
          // Fetch the personality routine's pointer itself from the data.
          cie->personality_address =
            reader_->ReadEncodedPointer(data, cie->personality_encoding,
                                        &len);
          if (len > size_t(data_end - data))
            return ReportIncomplete(cie);
          data += len;
          break;

        case DW_Z_has_FDE_address_encoding:
          // The CIE's augmentation data holds the pointer encoding to use
          // for addresses in the FDE.
          if (data >= data_end) return ReportIncomplete(cie);
          cie->pointer_encoding = DwarfPointerEncoding(*data++);
          if (!reader_->ValidEncoding(cie->pointer_encoding)) {
            reporter_->InvalidPointerEncoding(cie->offset,
                                              cie->pointer_encoding);
            return false;
          }
          if (!reader_->UsableEncoding(cie->pointer_encoding)) {
            reporter_->UnusablePointerEncoding(cie->offset,
                                               cie->pointer_encoding);
            return false;
          }
          break;

        case DW_Z_is_signal_trampoline:
          // Frames using this CIE are signal delivery frames.
          cie->has_z_signal_frame = true;
          break;

        default:
          // An augmentation we don't recognize.
          reporter_->UnrecognizedAugmentation(cie->offset, cie->augmentation);
          return false;
      }
    }
  }

  // The CIE's instructions start here.
  cie->instructions = cursor;

  return true;
}

bool CallFrameInfo::ReadFDEFields(FDE *fde) {
  const char *cursor = fde->fields;
  size_t size;

  fde->address = reader_->ReadEncodedPointer(cursor, fde->cie->pointer_encoding,
                                             &size);
  if (size > size_t(fde->end - cursor))
    return ReportIncomplete(fde);
  cursor += size;
  reader_->SetFunctionBase(fde->address);

  // For the length, we strip off the upper nybble of the encoding used for
  // the starting address.
  DwarfPointerEncoding length_encoding =
    DwarfPointerEncoding(fde->cie->pointer_encoding & 0x0f);
  fde->size = reader_->ReadEncodedPointer(cursor, length_encoding, &size);
  if (size > size_t(fde->end - cursor))
    return ReportIncomplete(fde);
  cursor += size;

  // If the CIE has a 'z' augmentation string, then augmentation data
  // appears here.
  if (fde->cie->has_z_augmentation) {
    uint64_t data_size = reader_->ReadUnsignedLEB128(cursor, &size);
    if (size_t(fde->end - cursor) < size + data_size)
      return ReportIncomplete(fde);
    cursor += size;

    // In the abstract, we should walk the augmentation string, and extract
    // items from the FDE's augmentation data as we encounter augmentation
    // string characters that specify their presence: the ordering of items
    // in the augmentation string determines the arrangement of values in
    // the augmentation data.
    //
    // In practice, there's only ever one value in FDE augmentation data
    // that we support --- the LSDA pointer --- and we have to bail if we
    // see any unrecognized augmentation string characters. So if there is
    // anything here at all, we know what it is, and where it starts.
    if (fde->cie->has_z_lsda) {
      // Check whether the LSDA's pointer encoding is usable now: only once
      // we've parsed the FDE's starting address do we call reader_->
      // SetFunctionBase, so that the DW_EH_PE_funcrel encoding becomes
      // usable.
      if (!reader_->UsableEncoding(fde->cie->lsda_encoding)) {
        reporter_->UnusablePointerEncoding(fde->cie->offset,
                                           fde->cie->lsda_encoding);
        return false;
      }

      fde->lsda_address =
        reader_->ReadEncodedPointer(cursor, fde->cie->lsda_encoding, &size);
      if (size > data_size)
        return ReportIncomplete(fde);
      // Ideally, we would also complain here if there were unconsumed
      // augmentation data.
    }

    cursor += data_size;
  }

  // The FDE's instructions start after those.
  fde->instructions = cursor;

  return true;
}

bool CallFrameInfo::Start() {
  const char *buffer_end = buffer_ + buffer_length_;
  const char *cursor;
  bool all_ok = true;
  const char *entry_end;
  bool ok;

  // Traverse all the entries in buffer_, skipping CIEs and offering
  // FDEs to the handler.
  for (cursor = buffer_; cursor < buffer_end;
       cursor = entry_end, all_ok = all_ok && ok) {
    FDE fde;

    // Make it easy to skip this entry with 'continue': assume that
    // things are not okay until we've checked all the data, and
    // prepare the address of the next entry.
    ok = false;

    // Read the entry's prologue.
    if (!ReadEntryPrologue(cursor, &fde)) {
      if (!fde.end) {
        // If we couldn't even figure out this entry's extent, then we
        // must stop processing entries altogether.
        all_ok = false;
        break;
      }
      entry_end = fde.end;
      continue;
    }

    // The next iteration picks up after this entry.
    entry_end = fde.end;

    // Did we see an .eh_frame terminating mark?
    if (fde.kind == kTerminator) {
      // If there appears to be more data left in the section after the
      // terminating mark, warn the user. But this is just a warning;
      // we leave all_ok true.
      if (fde.end < buffer_end) reporter_->EarlyEHTerminator(fde.offset);
      break;
    }

    // In this loop, we skip CIEs. We only parse them fully when we
    // parse an FDE that refers to them. This limits our memory
    // consumption (beyond the buffer itself) to that needed to
    // process the largest single entry.
    if (fde.kind != kFDE) {
      ok = true;
      continue;
    }

    // Validate the CIE pointer.
    if (fde.id > buffer_length_) {
      reporter_->CIEPointerOutOfRange(fde.offset, fde.id);
      continue;
    }

    CIE cie;

    // Parse this FDE's CIE header.
    if (!ReadEntryPrologue(buffer_ + fde.id, &cie))
      continue;
    // This had better be an actual CIE.
    if (cie.kind != kCIE) {
      reporter_->BadCIEId(fde.offset, fde.id);
      continue;
    }
    if (!ReadCIEFields(&cie))
      continue;

    // We now have the values that govern both the CIE and the FDE.
    cie.cie = &cie;
    fde.cie = &cie;

    // Parse the FDE's header.
    if (!ReadFDEFields(&fde))
      continue;

    // Call Entry to ask the consumer if they're interested.
    if (!handler_->Entry(fde.offset, fde.address, fde.size,
                         cie.version, cie.augmentation,
                         cie.return_address_register)) {
      // The handler isn't interested in this entry. That's not an error.
      ok = true;
      continue;
    }

    if (cie.has_z_augmentation) {
      // Report the personality routine address, if we have one.
      if (cie.has_z_personality) {
        if (!handler_
            ->PersonalityRoutine(cie.personality_address,
                                 IsIndirectEncoding(cie.personality_encoding)))
          continue;
      }

      // Report the language-specific data area address, if we have one.
      if (cie.has_z_lsda) {
        if (!handler_
            ->LanguageSpecificDataArea(fde.lsda_address,
                                       IsIndirectEncoding(cie.lsda_encoding)))
          continue;
      }

      // If this is a signal-handling frame, report that.
      if (cie.has_z_signal_frame) {
        if (!handler_->SignalHandler())
          continue;
      }
    }

    // Interpret the CIE's instructions, and then the FDE's instructions.
    State state(reader_, handler_, reporter_, fde.address);
    ok = state.InterpretCIE(cie) && state.InterpretFDE(fde);

    // Tell the ByteReader that the function start address from the
    // FDE header is no longer valid.
    reader_->ClearFunctionBase();

    // Report the end of the entry.
    handler_->End();
  }

  return all_ok;
}

const char *CallFrameInfo::KindName(EntryKind kind) {
  if (kind == CallFrameInfo::kUnknown)
    return "entry";
  else if (kind == CallFrameInfo::kCIE)
    return "common information entry";
  else if (kind == CallFrameInfo::kFDE)
    return "frame description entry";
  else {
    MOZ_ASSERT (kind == CallFrameInfo::kTerminator);
    return ".eh_frame sequence terminator";
  }
}

bool CallFrameInfo::ReportIncomplete(Entry *entry) {
  reporter_->Incomplete(entry->offset, entry->kind);
  return false;
}

void CallFrameInfo::Reporter::Incomplete(uint64 offset,
                                         CallFrameInfo::EntryKind kind) {
  char buf[300];
  SprintfLiteral(buf,
                 "%s: CFI %s at offset 0x%llx in '%s': entry ends early\n",
                 filename_.c_str(), CallFrameInfo::KindName(kind), offset,
                 section_.c_str());
  log_(buf);
}

void CallFrameInfo::Reporter::EarlyEHTerminator(uint64 offset) {
  char buf[300];
  SprintfLiteral(buf,
                 "%s: CFI at offset 0x%llx in '%s': saw end-of-data marker"
                 " before end of section contents\n",
                 filename_.c_str(), offset, section_.c_str());
  log_(buf);
}

void CallFrameInfo::Reporter::CIEPointerOutOfRange(uint64 offset,
                                                   uint64 cie_offset) {
  char buf[300];
  SprintfLiteral(buf,
                 "%s: CFI frame description entry at offset 0x%llx in '%s':"
                 " CIE pointer is out of range: 0x%llx\n",
                 filename_.c_str(), offset, section_.c_str(), cie_offset);
  log_(buf);
}

void CallFrameInfo::Reporter::BadCIEId(uint64 offset, uint64 cie_offset) {
  char buf[300];
  SprintfLiteral(buf,
                 "%s: CFI frame description entry at offset 0x%llx in '%s':"
                 " CIE pointer does not point to a CIE: 0x%llx\n",
                 filename_.c_str(), offset, section_.c_str(), cie_offset);
  log_(buf);
}

void CallFrameInfo::Reporter::UnrecognizedVersion(uint64 offset, int version) {
  char buf[300];
  SprintfLiteral(buf,
                 "%s: CFI frame description entry at offset 0x%llx in '%s':"
                 " CIE specifies unrecognized version: %d\n",
                 filename_.c_str(), offset, section_.c_str(), version);
  log_(buf);
}

void CallFrameInfo::Reporter::UnrecognizedAugmentation(uint64 offset,
                                                       const string &aug) {
  char buf[300];
  SprintfLiteral(buf,
                 "%s: CFI frame description entry at offset 0x%llx in '%s':"
                 " CIE specifies unrecognized augmentation: '%s'\n",
                 filename_.c_str(), offset, section_.c_str(), aug.c_str());
  log_(buf);
}

void CallFrameInfo::Reporter::InvalidPointerEncoding(uint64 offset,
                                                     uint8 encoding) {
  char buf[300];
  SprintfLiteral(buf,
                 "%s: CFI common information entry at offset 0x%llx in '%s':"
                 " 'z' augmentation specifies invalid pointer encoding: "
                 "0x%02x\n",
                 filename_.c_str(), offset, section_.c_str(), encoding);
  log_(buf);
}

void CallFrameInfo::Reporter::UnusablePointerEncoding(uint64 offset,
                                                      uint8 encoding) {
  char buf[300];
  SprintfLiteral(buf,
                 "%s: CFI common information entry at offset 0x%llx in '%s':"
                 " 'z' augmentation specifies a pointer encoding for which"
                 " we have no base address: 0x%02x\n",
                 filename_.c_str(), offset, section_.c_str(), encoding);
  log_(buf);
}

void CallFrameInfo::Reporter::RestoreInCIE(uint64 offset, uint64 insn_offset) {
  char buf[300];
  SprintfLiteral(buf,
                 "%s: CFI common information entry at offset 0x%llx in '%s':"
                 " the DW_CFA_restore instruction at offset 0x%llx"
                 " cannot be used in a common information entry\n",
                 filename_.c_str(), offset, section_.c_str(), insn_offset);
  log_(buf);
}

void CallFrameInfo::Reporter::BadInstruction(uint64 offset,
                                             CallFrameInfo::EntryKind kind,
                                             uint64 insn_offset) {
  char buf[300];
  SprintfLiteral(buf,
                 "%s: CFI %s at offset 0x%llx in section '%s':"
                 " the instruction at offset 0x%llx is unrecognized\n",
                 filename_.c_str(), CallFrameInfo::KindName(kind),
                 offset, section_.c_str(), insn_offset);
  log_(buf);
}

void CallFrameInfo::Reporter::NoCFARule(uint64 offset,
                                        CallFrameInfo::EntryKind kind,
                                        uint64 insn_offset) {
  char buf[300];
  SprintfLiteral(buf,
                 "%s: CFI %s at offset 0x%llx in section '%s':"
                 " the instruction at offset 0x%llx assumes that a CFA rule "
                 "has been set, but none has been set\n",
                 filename_.c_str(), CallFrameInfo::KindName(kind), offset,
                 section_.c_str(), insn_offset);
  log_(buf);
}

void CallFrameInfo::Reporter::EmptyStateStack(uint64 offset,
                                              CallFrameInfo::EntryKind kind,
                                              uint64 insn_offset) {
  char buf[300];
  SprintfLiteral(buf,
                 "%s: CFI %s at offset 0x%llx in section '%s':"
                 " the DW_CFA_restore_state instruction at offset 0x%llx"
                 " should pop a saved state from the stack, but the stack "
                 "is empty\n",
                 filename_.c_str(), CallFrameInfo::KindName(kind), offset,
                 section_.c_str(), insn_offset);
  log_(buf);
}

void CallFrameInfo::Reporter::ClearingCFARule(uint64 offset,
                                              CallFrameInfo::EntryKind kind,
                                              uint64 insn_offset) {
  char buf[300];
  SprintfLiteral(buf,
                 "%s: CFI %s at offset 0x%llx in section '%s':"
                 " the DW_CFA_restore_state instruction at offset 0x%llx"
                 " would clear the CFA rule in effect\n",
                 filename_.c_str(), CallFrameInfo::KindName(kind), offset,
                 section_.c_str(), insn_offset);
  log_(buf);
}


unsigned int DwarfCFIToModule::RegisterNames::I386() {
  /*
   8 "$eax", "$ecx", "$edx", "$ebx", "$esp", "$ebp", "$esi", "$edi",
   3 "$eip", "$eflags", "$unused1",
   8 "$st0", "$st1", "$st2", "$st3", "$st4", "$st5", "$st6", "$st7",
   2 "$unused2", "$unused3",
   8 "$xmm0", "$xmm1", "$xmm2", "$xmm3", "$xmm4", "$xmm5", "$xmm6", "$xmm7",
   8 "$mm0", "$mm1", "$mm2", "$mm3", "$mm4", "$mm5", "$mm6", "$mm7",
   3 "$fcw", "$fsw", "$mxcsr",
   8 "$es", "$cs", "$ss", "$ds", "$fs", "$gs", "$unused4", "$unused5",
   2 "$tr", "$ldtr"
  */
  return 8 + 3 + 8 + 2 + 8 + 8 + 3 + 8 + 2;
}

unsigned int DwarfCFIToModule::RegisterNames::X86_64() {
  /*
   8 "$rax", "$rdx", "$rcx", "$rbx", "$rsi", "$rdi", "$rbp", "$rsp",
   8 "$r8",  "$r9",  "$r10", "$r11", "$r12", "$r13", "$r14", "$r15",
   1 "$rip",
   8 "$xmm0","$xmm1","$xmm2", "$xmm3", "$xmm4", "$xmm5", "$xmm6", "$xmm7",
   8 "$xmm8","$xmm9","$xmm10","$xmm11","$xmm12","$xmm13","$xmm14","$xmm15",
   8 "$st0", "$st1", "$st2", "$st3", "$st4", "$st5", "$st6", "$st7",
   8 "$mm0", "$mm1", "$mm2", "$mm3", "$mm4", "$mm5", "$mm6", "$mm7",
   1 "$rflags",
   8 "$es", "$cs", "$ss", "$ds", "$fs", "$gs", "$unused1", "$unused2",
   4 "$fs.base", "$gs.base", "$unused3", "$unused4",
   2 "$tr", "$ldtr",
   3 "$mxcsr", "$fcw", "$fsw"
  */
  return 8 + 8 + 1 + 8 + 8 + 8 + 8 + 1 + 8 + 4 + 2 + 3;
}

// Per ARM IHI 0040A, section 3.1
unsigned int DwarfCFIToModule::RegisterNames::ARM() {
  /*
   8 "r0",  "r1",  "r2",  "r3",  "r4",  "r5",  "r6",  "r7",
   8 "r8",  "r9",  "r10", "r11", "r12", "sp",  "lr",  "pc",
   8 "f0",  "f1",  "f2",  "f3",  "f4",  "f5",  "f6",  "f7",
   8 "fps", "cpsr", "",   "",    "",    "",    "",    "",
   8 "",    "",    "",    "",    "",    "",    "",    "",
   8 "",    "",    "",    "",    "",    "",    "",    "",
   8 "",    "",    "",    "",    "",    "",    "",    "",
   8 "",    "",    "",    "",    "",    "",    "",    "",
   8 "s0",  "s1",  "s2",  "s3",  "s4",  "s5",  "s6",  "s7",
   8 "s8",  "s9",  "s10", "s11", "s12", "s13", "s14", "s15",
   8 "s16", "s17", "s18", "s19", "s20", "s21", "s22", "s23",
   8 "s24", "s25", "s26", "s27", "s28", "s29", "s30", "s31",
   8 "f0",  "f1",  "f2",  "f3",  "f4",  "f5",  "f6",  "f7"
  */
  return 13 * 8;
}

// See prototype for comments.
int32_t parseDwarfExpr(Summariser* summ, const ByteReader* reader,
                       string expr, bool debug,
                       bool pushCfaAtStart, bool derefAtEnd)
{
  const char* cursor = expr.c_str();
  const char* end1   = cursor + expr.length();

  char buf[100];
  if (debug) {
    SprintfLiteral(buf, "LUL.DW  << DwarfExpr, len is %d\n",
                   (int)(end1 - cursor));
    summ->Log(buf);
  }
  
  // Add a marker for the start of this expression.  In it, indicate
  // whether or not the CFA should be pushed onto the stack prior to
  // evaluation.
  int32_t start_ix
    = summ->AddPfxInstr(PfxInstr(PX_Start, pushCfaAtStart ? 1 : 0));
  MOZ_ASSERT(start_ix >= 0);

  while (cursor < end1) {

    uint8 opc = reader->ReadOneByte(cursor);
    cursor++;

    const char* nm   = nullptr;
    PfxExprOp   pxop = PX_End;
    
    switch (opc) {

      case DW_OP_lit0 ... DW_OP_lit31: {
        int32_t simm32 = (int32_t)(opc - DW_OP_lit0);
        if (debug) {
          SprintfLiteral(buf, "LUL.DW   DW_OP_lit%d\n", (int)simm32);
          summ->Log(buf);
        }
        (void) summ->AddPfxInstr(PfxInstr(PX_SImm32, simm32));
        break;
      }

      case DW_OP_breg0 ... DW_OP_breg31: {
        size_t len;
        int64_t n = reader->ReadSignedLEB128(cursor, &len);
        cursor += len;
        DW_REG_NUMBER reg = (DW_REG_NUMBER)(opc - DW_OP_breg0);
        if (debug) {
          SprintfLiteral(buf, "LUL.DW   DW_OP_breg%d %lld\n",
                         (int)reg, (long long int)n);
          summ->Log(buf);
        }
        // PfxInstr only allows a 32 bit signed offset.  So we
        // must fail if the immediate is out of range.
        if (n < INT32_MIN || INT32_MAX < n)
          goto fail;
        (void) summ->AddPfxInstr(PfxInstr(PX_DwReg, reg));
        (void) summ->AddPfxInstr(PfxInstr(PX_SImm32, (int32_t)n));
        (void) summ->AddPfxInstr(PfxInstr(PX_Add));
        break;
      }

      case DW_OP_const4s: {
        uint64_t u64 = reader->ReadFourBytes(cursor);
        cursor += 4;
        // u64 is guaranteed by |ReadFourBytes| to be in the
        // range 0 .. FFFFFFFF inclusive.  But to be safe:
        uint32_t u32 = (uint32_t)(u64 & 0xFFFFFFFF);
        int32_t  s32 = (int32_t)u32;
        if (debug) {
          SprintfLiteral(buf, "LUL.DW   DW_OP_const4s %d\n", (int)s32);
          summ->Log(buf);
        }
        (void) summ->AddPfxInstr(PfxInstr(PX_SImm32, s32));
        break;
      }
      
      case DW_OP_deref: nm = "deref"; pxop = PX_Deref;  goto no_operands;
      case DW_OP_and:   nm = "and";   pxop = PX_And;    goto no_operands;
      case DW_OP_plus:  nm = "plus";  pxop = PX_Add;    goto no_operands;
      case DW_OP_minus: nm = "minus"; pxop = PX_Sub;    goto no_operands;
      case DW_OP_shl:   nm = "shl";   pxop = PX_Shl;    goto no_operands;
      case DW_OP_ge:    nm = "ge";    pxop = PX_CmpGES; goto no_operands;
      no_operands:
        MOZ_ASSERT(nm && pxop != PX_End);
        if (debug) {
          SprintfLiteral(buf, "LUL.DW   DW_OP_%s\n", nm);
          summ->Log(buf);
        }
        (void) summ->AddPfxInstr(PfxInstr(pxop));
        break;

      default:
        if (debug) {
          SprintfLiteral(buf, "LUL.DW   unknown opc %d\n", (int)opc);
          summ->Log(buf);
        }
        goto fail;

    } // switch (opc)

  } // while (cursor < end1)
  
  MOZ_ASSERT(cursor >= end1);
  
  if (cursor > end1) {
    // We overran the Dwarf expression.  Give up.
    goto fail;
  }

  // For DW_CFA_expression, what the expression denotes is the address
  // of where the previous value is located.  The caller of this routine
  // may therefore request one last dereference before the end marker is
  // inserted.
  if (derefAtEnd) {
    (void) summ->AddPfxInstr(PfxInstr(PX_Deref));
  }

  // Insert an end marker, and declare success.
  (void) summ->AddPfxInstr(PfxInstr(PX_End));
  if (debug) {
    SprintfLiteral(buf, "LUL.DW   conversion of dwarf expression succeeded, "
                        "ix = %d\n", (int)start_ix);
    summ->Log(buf);
    summ->Log("LUL.DW  >>\n");
  }
  return start_ix;
      
 fail:
  if (debug) {
    summ->Log("LUL.DW   conversion of dwarf expression failed\n");
    summ->Log("LUL.DW  >>\n");
  }
  return -1;
}


bool DwarfCFIToModule::Entry(size_t offset, uint64 address, uint64 length,
                             uint8 version, const string &augmentation,
                             unsigned return_address) {
  if (DEBUG_DWARF) {
    char buf[100];
    SprintfLiteral(buf, "LUL.DW DwarfCFIToModule::Entry 0x%llx,+%lld\n",
                   address, length);
    summ_->Log(buf);
  }
  
  summ_->Entry(address, length);

  // If dwarf2reader::CallFrameInfo can handle this version and
  // augmentation, then we should be okay with that, so there's no
  // need to check them here.

  // Get ready to collect entries.
  return_address_ = return_address;

  // Breakpad STACK CFI records must provide a .ra rule, but DWARF CFI
  // may not establish any rule for .ra if the return address column
  // is an ordinary register, and that register holds the return
  // address on entry to the function. So establish an initial .ra
  // rule citing the return address register.
  if (return_address_ < num_dw_regs_) {
    summ_->Rule(address, return_address_, NODEREF, return_address, 0);
  }

  return true;
}

const UniqueString* DwarfCFIToModule::RegisterName(int i) {
  if (i < 0) {
    MOZ_ASSERT(i == kCFARegister);
    return usu_->ToUniqueString(".cfa");
  }
  unsigned reg = i;
  if (reg == return_address_)
    return usu_->ToUniqueString(".ra");

  char buf[30];
  SprintfLiteral(buf, "dwarf_reg_%u", reg);
  return usu_->ToUniqueString(buf);
}

bool DwarfCFIToModule::UndefinedRule(uint64 address, int reg) {
  reporter_->UndefinedNotSupported(entry_offset_, RegisterName(reg));
  // Treat this as a non-fatal error.
  return true;
}

bool DwarfCFIToModule::SameValueRule(uint64 address, int reg) {
  if (DEBUG_DWARF) {
    char buf[100];
    SprintfLiteral(buf, "LUL.DW  0x%llx: old r%d = Same\n", address, reg);
    summ_->Log(buf);
  }
  // reg + 0
  summ_->Rule(address, reg, NODEREF, reg, 0);
  return true;
}

bool DwarfCFIToModule::OffsetRule(uint64 address, int reg,
                                  int base_register, long offset) {
  if (DEBUG_DWARF) {
    char buf[100];
    SprintfLiteral(buf, "LUL.DW  0x%llx: old r%d = *(r%d + %ld)\n",
                   address, reg, base_register, offset);
    summ_->Log(buf);
  }
  // *(base_register + offset)
  summ_->Rule(address, reg, DEREF, base_register, offset);
  return true;
}

bool DwarfCFIToModule::ValOffsetRule(uint64 address, int reg,
                                     int base_register, long offset) {
  if (DEBUG_DWARF) {
    char buf[100];
    SprintfLiteral(buf, "LUL.DW  0x%llx: old r%d = r%d + %ld\n",
                   address, reg, base_register, offset);
    summ_->Log(buf);
  }
  // base_register + offset
  summ_->Rule(address, reg, NODEREF, base_register, offset);
  return true;
}

bool DwarfCFIToModule::RegisterRule(uint64 address, int reg,
                                    int base_register) {
  if (DEBUG_DWARF) {
    char buf[100];
    SprintfLiteral(buf, "LUL.DW  0x%llx: old r%d = r%d\n",
                   address, reg, base_register);
    summ_->Log(buf);
  }
  // base_register + 0
  summ_->Rule(address, reg, NODEREF, base_register, 0);
  return true;
}

bool DwarfCFIToModule::ExpressionRule(uint64 address, int reg,
                                      const string &expression)
{
  bool debug = !!DEBUG_DWARF;
  int32_t start_ix = parseDwarfExpr(summ_, reader_, expression, debug,
                                    true/*pushCfaAtStart*/,
                                    true/*derefAtEnd*/);
  if (start_ix >= 0) {
    summ_->Rule(address, reg, PFXEXPR, 0, start_ix);
  } else {
    // Parsing of the Dwarf expression failed.  Treat this as a
    // non-fatal error, hence return |true| even on this path.
    reporter_->ExpressionCouldNotBeSummarised(entry_offset_, RegisterName(reg));
  }
  return true;
}

bool DwarfCFIToModule::ValExpressionRule(uint64 address, int reg,
                                         const string &expression)
{
  bool debug = !!DEBUG_DWARF;
  int32_t start_ix = parseDwarfExpr(summ_, reader_, expression, debug,
                                    true/*pushCfaAtStart*/,
                                    false/*!derefAtEnd*/);
  if (start_ix >= 0) {
    summ_->Rule(address, reg, PFXEXPR, 0, start_ix);
  } else {
    // Parsing of the Dwarf expression failed.  Treat this as a
    // non-fatal error, hence return |true| even on this path.
    reporter_->ExpressionCouldNotBeSummarised(entry_offset_, RegisterName(reg));
  }
  return true;
}

bool DwarfCFIToModule::End() {
  //module_->AddStackFrameEntry(entry_);
  if (DEBUG_DWARF) {
    summ_->Log("LUL.DW DwarfCFIToModule::End()\n");
  }
  summ_->End();
  return true;
}

void DwarfCFIToModule::Reporter::UndefinedNotSupported(
    size_t offset,
    const UniqueString* reg) {
  char buf[300];
  SprintfLiteral(buf, "DwarfCFIToModule::Reporter::UndefinedNotSupported()\n");
  log_(buf);
  //BPLOG(INFO) << file_ << ", section '" << section_
  //  << "': the call frame entry at offset 0x"
  //  << std::setbase(16) << offset << std::setbase(10)
  //  << " sets the rule for register '" << FromUniqueString(reg)
  //  << "' to 'undefined', but the Breakpad symbol file format cannot "
  //  << " express this";
}

// FIXME: move this somewhere sensible
static bool is_power_of_2(uint64_t n)
{
  int i, nSetBits = 0;
  for (i = 0; i < 8*(int)sizeof(n); i++) {
    if ((n & ((uint64_t)1) << i) != 0)
      nSetBits++;
  }
  return nSetBits <= 1;
}

void DwarfCFIToModule::Reporter::ExpressionCouldNotBeSummarised(
    size_t offset,
    const UniqueString* reg) {
  static uint64_t n_complaints = 0; // This isn't threadsafe
  n_complaints++;
  if (!is_power_of_2(n_complaints))
    return;
  char buf[300];
  SprintfLiteral(buf,
                 "DwarfCFIToModule::Reporter::"
                 "ExpressionCouldNotBeSummarised(shown %llu times)\n",
                 (unsigned long long int)n_complaints);
  log_(buf);
}

} // namespace lul
