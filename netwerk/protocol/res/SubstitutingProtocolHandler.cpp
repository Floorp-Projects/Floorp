/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/chrome/RegistryMessageUtils.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/unused.h"

#include "SubstitutingProtocolHandler.h"
#include "nsIChannel.h"
#include "nsIIOService.h"
#include "nsIFile.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsURLHelper.h"
#include "nsEscape.h"

using mozilla::dom::ContentParent;

namespace mozilla {

// Log module for Substituting Protocol logging. We keep the pre-existing module
// name of "nsResProtocol" to avoid disruption.
static LazyLogModule gResLog("nsResProtocol");

static NS_DEFINE_CID(kSubstitutingURLCID, NS_SUBSTITUTINGURL_CID);

//---------------------------------------------------------------------------------
// SubstitutingURL : overrides nsStandardURL::GetFile to provide nsIFile resolution
//---------------------------------------------------------------------------------

nsresult
SubstitutingURL::EnsureFile()
{
  nsAutoCString ourScheme;
  nsresult rv = GetScheme(ourScheme);
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the handler associated with this scheme. It would be nice to just
  // pass this in when constructing SubstitutingURLs, but we need a generic
  // factory constructor.
  nsCOMPtr<nsIIOService> io = do_GetIOService(&rv);
  nsCOMPtr<nsIProtocolHandler> handler;
  rv = io->GetProtocolHandler(ourScheme.get(), getter_AddRefs(handler));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsISubstitutingProtocolHandler> substHandler = do_QueryInterface(handler);
  MOZ_ASSERT(substHandler);

  nsAutoCString spec;
  rv = substHandler->ResolveURI(this, spec);
  if (NS_FAILED(rv))
    return rv;

  nsAutoCString scheme;
  rv = net_ExtractURLScheme(spec, nullptr, nullptr, &scheme);
  if (NS_FAILED(rv))
    return rv;

  // Bug 585869:
  // In most cases, the scheme is jar if it's not file.
  // Regardless, net_GetFileFromURLSpec should be avoided
  // when the scheme isn't file.
  if (!scheme.EqualsLiteral("file"))
    return NS_ERROR_NO_INTERFACE;

  return net_GetFileFromURLSpec(spec, getter_AddRefs(mFile));
}

/* virtual */ nsStandardURL*
SubstitutingURL::StartClone()
{
  SubstitutingURL *clone = new SubstitutingURL();
  return clone;
}

NS_IMETHODIMP
SubstitutingURL::GetClassIDNoAlloc(nsCID *aClassIDNoAlloc)
{
  *aClassIDNoAlloc = kSubstitutingURLCID;
  return NS_OK;
}

SubstitutingProtocolHandler::SubstitutingProtocolHandler(const char* aScheme, uint32_t aFlags,
                                                         bool aEnforceFileOrJar)
  : mScheme(aScheme)
  , mSubstitutions(16)
  , mEnforceFileOrJar(aEnforceFileOrJar)
{
  mFlags.emplace(aFlags);
  ConstructInternal();
}

SubstitutingProtocolHandler::SubstitutingProtocolHandler(const char* aScheme)
  : mScheme(aScheme)
  , mSubstitutions(16)
  , mEnforceFileOrJar(true)
{
  ConstructInternal();
}

void
SubstitutingProtocolHandler::ConstructInternal()
{
  nsresult rv;
  mIOService = do_GetIOService(&rv);
  MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv) && mIOService);
}

//
// IPC marshalling.
//

void
SubstitutingProtocolHandler::CollectSubstitutions(InfallibleTArray<SubstitutionMapping>& aMappings)
{
  for (auto iter = mSubstitutions.ConstIter(); !iter.Done(); iter.Next()) {
    nsCOMPtr<nsIURI> uri = iter.Data();
    SerializedURI serialized;
    if (uri) {
      uri->GetSpec(serialized.spec);
      uri->GetOriginCharset(serialized.charset);
    }
    SubstitutionMapping substitution = { mScheme, nsCString(iter.Key()), serialized };
    aMappings.AppendElement(substitution);
  }
}

void
SubstitutingProtocolHandler::SendSubstitution(const nsACString& aRoot, nsIURI* aBaseURI)
{
  if (GeckoProcessType_Content == XRE_GetProcessType()) {
    return;
  }

  nsTArray<ContentParent*> parents;
  ContentParent::GetAll(parents);
  if (!parents.Length()) {
    return;
  }

  SubstitutionMapping mapping;
  mapping.scheme = mScheme;
  mapping.path = aRoot;
  if (aBaseURI) {
    aBaseURI->GetSpec(mapping.resolvedURI.spec);
    aBaseURI->GetOriginCharset(mapping.resolvedURI.charset);
  }

  for (uint32_t i = 0; i < parents.Length(); i++) {
    Unused << parents[i]->SendRegisterChromeItem(mapping);
  }
}

//----------------------------------------------------------------------------
// nsIProtocolHandler
//----------------------------------------------------------------------------

nsresult
SubstitutingProtocolHandler::GetScheme(nsACString &result)
{
  result = mScheme;
  return NS_OK;
}

nsresult
SubstitutingProtocolHandler::GetDefaultPort(int32_t *result)
{
  *result = -1;
  return NS_OK;
}

nsresult
SubstitutingProtocolHandler::GetProtocolFlags(uint32_t *result)
{
  if (mFlags.isNothing()) {
    NS_WARNING("Trying to get protocol flags the wrong way - use nsIProtocolHandlerWithDynamicFlags instead");
    return NS_ERROR_NOT_AVAILABLE;
  }

  *result = mFlags.ref();
  return NS_OK;
}

nsresult
SubstitutingProtocolHandler::NewURI(const nsACString &aSpec,
                                    const char *aCharset,
                                    nsIURI *aBaseURI,
                                    nsIURI **result)
{
  nsresult rv;

  RefPtr<SubstitutingURL> url = new SubstitutingURL();
  if (!url)
    return NS_ERROR_OUT_OF_MEMORY;

  // unescape any %2f and %2e to make sure nsStandardURL coalesces them.
  // Later net_GetFileFromURLSpec() will do a full unescape and we want to
  // treat them the same way the file system will. (bugs 380994, 394075)
  nsAutoCString spec;
  const char *src = aSpec.BeginReading();
  const char *end = aSpec.EndReading();
  const char *last = src;

  spec.SetCapacity(aSpec.Length()+1);
  for ( ; src < end; ++src) {
    if (*src == '%' && (src < end-2) && *(src+1) == '2') {
      char ch = '\0';
      if (*(src+2) == 'f' || *(src+2) == 'F') {
        ch = '/';
      } else if (*(src+2) == 'e' || *(src+2) == 'E') {
        ch = '.';
      }

      if (ch) {
        if (last < src) {
          spec.Append(last, src-last);
        }
        spec.Append(ch);
        src += 2;
        last = src+1; // src will be incremented by the loop
      }
    }
  }
  if (last < src)
    spec.Append(last, src-last);

  rv = url->Init(nsIStandardURL::URLTYPE_STANDARD, -1, spec, aCharset, aBaseURI);
  if (NS_SUCCEEDED(rv)) {
    url.forget(result);
  }
  return rv;
}

nsresult
SubstitutingProtocolHandler::NewChannel2(nsIURI* uri,
                                         nsILoadInfo* aLoadInfo,
                                         nsIChannel** result)
{
  NS_ENSURE_ARG_POINTER(uri);
  nsAutoCString spec;
  nsresult rv = ResolveURI(uri, spec);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIURI> newURI;
  rv = NS_NewURI(getter_AddRefs(newURI), spec);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = NS_NewChannelInternal(result, newURI, aLoadInfo);
  NS_ENSURE_SUCCESS(rv, rv);

  nsLoadFlags loadFlags = 0;
  (*result)->GetLoadFlags(&loadFlags);
  (*result)->SetLoadFlags(loadFlags & ~nsIChannel::LOAD_REPLACE);
  rv = (*result)->SetOriginalURI(uri);
  NS_ENSURE_SUCCESS(rv, rv);

  return SubstituteChannel(uri, aLoadInfo, result);
}

nsresult
SubstitutingProtocolHandler::NewChannel(nsIURI* uri, nsIChannel* *result)
{
  return NewChannel2(uri, nullptr, result);
}

nsresult
SubstitutingProtocolHandler::AllowPort(int32_t port, const char *scheme, bool *_retval)
{
  // don't override anything.
  *_retval = false;
  return NS_OK;
}

//----------------------------------------------------------------------------
// nsISubstitutingProtocolHandler
//----------------------------------------------------------------------------

nsresult
SubstitutingProtocolHandler::SetSubstitution(const nsACString& root, nsIURI *baseURI)
{
  if (!baseURI) {
    mSubstitutions.Remove(root);
    SendSubstitution(root, baseURI);
    return NS_OK;
  }

  // If baseURI isn't a same-scheme URI, we can set the substitution immediately.
  nsAutoCString scheme;
  nsresult rv = baseURI->GetScheme(scheme);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!scheme.Equals(mScheme)) {
    if (mEnforceFileOrJar && !scheme.EqualsLiteral("file") && !scheme.EqualsLiteral("jar")
        && !scheme.EqualsLiteral("app")) {
      NS_WARNING("Refusing to create substituting URI to non-file:// target");
      return NS_ERROR_INVALID_ARG;
    }

    mSubstitutions.Put(root, baseURI);
    SendSubstitution(root, baseURI);
    return NS_OK;
  }

  // baseURI is a same-type substituting URI, let's resolve it first.
  nsAutoCString newBase;
  rv = ResolveURI(baseURI, newBase);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIURI> newBaseURI;
  rv = mIOService->NewURI(newBase, nullptr, nullptr, getter_AddRefs(newBaseURI));
  NS_ENSURE_SUCCESS(rv, rv);

  mSubstitutions.Put(root, newBaseURI);
  SendSubstitution(root, newBaseURI);
  return NS_OK;
}

nsresult
SubstitutingProtocolHandler::GetSubstitution(const nsACString& root, nsIURI **result)
{
  NS_ENSURE_ARG_POINTER(result);

  if (mSubstitutions.Get(root, result))
    return NS_OK;

  return GetSubstitutionInternal(root, result);
}

nsresult
SubstitutingProtocolHandler::HasSubstitution(const nsACString& root, bool *result)
{
  NS_ENSURE_ARG_POINTER(result);
  *result = HasSubstitution(root);
  return NS_OK;
}

nsresult
SubstitutingProtocolHandler::ResolveURI(nsIURI *uri, nsACString &result)
{
  nsresult rv;

  nsAutoCString host;
  nsAutoCString path;

  rv = uri->GetAsciiHost(host);
  if (NS_FAILED(rv)) return rv;

  rv = uri->GetPath(path);
  if (NS_FAILED(rv)) return rv;

  if (ResolveSpecialCases(host, path, result)) {
    return NS_OK;
  }

  // Unescape the path so we can perform some checks on it.
  nsAutoCString unescapedPath(path);
  NS_UnescapeURL(unescapedPath);

  // Don't misinterpret the filepath as an absolute URI.
  if (unescapedPath.FindChar(':') != -1)
    return NS_ERROR_MALFORMED_URI;

  if (unescapedPath.FindChar('\\') != -1)
    return NS_ERROR_MALFORMED_URI;

  const char *p = path.get() + 1; // path always starts with a slash
  NS_ASSERTION(*(p-1) == '/', "Path did not begin with a slash!");

  if (*p == '/')
    return NS_ERROR_MALFORMED_URI;

  nsCOMPtr<nsIURI> baseURI;
  rv = GetSubstitution(host, getter_AddRefs(baseURI));
  if (NS_FAILED(rv)) return rv;

  rv = baseURI->Resolve(nsDependentCString(p, path.Length()-1), result);

  if (MOZ_LOG_TEST(gResLog, LogLevel::Debug)) {
    nsAutoCString spec;
    uri->GetAsciiSpec(spec);
    MOZ_LOG(gResLog, LogLevel::Debug, ("%s\n -> %s\n", spec.get(), PromiseFlatCString(result).get()));
  }
  return rv;
}

} // namespace mozilla
