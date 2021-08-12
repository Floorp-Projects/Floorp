/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"
#include "mozilla/Unused.h"
#include "mozilla/chrome/RegistryMessageUtils.h"
#include "mozilla/dom/ContentParent.h"

#include "SubstitutingProtocolHandler.h"
#include "SubstitutingURL.h"
#include "SubstitutingJARURI.h"
#include "nsIChannel.h"
#include "nsIIOService.h"
#include "nsIFile.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsReadableUtils.h"
#include "nsURLHelper.h"
#include "nsEscape.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsIClassInfoImpl.h"

using mozilla::dom::ContentParent;

namespace mozilla {
namespace net {

// Log module for Substituting Protocol logging. We keep the pre-existing module
// name of "nsResProtocol" to avoid disruption.
static LazyLogModule gResLog("nsResProtocol");

static NS_DEFINE_CID(kSubstitutingURLCID, NS_SUBSTITUTINGURL_CID);
static NS_DEFINE_CID(kSubstitutingJARURIImplCID,
                     NS_SUBSTITUTINGJARURI_IMPL_CID);

//---------------------------------------------------------------------------------
// SubstitutingURL : overrides nsStandardURL::GetFile to provide nsIFile
// resolution
//---------------------------------------------------------------------------------

// The list of interfaces should be in sync with nsStandardURL
// Queries this list of interfaces. If none match, it queries mURI.
NS_IMPL_NSIURIMUTATOR_ISUPPORTS(SubstitutingURL::Mutator, nsIURISetters,
                                nsIURIMutator, nsIStandardURLMutator,
                                nsIURLMutator, nsIFileURLMutator,
                                nsISerializable)

NS_IMPL_CLASSINFO(SubstitutingURL, nullptr, nsIClassInfo::THREADSAFE,
                  NS_SUBSTITUTINGURL_CID)
// Empty CI getter. We only need nsIClassInfo for Serialization
NS_IMPL_CI_INTERFACE_GETTER0(SubstitutingURL)

NS_IMPL_ADDREF_INHERITED(SubstitutingURL, nsStandardURL)
NS_IMPL_RELEASE_INHERITED(SubstitutingURL, nsStandardURL)
NS_IMPL_QUERY_INTERFACE_CI_INHERITED0(SubstitutingURL, nsStandardURL)

nsresult SubstitutingURL::EnsureFile() {
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
  nsCOMPtr<nsISubstitutingProtocolHandler> substHandler =
      do_QueryInterface(handler);
  MOZ_ASSERT(substHandler);

  nsAutoCString spec;
  rv = substHandler->ResolveURI(this, spec);
  if (NS_FAILED(rv)) return rv;

  nsAutoCString scheme;
  rv = net_ExtractURLScheme(spec, scheme);
  if (NS_FAILED(rv)) return rv;

  // Bug 585869:
  // In most cases, the scheme is jar if it's not file.
  // Regardless, net_GetFileFromURLSpec should be avoided
  // when the scheme isn't file.
  if (!scheme.EqualsLiteral("file")) return NS_ERROR_NO_INTERFACE;

  return net_GetFileFromURLSpec(spec, getter_AddRefs(mFile));
}

/* virtual */
nsStandardURL* SubstitutingURL::StartClone() {
  SubstitutingURL* clone = new SubstitutingURL();
  return clone;
}

void SubstitutingURL::Serialize(ipc::URIParams& aParams) {
  nsStandardURL::Serialize(aParams);
  aParams.get_StandardURLParams().isSubstituting() = true;
}

// SubstitutingJARURI

SubstitutingJARURI::SubstitutingJARURI(nsIURL* source, nsIJARURI* resolved)
    : mSource(source), mResolved(resolved) {}

// SubstitutingJARURI::nsIURI

NS_IMETHODIMP
SubstitutingJARURI::Equals(nsIURI* aOther, bool* aResult) {
  return EqualsInternal(aOther, eHonorRef, aResult);
}

NS_IMETHODIMP
SubstitutingJARURI::EqualsExceptRef(nsIURI* aOther, bool* aResult) {
  return EqualsInternal(aOther, eIgnoreRef, aResult);
}

nsresult SubstitutingJARURI::EqualsInternal(nsIURI* aOther,
                                            RefHandlingEnum aRefHandlingMode,
                                            bool* aResult) {
  *aResult = false;
  if (!aOther) {
    return NS_OK;
  }

  nsresult rv;
  RefPtr<SubstitutingJARURI> other;
  rv =
      aOther->QueryInterface(kSubstitutingJARURIImplCID, getter_AddRefs(other));
  if (NS_FAILED(rv)) {
    return NS_OK;
  }

  // We only need to check the source as the resolved URI is the same for a
  // given source
  return aRefHandlingMode == eHonorRef
             ? mSource->Equals(other->mSource, aResult)
             : mSource->EqualsExceptRef(other->mSource, aResult);
}

// SubstitutingJARURI::nsISerializable

NS_IMETHODIMP
SubstitutingJARURI::Read(nsIObjectInputStream* aStream) {
  MOZ_ASSERT(!mSource);
  MOZ_ASSERT(!mResolved);
  NS_ENSURE_ARG_POINTER(aStream);

  nsresult rv;
  nsCOMPtr<nsISupports> source;
  rv = aStream->ReadObject(true, getter_AddRefs(source));
  NS_ENSURE_SUCCESS(rv, rv);

  mSource = do_QueryInterface(source, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISupports> resolved;
  rv = aStream->ReadObject(true, getter_AddRefs(resolved));
  NS_ENSURE_SUCCESS(rv, rv);

  mResolved = do_QueryInterface(resolved, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
SubstitutingJARURI::Write(nsIObjectOutputStream* aStream) {
  NS_ENSURE_ARG_POINTER(aStream);

  nsresult rv;
  rv = aStream->WriteCompoundObject(mSource, NS_GET_IID(nsISupports), true);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aStream->WriteCompoundObject(mResolved, NS_GET_IID(nsISupports), true);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMPL_CLASSINFO(SubstitutingJARURI, nullptr, nsIClassInfo::MAIN_THREAD_ONLY,
                  NS_SUBSTITUTINGJARURI_CID)

NS_IMPL_ADDREF(SubstitutingJARURI)
NS_IMPL_RELEASE(SubstitutingJARURI)

NS_INTERFACE_MAP_BEGIN(SubstitutingJARURI)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIURI)
  NS_INTERFACE_MAP_ENTRY(nsIJARURI)
  NS_INTERFACE_MAP_ENTRY(nsIURL)
  NS_INTERFACE_MAP_ENTRY(nsIStandardURL)
  NS_INTERFACE_MAP_ENTRY(nsISerializable)
  if (aIID.Equals(kSubstitutingJARURIImplCID)) {
    foundInterface = static_cast<nsIURI*>(this);
  } else
    NS_INTERFACE_MAP_ENTRY(nsIURI)
  NS_IMPL_QUERY_CLASSINFO(SubstitutingJARURI)
NS_INTERFACE_MAP_END

NS_IMPL_CI_INTERFACE_GETTER(SubstitutingJARURI, nsIURI, nsIJARURI, nsIURL,
                            nsIStandardURL, nsISerializable)

SubstitutingProtocolHandler::SubstitutingProtocolHandler(const char* aScheme,
                                                         uint32_t aFlags,
                                                         bool aEnforceFileOrJar)
    : mScheme(aScheme),
      mSubstitutionsLock("SubstitutingProtocolHandler::mSubstitutions"),
      mSubstitutions(16),
      mEnforceFileOrJar(aEnforceFileOrJar) {
  mFlags.emplace(aFlags);
  ConstructInternal();
}

SubstitutingProtocolHandler::SubstitutingProtocolHandler(const char* aScheme)
    : mScheme(aScheme),
      mSubstitutionsLock("SubstitutingProtocolHandler::mSubstitutions"),
      mSubstitutions(16),
      mEnforceFileOrJar(true) {
  ConstructInternal();
}

void SubstitutingProtocolHandler::ConstructInternal() {
  nsresult rv;
  mIOService = do_GetIOService(&rv);
  MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv) && mIOService);
}

//
// IPC marshalling.
//

nsresult SubstitutingProtocolHandler::CollectSubstitutions(
    nsTArray<SubstitutionMapping>& aMappings) {
  AutoReadLock lock(mSubstitutionsLock);
  for (const auto& substitutionEntry : mSubstitutions) {
    const SubstitutionEntry& entry = substitutionEntry.GetData();
    nsCOMPtr<nsIURI> uri = entry.baseURI;
    SerializedURI serialized;
    if (uri) {
      nsresult rv = uri->GetSpec(serialized.spec);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    SubstitutionMapping substitution = {mScheme,
                                        nsCString(substitutionEntry.GetKey()),
                                        serialized, entry.flags};
    aMappings.AppendElement(substitution);
  }

  return NS_OK;
}

nsresult SubstitutingProtocolHandler::SendSubstitution(const nsACString& aRoot,
                                                       nsIURI* aBaseURI,
                                                       uint32_t aFlags) {
  if (GeckoProcessType_Content == XRE_GetProcessType()) {
    return NS_OK;
  }

  nsTArray<ContentParent*> parents;
  ContentParent::GetAll(parents);
  if (!parents.Length()) {
    return NS_OK;
  }

  SubstitutionMapping mapping;
  mapping.scheme = mScheme;
  mapping.path = aRoot;
  if (aBaseURI) {
    nsresult rv = aBaseURI->GetSpec(mapping.resolvedURI.spec);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  mapping.flags = aFlags;

  for (uint32_t i = 0; i < parents.Length(); i++) {
    Unused << parents[i]->SendRegisterChromeItem(mapping);
  }

  return NS_OK;
}

//----------------------------------------------------------------------------
// nsIProtocolHandler
//----------------------------------------------------------------------------

nsresult SubstitutingProtocolHandler::GetScheme(nsACString& result) {
  result = mScheme;
  return NS_OK;
}

nsresult SubstitutingProtocolHandler::GetDefaultPort(int32_t* result) {
  *result = -1;
  return NS_OK;
}

nsresult SubstitutingProtocolHandler::GetProtocolFlags(uint32_t* result) {
  if (mFlags.isNothing()) {
    NS_WARNING(
        "Trying to get protocol flags the wrong way - use "
        "nsIProtocolHandlerWithDynamicFlags instead");
    return NS_ERROR_NOT_AVAILABLE;
  }

  *result = mFlags.ref();
  return NS_OK;
}

nsresult SubstitutingProtocolHandler::NewURI(const nsACString& aSpec,
                                             const char* aCharset,
                                             nsIURI* aBaseURI,
                                             nsIURI** aResult) {
  // unescape any %2f and %2e to make sure nsStandardURL coalesces them.
  // Later net_GetFileFromURLSpec() will do a full unescape and we want to
  // treat them the same way the file system will. (bugs 380994, 394075)
  nsresult rv;
  nsAutoCString spec;
  const char* src = aSpec.BeginReading();
  const char* end = aSpec.EndReading();
  const char* last = src;

  spec.SetCapacity(aSpec.Length() + 1);
  for (; src < end; ++src) {
    if (*src == '%' && (src < end - 2) && *(src + 1) == '2') {
      char ch = '\0';
      if (*(src + 2) == 'f' || *(src + 2) == 'F') {
        ch = '/';
      } else if (*(src + 2) == 'e' || *(src + 2) == 'E') {
        ch = '.';
      }

      if (ch) {
        if (last < src) {
          spec.Append(last, src - last);
        }
        spec.Append(ch);
        src += 2;
        last = src + 1;  // src will be incremented by the loop
      }
    }
    if (*src == '?' || *src == '#') {
      break;  // Don't escape %2f and %2e in the query or ref parts of the URI
    }
  }

  if (last < end) {
    spec.Append(last, end - last);
  }

  nsCOMPtr<nsIURI> base(aBaseURI);
  nsCOMPtr<nsIURL> uri;
  rv = NS_MutateURI(new SubstitutingURL::Mutator())
           .Apply(NS_MutatorMethod(&nsIStandardURLMutator::Init,
                                   nsIStandardURL::URLTYPE_STANDARD, -1, spec,
                                   aCharset, base, nullptr))
           .Finalize(uri);
  if (NS_FAILED(rv)) return rv;

  nsAutoCString host;
  rv = uri->GetHost(host);
  if (NS_FAILED(rv)) return rv;

  // "android" is the only root that would return the RESOLVE_JAR_URI flag
  // see nsResProtocolHandler::GetSubstitutionInternal
  if (MustResolveJAR(host)) {
    return ResolveJARURI(uri, aResult);
  }

  uri.forget(aResult);
  return NS_OK;
}

nsresult SubstitutingProtocolHandler::ResolveJARURI(nsIURL* aURL,
                                                    nsIURI** aResult) {
  nsAutoCString spec;
  nsresult rv = ResolveURI(aURL, spec);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIURI> resolvedURI;
  rv = NS_NewURI(getter_AddRefs(resolvedURI), spec);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIURI> innermostURI = NS_GetInnermostURI(resolvedURI);
  nsAutoCString scheme;
  innermostURI->GetScheme(scheme);

  // We only ever want to resolve to a local jar.
  NS_ENSURE_TRUE(scheme.EqualsLiteral("file"), NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIJARURI> jarURI(do_QueryInterface(resolvedURI));
  if (!jarURI) {
    // This substitution does not resolve to a jar: URL, so we just
    // return the plain SubstitutionURL
    nsCOMPtr<nsIURI> url = aURL;
    url.forget(aResult);
    return NS_OK;
  }

  nsCOMPtr<nsIJARURI> result = new SubstitutingJARURI(aURL, jarURI);
  result.forget(aResult);

  return rv;
}

nsresult SubstitutingProtocolHandler::NewChannel(nsIURI* uri,
                                                 nsILoadInfo* aLoadInfo,
                                                 nsIChannel** result) {
  NS_ENSURE_ARG_POINTER(uri);
  NS_ENSURE_ARG_POINTER(aLoadInfo);

  nsAutoCString spec;
  nsresult rv = ResolveURI(uri, spec);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIURI> newURI;
  rv = NS_NewURI(getter_AddRefs(newURI), spec);
  NS_ENSURE_SUCCESS(rv, rv);

  // We don't want to allow the inner protocol handler to modify the result
  // principal URI since we want either |uri| or anything pre-set by upper
  // layers to prevail.
  nsCOMPtr<nsIURI> savedResultPrincipalURI;
  rv =
      aLoadInfo->GetResultPrincipalURI(getter_AddRefs(savedResultPrincipalURI));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = NS_NewChannelInternal(result, newURI, aLoadInfo);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aLoadInfo->SetResultPrincipalURI(savedResultPrincipalURI);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = (*result)->SetOriginalURI(uri);
  NS_ENSURE_SUCCESS(rv, rv);

  return SubstituteChannel(uri, aLoadInfo, result);
}

nsresult SubstitutingProtocolHandler::AllowPort(int32_t port,
                                                const char* scheme,
                                                bool* _retval) {
  // don't override anything.
  *_retval = false;
  return NS_OK;
}

//----------------------------------------------------------------------------
// nsISubstitutingProtocolHandler
//----------------------------------------------------------------------------

nsresult SubstitutingProtocolHandler::SetSubstitution(const nsACString& root,
                                                      nsIURI* baseURI) {
  // Add-ons use this API but they should not be able to make anything
  // content-accessible.
  return SetSubstitutionWithFlags(root, baseURI, 0);
}

nsresult SubstitutingProtocolHandler::SetSubstitutionWithFlags(
    const nsACString& origRoot, nsIURI* baseURI, uint32_t flags) {
  nsAutoCString root;
  ToLowerCase(origRoot, root);

  if (!baseURI) {
    {
      AutoWriteLock lock(mSubstitutionsLock);
      mSubstitutions.Remove(root);
    }

    return SendSubstitution(root, baseURI, flags);
  }

  // If baseURI isn't a same-scheme URI, we can set the substitution
  // immediately.
  nsAutoCString scheme;
  nsresult rv = baseURI->GetScheme(scheme);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!scheme.Equals(mScheme)) {
    if (mEnforceFileOrJar && !scheme.EqualsLiteral("file") &&
        !scheme.EqualsLiteral("jar") && !scheme.EqualsLiteral("app") &&
        !scheme.EqualsLiteral("resource")) {
      NS_WARNING("Refusing to create substituting URI to non-file:// target");
      return NS_ERROR_INVALID_ARG;
    }

    {
      AutoWriteLock lock(mSubstitutionsLock);
      mSubstitutions.InsertOrUpdate(root, SubstitutionEntry{baseURI, flags});
    }

    return SendSubstitution(root, baseURI, flags);
  }

  // baseURI is a same-type substituting URI, let's resolve it first.
  nsAutoCString newBase;
  rv = ResolveURI(baseURI, newBase);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIURI> newBaseURI;
  rv =
      mIOService->NewURI(newBase, nullptr, nullptr, getter_AddRefs(newBaseURI));
  NS_ENSURE_SUCCESS(rv, rv);

  {
    AutoWriteLock lock(mSubstitutionsLock);
    mSubstitutions.InsertOrUpdate(root, SubstitutionEntry{newBaseURI, flags});
  }

  return SendSubstitution(root, newBaseURI, flags);
}

nsresult SubstitutingProtocolHandler::GetSubstitution(
    const nsACString& origRoot, nsIURI** result) {
  NS_ENSURE_ARG_POINTER(result);

  nsAutoCString root;
  ToLowerCase(origRoot, root);

  {
    AutoReadLock lock(mSubstitutionsLock);
    SubstitutionEntry entry;
    if (mSubstitutions.Get(root, &entry)) {
      nsCOMPtr<nsIURI> baseURI = entry.baseURI;
      baseURI.forget(result);
      return NS_OK;
    }
  }

  uint32_t flags;
  return GetSubstitutionInternal(root, result, &flags);
}

nsresult SubstitutingProtocolHandler::GetSubstitutionFlags(
    const nsACString& root, uint32_t* flags) {
#ifdef DEBUG
  nsAutoCString lcRoot;
  ToLowerCase(root, lcRoot);
  MOZ_ASSERT(root.Equals(lcRoot),
             "GetSubstitutionFlags should never receive mixed-case root name");
#endif

  *flags = 0;

  {
    AutoReadLock lock(mSubstitutionsLock);

    SubstitutionEntry entry;
    if (mSubstitutions.Get(root, &entry)) {
      *flags = entry.flags;
      return NS_OK;
    }
  }

  nsCOMPtr<nsIURI> baseURI;
  return GetSubstitutionInternal(root, getter_AddRefs(baseURI), flags);
}

nsresult SubstitutingProtocolHandler::HasSubstitution(
    const nsACString& origRoot, bool* result) {
  NS_ENSURE_ARG_POINTER(result);

  nsAutoCString root;
  ToLowerCase(origRoot, root);

  *result = HasSubstitution(root);
  return NS_OK;
}

nsresult SubstitutingProtocolHandler::ResolveURI(nsIURI* uri,
                                                 nsACString& result) {
  nsresult rv;

  nsAutoCString host;
  nsAutoCString path;
  nsAutoCString pathname;

  nsCOMPtr<nsIURL> url = do_QueryInterface(uri);
  if (!url) {
    return NS_ERROR_MALFORMED_URI;
  }

  rv = uri->GetAsciiHost(host);
  if (NS_FAILED(rv)) return rv;

  rv = uri->GetPathQueryRef(path);
  if (NS_FAILED(rv)) return rv;

  rv = url->GetFilePath(pathname);
  if (NS_FAILED(rv)) return rv;

  if (ResolveSpecialCases(host, path, pathname, result)) {
    return NS_OK;
  }

  nsCOMPtr<nsIURI> baseURI;
  rv = GetSubstitution(host, getter_AddRefs(baseURI));
  if (NS_FAILED(rv)) return rv;

  // Unescape the path so we can perform some checks on it.
  NS_UnescapeURL(pathname);
  if (pathname.FindChar('\\') != -1) {
    return NS_ERROR_MALFORMED_URI;
  }

  // Some code relies on an empty path resolving to a file rather than a
  // directory.
  NS_ASSERTION(path.CharAt(0) == '/', "Path must begin with '/'");
  if (path.Length() == 1) {
    rv = baseURI->GetSpec(result);
  } else {
    // Make sure we always resolve the path as file-relative to our target URI.
    // When the baseURI is a nsIFileURL, and the directory it points to doesn't
    // exist, it doesn't end with a /. In that case, a file-relative resolution
    // is going to pick something in the parent directory, so we resolve using
    // an absolute path derived from the full path in that case.
    nsCOMPtr<nsIFileURL> baseDir = do_QueryInterface(baseURI);
    if (baseDir) {
      nsAutoCString basePath;
      rv = baseURI->GetFilePath(basePath);
      if (NS_SUCCEEDED(rv) && !StringEndsWith(basePath, "/"_ns)) {
        // Cf. the assertion above, path already starts with a /, so prefixing
        // with a string that doesn't end with one will leave us wit the right
        // amount of /.
        path.Insert(basePath, 0);
      } else {
        // Allow to fall through below.
        baseDir = nullptr;
      }
    }
    if (!baseDir) {
      path.Insert('.', 0);
    }
    rv = baseURI->Resolve(path, result);
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (MOZ_LOG_TEST(gResLog, LogLevel::Debug)) {
    nsAutoCString spec;
    uri->GetAsciiSpec(spec);
    MOZ_LOG(gResLog, LogLevel::Debug,
            ("%s\n -> %s\n", spec.get(), PromiseFlatCString(result).get()));
  }
  return rv;
}

}  // namespace net
}  // namespace mozilla
