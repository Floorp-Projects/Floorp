#include "RustURL.h"
#include "nsCOMPtr.h"
#include "nsURLHelper.h"

#ifndef MOZ_RUST_URLPARSE
  #error "Should be defined"
#endif

using namespace mozilla::ipc;

namespace mozilla {
namespace net {

NS_IMPL_ADDREF(RustURL)
NS_IMPL_RELEASE(RustURL)

NS_INTERFACE_MAP_BEGIN(RustURL)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIStandardURL)
  NS_INTERFACE_MAP_ENTRY(nsIURI)
  NS_INTERFACE_MAP_ENTRY(nsIURIWithQuery)
  NS_INTERFACE_MAP_ENTRY(nsIURL)
  // NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIFileURL, mSupportsFileURL)
  NS_INTERFACE_MAP_ENTRY(nsIStandardURL)
  // NS_INTERFACE_MAP_ENTRY(nsISerializable)
  NS_INTERFACE_MAP_ENTRY(nsIClassInfo)
  NS_INTERFACE_MAP_ENTRY(nsIMutable)
  // NS_INTERFACE_MAP_ENTRY(nsIIPCSerializableURI)
  // NS_INTERFACE_MAP_ENTRY(nsISensitiveInfoHiddenURI)
  NS_INTERFACE_MAP_ENTRY(nsISizeOf)
NS_INTERFACE_MAP_END

#define ENSURE_MUTABLE() \
  PR_BEGIN_MACRO \
    if (!mMutable) { \
      NS_WARNING("attempt to modify an immutable RustURL"); \
      return NS_ERROR_ABORT; \
    } \
  PR_END_MACRO

RustURL::RustURL()
  : mMutable(true)
{

}

RustURL::~RustURL()
{

}

NS_IMETHODIMP
RustURL::GetSpec(nsACString & aSpec)
{
  return static_cast<nsresult>(rusturl_get_spec(mURL.get(), &aSpec));
}

NS_IMETHODIMP
RustURL::SetSpec(const nsACString & aSpec)
{
  ENSURE_MUTABLE();

  rusturl* ptr = rusturl_new(&aSpec);
  if (!ptr) {
    return NS_ERROR_FAILURE;
  }
  mURL.reset(ptr);
  return NS_OK;
}

NS_IMETHODIMP
RustURL::GetPrePath(nsACString & aPrePath)
{
  // TODO: use slicing API to get actual prepath
  aPrePath.Truncate();
  nsAutoCString rustResult;
  nsAutoCString part;

  nsresult rv = GetScheme(part);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rustResult.Append(part);
  rustResult += "://";

  rv = GetUserPass(part);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (!part.IsEmpty()) {
    rustResult += part;
    rustResult += "@";
  }

  rv = GetHostPort(part);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rustResult += part;
  aPrePath.Assign(rustResult);
  return NS_OK;
}

NS_IMETHODIMP
RustURL::GetScheme(nsACString & aScheme)
{
  return static_cast<nsresult>(rusturl_get_scheme(mURL.get(), &aScheme));
}

NS_IMETHODIMP
RustURL::SetScheme(const nsACString & aScheme)
{
  ENSURE_MUTABLE();

  return static_cast<nsresult>(rusturl_set_scheme(mURL.get(), &aScheme));
}

NS_IMETHODIMP
RustURL::GetUserPass(nsACString & aUserPass)
{
  nsresult rv = GetUsername(aUserPass);
  if (NS_FAILED(rv)) {
    aUserPass.Truncate();
    return rv;
  }

  nsAutoCString password;
  rv = GetPassword(password);
  if (NS_FAILED(rv)) {
    aUserPass.Truncate();
    return rv;
  }

  if (password.Length()) {
    aUserPass.Append(':');
    aUserPass.Append(password);
  }
  return NS_OK;
}

NS_IMETHODIMP
RustURL::SetUserPass(const nsACString & aUserPass)
{
  ENSURE_MUTABLE();

  int32_t colonPos = aUserPass.FindChar(':');
  nsAutoCString user;
  nsAutoCString pass;
  if (colonPos == kNotFound) {
    user = aUserPass;
  } else {
    user = Substring(aUserPass, 0, colonPos);
    pass = Substring(aUserPass, colonPos + 1, aUserPass.Length());
  }

  if (rusturl_set_username(mURL.get(), &user) != 0) {
    return NS_ERROR_FAILURE;
  }
  return static_cast<nsresult>(rusturl_set_password(mURL.get(), &pass));
}

NS_IMETHODIMP
RustURL::GetUsername(nsACString & aUsername)
{
  return static_cast<nsresult>(rusturl_get_username(mURL.get(), &aUsername));
}

NS_IMETHODIMP
RustURL::SetUsername(const nsACString & aUsername)
{
  ENSURE_MUTABLE();
  return static_cast<nsresult>(rusturl_set_username(mURL.get(), &aUsername));
}

NS_IMETHODIMP
RustURL::GetPassword(nsACString & aPassword)
{
  return static_cast<nsresult>(rusturl_get_password(mURL.get(), &aPassword));
}

NS_IMETHODIMP
RustURL::SetPassword(const nsACString & aPassword)
{
  ENSURE_MUTABLE();
  return static_cast<nsresult>(rusturl_set_password(mURL.get(), &aPassword));
}

NS_IMETHODIMP
RustURL::GetHostPort(nsACString & aHostPort)
{
  nsresult rv = (nsresult) rusturl_get_host(mURL.get(), &aHostPort);
  if (NS_FAILED(rv)) {
    return rv;
  }

  int32_t port;
  rv = GetPort(&port);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (port >= 0) {
    aHostPort.Append(':');
    aHostPort.AppendInt(port);
  }

  return NS_OK;
}

NS_IMETHODIMP
RustURL::SetHostPort(const nsACString & aHostPort)
{
  ENSURE_MUTABLE();
  return static_cast<nsresult>(rusturl_set_host_port(mURL.get(), &aHostPort));
}

NS_IMETHODIMP
RustURL::SetHostAndPort(const nsACString & hostport)
{
  ENSURE_MUTABLE();
  return static_cast<nsresult>(rusturl_set_host_and_port(mURL.get(), &hostport));
}

NS_IMETHODIMP
RustURL::GetHost(nsACString & aHost)
{
  nsAutoCString host;
  nsresult rv = (nsresult) rusturl_get_host(mURL.get(), &host);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (host.Length() > 0 && host.First() == '[' && host.Last() == ']') {
    aHost = Substring(host, 1, host.Length() - 2);
    return NS_OK;
  }

  aHost = host;
  return NS_OK;
}

NS_IMETHODIMP
RustURL::SetHost(const nsACString & aHost)
{
  ENSURE_MUTABLE();
  return static_cast<nsresult>(rusturl_set_host(mURL.get(), &aHost));
}

NS_IMETHODIMP
RustURL::GetPort(int32_t *aPort)
{
  if (!mURL) {
    return NS_ERROR_FAILURE;
  }
  *aPort = rusturl_get_port(mURL.get());
  return NS_OK;
}

NS_IMETHODIMP
RustURL::SetPort(int32_t aPort)
{
  ENSURE_MUTABLE();
  return static_cast<nsresult>(rusturl_set_port_no(mURL.get(), aPort));
}

NS_IMETHODIMP
RustURL::GetPath(nsACString & aPath)
{
  return static_cast<nsresult>(rusturl_get_path(mURL.get(), &aPath));
}

NS_IMETHODIMP
RustURL::SetPath(const nsACString & aPath)
{
  ENSURE_MUTABLE();

  nsAutoCString path;
  nsresult rv = GetPrePath(path);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (aPath.Length() > 0 && aPath.First() != '/') {
    path.Append('/');
  }
  path.Append(aPath);

  return SetSpec(path);
}

NS_IMETHODIMP
RustURL::Equals(nsIURI *other, bool *aRetVal)
{
  *aRetVal = false;
  nsAutoCString spec;
  nsresult rv = other->GetSpec(spec);
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsAutoCString rustSpec;
  rv = GetSpec(rustSpec);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (rustSpec == spec) {
    *aRetVal = true;
  }
  return NS_OK;
}

NS_IMETHODIMP
RustURL::SchemeIs(const char * aScheme, bool *aRetVal)
{
  *aRetVal = false;
  nsAutoCString scheme;
  nsresult rv = GetScheme(scheme);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (scheme.Equals(aScheme)) {
    *aRetVal = true;
  }
  return NS_OK;
}

NS_IMETHODIMP
RustURL::Clone(nsIURI * *aRetVal)
{
  RefPtr<RustURL> url = new RustURL();
  nsAutoCString spec;
  nsresult rv = GetSpec(spec);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = url->SetSpec(spec);
  if (NS_FAILED(rv)) {
    return rv;
  }
  url.forget(aRetVal);
  return NS_OK;
}

NS_IMETHODIMP
RustURL::Resolve(const nsACString & relativePath, nsACString & aRetVal)
{
  return static_cast<nsresult>(rusturl_resolve(mURL.get(), &relativePath, &aRetVal));
}

NS_IMETHODIMP
RustURL::GetAsciiSpec(nsACString & aAsciiSpec)
{
  return GetSpec(aAsciiSpec);
}

NS_IMETHODIMP
RustURL::GetAsciiHostPort(nsACString & aAsciiHostPort)
{
  return GetHostPort(aAsciiHostPort);
}

NS_IMETHODIMP
RustURL::GetAsciiHost(nsACString & aAsciiHost)
{
  return GetHost(aAsciiHost);
}

NS_IMETHODIMP
RustURL::GetOriginCharset(nsACString & aOriginCharset)
{
  aOriginCharset.AssignLiteral("UTF-8");
  return NS_OK;
}

NS_IMETHODIMP
RustURL::GetRef(nsACString & aRef)
{
  return static_cast<nsresult>(rusturl_get_fragment(mURL.get(), &aRef));
}

NS_IMETHODIMP
RustURL::SetRef(const nsACString & aRef)
{
  ENSURE_MUTABLE();
  return static_cast<nsresult>(rusturl_set_fragment(mURL.get(), &aRef));
}

NS_IMETHODIMP
RustURL::EqualsExceptRef(nsIURI *other, bool *_retval)
{
  *_retval = false;
  nsAutoCString otherSpec;
  nsresult rv = other->GetSpecIgnoringRef(otherSpec);
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsAutoCString thisSpec;
  rv = GetSpecIgnoringRef(thisSpec);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (thisSpec == otherSpec) {
    *_retval = true;
  }
  return NS_OK;
}

NS_IMETHODIMP
RustURL::CloneIgnoringRef(nsIURI * *_retval)
{
  return CloneWithNewRef(NS_LITERAL_CSTRING(""), _retval);
}

NS_IMETHODIMP
RustURL::CloneWithNewRef(const nsACString & newRef, nsIURI * *_retval)
{
  nsCOMPtr<nsIURI> uri;
  nsresult rv = Clone(getter_AddRefs(uri));
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = uri->SetRef(newRef);
  if (NS_FAILED(rv)) {
    return rv;
  }
  uri.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
RustURL::GetSpecIgnoringRef(nsACString & aSpecIgnoringRef)
{
  nsresult rv = GetSpec(aSpecIgnoringRef);
  if (NS_FAILED(rv)) {
    return rv;
  }
  int32_t pos = aSpecIgnoringRef.FindChar('#');
  if (pos == kNotFound) {
    return NS_OK;
  }

  aSpecIgnoringRef.Truncate(pos);
  return NS_OK;
}

NS_IMETHODIMP
RustURL::GetHasRef(bool *aHasRef)
{
  *aHasRef = false;
  int32_t rv = rusturl_has_fragment(mURL.get());
  if (rv == 1) {
    *aHasRef = true;
  } else if (rv < 0) {
    return static_cast<nsresult>(rv);
  }
  return NS_OK;
}

/// nsIURL

NS_IMETHODIMP
RustURL::GetFilePath(nsACString & aFilePath)
{
  return static_cast<nsresult>(rusturl_get_path(mURL.get(), &aFilePath));
}

NS_IMETHODIMP
RustURL::SetFilePath(const nsACString & aFilePath)
{
  ENSURE_MUTABLE();
  return static_cast<nsresult>(rusturl_set_path(mURL.get(), &aFilePath));
}

NS_IMETHODIMP
RustURL::GetQuery(nsACString & aQuery)
{
  return static_cast<nsresult>(rusturl_get_query(mURL.get(), &aQuery));
}

NS_IMETHODIMP
RustURL::SetQuery(const nsACString & aQuery)
{
  ENSURE_MUTABLE();
  return static_cast<nsresult>(rusturl_set_query(mURL.get(), &aQuery));
}

NS_IMETHODIMP
RustURL::GetDirectory(nsACString & aDirectory)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RustURL::SetDirectory(const nsACString & aDirectory)
{
  ENSURE_MUTABLE();
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RustURL::GetFileName(nsACString & aFileName)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RustURL::SetFileName(const nsACString & aFileName)
{
  ENSURE_MUTABLE();
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RustURL::GetFileBaseName(nsACString & aFileBaseName)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RustURL::SetFileBaseName(const nsACString & aFileBaseName)
{
  ENSURE_MUTABLE();
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RustURL::GetFileExtension(nsACString & aFileExtension)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RustURL::SetFileExtension(const nsACString & aFileExtension)
{
  ENSURE_MUTABLE();
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RustURL::GetCommonBaseSpec(nsIURI *aURIToCompare, nsACString & _retval)
{
  RefPtr<RustURL> url = new RustURL();
  nsAutoCString spec;
  nsresult rv = aURIToCompare->GetSpec(spec);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = url->SetSpec(spec);
  if (NS_FAILED(rv)) {
    return rv;
  }
  return static_cast<nsresult>(rusturl_common_base_spec(mURL.get(), url->mURL.get(), &_retval));
}

NS_IMETHODIMP
RustURL::GetRelativeSpec(nsIURI *aURIToCompare, nsACString & _retval)
{
  RefPtr<RustURL> url = new RustURL();
  nsAutoCString spec;
  nsresult rv = aURIToCompare->GetSpec(spec);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = url->SetSpec(spec);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return static_cast<nsresult>(rusturl_relative_spec(mURL.get(), url->mURL.get(), &_retval));
}

// nsIFileURL


NS_IMETHODIMP
RustURL::GetFile(nsIFile * *aFile)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RustURL::SetFile(nsIFile *aFile)
{
  ENSURE_MUTABLE();
  return NS_ERROR_NOT_IMPLEMENTED;
}

// nsIStandardURL

NS_IMETHODIMP
RustURL::Init(uint32_t aUrlType, int32_t aDefaultPort, const nsACString & aSpec,
              const char * aOriginCharset, nsIURI *aBaseURI)
{
  ENSURE_MUTABLE();

  if (aBaseURI && net_IsAbsoluteURL(aSpec)) {
    aBaseURI = nullptr;
  }

  if (!aBaseURI) {
    return SetSpec(aSpec);
  }

  nsAutoCString buf;
  nsresult rv = aBaseURI->Resolve(aSpec, buf);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return SetSpec(buf);
}

NS_IMETHODIMP
RustURL::SetDefaultPort(int32_t aNewDefaultPort)
{
  ENSURE_MUTABLE();
  return NS_ERROR_NOT_IMPLEMENTED;
}

// nsISerializable

NS_IMETHODIMP
RustURL::Read(nsIObjectInputStream *aInputStream)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RustURL::Write(nsIObjectOutputStream *aOutputStream)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

void
RustURL::Serialize(URIParams& aParams)
{
  // TODO
}

bool
RustURL::Deserialize(const URIParams& aParams)
{
  // TODO
  return false;
}

// nsIClassInfo

NS_IMETHODIMP
RustURL::GetInterfaces(uint32_t *count, nsIID ***array)
{
  *count = 0;
  *array = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
RustURL::GetScriptableHelper(nsIXPCScriptable * *_retval)
{
  *_retval = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
RustURL::GetContractID(char * *aContractID)
{
  *aContractID = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
RustURL::GetClassDescription(char * *aClassDescription)
{
  *aClassDescription = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
RustURL::GetClassID(nsCID **aClassID)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RustURL::GetFlags(uint32_t *aFlags)
{
  *aFlags = nsIClassInfo::MAIN_THREAD_ONLY;
  return NS_OK;
}

NS_IMETHODIMP
RustURL::GetClassIDNoAlloc(nsCID *aClassIDNoAlloc)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/// nsIMutable

NS_IMETHODIMP
RustURL::GetMutable(bool *aValue)
{
  *aValue = mMutable;
  return NS_OK;
}

NS_IMETHODIMP
RustURL::SetMutable(bool aValue)
{
  if (mMutable || !aValue) {
    return NS_ERROR_FAILURE;
  }
  mMutable = aValue;
  return NS_OK;
}

// nsISensitiveInfoHiddenURI

NS_IMETHODIMP
RustURL::GetSensitiveInfoHiddenSpec(nsACString & _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

// nsISizeOf

size_t
RustURL::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  return mURL.get() ? sizeof_rusturl() : 0;
}

size_t
RustURL::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

} // namespace net
} // namespace mozilla

