// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "event_base.h"

namespace content_analysis {
namespace sdk {

ContentAnalysisEventBase::ContentAnalysisEventBase(
   const BrowserInfo& browser_info)
 : browser_info_(browser_info) {}

ResultCode ContentAnalysisEventBase::Close() {
  return ResultCode::OK;
}

ResultCode UpdateResponse(ContentAnalysisResponse& response,
                   const std::string& tag,
                   ContentAnalysisResponse::Result::Status status) {
  auto result = response.results_size() > 0
      ? response.mutable_results(0)
      : response.add_results();

  if (!tag.empty()) {
    result->set_tag(tag);
  }

  if (status != ContentAnalysisResponse::Result::STATUS_UNKNOWN) {
    result->set_status(status);
  }

  return ResultCode::OK;
}

ResultCode SetEventVerdictTo(
    ContentAnalysisEvent* event,
    ContentAnalysisResponse::Result::TriggeredRule::Action action) {
  // This function expects that the event's result has already been
  // initialized by a call to UpdateResponse().

  if (event->GetResponse().results_size() == 0) {
    return ResultCode::ERR_MISSING_RESULT;
  }

  auto result = event->GetResponse().mutable_results(0);

  // Content analysis responses generated with this SDK contain at most one
  // triggered rule.
  auto rule = result->triggered_rules_size() > 0
      ? result->mutable_triggered_rules(0)
      : result->add_triggered_rules();

  rule->set_action(action);

  return ResultCode::OK;
}

ResultCode SetEventVerdictToBlock(ContentAnalysisEvent* event) {
  return SetEventVerdictTo(event,
      ContentAnalysisResponse::Result::TriggeredRule::BLOCK);
}

}  // namespace sdk
}  // namespace content_analysis
