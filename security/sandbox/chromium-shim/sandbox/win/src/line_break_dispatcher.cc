/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "sandbox/win/src/line_break_dispatcher.h"

#include "sandbox/win/src/line_break_common.h"
#include "sandbox/win/src/line_break_policy.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/policy_params.h"

namespace sandbox {

LineBreakDispatcher::LineBreakDispatcher(PolicyBase* policy_base)
    : policy_base_(policy_base) {
  static const IPCCall get_complex_line_breaks = {
      {IpcTag::GETCOMPLEXLINEBREAKS, {INPTR_TYPE, UINT32_TYPE, INOUTPTR_TYPE}},
      reinterpret_cast<CallbackGeneric>(
          &LineBreakDispatcher::GetComplexLineBreaksCall)};

  ipc_calls_.push_back(get_complex_line_breaks);
}

bool LineBreakDispatcher::SetupService(InterceptionManager* manager,
                                       IpcTag service) {
  // We perform no interceptions for line breaking right now.
  switch (service) {
    case IpcTag::GETCOMPLEXLINEBREAKS:
      return true;

    default:
      return false;
  }
}

bool LineBreakDispatcher::GetComplexLineBreaksCall(
    IPCInfo* ipc, CountedBuffer* text_buf, uint32_t length,
    CountedBuffer* break_before_buf) {
  if (length > kMaxBrokeredLen ||
      text_buf->Size() != length * sizeof(wchar_t) ||
      break_before_buf->Size() != length) {
    return false;
  }

  CountedParameterSet<EmptyParams> params;
  EvalResult eval =
      policy_base_->EvalPolicy(IpcTag::GETCOMPLEXLINEBREAKS, params.GetBase());
  auto* text = static_cast<wchar_t*>(text_buf->Buffer());
  auto* break_before = static_cast<uint8_t*>(break_before_buf->Buffer());
  ipc->return_info.win32_result =
      LineBreakPolicy::GetComplexLineBreaksProxyAction(eval, text, length,
                                                       break_before);
  return true;
}

}  // namespace sandbox
