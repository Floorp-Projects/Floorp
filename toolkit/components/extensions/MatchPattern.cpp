/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/extensions/MatchPattern.h"
#include "mozilla/extensions/MatchGlob.h"

#include "js/RegExp.h"  // JS::NewUCRegExpObject, JS::ExecuteRegExpNoStatics
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/Unused.h"

#include "nsGkAtoms.h"
#include "nsIProtocolHandler.h"
#include "nsIURL.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace extensions {

using namespace mozilla::dom;

/*****************************************************************************
 * AtomSet
 *****************************************************************************/

template <typename Range, typename AsAtom>
static AtomSet::ArrayType AtomSetFromRange(Range&& aRange,
                                           AsAtom&& aTransform) {
  AtomSet::ArrayType atoms;
  atoms.SetCapacity(RangeSize(aRange));
  std::transform(aRange.begin(), aRange.end(), MakeBackInserter(atoms),
                 std::forward<AsAtom>(aTransform));

  atoms.Sort();

  nsAtom* prev = nullptr;
  atoms.RemoveElementsBy([&prev](const RefPtr<nsAtom>& aAtom) {
    bool remove = aAtom == prev;
    prev = aAtom;
    return remove;
  });

  atoms.Compact();
  return atoms;
}

AtomSet::AtomSet(const nsTArray<nsString>& aElems)
    : mElems(AtomSetFromRange(
          aElems, [](const nsString& elem) { return NS_Atomize(elem); })) {}

AtomSet::AtomSet(std::initializer_list<nsAtom*> aIL)
    : mElems(AtomSetFromRange(aIL, [](nsAtom* elem) { return elem; })) {}

bool AtomSet::Intersects(const AtomSet& aOther) const {
  for (const auto& atom : *this) {
    if (aOther.Contains(atom)) {
      return true;
    }
  }
  for (const auto& atom : aOther) {
    if (Contains(atom)) {
      return true;
    }
  }
  return false;
}

/*****************************************************************************
 * URLInfo
 *****************************************************************************/

nsAtom* URLInfo::Scheme() const {
  if (!mScheme) {
    nsCString scheme;
    if (NS_SUCCEEDED(mURI->GetScheme(scheme))) {
      mScheme = NS_AtomizeMainThread(NS_ConvertASCIItoUTF16(scheme));
    }
  }
  return mScheme;
}

const nsCString& URLInfo::Host() const {
  if (mHost.IsVoid()) {
    Unused << mURI->GetHost(mHost);
  }
  return mHost;
}

const nsAtom* URLInfo::HostAtom() const {
  if (!mHostAtom) {
    mHostAtom = NS_Atomize(Host());
  }
  return mHostAtom;
}

const nsCString& URLInfo::FilePath() const {
  if (mFilePath.IsEmpty()) {
    nsCOMPtr<nsIURL> url = do_QueryInterface(mURI);
    if (!url || NS_FAILED(url->GetFilePath(mFilePath))) {
      mFilePath = Path();
    }
  }
  return mFilePath;
}

const nsCString& URLInfo::Path() const {
  if (mPath.IsEmpty()) {
    if (NS_FAILED(URINoRef()->GetPathQueryRef(mPath))) {
      mPath.Truncate();
    }
  }
  return mPath;
}

const nsCString& URLInfo::CSpec() const {
  if (mCSpec.IsEmpty()) {
    Unused << URINoRef()->GetSpec(mCSpec);
  }
  return mCSpec;
}

const nsString& URLInfo::Spec() const {
  if (mSpec.IsEmpty()) {
    AppendUTF8toUTF16(CSpec(), mSpec);
  }
  return mSpec;
}

nsIURI* URLInfo::URINoRef() const {
  if (!mURINoRef) {
    if (NS_FAILED(NS_GetURIWithoutRef(mURI, getter_AddRefs(mURINoRef)))) {
      mURINoRef = mURI;
    }
  }
  return mURINoRef;
}

bool URLInfo::InheritsPrincipal() const {
  if (!mInheritsPrincipal.isSome()) {
    // For our purposes, about:blank and about:srcdoc are treated as URIs that
    // inherit principals.
    bool inherits = Spec().EqualsLiteral("about:blank") ||
                    Spec().EqualsLiteral("about:srcdoc");

    if (!inherits) {
      nsresult rv = NS_URIChainHasFlags(
          mURI, nsIProtocolHandler::URI_INHERITS_SECURITY_CONTEXT, &inherits);
      Unused << NS_WARN_IF(NS_FAILED(rv));
    }

    mInheritsPrincipal.emplace(inherits);
  }
  return mInheritsPrincipal.ref();
}

/*****************************************************************************
 * CookieInfo
 *****************************************************************************/

bool CookieInfo::IsDomain() const {
  if (mIsDomain.isNothing()) {
    mIsDomain.emplace(false);
    MOZ_ALWAYS_SUCCEEDS(mCookie->GetIsDomain(mIsDomain.ptr()));
  }
  return mIsDomain.ref();
}

bool CookieInfo::IsSecure() const {
  if (mIsSecure.isNothing()) {
    mIsSecure.emplace(false);
    MOZ_ALWAYS_SUCCEEDS(mCookie->GetIsSecure(mIsSecure.ptr()));
  }
  return mIsSecure.ref();
}

const nsCString& CookieInfo::Host() const {
  if (mHost.IsEmpty()) {
    MOZ_ALWAYS_SUCCEEDS(mCookie->GetHost(mHost));
  }
  return mHost;
}

const nsCString& CookieInfo::RawHost() const {
  if (mRawHost.IsEmpty()) {
    MOZ_ALWAYS_SUCCEEDS(mCookie->GetRawHost(mRawHost));
  }
  return mRawHost;
}

/*****************************************************************************
 * MatchPatternCore
 *****************************************************************************/

#define DEFINE_STATIC_ATOM_SET(name, ...)            \
  static already_AddRefed<AtomSet> name() {          \
    MOZ_ASSERT(NS_IsMainThread());                   \
    static StaticRefPtr<AtomSet> sAtomSet;           \
    RefPtr<AtomSet> atomSet = sAtomSet;              \
    if (!atomSet) {                                  \
      atomSet = sAtomSet = new AtomSet{__VA_ARGS__}; \
      ClearOnShutdown(&sAtomSet);                    \
    }                                                \
    return atomSet.forget();                         \
  }

DEFINE_STATIC_ATOM_SET(PermittedSchemes, nsGkAtoms::http, nsGkAtoms::https,
                       nsGkAtoms::ws, nsGkAtoms::wss, nsGkAtoms::file,
                       nsGkAtoms::ftp, nsGkAtoms::data);

// Known schemes that are followed by "://" instead of ":".
DEFINE_STATIC_ATOM_SET(HostLocatorSchemes, nsGkAtoms::http, nsGkAtoms::https,
                       nsGkAtoms::ws, nsGkAtoms::wss, nsGkAtoms::file,
                       nsGkAtoms::ftp, nsGkAtoms::moz_extension,
                       nsGkAtoms::chrome, nsGkAtoms::resource, nsGkAtoms::moz,
                       nsGkAtoms::moz_icon, nsGkAtoms::moz_gio);

DEFINE_STATIC_ATOM_SET(WildcardSchemes, nsGkAtoms::http, nsGkAtoms::https,
                       nsGkAtoms::ws, nsGkAtoms::wss);

#undef DEFINE_STATIC_ATOM_SET

MatchPatternCore::MatchPatternCore(const nsAString& aPattern, bool aIgnorePath,
                                   bool aRestrictSchemes, ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<AtomSet> permittedSchemes = PermittedSchemes();

  mPattern = aPattern;

  if (aPattern.EqualsLiteral("<all_urls>")) {
    mSchemes = permittedSchemes;
    mMatchSubdomain = true;
    return;
  }

  // The portion of the URL we're currently examining.
  uint32_t offset = 0;
  auto tail = Substring(aPattern, offset);

  /***************************************************************************
   * Scheme
   ***************************************************************************/
  int32_t index = aPattern.FindChar(':');
  if (index <= 0) {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return;
  }

  RefPtr<nsAtom> scheme = NS_AtomizeMainThread(StringHead(aPattern, index));
  bool requireHostLocatorScheme = true;
  if (scheme == nsGkAtoms::_asterisk) {
    mSchemes = WildcardSchemes();
  } else if (!aRestrictSchemes || permittedSchemes->Contains(scheme) ||
             scheme == nsGkAtoms::moz_extension) {
    RefPtr<AtomSet> hostLocatorSchemes = HostLocatorSchemes();
    mSchemes = new AtomSet({scheme});
    requireHostLocatorScheme = hostLocatorSchemes->Contains(scheme);
  } else {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return;
  }

  /***************************************************************************
   * Host
   ***************************************************************************/
  offset = index + 1;
  tail.Rebind(aPattern, offset);

  if (!requireHostLocatorScheme) {
    // Unrecognized schemes and some schemes such as about: and data: URIs
    // don't have hosts, so just match on the path.
    // And so, ignorePath doesn't make sense for these matchers.
    aIgnorePath = false;
  } else {
    if (!StringHead(tail, 2).EqualsLiteral("//")) {
      aRv.Throw(NS_ERROR_INVALID_ARG);
      return;
    }

    offset += 2;
    tail.Rebind(aPattern, offset);
    index = tail.FindChar('/');
    if (index < 0) {
      index = tail.Length();
    }

    auto host = StringHead(tail, index);
    if (host.IsEmpty() && scheme != nsGkAtoms::file) {
      aRv.Throw(NS_ERROR_INVALID_ARG);
      return;
    }

    offset += index;
    tail.Rebind(aPattern, offset);

    if (host.EqualsLiteral("*")) {
      mMatchSubdomain = true;
    } else if (StringHead(host, 2).EqualsLiteral("*.")) {
      CopyUTF16toUTF8(Substring(host, 2), mDomain);
      mMatchSubdomain = true;
    } else if (host.Length() > 1 && host[0] == '[' &&
               host[host.Length() - 1] == ']') {
      // This is an IPv6 literal, we drop the enclosing `[]` to be
      // consistent with nsIURI.
      CopyUTF16toUTF8(Substring(host, 1, host.Length() - 2), mDomain);
    } else {
      CopyUTF16toUTF8(host, mDomain);
    }
  }

  /***************************************************************************
   * Path
   ***************************************************************************/
  if (aIgnorePath) {
    mPattern.Truncate(offset);
    mPattern.AppendLiteral("/*");
    return;
  }

  NS_ConvertUTF16toUTF8 path(tail);
  if (path.IsEmpty()) {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return;
  }

  // Anything matched against one of the hosts in hostLocatorSchemes is expected
  // to have a path starting with "/". Pass isPathGlob=true in these cases to
  // ensure that MatchGlobCore treats "/*" paths as a wildcard (IsWildcard()).
  bool isPathGlob = requireHostLocatorScheme;
  mPath = new MatchGlobCore(path, false, isPathGlob, aRv);
}

bool MatchPatternCore::MatchesAllWebUrls() const {
  // Returns true if the match pattern matches any http(s) URL, i.e.:
  // - ["<all_urls>"]
  // - ["*://*/*"]
  return (mSchemes->Contains(nsGkAtoms::http) &&
          MatchesAllUrlsWithScheme(nsGkAtoms::https));
}

bool MatchPatternCore::MatchesAllUrlsWithScheme(const nsAtom* scheme) const {
  return (mSchemes->Contains(scheme) && DomainIsWildcard() &&
          (!mPath || mPath->IsWildcard()));
}

bool MatchPatternCore::MatchesDomain(const nsACString& aDomain) const {
  if (DomainIsWildcard() || mDomain == aDomain) {
    return true;
  }

  if (mMatchSubdomain) {
    int64_t offset = (int64_t)aDomain.Length() - mDomain.Length();
    if (offset > 0 && aDomain[offset - 1] == '.' &&
        Substring(aDomain, offset) == mDomain) {
      return true;
    }
  }

  return false;
}

bool MatchPatternCore::Matches(const nsAString& aURL, bool aExplicit,
                               ErrorResult& aRv) const {
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aURL);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return false;
  }

  return Matches(uri.get(), aExplicit);
}

bool MatchPatternCore::Matches(const URLInfo& aURL, bool aExplicit) const {
  if (aExplicit && mMatchSubdomain) {
    return false;
  }

  if (!mSchemes->Contains(aURL.Scheme())) {
    return false;
  }

  if (!MatchesDomain(aURL.Host())) {
    return false;
  }

  if (mPath && !mPath->IsWildcard() && !mPath->Matches(aURL.Path())) {
    return false;
  }

  return true;
}

bool MatchPatternCore::MatchesCookie(const CookieInfo& aCookie) const {
  if (!mSchemes->Contains(nsGkAtoms::https) &&
      (aCookie.IsSecure() || !mSchemes->Contains(nsGkAtoms::http))) {
    return false;
  }

  if (MatchesDomain(aCookie.RawHost())) {
    return true;
  }

  if (!aCookie.IsDomain()) {
    return false;
  }

  // Things get tricker for domain cookies. The extension needs to be able
  // to read any cookies that could be read by any host it has permissions
  // for. This means that our normal host matching checks won't work,
  // since the pattern "*://*.foo.example.com/" doesn't match ".example.com",
  // but it does match "bar.foo.example.com", which can read cookies
  // with the domain ".example.com".
  //
  // So, instead, we need to manually check our filters, and accept any
  // with hosts that end with our cookie's host.

  auto& host = aCookie.Host();
  return StringTail(mDomain, host.Length()) == host;
}

bool MatchPatternCore::SubsumesDomain(const MatchPatternCore& aPattern) const {
  if (!mMatchSubdomain && aPattern.mMatchSubdomain &&
      aPattern.mDomain == mDomain) {
    return false;
  }

  return MatchesDomain(aPattern.mDomain);
}

bool MatchPatternCore::Subsumes(const MatchPatternCore& aPattern) const {
  for (auto& scheme : *aPattern.mSchemes) {
    if (!mSchemes->Contains(scheme)) {
      return false;
    }
  }

  return SubsumesDomain(aPattern);
}

bool MatchPatternCore::Overlaps(const MatchPatternCore& aPattern) const {
  if (!mSchemes->Intersects(*aPattern.mSchemes)) {
    return false;
  }

  return SubsumesDomain(aPattern) || aPattern.SubsumesDomain(*this);
}

/*****************************************************************************
 * MatchPattern
 *****************************************************************************/

/* static */
already_AddRefed<MatchPattern> MatchPattern::Constructor(
    dom::GlobalObject& aGlobal, const nsAString& aPattern,
    const MatchPatternOptions& aOptions, ErrorResult& aRv) {
  RefPtr<MatchPattern> pattern = new MatchPattern(
      aGlobal.GetAsSupports(),
      MakeAndAddRef<MatchPatternCore>(aPattern, aOptions.mIgnorePath,
                                      aOptions.mRestrictSchemes, aRv));
  if (aRv.Failed()) {
    return nullptr;
  }
  return pattern.forget();
}

JSObject* MatchPattern::WrapObject(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  return MatchPattern_Binding::Wrap(aCx, this, aGivenProto);
}

/* static */
bool MatchPattern::MatchesAllURLs(const URLInfo& aURL) {
  RefPtr<AtomSet> permittedSchemes = PermittedSchemes();
  return permittedSchemes->Contains(aURL.Scheme());
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(MatchPattern, mParent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MatchPattern)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(MatchPattern)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MatchPattern)

bool MatchPatternSetCore::Matches(const nsAString& aURL, bool aExplicit,
                                  ErrorResult& aRv) const {
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aURL);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return false;
  }

  return Matches(uri.get(), aExplicit);
}

bool MatchPatternSetCore::Matches(const URLInfo& aURL, bool aExplicit) const {
  for (const auto& pattern : mPatterns) {
    if (pattern->Matches(aURL, aExplicit)) {
      return true;
    }
  }
  return false;
}

bool MatchPatternSetCore::MatchesAllWebUrls() const {
  // Returns true if the match pattern matches any http(s) URL, i.e.:
  // - ["<all_urls>"]
  // - ["*://*/*"]
  // - ["https://*/*", "http://*/*"]
  bool hasHttp = false;
  bool hasHttps = false;
  for (const auto& pattern : mPatterns) {
    if (!hasHttp && pattern->MatchesAllUrlsWithScheme(nsGkAtoms::http)) {
      hasHttp = true;
    }
    if (!hasHttps && pattern->MatchesAllUrlsWithScheme(nsGkAtoms::https)) {
      hasHttps = true;
    }
    if (hasHttp && hasHttps) {
      return true;
    }
  }
  return false;
}

bool MatchPatternSetCore::MatchesCookie(const CookieInfo& aCookie) const {
  for (const auto& pattern : mPatterns) {
    if (pattern->MatchesCookie(aCookie)) {
      return true;
    }
  }
  return false;
}

bool MatchPatternSetCore::Subsumes(const MatchPatternCore& aPattern) const {
  // Note: the implementation below assumes that a pattern can only be subsumed
  // if it is fully contained within another pattern. Logically, this is an
  // incorrect assumption: "*://example.com/" matches multiple schemes, and is
  // equivalent to a MatchPatternSet that lists all schemes explicitly.
  // TODO bug 1856380: account for all patterns if aPattern has a wildcard
  // scheme (such as when aPattern.MatchesAllWebUrls() is true).
  for (const auto& pattern : mPatterns) {
    if (pattern->Subsumes(aPattern)) {
      return true;
    }
  }
  return false;
}

bool MatchPatternSetCore::SubsumesDomain(
    const MatchPatternCore& aPattern) const {
  for (const auto& pattern : mPatterns) {
    if (pattern->SubsumesDomain(aPattern)) {
      return true;
    }
  }
  return false;
}

bool MatchPatternSetCore::Overlaps(
    const MatchPatternSetCore& aPatternSet) const {
  for (const auto& pattern : aPatternSet.mPatterns) {
    if (Overlaps(*pattern)) {
      return true;
    }
  }
  return false;
}

bool MatchPatternSetCore::Overlaps(const MatchPatternCore& aPattern) const {
  for (const auto& pattern : mPatterns) {
    if (pattern->Overlaps(aPattern)) {
      return true;
    }
  }
  return false;
}

bool MatchPatternSetCore::OverlapsAll(
    const MatchPatternSetCore& aPatternSet) const {
  for (const auto& pattern : aPatternSet.mPatterns) {
    if (!Overlaps(*pattern)) {
      return false;
    }
  }
  return aPatternSet.mPatterns.Length() > 0;
}

/*****************************************************************************
 * MatchPatternSet
 *****************************************************************************/

/* static */
already_AddRefed<MatchPatternSet> MatchPatternSet::Constructor(
    dom::GlobalObject& aGlobal,
    const nsTArray<dom::OwningStringOrMatchPattern>& aPatterns,
    const MatchPatternOptions& aOptions, ErrorResult& aRv) {
  MatchPatternSetCore::ArrayType patterns;

  for (auto& elem : aPatterns) {
    if (elem.IsMatchPattern()) {
      patterns.AppendElement(elem.GetAsMatchPattern()->Core());
    } else {
      RefPtr<MatchPatternCore> pattern =
          new MatchPatternCore(elem.GetAsString(), aOptions.mIgnorePath,
                               aOptions.mRestrictSchemes, aRv);

      if (aRv.Failed()) {
        return nullptr;
      }
      patterns.AppendElement(std::move(pattern));
    }
  }

  RefPtr<MatchPatternSet> patternSet = new MatchPatternSet(
      aGlobal.GetAsSupports(),
      do_AddRef(new MatchPatternSetCore(std::move(patterns))));
  return patternSet.forget();
}

void MatchPatternSet::GetPatterns(ArrayType& aPatterns) {
  if (!mPatternsCache) {
    mPatternsCache.emplace(Core()->mPatterns.Length());
    for (auto& elem : Core()->mPatterns) {
      mPatternsCache->AppendElement(new MatchPattern(this, do_AddRef(elem)));
    }
  }
  aPatterns.AppendElements(*mPatternsCache);
}

JSObject* MatchPatternSet::WrapObject(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return MatchPatternSet_Binding::Wrap(aCx, this, aGivenProto);
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(MatchPatternSet, mPatternsCache, mParent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MatchPatternSet)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(MatchPatternSet)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MatchPatternSet)

/*****************************************************************************
 * MatchGlobCore
 *****************************************************************************/

MatchGlobCore::MatchGlobCore(const nsACString& aGlob, bool aAllowQuestion,
                             bool aIsPathGlob, ErrorResult& aRv)
    : mGlob(aGlob) {
  // Check for a literal match with no glob metacharacters.
  auto index = mGlob.FindCharInSet(aAllowQuestion ? "*?" : "*");
  if (index < 0) {
    mPathLiteral = mGlob;
    return;
  }

  // Check for a prefix match, where the only glob metacharacter is a "*"
  // at the end of the string (or a sequence of it).
  for (int32_t i = mGlob.Length() - 1; i >= index && mGlob[i] == '*'; --i) {
    if (i == index) {
      mPathLiteral = StringHead(mGlob, index);
      if (aIsPathGlob && mPathLiteral.EqualsLiteral("/")) {
        // Ensure that IsWildcard() correctly treats us as a wildcard.
        mPathLiteral.Truncate();
      }
      mIsPrefix = true;
      return;
    }
  }

  // Fall back to the regexp slow path.
  constexpr auto metaChars = ".+*?^${}()|[]\\"_ns;

  nsAutoCString escaped;
  escaped.Append('^');

  // For any continuous string of * (and ? if aAllowQuestion) wildcards, only
  // emit the first *, later ones are redundant, and can hang regex matching.
  bool emittedFirstStar = false;

  for (uint32_t i = 0; i < mGlob.Length(); i++) {
    auto c = mGlob[i];
    if (c == '*') {
      if (!emittedFirstStar) {
        escaped.AppendLiteral(".*");
        emittedFirstStar = true;
      }
    } else if (c == '?' && aAllowQuestion) {
      escaped.Append('.');
    } else {
      if (metaChars.Contains(c)) {
        escaped.Append('\\');
      }
      escaped.Append(c);

      // String of wildcards broken by a non-wildcard char, reset tracking flag.
      emittedFirstStar = false;
    }
  }

  escaped.Append('$');

  mRegExp = RustRegex(escaped);
  if (!mRegExp) {
    aRv.ThrowTypeError("failed to compile regex for glob");
  }
}

bool MatchGlobCore::Matches(const nsACString& aString) const {
  if (mRegExp) {
    return mRegExp.IsMatch(aString);
  }

  if (mIsPrefix) {
    return mPathLiteral == StringHead(aString, mPathLiteral.Length());
  }

  return mPathLiteral == aString;
}

/*****************************************************************************
 * MatchGlob
 *****************************************************************************/

/* static */
already_AddRefed<MatchGlob> MatchGlob::Constructor(dom::GlobalObject& aGlobal,
                                                   const nsACString& aGlob,
                                                   bool aAllowQuestion,
                                                   ErrorResult& aRv) {
  RefPtr<MatchGlob> glob = new MatchGlob(
      aGlobal.GetAsSupports(),
      MakeAndAddRef<MatchGlobCore>(aGlob, aAllowQuestion, false, aRv));
  if (aRv.Failed()) {
    return nullptr;
  }
  return glob.forget();
}

JSObject* MatchGlob::WrapObject(JSContext* aCx,
                                JS::Handle<JSObject*> aGivenProto) {
  return MatchGlob_Binding::Wrap(aCx, this, aGivenProto);
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(MatchGlob, mParent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MatchGlob)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(MatchGlob)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MatchGlob)

/*****************************************************************************
 * MatchGlobSet
 *****************************************************************************/

bool MatchGlobSet::Matches(const nsACString& aValue) const {
  for (auto& glob : *this) {
    if (glob->Matches(aValue)) {
      return true;
    }
  }
  return false;
}

}  // namespace extensions
}  // namespace mozilla
