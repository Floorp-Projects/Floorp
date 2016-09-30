/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPKCS12Blob.h"

#include "ScopedNSSTypes.h"
#include "mozilla/Casting.h"
#include "nsCRT.h"
#include "nsCRTGlue.h"
#include "nsDirectoryServiceDefs.h"
#include "nsICertificateDialogs.h"
#include "nsIDirectoryService.h"
#include "nsIFile.h"
#include "nsIInputStream.h"
#include "nsKeygenHandler.h" // For GetSlotWithMechanism
#include "nsNSSCertificate.h"
#include "nsNSSComponent.h"
#include "nsNSSHelper.h"
#include "nsNSSShutDown.h"
#include "nsNetUtil.h"
#include "nsPK11TokenDB.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "pkix/pkixtypes.h"
#include "prmem.h"
#include "prprf.h"
#include "secerr.h"

using namespace mozilla;
extern LazyLogModule gPIPNSSLog;

#define PIP_PKCS12_TMPFILENAME   NS_LITERAL_CSTRING(".pip_p12tmp")
#define PIP_PKCS12_BUFFER_SIZE   2048
#define PIP_PKCS12_RESTORE_OK          1
#define PIP_PKCS12_BACKUP_OK           2
#define PIP_PKCS12_USER_CANCELED       3
#define PIP_PKCS12_NOSMARTCARD_EXPORT  4
#define PIP_PKCS12_RESTORE_FAILED      5
#define PIP_PKCS12_BACKUP_FAILED       6
#define PIP_PKCS12_NSS_ERROR           7

// constructor
nsPKCS12Blob::nsPKCS12Blob():mCertArray(0),
                             mTmpFile(nullptr),
                             mTokenSet(false)
{
  mUIContext = new PipUIContext();
}

// destructor
nsPKCS12Blob::~nsPKCS12Blob()
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return;
  }

  shutdown(ShutdownCalledFrom::Object);
}

// nsPKCS12Blob::SetToken
//
// Set the token to use for import/export
nsresult
nsPKCS12Blob::SetToken(nsIPK11Token *token)
{
 nsNSSShutDownPreventionLock locker;
 if (isAlreadyShutDown()) {
  return NS_ERROR_NOT_AVAILABLE;
 }
 nsresult rv = NS_OK;
 if (token) {
   mToken = token;
 } else {
   PK11SlotInfo *slot;
   rv = GetSlotWithMechanism(CKM_RSA_PKCS, mUIContext, &slot, locker);
   if (NS_FAILED(rv)) {
      mToken = 0;  
   } else {
     mToken = new nsPK11Token(slot);
     PK11_FreeSlot(slot);
   }
 }
 mTokenSet = true;
 return rv;
}

// nsPKCS12Blob::ImportFromFile
//
// Given a file handle, read a PKCS#12 blob from that file, decode it,
// and import the results into the token.
nsresult
nsPKCS12Blob::ImportFromFile(nsIFile *file)
{
  nsNSSShutDownPreventionLock locker;
  nsresult rv = NS_OK;

  if (!mToken) {
    if (!mTokenSet) {
      rv = SetToken(nullptr); // Ask the user to pick a slot
      if (NS_FAILED(rv)) {
        handleError(PIP_PKCS12_USER_CANCELED);
        return rv;
      }
    }
  }

  if (!mToken) {
    handleError(PIP_PKCS12_RESTORE_FAILED);
    return NS_ERROR_NOT_AVAILABLE;
  }

  // init slot
  rv = mToken->Login(true);
  if (NS_FAILED(rv)) return rv;
  
  RetryReason wantRetry;
  
  do {
    rv = ImportFromFileHelper(file, im_standard_prompt, wantRetry);
    
    if (NS_SUCCEEDED(rv) && wantRetry == rr_auto_retry_empty_password_flavors)
    {
      rv = ImportFromFileHelper(file, im_try_zero_length_secitem, wantRetry);
    }
  }
  while (NS_SUCCEEDED(rv) && (wantRetry != rr_do_not_retry));
  
  return rv;
}

nsresult
nsPKCS12Blob::ImportFromFileHelper(nsIFile *file, 
                                   nsPKCS12Blob::ImportMode aImportMode,
                                   nsPKCS12Blob::RetryReason &aWantRetry)
{
  nsNSSShutDownPreventionLock locker;
  nsresult rv = NS_OK;
  SECStatus srv = SECSuccess;
  SEC_PKCS12DecoderContext *dcx = nullptr;
  SECItem unicodePw;

  UniquePK11SlotInfo slot;
  nsXPIDLString tokenName;
  unicodePw.data = nullptr;

  aWantRetry = rr_do_not_retry;

  if (aImportMode == im_try_zero_length_secitem)
  {
    unicodePw.len = 0;
  }
  else
  {
    // get file password (unicode)
    rv = getPKCS12FilePassword(&unicodePw);
    if (NS_FAILED(rv)) goto finish;
    if (!unicodePw.data) {
      handleError(PIP_PKCS12_USER_CANCELED);
      return NS_OK;
    }
  }

  mToken->GetTokenName(getter_Copies(tokenName));
  {
    NS_ConvertUTF16toUTF8 tokenNameCString(tokenName);
    slot = UniquePK11SlotInfo(PK11_FindSlotByName(tokenNameCString.get()));
  }
  if (!slot) {
    srv = SECFailure;
    goto finish;
  }

  // initialize the decoder
  dcx = SEC_PKCS12DecoderStart(&unicodePw, slot.get(), nullptr, nullptr,
                               nullptr, nullptr, nullptr, nullptr);
  if (!dcx) {
    srv = SECFailure;
    goto finish;
  }
  // read input file and feed it to the decoder
  rv = inputToDecoder(dcx, file);
  if (NS_FAILED(rv)) {
    if (NS_ERROR_ABORT == rv) {
      // inputToDecoder indicated a NSS error
      srv = SECFailure;
    }
    goto finish;
  }
  // verify the blob
  srv = SEC_PKCS12DecoderVerify(dcx);
  if (srv) goto finish;
  // validate bags
  srv = SEC_PKCS12DecoderValidateBags(dcx, nickname_collision);
  if (srv) goto finish;
  // import cert and key
  srv = SEC_PKCS12DecoderImportBags(dcx);
  if (srv) goto finish;
  // Later - check to see if this should become default email cert
  handleError(PIP_PKCS12_RESTORE_OK);
finish:
  // If srv != SECSuccess, NSS probably set a specific error code.
  // We should use that error code instead of inventing a new one
  // for every error possible.
  if (srv != SECSuccess) {
    if (SEC_ERROR_BAD_PASSWORD == PORT_GetError()) {
      if (unicodePw.len == sizeof(char16_t))
      {
        // no password chars available, 
        // unicodeToItem allocated space for the trailing zero character only.
        aWantRetry = rr_auto_retry_empty_password_flavors;
      }
      else
      {
        aWantRetry = rr_bad_password;
        handleError(PIP_PKCS12_NSS_ERROR);
      }
    }
    else
    {
      handleError(PIP_PKCS12_NSS_ERROR);
    }
  } else if (NS_FAILED(rv)) {
    handleError(PIP_PKCS12_RESTORE_FAILED);
  }
  // finish the decoder
  if (dcx)
    SEC_PKCS12DecoderFinish(dcx);
  SECITEM_ZfreeItem(&unicodePw, false);
  return NS_OK;
}

static bool
isExtractable(SECKEYPrivateKey *privKey)
{
  ScopedAutoSECItem value;
  SECStatus rv = PK11_ReadRawAttribute(PK11_TypePrivKey, privKey,
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

// nsPKCS12Blob::ExportToFile
//
// Having already loaded the certs, form them into a blob (loading the keys
// also), encode the blob, and stuff it into the file.
//
// TODO: handle slots correctly
//       mirror "slotToUse" behavior from PSM 1.x
//       verify the cert array to start off with?
//       open output file as nsIFileStream object?
//       set appropriate error codes
nsresult
nsPKCS12Blob::ExportToFile(nsIFile *file, 
                           nsIX509Cert **certs, int numCerts)
{
  nsNSSShutDownPreventionLock locker;
  nsresult rv;
  SECStatus srv = SECSuccess;
  SEC_PKCS12ExportContext *ecx = nullptr;
  SEC_PKCS12SafeInfo *certSafe = nullptr, *keySafe = nullptr;
  SECItem unicodePw;
  nsAutoString filePath;
  int i;
  nsCOMPtr<nsIFile> localFileRef;
  NS_ASSERTION(mToken, "Need to set the token before exporting");
  // init slot

  bool InformedUserNoSmartcardBackup = false;
  int numCertsExported = 0;

  rv = mToken->Login(true);
  if (NS_FAILED(rv)) goto finish;
  // get file password (unicode)
  unicodePw.data = nullptr;
  rv = newPKCS12FilePassword(&unicodePw);
  if (NS_FAILED(rv)) goto finish;
  if (!unicodePw.data) {
    handleError(PIP_PKCS12_USER_CANCELED);
    return NS_OK;
  }
  // what about slotToUse in psm 1.x ???
  // create export context
  ecx = SEC_PKCS12CreateExportContext(nullptr, nullptr, nullptr /*slot*/, nullptr);
  if (!ecx) {
    srv = SECFailure;
    goto finish;
  }
  // add password integrity
  srv = SEC_PKCS12AddPasswordIntegrity(ecx, &unicodePw, SEC_OID_SHA1);
  if (srv) goto finish;
  for (i=0; i<numCerts; i++) {
    nsNSSCertificate *cert = (nsNSSCertificate *)certs[i];
    // get it as a CERTCertificate XXX
    UniqueCERTCertificate nssCert(cert->GetCert());
    if (!nssCert) {
      rv = NS_ERROR_FAILURE;
      goto finish;
    }
    // We can only successfully export certs that are on 
    // internal token.  Most, if not all, smart card vendors
    // won't let you extract the private key (in any way
    // shape or form) from the card.  So let's punt if 
    // the cert is not in the internal db.
    if (nssCert->slot && !PK11_IsInternal(nssCert->slot)) {
      // we aren't the internal token, see if the key is extractable.
      SECKEYPrivateKey *privKey=PK11_FindKeyByDERCert(nssCert->slot,
                                                      nssCert.get(), this);

      if (privKey) {
        bool privKeyIsExtractable = isExtractable(privKey);

        SECKEY_DestroyPrivateKey(privKey);

        if (!privKeyIsExtractable) {
          if (!InformedUserNoSmartcardBackup) {
            InformedUserNoSmartcardBackup = true;
            handleError(PIP_PKCS12_NOSMARTCARD_EXPORT);
          }
          continue;
        }
      }
    }

    // XXX this is why, to verify the slot is the same
    // PK11_FindObjectForCert(nssCert, nullptr, slot);
    // create the cert and key safes
    keySafe = SEC_PKCS12CreateUnencryptedSafe(ecx);
    if (!SEC_PKCS12IsEncryptionAllowed() || PK11_IsFIPS()) {
      certSafe = keySafe;
    } else {
      certSafe = SEC_PKCS12CreatePasswordPrivSafe(ecx, &unicodePw,
                           SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_40_BIT_RC2_CBC);
    }
    if (!certSafe || !keySafe) {
      rv = NS_ERROR_FAILURE;
      goto finish;
    }
    // add the cert and key to the blob
    srv = SEC_PKCS12AddCertAndKey(ecx, certSafe, nullptr, nssCert.get(),
                                  CERT_GetDefaultCertDB(), // XXX
                                  keySafe, nullptr, true, &unicodePw,
                      SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_3KEY_TRIPLE_DES_CBC);
    if (srv) goto finish;
    // cert was dup'ed, so release it
    ++numCertsExported;
  }
  
  if (!numCertsExported) goto finish;
  
  // prepare the instance to write to an export file
  this->mTmpFile = nullptr;
  file->GetPath(filePath);
  // Use the nsCOMPtr var localFileRef so that
  // the reference to the nsIFile we create gets released as soon as
  // we're out of scope, ie when this function exits.
  if (filePath.RFind(".p12", true, -1, 4) < 0) {
    // We're going to add the .p12 extension to the file name just like
    // Communicator used to.  We create a new nsIFile and initialize
    // it with the new patch.
    filePath.AppendLiteral(".p12");
    localFileRef = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) goto finish;
    localFileRef->InitWithPath(filePath);
    file = localFileRef;
  }
  rv = file->OpenNSPRFileDesc(PR_RDWR|PR_CREATE_FILE|PR_TRUNCATE, 0664, 
                              &mTmpFile);
  if (NS_FAILED(rv) || !this->mTmpFile) goto finish;
  // encode and write
  srv = SEC_PKCS12Encode(ecx, write_export_file, this);
  if (srv) goto finish;
  handleError(PIP_PKCS12_BACKUP_OK);
finish:
  if (NS_FAILED(rv) || srv != SECSuccess) {
    handleError(PIP_PKCS12_BACKUP_FAILED);
  }
  if (ecx)
    SEC_PKCS12DestroyExportContext(ecx);
  if (this->mTmpFile) {
    PR_Close(this->mTmpFile);
    this->mTmpFile = nullptr;
  }
  SECITEM_ZfreeItem(&unicodePw, false);
  return rv;
}

///////////////////////////////////////////////////////////////////////
//
//  private members
//
///////////////////////////////////////////////////////////////////////

// unicodeToItem
//
// For the NSS PKCS#12 library, must convert PRUnichars (shorts) to
// a buffer of octets.  Must handle byte order correctly.
nsresult
nsPKCS12Blob::unicodeToItem(const char16_t *uni, SECItem *item)
{
  uint32_t len = NS_strlen(uni) + 1;
  if (!SECITEM_AllocItem(nullptr, item, sizeof(char16_t) * len)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  mozilla::NativeEndian::copyAndSwapToBigEndian(item->data, uni, len);

  return NS_OK;
}

// newPKCS12FilePassword
//
// Launch a dialog requesting the user for a new PKCS#12 file passowrd.
// Handle user canceled by returning null password (caller must catch).
nsresult
nsPKCS12Blob::newPKCS12FilePassword(SECItem *unicodePw)
{
  nsresult rv = NS_OK;
  nsAutoString password;
  nsCOMPtr<nsICertificateDialogs> certDialogs;
  rv = ::getNSSDialogs(getter_AddRefs(certDialogs), 
                       NS_GET_IID(nsICertificateDialogs),
                       NS_CERTIFICATEDIALOGS_CONTRACTID);
  if (NS_FAILED(rv)) return rv;
  bool pressedOK;
  rv = certDialogs->SetPKCS12FilePassword(mUIContext, password, &pressedOK);
  if (NS_FAILED(rv) || !pressedOK) return rv;
  return unicodeToItem(password.get(), unicodePw);
}

// getPKCS12FilePassword
//
// Launch a dialog requesting the user for the password to a PKCS#12 file.
// Handle user canceled by returning null password (caller must catch).
nsresult
nsPKCS12Blob::getPKCS12FilePassword(SECItem *unicodePw)
{
  nsresult rv = NS_OK;
  nsAutoString password;
  nsCOMPtr<nsICertificateDialogs> certDialogs;
  rv = ::getNSSDialogs(getter_AddRefs(certDialogs), 
                       NS_GET_IID(nsICertificateDialogs),
                       NS_CERTIFICATEDIALOGS_CONTRACTID);
  if (NS_FAILED(rv)) return rv;
  bool pressedOK;
  rv = certDialogs->GetPKCS12FilePassword(mUIContext, password, &pressedOK);
  if (NS_FAILED(rv) || !pressedOK) return rv;
  return unicodeToItem(password.get(), unicodePw);
}

// inputToDecoder
//
// Given a decoder, read bytes from file and input them to the decoder.
nsresult
nsPKCS12Blob::inputToDecoder(SEC_PKCS12DecoderContext *dcx, nsIFile *file)
{
  nsNSSShutDownPreventionLock locker;
  nsresult rv;
  SECStatus srv;
  uint32_t amount;
  char buf[PIP_PKCS12_BUFFER_SIZE];

  nsCOMPtr<nsIInputStream> fileStream;
  rv = NS_NewLocalFileInputStream(getter_AddRefs(fileStream), file);
  
  if (NS_FAILED(rv)) {
    return rv;
  }

  while (true) {
    rv = fileStream->Read(buf, PIP_PKCS12_BUFFER_SIZE, &amount);
    if (NS_FAILED(rv)) {
      return rv;
    }
    // feed the file data into the decoder
    srv = SEC_PKCS12DecoderUpdate(dcx, 
				  (unsigned char*) buf, 
				  amount);
    if (srv) {
      // don't allow the close call to overwrite our precious error code
      int pr_err = PORT_GetError();
      PORT_SetError(pr_err);
      return NS_ERROR_ABORT;
    }
    if (amount < PIP_PKCS12_BUFFER_SIZE)
      break;
  }
  return NS_OK;
}

// nickname_collision
// what to do when the nickname collides with one already in the db.
// TODO: not handled, throw a dialog allowing the nick to be changed?
SECItem *
nsPKCS12Blob::nickname_collision(SECItem *oldNick, PRBool *cancel, void *wincx)
{
  static NS_DEFINE_CID(kNSSComponentCID, NS_NSSCOMPONENT_CID);

  nsNSSShutDownPreventionLock locker;
  *cancel = false;
  nsresult rv;
  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));
  if (NS_FAILED(rv)) return nullptr;
  int count = 1;
  nsCString nickname;
  nsAutoString nickFromProp;
  nssComponent->GetPIPNSSBundleString("P12DefaultNickname", nickFromProp);
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
    UniqueCERTCertificate cert(CERT_FindCertByNickname(CERT_GetDefaultCertDB(),
                                                       nickname.get()));
    if (!cert) {
      break;
    }
    count++;
  }
  SECItem *newNick = new SECItem;
  if (!newNick)
    return nullptr;

  newNick->type = siAsciiString;
  newNick->data = (unsigned char*) strdup(nickname.get());
  newNick->len  = strlen((char*)newNick->data);
  return newNick;
}

// write_export_file
// write bytes to the exported PKCS#12 file
void
nsPKCS12Blob::write_export_file(void *arg, const char *buf, unsigned long len)
{
  nsPKCS12Blob *cx = (nsPKCS12Blob *)arg;
  PR_Write(cx->mTmpFile, buf, len);
}

// pip_ucs2_ascii_conversion_fn
// required to be set by NSS (to do PKCS#12), but since we've already got
// unicode make this a no-op.
PRBool
pip_ucs2_ascii_conversion_fn(PRBool toUnicode,
                             unsigned char *inBuf,
                             unsigned int inBufLen,
                             unsigned char *outBuf,
                             unsigned int maxOutBufLen,
                             unsigned int *outBufLen,
                             PRBool swapBytes)
{
  // do a no-op, since I've already got unicode.  Hah!
  *outBufLen = inBufLen;
  memcpy(outBuf, inBuf, inBufLen);
  return true;
}

void
nsPKCS12Blob::handleError(int myerr)
{
  static NS_DEFINE_CID(kNSSComponentCID, NS_NSSCOMPONENT_CID);

  if (!NS_IsMainThread()) {
    NS_ERROR("nsPKCS12Blob::handleError called off the mai nthread.");
    return;
  }

  int prerr = PORT_GetError();
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("PKCS12: NSS/NSPR error(%d)", prerr));
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("PKCS12: I called(%d)", myerr));

  const char * msgID = nullptr;

  switch (myerr) {
  case PIP_PKCS12_RESTORE_OK:       msgID = "SuccessfulP12Restore"; break;
  case PIP_PKCS12_BACKUP_OK:        msgID = "SuccessfulP12Backup";  break;
  case PIP_PKCS12_USER_CANCELED:
    return;  /* Just ignore it for now */
  case PIP_PKCS12_NOSMARTCARD_EXPORT: msgID = "PKCS12InfoNoSmartcardBackup"; break;
  case PIP_PKCS12_RESTORE_FAILED:   msgID = "PKCS12UnknownErrRestore"; break;
  case PIP_PKCS12_BACKUP_FAILED:    msgID = "PKCS12UnknownErrBackup"; break;
  case PIP_PKCS12_NSS_ERROR:
    switch (prerr) {
    // The following errors have the potential to be "handled", by asking
    // the user (via a dialog) whether s/he wishes to continue
    case 0: break;
    case SEC_ERROR_PKCS12_CERT_COLLISION:
      /* pop a dialog saying the cert is already in the database */
      /* ask to keep going?  what happens if one collision but others ok? */
      // The following errors cannot be "handled", notify the user (via an alert)
      // that the operation failed.
    case SEC_ERROR_BAD_PASSWORD: msgID = "PK11BadPassword"; break;

    case SEC_ERROR_BAD_DER:
    case SEC_ERROR_PKCS12_CORRUPT_PFX_STRUCTURE:
    case SEC_ERROR_PKCS12_INVALID_MAC:
      msgID = "PKCS12DecodeErr";
      break;

    case SEC_ERROR_PKCS12_DUPLICATE_DATA: msgID = "PKCS12DupData"; break;
    }
    break;
  }

  if (!msgID)
    msgID = "PKCS12UnknownErr";

  nsresult rv;
  nsCOMPtr<nsINSSComponent> nssComponent = do_GetService(kNSSComponentCID, &rv);
  if (NS_SUCCEEDED(rv))
    (void) nssComponent->ShowAlertFromStringBundle(msgID);
}
