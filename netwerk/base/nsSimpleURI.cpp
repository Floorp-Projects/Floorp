/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"

#undef LOG
#include "IPCMessageUtils.h"

#include "nsSimpleURI.h"
#include "nscore.h"
#include "nsString.h"
#include "plstr.h"
#include "nsURLHelper.h"
#include "nsNetCID.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsEscape.h"
#include "nsError.h"
#include "nsIIPCSerializableURI.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/ipc/URIUtils.h"

using namespace mozilla::ipc;

namespace mozilla {
namespace net {

static NS_DEFINE_CID(kThisSimpleURIImplementationCID,
                     NS_THIS_SIMPLEURI_IMPLEMENTATION_CID);
static NS_DEFINE_CID(kSimpleURICID, NS_SIMPLEURI_CID);

////////////////////////////////////////////////////////////////////////////////
// nsSimpleURI methods:

nsSimpleURI::nsSimpleURI()
    : mMutable(true)
    , mIsRefValid(false)
    , mIsQueryValid(false)
{
}

nsSimpleURI::~nsSimpleURI()
{
}

NS_IMPL_ADDREF(nsSimpleURI)
NS_IMPL_RELEASE(nsSimpleURI)
NS_INTERFACE_TABLE_HEAD(nsSimpleURI)
NS_INTERFACE_TABLE(nsSimpleURI, nsIURI, nsISerializable,
                   nsIClassInfo, nsIMutable, nsIIPCSerializableURI)
NS_INTERFACE_TABLE_TO_MAP_SEGUE
  if (aIID.Equals(kThisSimpleURIImplementationCID))
    foundInterface = static_cast<nsIURI*>(this);
  else
  NS_INTERFACE_MAP_ENTRY(nsISizeOf)
NS_INTERFACE_MAP_END

////////////////////////////////////////////////////////////////////////////////
// nsISerializable methods:

NS_IMETHODIMP
nsSimpleURI::Read(nsIObjectInputStream* aStream)
{
    nsresult rv;

    bool isMutable;
    rv = aStream->ReadBoolean(&isMutable);
    if (NS_FAILED(rv)) return rv;
    mMutable = isMutable;

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
        mRef.Truncate(); // invariant: mRef should be empty when it's not valid
    }

    bool isQueryValid;
    rv = aStream->ReadBoolean(&isQueryValid);
    if (NS_FAILED(rv)) return rv;
    mIsQueryValid = isQueryValid;

    if (isQueryValid) {
        rv = aStream->ReadCString(mQuery);
        if (NS_FAILED(rv)) return rv;
    } else {
        mQuery.Truncate(); // invariant: mQuery should be empty when it's not valid
    }

    return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::Write(nsIObjectOutputStream* aStream)
{
    nsresult rv;

    rv = aStream->WriteBoolean(mMutable);
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

////////////////////////////////////////////////////////////////////////////////
// nsIIPCSerializableURI methods:

void
nsSimpleURI::Serialize(URIParams& aParams)
{
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

    params.isMutable() = mMutable;

    aParams = params;
}

bool
nsSimpleURI::Deserialize(const URIParams& aParams)
{
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

    mMutable = params.isMutable();

    return true;
}

////////////////////////////////////////////////////////////////////////////////
// nsIURI methods:

NS_IMETHODIMP
nsSimpleURI::GetSpec(nsACString &result)
{
    if (!result.Assign(mScheme, fallible) ||
        !result.Append(NS_LITERAL_CSTRING(":"), fallible) ||
        !result.Append(mPath, fallible)) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    if (mIsQueryValid) {
        if (!result.Append(NS_LITERAL_CSTRING("?"), fallible) ||
            !result.Append(mQuery, fallible)) {
            return NS_ERROR_OUT_OF_MEMORY;
        }
    } else {
        MOZ_ASSERT(mQuery.IsEmpty(), "mIsQueryValid/mQuery invariant broken");
    }

    if (mIsRefValid) {
        if (!result.Append(NS_LITERAL_CSTRING("#"), fallible) ||
            !result.Append(mRef, fallible)) {
            return NS_ERROR_OUT_OF_MEMORY;
        }
    } else {
        MOZ_ASSERT(mRef.IsEmpty(), "mIsRefValid/mRef invariant broken");
    }

    return NS_OK;
}

// result may contain unescaped UTF-8 characters
NS_IMETHODIMP
nsSimpleURI::GetSpecIgnoringRef(nsACString &result)
{
    result = mScheme + NS_LITERAL_CSTRING(":") + mPath;
    if (mIsQueryValid) {
        result += NS_LITERAL_CSTRING("?") + mQuery;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::GetDisplaySpec(nsACString &aUnicodeSpec)
{
    return GetSpec(aUnicodeSpec);
}

NS_IMETHODIMP
nsSimpleURI::GetDisplayHostPort(nsACString &aUnicodeHostPort)
{
    return GetHostPort(aUnicodeHostPort);
}

NS_IMETHODIMP
nsSimpleURI::GetDisplayHost(nsACString &aUnicodeHost)
{
    return GetHost(aUnicodeHost);
}

NS_IMETHODIMP
nsSimpleURI::GetHasRef(bool *result)
{
    *result = mIsRefValid;
    return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::SetSpec(const nsACString &aSpec)
{
    NS_ENSURE_STATE(mMutable);

    // filter out unexpected chars "\r\n\t" if necessary
    nsAutoCString filteredSpec;
    net_FilterURIString(aSpec, filteredSpec);

    // nsSimpleURI currently restricts the charset to US-ASCII
    nsAutoCString spec;
    nsresult rv = NS_EscapeURL(filteredSpec, esc_OnlyNonASCII, spec, fallible);
    if (NS_FAILED(rv)) {
      return rv;
    }

    int32_t colonPos = spec.FindChar(':');
    if (colonPos < 0 || !net_IsValidScheme(spec.get(), colonPos))
        return NS_ERROR_MALFORMED_URI;

    mScheme.Truncate();
    DebugOnly<int32_t> n = spec.Left(mScheme, colonPos);
    NS_ASSERTION(n == colonPos, "Left failed");
    ToLowerCase(mScheme);

    // This sets mPath, mQuery and mRef.
    return SetPath(Substring(spec, colonPos + 1));
}

NS_IMETHODIMP
nsSimpleURI::GetScheme(nsACString &result)
{
    result = mScheme;
    return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::SetScheme(const nsACString &scheme)
{
    NS_ENSURE_STATE(mMutable);

    const nsPromiseFlatCString &flat = PromiseFlatCString(scheme);
    if (!net_IsValidScheme(flat)) {
        NS_WARNING("the given url scheme contains invalid characters");
        return NS_ERROR_MALFORMED_URI;
    }

    mScheme = scheme;
    ToLowerCase(mScheme);
    return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::GetPrePath(nsACString &result)
{
    result = mScheme + NS_LITERAL_CSTRING(":");
    return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::GetUserPass(nsACString &result)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleURI::SetUserPass(const nsACString &userPass)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleURI::GetUsername(nsACString &result)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleURI::SetUsername(const nsACString &userName)
{
    NS_ENSURE_STATE(mMutable);

    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleURI::GetPassword(nsACString &result)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleURI::SetPassword(const nsACString &password)
{
    NS_ENSURE_STATE(mMutable);

    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleURI::GetHostPort(nsACString &result)
{
    // Note: Audit all callers before changing this to return an empty
    // string -- CAPS and UI code may depend on this throwing.
    // Note: If this is changed, change GetAsciiHostPort as well.
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleURI::SetHostPort(const nsACString &result)
{
    NS_ENSURE_STATE(mMutable);

    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleURI::SetHostAndPort(const nsACString &result)
{
    NS_ENSURE_STATE(mMutable);

    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleURI::GetHost(nsACString &result)
{
    // Note: Audit all callers before changing this to return an empty
    // string -- CAPS and UI code depend on this throwing.
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleURI::SetHost(const nsACString &host)
{
    NS_ENSURE_STATE(mMutable);

    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleURI::GetPort(int32_t *result)
{
    // Note: Audit all callers before changing this to return an empty
    // string -- CAPS and UI code may depend on this throwing.
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleURI::SetPort(int32_t port)
{
    NS_ENSURE_STATE(mMutable);

    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleURI::GetPath(nsACString &result)
{
    result = mPath;
    if (mIsQueryValid) {
        result += NS_LITERAL_CSTRING("?") + mQuery;
    }
    if (mIsRefValid) {
        result += NS_LITERAL_CSTRING("#") + mRef;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::SetPath(const nsACString &aPath)
{
    NS_ENSURE_STATE(mMutable);

    nsAutoCString path;
    if (!path.Assign(aPath, fallible)) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    int32_t queryPos = path.FindChar('?');
    int32_t hashPos = path.FindChar('#');

    if (queryPos != kNotFound && hashPos != kNotFound && hashPos < queryPos) {
        queryPos = kNotFound;
    }

    nsAutoCString query;
    if (queryPos != kNotFound) {
        query.Assign(Substring(path, queryPos));
        path.Truncate(queryPos);
    }

    nsAutoCString hash;
    if (hashPos != kNotFound) {
        if (query.IsEmpty()) {
            hash.Assign(Substring(path, hashPos));
            path.Truncate(hashPos);
        } else {
            // We have to search the hash character in the query
            hashPos = query.FindChar('#');
            hash.Assign(Substring(query, hashPos));
            query.Truncate(hashPos);
        }
    }

    mIsQueryValid = false;
    mQuery.Truncate();

    mIsRefValid = false;
    mRef.Truncate();

    // The path
    if (!mPath.Assign(path, fallible)) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    nsresult rv = SetQuery(query);
    if (NS_FAILED(rv)) {
        return rv;
    }

    return SetRef(hash);
}

NS_IMETHODIMP
nsSimpleURI::GetRef(nsACString &result)
{
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
NS_IMETHODIMP
nsSimpleURI::SetRef(const nsACString &aRef)
{
    NS_ENSURE_STATE(mMutable);

    nsAutoCString ref;
    nsresult rv = NS_EscapeURL(aRef, esc_OnlyNonASCII, ref, fallible);
    if (NS_FAILED(rv)) {
        return rv;
    }

    if (ref.IsEmpty()) {
        // Empty string means to remove ref completely.
        mIsRefValid = false;
        mRef.Truncate(); // invariant: mRef should be empty when it's not valid
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
nsSimpleURI::Equals(nsIURI* other, bool *result)
{
    return EqualsInternal(other, eHonorRef, result);
}

NS_IMETHODIMP
nsSimpleURI::EqualsExceptRef(nsIURI* other, bool *result)
{
    return EqualsInternal(other, eIgnoreRef, result);
}

/* virtual */ nsresult
nsSimpleURI::EqualsInternal(nsIURI* other,
                            nsSimpleURI::RefHandlingEnum refHandlingMode,
                            bool* result)
{
    NS_ENSURE_ARG_POINTER(other);
    NS_PRECONDITION(result, "null pointer");

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

bool
nsSimpleURI::EqualsInternal(nsSimpleURI* otherUri, RefHandlingEnum refHandlingMode)
{
    bool result = (mScheme == otherUri->mScheme &&
                   mPath   == otherUri->mPath);

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
nsSimpleURI::SchemeIs(const char *i_Scheme, bool *o_Equals)
{
    NS_ENSURE_ARG_POINTER(o_Equals);
    if (!i_Scheme) return NS_ERROR_NULL_POINTER;

    const char *this_scheme = mScheme.get();

    // mScheme is guaranteed to be lower case.
    if (*i_Scheme == *this_scheme || *i_Scheme == (*this_scheme - ('a' - 'A')) ) {
        *o_Equals = PL_strcasecmp(this_scheme, i_Scheme) ? false : true;
    } else {
        *o_Equals = false;
    }

    return NS_OK;
}

/* virtual */ nsSimpleURI*
nsSimpleURI::StartClone(nsSimpleURI::RefHandlingEnum refHandlingMode,
                        const nsACString& newRef)
{
    nsSimpleURI* url = new nsSimpleURI();
    SetRefOnClone(url, refHandlingMode, newRef);
    return url;
}

/* virtual */ void
nsSimpleURI::SetRefOnClone(nsSimpleURI* url,
                           nsSimpleURI::RefHandlingEnum refHandlingMode,
                           const nsACString& newRef)
{
    if (refHandlingMode == eHonorRef) {
        url->mRef = mRef;
        url->mIsRefValid = mIsRefValid;
    } else if (refHandlingMode == eReplaceRef) {
        url->SetRef(newRef);
    }
}

NS_IMETHODIMP
nsSimpleURI::Clone(nsIURI** result)
{
    return CloneInternal(eHonorRef, EmptyCString(), result);
}

NS_IMETHODIMP
nsSimpleURI::CloneIgnoringRef(nsIURI** result)
{
    return CloneInternal(eIgnoreRef, EmptyCString(), result);
}

NS_IMETHODIMP
nsSimpleURI::CloneWithNewRef(const nsACString &newRef, nsIURI** result)
{
    return CloneInternal(eReplaceRef, newRef, result);
}

nsresult
nsSimpleURI::CloneInternal(nsSimpleURI::RefHandlingEnum refHandlingMode,
                           const nsACString &newRef,
                           nsIURI** result)
{
    RefPtr<nsSimpleURI> url = StartClone(refHandlingMode, newRef);
    if (!url)
        return NS_ERROR_OUT_OF_MEMORY;

    // Note: |url| may well have mMutable false at this point, so
    // don't call any setter methods.
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
nsSimpleURI::Resolve(const nsACString &relativePath, nsACString &result)
{
    result = relativePath;
    return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::GetAsciiSpec(nsACString &result)
{
    nsAutoCString buf;
    nsresult rv = GetSpec(buf);
    if (NS_FAILED(rv)) return rv;
    return NS_EscapeURL(buf, esc_OnlyNonASCII|esc_AlwaysCopy, result, fallible);
}

NS_IMETHODIMP
nsSimpleURI::GetAsciiHostPort(nsACString &result)
{
    // XXX This behavior mimics GetHostPort.
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleURI::GetAsciiHost(nsACString &result)
{
    result.Truncate();
    return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::GetOriginCharset(nsACString &result)
{
    result.Truncate();
    return NS_OK;
}

//----------------------------------------------------------------------------
// nsSimpleURI::nsIClassInfo
//----------------------------------------------------------------------------

NS_IMETHODIMP
nsSimpleURI::GetInterfaces(uint32_t *count, nsIID * **array)
{
    *count = 0;
    *array = nullptr;
    return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::GetScriptableHelper(nsIXPCScriptable **_retval)
{
    *_retval = nullptr;
    return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::GetContractID(char * *aContractID)
{
    // Make sure to modify any subclasses as needed if this ever
    // changes.
    *aContractID = nullptr;
    return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::GetClassDescription(char * *aClassDescription)
{
    *aClassDescription = nullptr;
    return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::GetClassID(nsCID * *aClassID)
{
    // Make sure to modify any subclasses as needed if this ever
    // changes to not call the virtual GetClassIDNoAlloc.
    *aClassID = (nsCID*) moz_xmalloc(sizeof(nsCID));
    if (!*aClassID)
        return NS_ERROR_OUT_OF_MEMORY;
    return GetClassIDNoAlloc(*aClassID);
}

NS_IMETHODIMP
nsSimpleURI::GetFlags(uint32_t *aFlags)
{
    *aFlags = nsIClassInfo::MAIN_THREAD_ONLY;
    return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::GetClassIDNoAlloc(nsCID *aClassIDNoAlloc)
{
    *aClassIDNoAlloc = kSimpleURICID;
    return NS_OK;
}

//----------------------------------------------------------------------------
// nsSimpleURI::nsISimpleURI
//----------------------------------------------------------------------------
NS_IMETHODIMP
nsSimpleURI::GetMutable(bool *value)
{
    *value = mMutable;
    return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::SetMutable(bool value)
{
    NS_ENSURE_ARG(mMutable || !value);

    mMutable = value;
    return NS_OK;
}

//----------------------------------------------------------------------------
// nsSimpleURI::nsISizeOf
//----------------------------------------------------------------------------

size_t
nsSimpleURI::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  return mScheme.SizeOfExcludingThisIfUnshared(aMallocSizeOf) +
         mPath.SizeOfExcludingThisIfUnshared(aMallocSizeOf) +
         mQuery.SizeOfExcludingThisIfUnshared(aMallocSizeOf) +
         mRef.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
}

size_t
nsSimpleURI::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

NS_IMETHODIMP
nsSimpleURI::GetFilePath(nsACString& aFilePath)
{
    aFilePath = mPath;
    return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::SetFilePath(const nsACString& aFilePath)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleURI::GetQuery(nsACString& aQuery)
{
    if (!mIsQueryValid) {
        MOZ_ASSERT(mQuery.IsEmpty(), "mIsQueryValid/mQuery invariant broken");
        aQuery.Truncate();
    } else {
        aQuery = mQuery;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::SetQuery(const nsACString& aQuery)
{
    NS_ENSURE_STATE(mMutable);

    nsAutoCString query;
    nsresult rv = NS_EscapeURL(aQuery, esc_OnlyNonASCII, query, fallible);
    if (NS_FAILED(rv)) {
        return rv;
    }

    if (query.IsEmpty()) {
        // Empty string means to remove query completely.
        mIsQueryValid = false;
        mQuery.Truncate(); // invariant: mQuery should be empty when it's not valid
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

} // namespace net
} // namespace mozilla
