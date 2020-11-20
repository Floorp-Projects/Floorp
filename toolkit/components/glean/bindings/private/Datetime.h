/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_glean_GleanDatetime_h
#define mozilla_glean_GleanDatetime_h

#include "nsIGleanMetrics.h"
#include "nsString.h"
#include "prtime.h"

namespace mozilla {
namespace glean {

namespace impl {
extern "C" {
void fog_datetime_set(uint32_t id, int32_t year, uint32_t month, uint32_t day,
                      uint32_t hour, uint32_t minute, uint32_t second,
                      uint32_t nano, int32_t offset_seconds);
uint32_t fog_datetime_test_has_value(uint32_t id, const char* storageName);
void fog_datetime_test_get_value(uint32_t id, const char* storageName,
                                 nsACString& value);
}

class DatetimeMetric {
 public:
  constexpr explicit DatetimeMetric(uint32_t id) : mId(id) {}

  /*
   * Set the datetime to the provided value, or the local now.
   *
   * @param amount The date value to set.
   */
  void Set(const PRExplodedTime* value = nullptr) const {
    PRExplodedTime exploded;
    if (!value) {
      PR_ExplodeTime(PR_Now(), PR_LocalTimeParameters, &exploded);
    } else {
      exploded = *value;
    }

    int32_t offset =
        exploded.tm_params.tp_gmt_offset + exploded.tm_params.tp_dst_offset;
    fog_datetime_set(mId, exploded.tm_year, exploded.tm_month + 1,
                    exploded.tm_mday, exploded.tm_hour, exploded.tm_min,
                    exploded.tm_sec, exploded.tm_usec * 1000, offset);
  }

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
    return fog_datetime_test_has_value(mId, aStorageName) != 0;
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
  nsCString TestGetValue(const char* aStorageName) const {
    nsCString ret;
    fog_datetime_test_get_value(mId, aStorageName, ret);
    return ret;
  }

 private:
  const uint32_t mId;
};
}  // namespace impl

class GleanDatetime final : public nsIGleanDatetime {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIGLEANDATETIME

  explicit GleanDatetime(uint32_t id) : mDatetime(id){};

 private:
  virtual ~GleanDatetime() = default;

  const impl::DatetimeMetric mDatetime;
};

}  // namespace glean
}  // namespace mozilla

#endif /* mozilla_glean_GleanDatetime.h */
