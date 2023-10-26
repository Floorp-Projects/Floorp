// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_ANALYSIS_SRC_EVENT_MAC_H_
#define CONTENT_ANALYSIS_SRC_EVENT_MAC_H_

#include "event_base.h"

namespace content_analysis {
namespace sdk {

// ContentAnalysisEvent implementaton for macOS.
class ContentAnalysisEventMac : public ContentAnalysisEventBase {
 public:
  ContentAnalysisEventMac(const BrowserInfo& browser_info,
                          ContentAnalysisRequest request);

  // ContentAnalysisEvent:
  ResultCode Send() override;
  std::string DebugString() const override;

  // TODO(rogerta): Fill in implementation.
};

}  // namespace sdk
}  // namespace content_analysis

#endif  // CONTENT_ANALYSIS_SRC_EVENT_MAC_H_
