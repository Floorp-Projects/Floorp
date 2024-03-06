/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/glean/bindings/Object.h"

#include "Common.h"
#include "mozilla/dom/GleanMetricsBinding.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/Logging.h"
#include "jsapi.h"
#include "js/JSON.h"
#include "nsContentUtils.h"

using namespace mozilla::dom;

namespace mozilla::glean {

/* virtual */
JSObject* GleanObject::WrapObject(JSContext* aCx,
                                  JS::Handle<JSObject*> aGivenProto) {
  return dom::GleanObject_Binding::Wrap(aCx, this, aGivenProto);
}

void GleanObject::Set(JSContext* aCx, JS::Handle<JSObject*> aObj) {
  // We take in an `object`. Cannot be `null`!
  // But at this point the type system doesn't know that.
  JS::Rooted<JS::Value> value(aCx);
  value.setObjectOrNull(aObj);

  nsAutoString serializedValue;
  bool res = nsContentUtils::StringifyJSON(aCx, value, serializedValue,
                                           UndefinedIsNullStringLiteral);
  if (!res) {
    // JS_Stringify throws an exception, e.g. on cyclic objects.
    // We don't want this rethrown.
    JS_ClearPendingException(aCx);

    LogToBrowserConsole(nsIScriptError::warningFlag,
                        u"passed in object cannot be serialized"_ns);
    return;
  }

  NS_ConvertUTF16toUTF8 payload(serializedValue);
  mObject.SetStr(payload);
}

void GleanObject::TestGetValue(JSContext* aCx, const nsACString& aPingName,
                               JS::MutableHandle<JSObject*> aResult,
                               ErrorResult& aRv) {
  aResult.set(nullptr);

  auto result = mObject.TestGetValue(aPingName);
  if (result.isErr()) {
    aRv.ThrowDataError(result.unwrapErr());
    return;
  }
  auto optresult = result.unwrap();
  if (optresult.isNothing()) {
    return;
  }

  const NS_ConvertUTF8toUTF16 str(optresult.ref());
  JS::Rooted<JS::Value> json(aCx);
  bool res = JS_ParseJSON(aCx, str.get(), str.Length(), &json);
  if (!res) {
    aRv.ThrowDataError("couldn't parse stored object");
    return;
  }
  if (!json.isObject()) {
    aRv.ThrowDataError("stored data does not represent a valid object");
    return;
  }

  aResult.set(&json.toObject());
}

}  // namespace mozilla::glean
