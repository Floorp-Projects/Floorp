/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsClientAuthRemember.h"

#include "mozilla/BasePrincipal.h"
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

using namespace mozilla;
using namespace mozilla::psm;

NS_IMPL_ISUPPORTS(nsClientAuthRememberService, nsIClientAuthRememberService,
                  nsIObserver)
NS_IMPL_ISUPPORTS(nsClientAuthRemember, nsIClientAuthRememberRecord)

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

nsClientAuthRememberService::nsClientAuthRememberService()
    : monitor("nsClientAuthRememberService.monitor") {}

nsClientAuthRememberService::~nsClientAuthRememberService() {
  RemoveAllFromMemory();
}

nsresult nsClientAuthRememberService::Init() {
  if (!NS_IsMainThread()) {
    NS_ERROR("nsClientAuthRememberService::Init called off the main thread");
    return NS_ERROR_NOT_SAME_THREAD;
  }

  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (observerService) {
    observerService->AddObserver(this, "profile-before-change", false);
    observerService->AddObserver(this, "last-pb-context-exited", false);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsClientAuthRememberService::ForgetRememberedDecision(const nsACString& key) {
  {
    ReentrantMonitorAutoEnter lock(monitor);
    mSettingsTable.RemoveEntry(PromiseFlatCString(key).get());
  }

  nsNSSComponent::ClearSSLExternalAndInternalSessionCacheNative();
  return NS_OK;
}

NS_IMETHODIMP
nsClientAuthRememberService::GetDecisions(
    nsTArray<RefPtr<nsIClientAuthRememberRecord>>& results) {
  ReentrantMonitorAutoEnter lock(monitor);
  for (auto iter = mSettingsTable.Iter(); !iter.Done(); iter.Next()) {
    if (!nsClientAuthRememberService::IsPrivateBrowsingKey(
            iter.Get()->mEntryKey)) {
      results.AppendElement(iter.Get()->mSettings);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsClientAuthRememberService::Observe(nsISupports* aSubject, const char* aTopic,
                                     const char16_t* aData) {
  // check the topic
  if (!nsCRT::strcmp(aTopic, "profile-before-change")) {
    // The profile is about to change,
    // or is going away because the application is shutting down.

    ReentrantMonitorAutoEnter lock(monitor);
    RemoveAllFromMemory();
  } else if (!nsCRT::strcmp(aTopic, "last-pb-context-exited")) {
    ReentrantMonitorAutoEnter lock(monitor);
    ClearPrivateDecisions();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsClientAuthRememberService::ClearRememberedDecisions() {
  ReentrantMonitorAutoEnter lock(monitor);
  RemoveAllFromMemory();
  return NS_OK;
}

nsresult nsClientAuthRememberService::ClearPrivateDecisions() {
  ReentrantMonitorAutoEnter lock(monitor);
  for (auto iter = mSettingsTable.Iter(); !iter.Done(); iter.Next()) {
    if (nsClientAuthRememberService::IsPrivateBrowsingKey(
            iter.Get()->mEntryKey)) {
      iter.Remove();
    }
  }
  return NS_OK;
}

void nsClientAuthRememberService::RemoveAllFromMemory() {
  mSettingsTable.Clear();
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

  {
    ReentrantMonitorAutoEnter lock(monitor);
    if (aClientCert) {
      RefPtr<nsNSSCertificate> pipCert(new nsNSSCertificate(aClientCert));
      nsAutoCString dbkey;
      rv = pipCert->GetDbKey(dbkey);
      if (NS_SUCCEEDED(rv)) {
        AddEntryToList(aHostName, aOriginAttributes, fpStr, dbkey);
      }
    } else {
      nsCString empty;
      AddEntryToList(aHostName, aOriginAttributes, fpStr, empty);
    }
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

  nsresult rv;
  nsAutoCString fpStr;
  rv = GetCertFingerprintByOidTag(aCert, SEC_OID_SHA256, fpStr);
  if (NS_FAILED(rv)) return rv;

  nsAutoCString entryKey;
  GetEntryKey(aHostName, aOriginAttributes, fpStr, entryKey);
  {
    ReentrantMonitorAutoEnter lock(monitor);
    nsClientAuthRememberEntry* entry = mSettingsTable.GetEntry(entryKey.get());
    if (!entry) return NS_OK;
    entry->mSettings->GetDbKey(aCertDBKey);
    *aRetVal = true;
  }
  return NS_OK;
}

nsresult nsClientAuthRememberService::AddEntryToList(
    const nsACString& aHostName, const OriginAttributes& aOriginAttributes,
    const nsACString& aFingerprint, const nsACString& aDBKey) {
  nsAutoCString entryKey;
  GetEntryKey(aHostName, aOriginAttributes, aFingerprint, entryKey);

  {
    ReentrantMonitorAutoEnter lock(monitor);
    nsClientAuthRememberEntry* entry = mSettingsTable.PutEntry(entryKey.get());

    if (!entry) {
      NS_ERROR("can't insert a null entry!");
      return NS_ERROR_OUT_OF_MEMORY;
    }

    entry->mEntryKey = entryKey;

    entry->mSettings =
        new nsClientAuthRemember(aHostName, aFingerprint, aDBKey, entryKey);
  }

  return NS_OK;
}

void nsClientAuthRememberService::GetEntryKey(
    const nsACString& aHostName, const OriginAttributes& aOriginAttributes,
    const nsACString& aFingerprint, nsACString& aEntryKey) {
  nsAutoCString hostCert(aHostName);
  nsAutoCString suffix;
  aOriginAttributes.CreateSuffix(suffix);
  hostCert.Append(suffix);
  hostCert.Append(':');
  hostCert.Append(aFingerprint);

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
