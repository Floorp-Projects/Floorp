/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsClientAuthRemember.h"

#include "nsIX509Cert.h"
#include "mozilla/RefPtr.h"
#include "nsCRT.h"
#include "nsNetUtil.h"
#include "nsIObserverService.h"
#include "nsNetUtil.h"
#include "nsISupportsPrimitives.h"
#include "nsPromiseFlatString.h"
#include "nsThreadUtils.h"
#include "nsStringBuffer.h"
#include "cert.h"
#include "nspr.h"
#include "pk11pub.h"
#include "certdb.h"
#include "sechash.h"

using namespace mozilla;

NS_IMPL_THREADSAFE_ISUPPORTS2(nsClientAuthRememberService, 
                              nsIObserver,
                              nsISupportsWeakReference)

nsClientAuthRememberService::nsClientAuthRememberService()
  : monitor("nsClientAuthRememberService.monitor")
{
}

nsClientAuthRememberService::~nsClientAuthRememberService()
{
  RemoveAllFromMemory();
}

nsresult
nsClientAuthRememberService::Init()
{
  if (!NS_IsMainThread()) {
    NS_ERROR("nsClientAuthRememberService::Init called off the main thread");
    return NS_ERROR_NOT_SAME_THREAD;
  }

  mSettingsTable.Init();

  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (observerService) {
    observerService->AddObserver(this, "profile-before-change", true);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsClientAuthRememberService::Observe(nsISupports     *aSubject,
                               const char      *aTopic,
                               const PRUnichar *aData)
{
  // check the topic
  if (!nsCRT::strcmp(aTopic, "profile-before-change")) {
    // The profile is about to change,
    // or is going away because the application is shutting down.

    ReentrantMonitorAutoEnter lock(monitor);
    RemoveAllFromMemory();
  }

  return NS_OK;
}

void nsClientAuthRememberService::ClearRememberedDecisions()
{
  ReentrantMonitorAutoEnter lock(monitor);
  RemoveAllFromMemory();
}

void
nsClientAuthRememberService::RemoveAllFromMemory()
{
  mSettingsTable.Clear();
}

static nsresult
GetCertFingerprintByOidTag(CERTCertificate* nsscert,
                           SECOidTag aOidTag, 
                           nsCString &fp)
{
  unsigned int hash_len = HASH_ResultLenByOidTag(aOidTag);
  RefPtr<nsStringBuffer> fingerprint(nsStringBuffer::Alloc(hash_len));
  if (!fingerprint)
    return NS_ERROR_OUT_OF_MEMORY;

  PK11_HashBuf(aOidTag, (unsigned char*)fingerprint->Data(), 
               nsscert->derCert.data, nsscert->derCert.len);

  SECItem fpItem;
  fpItem.data = (unsigned char*)fingerprint->Data();
  fpItem.len = hash_len;

  fp.Adopt(CERT_Hexify(&fpItem, 1));
  return NS_OK;
}

nsresult
nsClientAuthRememberService::RememberDecision(const nsACString & aHostName, 
                                              CERTCertificate *aServerCert, CERTCertificate *aClientCert)
{
  // aClientCert == nullptr means: remember that user does not want to use a cert
  NS_ENSURE_ARG_POINTER(aServerCert);
  if (aHostName.IsEmpty())
    return NS_ERROR_INVALID_ARG;

  nsAutoCString fpStr;
  nsresult rv = GetCertFingerprintByOidTag(aServerCert, SEC_OID_SHA256, fpStr);
  if (NS_FAILED(rv))
    return rv;

  {
    ReentrantMonitorAutoEnter lock(monitor);
    if (aClientCert) {
      nsNSSCertificate pipCert(aClientCert);
      char *dbkey = nullptr;
      rv = pipCert.GetDbKey(&dbkey);
      if (NS_SUCCEEDED(rv) && dbkey) {
        AddEntryToList(aHostName, fpStr, 
                       nsDependentCString(dbkey));
      }
      if (dbkey) {
        PORT_Free(dbkey);
      }
    }
    else {
      nsCString empty;
      AddEntryToList(aHostName, fpStr, empty);
    }
  }

  return NS_OK;
}

nsresult
nsClientAuthRememberService::HasRememberedDecision(const nsACString & aHostName, 
                                                   CERTCertificate *aCert, 
                                                   nsACString & aCertDBKey,
                                                   bool *_retval)
{
  if (aHostName.IsEmpty())
    return NS_ERROR_INVALID_ARG;

  NS_ENSURE_ARG_POINTER(aCert);
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = false;

  nsresult rv;
  nsAutoCString fpStr;
  rv = GetCertFingerprintByOidTag(aCert, SEC_OID_SHA256, fpStr);
  if (NS_FAILED(rv))
    return rv;

  nsAutoCString hostCert;
  GetHostWithCert(aHostName, fpStr, hostCert);
  nsClientAuthRemember settings;

  {
    ReentrantMonitorAutoEnter lock(monitor);
    nsClientAuthRememberEntry *entry = mSettingsTable.GetEntry(hostCert.get());
    if (!entry)
      return NS_OK;
    settings = entry->mSettings; // copy
  }

  aCertDBKey = settings.mDBKey;
  *_retval = true;
  return NS_OK;
}

nsresult
nsClientAuthRememberService::AddEntryToList(const nsACString &aHostName, 
                                      const nsACString &fingerprint,
                                      const nsACString &db_key)

{
  nsAutoCString hostCert;
  GetHostWithCert(aHostName, fingerprint, hostCert);

  {
    ReentrantMonitorAutoEnter lock(monitor);
    nsClientAuthRememberEntry *entry = mSettingsTable.PutEntry(hostCert.get());

    if (!entry) {
      NS_ERROR("can't insert a null entry!");
      return NS_ERROR_OUT_OF_MEMORY;
    }

    entry->mHostWithCert = hostCert;

    nsClientAuthRemember &settings = entry->mSettings;
    settings.mAsciiHost = aHostName;
    settings.mFingerprint = fingerprint;
    settings.mDBKey = db_key;
  }

  return NS_OK;
}

void
nsClientAuthRememberService::GetHostWithCert(const nsACString & aHostName, 
                                             const nsACString & fingerprint, 
                                             nsACString& _retval)
{
  nsAutoCString hostCert(aHostName);
  hostCert.AppendLiteral(":");
  hostCert.Append(fingerprint);
  
  _retval.Assign(hostCert);
}
