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
 * The Original Code is js-reflect.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation <http://www.mozilla.org/>.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Dave Herman <dherman@mozilla.com>
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
             PRUint32 argc,
             jsval* argv,
             jsval* vp,
             bool* _retval)
{
  JSObject* global = JS_GetGlobalForScopeChain(cx);
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
