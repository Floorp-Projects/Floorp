/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DefaultURI.h"
#include "nsIClassInfoImpl.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsURLHelper.h"

#include "mozilla/ipc/URIParams.h"

namespace mozilla {
namespace net {

#define NS_DEFAULTURI_CID                            \
  { /* 04445aa0-fd27-4c99-bd41-6be6318ae92c */       \
    0x04445aa0, 0xfd27, 0x4c99, {                    \
      0xbd, 0x41, 0x6b, 0xe6, 0x31, 0x8a, 0xe9, 0x2c \
    }                                                \
  }

#define ASSIGN_AND_ADDREF_THIS(ptrToMutator)    \
  do {                                          \
    if (ptrToMutator) {                         \
      *(ptrToMutator) = do_AddRef(this).take(); \
    }                                           \
  } while (0)

static NS_DEFINE_CID(kDefaultURICID, NS_DEFAULTURI_CID);

//----------------------------------------------------------------------------
// nsIClassInfo
//----------------------------------------------------------------------------

NS_IMPL_CLASSINFO(DefaultURI, nullptr, nsIClassInfo::THREADSAFE,
                  NS_DEFAULTURI_CID)
// Empty CI getter. We only need nsIClassInfo for Serialization
NS_IMPL_CI_INTERFACE_GETTER0(DefaultURI)

//----------------------------------------------------------------------------
// nsISupports
//----------------------------------------------------------------------------

NS_IMPL_ADDREF(DefaultURI)
NS_IMPL_RELEASE(DefaultURI)
NS_INTERFACE_TABLE_HEAD(DefaultURI)
  NS_INTERFACE_TABLE(DefaultURI, nsIURI, nsISerializable)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE
  NS_IMPL_QUERY_CLASSINFO(DefaultURI)
  if (aIID.Equals(kDefaultURICID)) {
    foundInterface = static_cast<nsIURI*>(this);
  } else
    NS_INTERFACE_MAP_ENTRY(nsISizeOf)
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------------
// nsISerializable
//----------------------------------------------------------------------------

NS_IMETHODIMP DefaultURI::Read(nsIObjectInputStream* aInputStream) {
  MOZ_ASSERT_UNREACHABLE("Use nsIURIMutator.read() instead");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DefaultURI::Write(nsIObjectOutputStream* aOutputStream) {
  nsAutoCString spec(mURL->Spec());
  return aOutputStream->WriteStringZ(spec.get());
}

//----------------------------------------------------------------------------
// nsISizeOf
//----------------------------------------------------------------------------

size_t DefaultURI::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const {
  return mURL->SizeOf();
}

size_t DefaultURI::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

//----------------------------------------------------------------------------
// nsIURI
//----------------------------------------------------------------------------

NS_IMETHODIMP DefaultURI::GetSpec(nsACString& aSpec) {
  aSpec = mURL->Spec();
  return NS_OK;
}

NS_IMETHODIMP DefaultURI::GetPrePath(nsACString& aPrePath) {
  aPrePath = mURL->PrePath();
  return NS_OK;
}

NS_IMETHODIMP DefaultURI::GetScheme(nsACString& aScheme) {
  aScheme = mURL->Scheme();
  return NS_OK;
}

NS_IMETHODIMP DefaultURI::GetUserPass(nsACString& aUserPass) {
  aUserPass = mURL->Username();
  nsAutoCString pass(mURL->Password());
  if (pass.IsEmpty()) {
    return NS_OK;
  }
  aUserPass.Append(':');
  aUserPass.Append(pass);
  return NS_OK;
}

NS_IMETHODIMP DefaultURI::GetUsername(nsACString& aUsername) {
  aUsername = mURL->Username();
  return NS_OK;
}

NS_IMETHODIMP DefaultURI::GetPassword(nsACString& aPassword) {
  aPassword = mURL->Password();
  return NS_OK;
}

NS_IMETHODIMP DefaultURI::GetHostPort(nsACString& aHostPort) {
  aHostPort = mURL->HostPort();
  return NS_OK;
}

NS_IMETHODIMP DefaultURI::GetHost(nsACString& aHost) {
  aHost = mURL->Host();

  // Historically nsIURI.host has always returned an IPv6 address that isn't
  // enclosed in brackets. Ideally we want to change that, but for the sake of
  // consitency we'll leave it like that for the moment.
  // Bug 1603199 should fix this.
  if (StringBeginsWith(aHost, "["_ns) && StringEndsWith(aHost, "]"_ns) &&
      aHost.FindChar(':') != kNotFound) {
    aHost = Substring(aHost, 1, aHost.Length() - 2);
  }
  return NS_OK;
}

NS_IMETHODIMP DefaultURI::GetPort(int32_t* aPort) {
  *aPort = mURL->Port();
  return NS_OK;
}

NS_IMETHODIMP DefaultURI::GetPathQueryRef(nsACString& aPathQueryRef) {
  aPathQueryRef = mURL->Path();
  return NS_OK;
}

NS_IMETHODIMP DefaultURI::Equals(nsIURI* other, bool* _retval) {
  RefPtr<DefaultURI> otherUri;
  nsresult rv = other->QueryInterface(kDefaultURICID, getter_AddRefs(otherUri));
  if (NS_FAILED(rv)) {
    *_retval = false;
    return NS_OK;
  }

  *_retval = mURL->Spec() == otherUri->mURL->Spec();
  return NS_OK;
}

NS_IMETHODIMP DefaultURI::SchemeIs(const char* scheme, bool* _retval) {
  *_retval = mURL->Scheme().Equals(scheme);
  return NS_OK;
}

NS_IMETHODIMP DefaultURI::Resolve(const nsACString& aRelativePath,
                                  nsACString& aResult) {
  nsAutoCString scheme;
  nsresult rv = net_ExtractURLScheme(aRelativePath, scheme);
  if (NS_SUCCEEDED(rv)) {
    aResult = aRelativePath;
    return NS_OK;
  }

  // We try to create another URL with this one as its base.
  RefPtr<MozURL> resolvedURL;
  rv = MozURL::Init(getter_AddRefs(resolvedURL), aRelativePath, mURL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    // If parsing the relative url fails, we revert to the previous behaviour
    // and just return the relative path.
    aResult = aRelativePath;
    return NS_OK;
  }

  aResult = resolvedURL->Spec();
  return NS_OK;
}

NS_IMETHODIMP DefaultURI::GetAsciiSpec(nsACString& aAsciiSpec) {
  return GetSpec(aAsciiSpec);
}

NS_IMETHODIMP DefaultURI::GetAsciiHostPort(nsACString& aAsciiHostPort) {
  return GetHostPort(aAsciiHostPort);
}

NS_IMETHODIMP DefaultURI::GetAsciiHost(nsACString& aAsciiHost) {
  return GetHost(aAsciiHost);
}

NS_IMETHODIMP DefaultURI::GetRef(nsACString& aRef) {
  aRef = mURL->Ref();
  return NS_OK;
}

NS_IMETHODIMP DefaultURI::EqualsExceptRef(nsIURI* other, bool* _retval) {
  RefPtr<DefaultURI> otherUri;
  nsresult rv = other->QueryInterface(kDefaultURICID, getter_AddRefs(otherUri));
  if (NS_FAILED(rv)) {
    *_retval = false;
    return NS_OK;
  }

  *_retval = mURL->SpecNoRef().Equals(otherUri->mURL->SpecNoRef());
  return NS_OK;
}

NS_IMETHODIMP DefaultURI::GetSpecIgnoringRef(nsACString& aSpecIgnoringRef) {
  aSpecIgnoringRef = mURL->SpecNoRef();
  return NS_OK;
}

NS_IMETHODIMP DefaultURI::GetHasRef(bool* aHasRef) {
  *aHasRef = mURL->HasFragment();
  return NS_OK;
}

NS_IMETHODIMP DefaultURI::GetHasUserPass(bool* aHasUserPass) {
  *aHasUserPass = !mURL->Username().IsEmpty() || !mURL->Password().IsEmpty();
  return NS_OK;
}

NS_IMETHODIMP DefaultURI::GetFilePath(nsACString& aFilePath) {
  aFilePath = mURL->FilePath();
  return NS_OK;
}

NS_IMETHODIMP DefaultURI::GetQuery(nsACString& aQuery) {
  aQuery = mURL->Query();
  return NS_OK;
}

NS_IMETHODIMP DefaultURI::GetHasQuery(bool* aHasQuery) {
  *aHasQuery = mURL->HasQuery();
  return NS_OK;
}

NS_IMETHODIMP DefaultURI::GetDisplayHost(nsACString& aDisplayHost) {
  // At the moment it doesn't seem useful to decode the hostname if it happens
  // to contain punycode.
  return GetHost(aDisplayHost);
}

NS_IMETHODIMP DefaultURI::GetDisplayHostPort(nsACString& aDisplayHostPort) {
  // At the moment it doesn't seem useful to decode the hostname if it happens
  // to contain punycode.
  return GetHostPort(aDisplayHostPort);
}

NS_IMETHODIMP DefaultURI::GetDisplaySpec(nsACString& aDisplaySpec) {
  // At the moment it doesn't seem useful to decode the hostname if it happens
  // to contain punycode.
  return GetSpec(aDisplaySpec);
}

NS_IMETHODIMP DefaultURI::GetDisplayPrePath(nsACString& aDisplayPrePath) {
  // At the moment it doesn't seem useful to decode the hostname if it happens
  // to contain punycode.
  return GetPrePath(aDisplayPrePath);
}

NS_IMETHODIMP DefaultURI::Mutate(nsIURIMutator** _retval) {
  RefPtr<DefaultURI::Mutator> mutator = new DefaultURI::Mutator();
  mutator->Init(this);
  mutator.forget(_retval);
  return NS_OK;
}

void DefaultURI::Serialize(ipc::URIParams& aParams) {
  ipc::DefaultURIParams params;
  params.spec() = mURL->Spec();
  aParams = params;
}

//----------------------------------------------------------------------------
// nsIURIMutator
//----------------------------------------------------------------------------

NS_IMPL_ADDREF(DefaultURI::Mutator)
NS_IMPL_RELEASE(DefaultURI::Mutator)
NS_IMETHODIMP DefaultURI::Mutator::QueryInterface(REFNSIID aIID,
                                                  void** aInstancePtr) {
  NS_ASSERTION(aInstancePtr, "QueryInterface requires a non-NULL destination!");
  nsISupports* foundInterface = nullptr;
  if (aIID.Equals(NS_GET_IID(nsIURI))) {
    RefPtr<DefaultURI> defaultURI = new DefaultURI();
    mMutator->Finalize(getter_AddRefs(defaultURI->mURL));
    foundInterface =
        static_cast<nsISupports*>(static_cast<nsIURI*>((defaultURI.get())));
    NS_ADDREF(foundInterface);
    *aInstancePtr = foundInterface;
    return NS_OK;
  }

  if (aIID.Equals(NS_GET_IID(nsIURIMutator)) ||
      aIID.Equals(NS_GET_IID(nsISupports))) {
    foundInterface =
        static_cast<nsISupports*>(static_cast<nsIURIMutator*>(this));
  } else if (aIID.Equals(NS_GET_IID(nsIURISetters))) {
    foundInterface =
        static_cast<nsISupports*>(static_cast<nsIURISetters*>(this));
  } else if (aIID.Equals(NS_GET_IID(nsIURISetSpec))) {
    foundInterface =
        static_cast<nsISupports*>(static_cast<nsIURISetSpec*>(this));
  } else if (aIID.Equals(NS_GET_IID(nsISerializable))) {
    foundInterface =
        static_cast<nsISupports*>(static_cast<nsISerializable*>(this));
  }

  if (foundInterface) {
    NS_ADDREF(foundInterface);
    *aInstancePtr = foundInterface;
    return NS_OK;
  }

  return NS_NOINTERFACE;
}

NS_IMETHODIMP DefaultURI::Mutator::Read(nsIObjectInputStream* aStream) {
  nsAutoCString spec;
  nsresult rv = aStream->ReadCString(spec);
  if (NS_FAILED(rv)) {
    return rv;
  }
  return SetSpec(spec, nullptr);
}

NS_IMETHODIMP DefaultURI::Mutator::Deserialize(
    const mozilla::ipc::URIParams& aParams) {
  if (aParams.type() != ipc::URIParams::TDefaultURIParams) {
    NS_ERROR("Received unknown parameters from the other process!");
    return NS_ERROR_FAILURE;
  }

  const ipc::DefaultURIParams& params = aParams.get_DefaultURIParams();
  auto result = MozURL::Mutator::FromSpec(params.spec());
  if (result.isErr()) {
    return result.unwrapErr();
  }
  mMutator = Some(result.unwrap());
  return NS_OK;
}

NS_IMETHODIMP DefaultURI::Mutator::Finalize(nsIURI** aURI) {
  if (!mMutator.isSome()) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  RefPtr<DefaultURI> uri = new DefaultURI();
  mMutator->Finalize(getter_AddRefs(uri->mURL));
  mMutator = Nothing();
  uri.forget(aURI);
  return NS_OK;
}

NS_IMETHODIMP DefaultURI::Mutator::SetSpec(const nsACString& aSpec,
                                           nsIURIMutator** aMutator) {
  ASSIGN_AND_ADDREF_THIS(aMutator);
  auto result = MozURL::Mutator::FromSpec(aSpec);
  if (result.isErr()) {
    return result.unwrapErr();
  }
  mMutator = Some(result.unwrap());
  return NS_OK;
}

NS_IMETHODIMP
DefaultURI::Mutator::SetScheme(const nsACString& aScheme,
                               nsIURIMutator** aMutator) {
  ASSIGN_AND_ADDREF_THIS(aMutator);
  if (!mMutator.isSome()) {
    return NS_ERROR_NULL_POINTER;
  }
  mMutator->SetScheme(aScheme);
  return mMutator->GetStatus();
}

NS_IMETHODIMP
DefaultURI::Mutator::SetUserPass(const nsACString& aUserPass,
                                 nsIURIMutator** aMutator) {
  ASSIGN_AND_ADDREF_THIS(aMutator);
  if (!mMutator.isSome()) {
    return NS_ERROR_NULL_POINTER;
  }
  int32_t index = aUserPass.FindChar(':');
  if (index == kNotFound) {
    mMutator->SetUsername(aUserPass);
    mMutator->SetPassword(""_ns);
    return mMutator->GetStatus();
  }

  mMutator->SetUsername(Substring(aUserPass, 0, index));
  nsresult rv = mMutator->GetStatus();
  if (NS_FAILED(rv)) {
    return rv;
  }
  mMutator->SetPassword(Substring(aUserPass, index + 1));
  rv = mMutator->GetStatus();
  if (NS_FAILED(rv)) {
    return rv;
  }
  return NS_OK;
}

NS_IMETHODIMP
DefaultURI::Mutator::SetUsername(const nsACString& aUsername,
                                 nsIURIMutator** aMutator) {
  ASSIGN_AND_ADDREF_THIS(aMutator);
  if (!mMutator.isSome()) {
    return NS_ERROR_NULL_POINTER;
  }
  mMutator->SetUsername(aUsername);
  return mMutator->GetStatus();
}

NS_IMETHODIMP
DefaultURI::Mutator::SetPassword(const nsACString& aPassword,
                                 nsIURIMutator** aMutator) {
  ASSIGN_AND_ADDREF_THIS(aMutator);
  if (!mMutator.isSome()) {
    return NS_ERROR_NULL_POINTER;
  }
  mMutator->SetPassword(aPassword);
  return mMutator->GetStatus();
}

NS_IMETHODIMP
DefaultURI::Mutator::SetHostPort(const nsACString& aHostPort,
                                 nsIURIMutator** aMutator) {
  ASSIGN_AND_ADDREF_THIS(aMutator);
  if (!mMutator.isSome()) {
    return NS_ERROR_NULL_POINTER;
  }
  mMutator->SetHostPort(aHostPort);
  return mMutator->GetStatus();
}

NS_IMETHODIMP
DefaultURI::Mutator::SetHost(const nsACString& aHost,
                             nsIURIMutator** aMutator) {
  ASSIGN_AND_ADDREF_THIS(aMutator);
  if (!mMutator.isSome()) {
    return NS_ERROR_NULL_POINTER;
  }
  mMutator->SetHostname(aHost);
  return mMutator->GetStatus();
}

NS_IMETHODIMP
DefaultURI::Mutator::SetPort(int32_t aPort, nsIURIMutator** aMutator) {
  ASSIGN_AND_ADDREF_THIS(aMutator);
  if (!mMutator.isSome()) {
    return NS_ERROR_NULL_POINTER;
  }
  mMutator->SetPort(aPort);
  return mMutator->GetStatus();
}

NS_IMETHODIMP
DefaultURI::Mutator::SetPathQueryRef(const nsACString& aPathQueryRef,
                                     nsIURIMutator** aMutator) {
  ASSIGN_AND_ADDREF_THIS(aMutator);
  if (!mMutator.isSome()) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aPathQueryRef.IsEmpty()) {
    mMutator->SetFilePath(""_ns);
    mMutator->SetQuery(""_ns);
    mMutator->SetRef(""_ns);
    return mMutator->GetStatus();
  }

  nsAutoCString pathQueryRef(aPathQueryRef);
  if (!StringBeginsWith(pathQueryRef, "/"_ns)) {
    pathQueryRef.Insert('/', 0);
  }

  RefPtr<MozURL> url;
  mMutator->Finalize(getter_AddRefs(url));
  mMutator = Nothing();

  auto result = MozURL::Mutator::FromSpec(pathQueryRef, url);
  if (result.isErr()) {
    return result.unwrapErr();
  }
  mMutator = Some(result.unwrap());
  return mMutator->GetStatus();
}

NS_IMETHODIMP
DefaultURI::Mutator::SetRef(const nsACString& aRef, nsIURIMutator** aMutator) {
  ASSIGN_AND_ADDREF_THIS(aMutator);
  if (!mMutator.isSome()) {
    return NS_ERROR_NULL_POINTER;
  }
  mMutator->SetRef(aRef);
  return mMutator->GetStatus();
}

NS_IMETHODIMP
DefaultURI::Mutator::SetFilePath(const nsACString& aFilePath,
                                 nsIURIMutator** aMutator) {
  ASSIGN_AND_ADDREF_THIS(aMutator);
  if (!mMutator.isSome()) {
    return NS_ERROR_NULL_POINTER;
  }
  mMutator->SetFilePath(aFilePath);
  return mMutator->GetStatus();
}

NS_IMETHODIMP
DefaultURI::Mutator::SetQuery(const nsACString& aQuery,
                              nsIURIMutator** aMutator) {
  ASSIGN_AND_ADDREF_THIS(aMutator);
  if (!mMutator.isSome()) {
    return NS_ERROR_NULL_POINTER;
  }
  mMutator->SetQuery(aQuery);
  return mMutator->GetStatus();
}

NS_IMETHODIMP
DefaultURI::Mutator::SetQueryWithEncoding(const nsACString& aQuery,
                                          const mozilla::Encoding* aEncoding,
                                          nsIURIMutator** aMutator) {
  ASSIGN_AND_ADDREF_THIS(aMutator);
  if (!mMutator.isSome()) {
    return NS_ERROR_NULL_POINTER;
  }
  // we only support UTF-8 for DefaultURI
  mMutator->SetQuery(aQuery);
  return mMutator->GetStatus();
}

}  // namespace net
}  // namespace mozilla
