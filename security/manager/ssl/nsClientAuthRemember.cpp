/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsClientAuthRemember.h"

#include "mozilla/BasePrincipal.h"
#include "mozilla/DataStorage.h"
#include "mozilla/RefPtr.h"
#include "nsCRT.h"
#include "nsNSSCertHelper.h"
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
nsClientAuthRemember::GetFingerprint(/*out*/ nsACString& aFingerprint) {
  aFingerprint = mFingerprint;
  return NS_OK;
}

NS_IMETHODIMP
nsClientAuthRemember::GetDbKey(/*out*/ nsACString& aDBKey) {
  aDBKey = mDBKey;
  return NS_OK;
}

NS_IMETHODIMP
nsClientAuthRemember::GetEntryKey(/*out*/ nsACString& aEntryKey) {
  aEntryKey = mEntryKey;
  return NS_OK;
}

nsresult nsClientAuthRememberService::Init() {
  if (!NS_IsMainThread()) {
    NS_ERROR("nsClientAuthRememberService::Init called off the main thread");
    return NS_ERROR_NOT_SAME_THREAD;
  }

  mClientAuthRememberList =
      mozilla::DataStorage::Get(DataStorageClass::ClientAuthRememberList);
  nsresult rv = mClientAuthRememberList->Init();

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsClientAuthRememberService::ForgetRememberedDecision(const nsACString& key) {
  mClientAuthRememberList->Remove(PromiseFlatCString(key),
                                  mozilla::DataStorage_Persistent);

  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(NS_NSSCOMPONENT_CID));
  if (!nssComponent) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  return nssComponent->ClearSSLExternalAndInternalSessionCache();
}

NS_IMETHODIMP
nsClientAuthRememberService::GetDecisions(
    nsTArray<RefPtr<nsIClientAuthRememberRecord>>& results) {
  nsTArray<DataStorageItem> decisions;
  mClientAuthRememberList->GetAll(&decisions);

  for (const DataStorageItem& decision : decisions) {
    if (decision.type == DataStorageType::DataStorage_Persistent) {
      RefPtr<nsIClientAuthRememberRecord> tmp =
          new nsClientAuthRemember(decision.key, decision.value);

      results.AppendElement(tmp);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsClientAuthRememberService::ClearRememberedDecisions() {
  mClientAuthRememberList->Clear();
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
  DataStorageType storageType = GetDataStorageType(attrs);

  nsTArray<DataStorageItem> decisions;
  mClientAuthRememberList->GetAll(&decisions);

  for (const DataStorageItem& decision : decisions) {
    if (decision.type == storageType) {
      RefPtr<nsIClientAuthRememberRecord> tmp =
          new nsClientAuthRemember(decision.key, decision.value);
      nsAutoCString asciiHost;
      tmp->GetAsciiHost(asciiHost);
      if (asciiHost.Equals(aHostName)) {
        mClientAuthRememberList->Remove(decision.key, decision.type);
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
    nsIX509Cert* aServerCert, nsIX509Cert* aClientCert, JSContext* aCx) {
  OriginAttributes attrs;
  if (!aOriginAttributes.isObject() || !attrs.Init(aCx, aOriginAttributes)) {
    return NS_ERROR_INVALID_ARG;
  }
  return RememberDecision(aHostName, attrs, aServerCert, aClientCert);
}

NS_IMETHODIMP
nsClientAuthRememberService::RememberDecision(
    const nsACString& aHostName, const OriginAttributes& aOriginAttributes,
    nsIX509Cert* aServerCert, nsIX509Cert* aClientCert) {
  // aClientCert == nullptr means: remember that user does not want to use a
  // cert
  NS_ENSURE_ARG_POINTER(aServerCert);
  if (aHostName.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  nsAutoCString fpStr;
  nsresult rv = GetCertSha256Fingerprint(aServerCert, fpStr);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (aClientCert) {
    nsAutoCString dbkey;
    rv = aClientCert->GetDbKey(dbkey);
    if (NS_SUCCEEDED(rv)) {
      AddEntryToList(aHostName, aOriginAttributes, fpStr, dbkey);
    }
  } else {
    AddEntryToList(aHostName, aOriginAttributes, fpStr,
                   nsClientAuthRemember::SentinelValue);
  }

  return NS_OK;
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

NS_IMETHODIMP
nsClientAuthRememberService::HasRememberedDecision(
    const nsACString& aHostName, const OriginAttributes& aOriginAttributes,
    nsIX509Cert* aCert, nsACString& aCertDBKey, bool* aRetVal) {
  if (aHostName.IsEmpty()) return NS_ERROR_INVALID_ARG;

  NS_ENSURE_ARG_POINTER(aCert);
  NS_ENSURE_ARG_POINTER(aRetVal);
  *aRetVal = false;
  aCertDBKey.Truncate();

  nsAutoCString fpStr;
  nsresult rv = GetCertSha256Fingerprint(aCert, fpStr);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsAutoCString entryKey;
  GetEntryKey(aHostName, aOriginAttributes, fpStr, entryKey);
  DataStorageType storageType = GetDataStorageType(aOriginAttributes);

  nsCString listEntry = mClientAuthRememberList->Get(entryKey, storageType);
  if (!listEntry.IsEmpty()) {
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
    nsIX509Cert* aCert, nsACString& aCertDBKey, JSContext* aCx, bool* aRetVal) {
  OriginAttributes attrs;
  if (!aOriginAttributes.isObject() || !attrs.Init(aCx, aOriginAttributes)) {
    return NS_ERROR_INVALID_ARG;
  }
  return HasRememberedDecision(aHostName, attrs, aCert, aCertDBKey, aRetVal);
}

nsresult nsClientAuthRememberService::AddEntryToList(
    const nsACString& aHostName, const OriginAttributes& aOriginAttributes,
    const nsACString& aFingerprint, const nsACString& aDBKey) {
  nsAutoCString entryKey;
  GetEntryKey(aHostName, aOriginAttributes, aFingerprint, entryKey);
  DataStorageType storageType = GetDataStorageType(aOriginAttributes);

  nsCString tmpDbKey(aDBKey);
  nsresult rv = mClientAuthRememberList->Put(entryKey, tmpDbKey, storageType);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_OK;
}

void nsClientAuthRememberService::GetEntryKey(
    const nsACString& aHostName, const OriginAttributes& aOriginAttributes,
    const nsACString& aFingerprint, nsACString& aEntryKey) {
  nsAutoCString hostCert(aHostName);
  hostCert.Append(',');
  hostCert.Append(aFingerprint);
  hostCert.Append(',');

  nsAutoCString suffix;
  aOriginAttributes.CreateSuffix(suffix);
  hostCert.Append(suffix);

  aEntryKey.Assign(hostCert);
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

DataStorageType nsClientAuthRememberService::GetDataStorageType(
    const OriginAttributes& aOriginAttributes) {
  if (aOriginAttributes.mPrivateBrowsingId > 0) {
    return DataStorage_Private;
  }
  return DataStorage_Persistent;
}
