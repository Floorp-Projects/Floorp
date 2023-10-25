// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "event_mac.h"

#include "scoped_print_handle_mac.h"

namespace content_analysis {
namespace sdk {

ContentAnalysisEventMac::ContentAnalysisEventMac(
    const BrowserInfo& browser_info,
    ContentAnalysisRequest req)
    : ContentAnalysisEventBase(browser_info) {
  *request() = std::move(req);
}

ResultCode ContentAnalysisEventMac::Send() {
  return ResultCode::ERR_UNEXPECTED;
}

std::string ContentAnalysisEventMac::DebugString() const {
  return std::string();
}


}  // namespace sdk
}  // namespace content_analysis
