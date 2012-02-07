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
 * Portions created by the Initial Developer are Copyright (C) 2009-2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Dave Camp <dcamp@mozilla.com>
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

#include "nsJSInspector.h"
#include "nsIXPConnect.h"
#include "nsIJSContextStack.h"
#include "nsThreadUtils.h"
#include "jsapi.h"
#include "jsgc.h"
#include "jsfriendapi.h"
#include "jsdbgapi.h"
#include "mozilla/ModuleUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsMemory.h"

#define JSINSPECTOR_CONTRACTID \
  "@mozilla.org/jsinspector;1"

#define JSINSPECTOR_CID \
{ 0xec5aa99c, 0x7abb, 0x4142, { 0xac, 0x5f, 0xaa, 0xb2, 0x41, 0x9e, 0x38, 0xe2 } }

namespace mozilla {
namespace jsinspector {

NS_GENERIC_FACTORY_CONSTRUCTOR(nsJSInspector)

NS_IMPL_ISUPPORTS1(nsJSInspector, nsIJSInspector)

nsJSInspector::nsJSInspector() : mNestedLoopLevel(0)
{
}

nsJSInspector::~nsJSInspector()
{
}

NS_IMETHODIMP
nsJSInspector::EnterNestedEventLoop(PRUint32 *out)
{
  nsresult rv;
  nsCOMPtr<nsIJSContextStack> stack =
    do_GetService("@mozilla.org/js/xpc/ContextStack;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 nestLevel = ++mNestedLoopLevel;
  if (NS_SUCCEEDED(stack->Push(nsnull))) {
    while (NS_SUCCEEDED(rv) && mNestedLoopLevel >= nestLevel) {
      if (!NS_ProcessNextEvent())
        rv = NS_ERROR_UNEXPECTED;
    }

    JSContext *cx;
    stack->Pop(&cx);
    NS_ASSERTION(cx == nsnull, "JSContextStack mismatch");
  } else {
    rv = NS_ERROR_FAILURE;
  }

  NS_ASSERTION(mNestedLoopLevel <= nestLevel,
               "nested event didn't unwind properly");

  if (mNestedLoopLevel == nestLevel)
    --mNestedLoopLevel;

  *out = mNestedLoopLevel;
  return rv;
}

NS_IMETHODIMP
nsJSInspector::ExitNestedEventLoop(PRUint32 *out)
{
  if (mNestedLoopLevel > 0) {
    --mNestedLoopLevel;
  } else {
    return NS_ERROR_FAILURE;
  }

  *out = mNestedLoopLevel;

  return NS_OK;
}

NS_IMETHODIMP
nsJSInspector::GetEventLoopNestLevel(PRUint32 *out)
{
  *out = mNestedLoopLevel;
  return NS_OK;
}

}
}

NS_DEFINE_NAMED_CID(JSINSPECTOR_CID);

static const mozilla::Module::CIDEntry kJSInspectorCIDs[] = {
  { &kJSINSPECTOR_CID, false, NULL, mozilla::jsinspector::nsJSInspectorConstructor },
  { NULL }
};

static const mozilla::Module::ContractIDEntry kJSInspectorContracts[] = {
  { JSINSPECTOR_CONTRACTID, &kJSINSPECTOR_CID },
  { NULL }
};

static const mozilla::Module kJSInspectorModule = {
  mozilla::Module::kVersion,
  kJSInspectorCIDs,
  kJSInspectorContracts
};

NSMODULE_DEFN(jsinspector) = &kJSInspectorModule;
