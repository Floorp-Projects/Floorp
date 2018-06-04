/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPKCS12Blob.h"

#include "ScopedNSSTypes.h"
#include "mozilla/Assertions.h"
#include "mozilla/Casting.h"
#include "mozilla/Unused.h"
#include "nsICertificateDialogs.h"
#include "nsIFile.h"
#include "nsIInputStream.h"
#include "nsIPrompt.h"
#include "nsIWindowWatcher.h"
#include "nsNSSCertHelper.h"
#include "nsNSSCertificate.h"
#include "nsNSSHelper.h"
#include "nsNetUtil.h"
#include "nsReadableUtils.h"
#include "nsThreadUtils.h"
#include "p12plcy.h"
#include "pkix/pkixtypes.h"
#include "secerr.h"

using namespace mozilla;
extern LazyLogModule gPIPNSSLog;

#define PIP_PKCS12_BUFFER_SIZE 2048
#define PIP_PKCS12_NOSMARTCARD_EXPORT 4
#define PIP_PKCS12_RESTORE_FAILED 5
#define PIP_PKCS12_BACKUP_FAILED 6
#define PIP_PKCS12_NSS_ERROR 7

nsPKCS12Blob::nsPKCS12Blob()
  : mUIContext(new PipUIContext())
{
}

// Given a file handle, read a PKCS#12 blob from that file, decode it, and
// import the results into the internal database.
nsresult
nsPKCS12Blob::ImportFromFile(nsIFile* file)
{
  nsresult rv;
  RetryReason wantRetry;
  do {
    rv = ImportFromFileHelper(file, ImportMode::StandardPrompt, wantRetry);

    if (NS_SUCCEEDED(rv) && wantRetry == RetryReason::AutoRetryEmptyPassword) {
      rv = ImportFromFileHelper(file, ImportMode::TryZeroLengthSecitem,
                                wantRetry);
    }
  } while (NS_SUCCEEDED(rv) && (wantRetry != RetryReason::DoNotRetry));

  return rv;
}

void
nsPKCS12Blob::handleImportError(PRErrorCode nssError, RetryReason& retryReason,
                                uint32_t passwordLengthInBytes)
{
  if (nssError == SEC_ERROR_BAD_PASSWORD) {
    // If the password is 2 bytes, it only consists of the wide character null
    // terminator. In this case we want to retry with a zero-length password.
    if (passwordLengthInBytes == 2) {
      retryReason = nsPKCS12Blob::RetryReason::AutoRetryEmptyPassword;
    } else {
      retryReason = RetryReason::BadPassword;
      handleError(PIP_PKCS12_NSS_ERROR, nssError);
    }
  } else {
    handleError(PIP_PKCS12_NSS_ERROR, nssError);
  }
}

// Returns a failing nsresult if some XPCOM operation failed, and NS_OK
// otherwise. Returns by reference whether or not we want to retry the operation
// immediately.
nsresult
nsPKCS12Blob::ImportFromFileHelper(nsIFile* file, ImportMode aImportMode,
                                   RetryReason& aWantRetry)
{
  aWantRetry = RetryReason::DoNotRetry;

  UniquePK11SlotInfo slot(PK11_GetInternalKeySlot());
  if (!slot) {
    return NS_ERROR_FAILURE;
  }

  uint32_t passwordBufferLength;
  UniquePtr<uint8_t[]> passwordBuffer;
  if (aImportMode == ImportMode::TryZeroLengthSecitem) {
    passwordBufferLength = 0;
    passwordBuffer = nullptr;
  } else {
    // get file password (unicode)
    nsresult rv = getPKCS12FilePassword(passwordBufferLength, passwordBuffer);
    if (NS_FAILED(rv)) {
      return rv;
    }
    if (!passwordBuffer) {
      return NS_OK;
    }
  }

  // initialize the decoder
  SECItem unicodePw = { siBuffer, passwordBuffer.get(), passwordBufferLength };
  UniqueSEC_PKCS12DecoderContext dcx(
    SEC_PKCS12DecoderStart(&unicodePw, slot.get(), nullptr, nullptr, nullptr,
                           nullptr, nullptr, nullptr));
  if (!dcx) {
    return NS_ERROR_FAILURE;
  }
  // read input file and feed it to the decoder
  PRErrorCode nssError;
  nsresult rv = inputToDecoder(dcx, file, nssError);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (nssError != 0) {
    handleImportError(nssError, aWantRetry, unicodePw.len);
    return NS_OK;
  }
  // verify the blob
  SECStatus srv = SEC_PKCS12DecoderVerify(dcx.get());
  if (srv != SECSuccess) {
    handleImportError(PR_GetError(), aWantRetry, unicodePw.len);
    return NS_OK;
  }
  // validate bags
  srv = SEC_PKCS12DecoderValidateBags(dcx.get(), nicknameCollision);
  if (srv != SECSuccess) {
    handleImportError(PR_GetError(), aWantRetry, unicodePw.len);
    return NS_OK;
  }
  // import cert and key
  srv = SEC_PKCS12DecoderImportBags(dcx.get());
  if (srv != SECSuccess) {
    handleImportError(PR_GetError(), aWantRetry, unicodePw.len);
  }
  return NS_OK;
}

static bool
isExtractable(UniqueSECKEYPrivateKey& privKey)
{
  ScopedAutoSECItem value;
  SECStatus rv = PK11_ReadRawAttribute(
    PK11_TypePrivKey, privKey.get(), CKA_EXTRACTABLE, &value);
  if (rv != SECSuccess) {
    return false;
  }

  bool isExtractable = false;
  if ((value.len == 1) && value.data) {
    isExtractable = !!(*(CK_BBOOL*)value.data);
  }
  return isExtractable;
}

// Having already loaded the certs, form them into a blob (loading the keys
// also), encode the blob, and stuff it into the file.
nsresult
nsPKCS12Blob::ExportToFile(nsIFile* file, nsIX509Cert** certs, int numCerts)
{
  bool informedUserNoSmartcardBackup = false;

  // get file password (unicode)
  uint32_t passwordBufferLength;
  UniquePtr<uint8_t[]> passwordBuffer;
  nsresult rv = newPKCS12FilePassword(passwordBufferLength, passwordBuffer);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (!passwordBuffer) {
    return NS_OK;
  }
  UniqueSEC_PKCS12ExportContext ecx(
    SEC_PKCS12CreateExportContext(nullptr, nullptr, nullptr, nullptr));
  if (!ecx) {
    handleError(PIP_PKCS12_BACKUP_FAILED, PR_GetError());
    return NS_ERROR_FAILURE;
  }
  // add password integrity
  SECItem unicodePw = { siBuffer, passwordBuffer.get(), passwordBufferLength };
  SECStatus srv = SEC_PKCS12AddPasswordIntegrity(ecx.get(), &unicodePw,
                                                 SEC_OID_SHA1);
  if (srv != SECSuccess) {
    handleError(PIP_PKCS12_BACKUP_FAILED, PR_GetError());
    return NS_ERROR_FAILURE;
  }
  for (int i = 0; i < numCerts; i++) {
    UniqueCERTCertificate nssCert(certs[i]->GetCert());
    if (!nssCert) {
      handleError(PIP_PKCS12_BACKUP_FAILED, PR_GetError());
      return NS_ERROR_FAILURE;
    }
    // We can probably only successfully export certs that are on the internal
    // token. Most, if not all, smart card vendors won't let you extract the
    // private key (in any way shape or form) from the card. So let's punt if
    // the cert is not in the internal db.
    if (nssCert->slot && !PK11_IsInternal(nssCert->slot)) {
      // We aren't the internal token, see if the key is extractable.
      UniqueSECKEYPrivateKey privKey(
        PK11_FindKeyByDERCert(nssCert->slot, nssCert.get(), mUIContext));
      if (privKey && !isExtractable(privKey)) {
        if (!informedUserNoSmartcardBackup) {
          informedUserNoSmartcardBackup = true;
          handleError(PIP_PKCS12_NOSMARTCARD_EXPORT, PR_GetError());
        }
        continue;
      }
    }

    // certSafe and keySafe are owned by ecx.
    SEC_PKCS12SafeInfo* certSafe;
    SEC_PKCS12SafeInfo* keySafe = SEC_PKCS12CreateUnencryptedSafe(ecx.get());
    if (!SEC_PKCS12IsEncryptionAllowed() || PK11_IsFIPS()) {
      certSafe = keySafe;
    } else {
      certSafe = SEC_PKCS12CreatePasswordPrivSafe(
        ecx.get(), &unicodePw,
        SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_40_BIT_RC2_CBC);
    }
    if (!certSafe || !keySafe) {
      handleError(PIP_PKCS12_BACKUP_FAILED, PR_GetError());
      return NS_ERROR_FAILURE;
    }
    // add the cert and key to the blob
    srv = SEC_PKCS12AddCertAndKey(
      ecx.get(),
      certSafe,
      nullptr,
      nssCert.get(),
      CERT_GetDefaultCertDB(),
      keySafe,
      nullptr,
      true,
      &unicodePw,
      SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_3KEY_TRIPLE_DES_CBC);
    if (srv != SECSuccess) {
      handleError(PIP_PKCS12_BACKUP_FAILED, PR_GetError());
      return NS_ERROR_FAILURE;
    }
  }

  UniquePRFileDesc prFile;
  PRFileDesc* rawPRFile;
  rv = file->OpenNSPRFileDesc(
    PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE, 0664, &rawPRFile);
  if (NS_FAILED(rv) || !rawPRFile) {
    handleError(PIP_PKCS12_BACKUP_FAILED, PR_GetError());
    return NS_ERROR_FAILURE;
  }
  prFile.reset(rawPRFile);
  // encode and write
  srv = SEC_PKCS12Encode(ecx.get(), writeExportFile, prFile.get());
  if (srv != SECSuccess) {
    handleError(PIP_PKCS12_BACKUP_FAILED, PR_GetError());
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

// For the NSS PKCS#12 library, must convert PRUnichars (shorts) to a buffer of
// octets. Must handle byte order correctly.
UniquePtr<uint8_t[]>
nsPKCS12Blob::stringToBigEndianBytes(const nsString& uni, uint32_t& bytesLength)
{
  uint32_t wideLength = uni.Length() + 1; // +1 for the null terminator.
  bytesLength = wideLength * 2;
  auto buffer = MakeUnique<uint8_t[]>(bytesLength);

  // We have to use a cast here because on Windows, uni.get() returns
  // char16ptr_t instead of char16_t*.
  mozilla::NativeEndian::copyAndSwapToBigEndian(
    buffer.get(), static_cast<const char16_t*>(uni.get()), wideLength);

  return buffer;
}

// Launch a dialog requesting the user for a new PKCS#12 file passowrd.
// Handle user canceled by returning null password (caller must catch).
nsresult
nsPKCS12Blob::newPKCS12FilePassword(uint32_t& passwordBufferLength,
                                    UniquePtr<uint8_t[]>& passwordBuffer)
{
  nsAutoString password;
  nsCOMPtr<nsICertificateDialogs> certDialogs;
  nsresult rv = ::getNSSDialogs(getter_AddRefs(certDialogs),
                                NS_GET_IID(nsICertificateDialogs),
                                NS_CERTIFICATEDIALOGS_CONTRACTID);
  if (NS_FAILED(rv)) {
    return rv;
  }
  bool pressedOK = false;
  rv = certDialogs->SetPKCS12FilePassword(mUIContext, password, &pressedOK);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (!pressedOK) {
    return NS_OK;
  }
  passwordBuffer = stringToBigEndianBytes(password, passwordBufferLength);
  return NS_OK;
}

// Launch a dialog requesting the user for the password to a PKCS#12 file.
// Handle user canceled by returning null password (caller must catch).
nsresult
nsPKCS12Blob::getPKCS12FilePassword(uint32_t& passwordBufferLength,
                                    UniquePtr<uint8_t[]>& passwordBuffer)
{
  nsCOMPtr<nsICertificateDialogs> certDialogs;
  nsresult rv = ::getNSSDialogs(getter_AddRefs(certDialogs),
                                NS_GET_IID(nsICertificateDialogs),
                                NS_CERTIFICATEDIALOGS_CONTRACTID);
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsAutoString password;
  bool pressedOK = false;
  rv = certDialogs->GetPKCS12FilePassword(mUIContext, password, &pressedOK);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (!pressedOK) {
    return NS_OK;
  }
  passwordBuffer = stringToBigEndianBytes(password, passwordBufferLength);
  return NS_OK;
}

// Given a decoder, read bytes from file and input them to the decoder.
nsresult
nsPKCS12Blob::inputToDecoder(UniqueSEC_PKCS12DecoderContext& dcx, nsIFile* file,
                             PRErrorCode& nssError)
{
  nssError = 0;

  nsCOMPtr<nsIInputStream> fileStream;
  nsresult rv = NS_NewLocalFileInputStream(getter_AddRefs(fileStream), file);
  if (NS_FAILED(rv)) {
    return rv;
  }

  char buf[PIP_PKCS12_BUFFER_SIZE];
  uint32_t amount;
  while (true) {
    rv = fileStream->Read(buf, PIP_PKCS12_BUFFER_SIZE, &amount);
    if (NS_FAILED(rv)) {
      return rv;
    }
    // feed the file data into the decoder
    SECStatus srv = SEC_PKCS12DecoderUpdate(
      dcx.get(), (unsigned char*)buf, amount);
    if (srv != SECSuccess) {
      nssError = PR_GetError();
      return NS_OK;
    }
    if (amount < PIP_PKCS12_BUFFER_SIZE) {
      break;
    }
  }
  return NS_OK;
}

// What to do when the nickname collides with one already in the db.
SECItem*
nsPKCS12Blob::nicknameCollision(SECItem* oldNick, PRBool* cancel, void* wincx)
{
  *cancel = false;
  int count = 1;
  nsCString nickname;
  nsAutoString nickFromProp;
  nsresult rv = GetPIPNSSBundleString("P12DefaultNickname", nickFromProp);
  if (NS_FAILED(rv)) {
    return nullptr;
  }
  NS_ConvertUTF16toUTF8 nickFromPropC(nickFromProp);
  // The user is trying to import a PKCS#12 file that doesn't have the
  // attribute we use to set the nickname.  So in order to reduce the
  // number of interactions we require with the user, we'll build a nickname
  // for the user.  The nickname isn't prominently displayed in the UI,
  // so it's OK if we generate one on our own here.
  //   XXX If the NSS API were smarter and actually passed a pointer to
  //       the CERTCertificate* we're importing we could actually just
  //       call default_nickname (which is what the issuance code path
  //       does) and come up with a reasonable nickname.  Alas, the NSS
  //       API limits our ability to produce a useful nickname without
  //       bugging the user.  :(
  while (1) {
    // If we've gotten this far, that means there isn't a certificate
    // in the database that has the same subject name as the cert we're
    // trying to import.  So we need to come up with a "nickname" to
    // satisfy the NSS requirement or fail in trying to import.
    // Basically we use a default nickname from a properties file and
    // see if a certificate exists with that nickname.  If there isn't, then
    // create update the count by one and append the string '#1' Or
    // whatever the count currently is, and look for a cert with
    // that nickname.  Keep updating the count until we find a nickname
    // without a corresponding cert.
    //  XXX If a user imports *many* certs without the 'friendly name'
    //      attribute, then this may take a long time.  :(
    nickname = nickFromPropC;
    if (count > 1) {
      nickname.AppendPrintf(" #%d", count);
    }
    UniqueCERTCertificate cert(
      CERT_FindCertByNickname(CERT_GetDefaultCertDB(), nickname.get()));
    if (!cert) {
      break;
    }
    count++;
  }
  UniqueSECItem newNick(SECITEM_AllocItem(nullptr, nullptr,
                                          nickname.Length() + 1));
  if (!newNick) {
    return nullptr;
  }
  memcpy(newNick->data, nickname.get(), nickname.Length());
  newNick->data[nickname.Length()] = 0;

  return newNick.release();
}

// write bytes to the exported PKCS#12 file
void
nsPKCS12Blob::writeExportFile(void* arg, const char* buf, unsigned long len)
{
  PRFileDesc* file = static_cast<PRFileDesc*>(arg);
  MOZ_RELEASE_ASSERT(file);
  PR_Write(file, buf, len);
}

void
nsPKCS12Blob::handleError(int myerr, PRErrorCode prerr)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!NS_IsMainThread()) {
    return;
  }

  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("PKCS12: NSS/NSPR error(%d)", prerr));
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("PKCS12: I called(%d)", myerr));

  const char* msgID = nullptr;

  switch (myerr) {
    case PIP_PKCS12_NOSMARTCARD_EXPORT:
      msgID = "PKCS12InfoNoSmartcardBackup";
      break;
    case PIP_PKCS12_RESTORE_FAILED:
      msgID = "PKCS12UnknownErrRestore";
      break;
    case PIP_PKCS12_BACKUP_FAILED:
      msgID = "PKCS12UnknownErrBackup";
      break;
    case PIP_PKCS12_NSS_ERROR:
      switch (prerr) {
        case 0:
          break;
        case SEC_ERROR_PKCS12_CERT_COLLISION:
          msgID = "PKCS12DupData";
          break;
        case SEC_ERROR_BAD_PASSWORD:
          msgID = "PK11BadPassword";
          break;

        case SEC_ERROR_BAD_DER:
        case SEC_ERROR_PKCS12_CORRUPT_PFX_STRUCTURE:
        case SEC_ERROR_PKCS12_INVALID_MAC:
          msgID = "PKCS12DecodeErr";
          break;

        case SEC_ERROR_PKCS12_DUPLICATE_DATA:
          msgID = "PKCS12DupData";
          break;
      }
      break;
  }

  if (!msgID) {
    msgID = "PKCS12UnknownErr";
  }

  nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
  if (!wwatch) {
    return;
  }
  nsCOMPtr<nsIPrompt> prompter;
  if (NS_FAILED(wwatch->GetNewPrompter(nullptr, getter_AddRefs(prompter)))) {
    return;
  }
  nsAutoString message;
  if (NS_FAILED(GetPIPNSSBundleString(msgID, message))) {
    return;
  }

  Unused << prompter->Alert(nullptr, message.get());
}
