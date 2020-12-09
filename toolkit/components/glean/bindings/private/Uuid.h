/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_glean_GleanUuid_h
#define mozilla_glean_GleanUuid_h

#include "mozilla/Maybe.h"
#include "nsIGleanMetrics.h"
#include "nsString.h"
#include "mozilla/glean/fog_ffi_generated.h"

namespace mozilla::glean {

namespace impl {

class UuidMetric {
 public:
  constexpr explicit UuidMetric(uint32_t aId) : mId(aId) {}

  /*
   * Sets to the specified value.
   *
   * @param aValue The UUID to set the metric to.
   */
  void Set(const nsACString& aValue) const { fog_uuid_set(mId, &aValue); }

  /*
   * Generate a new random UUID and set the metric to it.
   */
  void GenerateAndSet() const { fog_uuid_generate_and_set(mId); }

  /**
   * **Test-only API**
   *
   * Gets the currently stored value as a hyphenated string.
   *
   * This function will attempt to await the last parent-process task (if any)
   * writing to the the metric's storage engine before returning a value.
   * This function will not wait for data from child processes.
   *
   * This doesn't clear the stored value.
   * Parent process only. Panics in child processes.
   * Panics if there is no value to get.
   *
   * @return value of the stored metric, or Nothing() if there is no value.
   */
  Maybe<nsCString> TestGetValue(const nsACString& aStorageName) const {
    if (!fog_uuid_test_has_value(mId, &aStorageName)) {
      return Nothing();
    }
    nsCString ret;
    fog_uuid_test_get_value(mId, &aStorageName, &ret);
    return Some(ret);
  }

 private:
  const uint32_t mId;
};
}  // namespace impl

class GleanUuid final : public nsIGleanUuid {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIGLEANUUID

  explicit GleanUuid(uint32_t aId) : mUuid(aId){};

 private:
  virtual ~GleanUuid() = default;

  const impl::UuidMetric mUuid;
};

}  // namespace mozilla::glean

#endif /* mozilla_glean_GleanUuid_h */
