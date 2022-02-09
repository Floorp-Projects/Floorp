/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_jni_GeckoResultUtils_h
#define mozilla_jni_GeckoResultUtils_h

#include "mozilla/java/GeckoResultNatives.h"
#include "mozilla/jni/Conversions.h"

namespace mozilla {
namespace jni {

// C++-side object bound to Java's GeckoResult.GeckoCallback.
//
// Note that we can't template this class because that breaks JNI dispatch
// (surprisingly: it compiles, but selects the wrong method specialization
// during dispatch). So instead we use a templated factory function, which
// bundles the per-ArgType conversion logic into the callback.
class GeckoResultCallback final
    : public java::GeckoResult::GeckoCallback::Natives<GeckoResultCallback> {
 public:
  typedef java::GeckoResult::GeckoCallback::Natives<GeckoResultCallback> Base;
  typedef std::function<void(mozilla::jni::Object::Param)> OuterCallback;

  void Call(mozilla::jni::Object::Param aArg) { mCallback(aArg); }

  template <typename ArgType>
  static java::GeckoResult::GeckoCallback::LocalRef CreateAndAttach(
      std::function<void(ArgType)>&& aInnerCallback) {
    auto java = java::GeckoResult::GeckoCallback::New();
    OuterCallback outerCallback =
        [inner{std::move(aInnerCallback)}](mozilla::jni::Object::Param aParam) {
          ArgType converted = Java2Native<ArgType>(aParam);
          inner(std::move(converted));
        };
    auto native = MakeUnique<GeckoResultCallback>(std::move(outerCallback));
    Base::AttachNative(java, std::move(native));
    return java;
  }

  explicit GeckoResultCallback(OuterCallback&& aCallback)
      : mCallback(std::move(aCallback)) {}

 private:
  OuterCallback mCallback;
};

}  // namespace jni
}  // namespace mozilla

#endif  // mozilla_jni_GeckoResultUtils_h
