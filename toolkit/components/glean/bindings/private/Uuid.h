/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_glean_GleanUuid_h
#define mozilla_glean_GleanUuid_h

#include "nsIGleanMetrics.h"
#include "nsString.h"

namespace mozilla {
namespace glean {

namespace impl {
extern "C" {
void fog_uuid_set(uint32_t id, const nsACString& uuid);
void fog_uuid_generate_and_set(uint32_t id);
uint32_t fog_uuid_test_has_value(uint32_t id, const char* storageName);
void fog_uuid_test_get_value(uint32_t id, const char* storageName,
                             nsACString& value);
}

class UuidMetric {
 public:
  constexpr explicit UuidMetric(uint32_t id) : mId(id) {}

  /*
   * Sets to the specified value.
   *
   * @param value The UUID to set the metric to.
   */
  void Set(const nsACString& value) const { fog_uuid_set(mId, value); }

  /*
   * Generate a new random UUID and set the metric to it.
   */
  void GenerateAndSet() const { fog_uuid_generate_and_set(mId); }

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
    return fog_uuid_test_has_value(mId, aStorageName) != 0;
  }

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
   * @return value of the stored metric.
   */
  nsCString TestGetValue(const char* aStorageName) const {
    nsCString ret;
    fog_uuid_test_get_value(mId, aStorageName, ret);
    return ret;
  }

 private:
  const uint32_t mId;
};
}  // namespace impl

class GleanUuid final : public nsIGleanUuid {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIGLEANUUID

  explicit GleanUuid(uint32_t id) : mUuid(id){};

 private:
  virtual ~GleanUuid() = default;

  const impl::UuidMetric mUuid;
};

}  // namespace glean
}  // namespace mozilla

#endif /* mozilla_glean_GleanUuid.h */
