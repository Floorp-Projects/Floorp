/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PerformanceUtils_h
#define PerformanceUtils_h

#include "mozilla/PerformanceTypes.h"

class nsPIDOMWindowOuter;

namespace mozilla {

/**
 * Returns an array of promises to asynchronously collect all performance
 * info in the current process.
 */
nsTArray<RefPtr<PerformanceInfoPromise>> CollectPerformanceInfo();

/**
 * Asynchronously collects memory info for a given window
 */
RefPtr<MemoryPromise> CollectMemoryInfo(
    const nsCOMPtr<nsPIDOMWindowOuter>& aWindow,
    const RefPtr<AbstractThread>& aEventTarget);

}  // namespace mozilla
#endif  // PerformanceUtils_h
