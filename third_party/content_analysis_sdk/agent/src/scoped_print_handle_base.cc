// Copyright 2023 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "scoped_print_handle_base.h"

namespace content_analysis {
namespace sdk {

ScopedPrintHandleBase::ScopedPrintHandleBase(
    const ContentAnalysisRequest::PrintData& print_data)
    : size_(print_data.size()) {}

size_t ScopedPrintHandleBase::size() { return size_; }

}  // namespace sdk
}  // namespace content_analysis
