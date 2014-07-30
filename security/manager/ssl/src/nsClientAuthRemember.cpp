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
#include "nsNSSCertHelper.h"
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
#include "SharedSSLState.h"

using namespace mozilla;
using namespace mozilla::psm;

NS_IMPL_ISUPPORTS(nsClientAuthRememberService,
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
                               const char16_t *aData)
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

void nsClientAuthRememberService::ClearAllRememberedDecisions()
{
  RefPtr<nsClientAuthRememberService> svc =
    PublicSSLState()->GetClientAuthRememberService();
  svc->ClearRememberedDecisions();

  svc = PrivateSSLState()->GetClientAuthRememberService();
  svc->ClearRememberedDecisions();
}

void
nsClientAuthRememberService::RemoveAllFromMemory()
{
  mSettingsTable.Clear();
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
      RefPtr<nsNSSCertificate> pipCert(new nsNSSCertificate(aClientCert));
      char *dbkey = nullptr;
      rv = pipCert->GetDbKey(&dbkey);
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
  hostCert.Append(':');
  hostCert.Append(fingerprint);
  
  _retval.Assign(hostCert);
}
