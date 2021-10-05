/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_extensions_ExtensionBrowser_h
#define mozilla_extensions_ExtensionBrowser_h

#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"

class nsIGlobalObject;

namespace mozilla {

class ErrorResult;

namespace extensions {

class ExtensionMockAPI;

bool ExtensionAPIAllowed(JSContext* aCx, JSObject* aGlobal);

class ExtensionBrowser final : public nsISupports, public nsWrapperCache {
  nsCOMPtr<nsIGlobalObject> mGlobal;
  RefPtr<ExtensionMockAPI> mExtensionMockAPI;

  ~ExtensionBrowser() = default;

 public:
  explicit ExtensionBrowser(nsIGlobalObject* aGlobal);

  // nsWrapperCache interface methods
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // DOM bindings methods

  nsIGlobalObject* GetParentObject() const;

  ExtensionMockAPI* GetExtensionMockAPI();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(ExtensionBrowser)
};

}  // namespace extensions
}  // namespace mozilla

#endif  // mozilla_extensions_ExtensionBrowser_h
