/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ExtensionEventManager.h"

#include "mozilla/dom/ExtensionEventManagerBinding.h"
#include "nsIGlobalObject.h"

namespace mozilla {
namespace extensions {

NS_IMPL_CYCLE_COLLECTING_ADDREF(ExtensionEventManager);
NS_IMPL_CYCLE_COLLECTING_RELEASE(ExtensionEventManager)
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(ExtensionEventManager, mGlobal);

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ExtensionEventManager)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

ExtensionEventManager::ExtensionEventManager(nsIGlobalObject* aGlobal,
                                             const nsAString& aNamespace,
                                             const nsAString& aEventName,
                                             const nsAString& aObjectType,
                                             const nsAString& aObjectId)
    : mGlobal(aGlobal),
      mAPINamespace(aNamespace),
      mEventName(aEventName),
      mAPIObjectType(aObjectType),
      mAPIObjectId(aObjectId) {
  MOZ_DIAGNOSTIC_ASSERT(mGlobal);
}

JSObject* ExtensionEventManager::WrapObject(JSContext* aCx,
                                            JS::Handle<JSObject*> aGivenProto) {
  return dom::ExtensionEventManager_Binding::Wrap(aCx, this, aGivenProto);
}

nsIGlobalObject* ExtensionEventManager::GetParentObject() const {
  return mGlobal;
}

bool ExtensionEventManager::HasListener(dom::Function& aCallback,
                                        ErrorResult& aRv) const {
  return false;
}

bool ExtensionEventManager::HasListeners(ErrorResult& aRv) const {
  return false;
}

}  // namespace extensions
}  // namespace mozilla
