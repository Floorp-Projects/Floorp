/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SANDBOX_SRC_LINE_BREAK_POLICY_H_
#define SANDBOX_SRC_LINE_BREAK_POLICY_H_

#include "base/win/windows_types.h"
#include "sandbox/win/src/policy_low_level.h"
#include "sandbox/win/src/sandbox_policy.h"

namespace sandbox {

enum EvalResult;

class LineBreakPolicy {
 public:
  // Creates the required low-level policy rules to evaluate a high-level
  // policy rule for complex line breaks.
  static bool GenerateRules(const wchar_t* type_name,
                            TargetPolicy::Semantics semantics,
                            LowLevelPolicy* policy);

  // Processes a TargetServices::GetComplexLineBreaks() request from the target.
  static DWORD GetComplexLineBreaksProxyAction(EvalResult eval_result,
                                               const wchar_t* text,
                                               uint32_t length,
                                               uint8_t* break_before);
};

}  // namespace sandbox

#endif  // SANDBOX_SRC_LINE_BREAK_POLICY_H_
