// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_ANALYSIS_AGENT_SRC_AGENT_BASE_H_
#define CONTENT_ANALYSIS_AGENT_SRC_AGENT_BASE_H_

#include <memory>

#include "content_analysis/sdk/analysis_agent.h"

namespace content_analysis {
namespace sdk {

// Base Agent class with code common to all platforms.
class AgentBase : public Agent {
 public:
  // Agent:
  const Config& GetConfig() const override;
  ResultCode Stop() override;

 protected:
  AgentBase(Config config, std::unique_ptr<AgentEventHandler> handler);

  AgentEventHandler* handler() const { return handler_.get(); }
  const Config& configuration() const { return config_; }

  // Notifies the handler of the given error.  Returns the error
  // passed into the method.
  ResultCode NotifyError(const char* context, ResultCode error);

 private:
  Config config_;
  std::unique_ptr<AgentEventHandler> handler_;
};

}  // namespace sdk
}  // namespace content_analysis

#endif  // CONTENT_ANALYSIS_AGENT_SRC_AGENT_BASE_H_