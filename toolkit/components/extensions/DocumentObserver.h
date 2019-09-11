/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_extensions_DocumentObserver_h
#define mozilla_extensions_DocumentObserver_h

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/MozDocumentObserverBinding.h"

#include "mozilla/extensions/WebExtensionContentScript.h"

class nsILoadInfo;
class nsPIDOMWindowOuter;

namespace mozilla {
namespace extensions {

class DocumentObserver final : public nsISupports, public nsWrapperCache {
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(DocumentObserver)

  static already_AddRefed<DocumentObserver> Constructor(
      dom::GlobalObject& aGlobal, dom::MozDocumentCallback& aCallbacks);

  void Observe(const dom::Sequence<OwningNonNull<MozDocumentMatcher>>& matchers,
               ErrorResult& aRv);

  void Disconnect();

  const nsTArray<RefPtr<MozDocumentMatcher>>& Matchers() const {
    return mMatchers;
  }

  void NotifyMatch(MozDocumentMatcher& aMatcher, nsPIDOMWindowOuter* aWindow);
  void NotifyMatch(MozDocumentMatcher& aMatcher, nsILoadInfo* aLoadInfo);

  nsISupports* GetParentObject() const { return mParent; }
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

 protected:
  virtual ~DocumentObserver() = default;

 private:
  explicit DocumentObserver(nsISupports* aParent,
                            dom::MozDocumentCallback& aCallbacks)
      : mParent(aParent), mCallbacks(&aCallbacks) {}

  nsCOMPtr<nsISupports> mParent;
  RefPtr<dom::MozDocumentCallback> mCallbacks;
  nsTArray<RefPtr<MozDocumentMatcher>> mMatchers;
};

}  // namespace extensions
}  // namespace mozilla

#endif  // mozilla_extensions_DocumentObserver_h
