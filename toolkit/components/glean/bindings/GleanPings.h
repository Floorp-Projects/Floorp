/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_glean_GleanPings_h
#define mozilla_glean_GleanPings_h

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/glean/bindings/Ping.h"
#include "nsGlobalWindowInner.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"

namespace mozilla::glean {

class GleanPings final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(GleanPings)

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;
  nsISupports* GetParentObject() { return nullptr; }

  static bool DefineGleanPings(JSContext* aCx, JS::Handle<JSObject*> aGlobal);

  already_AddRefed<GleanPing> NamedGetter(const nsAString& aName, bool& aFound);
  bool NameIsEnumerable(const nsAString& aName);
  void GetSupportedNames(nsTArray<nsString>& aNames);

 protected:
  virtual ~GleanPings() = default;
};

}  // namespace mozilla::glean

#endif /* mozilla_glean_GleanPings */
