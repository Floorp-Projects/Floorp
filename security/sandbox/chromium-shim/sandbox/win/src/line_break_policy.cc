/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "sandbox/win/src/line_break_policy.h"

#include <algorithm>
#include <array>
#include <usp10.h>

#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/line_break_common.h"
#include "sandbox/win/src/policy_engine_opcodes.h"
#include "sandbox/win/src/policy_params.h"

namespace sandbox {

bool LineBreakPolicy::GenerateRules(const wchar_t* null,
                                    TargetPolicy::Semantics semantics,
                                    LowLevelPolicy* policy) {
  if (TargetPolicy::LINE_BREAK_ALLOW != semantics) {
    return false;
  }

  PolicyRule line_break_rule(ASK_BROKER);
  if (!policy->AddRule(IpcTag::GETCOMPLEXLINEBREAKS, &line_break_rule)) {
    return false;
  }
  return true;
}

/* static */ DWORD LineBreakPolicy::GetComplexLineBreaksProxyAction(
    EvalResult eval_result, const wchar_t* text, uint32_t length,
    uint8_t* break_before) {
  // The only action supported is ASK_BROKER which means call the line breaker.
  if (ASK_BROKER != eval_result) {
    return ERROR_ACCESS_DENIED;
  }

  int outItems = 0;
  std::array<SCRIPT_ITEM, kMaxBrokeredLen + 1> items;
  HRESULT result = ::ScriptItemize(text, length, kMaxBrokeredLen, nullptr,
                                   nullptr, items.data(), &outItems);
  if (result != 0) {
    return ERROR_ACCESS_DENIED;
  }

  std::array<SCRIPT_LOGATTR, kMaxBrokeredLen> slas;
  for (int iItem = 0; iItem < outItems; ++iItem) {
    uint32_t endOffset = items[iItem + 1].iCharPos;
    uint32_t startOffset = items[iItem].iCharPos;
    if (FAILED(::ScriptBreak(text + startOffset, endOffset - startOffset,
                             &items[iItem].a, &slas[startOffset]))) {
      return ERROR_ACCESS_DENIED;
    }
  }

  std::transform(slas.data(), slas.data() + length, break_before,
                 [](const SCRIPT_LOGATTR& sla) { return sla.fSoftBreak; });

  return ERROR_SUCCESS;
}

}  // namespace sandbox
