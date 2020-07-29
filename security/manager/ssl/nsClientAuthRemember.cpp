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
  nsresult rv = mClientAuthRememberList->Init(nullptr);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsClientAuthRememberService::ForgetRememberedDecision(const nsACString& key) {
  mClientAuthRememberList->Remove(PromiseFlatCString(key),
                                  mozilla::DataStorage_Persistent);

  nsNSSComponent::ClearSSLExternalAndInternalSessionCacheNative();
  return NS_OK;
}

NS_IMETHODIMP
nsClientAuthRememberService::GetDecisions(
    nsTArray<RefPtr<nsIClientAuthRememberRecord>>& results) {
  nsTArray<mozilla::psm::DataStorageItem> decisions;
  mClientAuthRememberList->GetAll(&decisions);

  for (const mozilla::psm::DataStorageItem& decision : decisions) {
    if (decision.type() == DataStorageType::DataStorage_Persistent) {
      RefPtr<nsIClientAuthRememberRecord> tmp =
          new nsClientAuthRemember(decision.key(), decision.value());

      results.AppendElement(tmp);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsClientAuthRememberService::ClearRememberedDecisions() {
  mClientAuthRememberList->Clear();
  nsNSSComponent::ClearSSLExternalAndInternalSessionCacheNative();
  return NS_OK;
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

  nsTArray<mozilla::psm::DataStorageItem> decisions;
  mClientAuthRememberList->GetAll(&decisions);

  for (const mozilla::psm::DataStorageItem& decision : decisions) {
    if (decision.type() == storageType) {
      RefPtr<nsIClientAuthRememberRecord> tmp =
          new nsClientAuthRemember(decision.key(), decision.value());
      nsAutoCString asciiHost;
      tmp->GetAsciiHost(asciiHost);
      if (asciiHost.Equals(aHostName)) {
        mClientAuthRememberList->Remove(decision.key(), decision.type());
      }
    }
  }
  nsNSSComponent::ClearSSLExternalAndInternalSessionCacheNative();
  return NS_OK;
}

NS_IMETHODIMP
nsClientAuthRememberService::RememberDecision(
    const nsACString& aHostName, const OriginAttributes& aOriginAttributes,
    CERTCertificate* aServerCert, CERTCertificate* aClientCert) {
  // aClientCert == nullptr means: remember that user does not want to use a
  // cert
  NS_ENSURE_ARG_POINTER(aServerCert);
  if (aHostName.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  nsAutoCString fpStr;
  nsresult rv = GetCertFingerprintByOidTag(aServerCert, SEC_OID_SHA256, fpStr);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (aClientCert) {
    RefPtr<nsNSSCertificate> pipCert(new nsNSSCertificate(aClientCert));
    nsAutoCString dbkey;
    rv = pipCert->GetDbKey(dbkey);
    if (NS_SUCCEEDED(rv)) {
      AddEntryToList(aHostName, aOriginAttributes, fpStr, dbkey);
    }
  } else {
    AddEntryToList(aHostName, aOriginAttributes, fpStr,
                   nsClientAuthRemember::SentinelValue);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsClientAuthRememberService::HasRememberedDecision(
    const nsACString& aHostName, const OriginAttributes& aOriginAttributes,
    CERTCertificate* aCert, nsACString& aCertDBKey, bool* aRetVal) {
  if (aHostName.IsEmpty()) return NS_ERROR_INVALID_ARG;

  NS_ENSURE_ARG_POINTER(aCert);
  NS_ENSURE_ARG_POINTER(aRetVal);
  *aRetVal = false;
  aCertDBKey.Truncate();

  nsresult rv;
  nsAutoCString fpStr;
  rv = GetCertFingerprintByOidTag(aCert, SEC_OID_SHA256, fpStr);
  if (NS_FAILED(rv)) return rv;

  nsAutoCString entryKey;
  GetEntryKey(aHostName, aOriginAttributes, fpStr, entryKey);
  DataStorageType storageType = GetDataStorageType(aOriginAttributes);

  nsCString listEntry = mClientAuthRememberList->Get(entryKey, storageType);
  if (listEntry.IsEmpty()) {
    return NS_OK;
  }

  if (!listEntry.Equals(nsClientAuthRemember::SentinelValue)) {
    aCertDBKey = listEntry;
  }
  *aRetVal = true;

  return NS_OK;
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
  const int32_t separator = entryKey.Find(":", false, 0, -1);
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
