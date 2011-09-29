/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation <http://www.mozilla.org/>.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Zack Weinberg <zweinberg@mozilla.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
  JSObject* scope = JS_GetScopeChain(cx);
  if (!scope)
    return NS_ERROR_NOT_AVAILABLE;

  JSObject* global = JS_GetGlobalForObject(cx, scope);
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
