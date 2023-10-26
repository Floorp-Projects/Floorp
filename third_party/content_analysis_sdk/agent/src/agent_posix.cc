// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "agent_posix.h"
#include "event_posix.h"

namespace content_analysis {
namespace sdk {

// static
std::unique_ptr<Agent> Agent::Create(
    Config config,
    std::unique_ptr<AgentEventHandler> handler,
    ResultCode* rc) {
  *rc = ResultCode::ERR_UNEXPECTED;
  return std::make_unique<AgentPosix>(std::move(config), std::move(handler));
}

AgentPosix::AgentPosix(
    Config config,
    std::unique_ptr<AgentEventHandler> handler)
  : AgentBase(std::move(config), std::move(handler)) {}

ResultCode AgentPosix::HandleEvents() {
  return ResultCode::ERR_UNEXPECTED;
}

std::string AgentPosix::DebugString() const {
  return std::string();
}

}  // namespace sdk
}  // namespace content_analysis
