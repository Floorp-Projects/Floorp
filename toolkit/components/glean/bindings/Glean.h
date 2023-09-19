/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_glean_Glean_h
#define mozilla_glean_Glean_h

#include "js/TypeDecls.h"
#include "nsGlobalWindowInner.h"
#include "nsISupports.h"
#include "nsTArrayForwardDeclare.h"
#include "nsWrapperCache.h"

namespace mozilla::glean {

class Category;

class Glean final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(Glean)

  explicit Glean(nsIGlobalObject* aGlobal) : mParent(aGlobal) {}

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;
  nsISupports* GetParentObject() { return mParent; }

  static bool DefineGlean(JSContext* aCx, JS::Handle<JSObject*> aGlobal);

  already_AddRefed<Category> NamedGetter(const nsAString& aName, bool& aFound);
  bool NameIsEnumerable(const nsAString& aName);
  void GetSupportedNames(nsTArray<nsString>& aNames);

  /*
   * Test-only method.
   *
   * Set whether we should treat runtime-registered metrics as the
   * comprehensive list of all metrics, or whether compile-time-registered
   * metrics are allowed to count too.
   *
   * Allows us to test Artifact Build support flexibly.
   */
  static void TestSetRuntimeMetricsComprehensive(bool aIsComprehensive);

 private:
  nsCOMPtr<nsISupports> mParent;

 protected:
  virtual ~Glean() = default;
};

}  // namespace mozilla::glean

#endif /* mozilla_glean_Glean */
