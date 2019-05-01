/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebrtcTelemetry.h"

#include "jsapi.h"
#include "mozilla/Telemetry.h"
#include "nsPrintfCString.h"
#include "nsTHashtable.h"
void WebrtcTelemetry::RecordIceCandidateMask(const uint32_t iceCandidateBitmask,
                                             const bool success) {
  WebrtcIceCandidateType* entry =
      mWebrtcIceCandidates.GetEntry(iceCandidateBitmask);
  if (!entry) {
    entry = mWebrtcIceCandidates.PutEntry(iceCandidateBitmask);
    if (MOZ_UNLIKELY(!entry)) return;
  }

  if (success) {
    entry->mData.webrtc.successCount++;
  } else {
    entry->mData.webrtc.failureCount++;
  }
}

bool ReflectIceEntry(const WebrtcTelemetry::WebrtcIceCandidateType* entry,
                     const WebrtcTelemetry::WebrtcIceCandidateStats* stat,
                     JSContext* cx, JS::Handle<JSObject*> obj) {
  if ((stat->successCount == 0) && (stat->failureCount == 0)) return true;

  const uint32_t& bitmask = entry->GetKey();

  JS::Rooted<JSObject*> statsObj(cx, JS_NewPlainObject(cx));
  if (!statsObj) return false;
  if (!JS_DefineProperty(cx, obj,
                         nsPrintfCString("%" PRIu32, bitmask).BeginReading(),
                         statsObj, JSPROP_ENUMERATE)) {
    return false;
  }
  if (stat->successCount &&
      !JS_DefineProperty(cx, statsObj, "successCount", stat->successCount,
                         JSPROP_ENUMERATE)) {
    return false;
  }
  if (stat->failureCount &&
      !JS_DefineProperty(cx, statsObj, "failureCount", stat->failureCount,
                         JSPROP_ENUMERATE)) {
    return false;
  }
  return true;
}

bool ReflectIceWebrtc(WebrtcTelemetry::WebrtcIceCandidateType* entry,
                      JSContext* cx, JS::Handle<JSObject*> obj) {
  return ReflectIceEntry(entry, &entry->mData.webrtc, cx, obj);
}

bool WebrtcTelemetry::AddIceInfo(JSContext* cx, JS::Handle<JSObject*> iceObj) {
  JS::Rooted<JSObject*> statsObj(cx, JS_NewPlainObject(cx));
  if (!statsObj) return false;

  if (!mWebrtcIceCandidates.ReflectIntoJS(ReflectIceWebrtc, cx, statsObj)) {
    return false;
  }

  return JS_DefineProperty(cx, iceObj, "webrtc", statsObj, JSPROP_ENUMERATE);
}

bool WebrtcTelemetry::GetWebrtcStats(JSContext* cx,
                                     JS::MutableHandle<JS::Value> ret) {
  JS::Rooted<JSObject*> root_obj(cx, JS_NewPlainObject(cx));
  if (!root_obj) return false;
  ret.setObject(*root_obj);

  JS::Rooted<JSObject*> ice_obj(cx, JS_NewPlainObject(cx));
  if (!ice_obj) return false;
  JS_DefineProperty(cx, root_obj, "IceCandidatesStats", ice_obj,
                    JSPROP_ENUMERATE);

  if (!AddIceInfo(cx, ice_obj)) return false;

  return true;
}

size_t WebrtcTelemetry::SizeOfExcludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  return mWebrtcIceCandidates.ShallowSizeOfExcludingThis(aMallocSizeOf);
}
