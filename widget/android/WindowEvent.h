/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_WindowEvent_h
#define mozilla_widget_WindowEvent_h

#include "nsThreadUtils.h"
#include "mozilla/jni/Natives.h"

namespace mozilla {
namespace widget {

// An Event subclass that guards against stale events.
// (See the implmentation of mozilla::jni::detail::ProxyNativeCall for info
//  about the default template parameters for this class)
template <typename Lambda, bool IsStatic = Lambda::isStatic,
          typename InstanceType = typename Lambda::ThisArgType,
          class Impl = typename Lambda::TargetClass>
class WindowEvent : public Runnable {
  bool IsStaleCall() {
    if (IsStatic) {
      // Static calls are never stale.
      return false;
    }

    return jni::NativePtrTraits<Impl>::IsStale(mInstance);
  }

  Lambda mLambda;
  const InstanceType mInstance;

 public:
  WindowEvent(Lambda&& aLambda, InstanceType&& aInstance)
      : Runnable("mozilla::widget::WindowEvent"),
        mLambda(std::move(aLambda)),
        mInstance(std::forward<InstanceType>(aInstance)) {}

  explicit WindowEvent(Lambda&& aLambda)
      : Runnable("mozilla::widget::WindowEvent"),
        mLambda(std::move(aLambda)),
        mInstance(mLambda.GetThisArg()) {}

  NS_IMETHOD Run() override {
    if (!IsStaleCall()) {
      mLambda();
    }
    return NS_OK;
  }
};

}  // namespace widget
}  // namespace mozilla

#endif  // mozilla_widget_WindowEvent_h
