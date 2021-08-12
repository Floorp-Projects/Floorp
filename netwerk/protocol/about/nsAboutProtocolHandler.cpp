/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"
#include "mozilla/ArrayUtils.h"

#include "nsAboutProtocolHandler.h"
#include "nsIURI.h"
#include "nsIAboutModule.h"
#include "nsString.h"
#include "nsNetCID.h"
#include "nsAboutProtocolUtils.h"
#include "nsError.h"
#include "nsNetUtil.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsIWritablePropertyBag2.h"
#include "nsIChannel.h"
#include "nsIScriptError.h"
#include "nsIClassInfoImpl.h"

#include "mozilla/ipc/URIUtils.h"

namespace mozilla {
namespace net {

static NS_DEFINE_CID(kSimpleURICID, NS_SIMPLEURI_CID);
static NS_DEFINE_CID(kNestedAboutURICID, NS_NESTEDABOUTURI_CID);

static bool IsSafeForUntrustedContent(nsIAboutModule* aModule, nsIURI* aURI) {
  uint32_t flags;
  nsresult rv = aModule->GetURIFlags(aURI, &flags);
  NS_ENSURE_SUCCESS(rv, false);

  return (flags & nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT) != 0;
}

static bool IsSafeToLinkForUntrustedContent(nsIURI* aURI) {
  nsAutoCString path;
  aURI->GetPathQueryRef(path);

  int32_t f = path.FindChar('#');
  if (f >= 0) {
    path.SetLength(f);
  }

  f = path.FindChar('?');
  if (f >= 0) {
    path.SetLength(f);
  }

  ToLowerCase(path);

  // The about modules for these URL types have the
  // URI_SAFE_FOR_UNTRUSTED_CONTENT and MAKE_LINKABLE flags set.
  return path.EqualsLiteral("blank") || path.EqualsLiteral("logo") ||
         path.EqualsLiteral("srcdoc");
}

////////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS(nsAboutProtocolHandler, nsIProtocolHandler,
                  nsIProtocolHandlerWithDynamicFlags, nsISupportsWeakReference)

////////////////////////////////////////////////////////////////////////////////
// nsIProtocolHandler methods:

NS_IMETHODIMP
nsAboutProtocolHandler::GetScheme(nsACString& result) {
  result.AssignLiteral("about");
  return NS_OK;
}

NS_IMETHODIMP
nsAboutProtocolHandler::GetDefaultPort(int32_t* result) {
  *result = -1;  // no port for about: URLs
  return NS_OK;
}

NS_IMETHODIMP
nsAboutProtocolHandler::GetProtocolFlags(uint32_t* result) {
  *result = URI_NORELATIVE | URI_NOAUTH | URI_DANGEROUS_TO_LOAD |
            URI_SCHEME_NOT_SELF_LINKABLE;
  return NS_OK;
}

NS_IMETHODIMP
nsAboutProtocolHandler::GetFlagsForURI(nsIURI* aURI, uint32_t* aFlags) {
  // First use the default (which is "unsafe for content"):
  GetProtocolFlags(aFlags);

  // Now try to see if this URI overrides the default:
  nsCOMPtr<nsIAboutModule> aboutMod;
  nsresult rv = NS_GetAboutModule(aURI, getter_AddRefs(aboutMod));
  if (NS_FAILED(rv)) {
    // Swallow this and just tell the consumer the default:
    return NS_OK;
  }
  uint32_t aboutModuleFlags = 0;
  rv = aboutMod->GetURIFlags(aURI, &aboutModuleFlags);
  // This should never happen, so pass back the error:
  NS_ENSURE_SUCCESS(rv, rv);

  // Secure (https) pages can load safe about pages without becoming
  // mixed content.
  if (aboutModuleFlags & nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT) {
    *aFlags |= URI_IS_POTENTIALLY_TRUSTWORTHY;
    // about: pages can only be loaded by unprivileged principals
    // if they are marked as LINKABLE
    if (aboutModuleFlags & nsIAboutModule::MAKE_LINKABLE) {
      // Replace URI_DANGEROUS_TO_LOAD with URI_LOADABLE_BY_ANYONE.
      *aFlags &= ~URI_DANGEROUS_TO_LOAD;
      *aFlags |= URI_LOADABLE_BY_ANYONE;
    }
  }
  return NS_OK;
}

// static
nsresult nsAboutProtocolHandler::CreateNewURI(const nsACString& aSpec,
                                              const char* aCharset,
                                              nsIURI* aBaseURI,
                                              nsIURI** aResult) {
  *aResult = nullptr;
  nsresult rv;

  // Use a simple URI to parse out some stuff first
  nsCOMPtr<nsIURI> url;
  rv = NS_MutateURI(new nsSimpleURI::Mutator()).SetSpec(aSpec).Finalize(url);

  if (NS_FAILED(rv)) {
    return rv;
  }

  if (IsSafeToLinkForUntrustedContent(url)) {
    // We need to indicate that this baby is safe.  Use an inner URI that
    // no one but the security manager will see.  Make sure to preserve our
    // path, in case someone decides to hardcode checks for particular
    // about: URIs somewhere.
    nsAutoCString spec;
    rv = url->GetPathQueryRef(spec);
    NS_ENSURE_SUCCESS(rv, rv);

    spec.InsertLiteral("moz-safe-about:", 0);

    nsCOMPtr<nsIURI> inner;
    rv = NS_NewURI(getter_AddRefs(inner), spec);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = NS_MutateURI(new nsNestedAboutURI::Mutator())
             .Apply(&nsINestedAboutURIMutator::InitWithBase, inner, aBaseURI)
             .SetSpec(aSpec)
             .Finalize(url);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  url.swap(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsAboutProtocolHandler::NewChannel(nsIURI* uri, nsILoadInfo* aLoadInfo,
                                   nsIChannel** result) {
  NS_ENSURE_ARG_POINTER(uri);

  // about:what you ask?
  nsCOMPtr<nsIAboutModule> aboutMod;
  nsresult rv = NS_GetAboutModule(uri, getter_AddRefs(aboutMod));

  nsAutoCString path;
  nsresult rv2 = NS_GetAboutModuleName(uri, path);
  if (NS_SUCCEEDED(rv2) && path.EqualsLiteral("srcdoc")) {
    // about:srcdoc is meant to be unresolvable, yet is included in the
    // about lookup tables so that it can pass security checks when used in
    // a srcdoc iframe.  To ensure that it stays unresolvable, we pretend
    // that it doesn't exist.
    rv = NS_ERROR_FACTORY_NOT_REGISTERED;
  }

  if (NS_SUCCEEDED(rv)) {
    // The standard return case:
    rv = aboutMod->NewChannel(uri, aLoadInfo, result);
    if (NS_SUCCEEDED(rv)) {
      // Not all implementations of nsIAboutModule::NewChannel()
      // set the LoadInfo on the newly created channel yet, as
      // an interim solution we set the LoadInfo here if not
      // available on the channel. Bug 1087720
      nsCOMPtr<nsILoadInfo> loadInfo = (*result)->LoadInfo();
      if (aLoadInfo != loadInfo) {
        NS_ASSERTION(false,
                     "nsIAboutModule->newChannel(aURI, aLoadInfo) needs to "
                     "set LoadInfo");
        AutoTArray<nsString, 2> params = {
            u"nsIAboutModule->newChannel(aURI)"_ns,
            u"nsIAboutModule->newChannel(aURI, aLoadInfo)"_ns};
        nsContentUtils::ReportToConsole(
            nsIScriptError::warningFlag, "Security by Default"_ns,
            nullptr,  // aDocument
            nsContentUtils::eNECKO_PROPERTIES, "APIDeprecationWarning", params);
        (*result)->SetLoadInfo(aLoadInfo);
      }

      // If this URI is safe for untrusted content, enforce that its
      // principal be based on the channel's originalURI by setting the
      // owner to null.
      // Note: this relies on aboutMod's newChannel implementation
      // having set the proper originalURI, which probably isn't ideal.
      if (IsSafeForUntrustedContent(aboutMod, uri)) {
        (*result)->SetOwner(nullptr);
      }

      RefPtr<nsNestedAboutURI> aboutURI;
      nsresult rv2 =
          uri->QueryInterface(kNestedAboutURICID, getter_AddRefs(aboutURI));
      if (NS_SUCCEEDED(rv2) && aboutURI->GetBaseURI()) {
        nsCOMPtr<nsIWritablePropertyBag2> writableBag =
            do_QueryInterface(*result);
        if (writableBag) {
          writableBag->SetPropertyAsInterface(u"baseURI"_ns,
                                              aboutURI->GetBaseURI());
        }
      }
    }
    return rv;
  }

  // mumble...

  if (rv == NS_ERROR_FACTORY_NOT_REGISTERED) {
    // This looks like an about: we don't know about.  Convert
    // this to an invalid URI error.
    rv = NS_ERROR_MALFORMED_URI;
  }

  return rv;
}

NS_IMETHODIMP
nsAboutProtocolHandler::AllowPort(int32_t port, const char* scheme,
                                  bool* _retval) {
  // don't override anything.
  *_retval = false;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// Safe about protocol handler impl

NS_IMPL_ISUPPORTS(nsSafeAboutProtocolHandler, nsIProtocolHandler,
                  nsISupportsWeakReference)

// nsIProtocolHandler methods:

NS_IMETHODIMP
nsSafeAboutProtocolHandler::GetScheme(nsACString& result) {
  result.AssignLiteral("moz-safe-about");
  return NS_OK;
}

NS_IMETHODIMP
nsSafeAboutProtocolHandler::GetDefaultPort(int32_t* result) {
  *result = -1;  // no port for moz-safe-about: URLs
  return NS_OK;
}

NS_IMETHODIMP
nsSafeAboutProtocolHandler::GetProtocolFlags(uint32_t* result) {
  *result = URI_NORELATIVE | URI_NOAUTH | URI_LOADABLE_BY_ANYONE |
            URI_IS_POTENTIALLY_TRUSTWORTHY;
  return NS_OK;
}

NS_IMETHODIMP
nsSafeAboutProtocolHandler::NewChannel(nsIURI* uri, nsILoadInfo* aLoadInfo,
                                       nsIChannel** result) {
  *result = nullptr;
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsSafeAboutProtocolHandler::AllowPort(int32_t port, const char* scheme,
                                      bool* _retval) {
  // don't override anything.
  *_retval = false;
  return NS_OK;
}

////////////////////////////////////////////////////////////
// nsNestedAboutURI implementation

NS_IMPL_CLASSINFO(nsNestedAboutURI, nullptr, nsIClassInfo::THREADSAFE,
                  NS_NESTEDABOUTURI_CID);
// Empty CI getter. We only need nsIClassInfo for Serialization
NS_IMPL_CI_INTERFACE_GETTER0(nsNestedAboutURI)

NS_INTERFACE_MAP_BEGIN(nsNestedAboutURI)
  if (aIID.Equals(kNestedAboutURICID)) {
    foundInterface = static_cast<nsIURI*>(this);
  } else
    NS_IMPL_QUERY_CLASSINFO(nsNestedAboutURI)
NS_INTERFACE_MAP_END_INHERITING(nsSimpleNestedURI)

// nsISerializable

NS_IMETHODIMP
nsNestedAboutURI::Read(nsIObjectInputStream* aStream) {
  MOZ_ASSERT_UNREACHABLE("Use nsIURIMutator.read() instead");
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsNestedAboutURI::ReadPrivate(nsIObjectInputStream* aStream) {
  nsresult rv = nsSimpleNestedURI::ReadPrivate(aStream);
  if (NS_FAILED(rv)) return rv;

  bool haveBase;
  rv = aStream->ReadBoolean(&haveBase);
  if (NS_FAILED(rv)) return rv;

  if (haveBase) {
    nsCOMPtr<nsISupports> supports;
    rv = aStream->ReadObject(true, getter_AddRefs(supports));
    if (NS_FAILED(rv)) return rv;

    mBaseURI = do_QueryInterface(supports, &rv);
    if (NS_FAILED(rv)) return rv;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNestedAboutURI::Write(nsIObjectOutputStream* aStream) {
  nsresult rv = nsSimpleNestedURI::Write(aStream);
  if (NS_FAILED(rv)) return rv;

  rv = aStream->WriteBoolean(mBaseURI != nullptr);
  if (NS_FAILED(rv)) return rv;

  if (mBaseURI) {
    // A previous iteration of this code wrote out mBaseURI as nsISupports
    // and then read it in as nsIURI, which is non-kosher when mBaseURI
    // implements more than just a single line of interfaces and the
    // canonical nsISupports* isn't the one a static_cast<> of mBaseURI
    // would produce.  For backwards compatibility with existing
    // serializations we continue to write mBaseURI as nsISupports but
    // switch to reading it as nsISupports, with a post-read QI to get to
    // nsIURI.
    rv = aStream->WriteCompoundObject(mBaseURI, NS_GET_IID(nsISupports), true);
    if (NS_FAILED(rv)) return rv;
  }

  return NS_OK;
}

NS_IMETHODIMP_(void)
nsNestedAboutURI::Serialize(mozilla::ipc::URIParams& aParams) {
  using namespace mozilla::ipc;

  NestedAboutURIParams params;
  URIParams nestedParams;

  nsSimpleNestedURI::Serialize(nestedParams);
  params.nestedParams() = nestedParams;

  if (mBaseURI) {
    SerializeURI(mBaseURI, params.baseURI());
  }

  aParams = params;
}

bool nsNestedAboutURI::Deserialize(const mozilla::ipc::URIParams& aParams) {
  using namespace mozilla::ipc;

  if (aParams.type() != URIParams::TNestedAboutURIParams) {
    NS_ERROR("Received unknown parameters from the other process!");
    return false;
  }

  const NestedAboutURIParams& params = aParams.get_NestedAboutURIParams();
  if (!nsSimpleNestedURI::Deserialize(params.nestedParams())) {
    return false;
  }

  mBaseURI = nullptr;
  if (params.baseURI()) {
    mBaseURI = DeserializeURI(*params.baseURI());
  }
  return true;
}

// nsSimpleURI
/* virtual */ nsSimpleURI* nsNestedAboutURI::StartClone(
    nsSimpleURI::RefHandlingEnum aRefHandlingMode, const nsACString& aNewRef) {
  // Sadly, we can't make use of nsSimpleNestedURI::StartClone here.
  // However, this function is expected to exactly match that function,
  // aside from the "new ns***URI()" call.
  NS_ENSURE_TRUE(mInnerURI, nullptr);

  nsCOMPtr<nsIURI> innerClone;
  nsresult rv = NS_OK;
  if (aRefHandlingMode == eHonorRef) {
    innerClone = mInnerURI;
  } else if (aRefHandlingMode == eReplaceRef) {
    rv = NS_GetURIWithNewRef(mInnerURI, aNewRef, getter_AddRefs(innerClone));
  } else {
    rv = NS_GetURIWithoutRef(mInnerURI, getter_AddRefs(innerClone));
  }

  if (NS_FAILED(rv)) {
    return nullptr;
  }

  nsNestedAboutURI* url = new nsNestedAboutURI(innerClone, mBaseURI);
  SetRefOnClone(url, aRefHandlingMode, aNewRef);

  return url;
}

// Queries this list of interfaces. If none match, it queries mURI.
NS_IMPL_NSIURIMUTATOR_ISUPPORTS(nsNestedAboutURI::Mutator, nsIURISetters,
                                nsIURIMutator, nsISerializable,
                                nsINestedAboutURIMutator)

NS_IMETHODIMP
nsNestedAboutURI::Mutate(nsIURIMutator** aMutator) {
  RefPtr<nsNestedAboutURI::Mutator> mutator = new nsNestedAboutURI::Mutator();
  nsresult rv = mutator->InitFromURI(this);
  if (NS_FAILED(rv)) {
    return rv;
  }
  mutator.forget(aMutator);
  return NS_OK;
}

}  // namespace net
}  // namespace mozilla
