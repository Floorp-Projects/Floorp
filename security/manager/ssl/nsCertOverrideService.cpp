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
#include "mozilla/ScopeExit.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TextUtils.h"
#include "mozilla/Unused.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsCRT.h"
#include "nsILineInputStream.h"
#ifdef ENABLE_WEBDRIVER
#  include "nsIMarionette.h"
#endif
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIOutputStream.h"
#ifdef ENABLE_WEBDRIVER
#  include "nsIRemoteAgent.h"
#endif
#include "nsISafeOutputStream.h"
#include "nsIX509Cert.h"
#include "nsNSSCertHelper.h"
#include "nsNSSCertificate.h"
#include "nsNSSComponent.h"
#include "nsNetUtil.h"
#include "nsStreamUtils.h"
#include "nsStringBuffer.h"
#include "nsThreadUtils.h"

using namespace mozilla;
using namespace mozilla::psm;

#define CERT_OVERRIDE_FILE_NAME "cert_override.txt"

class WriterRunnable : public Runnable {
 public:
  WriterRunnable(nsCertOverrideService* aService, nsCString& aData,
                 nsCOMPtr<nsIFile> aFile)
      : Runnable("nsCertOverrideService::WriterRunnable"),
        mCertOverrideService(aService),
        mData(aData),
        mFile(std::move(aFile)) {}

  NS_IMETHOD
  Run() override {
    mCertOverrideService->AssertOnTaskQueue();
    nsresult rv;

    auto removeShutdownBlockerOnExit =
        MakeScopeExit([certOverrideService = mCertOverrideService]() {
          NS_DispatchToMainThread(NS_NewRunnableFunction(
              "nsCertOverrideService::RemoveShutdownBlocker",
              [certOverrideService] {
                certOverrideService->RemoveShutdownBlocker();
              }));
        });

    nsCOMPtr<nsIOutputStream> outputStream;
    rv = NS_NewSafeLocalFileOutputStream(
        getter_AddRefs(outputStream), mFile,
        PR_CREATE_FILE | PR_TRUNCATE | PR_WRONLY);
    NS_ENSURE_SUCCESS(rv, rv);

    const char* ptr = mData.get();
    uint32_t remaining = mData.Length();
    uint32_t written = 0;
    while (remaining > 0) {
      rv = outputStream->Write(ptr, remaining, &written);
      NS_ENSURE_SUCCESS(rv, rv);
      remaining -= written;
      ptr += written;
    }

    nsCOMPtr<nsISafeOutputStream> safeStream = do_QueryInterface(outputStream);
    MOZ_ASSERT(safeStream);
    rv = safeStream->Finish();
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

 private:
  const RefPtr<nsCertOverrideService> mCertOverrideService;
  nsCString mData;
  const nsCOMPtr<nsIFile> mFile;
};

NS_IMPL_ISUPPORTS(nsCertOverride, nsICertOverride)

NS_IMETHODIMP
nsCertOverride::GetAsciiHost(/*out*/ nsACString& aAsciiHost) {
  aAsciiHost = mAsciiHost;
  return NS_OK;
}

NS_IMETHODIMP
nsCertOverride::GetDbKey(/*out*/ nsACString& aDBKey) {
  aDBKey = mDBKey;
  return NS_OK;
}

NS_IMETHODIMP
nsCertOverride::GetPort(/*out*/ int32_t* aPort) {
  *aPort = mPort;
  return NS_OK;
}

NS_IMETHODIMP
nsCertOverride::GetIsTemporary(/*out*/ bool* aIsTemporary) {
  *aIsTemporary = mIsTemporary;
  return NS_OK;
}

NS_IMETHODIMP
nsCertOverride::GetHostPort(/*out*/ nsACString& aHostPort) {
  nsCertOverrideService::GetHostWithPort(mAsciiHost, mPort, aHostPort);
  return NS_OK;
}

void nsCertOverride::convertBitsToString(OverrideBits ob,
                                         /*out*/ nsACString& str) {
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

void nsCertOverride::convertStringToBits(const nsACString& str,
                                         /*out*/ OverrideBits& ob) {
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

NS_IMPL_ISUPPORTS(nsCertOverrideService, nsICertOverrideService, nsIObserver,
                  nsISupportsWeakReference, nsIAsyncShutdownBlocker)

nsCertOverrideService::nsCertOverrideService()
    : mMutex("nsCertOverrideService.mutex"),
      mDisableAllSecurityCheck(false),
      mPendingWriteCount(0) {
  nsCOMPtr<nsIEventTarget> target =
      do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
  MOZ_ASSERT(target);

  mWriterTaskQueue = new TaskQueue(target.forget());
}

nsCertOverrideService::~nsCertOverrideService() = default;

static nsCOMPtr<nsIAsyncShutdownClient> GetShutdownBarrier() {
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIAsyncShutdownService> svc =
      mozilla::services::GetAsyncShutdownService();
  MOZ_RELEASE_ASSERT(svc);

  nsCOMPtr<nsIAsyncShutdownClient> barrier;
  nsresult rv = svc->GetProfileBeforeChange(getter_AddRefs(barrier));

  MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
  MOZ_RELEASE_ASSERT(barrier);
  return barrier;
}

nsresult nsCertOverrideService::Init() {
  if (!NS_IsMainThread()) {
    MOZ_ASSERT_UNREACHABLE("nsCertOverrideService initialized off main thread");
    return NS_ERROR_NOT_SAME_THREAD;
  }

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
nsCertOverrideService::Observe(nsISupports*, const char* aTopic,
                               const char16_t* aData) {
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

    nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                         getter_AddRefs(mSettingsFile));
    if (NS_SUCCEEDED(rv)) {
      mSettingsFile->AppendNative(nsLiteralCString(CERT_OVERRIDE_FILE_NAME));
    } else {
      mSettingsFile = nullptr;
    }
    Read(lock);
    CountPermanentOverrideTelemetry(lock);
  }

  return NS_OK;
}

void nsCertOverrideService::RemoveAllFromMemory() {
  MutexAutoLock lock(mMutex);
  mSettingsTable.Clear();
}

void nsCertOverrideService::RemoveAllTemporaryOverrides() {
  MutexAutoLock lock(mMutex);
  for (auto iter = mSettingsTable.Iter(); !iter.Done(); iter.Next()) {
    nsCertOverrideEntry* entry = iter.Get();
    if (entry->mSettings->mIsTemporary) {
      entry->mSettings->mCert = nullptr;
      iter.Remove();
    }
  }
  // no need to write, as temporaries are never written to disk
}

nsresult nsCertOverrideService::Read(const MutexAutoLock& aProofOfLock) {
  // If we don't have a profile, then we won't try to read any settings file.
  if (!mSettingsFile) return NS_OK;

  nsresult rv;
  nsCOMPtr<nsIInputStream> fileInputStream;
  rv = NS_NewLocalFileInputStream(getter_AddRefs(fileInputStream),
                                  mSettingsFile);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsILineInputStream> lineInputStream =
      do_QueryInterface(fileInputStream, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsAutoCString buffer;
  bool isMore = true;
  int32_t hostIndex = 0, algoIndex, fingerprintIndex, overrideBitsIndex,
          dbKeyIndex;

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
    if ((algoIndex = buffer.FindChar('\t', hostIndex) + 1) == 0 ||
        (fingerprintIndex = buffer.FindChar('\t', algoIndex) + 1) == 0 ||
        (overrideBitsIndex = buffer.FindChar('\t', fingerprintIndex) + 1) ==
            0 ||
        (dbKeyIndex = buffer.FindChar('\t', overrideBitsIndex) + 1) == 0) {
      continue;
    }

    const nsACString& tmp =
        Substring(buffer, hostIndex, algoIndex - hostIndex - 1);
    // We just ignore the algorithm string.
    const nsACString& fingerprint = Substring(
        buffer, fingerprintIndex, overrideBitsIndex - fingerprintIndex - 1);
    const nsACString& bits_string = Substring(
        buffer, overrideBitsIndex, dbKeyIndex - overrideBitsIndex - 1);
    const nsACString& db_key =
        Substring(buffer, dbKeyIndex, buffer.Length() - dbKeyIndex);

    nsAutoCString host(tmp);
    nsCertOverride::OverrideBits bits;
    nsCertOverride::convertStringToBits(bits_string, bits);

    int32_t port;
    int32_t portIndex = host.RFindChar(':');
    if (portIndex == kNotFound) continue;  // Ignore broken entries

    nsresult portParseError;
    nsAutoCString portString(Substring(host, portIndex + 1));
    port = portString.ToInteger(&portParseError);
    if (NS_FAILED(portParseError)) continue;  // Ignore broken entries

    host.Truncate(portIndex);

    AddEntryToList(host, port,
                   nullptr,  // don't have the cert
                   false,    // not temporary
                   fingerprint, bits, db_key, aProofOfLock);
  }

  return NS_OK;
}

static const char sSHA256OIDString[] = "OID.2.16.840.1.101.3.4.2.1";
nsresult nsCertOverrideService::Write(const MutexAutoLock& aProofOfLock) {
  MOZ_ASSERT(NS_IsMainThread());
  if (!NS_IsMainThread()) {
    return NS_ERROR_NOT_SAME_THREAD;
  }

  // If we don't have any profile, then we won't try to write any file
  if (!mSettingsFile) {
    return NS_OK;
  }

  nsCString output;

  static const char kHeader[] =
      "# PSM Certificate Override Settings file" NS_LINEBREAK
      "# This is a generated file!  Do not edit." NS_LINEBREAK;

  /* see ::Read for file format */

  output.Append(kHeader);

  static const char kTab[] = "\t";
  for (auto iter = mSettingsTable.Iter(); !iter.Done(); iter.Next()) {
    nsCertOverrideEntry* entry = iter.Get();

    RefPtr<nsCertOverride> settings = entry->mSettings;
    if (settings->mIsTemporary) {
      continue;
    }

    nsAutoCString bits_string;
    nsCertOverride::convertBitsToString(settings->mOverrideBits, bits_string);

    output.Append(entry->mHostWithPort);
    output.Append(kTab);
    output.Append(sSHA256OIDString);
    output.Append(kTab);
    output.Append(settings->mFingerprint);
    output.Append(kTab);
    output.Append(bits_string);
    output.Append(kTab);
    output.Append(settings->mDBKey);
    output.Append(NS_LINEBREAK);
  }

  // Make a clone of the file to pass to the WriterRunnable.
  nsCOMPtr<nsIFile> file;
  nsresult rv;
  rv = mSettingsFile->Clone(getter_AddRefs(file));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIRunnable> runnable = new WriterRunnable(this, output, file);
  rv = mWriterTaskQueue->Dispatch(runnable.forget());
  if (NS_FAILED(rv)) {
    return rv;
  }
  mPendingWriteCount++;

  if (mPendingWriteCount == 1) {
    rv = GetShutdownBarrier()->AddBlocker(
        this, NS_LITERAL_STRING_FROM_CSTRING(__FILE__), __LINE__,
        u"nsCertOverrideService writing data"_ns);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

static nsresult GetCertSha256Fingerprint(nsIX509Cert* aCert,
                                         nsCString& aResult) {
  nsAutoString fpStrUTF16;
  nsresult rv = aCert->GetSha256Fingerprint(fpStrUTF16);
  if (NS_FAILED(rv)) {
    return rv;
  }
  aResult.Assign(NS_ConvertUTF16toUTF8(fpStrUTF16));
  return NS_OK;
}

NS_IMETHODIMP
nsCertOverrideService::RememberValidityOverride(const nsACString& aHostName,
                                                int32_t aPort,
                                                nsIX509Cert* aCert,
                                                uint32_t aOverrideBits,
                                                bool aTemporary) {
  NS_ENSURE_ARG_POINTER(aCert);
  if (aHostName.IsEmpty() || !IsAscii(aHostName)) {
    return NS_ERROR_INVALID_ARG;
  }
  if (aPort < -1) {
    return NS_ERROR_INVALID_ARG;
  }
  if (!NS_IsMainThread()) {
    return NS_ERROR_NOT_SAME_THREAD;
  }

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
  rv = GetCertSha256Fingerprint(aCert, fpStr);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsAutoCString dbkey;
  rv = aCert->GetDbKey(dbkey);
  if (NS_FAILED(rv)) {
    return rv;
  }

  {
    MutexAutoLock lock(mMutex);
    AddEntryToList(aHostName, aPort, aTemporary ? aCert : nullptr,
                   // keep a reference to the cert for temporary overrides
                   aTemporary, fpStr,
                   (nsCertOverride::OverrideBits)aOverrideBits, dbkey, lock);
    if (!aTemporary) {
      Write(lock);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCertOverrideService::RememberTemporaryValidityOverrideUsingFingerprint(
    const nsACString& aHostName, int32_t aPort,
    const nsACString& aCertFingerprint, uint32_t aOverrideBits) {
  if (aCertFingerprint.IsEmpty() || aHostName.IsEmpty() ||
      !IsAscii(aCertFingerprint) || !IsAscii(aHostName) || (aPort < -1)) {
    return NS_ERROR_INVALID_ARG;
  }

  MutexAutoLock lock(mMutex);
  AddEntryToList(aHostName, aPort,
                 nullptr,  // No cert to keep alive
                 true,     // temporary
                 aCertFingerprint, (nsCertOverride::OverrideBits)aOverrideBits,
                 ""_ns,  // dbkey
                 lock);

  return NS_OK;
}

NS_IMETHODIMP
nsCertOverrideService::HasMatchingOverride(const nsACString& aHostName,
                                           int32_t aPort, nsIX509Cert* aCert,
                                           uint32_t* aOverrideBits,
                                           bool* aIsTemporary, bool* _retval) {
  bool disableAllSecurityCheck = false;
  {
    MutexAutoLock lock(mMutex);
    disableAllSecurityCheck = mDisableAllSecurityCheck;
  }
  if (disableAllSecurityCheck) {
    nsCertOverride::OverrideBits all = nsCertOverride::OverrideBits::Untrusted |
                                       nsCertOverride::OverrideBits::Mismatch |
                                       nsCertOverride::OverrideBits::Time;
    *aOverrideBits = static_cast<uint32_t>(all);
    *aIsTemporary = false;
    *_retval = true;
    return NS_OK;
  }

  if (aHostName.IsEmpty() || !IsAscii(aHostName)) {
    return NS_ERROR_INVALID_ARG;
  }
  if (aPort < -1) return NS_ERROR_INVALID_ARG;

  NS_ENSURE_ARG_POINTER(aCert);
  NS_ENSURE_ARG_POINTER(aOverrideBits);
  NS_ENSURE_ARG_POINTER(aIsTemporary);
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = false;
  *aOverrideBits = static_cast<uint32_t>(nsCertOverride::OverrideBits::None);

  nsAutoCString hostPort;
  GetHostWithPort(aHostName, aPort, hostPort);
  RefPtr<nsCertOverride> settings;

  {
    MutexAutoLock lock(mMutex);
    nsCertOverrideEntry* entry = mSettingsTable.GetEntry(hostPort.get());

    if (!entry) return NS_OK;

    settings = entry->mSettings;
  }

  *aOverrideBits = static_cast<uint32_t>(settings->mOverrideBits);
  *aIsTemporary = settings->mIsTemporary;

  nsAutoCString fpStr;
  nsresult rv = GetCertSha256Fingerprint(aCert, fpStr);
  if (NS_FAILED(rv)) {
    return rv;
  }

  *_retval = settings->mFingerprint.Equals(fpStr);
  return NS_OK;
}

nsresult nsCertOverrideService::AddEntryToList(
    const nsACString& aHostName, int32_t aPort, nsIX509Cert* aCert,
    const bool aIsTemporary, const nsACString& fingerprint,
    nsCertOverride::OverrideBits ob, const nsACString& dbKey,
    const MutexAutoLock& aProofOfLock) {
  nsAutoCString hostPort;
  GetHostWithPort(aHostName, aPort, hostPort);

  nsCertOverrideEntry* entry = mSettingsTable.PutEntry(hostPort.get());

  if (!entry) {
    NS_ERROR("can't insert a null entry!");
    return NS_ERROR_OUT_OF_MEMORY;
  }

  entry->mHostWithPort = hostPort;

  RefPtr<nsCertOverride> settings(new nsCertOverride());

  settings->mAsciiHost = aHostName;
  settings->mPort = aPort;
  settings->mIsTemporary = aIsTemporary;
  settings->mFingerprint = fingerprint;
  settings->mOverrideBits = ob;
  settings->mDBKey = dbKey;
  // remove whitespace from stored dbKey for backwards compatibility
  settings->mDBKey.StripWhitespace();
  settings->mCert = aCert;
  entry->mSettings = settings;

  return NS_OK;
}

NS_IMETHODIMP
nsCertOverrideService::ClearValidityOverride(const nsACString& aHostName,
                                             int32_t aPort) {
  if (aHostName.IsEmpty() || !IsAscii(aHostName)) {
    return NS_ERROR_INVALID_ARG;
  }
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
    nss->ClearSSLExternalAndInternalSessionCache();
  } else {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCertOverrideService::ClearAllOverrides() {
  if (!NS_IsMainThread()) {
    return NS_ERROR_NOT_SAME_THREAD;
  }

  {
    MutexAutoLock lock(mMutex);
    mSettingsTable.Clear();
    Write(lock);
  }

  nsCOMPtr<nsINSSComponent> nss(do_GetService(PSM_COMPONENT_CONTRACTID));
  if (nss) {
    nss->ClearSSLExternalAndInternalSessionCache();
  } else {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return NS_OK;
}

void nsCertOverrideService::CountPermanentOverrideTelemetry(
    const MutexAutoLock& aProofOfLock) {
  uint32_t overrideCount = 0;
  for (auto iter = mSettingsTable.Iter(); !iter.Done(); iter.Next()) {
    if (!iter.Get()->mSettings->mIsTemporary) {
      overrideCount++;
    }
  }
  Telemetry::Accumulate(Telemetry::SSL_PERMANENT_CERT_ERROR_OVERRIDES,
                        overrideCount);
}

static bool matchesDBKey(nsIX509Cert* cert, const nsCString& matchDbKey) {
  nsAutoCString dbKey;
  nsresult rv = cert->GetDbKey(dbKey);
  if (NS_FAILED(rv)) {
    return false;
  }
  return dbKey.Equals(matchDbKey);
}

NS_IMETHODIMP
nsCertOverrideService::IsCertUsedForOverrides(nsIX509Cert* aCert,
                                              bool aCheckTemporaries,
                                              bool aCheckPermanents,
                                              uint32_t* _retval) {
  NS_ENSURE_ARG(aCert);
  NS_ENSURE_ARG(_retval);

  uint32_t counter = 0;
  {
    MutexAutoLock lock(mMutex);
    for (auto iter = mSettingsTable.Iter(); !iter.Done(); iter.Next()) {
      RefPtr<nsCertOverride> settings = iter.Get()->mSettings;

      if ((settings->mIsTemporary && !aCheckTemporaries) ||
          (!settings->mIsTemporary && !aCheckPermanents)) {
        continue;
      }

      if (matchesDBKey(aCert, settings->mDBKey)) {
        nsAutoCString certFingerprint;
        nsresult rv = GetCertSha256Fingerprint(aCert, certFingerprint);
        if (NS_SUCCEEDED(rv) &&
            settings->mFingerprint.Equals(certFingerprint)) {
          counter++;
        }
      }
    }
  }
  *_retval = counter;
  return NS_OK;
}

static bool IsDebugger() {
#ifdef ENABLE_WEBDRIVER
  nsCOMPtr<nsIMarionette> marionette = do_GetService(NS_MARIONETTE_CONTRACTID);
  if (marionette) {
    bool marionetteRunning = false;
    marionette->GetRunning(&marionetteRunning);
    if (marionetteRunning) {
      return true;
    }
  }

  nsCOMPtr<nsIRemoteAgent> agent = do_GetService(NS_REMOTEAGENT_CONTRACTID);
  if (agent) {
    bool remoteAgentListening = false;
    agent->GetListening(&remoteAgentListening);
    if (remoteAgentListening) {
      return true;
    }
  }
#endif

  return false;
}

NS_IMETHODIMP
nsCertOverrideService::
    SetDisableAllSecurityChecksAndLetAttackersInterceptMyData(bool aDisable) {
  if (!(PR_GetEnv("XPCSHELL_TEST_PROFILE_DIR") || IsDebugger())) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  MutexAutoLock lock(mMutex);
  mDisableAllSecurityCheck = aDisable;
  return NS_OK;
}

NS_IMETHODIMP
nsCertOverrideService::GetOverrides(
    /*out*/ nsTArray<RefPtr<nsICertOverride>>& retval) {
  MutexAutoLock lock(mMutex);
  for (auto iter = mSettingsTable.Iter(); !iter.Done(); iter.Next()) {
    const RefPtr<nsICertOverride> settings = iter.Get()->mSettings;

    retval.AppendElement(settings);
  }
  return NS_OK;
}

void nsCertOverrideService::GetHostWithPort(const nsACString& aHostName,
                                            int32_t aPort,
                                            nsACString& _retval) {
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

// nsIAsyncShutdownBlocker implementation
NS_IMETHODIMP
nsCertOverrideService::GetName(nsAString& aName) {
  aName = u"nsCertOverrideService: shutdown"_ns;
  return NS_OK;
}

NS_IMETHODIMP
nsCertOverrideService::GetState(nsIPropertyBag** aState) {
  if (!aState) {
    return NS_ERROR_INVALID_ARG;
  }
  *aState = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsCertOverrideService::BlockShutdown(nsIAsyncShutdownClient*) { return NS_OK; }

void nsCertOverrideService::RemoveShutdownBlocker() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mPendingWriteCount > 0);
  mPendingWriteCount--;
  if (mPendingWriteCount == 0) {
    nsresult rv = GetShutdownBarrier()->RemoveBlocker(this);
    MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
  }
}
