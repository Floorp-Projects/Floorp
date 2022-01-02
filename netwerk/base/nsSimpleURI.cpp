/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"

#undef LOG
#include "ipc/IPCMessageUtils.h"

#include "nsSimpleURI.h"
#include "nscore.h"
#include "nsCRT.h"
#include "nsString.h"
#include "plstr.h"
#include "nsURLHelper.h"
#include "nsNetCID.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsEscape.h"
#include "nsError.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/TextUtils.h"
#include "mozilla/ipc/URIUtils.h"
#include "nsIClassInfoImpl.h"
#include "nsIURIMutator.h"
#include "mozilla/net/MozURL.h"

using namespace mozilla::ipc;

namespace mozilla {
namespace net {

static NS_DEFINE_CID(kThisSimpleURIImplementationCID,
                     NS_THIS_SIMPLEURI_IMPLEMENTATION_CID);

/* static */
already_AddRefed<nsSimpleURI> nsSimpleURI::From(nsIURI* aURI) {
  RefPtr<nsSimpleURI> uri;
  nsresult rv = aURI->QueryInterface(kThisSimpleURIImplementationCID,
                                     getter_AddRefs(uri));
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  return uri.forget();
}

NS_IMPL_CLASSINFO(nsSimpleURI, nullptr, nsIClassInfo::THREADSAFE,
                  NS_SIMPLEURI_CID)
// Empty CI getter. We only need nsIClassInfo for Serialization
NS_IMPL_CI_INTERFACE_GETTER0(nsSimpleURI)

////////////////////////////////////////////////////////////////////////////////
// nsSimpleURI methods:

NS_IMPL_ADDREF(nsSimpleURI)
NS_IMPL_RELEASE(nsSimpleURI)
NS_INTERFACE_TABLE_HEAD(nsSimpleURI)
  NS_INTERFACE_TABLE(nsSimpleURI, nsIURI, nsISerializable)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE
  NS_IMPL_QUERY_CLASSINFO(nsSimpleURI)
  if (aIID.Equals(kThisSimpleURIImplementationCID)) {
    foundInterface = static_cast<nsIURI*>(this);
  } else
    NS_INTERFACE_MAP_ENTRY(nsISizeOf)
NS_INTERFACE_MAP_END

////////////////////////////////////////////////////////////////////////////////
// nsISerializable methods:

NS_IMETHODIMP
nsSimpleURI::Read(nsIObjectInputStream* aStream) {
  MOZ_ASSERT_UNREACHABLE("Use nsIURIMutator.read() instead");
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsSimpleURI::ReadPrivate(nsIObjectInputStream* aStream) {
  nsresult rv;

  bool isMutable;
  rv = aStream->ReadBoolean(&isMutable);
  if (NS_FAILED(rv)) return rv;
  Unused << isMutable;

  rv = aStream->ReadCString(mScheme);
  if (NS_FAILED(rv)) return rv;

  rv = aStream->ReadCString(mPath);
  if (NS_FAILED(rv)) return rv;

  bool isRefValid;
  rv = aStream->ReadBoolean(&isRefValid);
  if (NS_FAILED(rv)) return rv;
  mIsRefValid = isRefValid;

  if (isRefValid) {
    rv = aStream->ReadCString(mRef);
    if (NS_FAILED(rv)) return rv;
  } else {
    mRef.Truncate();  // invariant: mRef should be empty when it's not valid
  }

  bool isQueryValid;
  rv = aStream->ReadBoolean(&isQueryValid);
  if (NS_FAILED(rv)) return rv;
  mIsQueryValid = isQueryValid;

  if (isQueryValid) {
    rv = aStream->ReadCString(mQuery);
    if (NS_FAILED(rv)) return rv;
  } else {
    mQuery.Truncate();  // invariant: mQuery should be empty when it's not valid
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::Write(nsIObjectOutputStream* aStream) {
  nsresult rv;

  rv = aStream->WriteBoolean(false);  // former mMutable
  if (NS_FAILED(rv)) return rv;

  rv = aStream->WriteStringZ(mScheme.get());
  if (NS_FAILED(rv)) return rv;

  rv = aStream->WriteStringZ(mPath.get());
  if (NS_FAILED(rv)) return rv;

  rv = aStream->WriteBoolean(mIsRefValid);
  if (NS_FAILED(rv)) return rv;

  if (mIsRefValid) {
    rv = aStream->WriteStringZ(mRef.get());
    if (NS_FAILED(rv)) return rv;
  }

  rv = aStream->WriteBoolean(mIsQueryValid);
  if (NS_FAILED(rv)) return rv;

  if (mIsQueryValid) {
    rv = aStream->WriteStringZ(mQuery.get());
    if (NS_FAILED(rv)) return rv;
  }

  return NS_OK;
}

void nsSimpleURI::Serialize(URIParams& aParams) {
  SimpleURIParams params;

  params.scheme() = mScheme;
  params.path() = mPath;

  if (mIsRefValid) {
    params.ref() = mRef;
  } else {
    params.ref().SetIsVoid(true);
  }

  if (mIsQueryValid) {
    params.query() = mQuery;
  } else {
    params.query().SetIsVoid(true);
  }

  aParams = params;
}

bool nsSimpleURI::Deserialize(const URIParams& aParams) {
  if (aParams.type() != URIParams::TSimpleURIParams) {
    NS_ERROR("Received unknown parameters from the other process!");
    return false;
  }

  const SimpleURIParams& params = aParams.get_SimpleURIParams();

  mScheme = params.scheme();
  mPath = params.path();

  if (params.ref().IsVoid()) {
    mRef.Truncate();
    mIsRefValid = false;
  } else {
    mRef = params.ref();
    mIsRefValid = true;
  }

  if (params.query().IsVoid()) {
    mQuery.Truncate();
    mIsQueryValid = false;
  } else {
    mQuery = params.query();
    mIsQueryValid = true;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
// nsIURI methods:

NS_IMETHODIMP
nsSimpleURI::GetSpec(nsACString& result) {
  if (!result.Assign(mScheme, fallible) || !result.Append(":"_ns, fallible) ||
      !result.Append(mPath, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (mIsQueryValid) {
    if (!result.Append("?"_ns, fallible) || !result.Append(mQuery, fallible)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  } else {
    MOZ_ASSERT(mQuery.IsEmpty(), "mIsQueryValid/mQuery invariant broken");
  }

  if (mIsRefValid) {
    if (!result.Append("#"_ns, fallible) || !result.Append(mRef, fallible)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  } else {
    MOZ_ASSERT(mRef.IsEmpty(), "mIsRefValid/mRef invariant broken");
  }

  return NS_OK;
}

// result may contain unescaped UTF-8 characters
NS_IMETHODIMP
nsSimpleURI::GetSpecIgnoringRef(nsACString& result) {
  result = mScheme + ":"_ns + mPath;
  if (mIsQueryValid) {
    result += "?"_ns + mQuery;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::GetDisplaySpec(nsACString& aUnicodeSpec) {
  return GetSpec(aUnicodeSpec);
}

NS_IMETHODIMP
nsSimpleURI::GetDisplayHostPort(nsACString& aUnicodeHostPort) {
  return GetHostPort(aUnicodeHostPort);
}

NS_IMETHODIMP
nsSimpleURI::GetDisplayHost(nsACString& aUnicodeHost) {
  return GetHost(aUnicodeHost);
}

NS_IMETHODIMP
nsSimpleURI::GetDisplayPrePath(nsACString& aPrePath) {
  return GetPrePath(aPrePath);
}

NS_IMETHODIMP
nsSimpleURI::GetHasRef(bool* result) {
  *result = mIsRefValid;
  return NS_OK;
}

nsresult nsSimpleURI::SetSpecInternal(const nsACString& aSpec,
                                      bool aStripWhitespace) {
  if (StaticPrefs::network_url_max_length() &&
      aSpec.Length() > StaticPrefs::network_url_max_length()) {
    return NS_ERROR_MALFORMED_URI;
  }

  nsresult rv = net_ExtractURLScheme(aSpec, mScheme);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsAutoCString spec;
  rv = net_FilterAndEscapeURI(
      aSpec, esc_OnlyNonASCII,
      aStripWhitespace ? ASCIIMask::MaskWhitespace() : ASCIIMask::MaskCRLFTab(),
      spec);
  if (NS_FAILED(rv)) {
    return rv;
  }

  int32_t colonPos = spec.FindChar(':');
  MOZ_ASSERT(colonPos != kNotFound, "A colon should be in this string");
  // This sets mPath, mQuery and mRef.
  return SetPathQueryRefInternal(Substring(spec, colonPos + 1));
}

NS_IMETHODIMP
nsSimpleURI::GetScheme(nsACString& result) {
  result = mScheme;
  return NS_OK;
}

nsresult nsSimpleURI::SetScheme(const nsACString& scheme) {
  const nsPromiseFlatCString& flat = PromiseFlatCString(scheme);
  if (!net_IsValidScheme(flat)) {
    NS_WARNING("the given url scheme contains invalid characters");
    return NS_ERROR_MALFORMED_URI;
  }

  mScheme = scheme;
  ToLowerCase(mScheme);
  return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::GetPrePath(nsACString& result) {
  result = mScheme + ":"_ns;
  return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::GetUserPass(nsACString& result) { return NS_ERROR_FAILURE; }

nsresult nsSimpleURI::SetUserPass(const nsACString& userPass) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleURI::GetUsername(nsACString& result) { return NS_ERROR_FAILURE; }

nsresult nsSimpleURI::SetUsername(const nsACString& userName) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleURI::GetPassword(nsACString& result) { return NS_ERROR_FAILURE; }

nsresult nsSimpleURI::SetPassword(const nsACString& password) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleURI::GetHostPort(nsACString& result) {
  // Note: Audit all callers before changing this to return an empty
  // string -- CAPS and UI code may depend on this throwing.
  // Note: If this is changed, change GetAsciiHostPort as well.
  return NS_ERROR_FAILURE;
}

nsresult nsSimpleURI::SetHostPort(const nsACString& result) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleURI::GetHost(nsACString& result) {
  // Note: Audit all callers before changing this to return an empty
  // string -- CAPS and UI code depend on this throwing.
  return NS_ERROR_FAILURE;
}

nsresult nsSimpleURI::SetHost(const nsACString& host) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleURI::GetPort(int32_t* result) {
  // Note: Audit all callers before changing this to return an empty
  // string -- CAPS and UI code may depend on this throwing.
  return NS_ERROR_FAILURE;
}

nsresult nsSimpleURI::SetPort(int32_t port) { return NS_ERROR_FAILURE; }

NS_IMETHODIMP
nsSimpleURI::GetPathQueryRef(nsACString& result) {
  result = mPath;
  if (mIsQueryValid) {
    result += "?"_ns + mQuery;
  }
  if (mIsRefValid) {
    result += "#"_ns + mRef;
  }

  return NS_OK;
}

nsresult nsSimpleURI::SetPathQueryRef(const nsACString& aPath) {
  if (StaticPrefs::network_url_max_length() &&
      aPath.Length() > StaticPrefs::network_url_max_length()) {
    return NS_ERROR_MALFORMED_URI;
  }

  nsAutoCString path;
  nsresult rv = NS_EscapeURL(aPath, esc_OnlyNonASCII, path, fallible);
  if (NS_FAILED(rv)) {
    return rv;
  }
  return SetPathQueryRefInternal(path);
}

nsresult nsSimpleURI::SetPathQueryRefInternal(const nsACString& aPath) {
  nsresult rv;
  const auto* start = aPath.BeginReading();
  const auto* end = aPath.EndReading();

  // Find the first instance of ? or # that marks the end of the path.
  auto hashOrQueryFilter = [](char c) { return c == '?' || c == '#'; };
  const auto* pathEnd = std::find_if(start, end, hashOrQueryFilter);

  mIsQueryValid = false;
  mQuery.Truncate();

  mIsRefValid = false;
  mRef.Truncate();

  // The path
  if (!mPath.Assign(Substring(start, pathEnd), fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (pathEnd == end) {
    return NS_OK;
  }

  const auto* queryEnd =
      std::find_if(pathEnd, end, [](char c) { return c == '#'; });

  rv = SetQuery(Substring(pathEnd, queryEnd));
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (queryEnd == end) {
    return NS_OK;
  }

  return SetRef(Substring(queryEnd, end));
}

NS_IMETHODIMP
nsSimpleURI::GetRef(nsACString& result) {
  if (!mIsRefValid) {
    MOZ_ASSERT(mRef.IsEmpty(), "mIsRefValid/mRef invariant broken");
    result.Truncate();
  } else {
    result = mRef;
  }

  return NS_OK;
}

// NOTE: SetRef("") removes our ref, whereas SetRef("#") sets it to the empty
// string (and will result in .spec and .path having a terminal #).
nsresult nsSimpleURI::SetRef(const nsACString& aRef) {
  if (StaticPrefs::network_url_max_length() &&
      aRef.Length() > StaticPrefs::network_url_max_length()) {
    return NS_ERROR_MALFORMED_URI;
  }

  nsAutoCString ref;
  nsresult rv =
      NS_EscapeURL(aRef, esc_OnlyNonASCII | esc_Spaces, ref, fallible);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (ref.IsEmpty()) {
    // Empty string means to remove ref completely.
    mIsRefValid = false;
    mRef.Truncate();  // invariant: mRef should be empty when it's not valid
    return NS_OK;
  }

  mIsRefValid = true;

  // Gracefully skip initial hash
  if (ref[0] == '#') {
    mRef = Substring(ref, 1);
  } else {
    mRef = ref;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::Equals(nsIURI* other, bool* result) {
  return EqualsInternal(other, eHonorRef, result);
}

NS_IMETHODIMP
nsSimpleURI::EqualsExceptRef(nsIURI* other, bool* result) {
  return EqualsInternal(other, eIgnoreRef, result);
}

/* virtual */
nsresult nsSimpleURI::EqualsInternal(
    nsIURI* other, nsSimpleURI::RefHandlingEnum refHandlingMode, bool* result) {
  NS_ENSURE_ARG_POINTER(other);
  MOZ_ASSERT(result, "null pointer");

  RefPtr<nsSimpleURI> otherUri;
  nsresult rv = other->QueryInterface(kThisSimpleURIImplementationCID,
                                      getter_AddRefs(otherUri));
  if (NS_FAILED(rv)) {
    *result = false;
    return NS_OK;
  }

  *result = EqualsInternal(otherUri, refHandlingMode);
  return NS_OK;
}

bool nsSimpleURI::EqualsInternal(nsSimpleURI* otherUri,
                                 RefHandlingEnum refHandlingMode) {
  bool result = (mScheme == otherUri->mScheme && mPath == otherUri->mPath);

  if (result) {
    result = (mIsQueryValid == otherUri->mIsQueryValid &&
              (!mIsQueryValid || mQuery == otherUri->mQuery));
  }

  if (result && refHandlingMode == eHonorRef) {
    result = (mIsRefValid == otherUri->mIsRefValid &&
              (!mIsRefValid || mRef == otherUri->mRef));
  }

  return result;
}

NS_IMETHODIMP
nsSimpleURI::SchemeIs(const char* i_Scheme, bool* o_Equals) {
  MOZ_ASSERT(o_Equals, "null pointer");
  if (!i_Scheme) {
    *o_Equals = false;
    return NS_OK;
  }

  const char* this_scheme = mScheme.get();

  // mScheme is guaranteed to be lower case.
  if (*i_Scheme == *this_scheme || *i_Scheme == (*this_scheme - ('a' - 'A'))) {
    *o_Equals = nsCRT::strcasecmp(this_scheme, i_Scheme) == 0;
  } else {
    *o_Equals = false;
  }

  return NS_OK;
}

/* virtual */ nsSimpleURI* nsSimpleURI::StartClone(
    nsSimpleURI::RefHandlingEnum refHandlingMode, const nsACString& newRef) {
  nsSimpleURI* url = new nsSimpleURI();
  SetRefOnClone(url, refHandlingMode, newRef);
  return url;
}

/* virtual */
void nsSimpleURI::SetRefOnClone(nsSimpleURI* url,
                                nsSimpleURI::RefHandlingEnum refHandlingMode,
                                const nsACString& newRef) {
  if (refHandlingMode == eHonorRef) {
    url->mRef = mRef;
    url->mIsRefValid = mIsRefValid;
  } else if (refHandlingMode == eReplaceRef) {
    url->SetRef(newRef);
  }
}

nsresult nsSimpleURI::Clone(nsIURI** result) {
  return CloneInternal(eHonorRef, ""_ns, result);
}

nsresult nsSimpleURI::CloneInternal(
    nsSimpleURI::RefHandlingEnum refHandlingMode, const nsACString& newRef,
    nsIURI** result) {
  RefPtr<nsSimpleURI> url = StartClone(refHandlingMode, newRef);
  if (!url) return NS_ERROR_OUT_OF_MEMORY;

  url->mScheme = mScheme;
  url->mPath = mPath;

  url->mIsQueryValid = mIsQueryValid;
  if (url->mIsQueryValid) {
    url->mQuery = mQuery;
  }

  url.forget(result);
  return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::Resolve(const nsACString& relativePath, nsACString& result) {
  nsAutoCString scheme;
  nsresult rv = net_ExtractURLScheme(relativePath, scheme);
  if (NS_SUCCEEDED(rv)) {
    result = relativePath;
    return NS_OK;
  }

  nsAutoCString spec;
  rv = GetAsciiSpec(spec);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    // If getting the spec fails for some reason, preserve behaviour and just
    // return the relative path.
    result = relativePath;
    return NS_OK;
  }

  RefPtr<MozURL> url;
  rv = MozURL::Init(getter_AddRefs(url), spec);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    // If parsing the current url fails, we revert to the previous behaviour
    // and just return the relative path.
    result = relativePath;
    return NS_OK;
  }

  RefPtr<MozURL> url2;
  rv = MozURL::Init(getter_AddRefs(url2), relativePath, url);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    // If parsing the relative url fails, we revert to the previous behaviour
    // and just return the relative path.
    result = relativePath;
    return NS_OK;
  }

  result = url2->Spec();
  return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::GetAsciiSpec(nsACString& aResult) {
  nsresult rv = GetSpec(aResult);
  if (NS_FAILED(rv)) return rv;
  MOZ_ASSERT(IsAscii(aResult), "The spec should be ASCII");
  return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::GetAsciiHostPort(nsACString& result) {
  // XXX This behavior mimics GetHostPort.
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleURI::GetAsciiHost(nsACString& result) {
  result.Truncate();
  return NS_OK;
}

//----------------------------------------------------------------------------
// nsSimpleURI::nsISizeOf
//----------------------------------------------------------------------------

size_t nsSimpleURI::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const {
  return mScheme.SizeOfExcludingThisIfUnshared(aMallocSizeOf) +
         mPath.SizeOfExcludingThisIfUnshared(aMallocSizeOf) +
         mQuery.SizeOfExcludingThisIfUnshared(aMallocSizeOf) +
         mRef.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
}

size_t nsSimpleURI::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

NS_IMETHODIMP
nsSimpleURI::GetFilePath(nsACString& aFilePath) {
  aFilePath = mPath;
  return NS_OK;
}

nsresult nsSimpleURI::SetFilePath(const nsACString& aFilePath) {
  if (mPath.IsEmpty() || mPath.First() != '/') {
    // cannot-be-a-base
    return NS_ERROR_MALFORMED_URI;
  }
  const char* current = aFilePath.BeginReading();
  const char* end = aFilePath.EndReading();

  // Only go up to the first ? or # symbol
  for (; current < end; ++current) {
    if (*current == '?' || *current == '#') {
      break;
    }
  }
  return SetPathQueryRef(
      nsDependentCSubstring(aFilePath.BeginReading(), current));
}

NS_IMETHODIMP
nsSimpleURI::GetQuery(nsACString& aQuery) {
  if (!mIsQueryValid) {
    MOZ_ASSERT(mQuery.IsEmpty(), "mIsQueryValid/mQuery invariant broken");
    aQuery.Truncate();
  } else {
    aQuery = mQuery;
  }
  return NS_OK;
}

nsresult nsSimpleURI::SetQuery(const nsACString& aQuery) {
  if (StaticPrefs::network_url_max_length() &&
      aQuery.Length() > StaticPrefs::network_url_max_length()) {
    return NS_ERROR_MALFORMED_URI;
  }
  nsAutoCString query;
  nsresult rv = NS_EscapeURL(aQuery, esc_OnlyNonASCII, query, fallible);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (query.IsEmpty()) {
    // Empty string means to remove query completely.
    mIsQueryValid = false;
    mQuery.Truncate();  // invariant: mQuery should be empty when it's not valid
    return NS_OK;
  }

  mIsQueryValid = true;

  // Gracefully skip initial question mark
  if (query[0] == '?') {
    mQuery = Substring(query, 1);
  } else {
    mQuery = query;
  }

  return NS_OK;
}

nsresult nsSimpleURI::SetQueryWithEncoding(const nsACString& aQuery,
                                           const Encoding* aEncoding) {
  return SetQuery(aQuery);
}

// Queries this list of interfaces. If none match, it queries mURI.
NS_IMPL_NSIURIMUTATOR_ISUPPORTS(nsSimpleURI::Mutator, nsIURISetters,
                                nsIURIMutator, nsISerializable,
                                nsISimpleURIMutator)

NS_IMETHODIMP
nsSimpleURI::Mutate(nsIURIMutator** aMutator) {
  RefPtr<nsSimpleURI::Mutator> mutator = new nsSimpleURI::Mutator();
  nsresult rv = mutator->InitFromURI(this);
  if (NS_FAILED(rv)) {
    return rv;
  }
  mutator.forget(aMutator);
  return NS_OK;
}

}  // namespace net
}  // namespace mozilla
