/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 *  Ian McGreer <mcgreer@netscape.com>
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 *
 * $Id: nsPKCS12Blob.cpp,v 1.13 2001/06/24 19:15:59 mcgreer%netscape.com Exp $
 */

#include "prmem.h"

#include "nsISupportsArray.h"
#include "nsIFileSpec.h"
#include "nsINSSDialogs.h"
#include "nsIDirectoryService.h"
#include "nsIWindowWatcher.h"
#include "nsIPrompt.h"
#include "nsProxiedService.h"

#include "nsNSSComponent.h"
#include "nsNSSHelper.h"
#include "nsPKCS12Blob.h"
#include "nsString.h"
#include "nsFileStream.h"
#include "nsXPIDLString.h"
#include "nsDirectoryServiceDefs.h"
#include "nsNSSHelper.h"
#include "nsNSSCertificate.h"
#include "nsKeygenHandler.h" //For GetSlotWithMechanism
#include "nsPK11TokenDB.h"

#include "pk11func.h"
#include "secerr.h"

#ifdef PR_LOGGING
extern PRLogModuleInfo* gPIPNSSLog;
#endif

static NS_DEFINE_CID(kNSSComponentCID, NS_NSSCOMPONENT_CID);

#define PIP_PKCS12_TMPFILENAME   ".pip_p12tmp"
#define PIP_PKCS12_BUFFER_SIZE   2048
#define PIP_PKCS12_RESTORE_OK    1
#define PIP_PKCS12_BACKUP_OK     2
#define PIP_PKCS12_USER_CANCELED 3

// constructor
nsPKCS12Blob::nsPKCS12Blob():mCertArray(0),
                             mTmpFile(nsnull),
                             mTmpFilePath(nsnull),
                             mTokenSet(PR_FALSE)
{
  mUIContext = new PipUIContext();
}

// destructor
nsPKCS12Blob::~nsPKCS12Blob()
{
}

// nsPKCS12Blob::SetToken
//
// Set the token to use for import/export
void 
nsPKCS12Blob::SetToken(nsIPK11Token *token)
{
 if (token) {
   mToken = token;
 } else {
   PK11SlotInfo *slot;
   nsresult rv = GetSlotWithMechanism(CKM_RSA_PKCS, mUIContext,&slot);
   if (NS_FAILED(rv)) {
      mToken = 0;  
   } else {
     mToken = new nsPK11Token(slot);
   }
 }
 mTokenSet = PR_TRUE;
}

// nsPKCS12Blob::ImportFromFile
//
// Given a file handle, read a PKCS#12 blob from that file, decode it,
// and import the results into the token.
nsresult
nsPKCS12Blob::ImportFromFile(nsILocalFile *file)
{
  nsresult rv;
  SECStatus srv = SECSuccess;
  SEC_PKCS12DecoderContext *dcx = NULL;
  SECItem unicodePw;

  PK11SlotInfo *slot=nsnull;
  nsXPIDLString tokenName;
  nsXPIDLCString tokenNameCString;
  const char *tokNameRef;

  if (!mToken && !mTokenSet) {
    SetToken(NULL); // Ask the user to pick a slot
  } else if (!mToken && mTokenSet) {
    // Someone tried setting the token before, but that failed.
    // Probably because the user canceled.
    handleError(PIP_PKCS12_USER_CANCELED);
    return NS_OK;
  }
  // init slot
  rv = mToken->Login(PR_TRUE);
  if (NS_FAILED(rv)) goto finish;
  // get file password (unicode)
  unicodePw.data = NULL;
  rv = getPKCS12FilePassword(&unicodePw);
  if (NS_FAILED(rv)) goto finish;
  if (unicodePw.data == NULL) {
    handleError(PIP_PKCS12_USER_CANCELED);
    return NS_OK;
  }

  mToken->GetTokenName(getter_Copies(tokenName));
  tokenNameCString.Adopt(NS_ConvertUCS2toUTF8(tokenName).ToNewCString());
  tokNameRef = tokenNameCString; //I do this here so that the
                                 //NS_CONST_CAST below doesn't
                                 //break the build on Win32

  slot = PK11_FindSlotByName(NS_CONST_CAST(char*,tokNameRef));
  if (!slot) {
    srv = SECFailure;
    goto finish;
  }

  // initialize the decoder
  dcx = SEC_PKCS12DecoderStart(&unicodePw, slot, NULL,
                               digest_open, digest_close,
                               digest_read, digest_write,
                               this);
  if (!dcx) {
    srv = SECFailure;
    goto finish;
  }
  // read input file and feed it to the decoder
  rv = inputToDecoder(dcx, file);
  if (NS_FAILED(rv)) goto finish;
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
  if (NS_FAILED(rv) || srv != SECSuccess) {
    handleError();
  }
  // finish the decoder
  if (dcx)
    SEC_PKCS12DecoderFinish(dcx);
  return NS_OK;
}

#if 0
// nsPKCS12Blob::LoadCerts
//
// Given an array of certificate nicknames, load the corresponding
// certificates into a local array.
nsresult
nsPKCS12Blob::LoadCerts(const PRUnichar **certNames, int numCerts)
{
  nsresult rv;
  char namecpy[256];
  /* Create the local array if needed */
  if (!mCertArray) {
    rv = NS_NewISupportsArray(getter_AddRefs(mCertArray));
    if (NS_FAILED(rv)) {
      if (!handleError())
        return NS_ERROR_OUT_OF_MEMORY;
    }
  }
  /* Add the certs */
  for (int i=0; i<numCerts; i++) {
    const char *name = NS_ConvertUCS2toUTF8(certNames[i]).get();
    strcpy(namecpy, name);
    CERTCertificate *nssCert = PK11_FindCertFromNickname(namecpy, NULL);
    if (!nssCert) {
      if (!handleError())
        return NS_ERROR_FAILURE;
      else continue; /* user may request to keep going */
    }
    nsCOMPtr<nsIX509Cert> cert = new nsNSSCertificate(nssCert);
    if (!cert) {
      if (!handleError())
        return NS_ERROR_OUT_OF_MEMORY;
    } else {
      mCertArray->AppendElement(cert);
    }
    CERT_DestroyCertificate(nssCert);
  }
  return NS_OK;
}
#endif

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
nsPKCS12Blob::ExportToFile(nsILocalFile *file, 
                           nsIX509Cert **certs, int numCerts)
{
  nsresult rv;
  SECStatus srv;
  SEC_PKCS12ExportContext *ecx = NULL;
  SEC_PKCS12SafeInfo *certSafe = NULL, *keySafe = NULL;
  SECItem unicodePw;
  nsXPIDLCString xpidlFilePath;
  nsAutoString filePath;
  int i;
  NS_ASSERTION(mToken, "Need to set the token before exporting");
  // init slot
  rv = mToken->Login(PR_TRUE);
  if (NS_FAILED(rv)) goto finish;
  // get file password (unicode)
  unicodePw.data = NULL;
  rv = newPKCS12FilePassword(&unicodePw);
  if (NS_FAILED(rv)) goto finish;
  if (unicodePw.data == NULL) {
    handleError(PIP_PKCS12_USER_CANCELED);
    return NS_OK;
  }
  // what about slotToUse in psm 1.x ???
  // create export context
  ecx = SEC_PKCS12CreateExportContext(NULL, NULL, NULL /*slot*/, NULL);
  if (!ecx) {
    srv = SECFailure;
    goto finish;
  }
  // add password integrity
  srv = SEC_PKCS12AddPasswordIntegrity(ecx, &unicodePw, SEC_OID_SHA1);
  if (srv) goto finish;
#if 0
  // count the number of certs to export
  nrv = mCertArray->Count(&numCerts);
  if (NS_FAILED(nrv)) goto finish;
  // loop over the certs
  for (i=0; i<numCerts; i++) {
    nsCOMPtr<nsIX509Cert> cert;
    nrv = mCertArray->GetElementAt(i, getter_AddRefs(cert));
    if (NS_FAILED(nrv)) goto finish;
#endif
  for (i=0; i<numCerts; i++) {
//    nsNSSCertificate *cert = NS_REINTREPRET_POINTER_CAST(nsNSSCertificate *,
//                                                         certs[i]);
    nsNSSCertificate *cert = (nsNSSCertificate *)certs[i];
    // get it as a CERTCertificate XXX
    CERTCertificate *nssCert = NULL;
    nssCert = cert->GetCert();
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
      CERT_DestroyCertificate(nssCert);
      continue;
    }

    // XXX this is why, to verify the slot is the same
    // PK11_FindObjectForCert(nssCert, NULL, slot);
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
    srv = SEC_PKCS12AddCertAndKey(ecx, certSafe, NULL, nssCert,
                                  CERT_GetDefaultCertDB(), // XXX
                                  keySafe, NULL, PR_TRUE, &unicodePw,
                      SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_3KEY_TRIPLE_DES_CBC);
    if (srv) goto finish;
    // cert was dup'ed, so release it
    CERT_DestroyCertificate(nssCert);
  }
  // prepare the instance to write to an export file
  this->mTmpFile = NULL;
  file->GetPath(getter_Copies(xpidlFilePath));
  filePath.AssignWithConversion(xpidlFilePath);
  if (filePath.RFind(".p12", PR_TRUE, -1, 4) < 0) {
    filePath.AppendWithConversion(".p12");
  }
  this->mTmpFile = PR_Open(NS_ConvertUCS2toUTF8(filePath.get()).get(),
                           PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE, 0600);
  if (!this->mTmpFile) goto finish;
  // encode and write
  srv = SEC_PKCS12Encode(ecx, write_export_file, this);
  if (srv) goto finish;
  handleError(PIP_PKCS12_BACKUP_OK);
finish:
  if (NS_FAILED(rv) || srv != SECSuccess) {
    handleError();
  }
  if (ecx)
    SEC_PKCS12DestroyExportContext(ecx);
  if (this->mTmpFile) {
    PR_Close(this->mTmpFile);
    this->mTmpFile = NULL;
  }
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
// TODO: Is there a mozilla way to do this?  In the string lib?
void
nsPKCS12Blob::unicodeToItem(PRUnichar *uni, SECItem *item)
{
  int len = 0;
  int i = 0;
  while (uni[len++] != 0);
  SECITEM_AllocItem(NULL, item, sizeof(PRUnichar) * len);
#ifdef IS_LITTLE_ENDIAN
  for (i=0; i<len; i++) {
    item->data[2*i  ] = (unsigned char )(uni[i] << 8);
    item->data[2*i+1] = (unsigned char )(uni[i]);
  }
#else
  memcpy(item->data, uni, item->len);
#endif
}

// newPKCS12FilePassword
//
// Launch a dialog requesting the user for a new PKCS#12 file passowrd.
// Handle user canceled by returning null password (caller must catch).
nsresult
nsPKCS12Blob::newPKCS12FilePassword(SECItem *unicodePw)
{
  nsresult rv = NS_OK;
  PRUnichar *password;
  PRBool canceled;
  nsCOMPtr<nsICertificateDialogs> certDialogs;
  rv = ::getNSSDialogs(getter_AddRefs(certDialogs), 
                       NS_GET_IID(nsICertificateDialogs));
  if (NS_FAILED(rv)) return rv;
  rv = certDialogs->SetPKCS12FilePassword(mUIContext, &password, &canceled);
  if (NS_FAILED(rv) || canceled) return rv;
  unicodeToItem(password, unicodePw);
  return NS_OK;
}

// getPKCS12FilePassword
//
// Launch a dialog requesting the user for the password to a PKCS#12 file.
// Handle user canceled by returning null password (caller must catch).
nsresult
nsPKCS12Blob::getPKCS12FilePassword(SECItem *unicodePw)
{
  nsresult rv = NS_OK;
  PRUnichar *password;
  PRBool canceled;
  nsCOMPtr<nsICertificateDialogs> certDialogs;
  rv = ::getNSSDialogs(getter_AddRefs(certDialogs), 
                       NS_GET_IID(nsICertificateDialogs));
  if (NS_FAILED(rv)) return rv;
  rv = certDialogs->GetPKCS12FilePassword(mUIContext, &password, &canceled);
  if (NS_FAILED(rv) || canceled) return rv;
  unicodeToItem(password, unicodePw);
  return NS_OK;
}

// inputToDecoder
//
// Given a decoder, read bytes from file and input them to the decoder.
nsresult
nsPKCS12Blob::inputToDecoder(SEC_PKCS12DecoderContext *dcx, nsILocalFile *file)
{
  nsresult rv;
  SECStatus srv;
  PRUint32 amount;
  unsigned char buf[PIP_PKCS12_BUFFER_SIZE];
  // everybody else is doin' it
  nsCOMPtr<nsIFileSpec> tempSpec;
  {
    nsXPIDLCString pathBuf;
    file->GetPath(getter_Copies(pathBuf));
    rv = NS_NewFileSpec(getter_AddRefs(tempSpec));
    if (NS_FAILED(rv)) return rv;
    rv = tempSpec->SetNativePath(pathBuf);
    if (NS_FAILED(rv)) return rv;
  }
  nsInputFileStream fileStream(tempSpec);
  while (PR_TRUE) {
    amount = fileStream.read(buf, PIP_PKCS12_BUFFER_SIZE);
    if (amount < 0) {
      fileStream.close();
      return NS_ERROR_FAILURE;
    }
    // feed the file data into the decoder
    srv = SEC_PKCS12DecoderUpdate(dcx, buf, amount);
    if (srv) {
      fileStream.close();
      return NS_ERROR_FAILURE;
    }
    if (amount < PIP_PKCS12_BUFFER_SIZE)
      break;
  }
  fileStream.close();
  return NS_OK;
}

//
// C callback methods
//

// digest_open
// open a temporary file for reading/writing digests
SECStatus PR_CALLBACK
nsPKCS12Blob::digest_open(void *arg, PRBool reading)
{
  nsPKCS12Blob *cx = (nsPKCS12Blob *)arg;
  nsresult rv;
  // use DirectoryService to find the system temp directory
  nsCOMPtr<nsILocalFile> tmpFile;
  NS_WITH_SERVICE(nsIProperties, directoryService, 
                                 NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return SECFailure;
  directoryService->Get(NS_OS_TEMP_DIR, 
                        NS_GET_IID(nsIFile),
                        getter_AddRefs(tmpFile));
  if (tmpFile) {
    tmpFile->Append(PIP_PKCS12_TMPFILENAME);
    nsXPIDLCString pathBuf;
    tmpFile->GetPath(getter_Copies(pathBuf));
    cx->mTmpFilePath = PL_strdup(pathBuf.get());
#ifdef XP_MAC
    char *unixPath = nsnull;
    ConvertMacPathToUnixPath(cx->mTmpFilePath, &unixPath);
    nsMemory::Free(cx->mTmpFilePath);
    cx->mTmpFilePath = unixPath;
#endif    
  }
  // Open the file using NSPR
  if (reading) {
    cx->mTmpFile = PR_Open(cx->mTmpFilePath, PR_RDONLY, 0400);
  } else {
    cx->mTmpFile = PR_Open(cx->mTmpFilePath, 
                           PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE, 0600);
  }
  return (cx->mTmpFile != NULL) ? SECSuccess : SECFailure;
}

// digest_close
// close the temp file opened above
SECStatus PR_CALLBACK
nsPKCS12Blob::digest_close(void *arg, PRBool remove_it)
{
  nsPKCS12Blob *cx = (nsPKCS12Blob *)arg;
  PR_Close(cx->mTmpFile);
  if (remove_it) {
    PR_Delete(cx->mTmpFilePath);
    PR_Free(cx->mTmpFilePath);
    cx->mTmpFilePath = NULL;
  }
  cx->mTmpFile = NULL;
  return SECSuccess;
}

// digest_read
// read bytes from the temp digest file
int PR_CALLBACK
nsPKCS12Blob::digest_read(void *arg, unsigned char *buf, unsigned long len)
{
  nsPKCS12Blob *cx = (nsPKCS12Blob *)arg;
  return PR_Read(cx->mTmpFile, buf, len);
}

// digest_write
// write bytes to the temp digest file
int PR_CALLBACK
nsPKCS12Blob::digest_write(void *arg, unsigned char *buf, unsigned long len)
{
  nsPKCS12Blob *cx = (nsPKCS12Blob *)arg;
  return PR_Write(cx->mTmpFile, buf, len);
}

// nickname_collision
// what to do when the nickname collides with one already in the db.
// TODO: not handled, throw a dialog allowing the nick to be changed?
SECItem * PR_CALLBACK
nsPKCS12Blob::nickname_collision(SECItem *oldNick, PRBool *cancel, void *wincx)
{
  // not handled yet (wasn't in psm 1.x either)
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("pkcs12 nickname collision"));
  *cancel = PR_TRUE;
  return NULL;
}

// write_export_file
// write bytes to the exported PKCS#12 file
void PR_CALLBACK
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
  return PR_TRUE;
}

#define kWindowWatcherCID "@mozilla.org/embedcomp/window-watcher;1"

PRBool
nsPKCS12Blob::handleError(int myerr)
{
  nsresult rv;
  PRBool keepGoing = PR_FALSE;
  int prerr = PORT_GetError();
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("PKCS12: NSS/NSPR error(%d)", prerr));
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("PKCS12: I called(%d)", myerr));
  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));
  if (NS_FAILED(rv)) return PR_FALSE;
  nsCOMPtr<nsIProxyObjectManager> proxyman(
                                      do_GetService(NS_XPCOMPROXY_CONTRACTID));
  if (!proxyman) return PR_FALSE;
  nsCOMPtr<nsIPrompt> errPrompt;
  nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(kWindowWatcherCID));
  if (wwatch) {
    wwatch->GetNewPrompter(0, getter_AddRefs(errPrompt));
    if (errPrompt) {
      nsCOMPtr<nsIPrompt> proxyPrompt;
      proxyman->GetProxyForObject(NS_UI_THREAD_EVENTQ, NS_GET_IID(nsIPrompt),
                                  errPrompt, PROXY_SYNC, 
                                  getter_AddRefs(proxyPrompt));
      if (!proxyPrompt) return PR_FALSE;
    } else {
      return PR_FALSE;
    }
  } else {
    return PR_FALSE;
  }
  nsAutoString errorMsg;
  switch (myerr) {
  case PIP_PKCS12_RESTORE_OK:
    rv = nssComponent->GetPIPNSSBundleString(
                              NS_LITERAL_STRING("SuccessfulP12Restore").get(), 
                              errorMsg);
    if (NS_FAILED(rv)) return rv;
    errPrompt->Alert(nsnull, errorMsg.GetUnicode());
    return PR_TRUE;
  case PIP_PKCS12_BACKUP_OK:
    rv = nssComponent->GetPIPNSSBundleString(
                              NS_LITERAL_STRING("SuccessfulP12Backup").get(), 
                              errorMsg);
    if (NS_FAILED(rv)) return rv;
    errPrompt->Alert(nsnull, errorMsg.GetUnicode());
    return PR_TRUE;
  case PIP_PKCS12_USER_CANCELED:
    return PR_TRUE;  /* Just ignore it for now */
  case 0: 
  default:
    break;
  }
  switch (prerr) {
  // The following errors have the potential to be "handled", by asking
  // the user (via a dialog) whether s/he wishes to continue
  case 0: break;
  case SEC_ERROR_PKCS12_CERT_COLLISION:
    /* pop a dialog saying the cert is already in the database */
    /* ask to keep going?  what happens if one collision but others ok? */
  // The following errors cannot be "handled", notify the user (via an alert)
  // that the operation failed.
#if 0
// XXX a boy can dream...
//     but the PKCS12 lib never throws this error
//     but then again, how would it?  anyway, convey the info below
  case SEC_ERROR_PKCS12_PRIVACY_PASSWORD_INCORRECT:
    rv = nssComponent->GetPIPNSSBundleString(
                              NS_LITERAL_STRING("PKCS12PasswordInvalid").get(), 
                              errorMsg);
    if (NS_FAILED(rv)) return rv;
    errPrompt->Alert(nsnull, errorMsg.GetUnicode());
    break;
#endif
  case SEC_ERROR_BAD_PASSWORD:
    rv = nssComponent->GetPIPNSSBundleString(
                            NS_LITERAL_STRING("PK11BadPassword").get(), 
                            errorMsg);
    if (NS_FAILED(rv)) return rv;
    errPrompt->Alert(nsnull, errorMsg.GetUnicode());
    break;
  case SEC_ERROR_BAD_DER:
  case SEC_ERROR_PKCS12_CORRUPT_PFX_STRUCTURE:
  case SEC_ERROR_PKCS12_INVALID_MAC:
    rv = nssComponent->GetPIPNSSBundleString(
                            NS_LITERAL_STRING("PKCS12DecodeErr").get(), 
                            errorMsg);
    if (NS_FAILED(rv)) return rv;
    errPrompt->Alert(nsnull, errorMsg.GetUnicode());
    break;
  default:
    rv = nssComponent->GetPIPNSSBundleString(
                            NS_LITERAL_STRING("PKCS12UnknownErrRestore").get(), 
                            errorMsg);
    if (NS_FAILED(rv)) return rv;
    errPrompt->Alert(nsnull, errorMsg.GetUnicode());
  }
  if (NS_FAILED(rv)) return rv;
  return keepGoing;
}

