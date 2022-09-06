/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_extensions_ExtensionMockAPI_h
#define mozilla_extensions_ExtensionMockAPI_h

#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsCycleCollectionParticipant.h"
#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"

#include "ExtensionAPIBase.h"
#include "ExtensionBrowser.h"

class nsIGlobalObject;

namespace mozilla {

namespace extensions {

using dom::DOMString;

class ExtensionEventManager;
class ExtensionPort;

class ExtensionMockAPI final : public nsISupports,
                               public nsWrapperCache,
                               public ExtensionAPINamespace {
  nsCOMPtr<nsIGlobalObject> mGlobal;
  RefPtr<ExtensionBrowser> mExtensionBrowser;
  RefPtr<ExtensionEventManager> mOnTestEventMgr;

  ~ExtensionMockAPI() = default;

 protected:
  // ExtensionAPIBase methods
  nsIGlobalObject* GetGlobalObject() const override { return mGlobal; }

  ExtensionBrowser* GetExtensionBrowser() const override {
    return mExtensionBrowser;
  }

  nsString GetAPINamespace() const override { return u"mockExtensionAPI"_ns; }

 public:
  ExtensionMockAPI(nsIGlobalObject* aGlobal,
                   ExtensionBrowser* aExtensionBrowser);

  // nsWrapperCache interface methods
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // DOM bindings methods
  static bool IsAllowed(JSContext* aCx, JSObject* aGlobal);

  nsIGlobalObject* GetParentObject() const;

  void GetPropertyAsErrorObject(JSContext* aCx,
                                JS::MutableHandle<JS::Value> aRetval);
  void GetPropertyAsString(DOMString& aRetval);

  ExtensionEventManager* OnTestEvent();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(ExtensionMockAPI)
};

}  // namespace extensions
}  // namespace mozilla

#endif  // mozilla_extensions_ExtensionMockAPI_h
