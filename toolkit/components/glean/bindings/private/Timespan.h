/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_glean_GleanTimespan_h
#define mozilla_glean_GleanTimespan_h

#include "nsIGleanMetrics.h"

namespace mozilla {
namespace glean {

namespace impl {
extern "C" {
void fog_timespan_start(uint32_t id);
void fog_timespan_stop(uint32_t id);
uint32_t fog_timespan_test_has_value(uint32_t id, const char* storageName);
int64_t fog_timespan_test_get_value(uint32_t id, const char* storageName);
}

class TimespanMetric {
 public:
  constexpr explicit TimespanMetric(uint32_t id) : mId(id) {}

  /**
   * Start tracking time for the provided metric.
   *
   * This records an error if it’s already tracking time (i.e. start was already
   * called with no corresponding [stop]): in that case the original
   * start time will be preserved.
   */
  void Start() const { fog_timespan_start(mId); }

  /**
   * Stop tracking time for the provided metric.
   *
   * Sets the metric to the elapsed time, but does not overwrite an already
   * existing value.
   * This will record an error if no [start] was called or there is an already
   * existing value.
   */
  void Stop() const { fog_timespan_stop(mId); }

  /**
   * **Test-only API**
   *
   * Tests whether a value is stored for the metric.
   *
   * This function will attempt to await the last parent-process task (if any)
   * writing to the the metric's storage engine before returning a value.
   * This function will not wait for data from child processes.
   *
   * Parent process only. Panics in child processes.
   *
   * @param aStorageName the name of the ping to retrieve the metric for.
   * @return true if metric value exists, otherwise false
   */
  bool TestHasValue(const char* aStorageName) const {
    return fog_timespan_test_has_value(mId, aStorageName) != 0;
  }

  /**
   * **Test-only API**
   *
   * Gets the currently stored value as an integer.
   *
   * This function will attempt to await the last parent-process task (if any)
   * writing to the the metric's storage engine before returning a value.
   * This function will not wait for data from child processes.
   *
   * This doesn't clear the stored value.
   * Parent process only. Panics in child processes.
   *
   * @return value of the stored metric.
   */
  int64_t TestGetValue(const char* aStorageName) const {
    return fog_timespan_test_get_value(mId, aStorageName);
  }

 private:
  const uint32_t mId;
};
}  // namespace impl

class GleanTimespan final : public nsIGleanTimespan {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIGLEANTIMESPAN

  explicit GleanTimespan(uint32_t id) : mTimespan(id){};

 private:
  virtual ~GleanTimespan() = default;

  const impl::TimespanMetric mTimespan;
};

}  // namespace glean
}  // namespace mozilla

#endif /* mozilla_glean_GleanTimespan.h */
