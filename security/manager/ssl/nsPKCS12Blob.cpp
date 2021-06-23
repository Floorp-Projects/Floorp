/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPKCS12Blob.h"

#include "mozilla/Assertions.h"
#include "mozilla/Casting.h"
#include "mozilla/Preferences.h"
#include "mozilla/Unused.h"
#include "mozpkix/pkixtypes.h"
#include "nsIFile.h"
#include "nsIInputStream.h"
#include "nsIX509CertDB.h"
#include "nsNetUtil.h"
#include "nsNSSCertHelper.h"
#include "nsNSSCertificate.h"
#include "nsNSSHelper.h"
#include "nsReadableUtils.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"
#include "p12plcy.h"
#include "ScopedNSSTypes.h"
#include "secerr.h"

using namespace mozilla;
extern LazyLogModule gPIPNSSLog;

#define PIP_PKCS12_BUFFER_SIZE 2048
#define PIP_PKCS12_NOSMARTCARD_EXPORT 4
#define PIP_PKCS12_RESTORE_FAILED 5
#define PIP_PKCS12_BACKUP_FAILED 6
#define PIP_PKCS12_NSS_ERROR 7

nsPKCS12Blob::nsPKCS12Blob() : mUIContext(new PipUIContext()) {}

// Given a file handle, read a PKCS#12 blob from that file, decode it, and
// import the results into the internal database.
nsresult nsPKCS12Blob::ImportFromFile(nsIFile* aFile,
                                      const nsAString& aPassword,
                                      uint32_t& aError) {
  uint32_t passwordBufferLength;
  UniquePtr<uint8_t[]> passwordBuffer;

  UniquePK11SlotInfo slot(PK11_GetInternalKeySlot());
  if (!slot) {
    return NS_ERROR_FAILURE;
  }

  passwordBuffer = stringToBigEndianBytes(aPassword, passwordBufferLength);

  // initialize the decoder
  SECItem unicodePw = {siBuffer, passwordBuffer.get(), passwordBufferLength};
  UniqueSEC_PKCS12DecoderContext dcx(
      SEC_PKCS12DecoderStart(&unicodePw, slot.get(), nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr));
  if (!dcx) {
    return NS_ERROR_FAILURE;
  }
  // read input aFile and feed it to the decoder
  PRErrorCode nssError;
  nsresult rv = inputToDecoder(dcx, aFile, nssError);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (nssError != 0) {
    aError = handlePRErrorCode(nssError);
    return NS_OK;
  }
  // verify the blob
  SECStatus srv = SEC_PKCS12DecoderVerify(dcx.get());
  if (srv != SECSuccess) {
    aError = handlePRErrorCode(PR_GetError());
    return NS_OK;
  }
  // validate bags
  srv = SEC_PKCS12DecoderValidateBags(dcx.get(), nicknameCollision);
  if (srv != SECSuccess) {
    aError = handlePRErrorCode(PR_GetError());
    return NS_OK;
  }
  // import cert and key
  srv = SEC_PKCS12DecoderImportBags(dcx.get());
  if (srv != SECSuccess) {
    aError = handlePRErrorCode(PR_GetError());
    return NS_OK;
  }
  aError = nsIX509CertDB::Success;
  return NS_OK;
}

static bool isExtractable(UniqueSECKEYPrivateKey& privKey) {
  ScopedAutoSECItem value;
  SECStatus rv = PK11_ReadRawAttribute(PK11_TypePrivKey, privKey.get(),
                                       CKA_EXTRACTABLE, &value);
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
nsresult nsPKCS12Blob::ExportToFile(nsIFile* aFile,
                                    const nsTArray<RefPtr<nsIX509Cert>>& aCerts,
                                    const nsAString& aPassword,
                                    uint32_t& aError) {
  nsCString passwordUtf8 = NS_ConvertUTF16toUTF8(aPassword);
  uint32_t passwordBufferLength = passwordUtf8.Length();
  aError = nsIX509CertDB::Success;
  // The conversion to UCS2 is executed by sec_pkcs12_encode_password when
  // necessary (for some older PKCS12 algorithms). The NSS 3.31 and newer
  // expects password to be in the utf8 encoding to support modern encoders.
  UniquePtr<unsigned char[]> passwordBuffer(
      reinterpret_cast<unsigned char*>(ToNewCString(passwordUtf8)));
  if (!passwordBuffer.get()) {
    return NS_OK;
  }
  UniqueSEC_PKCS12ExportContext ecx(
      SEC_PKCS12CreateExportContext(nullptr, nullptr, nullptr, nullptr));
  if (!ecx) {
    aError = nsIX509CertDB::ERROR_PKCS12_BACKUP_FAILED;
    return NS_OK;
  }
  // add password integrity
  SECItem unicodePw = {siBuffer, passwordBuffer.get(), passwordBufferLength};
  SECStatus srv =
      SEC_PKCS12AddPasswordIntegrity(ecx.get(), &unicodePw, SEC_OID_SHA1);
  if (srv != SECSuccess) {
    aError = nsIX509CertDB::ERROR_PKCS12_BACKUP_FAILED;
    return NS_OK;
  }
  for (auto& cert : aCerts) {
    UniqueCERTCertificate nssCert(cert->GetCert());
    if (!nssCert) {
      aError = nsIX509CertDB::ERROR_PKCS12_BACKUP_FAILED;
      return NS_OK;
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
        // This is informative.  If a serious error occurs later it will
        // override it later and return.
        aError = nsIX509CertDB::ERROR_PKCS12_NOSMARTCARD_EXPORT;
        continue;
      }
    }

    // certSafe and keySafe are owned by ecx.
    SEC_PKCS12SafeInfo* certSafe;
    SEC_PKCS12SafeInfo* keySafe = SEC_PKCS12CreateUnencryptedSafe(ecx.get());
    bool useModernCrypto = Preferences::GetBool(
        "security.pki.use_modern_crypto_with_pkcs12", false);
    // We use SEC_OID_AES_128_CBC for the password and SEC_OID_AES_256_CBC
    // for the certificate because it's a default for openssl an pk12util
    // command.
    if (!SEC_PKCS12IsEncryptionAllowed() || PK11_IsFIPS()) {
      certSafe = keySafe;
    } else {
      SECOidTag privAlg =
          useModernCrypto ? SEC_OID_AES_128_CBC
                          : SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_40_BIT_RC2_CBC;
      certSafe =
          SEC_PKCS12CreatePasswordPrivSafe(ecx.get(), &unicodePw, privAlg);
    }
    if (!certSafe || !keySafe) {
      aError = nsIX509CertDB::ERROR_PKCS12_BACKUP_FAILED;
      return NS_OK;
    }
    // add the cert and key to the blob
    SECOidTag algorithm =
        useModernCrypto
            ? SEC_OID_AES_256_CBC
            : SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_3KEY_TRIPLE_DES_CBC;
    srv = SEC_PKCS12AddCertAndKey(ecx.get(), certSafe, nullptr, nssCert.get(),
                                  CERT_GetDefaultCertDB(), keySafe, nullptr,
                                  true, &unicodePw, algorithm);
    if (srv != SECSuccess) {
      aError = nsIX509CertDB::ERROR_PKCS12_BACKUP_FAILED;
      return NS_OK;
    }
  }

  UniquePRFileDesc prFile;
  PRFileDesc* rawPRFile;
  nsresult rv = aFile->OpenNSPRFileDesc(PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE,
                                        0664, &rawPRFile);
  if (NS_FAILED(rv) || !rawPRFile) {
    aError = nsIX509CertDB::ERROR_PKCS12_BACKUP_FAILED;
    return NS_OK;
  }
  prFile.reset(rawPRFile);
  // encode and write
  srv = SEC_PKCS12Encode(ecx.get(), writeExportFile, prFile.get());
  if (srv != SECSuccess) {
    aError = nsIX509CertDB::ERROR_PKCS12_BACKUP_FAILED;
  }
  return NS_OK;
}

// For the NSS PKCS#12 library, must convert PRUnichars (shorts) to a buffer of
// octets. Must handle byte order correctly.
UniquePtr<uint8_t[]> nsPKCS12Blob::stringToBigEndianBytes(
    const nsAString& uni, uint32_t& bytesLength) {
  if (uni.IsVoid()) {
    bytesLength = 0;
    return nullptr;
  }

  uint32_t wideLength = uni.Length() + 1;  // +1 for the null terminator.
  bytesLength = wideLength * 2;
  auto buffer = MakeUnique<uint8_t[]>(bytesLength);

  // We have to use a cast here because on Windows, uni.get() returns
  // char16ptr_t instead of char16_t*.
  mozilla::NativeEndian::copyAndSwapToBigEndian(
      buffer.get(), static_cast<const char16_t*>(uni.BeginReading()),
      wideLength);

  return buffer;
}

// Given a decoder, read bytes from file and input them to the decoder.
nsresult nsPKCS12Blob::inputToDecoder(UniqueSEC_PKCS12DecoderContext& dcx,
                                      nsIFile* file, PRErrorCode& nssError) {
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
    SECStatus srv =
        SEC_PKCS12DecoderUpdate(dcx.get(), (unsigned char*)buf, amount);
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
SECItem* nsPKCS12Blob::nicknameCollision(SECItem* oldNick, PRBool* cancel,
                                         void* wincx) {
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
  UniqueSECItem newNick(
      SECITEM_AllocItem(nullptr, nullptr, nickname.Length() + 1));
  if (!newNick) {
    return nullptr;
  }
  memcpy(newNick->data, nickname.get(), nickname.Length());
  newNick->data[nickname.Length()] = 0;

  return newNick.release();
}

// write bytes to the exported PKCS#12 file
void nsPKCS12Blob::writeExportFile(void* arg, const char* buf,
                                   unsigned long len) {
  PRFileDesc* file = static_cast<PRFileDesc*>(arg);
  MOZ_RELEASE_ASSERT(file);
  PR_Write(file, buf, len);
}

// Translate PRErrorCode to nsIX509CertDB error
uint32_t nsPKCS12Blob::handlePRErrorCode(PRErrorCode aPrerr) {
  MOZ_ASSERT(aPrerr != 0);
  uint32_t error = nsIX509CertDB::ERROR_UNKNOWN;
  switch (aPrerr) {
    case SEC_ERROR_PKCS12_CERT_COLLISION:
      error = nsIX509CertDB::ERROR_PKCS12_DUPLICATE_DATA;
      break;
    // INVALID_ARGS is returned on bad password when importing cert
    // exported from firefox or generated by openssl
    case SEC_ERROR_INVALID_ARGS:
    case SEC_ERROR_BAD_PASSWORD:
      error = nsIX509CertDB::ERROR_BAD_PASSWORD;
      break;
    case SEC_ERROR_BAD_DER:
    case SEC_ERROR_PKCS12_CORRUPT_PFX_STRUCTURE:
    case SEC_ERROR_PKCS12_INVALID_MAC:
      error = nsIX509CertDB::ERROR_DECODE_ERROR;
      break;
    case SEC_ERROR_PKCS12_DUPLICATE_DATA:
      error = nsIX509CertDB::ERROR_PKCS12_DUPLICATE_DATA;
      break;
  }
  return error;
}
