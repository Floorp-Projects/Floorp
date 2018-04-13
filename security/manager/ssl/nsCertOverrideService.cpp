/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCertOverrideService.h"

#include "NSSCertDBTrustDomain.h"
#include "ScopedNSSTypes.h"
#include "SharedSSLState.h"
#include "mozilla/Assertions.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Unused.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsCRT.h"
#include "nsILineInputStream.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIOutputStream.h"
#include "nsISafeOutputStream.h"
#include "nsIX509Cert.h"
#include "nsNSSCertHelper.h"
#include "nsNSSCertificate.h"
#include "nsNSSComponent.h"
#include "nsNetUtil.h"
#include "nsStreamUtils.h"
#include "nsStringBuffer.h"
#include "nsThreadUtils.h"
#include "ssl.h" // For SSL_ClearSessionCache

using namespace mozilla;
using namespace mozilla::psm;

#define CERT_OVERRIDE_FILE_NAME "cert_override.txt"

void
nsCertOverride::convertBitsToString(OverrideBits ob, /*out*/ nsACString& str)
{
  str.Truncate();

  if (ob & OverrideBits::Mismatch) {
    str.Append('M');
  }

  if (ob & OverrideBits::Untrusted) {
    str.Append('U');
  }

  if (ob & OverrideBits::Time) {
    str.Append('T');
  }
}

void
nsCertOverride::convertStringToBits(const nsACString& str,
                            /*out*/ OverrideBits& ob)
{
  ob = OverrideBits::None;

  for (uint32_t i = 0; i < str.Length(); i++) {
    switch (str.CharAt(i)) {
      case 'm':
      case 'M':
        ob |= OverrideBits::Mismatch;
        break;

      case 'u':
      case 'U':
        ob |= OverrideBits::Untrusted;
        break;

      case 't':
      case 'T':
        ob |= OverrideBits::Time;
        break;

      default:
        break;
    }
  }
}

NS_IMPL_ISUPPORTS(nsCertOverrideService,
                  nsICertOverrideService,
                  nsIObserver,
                  nsISupportsWeakReference)

nsCertOverrideService::nsCertOverrideService()
  : mMutex("nsCertOverrideService.mutex")
{
}

nsCertOverrideService::~nsCertOverrideService()
{
}

nsresult
nsCertOverrideService::Init()
{
  if (!NS_IsMainThread()) {
    MOZ_ASSERT_UNREACHABLE("nsCertOverrideService initialized off main thread");
    return NS_ERROR_NOT_SAME_THREAD;
  }

  // Note that the names of these variables would seem to indicate that at one
  // point another hash algorithm was used and is still supported for backwards
  // compatibility. This is not the case. It has always been SHA256.
  mOidTagForStoringNewHashes = SEC_OID_SHA256;
  mDottedOidForStoringNewHashes.AssignLiteral("OID.2.16.840.1.101.3.4.2.1");

  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();

  // If we cannot add ourselves as a profile change observer, then we will not
  // attempt to read/write any settings file. Otherwise, we would end up
  // reading/writing the wrong settings file after a profile change.
  if (observerService) {
    observerService->AddObserver(this, "profile-before-change", true);
    observerService->AddObserver(this, "profile-do-change", true);
    // simulate a profile change so we read the current profile's settings file
    Observe(nullptr, "profile-do-change", nullptr);
  }

  SharedSSLState::NoteCertOverrideServiceInstantiated();
  return NS_OK;
}

NS_IMETHODIMP
nsCertOverrideService::Observe(nsISupports     *,
                               const char      *aTopic,
                               const char16_t *aData)
{
  // check the topic
  if (!nsCRT::strcmp(aTopic, "profile-before-change")) {
    // The profile is about to change,
    // or is going away because the application is shutting down.

    RemoveAllFromMemory();
  } else if (!nsCRT::strcmp(aTopic, "profile-do-change")) {
    // The profile has already changed.
    // Now read from the new profile location.
    // we also need to update the cached file location

    MutexAutoLock lock(mMutex);

    nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(mSettingsFile));
    if (NS_SUCCEEDED(rv)) {
      mSettingsFile->AppendNative(NS_LITERAL_CSTRING(CERT_OVERRIDE_FILE_NAME));
    } else {
      mSettingsFile = nullptr;
    }
    Read(lock);
    CountPermanentOverrideTelemetry(lock);
  }

  return NS_OK;
}

void
nsCertOverrideService::RemoveAllFromMemory()
{
  MutexAutoLock lock(mMutex);
  mSettingsTable.Clear();
}

void
nsCertOverrideService::RemoveAllTemporaryOverrides()
{
  MutexAutoLock lock(mMutex);
  for (auto iter = mSettingsTable.Iter(); !iter.Done(); iter.Next()) {
    nsCertOverrideEntry *entry = iter.Get();
    if (entry->mSettings.mIsTemporary) {
      entry->mSettings.mCert = nullptr;
      iter.Remove();
    }
  }
  // no need to write, as temporaries are never written to disk
}

nsresult
nsCertOverrideService::Read(const MutexAutoLock& aProofOfLock)
{
  // If we don't have a profile, then we won't try to read any settings file.
  if (!mSettingsFile)
    return NS_OK;

  nsresult rv;
  nsCOMPtr<nsIInputStream> fileInputStream;
  rv = NS_NewLocalFileInputStream(getter_AddRefs(fileInputStream), mSettingsFile);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsILineInputStream> lineInputStream = do_QueryInterface(fileInputStream, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsAutoCString buffer;
  bool isMore = true;
  int32_t hostIndex = 0, algoIndex, fingerprintIndex, overrideBitsIndex, dbKeyIndex;

  /* file format is:
   *
   * host:port \t fingerprint-algorithm \t fingerprint \t override-mask \t dbKey
   *
   *   where override-mask is a sequence of characters,
   *     M meaning hostname-Mismatch-override
   *     U meaning Untrusted-override
   *     T meaning Time-error-override (expired/not yet valid)
   *
   * if this format isn't respected we move onto the next line in the file.
   */

  while (isMore && NS_SUCCEEDED(lineInputStream->ReadLine(buffer, &isMore))) {
    if (buffer.IsEmpty() || buffer.First() == '#') {
      continue;
    }

    // this is a cheap, cheesy way of parsing a tab-delimited line into
    // string indexes, which can be lopped off into substrings. just for
    // purposes of obfuscation, it also checks that each token was found.
    // todo: use iterators?
    if ((algoIndex         = buffer.FindChar('\t', hostIndex)         + 1) == 0 ||
        (fingerprintIndex  = buffer.FindChar('\t', algoIndex)         + 1) == 0 ||
        (overrideBitsIndex = buffer.FindChar('\t', fingerprintIndex)  + 1) == 0 ||
        (dbKeyIndex        = buffer.FindChar('\t', overrideBitsIndex) + 1) == 0) {
      continue;
    }

    const nsACString& tmp = Substring(buffer, hostIndex, algoIndex - hostIndex - 1);
    const nsACString& algo_string = Substring(buffer, algoIndex, fingerprintIndex - algoIndex - 1);
    const nsACString& fingerprint = Substring(buffer, fingerprintIndex, overrideBitsIndex - fingerprintIndex - 1);
    const nsACString& bits_string = Substring(buffer, overrideBitsIndex, dbKeyIndex - overrideBitsIndex - 1);
    const nsACString& db_key = Substring(buffer, dbKeyIndex, buffer.Length() - dbKeyIndex);

    nsAutoCString host(tmp);
    nsCertOverride::OverrideBits bits;
    nsCertOverride::convertStringToBits(bits_string, bits);

    int32_t port;
    int32_t portIndex = host.RFindChar(':');
    if (portIndex == kNotFound)
      continue; // Ignore broken entries

    nsresult portParseError;
    nsAutoCString portString(Substring(host, portIndex+1));
    port = portString.ToInteger(&portParseError);
    if (NS_FAILED(portParseError))
      continue; // Ignore broken entries

    host.Truncate(portIndex);

    AddEntryToList(host, port,
                   nullptr, // don't have the cert
                   false, // not temporary
                   algo_string, fingerprint, bits, db_key, aProofOfLock);
  }

  return NS_OK;
}

nsresult
nsCertOverrideService::Write(const MutexAutoLock& aProofOfLock)
{
  // If we don't have any profile, then we won't try to write any file
  if (!mSettingsFile) {
    return NS_OK;
  }

  nsresult rv;
  nsCOMPtr<nsIOutputStream> fileOutputStream;
  rv = NS_NewSafeLocalFileOutputStream(getter_AddRefs(fileOutputStream),
                                       mSettingsFile,
                                       -1,
                                       0600);
  if (NS_FAILED(rv)) {
    NS_ERROR("failed to open cert_warn_settings.txt for writing");
    return rv;
  }

  // get a buffered output stream 4096 bytes big, to optimize writes
  nsCOMPtr<nsIOutputStream> bufferedOutputStream;
  rv = NS_NewBufferedOutputStream(getter_AddRefs(bufferedOutputStream),
                                  fileOutputStream.forget(), 4096);
  if (NS_FAILED(rv)) {
    return rv;
  }

  static const char kHeader[] =
      "# PSM Certificate Override Settings file" NS_LINEBREAK
      "# This is a generated file!  Do not edit." NS_LINEBREAK;

  /* see ::Read for file format */

  uint32_t unused;
  bufferedOutputStream->Write(kHeader, sizeof(kHeader) - 1, &unused);

  static const char kTab[] = "\t";
  for (auto iter = mSettingsTable.Iter(); !iter.Done(); iter.Next()) {
    nsCertOverrideEntry *entry = iter.Get();

    const nsCertOverride &settings = entry->mSettings;
    if (settings.mIsTemporary) {
      continue;
    }

    nsAutoCString bits_string;
    nsCertOverride::convertBitsToString(settings.mOverrideBits, bits_string);

    bufferedOutputStream->Write(entry->mHostWithPort.get(),
                                entry->mHostWithPort.Length(), &unused);
    bufferedOutputStream->Write(kTab, sizeof(kTab) - 1, &unused);
    bufferedOutputStream->Write(settings.mFingerprintAlgOID.get(),
                                settings.mFingerprintAlgOID.Length(), &unused);
    bufferedOutputStream->Write(kTab, sizeof(kTab) - 1, &unused);
    bufferedOutputStream->Write(settings.mFingerprint.get(),
                                settings.mFingerprint.Length(), &unused);
    bufferedOutputStream->Write(kTab, sizeof(kTab) - 1, &unused);
    bufferedOutputStream->Write(bits_string.get(),
                                bits_string.Length(), &unused);
    bufferedOutputStream->Write(kTab, sizeof(kTab) - 1, &unused);
    bufferedOutputStream->Write(settings.mDBKey.get(),
                                settings.mDBKey.Length(), &unused);
    bufferedOutputStream->Write(NS_LINEBREAK, NS_LINEBREAK_LEN, &unused);
  }

  // All went ok. Maybe except for problems in Write(), but the stream detects
  // that for us
  nsCOMPtr<nsISafeOutputStream> safeStream = do_QueryInterface(bufferedOutputStream);
  MOZ_ASSERT(safeStream, "Expected a safe output stream!");
  if (safeStream) {
    rv = safeStream->Finish();
    if (NS_FAILED(rv)) {
      NS_WARNING("failed to save cert warn settings file! possible dataloss");
      return rv;
    }
  }

  return NS_OK;
}

static nsresult
GetCertFingerprintByOidTag(nsIX509Cert *aCert,
                           SECOidTag aOidTag,
                           nsCString &fp)
{
  UniqueCERTCertificate nsscert(aCert->GetCert());
  if (!nsscert) {
    return NS_ERROR_FAILURE;
  }
  return GetCertFingerprintByOidTag(nsscert.get(), aOidTag, fp);
}

NS_IMETHODIMP
nsCertOverrideService::RememberValidityOverride(const nsACString& aHostName,
                                                int32_t aPort,
                                                nsIX509Cert* aCert,
                                                uint32_t aOverrideBits,
                                                bool aTemporary)
{
  NS_ENSURE_ARG_POINTER(aCert);
  if (aHostName.IsEmpty())
    return NS_ERROR_INVALID_ARG;
  if (aPort < -1)
    return NS_ERROR_INVALID_ARG;

  UniqueCERTCertificate nsscert(aCert->GetCert());
  if (!nsscert) {
    return NS_ERROR_FAILURE;
  }

  nsAutoCString nickname;
  nsresult rv = DefaultServerNicknameForCert(nsscert.get(), nickname);
  if (!aTemporary && NS_SUCCEEDED(rv)) {
    UniquePK11SlotInfo slot(PK11_GetInternalKeySlot());
    if (!slot) {
      return NS_ERROR_FAILURE;
    }

    // This can fail (for example, if we're in read-only mode). Luckily, we
    // don't even need it to succeed - we always match on the stored hash of the
    // certificate rather than the full certificate. It makes the display a bit
    // less informative (since we won't have a certificate to display), but it's
    // better than failing the entire operation.
    Unused << PK11_ImportCert(slot.get(), nsscert.get(), CK_INVALID_HANDLE,
                              nickname.get(), false);
  }

  nsAutoCString fpStr;
  rv = GetCertFingerprintByOidTag(nsscert.get(), mOidTagForStoringNewHashes,
                                  fpStr);
  if (NS_FAILED(rv))
    return rv;

  nsAutoCString dbkey;
  rv = aCert->GetDbKey(dbkey);
  if (NS_FAILED(rv)) {
    return rv;
  }

  {
    MutexAutoLock lock(mMutex);
    AddEntryToList(aHostName, aPort,
                   aTemporary ? aCert : nullptr,
                     // keep a reference to the cert for temporary overrides
                   aTemporary,
                   mDottedOidForStoringNewHashes, fpStr,
                   (nsCertOverride::OverrideBits)aOverrideBits,
                   dbkey, lock);
    if (!aTemporary) {
      Write(lock);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCertOverrideService::RememberTemporaryValidityOverrideUsingFingerprint(
  const nsACString& aHostName,
  int32_t aPort,
  const nsACString& aCertFingerprint,
  uint32_t aOverrideBits)
{
  if(aCertFingerprint.IsEmpty() || aHostName.IsEmpty() || (aPort < -1)) {
    return NS_ERROR_INVALID_ARG;
  }

  MutexAutoLock lock(mMutex);
  AddEntryToList(aHostName, aPort,
                 nullptr, // No cert to keep alive
                 true, // temporary
                 mDottedOidForStoringNewHashes,
                 aCertFingerprint,
                 (nsCertOverride::OverrideBits)aOverrideBits,
                 EmptyCString(),  // dbkey
                 lock);

  return NS_OK;
}

NS_IMETHODIMP
nsCertOverrideService::HasMatchingOverride(const nsACString & aHostName, int32_t aPort,
                                           nsIX509Cert *aCert,
                                           uint32_t *aOverrideBits,
                                           bool *aIsTemporary,
                                           bool *_retval)
{
  if (aHostName.IsEmpty())
    return NS_ERROR_INVALID_ARG;
  if (aPort < -1)
    return NS_ERROR_INVALID_ARG;

  NS_ENSURE_ARG_POINTER(aCert);
  NS_ENSURE_ARG_POINTER(aOverrideBits);
  NS_ENSURE_ARG_POINTER(aIsTemporary);
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = false;
  *aOverrideBits = static_cast<uint32_t>(nsCertOverride::OverrideBits::None);

  nsAutoCString hostPort;
  GetHostWithPort(aHostName, aPort, hostPort);
  nsCertOverride settings;

  {
    MutexAutoLock lock(mMutex);
    nsCertOverrideEntry *entry = mSettingsTable.GetEntry(hostPort.get());

    if (!entry)
      return NS_OK;

    settings = entry->mSettings; // copy
  }

  *aOverrideBits = static_cast<uint32_t>(settings.mOverrideBits);
  *aIsTemporary = settings.mIsTemporary;

  nsAutoCString fpStr;
  nsresult rv;

  // This code was originally written in a way that suggested that other hash
  // algorithms are supported for backwards compatibility. However, this was
  // always unnecessary, because only SHA256 has ever been used here.
  if (settings.mFingerprintAlgOID.Equals(mDottedOidForStoringNewHashes)) {
    rv = GetCertFingerprintByOidTag(aCert, mOidTagForStoringNewHashes, fpStr);
    if (NS_FAILED(rv)) {
      return rv;
    }
  } else {
    return NS_ERROR_UNEXPECTED;
  }

  *_retval = settings.mFingerprint.Equals(fpStr);
  return NS_OK;
}

NS_IMETHODIMP
nsCertOverrideService::GetValidityOverride(const nsACString & aHostName, int32_t aPort,
                                           nsACString & aHashAlg,
                                           nsACString & aFingerprint,
                                           uint32_t *aOverrideBits,
                                           bool *aIsTemporary,
                                           bool *_found)
{
  NS_ENSURE_ARG_POINTER(_found);
  NS_ENSURE_ARG_POINTER(aIsTemporary);
  NS_ENSURE_ARG_POINTER(aOverrideBits);
  *_found = false;
  *aOverrideBits = static_cast<uint32_t>(nsCertOverride::OverrideBits::None);

  nsAutoCString hostPort;
  GetHostWithPort(aHostName, aPort, hostPort);
  nsCertOverride settings;

  {
    MutexAutoLock lock(mMutex);
    nsCertOverrideEntry *entry = mSettingsTable.GetEntry(hostPort.get());

    if (entry) {
      *_found = true;
      settings = entry->mSettings; // copy
    }
  }

  if (*_found) {
    *aOverrideBits = static_cast<uint32_t>(settings.mOverrideBits);
    *aIsTemporary = settings.mIsTemporary;
    aFingerprint = settings.mFingerprint;
    aHashAlg = settings.mFingerprintAlgOID;
  }

  return NS_OK;
}

nsresult
nsCertOverrideService::AddEntryToList(const nsACString &aHostName, int32_t aPort,
                                      nsIX509Cert *aCert,
                                      const bool aIsTemporary,
                                      const nsACString &fingerprintAlgOID,
                                      const nsACString &fingerprint,
                                      nsCertOverride::OverrideBits ob,
                                      const nsACString &dbKey,
                                      const MutexAutoLock& aProofOfLock)
{
  nsAutoCString hostPort;
  GetHostWithPort(aHostName, aPort, hostPort);

  nsCertOverrideEntry *entry = mSettingsTable.PutEntry(hostPort.get());

  if (!entry) {
    NS_ERROR("can't insert a null entry!");
    return NS_ERROR_OUT_OF_MEMORY;
  }

  entry->mHostWithPort = hostPort;

  nsCertOverride &settings = entry->mSettings;
  settings.mAsciiHost = aHostName;
  settings.mPort = aPort;
  settings.mIsTemporary = aIsTemporary;
  settings.mFingerprintAlgOID = fingerprintAlgOID;
  settings.mFingerprint = fingerprint;
  settings.mOverrideBits = ob;
  settings.mDBKey = dbKey;
  // remove whitespace from stored dbKey for backwards compatibility
  settings.mDBKey.StripWhitespace();
  settings.mCert = aCert;

  return NS_OK;
}

NS_IMETHODIMP
nsCertOverrideService::ClearValidityOverride(const nsACString & aHostName, int32_t aPort)
{
  if (!NS_IsMainThread()) {
    return NS_ERROR_NOT_SAME_THREAD;
  }

  if (aPort == 0 && aHostName.EqualsLiteral("all:temporary-certificates")) {
    RemoveAllTemporaryOverrides();
    return NS_OK;
  }
  nsAutoCString hostPort;
  GetHostWithPort(aHostName, aPort, hostPort);
  {
    MutexAutoLock lock(mMutex);
    mSettingsTable.RemoveEntry(hostPort.get());
    Write(lock);
  }

  nsCOMPtr<nsINSSComponent> nss(do_GetService(PSM_COMPONENT_CONTRACTID));
  if (nss) {
    SSL_ClearSessionCache();
  } else {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return NS_OK;
}

void
nsCertOverrideService::CountPermanentOverrideTelemetry(const MutexAutoLock& aProofOfLock)
{
  uint32_t overrideCount = 0;
  for (auto iter = mSettingsTable.Iter(); !iter.Done(); iter.Next()) {
    if (!iter.Get()->mSettings.mIsTemporary) {
      overrideCount++;
    }
  }
  Telemetry::Accumulate(Telemetry::SSL_PERMANENT_CERT_ERROR_OVERRIDES,
                        overrideCount);
}

static bool
matchesDBKey(nsIX509Cert* cert, const nsCString& matchDbKey)
{
  nsAutoCString dbKey;
  nsresult rv = cert->GetDbKey(dbKey);
  if (NS_FAILED(rv)) {
    return false;
  }
  return dbKey.Equals(matchDbKey);
}

NS_IMETHODIMP
nsCertOverrideService::IsCertUsedForOverrides(nsIX509Cert *aCert,
                                              bool aCheckTemporaries,
                                              bool aCheckPermanents,
                                              uint32_t *_retval)
{
  NS_ENSURE_ARG(aCert);
  NS_ENSURE_ARG(_retval);

  uint32_t counter = 0;
  {
    MutexAutoLock lock(mMutex);
    for (auto iter = mSettingsTable.Iter(); !iter.Done(); iter.Next()) {
      const nsCertOverride &settings = iter.Get()->mSettings;

      if (( settings.mIsTemporary && !aCheckTemporaries) ||
          (!settings.mIsTemporary && !aCheckPermanents)) {
        continue;
      }

      if (matchesDBKey(aCert, settings.mDBKey)) {
        nsAutoCString cert_fingerprint;
        nsresult rv = NS_ERROR_UNEXPECTED;
        if (settings.mFingerprintAlgOID.Equals(mDottedOidForStoringNewHashes)) {
          rv = GetCertFingerprintByOidTag(aCert,
                 mOidTagForStoringNewHashes, cert_fingerprint);
        }
        if (NS_SUCCEEDED(rv) &&
            settings.mFingerprint.Equals(cert_fingerprint)) {
          counter++;
        }
      }
    }
  }
  *_retval = counter;
  return NS_OK;
}

nsresult
nsCertOverrideService::EnumerateCertOverrides(nsIX509Cert *aCert,
                         CertOverrideEnumerator aEnumerator,
                         void *aUserData)
{
  MutexAutoLock lock(mMutex);
  for (auto iter = mSettingsTable.Iter(); !iter.Done(); iter.Next()) {
    const nsCertOverride &settings = iter.Get()->mSettings;

    if (!aCert) {
      aEnumerator(settings, aUserData);
    } else {
      if (matchesDBKey(aCert, settings.mDBKey)) {
        nsAutoCString cert_fingerprint;
        nsresult rv = NS_ERROR_UNEXPECTED;
        if (settings.mFingerprintAlgOID.Equals(mDottedOidForStoringNewHashes)) {
          rv = GetCertFingerprintByOidTag(aCert,
                 mOidTagForStoringNewHashes, cert_fingerprint);
        }
        if (NS_SUCCEEDED(rv) &&
            settings.mFingerprint.Equals(cert_fingerprint)) {
          aEnumerator(settings, aUserData);
        }
      }
    }
  }
  return NS_OK;
}

void
nsCertOverrideService::GetHostWithPort(const nsACString & aHostName, int32_t aPort, nsACString& _retval)
{
  nsAutoCString hostPort(aHostName);
  if (aPort == -1) {
    aPort = 443;
  }
  if (!hostPort.IsEmpty()) {
    hostPort.Append(':');
    hostPort.AppendInt(aPort);
  }
  _retval.Assign(hostPort);
}
