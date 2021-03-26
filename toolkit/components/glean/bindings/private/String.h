/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_glean_GleanString_h
#define mozilla_glean_GleanString_h

#include "mozilla/glean/bindings/ScalarGIFFTMap.h"
#include "mozilla/glean/fog_ffi_generated.h"
#include "mozilla/Maybe.h"
#include "nsIGleanMetrics.h"
#include "nsString.h"

namespace mozilla::glean {

namespace impl {

class StringMetric {
 public:
  constexpr explicit StringMetric(uint32_t aId) : mId(aId) {}

  /*
   * Set to the specified value.
   *
   * Truncates the value if it is longer than 100 bytes and logs an error.
   * See https://mozilla.github.io/glean/book/user/metrics/string.html#limits.
   *
   * @param aValue The string to set the metric to.
   */
  void Set(const nsACString& aValue) const {
    auto scalarId = ScalarIdForMetric(mId);
    if (scalarId) {
      Telemetry::ScalarSet(scalarId.extract(), NS_ConvertUTF8toUTF16(aValue));
    }
#ifndef MOZ_GLEAN_ANDROID
    fog_string_set(mId, &aValue);
#endif
  }

  /**
   * **Test-only API**
   *
   * Gets the currently stored value as a string.
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
  Maybe<nsCString> TestGetValue(
      const nsACString& aPingName = nsCString()) const {
#ifdef MOZ_GLEAN_ANDROID
    Unused << mId;
    return Nothing();
#else
    if (!fog_string_test_has_value(mId, &aPingName)) {
      return Nothing();
    }
    nsCString ret;
    fog_string_test_get_value(mId, &aPingName, &ret);
    return Some(ret);
#endif
  }

 private:
  const uint32_t mId;
};
}  // namespace impl

class GleanString final : public nsIGleanString {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIGLEANSTRING

  explicit GleanString(uint32_t aId) : mString(aId){};

 private:
  virtual ~GleanString() = default;

  const impl::StringMetric mString;
};

}  // namespace mozilla::glean

#endif /* mozilla_glean_GleanString_h */
