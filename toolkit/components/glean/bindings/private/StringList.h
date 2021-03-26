/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_glean_GleanStringList_h
#define mozilla_glean_GleanStringList_h

#include "mozilla/glean/bindings/ScalarGIFFTMap.h"
#include "mozilla/glean/fog_ffi_generated.h"
#include "mozilla/Maybe.h"
#include "nsDebug.h"
#include "nsIGleanMetrics.h"
#include "nsString.h"
#include "nsTArray.h"

namespace mozilla::glean {

namespace impl {

class StringListMetric {
 public:
  constexpr explicit StringListMetric(uint32_t aId) : mId(aId) {}

  /*
   * Adds a new string to the list.
   *
   * Truncates the value if it is longer than 50 bytes and logs an error.
   *
   * @param aValue The string to add.
   */
  void Add(const nsACString& aValue) const {
    auto scalarId = ScalarIdForMetric(mId);
    if (scalarId) {
      Telemetry::ScalarSet(scalarId.extract(), NS_ConvertUTF8toUTF16(aValue),
                           true);
    }
#ifndef MOZ_GLEAN_ANDROID
    fog_string_list_add(mId, &aValue);
#endif
  }

  /*
   * Set to a specific list of strings.
   *
   * Truncates any values longer than 50 bytes and logs an error.
   * Truncates the list if it is over 20 items long.
   * See
   * https://mozilla.github.io/glean/book/user/metrics/string_list.html#limits.
   *
   * @param aValue The list of strings to set the metric to.
   */
  void Set(const nsTArray<nsCString>& aValue) const {
    // Calling `Set` on a mirrored labeled_string is likely an error.
    // We can't remove keys from the mirror scalar and handle this 'properly',
    // so you shouldn't use this operation at all.
    (void)NS_WARN_IF(ScalarIdForMetric(mId).isSome());
#ifndef MOZ_GLEAN_ANDROID
    fog_string_list_set(mId, &aValue);
#endif
  }

  /**
   * **Test-only API**
   *
   * Gets the currently stored value.
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
  Maybe<nsTArray<nsCString>> TestGetValue(
      const nsACString& aPingName = nsCString()) const {
#ifdef MOZ_GLEAN_ANDROID
    Unused << mId;
    return Nothing();
#else
    if (!fog_string_list_test_has_value(mId, &aPingName)) {
      return Nothing();
    }
    nsTArray<nsCString> ret;
    fog_string_list_test_get_value(mId, &aPingName, &ret);
    return Some(std::move(ret));
#endif
  }

 private:
  const uint32_t mId;
};
}  // namespace impl

class GleanStringList final : public nsIGleanStringList {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIGLEANSTRINGLIST

  explicit GleanStringList(uint32_t aId) : mStringList(aId){};

 private:
  virtual ~GleanStringList() = default;

  const impl::StringListMetric mStringList;
};

}  // namespace mozilla::glean

#endif /* mozilla_glean_GleanStringList_h */
