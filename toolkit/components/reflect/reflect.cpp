/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "reflect.h"
#include "jsapi.h"
#include "mozilla/ModuleUtils.h"
#include "nsMemory.h"
#include "nsString.h"
#include "nsNativeCharsetUtils.h"

#define JSREFLECT_CONTRACTID \
  "@mozilla.org/jsreflect;1"

#define JSREFLECT_CID \
{ 0x1a817186, 0x357a, 0x47cd, { 0x8a, 0xea, 0x28, 0x50, 0xd6, 0x0e, 0x95, 0x9e } }

namespace mozilla {
namespace reflect {

NS_GENERIC_FACTORY_CONSTRUCTOR(Module)

NS_IMPL_ISUPPORTS1(Module, nsIXPCScriptable)

Module::Module()
{
}

Module::~Module()
{
}

#define XPC_MAP_CLASSNAME Module
#define XPC_MAP_QUOTED_CLASSNAME "Module"
#define XPC_MAP_WANT_CALL
#define XPC_MAP_FLAGS nsIXPCScriptable::WANT_CALL
#include "xpc_map_end.h"

NS_IMETHODIMP
Module::Call(nsIXPConnectWrappedNative* wrapper,
             JSContext* cx,
             JSObject* obj,
             const JS::CallArgs& args,
             bool* _retval)
{
  JSObject* global = JS::CurrentGlobalOrNull(cx);
  if (!global)
    return NS_ERROR_NOT_AVAILABLE;

  *_retval = !!JS_InitReflect(cx, global);
  return NS_OK;
}

}
}

NS_DEFINE_NAMED_CID(JSREFLECT_CID);

static const mozilla::Module::CIDEntry kReflectCIDs[] = {
  { &kJSREFLECT_CID, false, NULL, mozilla::reflect::ModuleConstructor },
  { NULL }
};

static const mozilla::Module::ContractIDEntry kReflectContracts[] = {
  { JSREFLECT_CONTRACTID, &kJSREFLECT_CID },
  { NULL }
};

static const mozilla::Module kReflectModule = {
  mozilla::Module::kVersion,
  kReflectCIDs,
  kReflectContracts
};

NSMODULE_DEFN(jsreflect) = &kReflectModule;
