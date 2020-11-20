/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_glean_GleanString_h
#define mozilla_glean_GleanString_h

#include "nsIGleanMetrics.h"
#include "nsString.h"

namespace mozilla {
namespace glean {

namespace impl {
extern "C" {
void fog_string_set(uint32_t id, const nsACString& value);
uint32_t fog_string_test_has_value(uint32_t id, const char* storageName);
void fog_string_test_get_value(uint32_t id, const char* storageName,
                               nsACString& value);
}

class StringMetric {
 public:
  constexpr explicit StringMetric(uint32_t id) : mId(id) {}

  /*
   * Set to the specified value.
   *
   * Truncates the value if it is longer than 100 bytes and logs an error.
   * See https://mozilla.github.io/glean/book/user/metrics/string.html#limits.
   *
   * @param value The string to set the metric to.
   */
  void Set(const nsACString& value) const { fog_string_set(mId, value); }

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
   * @return value of the stored metric, or Nothing() if there is no value.
   */
  Maybe<nsCString> TestGetValue(const char* aStorageName) const {
    if (!fog_string_test_has_value(mId, aStorageName)) {
      return Nothing();
    }
    nsCString ret;
    fog_string_test_get_value(mId, aStorageName, ret);
    return Some(ret);
  }

 private:
  const uint32_t mId;
};
}  // namespace impl

class GleanString final : public nsIGleanString {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIGLEANSTRING

  explicit GleanString(uint32_t id) : mString(id){};

 private:
  virtual ~GleanString() = default;

  const impl::StringMetric mString;
};

}  // namespace glean
}  // namespace mozilla

#endif /* mozilla_glean_GleanString.h */
