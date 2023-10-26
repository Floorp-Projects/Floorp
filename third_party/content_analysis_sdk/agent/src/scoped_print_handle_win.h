// Copyright 2023 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_ANALYSIS_AGENT_SRC_SCOPED_PRINT_HANDLE_WIN_H_
#define CONTENT_ANALYSIS_AGENT_SRC_SCOPED_PRINT_HANDLE_WIN_H_

#include <windows.h>

#include "scoped_print_handle_base.h"

namespace content_analysis {
namespace sdk {

class ScopedPrintHandleWin : public ScopedPrintHandleBase {
 public:
  ScopedPrintHandleWin(const ContentAnalysisRequest::PrintData& print_data);
  ~ScopedPrintHandleWin() override;

  const char* data() override;
 private:
  void* mapped_ = nullptr;
  HANDLE handle_ = nullptr;
};

}  // namespace sdk
}  // namespace content_analysis

#endif  // CONTENT_ANALYSIS_AGENT_SRC_SCOPED_PRINT_HANDLE_WIN_H_
