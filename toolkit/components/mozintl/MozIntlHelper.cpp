/* -*- Mode: C++; tab-width: 2; indent-tabs-mode:nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MozIntlHelper.h"
#include "js/Wrapper.h"
#include "mozilla/ModuleUtils.h"

#define MOZ_MOZINTLHELPER_CID \
  { 0xb43c96be, 0x2b3a, 0x4dc4, { 0x90, 0xe9, 0xb0, 0x6d, 0x34, 0x21, 0x9b, 0x68 } }

using namespace mozilla;

NS_IMPL_ISUPPORTS(MozIntlHelper, mozIMozIntlHelper)

MozIntlHelper::MozIntlHelper() = default;

MozIntlHelper::~MozIntlHelper() = default;

static nsresult
AddFunctions(JSContext* cx, JS::Handle<JS::Value> val, const JSFunctionSpec* funcs)
{
  if (!val.isObject()) {
    return NS_ERROR_INVALID_ARG;
  }

  JS::Rooted<JSObject*> realIntlObj(cx, js::CheckedUnwrap(&val.toObject()));
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
MozIntlHelper::AddGetCalendarInfo(JS::Handle<JS::Value> val, JSContext* cx)
{
  static const JSFunctionSpec funcs[] = {
    JS_SELF_HOSTED_FN("getCalendarInfo", "Intl_getCalendarInfo", 1, 0),
    JS_FS_END
  };

  return AddFunctions(cx, val, funcs);
}

NS_IMETHODIMP
MozIntlHelper::AddGetDisplayNames(JS::Handle<JS::Value> val, JSContext* cx)
{
  static const JSFunctionSpec funcs[] = {
    JS_SELF_HOSTED_FN("getDisplayNames", "Intl_getDisplayNames", 2, 0),
    JS_FS_END
  };

  return AddFunctions(cx, val, funcs);
}

NS_IMETHODIMP
MozIntlHelper::AddDateTimeFormatConstructor(JS::Handle<JS::Value> val, JSContext* cx)
{
  if (!val.isObject()) {
    return NS_ERROR_INVALID_ARG;
  }

  JS::Rooted<JSObject*> realIntlObj(cx, js::CheckedUnwrap(&val.toObject()));
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
MozIntlHelper::AddRelativeTimeFormatConstructor(JS::Handle<JS::Value> val, JSContext* cx)
{
  if (!val.isObject()) {
    return NS_ERROR_INVALID_ARG;
  }

  JS::Rooted<JSObject*> realIntlObj(cx, js::CheckedUnwrap(&val.toObject()));
  if (!realIntlObj) {
    return NS_ERROR_INVALID_ARG;
  }

  JSAutoRealm ar(cx, realIntlObj);

  if (!js::AddRelativeTimeFormatConstructor(cx, realIntlObj)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
MozIntlHelper::AddGetLocaleInfo(JS::Handle<JS::Value> val, JSContext* cx)
{
  static const JSFunctionSpec funcs[] = {
    JS_SELF_HOSTED_FN("getLocaleInfo", "Intl_getLocaleInfo", 1, 0),
    JS_FS_END
  };

  return AddFunctions(cx, val, funcs);
}

NS_GENERIC_FACTORY_CONSTRUCTOR(MozIntlHelper)
NS_DEFINE_NAMED_CID(MOZ_MOZINTLHELPER_CID);

static const Module::CIDEntry kMozIntlHelperCIDs[] = {
  { &kMOZ_MOZINTLHELPER_CID, false, nullptr, MozIntlHelperConstructor },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kMozIntlHelperContracts[] = {
  { "@mozilla.org/mozintlhelper;1", &kMOZ_MOZINTLHELPER_CID },
  { nullptr }
};

static const mozilla::Module kMozIntlHelperModule = {
  mozilla::Module::kVersion,
  kMozIntlHelperCIDs,
  kMozIntlHelperContracts,
  nullptr,
  nullptr,
  nullptr,
  nullptr
};

NSMODULE_DEFN(mozMozIntlHelperModule) = &kMozIntlHelperModule;
