/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_glean_GleanBoolean_h
#define mozilla_glean_GleanBoolean_h

#include "nsIGleanMetrics.h"
#include "nsString.h"

namespace mozilla {
namespace glean {

namespace impl {

class BooleanMetric {
 public:
  constexpr explicit BooleanMetric(uint32_t id) : mId(id) {}

  /**
   * Set to the specified boolean value.
   *
   * @param aValue the value to set.
   */
  void Set(bool aValue) const;

  /**
   * **Test-only API**
   *
   * Gets the currently stored value as a boolean.
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
   * @return value of the stored metric.
   */
  Maybe<bool> TestGetValue(const nsACString& aPingName = nsCString()) const;

 private:
  const uint32_t mId;
};

}  // namespace impl

class GleanBoolean final : public nsIGleanBoolean {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIGLEANBOOLEAN

  explicit GleanBoolean(uint32_t id) : mBoolean(id){};

 private:
  virtual ~GleanBoolean() = default;

  const impl::BooleanMetric mBoolean;
};

}  // namespace glean
}  // namespace mozilla

#endif /* mozilla_glean_GleanBoolean.h */
