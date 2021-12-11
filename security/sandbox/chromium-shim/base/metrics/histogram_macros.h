/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is a dummy version of Chromium source file base/metrics/histogram_macros.h.
// To provide empty histogram macros required for compilation.
// We don't require Chromiums histogram collection code.

#ifndef BASE_METRICS_HISTOGRAM_MACROS_H_
#define BASE_METRICS_HISTOGRAM_MACROS_H_

#define UMA_HISTOGRAM_ENUMERATION(name, sample) do { } while (0)

#endif  // BASE_METRICS_HISTOGRAM_MACROS_H_
