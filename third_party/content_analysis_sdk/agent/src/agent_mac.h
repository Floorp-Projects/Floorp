// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_ANALYSIS_SRC_AGENT_MAC_H_
#define CONTENT_ANALYSIS_SRC_AGENT_MAC_H_

#include "agent_base.h"

namespace content_analysis {
namespace sdk {

// Agent implementaton for macOS.
class AgentMac : public AgentBase {
 public:
  AgentMac(Config config, std::unique_ptr<AgentEventHandler> handler);

  ResultCode HandleEvents() override;
  std::string DebugString() const override;

  // TODO(rogerta): Fill in implementation.
};

}  // namespace sdk
}  // namespace content_analysis

#endif  // CONTENT_ANALYSIS_SRC_AGENT_MAC_H_
