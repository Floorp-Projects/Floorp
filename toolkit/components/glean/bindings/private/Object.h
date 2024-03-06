/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_glean_GleanObject_h
#define mozilla_glean_GleanObject_h

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/glean/bindings/GleanMetric.h"
#include "mozilla/glean/fog_ffi_generated.h"
#include "mozilla/ResultVariant.h"
#include "nsString.h"
#include "nsTArray.h"

namespace mozilla::glean {

// forward declaration
class GleanObject;

namespace impl {

template <class T>
class ObjectMetric {
  friend class mozilla::glean::GleanObject;

 public:
  constexpr explicit ObjectMetric(uint32_t id) : mId(id) {}

 private:
  const uint32_t mId;

  /* TODO(bug 1881023): Turn this into the public C++ API */
  /**
   * **Test-only API**
   *
   * Gets the currently stored object as a JSON-encoded string.
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
  Result<Maybe<nsCString>, nsCString> TestGetValue(
      const nsACString& aPingName) const {
    nsCString err;
    if (fog_object_test_get_error(mId, &err)) {
      return Err(err);
    }
    if (!fog_object_test_has_value(mId, &aPingName)) {
      return Maybe<nsCString>();
    }
    nsCString ret;
    fog_object_test_get_value(mId, &aPingName, &ret);
    return Some(ret);
  }

  void SetStr(const nsACString& aValue) const {
    fog_object_set_string(mId, &aValue);
  }
};

}  // namespace impl

class GleanObject final : public GleanMetric {
 public:
  explicit GleanObject(uint32_t aId, nsISupports* aParent)
      : GleanMetric(aParent), mObject(aId) {}

  virtual JSObject* WrapObject(
      JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override final;

  void Set(JSContext* aCx, JS::Handle<JSObject*> aObj);

  void TestGetValue(JSContext* aCx, const nsACString& aPingName,
                    JS::MutableHandle<JSObject*> aResult, ErrorResult& aRv);

  virtual ~GleanObject() = default;

 private:
  const impl::ObjectMetric<void> mObject;
};

}  // namespace mozilla::glean

#endif /* mozilla_glean_GleanObject.h */
