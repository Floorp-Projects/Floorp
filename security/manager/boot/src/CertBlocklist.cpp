/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CertBlocklist.h"
#include "mozilla/Base64.h"
#include "mozilla/unused.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsCRTGlue.h"
#include "nsDirectoryServiceUtils.h"
#include "nsIFileStreams.h"
#include "nsILineInputStream.h"
#include "nsIX509Cert.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsTHashtable.h"
#include "nsThreadUtils.h"
#include "pkix/Input.h"
#include "prlog.h"

NS_IMPL_ISUPPORTS(CertBlocklist, nsICertBlocklist)

static PRLogModuleInfo* gCertBlockPRLog;

CertBlocklistItem::CertBlocklistItem(mozilla::pkix::Input aIssuer,
                                     mozilla::pkix::Input aSerial)
  : mIsCurrent(false)
{
  mIssuerData = new uint8_t[aIssuer.GetLength()];
  memcpy(mIssuerData, aIssuer.UnsafeGetData(), aIssuer.GetLength());
  mozilla::unused << mIssuer.Init(mIssuerData, aIssuer.GetLength());

  mSerialData = new uint8_t[aSerial.GetLength()];
  memcpy(mSerialData, aSerial.UnsafeGetData(), aSerial.GetLength());
  mozilla::unused << mSerial.Init(mSerialData, aSerial.GetLength());
}

CertBlocklistItem::CertBlocklistItem(const CertBlocklistItem& aItem)
{
  uint32_t issuerLength = aItem.mIssuer.GetLength();
  mIssuerData = new uint8_t[issuerLength];
  memcpy(mIssuerData, aItem.mIssuerData, issuerLength);
  mozilla::unused << mIssuer.Init(mIssuerData, issuerLength);

  uint32_t serialLength = aItem.mSerial.GetLength();
  mSerialData = new uint8_t[serialLength];
  memcpy(mSerialData, aItem.mSerialData, serialLength);
  mozilla::unused << mSerial.Init(mSerialData, serialLength);
  mIsCurrent = aItem.mIsCurrent;
}

CertBlocklistItem::~CertBlocklistItem()
{
  delete[] mIssuerData;
  delete[] mSerialData;
}

nsresult
CertBlocklistItem::ToBase64(nsACString& b64IssuerOut, nsACString& b64SerialOut)
{
  nsDependentCSubstring issuerString(reinterpret_cast<char*>(mIssuerData),
                                     mIssuer.GetLength());
  nsDependentCSubstring serialString(reinterpret_cast<char*>(mSerialData),
                                     mSerial.GetLength());
  nsresult rv = mozilla::Base64Encode(issuerString, b64IssuerOut);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = mozilla::Base64Encode(serialString, b64SerialOut);
  return rv;
}

bool
CertBlocklistItem::operator==(const CertBlocklistItem& aItem) const
{
  bool retval = InputsAreEqual(aItem.mIssuer, mIssuer) &&
                InputsAreEqual(aItem.mSerial, mSerial);
  return retval;
}

uint32_t
CertBlocklistItem::Hash() const
{
  uint32_t hash;
  uint32_t serialLength = mSerial.GetLength();
  // there's no requirement for a serial to be as large as the size of the hash
  // key; if it's smaller, fall back to the first octet (otherwise, the last
  // four)
   if (serialLength >= sizeof(hash)) {
     memcpy(&hash, mSerialData + serialLength - sizeof(hash), sizeof(hash));
  } else {
    hash = *mSerialData;
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

  // Get the profile directory
  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
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
CertBlocklist::EnsureBackingFileInitialized(mozilla::MutexAutoLock& lock)
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
  nsAutoCString issuer;
  nsAutoCString serial;
  // read in the revocations file. The file format is as follows: each line
  // contains a comment, base64 encoded DER for an issuer or base64 encoded DER
  // for a serial number. Comment lines start with '#', serial number lines, ' '
  // (a space) and anything else is assumed to be an issuer.
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
    if (line.First() != ' ') {
      issuer = line;
      continue;
    }
    serial = line;
    serial.Trim(" ", true, false, false);
    // serial numbers 'belong' to the last issuer line seen; if no issuer has
    // been seen, the serial number is ignored
    if (issuer.IsEmpty() || serial.IsEmpty()) {
      continue;
    }
    PR_LOG(gCertBlockPRLog, PR_LOG_DEBUG,
           ("CertBlocklist::EnsureBackingFileInitialized adding: %s %s",
           issuer.get(), serial.get()));
    rv = AddRevokedCertInternal(issuer.get(),
                                serial.get(),
                                CertOldFromLocalCache,
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

// void addRevokedCert (in string issuer, in string serialNumber);
NS_IMETHODIMP
CertBlocklist::AddRevokedCert(const char* aIssuer,
                              const char* aSerialNumber)
{
  PR_LOG(gCertBlockPRLog, PR_LOG_DEBUG,
         ("CertBlocklist::AddRevokedCert - issuer is: %s and serial: %s",
          aIssuer, aSerialNumber));
  mozilla::MutexAutoLock lock(mMutex);
  return AddRevokedCertInternal(aIssuer,
                                aSerialNumber,
                                CertNewFromBlocklist,
                                lock);
}

nsresult
CertBlocklist::AddRevokedCertInternal(const char* aIssuer,
                                      const char* aSerialNumber,
                                      CertBlocklistItemState aItemState,
                                      mozilla::MutexAutoLock& /*proofOfLock*/)
{
  nsCString decodedIssuer;
  nsCString decodedSerial;

  nsresult rv;
  rv = mozilla::Base64Decode(nsDependentCString(aIssuer), decodedIssuer);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = mozilla::Base64Decode(nsDependentCString(aSerialNumber), decodedSerial);
  if (NS_FAILED(rv)) {
    return rv;
  }

  mozilla::pkix::Input issuer;
  mozilla::pkix::Input serial;

  mozilla::pkix::Result pkrv;
  pkrv = issuer.Init(reinterpret_cast<const uint8_t*>(decodedIssuer.get()),
                     decodedIssuer.Length());
  if (pkrv != mozilla::pkix::Success) {
    return NS_ERROR_FAILURE;
  }
  pkrv = serial.Init(reinterpret_cast<const uint8_t*>(decodedSerial.get()),
                     decodedSerial.Length());
  if (pkrv != mozilla::pkix::Success) {
    return NS_ERROR_FAILURE;
  }

  CertBlocklistItem item(issuer, serial);

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

  nsAutoCString encIssuer;
  nsAutoCString encSerial;

  nsresult rv = item.ToBase64(encIssuer, encSerial);
  if (NS_FAILED(rv)) {
    saveInfo->success = false;
    return PL_DHASH_STOP;
  }

  saveInfo->issuers.PutEntry(encIssuer);
  BlocklistStringSet* issuerSet = saveInfo->issuerTable.Get(encIssuer);
  if (!issuerSet) {
    issuerSet = new BlocklistStringSet();
    saveInfo->issuerTable.Put(encIssuer, issuerSet);
  }
  issuerSet->PutEntry(encSerial);
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
  mozilla::MutexAutoLock lock(mMutex);
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
  mBlocklist.EnumerateEntries(ProcessEntry, &saveInfo);
  if (!saveInfo.success) {
    PR_LOG(gCertBlockPRLog, PR_LOG_WARN,
           ("CertBlocklist::SaveEntries writing revocation data failed"));
    return NS_ERROR_FAILURE;
  }

  rv = WriteLine(saveInfo.outputStream,
                 NS_LITERAL_CSTRING("# Auto generated contents. Do not edit."));
  if (NS_FAILED(rv)) {
    return rv;
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
//                       in unsigned long serialLength);
NS_IMETHODIMP CertBlocklist::IsCertRevoked(const uint8_t* aIssuer,
                                           uint32_t aIssuerLength,
                                           const uint8_t* aSerial,
                                           uint32_t aSerialLength,
                                           bool* _retval)
{
  mozilla::MutexAutoLock lock(mMutex);

  nsresult rv = EnsureBackingFileInitialized(lock);
  if (NS_FAILED(rv)) {
    return rv;
  }

  mozilla::pkix::Input issuer;
  mozilla::pkix::Input serial;
  if (issuer.Init(aIssuer, aIssuerLength) != mozilla::pkix::Success) {
    return NS_ERROR_FAILURE;
  }
  if (serial.Init(aSerial, aSerialLength) != mozilla::pkix::Success) {
    return NS_ERROR_FAILURE;
  }

  CertBlocklistItem item(issuer, serial);
  *_retval = mBlocklist.Contains(item);

  return NS_OK;
}
