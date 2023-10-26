// Copyright 2023 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_ANALYSIS_AGENT_SRC_SCOPED_PRINT_HANDLE_BASE_H_
#define CONTENT_ANALYSIS_AGENT_SRC_SCOPED_PRINT_HANDLE_BASE_H_

#include "content_analysis/sdk/analysis_agent.h"

namespace content_analysis {
namespace sdk {

class ScopedPrintHandleBase : public ScopedPrintHandle {
 public:
  ScopedPrintHandleBase(const ContentAnalysisRequest::PrintData& print_data);

  size_t size() override;
 protected:
  size_t size_ = 0;
};

}  // namespace sdk
}  // namespace content_analysis

#endif  // CONTENT_ANALYSIS_AGENT_SRC_SCOPED_PRINT_HANDLE_BASE_H_
