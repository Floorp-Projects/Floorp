/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_extensions_MatchPattern_h
#define mozilla_extensions_MatchPattern_h

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/MatchPatternBinding.h"
#include "mozilla/extensions/MatchGlob.h"

#include "jspubtd.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Likely.h"
#include "mozilla/Maybe.h"
#include "mozilla/RefCounted.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsTArray.h"
#include "nsAtom.h"
#include "nsICookie2.h"
#include "nsISupports.h"
#include "nsIURI.h"
#include "nsWrapperCache.h"


namespace mozilla {
namespace extensions {

using dom::MatchPatternOptions;


// A sorted, binary-search-backed set of atoms, optimized for frequent lookups
// and infrequent updates.
class AtomSet final : public RefCounted<AtomSet>
{
  using ArrayType = AutoTArray<RefPtr<nsAtom>, 1>;

public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(AtomSet)

  explicit AtomSet(const nsTArray<nsString>& aElems);

  explicit AtomSet(const char** aElems);

  MOZ_IMPLICIT AtomSet(std::initializer_list<nsAtom*> aIL);

  bool Contains(const nsAString& elem) const
  {
    RefPtr<nsAtom> atom = NS_AtomizeMainThread(elem);
    return Contains(atom);
  }

  bool Contains(const nsACString& aElem) const
  {
    RefPtr<nsAtom> atom = NS_Atomize(aElem);
    return Contains(atom);
  }

  bool Contains(const nsAtom* aAtom) const
  {
    return mElems.ContainsSorted(aAtom);
  }

  bool Intersects(const AtomSet& aOther) const;


  void Add(nsAtom* aElem);
  void Remove(nsAtom* aElem);

  void Add(const nsAString& aElem)
  {
    RefPtr<nsAtom> atom = NS_AtomizeMainThread(aElem);
    return Add(atom);
  }

  void Remove(const nsAString& aElem)
  {
    RefPtr<nsAtom> atom = NS_AtomizeMainThread(aElem);
    return Remove(atom);
  }

  // Returns a cached, statically-allocated matcher for the given set of
  // literal strings.
  template <const char** schemes>
  static already_AddRefed<AtomSet>
  Get()
  {
    static RefPtr<AtomSet> sMatcher;

    if (MOZ_UNLIKELY(!sMatcher)) {
      sMatcher = new AtomSet(schemes);
      ClearOnShutdown(&sMatcher);
    }

    return do_AddRef(sMatcher);
  }

  void
  Get(nsTArray<nsString>& aResult) const
  {
    aResult.SetCapacity(mElems.Length());

    for (const auto& atom : mElems) {
      aResult.AppendElement(nsDependentAtomString(atom));
    }
  }

  auto begin() const
    -> decltype(DeclVal<const ArrayType>().begin())
  {
    return mElems.begin();
  }

  auto end() const
    -> decltype(DeclVal<const ArrayType>().end())
  {
    return mElems.end();
  }

private:
  ArrayType mElems;

  void SortAndUniquify();
};


// A helper class to lazily retrieve, transcode, and atomize certain URI
// properties the first time they're used, and cache the results, so that they
// can be used across multiple match operations.
class URLInfo final
{
public:
  MOZ_IMPLICIT URLInfo(nsIURI* aURI)
    : mURI(aURI)
  {
    mHost.SetIsVoid(true);
  }

  URLInfo(nsIURI* aURI, bool aNoRef)
    : URLInfo(aURI)
  {
    if (aNoRef) {
      mURINoRef = mURI;
    }
  }

  URLInfo(const URLInfo& aOther)
    : URLInfo(aOther.mURI.get())
  {}

  nsIURI* URI() const { return mURI; }

  nsAtom* Scheme() const;
  const nsCString& Host() const;
  const nsAtom* HostAtom() const;
  const nsString& Path() const;
  const nsString& FilePath() const;
  const nsString& Spec() const;
  const nsCString& CSpec() const;

  bool InheritsPrincipal() const;

private:
  nsIURI* URINoRef() const;

  nsCOMPtr<nsIURI> mURI;
  mutable nsCOMPtr<nsIURI> mURINoRef;

  mutable RefPtr<nsAtom> mScheme;
  mutable nsCString mHost;
  mutable RefPtr<nsAtom> mHostAtom;

  mutable nsString mPath;
  mutable nsString mFilePath;
  mutable nsString mSpec;
  mutable nsCString mCSpec;

  mutable Maybe<bool> mInheritsPrincipal;
};


// Similar to URLInfo, but for cookies.
class MOZ_STACK_CLASS CookieInfo final
{
public:
  MOZ_IMPLICIT CookieInfo(nsICookie2* aCookie)
    : mCookie(aCookie)
  {}

  bool IsSecure() const;
  bool IsDomain() const;

  const nsCString& Host() const;
  const nsCString& RawHost() const;

private:
  nsCOMPtr<nsICookie2> mCookie;

  mutable Maybe<bool> mIsSecure;
  mutable Maybe<bool> mIsDomain;

  mutable nsCString mHost;
  mutable nsCString mRawHost;
};


class MatchPattern final : public nsISupports
                         , public nsWrapperCache
{
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(MatchPattern)

  static already_AddRefed<MatchPattern>
  Constructor(dom::GlobalObject& aGlobal,
              const nsAString& aPattern,
              const MatchPatternOptions& aOptions,
              ErrorResult& aRv);

  bool Matches(const nsAString& aURL, bool aExplicit, ErrorResult& aRv) const;

  bool Matches(const URLInfo& aURL, bool aExplicit = false) const;

  bool Matches(const URLInfo& aURL, bool aExplicit, ErrorResult& aRv) const
  {
    return Matches(aURL, aExplicit);
  }


  bool MatchesCookie(const CookieInfo& aCookie) const;

  bool MatchesDomain(const nsACString& aDomain) const;

  bool Subsumes(const MatchPattern& aPattern) const;

  bool Overlaps(const MatchPattern& aPattern) const;

  bool DomainIsWildcard() const
  {
    return mMatchSubdomain && mDomain.IsEmpty();
  }

  void GetPattern(nsAString& aPattern) const
  {
    aPattern = mPattern;
  }

  nsISupports* GetParentObject() const { return mParent; }

  virtual JSObject* WrapObject(JSContext* aCx, JS::HandleObject aGivenProto) override;

protected:
  virtual ~MatchPattern() = default;

private:
  explicit MatchPattern(nsISupports* aParent) : mParent(aParent) {}

  void Init(JSContext* aCx, const nsAString& aPattern, bool aIgnorePath,
            bool aRestrictSchemes, ErrorResult& aRv);

  bool SubsumesDomain(const MatchPattern& aPattern) const;


  nsCOMPtr<nsISupports> mParent;

  // The normalized match pattern string that this object represents.
  nsString mPattern;

  // The set of atomized URI schemes that this pattern matches.
  RefPtr<AtomSet> mSchemes;

  // The domain that this matcher matches. If mMatchSubdomain is false, only
  // matches the exact domain. If it's true, matches the domain or any
  // subdomain.
  //
  // For instance, "*.foo.com" gives mDomain = "foo.com" and mMatchSubdomain = true,
  // and matches "foo.com" or "bar.foo.com" but not "barfoo.com".
  //
  // While "foo.com" gives mDomain = "foo.com" and mMatchSubdomain = false,
  // and matches "foo.com" but not "bar.foo.com".
  nsCString mDomain;
  bool mMatchSubdomain = false;

  // The glob against which the URL path must match. If null, the path is
  // ignored entirely. If non-null, the path must match this glob.
  RefPtr<MatchGlob> mPath;

 public:
  // A quick way to check if a particular URL matches <all_urls> without
  // actually instantiating a MatchPattern
  static bool MatchesAllURLs(const URLInfo& aURL);
};


class MatchPatternSet final : public nsISupports
                            , public nsWrapperCache
{
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(MatchPatternSet)

  using ArrayType = nsTArray<RefPtr<MatchPattern>>;


  static already_AddRefed<MatchPatternSet>
  Constructor(dom::GlobalObject& aGlobal,
              const nsTArray<dom::OwningStringOrMatchPattern>& aPatterns,
              const MatchPatternOptions& aOptions,
              ErrorResult& aRv);


  bool Matches(const nsAString& aURL, bool aExplicit, ErrorResult& aRv) const;

  bool Matches(const URLInfo& aURL, bool aExplicit = false) const;

  bool Matches(const URLInfo& aURL, bool aExplicit, ErrorResult& aRv) const
  {
    return Matches(aURL, aExplicit);
  }


  bool MatchesCookie(const CookieInfo& aCookie) const;

  bool Subsumes(const MatchPattern& aPattern) const;

  bool Overlaps(const MatchPattern& aPattern) const;

  bool Overlaps(const MatchPatternSet& aPatternSet) const;

  bool OverlapsAll(const MatchPatternSet& aPatternSet) const;

  void GetPatterns(ArrayType& aPatterns)
  {
    aPatterns.AppendElements(mPatterns);
  }


  nsISupports* GetParentObject() const { return mParent; }

  virtual JSObject* WrapObject(JSContext* aCx, JS::HandleObject aGivenProto) override;

protected:
  virtual ~MatchPatternSet() = default;

private:
  explicit MatchPatternSet(nsISupports* aParent, ArrayType&& aPatterns)
    : mParent(aParent)
    , mPatterns(Forward<ArrayType>(aPatterns))
  {}

  nsCOMPtr<nsISupports> mParent;

  ArrayType mPatterns;
};

} // namespace extensions
} // namespace mozilla

#endif // mozilla_extensions_MatchPattern_h
