// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_ANALYSIS_AGENT_SRC_EVENT_BASE_H_
#define CONTENT_ANALYSIS_AGENT_SRC_EVENT_BASE_H_

#include "content_analysis/sdk/analysis_agent.h"

namespace content_analysis {
namespace sdk {

// Base ContentAnalysisEvent class with code common to all platforms.
class ContentAnalysisEventBase : public ContentAnalysisEvent {
 public:
   // ContentAnalysisEvent:
  ResultCode Close() override;
  const BrowserInfo& GetBrowserInfo() const override { return browser_info_; }
  const ContentAnalysisRequest& GetRequest() const override { return request_; }
  ContentAnalysisResponse& GetResponse() override { return *response(); }

 protected:
  explicit ContentAnalysisEventBase(const BrowserInfo& browser_info);

  ContentAnalysisRequest* request() { return &request_; }
  AgentToChrome* agent_to_chrome() { return &agent_to_chrome_; }
  ContentAnalysisResponse* response() { return agent_to_chrome()->mutable_response(); }

private:
  BrowserInfo browser_info_;
  ContentAnalysisRequest request_;
  AgentToChrome agent_to_chrome_;
};

}  // namespace sdk
}  // namespace content_analysis

#endif  // CONTENT_ANALYSIS_AGENT_SRC_EVENT_BASE_H_
