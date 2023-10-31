// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_ANALYSIS_AGENT_SRC_EVENT_WIN_H_
#define CONTENT_ANALYSIS_AGENT_SRC_EVENT_WIN_H_

#include <windows.h>

#include "event_base.h"

namespace content_analysis {
namespace sdk {

// ContentAnalysisEvent implementaton for Windows.
class ContentAnalysisEventWin : public ContentAnalysisEventBase {
 public:
  ContentAnalysisEventWin(HANDLE handle,
                          const BrowserInfo& browser_info,
                          ContentAnalysisRequest request);
  ~ContentAnalysisEventWin() override;

  // Initialize the event.  This involves reading the request from Google
  // Chrome and making sure it is well formed.
  ResultCode Init();

  // ContentAnalysisEvent:
  ResultCode Close() override;
  ResultCode Send() override;
  std::string DebugString() const override;
  std::string SerializeStringToSendToBrowser() {
    return agent_to_chrome()->SerializeAsString();
  }
  void SetResponseSent() { response_sent_ = true; }
  
  HANDLE Pipe() const { return hPipe_; }

 private:
  void Shutdown();

  // This handle is not owned by the event.
  HANDLE hPipe_ = INVALID_HANDLE_VALUE;

  // Set to true when Send() is called the first time.
  bool response_sent_ = false;
};

}  // namespace sdk
}  // namespace content_analysis

#endif  // CONTENT_ANALYSIS_AGENT_SRC_EVENT_WIN_H_