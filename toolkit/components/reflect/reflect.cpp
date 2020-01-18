/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "reflect.h"
#include "jsapi.h"
#include "nsMemory.h"
#include "nsString.h"
#include "nsNativeCharsetUtils.h"
#include "xpc_make_class.h"

namespace mozilla::reflect {

NS_IMPL_ISUPPORTS(Module, nsIXPCScriptable)

Module::Module() = default;

Module::~Module() = default;

#define XPC_MAP_CLASSNAME Module
#define XPC_MAP_QUOTED_CLASSNAME "Module"
#define XPC_MAP_FLAGS XPC_SCRIPTABLE_WANT_CALL
#include "xpc_map_end.h"

NS_IMETHODIMP
Module::Call(nsIXPConnectWrappedNative* wrapper, JSContext* cx, JSObject* obj,
             const JS::CallArgs& args, bool* _retval) {
  JS::Rooted<JSObject*> global(cx, JS::GetScriptedCallerGlobal(cx));
  if (!global) return NS_ERROR_NOT_AVAILABLE;

  JSAutoRealm ar(cx, global);
  *_retval = JS_InitReflectParse(cx, global);
  return NS_OK;
}

}  // namespace mozilla::reflect
