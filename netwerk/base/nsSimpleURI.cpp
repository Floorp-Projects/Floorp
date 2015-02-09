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
#include "nsIProgrammingLanguage.h"
#include "nsIIPCSerializableURI.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/ipc/URIUtils.h"

using namespace mozilla::ipc;

static NS_DEFINE_CID(kThisSimpleURIImplementationCID,
                     NS_THIS_SIMPLEURI_IMPLEMENTATION_CID);
static NS_DEFINE_CID(kSimpleURICID, NS_SIMPLEURI_CID);

////////////////////////////////////////////////////////////////////////////////
// nsSimpleURI methods:

nsSimpleURI::nsSimpleURI()
    : mMutable(true),
      mIsRefValid(false)
{
}

nsSimpleURI::~nsSimpleURI()
{
}

NS_IMPL_ADDREF(nsSimpleURI)
NS_IMPL_RELEASE(nsSimpleURI)
NS_INTERFACE_TABLE_HEAD(nsSimpleURI)
NS_INTERFACE_TABLE(nsSimpleURI, nsIURI, nsISerializable, nsIClassInfo,
                    nsIMutable, nsIIPCSerializableURI)
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
    }
    else {
      params.ref().SetIsVoid(true);
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
    }
    else {
        mRef = params.ref();
        mIsRefValid = true;
    }
    mMutable = params.isMutable();

    return true;
}

////////////////////////////////////////////////////////////////////////////////
// nsIURI methods:

NS_IMETHODIMP
nsSimpleURI::GetSpec(nsACString &result)
{
    result = mScheme + NS_LITERAL_CSTRING(":") + mPath;
    if (mIsRefValid) {
        result += NS_LITERAL_CSTRING("#") + mRef;
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
    return NS_OK;
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
    
    const nsAFlatCString& flat = PromiseFlatCString(aSpec);
    const char* specPtr = flat.get();

    // filter out unexpected chars "\r\n\t" if necessary
    nsAutoCString filteredSpec;
    int32_t specLen;
    if (net_FilterURIString(specPtr, filteredSpec)) {
        specPtr = filteredSpec.get();
        specLen = filteredSpec.Length();
    } else
        specLen = flat.Length();

    // nsSimpleURI currently restricts the charset to US-ASCII
    nsAutoCString spec;
    NS_EscapeURL(specPtr, specLen, esc_OnlyNonASCII|esc_AlwaysCopy, spec);

    int32_t colonPos = spec.FindChar(':');
    if (colonPos < 0 || !net_IsValidScheme(spec.get(), colonPos))
        return NS_ERROR_MALFORMED_URI;

    mScheme.Truncate();
    mozilla::DebugOnly<int32_t> n = spec.Left(mScheme, colonPos);
    NS_ASSERTION(n == colonPos, "Left failed");
    ToLowerCase(mScheme);

    // This sets both mPath and mRef.
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
        NS_ERROR("the given url scheme contains invalid characters");
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
    NS_ENSURE_STATE(mMutable);
    
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
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleURI::SetHostPort(const nsACString &result)
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
    if (mIsRefValid) {
        result += NS_LITERAL_CSTRING("#") + mRef;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::SetPath(const nsACString &path)
{
    NS_ENSURE_STATE(mMutable);
    
    int32_t hashPos = path.FindChar('#');
    if (hashPos < 0) {
        mIsRefValid = false;
        mRef.Truncate(); // invariant: mRef should be empty when it's not valid
        mPath = path;
        return NS_OK;
    }

    mPath = StringHead(path, hashPos);
    return SetRef(Substring(path, uint32_t(hashPos)));
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

    if (aRef.IsEmpty()) {
      // Empty string means to remove ref completely.
      mIsRefValid = false;
      mRef.Truncate(); // invariant: mRef should be empty when it's not valid
      return NS_OK;
    }

    mIsRefValid = true;

    // Gracefully skip initial hash
    if (aRef[0] == '#') {
        mRef = Substring(aRef, 1);
    } else {
        mRef = aRef;
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

    nsRefPtr<nsSimpleURI> otherUri;
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
nsSimpleURI::StartClone(nsSimpleURI::RefHandlingEnum /* ignored */)
{
    return new nsSimpleURI();
}

NS_IMETHODIMP
nsSimpleURI::Clone(nsIURI** result)
{
    return CloneInternal(eHonorRef, result);
}

NS_IMETHODIMP
nsSimpleURI::CloneIgnoringRef(nsIURI** result)
{
    return CloneInternal(eIgnoreRef, result);
}

nsresult
nsSimpleURI::CloneInternal(nsSimpleURI::RefHandlingEnum refHandlingMode,
                           nsIURI** result)
{
    nsRefPtr<nsSimpleURI> url = StartClone(refHandlingMode);
    if (!url)
        return NS_ERROR_OUT_OF_MEMORY;

    // Note: |url| may well have mMutable false at this point, so
    // don't call any setter methods.
    url->mScheme = mScheme;
    url->mPath = mPath;
    if (refHandlingMode == eHonorRef) {
        url->mRef = mRef;
        url->mIsRefValid = mIsRefValid;
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
    NS_EscapeURL(buf, esc_OnlyNonASCII|esc_AlwaysCopy, result);
    return NS_OK;
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
nsSimpleURI::GetHelperForLanguage(uint32_t language, nsISupports **_retval)
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
    *aClassID = (nsCID*) nsMemory::Alloc(sizeof(nsCID));
    if (!*aClassID)
        return NS_ERROR_OUT_OF_MEMORY;
    return GetClassIDNoAlloc(*aClassID);
}

NS_IMETHODIMP 
nsSimpleURI::GetImplementationLanguage(uint32_t *aImplementationLanguage)
{
    *aImplementationLanguage = nsIProgrammingLanguage::CPLUSPLUS;
    return NS_OK;
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
nsSimpleURI::SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  return mScheme.SizeOfExcludingThisIfUnshared(aMallocSizeOf) +
         mPath.SizeOfExcludingThisIfUnshared(aMallocSizeOf) +
         mRef.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
}

size_t
nsSimpleURI::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

