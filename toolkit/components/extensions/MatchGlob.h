/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_extensions_MatchGlob_h
#define mozilla_extensions_MatchGlob_h

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/MatchGlobBinding.h"

#include "jspubtd.h"
#include "js/RootingAPI.h"

#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace extensions {

class MatchPattern;

class MatchGlob final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(MatchGlob)

  static already_AddRefed<MatchGlob> Constructor(dom::GlobalObject& aGlobal,
                                                 const nsAString& aGlob,
                                                 bool aAllowQuestion,
                                                 ErrorResult& aRv);

  bool Matches(const nsAString& aString) const;

  bool IsWildcard() const { return mIsPrefix && mPathLiteral.IsEmpty(); }

  void GetGlob(nsAString& aGlob) const { aGlob = mGlob; }

  nsISupports* GetParentObject() const { return mParent; }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::HandleObject aGivenProto) override;

 protected:
  virtual ~MatchGlob();

 private:
  friend class MatchPattern;

  explicit MatchGlob(nsISupports* aParent) : mParent(aParent) {}

  void Init(JSContext* aCx, const nsAString& aGlob, bool aAllowQuestion,
            ErrorResult& aRv);

  nsCOMPtr<nsISupports> mParent;

  // The original glob string that this glob object represents.
  nsString mGlob;

  // The literal path string to match against. If this contains a non-void
  // value, the glob matches against this exact literal string, rather than
  // performng a pattern match. If mIsPrefix is true, the literal must appear
  // at the start of the matched string. If it is false, the the literal must
  // be exactly equal to the matched string.
  nsString mPathLiteral;
  bool mIsPrefix = false;

  // The regular expression object which is equivalent to this glob pattern.
  // Used for matching if, and only if, mPathLiteral is non-void.
  JS::Heap<JSObject*> mRegExp;
};

class MatchGlobSet final : public CopyableTArray<RefPtr<MatchGlob>> {
 public:
  // Note: We can't use the nsTArray constructors directly, since the static
  // analyzer doesn't handle their MOZ_IMPLICIT annotations correctly.
  MatchGlobSet() = default;
  explicit MatchGlobSet(size_type aCapacity) : CopyableTArray(aCapacity) {}
  explicit MatchGlobSet(const nsTArray& aOther) : CopyableTArray(aOther) {}
  MOZ_IMPLICIT MatchGlobSet(nsTArray&& aOther)
      : CopyableTArray(std::move(aOther)) {}
  MOZ_IMPLICIT MatchGlobSet(std::initializer_list<RefPtr<MatchGlob>> aIL)
      : CopyableTArray(aIL) {}

  bool Matches(const nsAString& aValue) const;
};

}  // namespace extensions
}  // namespace mozilla

#endif  // mozilla_extensions_MatchGlob_h
