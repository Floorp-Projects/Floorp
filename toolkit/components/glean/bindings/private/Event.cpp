/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/glean/bindings/Event.h"

#include "Common.h"
#include "nsString.h"
#include "mozilla/Components.h"
#include "mozilla/dom/ToJSValue.h"
#include "nsIClassInfoImpl.h"
#include "jsapi.h"
#include "js/PropertyAndElement.h"  // JS_DefineElement, JS_DefineProperty, JS_Enumerate, JS_GetProperty, JS_GetPropertyById
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
  CopyableTArray<Telemetry::EventExtraEntry> telExtras;

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
    if (value.isString() || (value.isInt32() && value.toInt32() >= 0) ||
        value.isBoolean()) {
      if (!jsValue.init(aCx, value)) {
        LogToBrowserConsole(nsIScriptError::warningFlag,
                            u"Can't extract extra property"_ns);
        return NS_OK;
      }
    } else {
      LogToBrowserConsole(
          nsIScriptError::warningFlag,
          u"Extra properties should have string, bool or non-negative integer values."_ns);
      return NS_OK;
    }

    extraKeys.AppendElement(jsKey);
    extraValues.AppendElement(jsValue);
    telExtras.EmplaceBack(Telemetry::EventExtraEntry{jsKey, jsValue});
  }

  // Since this calls the implementation directly, we need to implement GIFFT
  // here as well as in EventMetric::Record.
  auto id = EventIdForMetric(mEvent.mId);
  if (id) {
    Telemetry::RecordEvent(id.extract(), Nothing(),
                           telExtras.IsEmpty() ? Nothing() : Some(telExtras));
  }

  // Calling the implementation directly, because we have a `string->string`
  // map, not a `T->string` map the C++ API expects.
  impl::fog_event_record(mEvent.mId, &extraKeys, &extraValues);
  return NS_OK;
}

NS_IMETHODIMP
GleanEvent::TestGetValue(const nsACString& aStorageName, JSContext* aCx,
                         JS::MutableHandleValue aResult) {
  auto resEvents = mEvent.TestGetValue(aStorageName);
  if (resEvents.isErr()) {
    aResult.set(JS::UndefinedValue());
    LogToBrowserConsole(nsIScriptError::errorFlag,
                        NS_ConvertUTF8toUTF16(resEvents.unwrapErr()));
    return NS_ERROR_LOSS_OF_SIGNIFICANT_DATA;
  }
  auto optEvents = resEvents.unwrap();
  if (optEvents.isNothing()) {
    aResult.set(JS::UndefinedValue());
    return NS_OK;
  }

  auto events = optEvents.extract();

  auto count = events.Length();
  JS::RootedObject eventArray(aCx, JS::NewArrayObject(aCx, count));
  if (NS_WARN_IF(!eventArray)) {
    return NS_ERROR_FAILURE;
  }

  for (size_t i = 0; i < count; i++) {
    auto* value = &events[i];

    JS::RootedObject eventObj(aCx, JS_NewPlainObject(aCx));
    if (NS_WARN_IF(!eventObj)) {
      return NS_ERROR_FAILURE;
    }

    if (!JS_DefineProperty(aCx, eventObj, "timestamp",
                           (double)value->mTimestamp, JSPROP_ENUMERATE)) {
      NS_WARNING("Failed to define timestamp for event object.");
      return NS_ERROR_FAILURE;
    }

    JS::Rooted<JS::Value> catStr(aCx);
    if (!dom::ToJSValue(aCx, value->mCategory, &catStr) ||
        !JS_DefineProperty(aCx, eventObj, "category", catStr,
                           JSPROP_ENUMERATE)) {
      NS_WARNING("Failed to define category for event object.");
      return NS_ERROR_FAILURE;
    }
    JS::Rooted<JS::Value> nameStr(aCx);
    if (!dom::ToJSValue(aCx, value->mName, &nameStr) ||
        !JS_DefineProperty(aCx, eventObj, "name", nameStr, JSPROP_ENUMERATE)) {
      NS_WARNING("Failed to define name for event object.");
      return NS_ERROR_FAILURE;
    }

    JS::RootedObject extraObj(aCx, JS_NewPlainObject(aCx));
    if (!JS_DefineProperty(aCx, eventObj, "extra", extraObj,
                           JSPROP_ENUMERATE)) {
      NS_WARNING("Failed to define extra for event object.");
      return NS_ERROR_FAILURE;
    }

    for (auto pair : value->mExtra) {
      auto key = mozilla::Get<0>(pair);
      auto val = mozilla::Get<1>(pair);
      JS::Rooted<JS::Value> valStr(aCx);
      if (!dom::ToJSValue(aCx, val, &valStr) ||
          !JS_DefineProperty(aCx, extraObj, key.Data(), valStr,
                             JSPROP_ENUMERATE)) {
        NS_WARNING("Failed to define extra property for event object.");
        return NS_ERROR_FAILURE;
      }
    }

    if (!JS_DefineElement(aCx, eventArray, i, eventObj, JSPROP_ENUMERATE)) {
      NS_WARNING("Failed to define item in events array.");
      return NS_ERROR_FAILURE;
    }
  }

  aResult.setObject(*eventArray);
  return NS_OK;
}

}  // namespace mozilla::glean
