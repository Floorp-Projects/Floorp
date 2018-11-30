/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PerformanceTypes_h
#define PerformanceTypes_h

#include "mozilla/MozPromise.h"

namespace mozilla {

namespace dom {
class PerformanceInfo;
class PerformanceMemoryInfo;
struct PerformanceInfoDictionary;
}  // namespace dom

/**
 * Promises definitions
 */
typedef MozPromise<dom::PerformanceInfo, nsresult, true> PerformanceInfoPromise;
typedef MozPromise<nsTArray<dom::PerformanceInfoDictionary>, nsresult, true>
    RequestMetricsPromise;
typedef MozPromise<nsTArray<dom::PerformanceInfo>, nsresult, true>
    PerformanceInfoArrayPromise;
typedef MozPromise<mozilla::dom::PerformanceMemoryInfo, nsresult, true>
    MemoryPromise;

}  // namespace mozilla
#endif  // PerformanceTypes_h
