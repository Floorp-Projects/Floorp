/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(MozPromiseInlines_h_)
#  define MozPromiseInlines_h_

#  include <type_traits>

#  include "mozilla/MozPromise.h"
#  include "mozilla/dom/PrimitiveConversions.h"
#  include "mozilla/dom/PromiseNativeHandler.h"

namespace mozilla {

// Creates a C++ MozPromise from its JS counterpart, dom::Promise.
// FromDomPromise currently only supports primitive types (int8/16/32, float,
// double) And the reject value type must be a nsresult.
template <typename ResolveValueT, typename RejectValueT, bool IsExclusive>
RefPtr<MozPromise<ResolveValueT, RejectValueT, IsExclusive>>
MozPromise<ResolveValueT, RejectValueT, IsExclusive>::FromDomPromise(
    dom::Promise* aDOMPromise) {
  static_assert(std::is_same_v<RejectValueType, nsresult>,
                "Reject type must be nsresult");
  RefPtr<Private> p = new Private(__func__);
  RefPtr<dom::DomPromiseListener> listener = new dom::DomPromiseListener(
      aDOMPromise,
      [p](JSContext* aCx, JS::Handle<JS::Value> aValue) {
        ResolveValueT value;
        bool ok = dom::ValueToPrimitive<ResolveValueT,
                                        dom::ConversionBehavior::eDefault>(
            aCx, aValue, "Resolution value", &value);
        if (!ok) {
          p->Reject(NS_ERROR_FAILURE, __func__);
          return;
        }
        p->Resolve(value, __func__);
      },
      [p](JSContext* aCx, JS::Handle<JS::Value> aValue) {
        if (!aValue.isInt32()) {
          p->Reject(NS_ERROR_DOM_NOT_NUMBER_ERR, __func__);
          return;
        }
        nsresult rv = nsresult(aValue.toInt32());
        MOZ_ASSERT(NS_SUCCEEDED(rv));
        p->Reject(rv, __func__);
      });
  return p;
}

}  // namespace mozilla

#endif
