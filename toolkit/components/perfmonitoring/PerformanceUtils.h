/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PerformanceCollector_h
#define PerformanceCollector_h

#include "mozilla/dom/DOMTypes.h"   // defines PerformanceInfo

namespace mozilla {

/**
 * Collects all performance info in the current process
 * and adds then in the aMetrics arrey
 */
void CollectPerformanceInfo(nsTArray<dom::PerformanceInfo>& aMetrics);

/**
 * Converts a PerformanceInfo array into a nsIPerformanceMetricsData and
 * sends a performance-metrics notification with it
 */
nsresult NotifyPerformanceInfo(const nsTArray<dom::PerformanceInfo>& aMetrics);

} // namespace mozilla
#endif   // PerformanceCollector_h
