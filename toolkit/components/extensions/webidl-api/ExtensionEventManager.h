/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_extensions_ExtensionEventManager_h
#define mozilla_extensions_ExtensionEventManager_h

#include "js/GCHashTable.h"  // for JS::GCHashMap
#include "js/TypeDecls.h"    // for JS::Handle, JSContext, JSObject, ...
#include "mozilla/Attributes.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/ErrorResult.h"
#include "nsCycleCollectionParticipant.h"
#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsPointerHashKeys.h"
#include "nsRefPtrHashtable.h"
#include "nsWrapperCache.h"

#include "ExtensionAPIBase.h"

class nsIGlobalObject;

namespace mozilla {

namespace dom {
class Function;
}  // namespace dom

namespace extensions {

class ExtensionBrowser;
class ExtensionEventListener;

class ExtensionEventManager final : public nsISupports,
                                    public nsWrapperCache,
                                    public ExtensionAPIBase {
 public:
  ExtensionEventManager(nsIGlobalObject* aGlobal,
                        ExtensionBrowser* aExtensionBrowser,
                        const nsAString& aNamespace,
                        const nsAString& aEventName,
                        const nsAString& aObjectType = VoidString(),
                        const nsAString& aObjectId = VoidString());

  // nsWrapperCache interface methods
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;
  nsIGlobalObject* GetParentObject() const;

  bool HasListener(dom::Function& aCallback, ErrorResult& aRv) const;
  bool HasListeners(ErrorResult& aRv) const;

  void AddListener(JSContext* aCx, dom::Function& aCallback,
                   const dom::Optional<JS::Handle<JSObject*>>& aOptions,
                   ErrorResult& aRv);
  void RemoveListener(dom::Function& aCallback, ErrorResult& aRv);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(ExtensionEventManager)

 protected:
  // ExtensionAPIBase methods
  nsIGlobalObject* GetGlobalObject() const override { return mGlobal; }
  ExtensionBrowser* GetExtensionBrowser() const override {
    return mExtensionBrowser;
  }

  nsString GetAPINamespace() const override { return mAPINamespace; }

  nsString GetAPIObjectType() const override { return mAPIObjectType; }

  nsString GetAPIObjectId() const override { return mAPIObjectId; }

 private:
  using ListenerWrappersMap =
      JS::GCHashMap<JS::Heap<JSObject*>, RefPtr<ExtensionEventListener>,
                    js::MovableCellHasher<JS::Heap<JSObject*>>,
                    js::SystemAllocPolicy>;

  ~ExtensionEventManager();

  void ReleaseListeners();

  nsCOMPtr<nsIGlobalObject> mGlobal;
  RefPtr<ExtensionBrowser> mExtensionBrowser;
  nsString mAPINamespace;
  nsString mEventName;
  nsString mAPIObjectType;
  nsString mAPIObjectId;
  ListenerWrappersMap mListeners;
};

}  // namespace extensions
}  // namespace mozilla

#endif  // mozilla_extensions_ExtensionEventManager_h
