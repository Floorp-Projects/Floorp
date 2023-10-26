// Copyright 2023 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "scoped_print_handle_mac.h"

namespace content_analysis {
namespace sdk {

std::unique_ptr<ScopedPrintHandle>
CreateScopedPrintHandle(const ContentAnalysisRequest& request,
                        int64_t browser_pid) {
  if (!request.has_print_data() || !request.print_data().has_handle()) {
    return nullptr;
  }

  return std::make_unique<ScopedPrintHandleMac>(request.print_data());
}

ScopedPrintHandleMac::ScopedPrintHandleMac(
    const ContentAnalysisRequest::PrintData& print_data)
    : ScopedPrintHandleBase(print_data) {
  // TODO
}

ScopedPrintHandleMac::~ScopedPrintHandleMac() {
  // TODO
}

const char* ScopedPrintHandleMac::data() {
  // TODO
  return nullptr;
}

}  // namespace sdk
}  // namespace content_analysis
