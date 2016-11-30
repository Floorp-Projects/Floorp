/* -*- Mode: C++; tab-width: 2; indent-tabs-mode:nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MozIntl.h"
#include "jswrapper.h"
#include "mozilla/ModuleUtils.h"

#define MOZ_MOZINTL_CID \
  { 0x83f8f991, 0x6b81, 0x4dd8, { 0xa0, 0x93, 0x72, 0x0b, 0xfb, 0x67, 0x4d, 0x38 } }

using namespace mozilla;

NS_IMPL_ISUPPORTS(MozIntl, mozIMozIntl)

MozIntl::MozIntl() = default;

MozIntl::~MozIntl() = default;

NS_IMETHODIMP
MozIntl::AddGetCalendarInfo(JS::Handle<JS::Value> val, JSContext* cx)
{
  if (!val.isObject()) {
    return NS_ERROR_INVALID_ARG;
  }

  JS::Rooted<JSObject*> realIntlObj(cx, js::CheckedUnwrap(&val.toObject()));
  if (!realIntlObj) {
    return NS_ERROR_INVALID_ARG;
  }

  JSAutoCompartment ac(cx, realIntlObj);

  static const JSFunctionSpec funcs[] = {
    JS_SELF_HOSTED_FN("getCalendarInfo", "Intl_getCalendarInfo", 1, 0),
    JS_FS_END
  };

  if (!JS_DefineFunctions(cx, realIntlObj, funcs)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
MozIntl::AddGetDisplayNames(JS::Handle<JS::Value> val, JSContext* cx)
{
  if (!val.isObject()) {
    return NS_ERROR_INVALID_ARG;
  }

  JS::Rooted<JSObject*> realIntlObj(cx, js::CheckedUnwrap(&val.toObject()));
  if (!realIntlObj) {
    return NS_ERROR_INVALID_ARG;
  }

  JSAutoCompartment ac(cx, realIntlObj);

  static const JSFunctionSpec funcs[] = {
    JS_SELF_HOSTED_FN("getDisplayNames", "Intl_getDisplayNames", 2, 0),
    JS_FS_END
  };

  if (!JS_DefineFunctions(cx, realIntlObj, funcs)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_GENERIC_FACTORY_CONSTRUCTOR(MozIntl)
NS_DEFINE_NAMED_CID(MOZ_MOZINTL_CID);

static const Module::CIDEntry kMozIntlCIDs[] = {
  { &kMOZ_MOZINTL_CID, false, nullptr, MozIntlConstructor },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kMozIntlContracts[] = {
  { "@mozilla.org/mozintl;1", &kMOZ_MOZINTL_CID },
  { nullptr }
};

static const mozilla::Module kMozIntlModule = {
  mozilla::Module::kVersion,
  kMozIntlCIDs,
  kMozIntlContracts,
  nullptr,
  nullptr,
  nullptr,
  nullptr
};

NSMODULE_DEFN(mozMozIntlModule) = &kMozIntlModule;
