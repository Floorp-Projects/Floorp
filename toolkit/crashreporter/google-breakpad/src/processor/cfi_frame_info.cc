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

// cfi_frame_info.cc: Implementation of CFIFrameInfo class.
// See cfi_frame_info.h for details.

#include "processor/cfi_frame_info.h"

#include <string.h>

#include <algorithm>
#include <sstream>

#include "common/scoped_ptr.h"
#include "processor/postfix_evaluator-inl.h"

namespace google_breakpad {

#ifdef _WIN32
#define strtok_r strtok_s
#endif

template<typename V>
bool CFIFrameInfo::FindCallerRegs(const RegisterValueMap<V> &registers,
                                  const MemoryRegion &memory,
                                  RegisterValueMap<V> *caller_registers) const {
  // If there are not rules for both .ra and .cfa in effect at this address,
  // don't use this CFI data for stack walking.
  if (cfa_rule_.isExprInvalid() || ra_rule_.isExprInvalid())
    return false;

  RegisterValueMap<V> working;
  PostfixEvaluator<V> evaluator(&working, &memory);

  caller_registers->clear();

  // First, compute the CFA.
  V cfa;
  working = registers;
  if (!evaluator.EvaluateForValue(cfa_rule_, &cfa))
    return false;

  // Then, compute the return address.
  V ra;
  working = registers;
  working.set(ustr__ZDcfa(), cfa);
  if (!evaluator.EvaluateForValue(ra_rule_, &ra))
    return false;

  // Now, compute values for all the registers register_rules_ mentions.
  for (RuleMap::const_iterator it = register_rules_.begin();
       it != register_rules_.end(); it++) {
    V value;
    working = registers;
    working.set(ustr__ZDcfa(), cfa);
    if (!evaluator.EvaluateForValue(it->second, &value))
      return false;
    caller_registers->set(it->first, value);
  }

  caller_registers->set(ustr__ZDra(), ra);
  caller_registers->set(ustr__ZDcfa(), cfa);

  return true;
}

// Explicit instantiations for 32-bit and 64-bit architectures.
template bool CFIFrameInfo::FindCallerRegs<uint32_t>(
    const RegisterValueMap<uint32_t> &registers,
    const MemoryRegion &memory,
    RegisterValueMap<uint32_t> *caller_registers) const;
template bool CFIFrameInfo::FindCallerRegs<uint64_t>(
    const RegisterValueMap<uint64_t> &registers,
    const MemoryRegion &memory,
    RegisterValueMap<uint64_t> *caller_registers) const;

string CFIFrameInfo::Serialize() const {
  std::ostringstream stream;

  if (!cfa_rule_.isExprInvalid()) {
    stream << ".cfa: " << cfa_rule_;
  }
  if (!ra_rule_.isExprInvalid()) {
    if (static_cast<std::streamoff>(stream.tellp()) != 0)
      stream << " ";
    stream << ".ra: " << ra_rule_;
  }

  // Visit the register rules in alphabetical order.  Because
  // register_rules_ has the elements in some arbitrary order,
  // get the names out into a vector, sort them, and visit in
  // sorted order.
  std::vector<const UniqueString*> rr_names;
  for (RuleMap::const_iterator iter = register_rules_.begin();
       iter != register_rules_.end();
       ++iter) {
    rr_names.push_back(iter->first);
  }

  std::sort(rr_names.begin(), rr_names.end(), LessThan_UniqueString);

  // Now visit the register rules in alphabetical order.
  for (std::vector<const UniqueString*>::const_iterator name = rr_names.begin();
       name != rr_names.end();
       ++name) {
    const UniqueString* nm = *name;
    Module::Expr rule = register_rules_.find(nm)->second;
    if (static_cast<std::streamoff>(stream.tellp()) != 0)
      stream << " ";
    stream << FromUniqueString(nm) << ": " << rule;
  }

  return stream.str();
}

bool CFIRuleParser::Parse(const string &rule_set) {
  size_t rule_set_len = rule_set.size();
  scoped_array<char> working_copy(new char[rule_set_len + 1]);
  memcpy(working_copy.get(), rule_set.data(), rule_set_len);
  working_copy[rule_set_len] = '\0';

  name_ = ustr__empty();
  expression_.clear();

  char *cursor;
  static const char token_breaks[] = " \t\r\n";
  char *token = strtok_r(working_copy.get(), token_breaks, &cursor);

  for (;;) {
    // End of rule set?
    if (!token) return Report();

    // Register/pseudoregister name?
    size_t token_len = strlen(token);
    if (token_len >= 1 && token[token_len - 1] == ':') {
      // Names can't be empty.
      if (token_len < 2) return false;
      // If there is any pending content, report it.
      if (name_ != ustr__empty() || !expression_.empty()) {
        if (!Report()) return false;
      }
      name_ = ToUniqueString_n(token, token_len - 1);
      expression_.clear();
    } else {
      // Another expression component.
      assert(token_len > 0); // strtok_r guarantees this, I think.
      if (!expression_.empty())
        expression_ += ' ';
      expression_ += token;
    }
    token = strtok_r(NULL, token_breaks, &cursor);
  }
}

bool CFIRuleParser::Report() {
  if (name_ == ustr__empty() || expression_.empty()) return false;
  if (name_ == ustr__ZDcfa()) handler_->CFARule(expression_);
  else if (name_ == ustr__ZDra()) handler_->RARule(expression_);
  else handler_->RegisterRule(name_, expression_);
  return true;
}

void CFIFrameInfoParseHandler::CFARule(const string &expression) {
  // 'expression' is a postfix expression string.
  frame_info_->SetCFARule(Module::Expr(expression));
}

void CFIFrameInfoParseHandler::RARule(const string &expression) {
  frame_info_->SetRARule(Module::Expr(expression));
}

void CFIFrameInfoParseHandler::RegisterRule(const UniqueString* name,
                                            const string &expression) {
  frame_info_->SetRegisterRule(name, Module::Expr(expression));
}

} // namespace google_breakpad
