/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_glean_GleanCounter_h
#define mozilla_glean_GleanCounter_h

#include "nsIGleanMetrics.h"

namespace mozilla {
namespace glean {

namespace impl {
extern "C" {
void fog_counter_add(uint32_t id, int32_t amount);
uint32_t fog_counter_test_has_value(uint32_t id, const char* storageName);
int32_t fog_counter_test_get_value(uint32_t id, const char* storageName);
}

class CounterMetric {
 public:
  constexpr explicit CounterMetric(uint32_t id) : mId(id) {}

  /*
   * Increases the counter by `amount`.
   *
   * @param amount The amount to increase by. Should be positive.
   */
  void Add(int32_t amount = 1) const { fog_counter_add(mId, amount); }

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
    return fog_counter_test_has_value(mId, aStorageName) != 0;
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
  int32_t TestGetValue(const char* aStorageName) const {
    return fog_counter_test_get_value(mId, aStorageName);
  }

 private:
  const uint32_t mId;
};
}  // namespace impl

class GleanCounter final : public nsIGleanCounter {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIGLEANCOUNTER

  explicit GleanCounter(uint32_t id) : mCounter(id){};

 private:
  virtual ~GleanCounter() = default;

  const impl::CounterMetric mCounter;
};

}  // namespace glean
}  // namespace mozilla

#endif /* mozilla_glean_GleanCounter.h */
