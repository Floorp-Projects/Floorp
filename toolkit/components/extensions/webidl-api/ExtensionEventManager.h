/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_extensions_ExtensionEventManager_h
#define mozilla_extensions_ExtensionEventManager_h

#include "js/TypeDecls.h"  // for JS::Handle, JSContext, JSObject, ...
#include "mozilla/Attributes.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/ErrorResult.h"
#include "nsCycleCollectionParticipant.h"
#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"

#include "ExtensionAPIBase.h"

class nsIGlobalObject;

namespace mozilla {

namespace dom {
class Function;
}

namespace extensions {

class ExtensionBrowser;

class ExtensionEventManager final : public nsISupports,
                                    public nsWrapperCache,
                                    public ExtensionAPIBase {
  nsCOMPtr<nsIGlobalObject> mGlobal;
  nsString mAPINamespace;
  nsString mEventName;

  ~ExtensionEventManager() = default;

 public:
  ExtensionEventManager(nsIGlobalObject* aGlobal, const nsAString& aNamespace,
                        const nsAString& aEventName);

  // nsWrapperCache interface methods
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;
  nsIGlobalObject* GetParentObject() const;

  bool HasListener(dom::Function& aCallback, ErrorResult& aRv) const;
  bool HasListeners(ErrorResult& aRv) const;

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(ExtensionEventManager)
};

}  // namespace extensions
}  // namespace mozilla

#endif  // mozilla_extensions_ExtensionEventManager_h
