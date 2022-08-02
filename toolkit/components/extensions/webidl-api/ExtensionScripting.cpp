/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ExtensionScripting.h"
#include "ExtensionEventManager.h"

#include "mozilla/dom/ExtensionScriptingBinding.h"
#include "nsIGlobalObject.h"

namespace mozilla::extensions {

NS_IMPL_CYCLE_COLLECTING_ADDREF(ExtensionScripting);
NS_IMPL_CYCLE_COLLECTING_RELEASE(ExtensionScripting)
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(ExtensionScripting, mGlobal,
                                      mExtensionBrowser);

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ExtensionScripting)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

ExtensionScripting::ExtensionScripting(nsIGlobalObject* aGlobal,
                                       ExtensionBrowser* aExtensionBrowser)
    : mGlobal(aGlobal), mExtensionBrowser(aExtensionBrowser) {
  MOZ_DIAGNOSTIC_ASSERT(mGlobal);
  MOZ_DIAGNOSTIC_ASSERT(mExtensionBrowser);
}

/* static */
bool ExtensionScripting::IsAllowed(JSContext* aCx, JSObject* aGlobal) {
  return true;
}

JSObject* ExtensionScripting::WrapObject(JSContext* aCx,
                                         JS::Handle<JSObject*> aGivenProto) {
  return dom::ExtensionScripting_Binding::Wrap(aCx, this, aGivenProto);
}

nsIGlobalObject* ExtensionScripting::GetParentObject() const { return mGlobal; }

}  // namespace mozilla::extensions
