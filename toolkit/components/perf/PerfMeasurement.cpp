/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PerfMeasurement.h"
#include "jsperf.h"
#include "mozilla/ModuleUtils.h"
#include "nsMemory.h"

#define JSPERF_CONTRACTID \
  "@mozilla.org/jsperf;1"

#define JSPERF_CID            \
{ 0x421c38e6, 0xaee0, 0x4509, \
  { 0xa0, 0x25, 0x13, 0x0f, 0x43, 0x78, 0x03, 0x5a } }

namespace mozilla {
namespace jsperf {

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

static JSBool
SealObjectAndPrototype(JSContext* cx, JSObject* parent, const char* name)
{
  jsval prop;
  if (!JS_GetProperty(cx, parent, name, &prop))
    return false;

  JSObject* obj = JSVAL_TO_OBJECT(prop);
  if (!JS_GetProperty(cx, obj, "prototype", &prop))
    return false;

  JSObject* prototype = JSVAL_TO_OBJECT(prop);
  return JS_FreezeObject(cx, obj) && JS_FreezeObject(cx, prototype);
}

static JSBool
InitAndSealPerfMeasurementClass(JSContext* cx, JSObject* global)
{
  // Init the PerfMeasurement class
  if (!JS::RegisterPerfMeasurement(cx, global))
    return false;

  // Seal up Object, Function, and Array and their prototypes.  (This single
  // object instance is shared amongst everyone who imports the jsperf module.)
  if (!SealObjectAndPrototype(cx, global, "Object") ||
      !SealObjectAndPrototype(cx, global, "Function") ||
      !SealObjectAndPrototype(cx, global, "Array"))
    return false;

  // Finally, seal the global object, for good measure. (But not recursively;
  // this breaks things.)
  return JS_FreezeObject(cx, global);
}

NS_IMETHODIMP
Module::Call(nsIXPConnectWrappedNative* wrapper,
             JSContext* cx,
             JSObject* obj,
             PRUint32 argc,
             jsval* argv,
             jsval* vp,
             bool* _retval)
{
  JSObject* global = JS_GetGlobalForScopeChain(cx);
  if (!global)
    return NS_ERROR_NOT_AVAILABLE;

  *_retval = InitAndSealPerfMeasurementClass(cx, global);
  return NS_OK;
}

}
}

NS_DEFINE_NAMED_CID(JSPERF_CID);

static const mozilla::Module::CIDEntry kPerfCIDs[] = {
  { &kJSPERF_CID, false, NULL, mozilla::jsperf::ModuleConstructor },
  { NULL }
};

static const mozilla::Module::ContractIDEntry kPerfContracts[] = {
  { JSPERF_CONTRACTID, &kJSPERF_CID },
  { NULL }
};

static const mozilla::Module kPerfModule = {
  mozilla::Module::kVersion,
  kPerfCIDs,
  kPerfContracts
};

NSMODULE_DEFN(jsperf) = &kPerfModule;
