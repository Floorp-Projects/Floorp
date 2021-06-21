/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_glean_GleanDatetime_h
#define mozilla_glean_GleanDatetime_h

#include "mozilla/Maybe.h"
#include "nsIGleanMetrics.h"
#include "nsString.h"
#include "prtime.h"

namespace mozilla::glean {

namespace impl {

class DatetimeMetric {
 public:
  constexpr explicit DatetimeMetric(uint32_t aId) : mId(aId) {}

  /*
   * Set the datetime to the provided value, or the local now.
   *
   * @param amount The date value to set.
   */
  void Set(const PRExplodedTime* aValue = nullptr) const;

  /**
   * **Test-only API**
   *
   * Gets the currently stored value as a PRExplodedTime.
   *
   * This function will attempt to await the last parent-process task (if any)
   * writing to the the metric's storage engine before returning a value.
   * This function will not wait for data from child processes.
   *
   * This doesn't clear the stored value.
   * Parent process only. Panics in child processes.
   *
   * @param aPingName The (optional) name of the ping to retrieve the metric
   *        for. Defaults to the first value in `send_in_pings`.
   *
   * @return value of the stored metric, or Nothing() if there is no value.
   */
  Maybe<PRExplodedTime> TestGetValue(
      const nsACString& aPingName = nsCString()) const;

 private:
  const uint32_t mId;
};
}  // namespace impl

class GleanDatetime final : public nsIGleanDatetime {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIGLEANDATETIME

  explicit GleanDatetime(uint32_t aId) : mDatetime(aId){};

 private:
  virtual ~GleanDatetime() = default;

  const impl::DatetimeMetric mDatetime;
};

}  // namespace mozilla::glean

#endif /* mozilla_glean_GleanDatetime_h */
