/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsClientAuthRemember.h"

#include "mozilla/BasePrincipal.h"
#include "mozilla/RefPtr.h"
#include "nsCRT.h"
#include "nsINSSComponent.h"
#include "nsPrintfCString.h"
#include "nsNSSComponent.h"
#include "nsIDataStorage.h"
#include "nsIObserverService.h"
#include "nsNetUtil.h"
#include "nsPromiseFlatString.h"
#include "nsThreadUtils.h"
#include "nsStringBuffer.h"
#include "cert.h"
#include "nspr.h"
#include "pk11pub.h"
#include "certdb.h"
#include "sechash.h"
#include "SharedSSLState.h"

#include "nsJSUtils.h"

#ifdef XP_MACOSX
#  include <CoreFoundation/CoreFoundation.h>
#  include <Security/Security.h>
#  include "KeychainSecret.h"  // for ScopedCFType
#endif                         // XP_MACOSX

using namespace mozilla;
using namespace mozilla::psm;

NS_IMPL_ISUPPORTS(nsClientAuthRememberService, nsIClientAuthRememberService)
NS_IMPL_ISUPPORTS(nsClientAuthRemember, nsIClientAuthRememberRecord)

const nsCString nsClientAuthRemember::SentinelValue =
    "no client certificate"_ns;

NS_IMETHODIMP
nsClientAuthRemember::GetAsciiHost(/*out*/ nsACString& aAsciiHost) {
  aAsciiHost = mAsciiHost;
  return NS_OK;
}

NS_IMETHODIMP
nsClientAuthRemember::GetDbKey(/*out*/ nsACString& aDBKey) {
  aDBKey = mDBKey;
  return NS_OK;
}

NS_IMETHODIMP
nsClientAuthRemember::GetEntryKey(/*out*/ nsACString& aEntryKey) {
  aEntryKey.Assign(mAsciiHost);
  aEntryKey.Append(',');
  // This used to include the SHA-256 hash of the server certificate.
  aEntryKey.Append(',');
  aEntryKey.Append(mOriginAttributesSuffix);
  return NS_OK;
}

nsresult nsClientAuthRememberService::Init() {
  if (!NS_IsMainThread()) {
    NS_ERROR("nsClientAuthRememberService::Init called off the main thread");
    return NS_ERROR_NOT_SAME_THREAD;
  }

  nsCOMPtr<nsIDataStorageManager> dataStorageManager(
      do_GetService("@mozilla.org/security/datastoragemanager;1"));
  if (!dataStorageManager) {
    return NS_ERROR_FAILURE;
  }
  nsresult rv =
      dataStorageManager->Get(nsIDataStorageManager::ClientAuthRememberList,
                              getter_AddRefs(mClientAuthRememberList));
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (!mClientAuthRememberList) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsClientAuthRememberService::ForgetRememberedDecision(const nsACString& key) {
  nsresult rv = mClientAuthRememberList->Remove(
      PromiseFlatCString(key), nsIDataStorage::DataType::Persistent);
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(NS_NSSCOMPONENT_CID));
  if (!nssComponent) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  return nssComponent->ClearSSLExternalAndInternalSessionCache();
}

NS_IMETHODIMP
nsClientAuthRememberService::GetDecisions(
    nsTArray<RefPtr<nsIClientAuthRememberRecord>>& results) {
  nsTArray<RefPtr<nsIDataStorageItem>> decisions;
  nsresult rv = mClientAuthRememberList->GetAll(decisions);
  if (NS_FAILED(rv)) {
    return rv;
  }

  for (const auto& decision : decisions) {
    nsIDataStorage::DataType type;
    rv = decision->GetType(&type);
    if (NS_FAILED(rv)) {
      return rv;
    }
    if (type == nsIDataStorage::DataType::Persistent) {
      nsAutoCString key;
      rv = decision->GetKey(key);
      if (NS_FAILED(rv)) {
        return rv;
      }
      nsAutoCString value;
      rv = decision->GetValue(value);
      if (NS_FAILED(rv)) {
        return rv;
      }
      RefPtr<nsIClientAuthRememberRecord> tmp =
          new nsClientAuthRemember(key, value);

      results.AppendElement(tmp);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsClientAuthRememberService::ClearRememberedDecisions() {
  nsresult rv = mClientAuthRememberList->Clear();
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(NS_NSSCOMPONENT_CID));
  if (!nssComponent) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  return nssComponent->ClearSSLExternalAndInternalSessionCache();
}

NS_IMETHODIMP
nsClientAuthRememberService::DeleteDecisionsByHost(
    const nsACString& aHostName, JS::Handle<JS::Value> aOriginAttributes,
    JSContext* aCx) {
  OriginAttributes attrs;
  if (!aOriginAttributes.isObject() || !attrs.Init(aCx, aOriginAttributes)) {
    return NS_ERROR_INVALID_ARG;
  }
  nsIDataStorage::DataType storageType = GetDataStorageType(attrs);

  nsTArray<RefPtr<nsIDataStorageItem>> decisions;
  nsresult rv = mClientAuthRememberList->GetAll(decisions);
  if (NS_FAILED(rv)) {
    return rv;
  }

  for (const auto& decision : decisions) {
    nsIDataStorage::DataType type;
    nsresult rv = decision->GetType(&type);
    if (NS_FAILED(rv)) {
      return rv;
    }
    if (type == storageType) {
      nsAutoCString key;
      rv = decision->GetKey(key);
      if (NS_FAILED(rv)) {
        return rv;
      }
      nsAutoCString value;
      rv = decision->GetValue(value);
      if (NS_FAILED(rv)) {
        return rv;
      }
      RefPtr<nsIClientAuthRememberRecord> tmp =
          new nsClientAuthRemember(key, value);
      nsAutoCString asciiHost;
      tmp->GetAsciiHost(asciiHost);
      if (asciiHost.Equals(aHostName)) {
        rv = mClientAuthRememberList->Remove(key, type);
        if (NS_FAILED(rv)) {
          return rv;
        }
      }
    }
  }
  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(NS_NSSCOMPONENT_CID));
  if (!nssComponent) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  return nssComponent->ClearSSLExternalAndInternalSessionCache();
}

NS_IMETHODIMP
nsClientAuthRememberService::RememberDecisionScriptable(
    const nsACString& aHostName, JS::Handle<JS::Value> aOriginAttributes,
    nsIX509Cert* aClientCert, JSContext* aCx) {
  OriginAttributes attrs;
  if (!aOriginAttributes.isObject() || !attrs.Init(aCx, aOriginAttributes)) {
    return NS_ERROR_INVALID_ARG;
  }
  return RememberDecision(aHostName, attrs, aClientCert);
}

NS_IMETHODIMP
nsClientAuthRememberService::RememberDecision(
    const nsACString& aHostName, const OriginAttributes& aOriginAttributes,
    nsIX509Cert* aClientCert) {
  if (aHostName.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  // aClientCert == nullptr means: remember that user does not want to use a
  // cert
  if (aClientCert) {
    nsAutoCString dbkey;
    nsresult rv = aClientCert->GetDbKey(dbkey);
    if (NS_FAILED(rv)) {
      return rv;
    }
    return AddEntryToList(aHostName, aOriginAttributes, dbkey);
  }
  return AddEntryToList(aHostName, aOriginAttributes,
                        nsClientAuthRemember::SentinelValue);
}

#ifdef XP_MACOSX
// On macOS, users can add "identity preference" items in the keychain. These
// can be added via the Keychain Access tool. These specify mappings from
// URLs/wildcards like "*.mozilla.org" to specific client certificates. This
// function retrieves the preferred client certificate for a hostname by
// querying a system API that checks for these identity preferences.
nsresult CheckForPreferredCertificate(const nsACString& aHostName,
                                      nsACString& aCertDBKey) {
  aCertDBKey.Truncate();
  // SecIdentityCopyPreferred seems to expect a proper URI which it can use
  // for prefix and wildcard matches.
  // We don't have the full URL but we can turn the hostname into a URI with
  // an authority section, so that it matches against macOS identity preferences
  // like `*.foo.com`. If we know that this connection is always going to be
  // https, then we should put that in the URI as well, so that it matches
  // identity preferences like `https://foo.com/` as well. If we can plumb
  // the path or the full URL into this function we could also match identity
  // preferences like `https://foo.com/bar/` but for now we cannot.
  nsPrintfCString fakeUrl("//%s/", PromiseFlatCString(aHostName).get());
  ScopedCFType<CFStringRef> host(::CFStringCreateWithCString(
      kCFAllocatorDefault, fakeUrl.get(), kCFStringEncodingUTF8));
  if (!host) {
    return NS_ERROR_UNEXPECTED;
  }
  ScopedCFType<SecIdentityRef> identity(
      ::SecIdentityCopyPreferred(host.get(), NULL, NULL));
  if (!identity) {
    // No preferred identity for this hostname, leave aCertDBKey empty and
    // return
    return NS_OK;
  }
  SecCertificateRef certRefRaw = NULL;
  OSStatus copyResult =
      ::SecIdentityCopyCertificate(identity.get(), &certRefRaw);
  ScopedCFType<SecCertificateRef> certRef(certRefRaw);
  if (copyResult != errSecSuccess || certRef.get() == NULL) {
    return NS_ERROR_UNEXPECTED;
  }
  ScopedCFType<CFDataRef> der(::SecCertificateCopyData(certRef.get()));
  if (!der) {
    return NS_ERROR_UNEXPECTED;
  }

  nsTArray<uint8_t> derArray(::CFDataGetBytePtr(der.get()),
                             ::CFDataGetLength(der.get()));
  nsCOMPtr<nsIX509Cert> cert(new nsNSSCertificate(std::move(derArray)));
  return cert->GetDbKey(aCertDBKey);
}
#endif

void nsClientAuthRememberService::Migrate() {
  MOZ_ASSERT(NS_IsMainThread());
  static bool migrated = false;
  if (migrated) {
    return;
  }
  migrated = true;
  nsTArray<RefPtr<nsIDataStorageItem>> decisions;
  nsresult rv = mClientAuthRememberList->GetAll(decisions);
  if (NS_FAILED(rv)) {
    return;
  }
  for (const auto& decision : decisions) {
    nsIDataStorage::DataType type;
    if (NS_FAILED(decision->GetType(&type))) {
      continue;
    }
    if (type != nsIDataStorage::DataType::Persistent) {
      continue;
    }
    nsAutoCString key;
    if (NS_FAILED(decision->GetKey(key))) {
      continue;
    }
    nsAutoCString value;
    if (NS_FAILED(decision->GetValue(value))) {
      continue;
    }
    RefPtr<nsClientAuthRemember> entry(new nsClientAuthRemember(key, value));
    nsAutoCString newKey;
    if (NS_FAILED(entry->GetEntryKey(newKey))) {
      continue;
    }
    if (newKey != key) {
      if (NS_FAILED(mClientAuthRememberList->Remove(
              key, nsIDataStorage::DataType::Persistent))) {
        continue;
      }
      if (NS_FAILED(mClientAuthRememberList->Put(
              newKey, value, nsIDataStorage::DataType::Persistent))) {
        continue;
      }
    }
  }
}

NS_IMETHODIMP
nsClientAuthRememberService::HasRememberedDecision(
    const nsACString& aHostName, const OriginAttributes& aOriginAttributes,
    nsACString& aCertDBKey, bool* aRetVal) {
  NS_ENSURE_ARG_POINTER(aRetVal);
  if (aHostName.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }
  if (!NS_IsMainThread()) {
    return NS_ERROR_NOT_SAME_THREAD;
  }

  *aRetVal = false;
  aCertDBKey.Truncate();

  Migrate();

  nsAutoCString entryKey;
  RefPtr<nsClientAuthRemember> entry(
      new nsClientAuthRemember(aHostName, aOriginAttributes));
  nsresult rv = entry->GetEntryKey(entryKey);
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsIDataStorage::DataType storageType = GetDataStorageType(aOriginAttributes);

  nsAutoCString listEntry;
  rv = mClientAuthRememberList->Get(entryKey, storageType, listEntry);
  if (NS_FAILED(rv) && rv != NS_ERROR_NOT_AVAILABLE) {
    return rv;
  }
  if (NS_SUCCEEDED(rv) && !listEntry.IsEmpty()) {
    if (!listEntry.Equals(nsClientAuthRemember::SentinelValue)) {
      aCertDBKey = listEntry;
    }
    *aRetVal = true;
    return NS_OK;
  }

#ifdef XP_MACOSX
  rv = CheckForPreferredCertificate(aHostName, aCertDBKey);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (!aCertDBKey.IsEmpty()) {
    *aRetVal = true;
    return NS_OK;
  }
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsClientAuthRememberService::HasRememberedDecisionScriptable(
    const nsACString& aHostName, JS::Handle<JS::Value> aOriginAttributes,
    nsACString& aCertDBKey, JSContext* aCx, bool* aRetVal) {
  OriginAttributes attrs;
  if (!aOriginAttributes.isObject() || !attrs.Init(aCx, aOriginAttributes)) {
    return NS_ERROR_INVALID_ARG;
  }
  return HasRememberedDecision(aHostName, attrs, aCertDBKey, aRetVal);
}

nsresult nsClientAuthRememberService::AddEntryToList(
    const nsACString& aHostName, const OriginAttributes& aOriginAttributes,
    const nsACString& aDBKey) {
  nsAutoCString entryKey;
  RefPtr<nsClientAuthRemember> entry(
      new nsClientAuthRemember(aHostName, aOriginAttributes));
  nsresult rv = entry->GetEntryKey(entryKey);
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsIDataStorage::DataType storageType = GetDataStorageType(aOriginAttributes);

  nsCString tmpDbKey(aDBKey);
  rv = mClientAuthRememberList->Put(entryKey, tmpDbKey, storageType);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_OK;
}

bool nsClientAuthRememberService::IsPrivateBrowsingKey(
    const nsCString& entryKey) {
  const int32_t separator = entryKey.Find(":");
  nsCString suffix;
  if (separator >= 0) {
    entryKey.Left(suffix, separator);
  } else {
    suffix = entryKey;
  }
  return OriginAttributes::IsPrivateBrowsing(suffix);
}

nsIDataStorage::DataType nsClientAuthRememberService::GetDataStorageType(
    const OriginAttributes& aOriginAttributes) {
  if (aOriginAttributes.mPrivateBrowsingId > 0) {
    return nsIDataStorage::DataType::Private;
  }
  return nsIDataStorage::DataType::Persistent;
}
