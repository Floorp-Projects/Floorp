/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/glean/bindings/Event.h"

#include "Common.h"
#include "nsString.h"
#include "mozilla/Components.h"
#include "nsIClassInfoImpl.h"
#include "jsapi.h"
#include "nsIScriptError.h"

namespace mozilla::glean {

NS_IMPL_CLASSINFO(GleanEvent, nullptr, 0, {0})
NS_IMPL_ISUPPORTS_CI(GleanEvent, nsIGleanEvent)

NS_IMETHODIMP
GleanEvent::Record(JS::HandleValue aExtra, JSContext* aCx) {
  if (aExtra.isNullOrUndefined()) {
    mEvent.Record();
    return NS_OK;
  }

  if (!aExtra.isObject()) {
    LogToBrowserConsole(nsIScriptError::warningFlag,
                        u"Extras need to be an object"_ns);
    return NS_OK;
  }

  nsTArray<nsCString> extraKeys;
  nsTArray<nsCString> extraValues;

  JS::RootedObject obj(aCx, &aExtra.toObject());
  JS::Rooted<JS::IdVector> ids(aCx, JS::IdVector(aCx));
  if (!JS_Enumerate(aCx, obj, &ids)) {
    LogToBrowserConsole(nsIScriptError::warningFlag,
                        u"Failed to enumerate object."_ns);
    return NS_OK;
  }

  for (size_t i = 0, n = ids.length(); i < n; i++) {
    nsAutoJSCString jsKey;
    if (!jsKey.init(aCx, ids[i])) {
      LogToBrowserConsole(
          nsIScriptError::warningFlag,
          u"Extra dictionary should only contain string keys."_ns);
      return NS_OK;
    }

    JS::Rooted<JS::Value> value(aCx);
    if (!JS_GetPropertyById(aCx, obj, ids[i], &value)) {
      LogToBrowserConsole(nsIScriptError::warningFlag,
                          u"Failed to get extra property."_ns);
      return NS_OK;
    }

    nsAutoJSCString jsValue;
    if (!value.isString() || !jsValue.init(aCx, value)) {
      LogToBrowserConsole(nsIScriptError::warningFlag,
                          u"Extra properties should have string values."_ns);
      return NS_OK;
    }

    extraKeys.AppendElement(jsKey);
    extraValues.AppendElement(jsValue);
  }

  // Calling the implementation directly, because we have a `string->string`
  // map, not a `T->string` map the C++ API expects.
  impl::fog_event_record_str(mEvent.mId, &extraKeys, &extraValues);
  return NS_OK;
}

NS_IMETHODIMP
GleanEvent::TestGetValue(const nsACString& aStorageName, JSContext* aCx,
                         JS::MutableHandleValue aResult) {
  auto result = mEvent.TestGetValue(aStorageName);
  if (result.isNothing()) {
    aResult.set(JS::UndefinedValue());
    return NS_OK;
  }

  // TODO(bug 1678567): Implement this.
  return NS_ERROR_FAILURE;
}

}  // namespace mozilla::glean
