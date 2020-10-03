/* -*- Mode: C++; tab-width: 2; indent-tabs-mode:nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MozIntlHelper.h"
#include "jsapi.h"
#include "js/PropertySpec.h"
#include "js/Wrapper.h"

using namespace mozilla;

NS_IMPL_ISUPPORTS(MozIntlHelper, mozIMozIntlHelper)

MozIntlHelper::MozIntlHelper() = default;

MozIntlHelper::~MozIntlHelper() = default;

static nsresult AddFunctions(JSContext* cx, JS::Handle<JS::Value> val,
                             const JSFunctionSpec* funcs) {
  if (!val.isObject()) {
    return NS_ERROR_INVALID_ARG;
  }

  // We might be adding functions to a Window.
  JS::Rooted<JSObject*> realIntlObj(
      cx, js::CheckedUnwrapDynamic(&val.toObject(), cx));
  if (!realIntlObj) {
    return NS_ERROR_INVALID_ARG;
  }

  JSAutoRealm ar(cx, realIntlObj);

  if (!JS_DefineFunctions(cx, realIntlObj, funcs)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
MozIntlHelper::AddGetCalendarInfo(JS::Handle<JS::Value> val, JSContext* cx) {
  static const JSFunctionSpec funcs[] = {
      JS_SELF_HOSTED_FN("getCalendarInfo", "Intl_getCalendarInfo", 1, 0),
      JS_FS_END};

  return AddFunctions(cx, val, funcs);
}

NS_IMETHODIMP
MozIntlHelper::AddGetDisplayNames(JS::Handle<JS::Value> val, JSContext* cx) {
  static const JSFunctionSpec funcs[] = {
      JS_SELF_HOSTED_FN("getDisplayNames", "Intl_getDisplayNames", 2, 0),
      JS_FS_END};

  return AddFunctions(cx, val, funcs);
}

NS_IMETHODIMP
MozIntlHelper::AddDateTimeFormatConstructor(JS::Handle<JS::Value> val,
                                            JSContext* cx) {
  if (!val.isObject()) {
    return NS_ERROR_INVALID_ARG;
  }

  // We might be adding this constructor to a Window
  JS::Rooted<JSObject*> realIntlObj(
      cx, js::CheckedUnwrapDynamic(&val.toObject(), cx));
  if (!realIntlObj) {
    return NS_ERROR_INVALID_ARG;
  }

  JSAutoRealm ar(cx, realIntlObj);

  if (!js::AddMozDateTimeFormatConstructor(cx, realIntlObj)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
MozIntlHelper::AddGetLocaleInfo(JS::Handle<JS::Value> val, JSContext* cx) {
  static const JSFunctionSpec funcs[] = {
      JS_SELF_HOSTED_FN("getLocaleInfo", "Intl_getLocaleInfo", 1, 0),
      JS_FS_END};

  return AddFunctions(cx, val, funcs);
}
