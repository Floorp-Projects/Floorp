/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CertBlocklist.h"

#include "mozilla/Assertions.h"
#include "mozilla/Base64.h"
#include "mozilla/Casting.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Logging.h"
#include "mozilla/Preferences.h"
#include "mozilla/Unused.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDependentString.h"
#include "nsDirectoryServiceUtils.h"
#include "nsICryptoHash.h"
#include "nsIFileStreams.h"
#include "nsILineInputStream.h"
#include "nsISafeOutputStream.h"
#include "nsIX509Cert.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsPromiseFlatString.h"
#include "nsTHashtable.h"
#include "nsThreadUtils.h"
#include "mozpkix/Input.h"
#include "prtime.h"

NS_IMPL_ISUPPORTS(CertBlocklist, nsICertBlocklist)

using namespace mozilla;
using namespace mozilla::pkix;

#define PREF_BACKGROUND_UPDATE_TIMER \
  "app.update.lastUpdateTime.blocklist-background-update-timer"
#define PREF_BLOCKLIST_ONECRL_CHECKED \
  "services.settings.security.onecrl.checked"
#define PREF_MAX_STALENESS_IN_SECONDS \
  "security.onecrl.maximum_staleness_in_seconds"

static LazyLogModule gCertBlockPRLog("CertBlock");

uint32_t CertBlocklist::sLastBlocklistUpdate = 0U;
uint32_t CertBlocklist::sMaxStaleness = 0U;

CertBlocklistItem::CertBlocklistItem(const uint8_t* DNData, size_t DNLength,
                                     const uint8_t* otherData,
                                     size_t otherLength,
                                     CertBlocklistItemMechanism itemMechanism)
    : mIsCurrent(false), mItemMechanism(itemMechanism) {
  mDNData = new uint8_t[DNLength];
  memcpy(mDNData, DNData, DNLength);
  mDNLength = DNLength;

  mOtherData = new uint8_t[otherLength];
  memcpy(mOtherData, otherData, otherLength);
  mOtherLength = otherLength;
}

CertBlocklistItem::CertBlocklistItem(const CertBlocklistItem& aItem) {
  mDNLength = aItem.mDNLength;
  mDNData = new uint8_t[mDNLength];
  memcpy(mDNData, aItem.mDNData, mDNLength);

  mOtherLength = aItem.mOtherLength;
  mOtherData = new uint8_t[mOtherLength];
  memcpy(mOtherData, aItem.mOtherData, mOtherLength);

  mItemMechanism = aItem.mItemMechanism;

  mIsCurrent = aItem.mIsCurrent;
}

CertBlocklistItem::~CertBlocklistItem() {
  delete[] mDNData;
  delete[] mOtherData;
}

nsresult CertBlocklistItem::ToBase64(nsACString& b64DNOut,
                                     nsACString& b64OtherOut) {
  nsDependentCSubstring DNString(BitwiseCast<char*, uint8_t*>(mDNData),
                                 mDNLength);
  nsDependentCSubstring otherString(BitwiseCast<char*, uint8_t*>(mOtherData),
                                    mOtherLength);
  nsresult rv = Base64Encode(DNString, b64DNOut);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Base64Encode(otherString, b64OtherOut);
  return rv;
}

bool CertBlocklistItem::operator==(const CertBlocklistItem& aItem) const {
  if (aItem.mItemMechanism != mItemMechanism) {
    return false;
  }
  if (aItem.mDNLength != mDNLength || aItem.mOtherLength != mOtherLength) {
    return false;
  }
  return memcmp(aItem.mDNData, mDNData, mDNLength) == 0 &&
         memcmp(aItem.mOtherData, mOtherData, mOtherLength) == 0;
}

uint32_t CertBlocklistItem::Hash() const {
  uint32_t hash;
  // there's no requirement for a serial to be as large as the size of the hash
  // key; if it's smaller, fall back to the first octet (otherwise, the last
  // four)
  if (mItemMechanism == BlockByIssuerAndSerial &&
      mOtherLength >= sizeof(hash)) {
    memcpy(&hash, mOtherData + mOtherLength - sizeof(hash), sizeof(hash));
  } else {
    hash = *mOtherData;
  }
  return hash;
}

CertBlocklist::CertBlocklist()
    : mMutex("CertBlocklist::mMutex"),
      mModified(false),
      mBackingFileIsInitialized(false),
      mBackingFile(nullptr) {}

CertBlocklist::~CertBlocklist() {
  Preferences::UnregisterCallback(CertBlocklist::PreferenceChanged,
                                  PREF_MAX_STALENESS_IN_SECONDS, this);
  Preferences::UnregisterCallback(CertBlocklist::PreferenceChanged,
                                  PREF_BLOCKLIST_ONECRL_CHECKED, this);
}

nsresult CertBlocklist::Init() {
  MOZ_LOG(gCertBlockPRLog, LogLevel::Debug, ("CertBlocklist::Init"));

  // Init must be on main thread for getting the profile directory
  if (!NS_IsMainThread()) {
    MOZ_LOG(gCertBlockPRLog, LogLevel::Debug,
            ("CertBlocklist::Init - called off main thread"));
    return NS_ERROR_NOT_SAME_THREAD;
  }

  // Register preference callbacks
  nsresult rv = Preferences::RegisterCallbackAndCall(
      CertBlocklist::PreferenceChanged, PREF_MAX_STALENESS_IN_SECONDS, this);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Preferences::RegisterCallbackAndCall(
      CertBlocklist::PreferenceChanged, PREF_BLOCKLIST_ONECRL_CHECKED, this);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Get the profile directory
  rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                              getter_AddRefs(mBackingFile));
  if (NS_FAILED(rv) || !mBackingFile) {
    MOZ_LOG(gCertBlockPRLog, LogLevel::Debug,
            ("CertBlocklist::Init - couldn't get profile dir"));
    // Since we're returning NS_OK here, set mBackingFile to a safe value.
    // (We need initialization to succeed and CertBlocklist to be in a
    // well-defined state if the profile directory doesn't exist.)
    mBackingFile = nullptr;
    return NS_OK;
  }
  rv = mBackingFile->Append(NS_LITERAL_STRING("revocations.txt"));
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsAutoCString path;
  rv = mBackingFile->GetPersistentDescriptor(path);
  if (NS_FAILED(rv)) {
    return rv;
  }
  MOZ_LOG(gCertBlockPRLog, LogLevel::Debug,
          ("CertBlocklist::Init certList path: %s", path.get()));

  return NS_OK;
}

nsresult CertBlocklist::EnsureBackingFileInitialized(MutexAutoLock& lock) {
  MOZ_LOG(gCertBlockPRLog, LogLevel::Debug,
          ("CertBlocklist::EnsureBackingFileInitialized"));
  if (mBackingFileIsInitialized || !mBackingFile) {
    return NS_OK;
  }

  MOZ_LOG(gCertBlockPRLog, LogLevel::Debug,
          ("CertBlocklist::EnsureBackingFileInitialized - not initialized"));

  bool exists = false;
  nsresult rv = mBackingFile->Exists(&exists);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (!exists) {
    MOZ_LOG(
        gCertBlockPRLog, LogLevel::Warning,
        ("CertBlocklist::EnsureBackingFileInitialized no revocations file"));
    return NS_OK;
  }

  // Load the revocations file into the cert blocklist
  nsCOMPtr<nsIFileInputStream> fileStream(
      do_CreateInstance(NS_LOCALFILEINPUTSTREAM_CONTRACTID, &rv));
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = fileStream->Init(mBackingFile, -1, -1, false);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsILineInputStream> lineStream(do_QueryInterface(fileStream, &rv));
  nsAutoCString line;
  nsAutoCString DN;
  nsAutoCString other;
  CertBlocklistItemMechanism mechanism;
  // read in the revocations file. The file format is as follows: each line
  // contains a comment, base64 encoded DER for a DN, base64 encoded DER for a
  // serial number or a Base64 encoded SHA256 hash of a public key. Comment
  // lines start with '#', serial number lines, ' ' (a space), public key hashes
  // with '\t' (a tab) and anything else is assumed to be a DN.
  bool more = true;
  do {
    rv = lineStream->ReadLine(line, &more);
    if (NS_FAILED(rv)) {
      break;
    }
    // ignore comments and empty lines
    if (line.IsEmpty() || line.First() == '#') {
      continue;
    }
    if (line.First() != ' ' && line.First() != '\t') {
      DN = line;
      continue;
    }
    other = line;
    if (line.First() == ' ') {
      mechanism = BlockByIssuerAndSerial;
    } else {
      mechanism = BlockBySubjectAndPubKey;
    }
    other.Trim(" \t", true, false, false);
    // Serial numbers and public key hashes 'belong' to the last DN line seen;
    // if no DN has been seen, the serial number or public key hash is ignored.
    if (DN.IsEmpty() || other.IsEmpty()) {
      continue;
    }
    MOZ_LOG(gCertBlockPRLog, LogLevel::Debug,
            ("CertBlocklist::EnsureBackingFileInitialized adding: %s %s",
             DN.get(), other.get()));

    MOZ_LOG(gCertBlockPRLog, LogLevel::Debug,
            ("CertBlocklist::EnsureBackingFileInitialized - pre-decode"));

    rv = AddRevokedCertInternal(DN, other, mechanism, CertOldFromLocalCache,
                                lock);

    if (NS_FAILED(rv)) {
      // we warn here, rather than abandoning, since we need to
      // ensure that as many items as possible are read
      MOZ_LOG(
          gCertBlockPRLog, LogLevel::Warning,
          ("CertBlocklist::EnsureBackingFileInitialized adding revoked cert "
           "failed"));
    }
  } while (more);
  mBackingFileIsInitialized = true;
  return NS_OK;
}

NS_IMETHODIMP
CertBlocklist::RevokeCertBySubjectAndPubKey(const nsACString& aSubject,
                                            const nsACString& aPubKeyHash) {
  MOZ_LOG(gCertBlockPRLog, LogLevel::Debug,
          ("CertBlocklist::RevokeCertBySubjectAndPubKey - subject is: %s and "
           "pubKeyHash: %s",
           PromiseFlatCString(aSubject).get(),
           PromiseFlatCString(aPubKeyHash).get()));
  MutexAutoLock lock(mMutex);

  return AddRevokedCertInternal(aSubject, aPubKeyHash, BlockBySubjectAndPubKey,
                                CertNewFromBlocklist, lock);
}

NS_IMETHODIMP
CertBlocklist::RevokeCertByIssuerAndSerial(const nsACString& aIssuer,
                                           const nsACString& aSerialNumber) {
  MOZ_LOG(gCertBlockPRLog, LogLevel::Debug,
          ("CertBlocklist::RevokeCertByIssuerAndSerial - issuer is: %s and "
           "serial: %s",
           PromiseFlatCString(aIssuer).get(),
           PromiseFlatCString(aSerialNumber).get()));
  MutexAutoLock lock(mMutex);

  return AddRevokedCertInternal(aIssuer, aSerialNumber, BlockByIssuerAndSerial,
                                CertNewFromBlocklist, lock);
}

nsresult CertBlocklist::AddRevokedCertInternal(
    const nsACString& aEncodedDN, const nsACString& aEncodedOther,
    CertBlocklistItemMechanism aMechanism, CertBlocklistItemState aItemState,
    MutexAutoLock& /*proofOfLock*/) {
  nsCString decodedDN;
  nsCString decodedOther;

  nsresult rv = Base64Decode(aEncodedDN, decodedDN);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Base64Decode(aEncodedOther, decodedOther);
  if (NS_FAILED(rv)) {
    return rv;
  }

  CertBlocklistItem item(
      BitwiseCast<const uint8_t*, const char*>(decodedDN.get()),
      decodedDN.Length(),
      BitwiseCast<const uint8_t*, const char*>(decodedOther.get()),
      decodedOther.Length(), aMechanism);

  if (aItemState == CertNewFromBlocklist) {
    // We want SaveEntries to be a no-op if no new entries are added.
    nsGenericHashKey<CertBlocklistItem>* entry = mBlocklist.GetEntry(item);
    if (!entry) {
      mModified = true;
    } else {
      // Ensure that any existing item is replaced by a fresh one so we can
      // use mIsCurrent to decide which entries to write out.
      mBlocklist.RemoveEntry(entry);
    }
    item.mIsCurrent = true;
  }
  mBlocklist.PutEntry(item);

  return NS_OK;
}

// Write a line for a given string in the output stream
nsresult WriteLine(nsIOutputStream* outputStream, const nsACString& string) {
  nsAutoCString line(string);
  line.Append('\n');

  const char* data = line.get();
  uint32_t length = line.Length();
  nsresult rv = NS_OK;
  while (NS_SUCCEEDED(rv) && length) {
    uint32_t bytesWritten = 0;
    rv = outputStream->Write(data, length, &bytesWritten);
    if (NS_FAILED(rv)) {
      return rv;
    }
    // if no data is written, something is wrong
    if (!bytesWritten) {
      return NS_ERROR_FAILURE;
    }
    length -= bytesWritten;
    data += bytesWritten;
  }
  return rv;
}

// void saveEntries();
// Store the blockist in a text file containing base64 encoded issuers and
// serial numbers.
//
// Each item is stored on a separate line; each issuer is followed by its
// revoked serial numbers, indented by one space.
//
// lines starting with a # character are ignored
NS_IMETHODIMP
CertBlocklist::SaveEntries() {
  MOZ_LOG(gCertBlockPRLog, LogLevel::Debug,
          ("CertBlocklist::SaveEntries - not initialized"));
  MutexAutoLock lock(mMutex);
  if (!mModified) {
    return NS_OK;
  }

  nsresult rv = EnsureBackingFileInitialized(lock);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (!mBackingFile) {
    // We allow this to succeed with no profile directory for tests
    MOZ_LOG(gCertBlockPRLog, LogLevel::Warning,
            ("CertBlocklist::SaveEntries no file in profile to write to"));
    return NS_OK;
  }

  // Data needed for writing blocklist items out to the revocations file
  IssuerTable issuerTable;
  BlocklistStringSet issuers;
  nsCOMPtr<nsIOutputStream> outputStream;

  rv = NS_NewAtomicFileOutputStream(getter_AddRefs(outputStream), mBackingFile,
                                    -1, -1, 0);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = WriteLine(outputStream,
                 NS_LITERAL_CSTRING("# Auto generated contents. Do not edit."));
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Sort blocklist items into lists of serials for each issuer
  for (auto iter = mBlocklist.Iter(); !iter.Done(); iter.Next()) {
    CertBlocklistItem item = iter.Get()->GetKey();
    if (!item.mIsCurrent) {
      continue;
    }

    nsAutoCString encDN;
    nsAutoCString encOther;

    nsresult rv = item.ToBase64(encDN, encOther);
    if (NS_FAILED(rv)) {
      MOZ_LOG(gCertBlockPRLog, LogLevel::Warning,
              ("CertBlocklist::SaveEntries writing revocation data failed"));
      return NS_ERROR_FAILURE;
    }

    // If it's a subject / public key block, write it straight out
    if (item.mItemMechanism == BlockBySubjectAndPubKey) {
      WriteLine(outputStream, encDN);
      WriteLine(outputStream, NS_LITERAL_CSTRING("\t") + encOther);
      continue;
    }

    // Otherwise, we have to group entries by issuer
    issuers.PutEntry(encDN);
    BlocklistStringSet* issuerSet = issuerTable.Get(encDN);
    if (!issuerSet) {
      issuerSet = new BlocklistStringSet();
      issuerTable.Put(encDN, issuerSet);
    }
    issuerSet->PutEntry(encOther);
  }

  for (auto iter = issuers.Iter(); !iter.Done(); iter.Next()) {
    nsCStringHashKey* hashKey = iter.Get();
    nsAutoPtr<BlocklistStringSet> issuerSet;
    issuerTable.Remove(hashKey->GetKey(), &issuerSet);

    nsresult rv = WriteLine(outputStream, hashKey->GetKey());
    if (NS_FAILED(rv)) {
      break;
    }

    // Write serial data to the output stream
    for (auto iter = issuerSet->Iter(); !iter.Done(); iter.Next()) {
      nsresult rv = WriteLine(outputStream,
                              NS_LITERAL_CSTRING(" ") + iter.Get()->GetKey());
      if (NS_FAILED(rv)) {
        MOZ_LOG(gCertBlockPRLog, LogLevel::Warning,
                ("CertBlocklist::SaveEntries writing revocation data failed"));
        return NS_ERROR_FAILURE;
      }
    }
  }

  nsCOMPtr<nsISafeOutputStream> safeStream = do_QueryInterface(outputStream);
  MOZ_ASSERT(safeStream, "expected a safe output stream!");
  if (!safeStream) {
    return NS_ERROR_FAILURE;
  }
  rv = safeStream->Finish();
  if (NS_FAILED(rv)) {
    MOZ_LOG(gCertBlockPRLog, LogLevel::Warning,
            ("CertBlocklist::SaveEntries saving revocation data failed"));
    return rv;
  }
  mModified = false;
  return NS_OK;
}

NS_IMETHODIMP
CertBlocklist::IsCertRevoked(const nsACString& aIssuerString,
                             const nsACString& aSerialNumberString,
                             const nsACString& aSubjectString,
                             const nsACString& aPubKeyString, bool* _retval) {
  MutexAutoLock lock(mMutex);
  MOZ_LOG(gCertBlockPRLog, LogLevel::Warning, ("CertBlocklist::IsCertRevoked"));

  nsCString decodedIssuer;
  nsCString decodedSerial;
  nsCString decodedSubject;
  nsCString decodedPubKey;

  nsresult rv = Base64Decode(aIssuerString, decodedIssuer);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Base64Decode(aSerialNumberString, decodedSerial);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Base64Decode(aSubjectString, decodedSubject);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Base64Decode(aPubKeyString, decodedPubKey);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = EnsureBackingFileInitialized(lock);
  if (NS_FAILED(rv)) {
    return rv;
  }

  CertBlocklistItem issuerSerial(
      BitwiseCast<const uint8_t*, const char*>(decodedIssuer.get()),
      decodedIssuer.Length(),
      BitwiseCast<const uint8_t*, const char*>(decodedSerial.get()),
      decodedSerial.Length(), BlockByIssuerAndSerial);

  MOZ_LOG(gCertBlockPRLog, LogLevel::Warning,
          ("CertBlocklist::IsCertRevoked issuer %s - serial %s",
           PromiseFlatCString(aIssuerString).get(),
           PromiseFlatCString(aSerialNumberString).get()));

  *_retval = mBlocklist.Contains(issuerSerial);

  if (*_retval) {
    MOZ_LOG(gCertBlockPRLog, LogLevel::Warning,
            ("certblocklist::IsCertRevoked found by issuer / serial"));
    return NS_OK;
  }

  nsCOMPtr<nsICryptoHash> crypto;
  crypto = do_CreateInstance(NS_CRYPTO_HASH_CONTRACTID, &rv);

  rv = crypto->Init(nsICryptoHash::SHA256);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = crypto->Update(
      BitwiseCast<const uint8_t*, const char*>(decodedPubKey.get()),
      decodedPubKey.Length());
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCString hashString;
  rv = crypto->Finish(false, hashString);
  if (NS_FAILED(rv)) {
    return rv;
  }

  CertBlocklistItem subjectPubKey(
      BitwiseCast<const uint8_t*, const char*>(decodedSubject.get()),
      decodedSubject.Length(),
      BitwiseCast<const uint8_t*, const char*>(hashString.get()),
      hashString.Length(), BlockBySubjectAndPubKey);

  nsCString encodedHash;
  rv = Base64Encode(hashString, encodedHash);
  if (NS_FAILED(rv)) {
    return rv;
  }

  MOZ_LOG(
      gCertBlockPRLog, LogLevel::Warning,
      ("CertBlocklist::IsCertRevoked subject %s - pubKeyHash %s (pubKey %s)",
       PromiseFlatCString(aSubjectString).get(),
       PromiseFlatCString(encodedHash).get(),
       PromiseFlatCString(aPubKeyString).get()));
  *_retval = mBlocklist.Contains(subjectPubKey);

  MOZ_LOG(gCertBlockPRLog, LogLevel::Warning,
          ("CertBlocklist::IsCertRevoked by subject / pubkey? %s",
           *_retval ? "true" : "false"));

  return NS_OK;
}

NS_IMETHODIMP
CertBlocklist::IsBlocklistFresh(bool* _retval) {
  MutexAutoLock lock(mMutex);
  *_retval = false;

  uint32_t now = uint32_t(PR_Now() / PR_USEC_PER_SEC);
  MOZ_LOG(gCertBlockPRLog, LogLevel::Warning,
          ("CertBlocklist::IsBlocklistFresh ? lastUpdate is %i",
           sLastBlocklistUpdate));

  if (now > sLastBlocklistUpdate) {
    int64_t interval = now - sLastBlocklistUpdate;
    MOZ_LOG(
        gCertBlockPRLog, LogLevel::Warning,
        ("CertBlocklist::IsBlocklistFresh we're after the last BlocklistUpdate "
         "interval is %" PRId64 ", staleness %u",
         interval, sMaxStaleness));
    *_retval = sMaxStaleness > interval;
  }
  MOZ_LOG(
      gCertBlockPRLog, LogLevel::Warning,
      ("CertBlocklist::IsBlocklistFresh ? %s", *_retval ? "true" : "false"));
  return NS_OK;
}

/* static */
void CertBlocklist::PreferenceChanged(const char* aPref,
                                      CertBlocklist* aBlocklist)

{
  MutexAutoLock lock(aBlocklist->mMutex);

  MOZ_LOG(gCertBlockPRLog, LogLevel::Warning,
          ("CertBlocklist::PreferenceChanged %s changed", aPref));
  if (strcmp(aPref, PREF_BLOCKLIST_ONECRL_CHECKED) == 0) {
    sLastBlocklistUpdate =
        Preferences::GetUint(PREF_BLOCKLIST_ONECRL_CHECKED, uint32_t(0));
  } else if (strcmp(aPref, PREF_MAX_STALENESS_IN_SECONDS) == 0) {
    sMaxStaleness =
        Preferences::GetUint(PREF_MAX_STALENESS_IN_SECONDS, uint32_t(0));
  }
}
