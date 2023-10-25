// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "agent_base.h"

namespace content_analysis {
namespace sdk {

AgentBase::AgentBase(Config config, std::unique_ptr<AgentEventHandler> handler)
    : config_(std::move(config)), handler_(std::move(handler)) {}

const Agent::Config& AgentBase::GetConfig() const {
  return config_;
}

ResultCode AgentBase::Stop() {
  return ResultCode::OK;
}

ResultCode AgentBase::NotifyError(const char* context, ResultCode error) {
  if (handler_) {
    handler_->OnInternalError(context, error);
  }
  return error;
}

#define RC_RECOVERABLE(RC, MSG) case ResultCode::RC: return MSG;
#define RC_UNRECOVERABLE(RC, MSG) case ResultCode::RC: return MSG;
const char* ResultCodeToString(ResultCode rc) {
  switch (rc) {
#include "content_analysis/sdk/result_codes.inc"
  }
  return "Unknown error code.";
}
#undef RC_RECOVERABLE
#undef RC_UNRECOVERABLE

}  // namespace sdk
}  // namespace content_analysis
