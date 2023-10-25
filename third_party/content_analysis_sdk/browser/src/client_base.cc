// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "client_base.h"

namespace content_analysis {
namespace sdk {

ClientBase::ClientBase(Config config) : config_(config) {}

const Client::Config& ClientBase::GetConfig() const {
  return config_;
}

const AgentInfo& ClientBase::GetAgentInfo() const {
  return agent_info_;
}

}  // namespace sdk
}  // namespace content_analysis
