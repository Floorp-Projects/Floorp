/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CertBlocklist.h"
#include "mozilla/Base64.h"
#include "mozilla/Preferences.h"
#include "mozilla/unused.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsCRTGlue.h"
#include "nsDirectoryServiceUtils.h"
#include "nsICryptoHash.h"
#include "nsIFileStreams.h"
#include "nsILineInputStream.h"
#include "nsIX509Cert.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsTHashtable.h"
#include "nsThreadUtils.h"
#include "pkix/Input.h"
#include "mozilla/Logging.h"

NS_IMPL_ISUPPORTS(CertBlocklist, nsICertBlocklist)

using namespace mozilla;
using namespace mozilla::pkix;

#define PREF_BACKGROUND_UPDATE_TIMER "app.update.lastUpdateTime.blocklist-background-update-timer"
#define PREF_MAX_STALENESS_IN_SECONDS "security.onecrl.maximum_staleness_in_seconds"

static PRLogModuleInfo* gCertBlockPRLog;

uint32_t CertBlocklist::sLastBlocklistUpdate = 0U;
uint32_t CertBlocklist::sMaxStaleness = 0U;

CertBlocklistItem::CertBlocklistItem(const uint8_t* DNData,
                                     size_t DNLength,
                                     const uint8_t* otherData,
                                     size_t otherLength,
                                     CertBlocklistItemMechanism itemMechanism)
  : mIsCurrent(false)
  , mItemMechanism(itemMechanism)
{
  mDNData = new uint8_t[DNLength];
  memcpy(mDNData, DNData, DNLength);
  mDNLength = DNLength;

  mOtherData = new uint8_t[otherLength];
  memcpy(mOtherData, otherData, otherLength);
  mOtherLength = otherLength;
}

CertBlocklistItem::CertBlocklistItem(const CertBlocklistItem& aItem)
{
  mDNLength = aItem.mDNLength;
  mDNData = new uint8_t[mDNLength];
  memcpy(mDNData, aItem.mDNData, mDNLength);

  mOtherLength = aItem.mOtherLength;
  mOtherData = new uint8_t[mOtherLength];
  memcpy(mOtherData, aItem.mOtherData, mOtherLength);

  mItemMechanism = aItem.mItemMechanism;

  mIsCurrent = aItem.mIsCurrent;
}

CertBlocklistItem::~CertBlocklistItem()
{
  delete[] mDNData;
  delete[] mOtherData;
}

nsresult
CertBlocklistItem::ToBase64(nsACString& b64DNOut, nsACString& b64OtherOut)
{
  nsDependentCSubstring DNString(reinterpret_cast<char*>(mDNData),
                                 mDNLength);
  nsDependentCSubstring otherString(reinterpret_cast<char*>(mOtherData),
                                    mOtherLength);
  nsresult rv = Base64Encode(DNString, b64DNOut);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Base64Encode(otherString, b64OtherOut);
  return rv;
}

bool
CertBlocklistItem::operator==(const CertBlocklistItem& aItem) const
{
  if (aItem.mItemMechanism != mItemMechanism) {
    return false;
  }
  if (aItem.mDNLength != mDNLength ||
      aItem.mOtherLength != mOtherLength) {
    return false;
  }
  return memcmp(aItem.mDNData, mDNData, mDNLength) == 0 &&
         memcmp(aItem.mOtherData, mOtherData, mOtherLength) == 0;
}

uint32_t
CertBlocklistItem::Hash() const
{
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
  : mMutex("CertBlocklist::mMutex")
  , mModified(false)
  , mBackingFileIsInitialized(false)
  , mBackingFile(nullptr)
{
  if (!gCertBlockPRLog) {
    gCertBlockPRLog = PR_NewLogModule("CertBlock");
  }
}

CertBlocklist::~CertBlocklist()
{
  Preferences::UnregisterCallback(CertBlocklist::PreferenceChanged,
                                  PREF_BACKGROUND_UPDATE_TIMER,
                                  this);
  Preferences::UnregisterCallback(CertBlocklist::PreferenceChanged,
                                  PREF_MAX_STALENESS_IN_SECONDS,
                                  this);
}

nsresult
CertBlocklist::Init()
{
  PR_LOG(gCertBlockPRLog, PR_LOG_DEBUG, ("CertBlocklist::Init"));

  // Init must be on main thread for getting the profile directory
  if (!NS_IsMainThread()) {
    PR_LOG(gCertBlockPRLog, PR_LOG_DEBUG,
           ("CertBlocklist::Init - called off main thread"));
    return NS_ERROR_NOT_SAME_THREAD;
  }

  // Register preference callbacks
  nsresult rv =
      Preferences::RegisterCallbackAndCall(CertBlocklist::PreferenceChanged,
                                           PREF_BACKGROUND_UPDATE_TIMER,
                                           this);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Preferences::RegisterCallbackAndCall(CertBlocklist::PreferenceChanged,
                                            PREF_MAX_STALENESS_IN_SECONDS,
                                            this);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Get the profile directory
  rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                              getter_AddRefs(mBackingFile));
  if (NS_FAILED(rv) || !mBackingFile) {
    PR_LOG(gCertBlockPRLog, PR_LOG_DEBUG,
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
  rv = mBackingFile->GetNativePath(path);
  if (NS_FAILED(rv)) {
    return rv;
  }
  PR_LOG(gCertBlockPRLog, PR_LOG_DEBUG,
         ("CertBlocklist::Init certList path: %s", path.get()));

  return NS_OK;
}

nsresult
CertBlocklist::EnsureBackingFileInitialized(MutexAutoLock& lock)
{
  PR_LOG(gCertBlockPRLog, PR_LOG_DEBUG,
         ("CertBlocklist::EnsureBackingFileInitialized"));
  if (mBackingFileIsInitialized || !mBackingFile) {
    return NS_OK;
  }

  PR_LOG(gCertBlockPRLog, PR_LOG_DEBUG,
         ("CertBlocklist::EnsureBackingFileInitialized - not initialized"));

  bool exists = false;
  nsresult rv = mBackingFile->Exists(&exists);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (!exists) {
    PR_LOG(gCertBlockPRLog, PR_LOG_WARN,
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
    PR_LOG(gCertBlockPRLog, PR_LOG_DEBUG,
           ("CertBlocklist::EnsureBackingFileInitialized adding: %s %s",
            DN.get(), other.get()));

    PR_LOG(gCertBlockPRLog, PR_LOG_DEBUG,
           ("CertBlocklist::EnsureBackingFileInitialized - pre-decode"));

    rv = AddRevokedCertInternal(DN, other, mechanism, CertOldFromLocalCache,
                                lock);

    if (NS_FAILED(rv)) {
      // we warn here, rather than abandoning, since we need to
      // ensure that as many items as possible are read
      PR_LOG(gCertBlockPRLog, PR_LOG_WARN,
             ("CertBlocklist::EnsureBackingFileInitialized adding revoked cert "
              "failed"));
    }
  } while (more);
  mBackingFileIsInitialized = true;
  return NS_OK;
}

// void revokeCertBySubjectAndPubKey(in string subject, in string pubKeyHash);
NS_IMETHODIMP
CertBlocklist::RevokeCertBySubjectAndPubKey(const char* aSubject,
                                            const char* aPubKeyHash)
{
  PR_LOG(gCertBlockPRLog, PR_LOG_DEBUG,
         ("CertBlocklist::RevokeCertBySubjectAndPubKey - subject is: %s and pubKeyHash: %s",
          aSubject, aPubKeyHash));
  MutexAutoLock lock(mMutex);

  return AddRevokedCertInternal(nsDependentCString(aSubject),
                                nsDependentCString(aPubKeyHash),
                                BlockBySubjectAndPubKey,
                                CertNewFromBlocklist, lock);
}

// void revokeCertByIssuerAndSerial(in string issuer, in string serialNumber);
NS_IMETHODIMP
CertBlocklist::RevokeCertByIssuerAndSerial(const char* aIssuer,
                                           const char* aSerialNumber)
{
  PR_LOG(gCertBlockPRLog, PR_LOG_DEBUG,
         ("CertBlocklist::RevokeCertByIssuerAndSerial - issuer is: %s and serial: %s",
          aIssuer, aSerialNumber));
  MutexAutoLock lock(mMutex);

  return AddRevokedCertInternal(nsDependentCString(aIssuer),
                                nsDependentCString(aSerialNumber),
                                BlockByIssuerAndSerial,
                                CertNewFromBlocklist, lock);
}

nsresult
CertBlocklist::AddRevokedCertInternal(const nsACString& aEncodedDN,
                                      const nsACString& aEncodedOther,
                                      CertBlocklistItemMechanism aMechanism,
                                      CertBlocklistItemState aItemState,
                                      MutexAutoLock& /*proofOfLock*/)
{
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

    CertBlocklistItem item(reinterpret_cast<const uint8_t*>(decodedDN.get()),
                           decodedDN.Length(),
                           reinterpret_cast<const uint8_t*>(decodedOther.get()),
                           decodedOther.Length(),
                           aMechanism);



  if (aItemState == CertNewFromBlocklist) {
    // we want SaveEntries to be a no-op if no new entries are added
    if (!mBlocklist.Contains(item)) {
      mModified = true;
    }

    // Ensure that any existing item is replaced by a fresh one so we can
    // use mIsCurrent to decide which entries to write out
    mBlocklist.RemoveEntry(item);
    item.mIsCurrent = true;
  }
  mBlocklist.PutEntry(item);

  return NS_OK;
}

// Data needed for writing blocklist items out to the revocations file
struct BlocklistSaveInfo
{
  IssuerTable issuerTable;
  BlocklistStringSet issuers;
  nsCOMPtr<nsIOutputStream> outputStream;
  bool success;
};

// Write a line for a given string in the output stream
nsresult
WriteLine(nsIOutputStream* outputStream, const nsACString& string)
{
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

// sort blocklist items into lists of serials for each issuer
PLDHashOperator
ProcessEntry(BlocklistItemKey* aHashKey, void* aUserArg)
{
  BlocklistSaveInfo* saveInfo = reinterpret_cast<BlocklistSaveInfo*>(aUserArg);
  CertBlocklistItem item = aHashKey->GetKey();

  if (!item.mIsCurrent) {
    return PL_DHASH_NEXT;
  }

  nsAutoCString encDN;
  nsAutoCString encOther;

  nsresult rv = item.ToBase64(encDN, encOther);
  if (NS_FAILED(rv)) {
    saveInfo->success = false;
    return PL_DHASH_STOP;
  }

  // If it's a subject / public key block, write it straight out
  if (item.mItemMechanism == BlockBySubjectAndPubKey) {
    WriteLine(saveInfo->outputStream, encDN);
    WriteLine(saveInfo->outputStream, NS_LITERAL_CSTRING("\t") + encOther);
    return PL_DHASH_NEXT;
  }

  // Otherwise, we have to group entries by issuer
  saveInfo->issuers.PutEntry(encDN);
  BlocklistStringSet* issuerSet = saveInfo->issuerTable.Get(encDN);
  if (!issuerSet) {
    issuerSet = new BlocklistStringSet();
    saveInfo->issuerTable.Put(encDN, issuerSet);
  }
  issuerSet->PutEntry(encOther);
  return PL_DHASH_NEXT;
}

// write serial data to the output stream
PLDHashOperator
WriteSerial(nsCStringHashKey* aHashKey, void* aUserArg)
{
  BlocklistSaveInfo* saveInfo = reinterpret_cast<BlocklistSaveInfo*>(aUserArg);

  nsresult rv = WriteLine(saveInfo->outputStream,
                          NS_LITERAL_CSTRING(" ") + aHashKey->GetKey());
  if (NS_FAILED(rv)) {
    saveInfo->success = false;
    return PL_DHASH_STOP;
  }
  return PL_DHASH_NEXT;
}

// Write issuer data to the output stream
PLDHashOperator
WriteIssuer(nsCStringHashKey* aHashKey, void* aUserArg)
{
  BlocklistSaveInfo* saveInfo = reinterpret_cast<BlocklistSaveInfo*>(aUserArg);
  nsAutoPtr<BlocklistStringSet> issuerSet;

  saveInfo->issuerTable.RemoveAndForget(aHashKey->GetKey(), issuerSet);

  nsresult rv = WriteLine(saveInfo->outputStream, aHashKey->GetKey());
  if (NS_FAILED(rv)) {
    return PL_DHASH_STOP;
  }

  issuerSet->EnumerateEntries(WriteSerial, saveInfo);
  if (!saveInfo->success) {
    saveInfo->success = false;
    return PL_DHASH_STOP;
  }
  return PL_DHASH_NEXT;
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
CertBlocklist::SaveEntries()
{
  PR_LOG(gCertBlockPRLog, PR_LOG_DEBUG,
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
    PR_LOG(gCertBlockPRLog, PR_LOG_WARN,
           ("CertBlocklist::SaveEntries no file in profile to write to"));
    return NS_OK;
  }

  BlocklistSaveInfo saveInfo;
  saveInfo.success = true;
  rv = NS_NewAtomicFileOutputStream(getter_AddRefs(saveInfo.outputStream),
                                    mBackingFile, -1, -1, 0);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = WriteLine(saveInfo.outputStream,
                 NS_LITERAL_CSTRING("# Auto generated contents. Do not edit."));
  if (NS_FAILED(rv)) {
    return rv;
  }

  mBlocklist.EnumerateEntries(ProcessEntry, &saveInfo);
  if (!saveInfo.success) {
    PR_LOG(gCertBlockPRLog, PR_LOG_WARN,
           ("CertBlocklist::SaveEntries writing revocation data failed"));
    return NS_ERROR_FAILURE;
  }

  saveInfo.issuers.EnumerateEntries(WriteIssuer, &saveInfo);
  if (!saveInfo.success) {
    PR_LOG(gCertBlockPRLog, PR_LOG_WARN,
           ("CertBlocklist::SaveEntries writing revocation data failed"));
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsISafeOutputStream> safeStream =
      do_QueryInterface(saveInfo.outputStream);
  NS_ASSERTION(safeStream, "expected a safe output stream!");
  if (!safeStream) {
    return NS_ERROR_FAILURE;
  }
  rv = safeStream->Finish();
  if (NS_FAILED(rv)) {
    PR_LOG(gCertBlockPRLog, PR_LOG_WARN,
           ("CertBlocklist::SaveEntries saving revocation data failed"));
    return rv;
  }
  mModified = false;
  return NS_OK;
}

// boolean isCertRevoked([const, array, size_is(issuerLength)] in octet issuer,
//                       in unsigned long issuerLength,
//                       [const, array, size_is(serialLength)] in octet serial,
//                       in unsigned long serialLength),
//                       [const, array, size_is(subject_length)] in octet subject,
//                       in unsigned long subject_length,
//                       [const, array, size_is(pubkey_length)] in octet pubkey,
//                       in unsigned long pubkey_length);
NS_IMETHODIMP
CertBlocklist::IsCertRevoked(const uint8_t* aIssuer,
                             uint32_t aIssuerLength,
                             const uint8_t* aSerial,
                             uint32_t aSerialLength,
                             const uint8_t* aSubject,
                             uint32_t aSubjectLength,
                             const uint8_t* aPubKey,
                             uint32_t aPubKeyLength,
                             bool* _retval)
{
  MutexAutoLock lock(mMutex);

  nsresult rv = EnsureBackingFileInitialized(lock);
  if (NS_FAILED(rv)) {
    return rv;
  }

  Input issuer;
  Input serial;
  if (issuer.Init(aIssuer, aIssuerLength) != Success) {
    return NS_ERROR_FAILURE;
  }
  if (serial.Init(aSerial, aSerialLength) != Success) {
    return NS_ERROR_FAILURE;
  }

  CertBlocklistItem issuerSerial(aIssuer, aIssuerLength, aSerial, aSerialLength,
                                 BlockByIssuerAndSerial);
  *_retval = mBlocklist.Contains(issuerSerial);

  if (*_retval) {
    return NS_OK;
  }

  nsCOMPtr<nsICryptoHash> crypto;
  crypto = do_CreateInstance(NS_CRYPTO_HASH_CONTRACTID, &rv);

  rv = crypto->Init(nsICryptoHash::SHA256);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = crypto->Update(reinterpret_cast<const unsigned char*>(aPubKey),
                      aPubKeyLength);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCString hashString;
  rv = crypto->Finish(false, hashString);
  if (NS_FAILED(rv)) {
    return rv;
  }

  CertBlocklistItem subjectPubKey(aSubject,
                                  static_cast<size_t>(aSubjectLength),
                                  reinterpret_cast<const uint8_t*>(hashString.get()),
                                  hashString.Length(),
                                  BlockBySubjectAndPubKey);
  *_retval = mBlocklist.Contains(subjectPubKey);

  return NS_OK;
}

NS_IMETHODIMP
CertBlocklist::IsBlocklistFresh(bool* _retval)
{
  MutexAutoLock lock(mMutex);
  *_retval = false;

  uint32_t now = uint32_t(PR_Now() / PR_USEC_PER_SEC);

  if (now > sLastBlocklistUpdate) {
    int64_t interval = now - sLastBlocklistUpdate;
    PR_LOG(gCertBlockPRLog, PR_LOG_WARN,
           ("CertBlocklist::IsBlocklistFresh we're after the last BlocklistUpdate "
            "interval is %i, staleness %u", interval, sMaxStaleness));
    *_retval = sMaxStaleness > interval;
  }
  PR_LOG(gCertBlockPRLog, PR_LOG_WARN,
         ("CertBlocklist::IsBlocklistFresh ? %s", *_retval ? "true" : "false"));
  return NS_OK;
}


/* static */
void
CertBlocklist::PreferenceChanged(const char* aPref, void* aClosure)

{
  CertBlocklist* blocklist = reinterpret_cast<CertBlocklist*>(aClosure);
  MutexAutoLock lock(blocklist->mMutex);

  PR_LOG(gCertBlockPRLog, PR_LOG_WARN,
         ("CertBlocklist::PreferenceChanged %s changed", aPref));
  if (strcmp(aPref, PREF_BACKGROUND_UPDATE_TIMER) == 0) {
    sLastBlocklistUpdate = Preferences::GetUint(PREF_BACKGROUND_UPDATE_TIMER,
                                                uint32_t(0));
  } else if (strcmp(aPref, PREF_MAX_STALENESS_IN_SECONDS) == 0) {
    sMaxStaleness = Preferences::GetUint(PREF_MAX_STALENESS_IN_SECONDS,
                                         uint32_t(0));
  }
}
