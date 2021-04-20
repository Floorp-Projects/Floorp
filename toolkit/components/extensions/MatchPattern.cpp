/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/extensions/MatchPattern.h"
#include "mozilla/extensions/MatchGlob.h"

#include "js/RegExp.h"  // JS::NewUCRegExpObject, JS::ExecuteRegExpNoStatics
#include "mozilla/dom/ScriptSettings.h"
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

AtomSet::AtomSet(const nsTArray<nsString>& aElems) {
  mElems.SetCapacity(aElems.Length());

  for (const auto& elem : aElems) {
    mElems.AppendElement(NS_AtomizeMainThread(elem));
  }

  SortAndUniquify();
}

AtomSet::AtomSet(const char** aElems) {
  for (const char** elemp = aElems; *elemp; elemp++) {
    mElems.AppendElement(NS_Atomize(*elemp));
  }

  SortAndUniquify();
}

AtomSet::AtomSet(std::initializer_list<nsAtom*> aIL) {
  mElems.SetCapacity(aIL.size());

  for (const auto& elem : aIL) {
    mElems.AppendElement(elem);
  }

  SortAndUniquify();
}

void AtomSet::SortAndUniquify() {
  mElems.Sort();

  nsAtom* prev = nullptr;
  mElems.RemoveElementsBy([&prev](const RefPtr<nsAtom>& aAtom) {
    bool remove = aAtom == prev;
    prev = aAtom;
    return remove;
  });

  mElems.Compact();
}

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

void AtomSet::Add(nsAtom* aAtom) {
  auto index = mElems.IndexOfFirstElementGt(aAtom);
  if (index == 0 || mElems[index - 1] != aAtom) {
    mElems.InsertElementAt(index, aAtom);
  }
}

void AtomSet::Remove(nsAtom* aAtom) {
  auto index = mElems.BinaryIndexOf(aAtom);
  if (index != ArrayType::NoIndex) {
    mElems.RemoveElementAt(index);
  }
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

const nsString& URLInfo::FilePath() const {
  if (mFilePath.IsEmpty()) {
    nsCString path;
    nsCOMPtr<nsIURL> url = do_QueryInterface(mURI);
    if (url && NS_SUCCEEDED(url->GetFilePath(path))) {
      AppendUTF8toUTF16(path, mFilePath);
    } else {
      mFilePath = Path();
    }
  }
  return mFilePath;
}

const nsString& URLInfo::Path() const {
  if (mPath.IsEmpty()) {
    nsCString path;
    if (NS_SUCCEEDED(URINoRef()->GetPathQueryRef(path))) {
      AppendUTF8toUTF16(path, mPath);
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
 * MatchPattern
 *****************************************************************************/

const char* PERMITTED_SCHEMES[] = {"http", "https", "ws",   "wss",
                                   "file", "ftp",   "data", nullptr};

// Known schemes that are followed by "://" instead of ":".
const char* HOST_LOCATOR_SCHEMES[] = {
    "http",   "https",    "ws",  "wss",      "file",    "ftp",  "moz-extension",
    "chrome", "resource", "moz", "moz-icon", "moz-gio", nullptr};

const char* WILDCARD_SCHEMES[] = {"http", "https", "ws", "wss", nullptr};

/* static */
already_AddRefed<MatchPattern> MatchPattern::Constructor(
    dom::GlobalObject& aGlobal, const nsAString& aPattern,
    const MatchPatternOptions& aOptions, ErrorResult& aRv) {
  RefPtr<MatchPattern> pattern = new MatchPattern(aGlobal.GetAsSupports());
  pattern->Init(aGlobal.Context(), aPattern, aOptions.mIgnorePath,
                aOptions.mRestrictSchemes, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  return pattern.forget();
}

void MatchPattern::Init(JSContext* aCx, const nsAString& aPattern,
                        bool aIgnorePath, bool aRestrictSchemes,
                        ErrorResult& aRv) {
  RefPtr<AtomSet> permittedSchemes;
  nsresult rv = AtomSet::Get<PERMITTED_SCHEMES>(permittedSchemes);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

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
    rv = AtomSet::Get<WILDCARD_SCHEMES>(mSchemes);
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
      return;
    }
  } else if (!aRestrictSchemes || permittedSchemes->Contains(scheme) ||
             scheme == nsGkAtoms::moz_extension) {
    RefPtr<AtomSet> hostLocatorSchemes;
    rv = AtomSet::Get<HOST_LOCATOR_SCHEMES>(hostLocatorSchemes);
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
      return;
    }
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

  auto path = tail;
  if (path.IsEmpty()) {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return;
  }

  mPath = new MatchGlob(this);
  mPath->Init(aCx, path, false, aRv);
}

bool MatchPattern::MatchesDomain(const nsACString& aDomain) const {
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

bool MatchPattern::Matches(const nsAString& aURL, bool aExplicit,
                           ErrorResult& aRv) const {
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aURL);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return false;
  }

  return Matches(uri.get(), aExplicit);
}

bool MatchPattern::Matches(const URLInfo& aURL, bool aExplicit) const {
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

bool MatchPattern::MatchesCookie(const CookieInfo& aCookie) const {
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

bool MatchPattern::SubsumesDomain(const MatchPattern& aPattern) const {
  if (!mMatchSubdomain && aPattern.mMatchSubdomain &&
      aPattern.mDomain == mDomain) {
    return false;
  }

  return MatchesDomain(aPattern.mDomain);
}

bool MatchPattern::Subsumes(const MatchPattern& aPattern) const {
  for (auto& scheme : *aPattern.mSchemes) {
    if (!mSchemes->Contains(scheme)) {
      return false;
    }
  }

  return SubsumesDomain(aPattern);
}

bool MatchPattern::Overlaps(const MatchPattern& aPattern) const {
  if (!mSchemes->Intersects(*aPattern.mSchemes)) {
    return false;
  }

  return SubsumesDomain(aPattern) || aPattern.SubsumesDomain(*this);
}

JSObject* MatchPattern::WrapObject(JSContext* aCx,
                                   JS::HandleObject aGivenProto) {
  return MatchPattern_Binding::Wrap(aCx, this, aGivenProto);
}

/* static */
bool MatchPattern::MatchesAllURLs(const URLInfo& aURL) {
  RefPtr<AtomSet> permittedSchemes;
  nsresult rv = AtomSet::Get<PERMITTED_SCHEMES>(permittedSchemes);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to retrireve PERMITTED_SCHEMES AtomSet");
    return false;
  }
  return permittedSchemes->Contains(aURL.Scheme());
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(MatchPattern, mPath, mParent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MatchPattern)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(MatchPattern)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MatchPattern)

/*****************************************************************************
 * MatchPatternSet
 *****************************************************************************/

/* static */
already_AddRefed<MatchPatternSet> MatchPatternSet::Constructor(
    dom::GlobalObject& aGlobal,
    const nsTArray<dom::OwningStringOrMatchPattern>& aPatterns,
    const MatchPatternOptions& aOptions, ErrorResult& aRv) {
  ArrayType patterns;

  for (auto& elem : aPatterns) {
    if (elem.IsMatchPattern()) {
      patterns.AppendElement(elem.GetAsMatchPattern());
    } else {
      RefPtr<MatchPattern> pattern =
          MatchPattern::Constructor(aGlobal, elem.GetAsString(), aOptions, aRv);

      if (!pattern) {
        return nullptr;
      }
      patterns.AppendElement(std::move(pattern));
    }
  }

  RefPtr<MatchPatternSet> patternSet =
      new MatchPatternSet(aGlobal.GetAsSupports(), std::move(patterns));
  return patternSet.forget();
}

bool MatchPatternSet::Matches(const nsAString& aURL, bool aExplicit,
                              ErrorResult& aRv) const {
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aURL);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return false;
  }

  return Matches(uri.get(), aExplicit);
}

bool MatchPatternSet::Matches(const URLInfo& aURL, bool aExplicit) const {
  for (const auto& pattern : mPatterns) {
    if (pattern->Matches(aURL, aExplicit)) {
      return true;
    }
  }
  return false;
}

bool MatchPatternSet::MatchesCookie(const CookieInfo& aCookie) const {
  for (const auto& pattern : mPatterns) {
    if (pattern->MatchesCookie(aCookie)) {
      return true;
    }
  }
  return false;
}

bool MatchPatternSet::Subsumes(const MatchPattern& aPattern) const {
  for (const auto& pattern : mPatterns) {
    if (pattern->Subsumes(aPattern)) {
      return true;
    }
  }
  return false;
}

bool MatchPatternSet::SubsumesDomain(const MatchPattern& aPattern) const {
  for (const auto& pattern : mPatterns) {
    if (pattern->SubsumesDomain(aPattern)) {
      return true;
    }
  }
  return false;
}

bool MatchPatternSet::Overlaps(const MatchPatternSet& aPatternSet) const {
  for (const auto& pattern : aPatternSet.mPatterns) {
    if (Overlaps(*pattern)) {
      return true;
    }
  }
  return false;
}

bool MatchPatternSet::Overlaps(const MatchPattern& aPattern) const {
  for (const auto& pattern : mPatterns) {
    if (pattern->Overlaps(aPattern)) {
      return true;
    }
  }
  return false;
}

bool MatchPatternSet::OverlapsAll(const MatchPatternSet& aPatternSet) const {
  for (const auto& pattern : aPatternSet.mPatterns) {
    if (!Overlaps(*pattern)) {
      return false;
    }
  }
  return aPatternSet.mPatterns.Length() > 0;
}

JSObject* MatchPatternSet::WrapObject(JSContext* aCx,
                                      JS::HandleObject aGivenProto) {
  return MatchPatternSet_Binding::Wrap(aCx, this, aGivenProto);
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(MatchPatternSet, mPatterns, mParent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MatchPatternSet)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(MatchPatternSet)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MatchPatternSet)

/*****************************************************************************
 * MatchGlob
 *****************************************************************************/

MatchGlob::~MatchGlob() { mozilla::DropJSObjects(this); }

/* static */
already_AddRefed<MatchGlob> MatchGlob::Constructor(dom::GlobalObject& aGlobal,
                                                   const nsAString& aGlob,
                                                   bool aAllowQuestion,
                                                   ErrorResult& aRv) {
  RefPtr<MatchGlob> glob = new MatchGlob(aGlobal.GetAsSupports());
  glob->Init(aGlobal.Context(), aGlob, aAllowQuestion, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  return glob.forget();
}

void MatchGlob::Init(JSContext* aCx, const nsAString& aGlob,
                     bool aAllowQuestion, ErrorResult& aRv) {
  mGlob = aGlob;

  // Check for a literal match with no glob metacharacters.
  auto index = mGlob.FindCharInSet(aAllowQuestion ? "*?" : "*");
  if (index < 0) {
    mPathLiteral = mGlob;
    return;
  }

  // Check for a prefix match, where the only glob metacharacter is a "*"
  // at the end of the string.
  if (index == (int32_t)mGlob.Length() - 1 && mGlob[index] == '*') {
    mPathLiteral = StringHead(mGlob, index);
    mIsPrefix = true;
    return;
  }

  // Fall back to the regexp slow path.
  constexpr auto metaChars = ".+*?^${}()|[]\\"_ns;

  nsAutoString escaped;
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

  // TODO: Switch to the Rust regexp crate, when Rust integration is easier.
  // It uses a much more efficient, linear time matching algorithm, and
  // doesn't require special casing for the literal and prefix cases.
  mRegExp = JS::NewUCRegExpObject(aCx, escaped.get(), escaped.Length(), 0);
  if (mRegExp) {
    mozilla::HoldJSObjects(this);
  } else {
    aRv.NoteJSContextException(aCx);
  }
}

bool MatchGlob::Matches(const nsAString& aString) const {
  if (mRegExp) {
    AutoJSAPI jsapi;
    jsapi.Init();
    JSContext* cx = jsapi.cx();

    JSAutoRealm ar(cx, mRegExp);

    JS::RootedObject regexp(cx, mRegExp);
    JS::RootedValue result(cx);

    nsString input(aString);

    size_t index = 0;
    if (!JS::ExecuteRegExpNoStatics(cx, regexp, input.BeginWriting(),
                                    aString.Length(), &index, true, &result)) {
      return false;
    }

    return result.isBoolean() && result.toBoolean();
  }

  if (mIsPrefix) {
    return mPathLiteral == StringHead(aString, mPathLiteral.Length());
  }

  return mPathLiteral == aString;
}

JSObject* MatchGlob::WrapObject(JSContext* aCx, JS::HandleObject aGivenProto) {
  return MatchGlob_Binding::Wrap(aCx, this, aGivenProto);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(MatchGlob)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(MatchGlob)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mParent)
  tmp->mRegExp = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(MatchGlob)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParent)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(MatchGlob)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mRegExp)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MatchGlob)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(MatchGlob)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MatchGlob)

/*****************************************************************************
 * MatchGlobSet
 *****************************************************************************/

bool MatchGlobSet::Matches(const nsAString& aValue) const {
  for (auto& glob : *this) {
    if (glob->Matches(aValue)) {
      return true;
    }
  }
  return false;
}

}  // namespace extensions
}  // namespace mozilla
