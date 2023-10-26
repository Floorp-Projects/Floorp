// Copyright 2023 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_ANALYSIS_AGENT_SRC_SCOPED_PRINT_HANDLE_MAC_H_
#define CONTENT_ANALYSIS_AGENT_SRC_SCOPED_PRINT_HANDLE_MAC_H_

#include "scoped_print_handle_base.h"

namespace content_analysis {
namespace sdk {

class ScopedPrintHandleMac : public ScopedPrintHandleBase {
 public:
  ScopedPrintHandleMac(const ContentAnalysisRequest::PrintData& print_data);
  ~ScopedPrintHandleMac() override;

  const char* data() override;
};

}  // namespace sdk
}  // namespace content_analysis

#endif  // CONTENT_ANALYSIS_AGENT_SRC_SCOPED_PRINT_HANDLE_MAC_H_
