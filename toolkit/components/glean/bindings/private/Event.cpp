/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/glean/bindings/Event.h"

#include "nsString.h"
#include "mozilla/dom/GleanMetricsBinding.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/glean/bindings/Common.h"
#include "jsapi.h"
#include "js/PropertyAndElement.h"  // JS_DefineElement, JS_DefineProperty, JS_Enumerate, JS_GetProperty, JS_GetPropertyById

namespace mozilla::glean {

/* virtual */
JSObject* GleanEvent::WrapObject(JSContext* aCx,
                                 JS::Handle<JSObject*> aGivenProto) {
  return dom::GleanEvent_Binding::Wrap(aCx, this, aGivenProto);
}

// Convert all capital letters to "_x" where "x" is the corresponding lowercase.
nsCString camelToSnake(const nsACString& aCamel) {
  nsCString snake;
  const auto* start = aCamel.BeginReading();
  const auto* end = aCamel.EndReading();
  for (; start != end; ++start) {
    if ('A' <= *start && *start <= 'Z') {
      snake.AppendLiteral("_");
      snake.Append(static_cast<char>(std::tolower(*start)));
    } else {
      snake.Append(*start);
    }
  }
  return snake;
}

void GleanEvent::Record(
    const dom::Optional<dom::Record<nsCString, nsCString>>& aExtra) {
  if (!aExtra.WasPassed()) {
    mEvent.Record();
    return;
  }

  nsTArray<nsCString> extraKeys;
  nsTArray<nsCString> extraValues;
  CopyableTArray<Telemetry::EventExtraEntry> telExtras;
  for (const auto& entry : aExtra.Value().Entries()) {
    if (entry.mValue.IsVoid()) {
      // Someone passed undefined/null for this value.
      // Pretend it wasn't here.
      continue;
    }
    // We accept camelCase extra keys, but Glean requires snake_case.
    auto snakeKey = camelToSnake(entry.mKey);

    extraKeys.AppendElement(snakeKey);
    extraValues.AppendElement(entry.mValue);
    telExtras.EmplaceBack(Telemetry::EventExtraEntry{entry.mKey, entry.mValue});
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
}

void GleanEvent::TestGetValue(
    const nsACString& aPingName,
    dom::Nullable<nsTArray<dom::GleanEventRecord>>& aResult, ErrorResult& aRv) {
  auto resEvents = mEvent.TestGetValue(aPingName);
  if (resEvents.isErr()) {
    aRv.ThrowDataError(resEvents.unwrapErr());
    return;
  }
  auto optEvents = resEvents.unwrap();
  if (optEvents.isNothing()) {
    return;
  }

  nsTArray<dom::GleanEventRecord> ret;
  for (auto& event : optEvents.extract()) {
    dom::GleanEventRecord record;
    if (!event.mExtra.IsEmpty()) {
      record.mExtra.Construct();
      for (auto& extraEntry : event.mExtra) {
        dom::binding_detail::RecordEntry<nsCString, nsCString> extra;
        extra.mKey = std::get<0>(extraEntry);
        extra.mValue = std::get<1>(extraEntry);
        record.mExtra.Value().Entries().EmplaceBack(std::move(extra));
      }
    }
    record.mCategory = event.mCategory;
    record.mName = event.mName;
    record.mTimestamp = event.mTimestamp;
    ret.EmplaceBack(std::move(record));
  }
  aResult.SetValue(std::move(ret));
}

}  // namespace mozilla::glean
