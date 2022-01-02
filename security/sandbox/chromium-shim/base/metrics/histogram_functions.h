/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is a dummy version of Chromium source file base/metrics/histogram_functions.h.
// To provide to empty histogram functions required for compilation.
// We don't require Chromiums histogram collection code.

#ifndef BASE_METRICS_HISTOGRAM_FUNCTIONS_H_
#define BASE_METRICS_HISTOGRAM_FUNCTIONS_H_

namespace base {

BASE_EXPORT void UmaHistogramSparse(const std::string& name, int sample) {}

}  // namespace base

#endif  // BASE_METRICS_HISTOGRAM_FUNCTIONS_H_
