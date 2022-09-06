/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_glean_Labeled_h
#define mozilla_glean_Labeled_h

#include "nsIGleanMetrics.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/glean/fog_ffi_generated.h"

namespace mozilla::glean {

namespace impl {

template <typename T>
class Labeled {
 public:
  constexpr explicit Labeled<T>(uint32_t id) : mId(id) {}

  /**
   * Gets a specific metric for a given label.
   *
   * If a set of acceptable labels were specified in the `metrics.yaml` file,
   * and the given label is not in the set, it will be recorded under the
   * special `OTHER_LABEL` label.
   *
   * If a set of acceptable labels was not specified in the `metrics.yaml` file,
   * only the first 16 unique labels will be used.
   * After that, any additional labels will be recorded under the special
   * `OTHER_LABEL` label.
   *
   * @param aLabel - a snake_case string under 30 characters in length,
   *                 otherwise the metric will be recorded under the special
   *                 `OTHER_LABEL` label and an error will be recorded.
   */
  T Get(const nsACString& aLabel) const;

 private:
  const uint32_t mId;
};

}  // namespace impl

class GleanLabeled final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(GleanLabeled)

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;
  nsISupports* GetParentObject() { return nullptr; }

  explicit GleanLabeled(uint32_t aId, uint32_t aTypeId)
      : mId(aId), mTypeId(aTypeId){};

  already_AddRefed<nsISupports> NamedGetter(const nsAString& aName,
                                            bool& aFound);
  bool NameIsEnumerable(const nsAString& aName);
  void GetSupportedNames(nsTArray<nsString>& aNames);

 private:
  virtual ~GleanLabeled() = default;

  const uint32_t mId;
  const uint32_t mTypeId;
};

}  // namespace mozilla::glean

#endif /* mozilla_glean_Labeled_h */
