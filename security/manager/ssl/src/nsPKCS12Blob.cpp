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
 * $Id: nsPKCS12Blob.cpp,v 1.1 2001/03/20 18:00:43 mcgreer%netscape.com Exp $
 */

#include "prmem.h"

#include "nsISupportsArray.h"
#include "nsIFileSpec.h"
#include "nsINSSDialogs.h"
#include "nsIDirectoryService.h"

// XXX remove this once slot login works correctly
#include "nsNSSHelper.h"

#include "nsPKCS12Blob.h"
#include "nsString.h"
#include "nsFileStream.h"
#include "nsXPIDLString.h"
#include "nsDirectoryServiceDefs.h"
#include "nsNSSHelper.h"
#include "nsNSSCertificate.h"

#include "pk11func.h"

#ifdef PR_LOGGING
extern PRLogModuleInfo* gPIPNSSLog;
#endif

#define PIP_PKCS12_TMPFILENAME ".p12tmp"

/* XXX temporary -- allows login to slot */
char * 
pk11PasswordPrompt(PK11SlotInfo *slot, PRBool retry, void *arg)
{
  nsresult rv = NS_OK;
  PRUnichar *tokenName;
  PRUnichar *password;
  PRBool canceled;
  int *retries = (int *)arg;
  if ((*retries)++ >= 3) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("failed after 3 retries"));
    return NULL;
  }
  tokenName = NS_ConvertUTF8toUCS2(PK11_GetTokenName(slot)).get();
  nsCOMPtr<nsIInterfaceRequestor> uiContext = new PipUIContext();
  nsCOMPtr<nsITokenPasswordDialogs> tokenDialogs;
  rv = ::getNSSDialogs(getter_AddRefs(tokenDialogs), 
                       NS_GET_IID(nsITokenPasswordDialogs));
  if (NS_FAILED(rv))
    return NULL;
  rv = tokenDialogs->GetPassword(uiContext, tokenName, &password, &canceled);
  if (NS_FAILED(rv) || canceled)
    return NULL;
  const char *pass = NS_ConvertUCS2toUTF8(password).get();
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("password is: \"%s\"\n", pass));
  return PL_strdup(pass);
}

nsPKCS12Blob::nsPKCS12Blob() : mCertArray(nsnull), mToken(nsnull)
{
  __mTmpFilePath = NULL;
}

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
    // free the old one, how?
    mToken = getter_AddRefs(token);
  } else {
    // set it to internal token
    nsCOMPtr<nsIPK11TokenDB> tokendb = 
                                  do_GetService(NS_PK11TOKENDB_CONTRACTID);
    tokendb->GetInternalKeyToken(getter_AddRefs(mToken));
  }
}

// nsPKCS12Blob::ImportFromFile
//
// Given a file handle, read a PKCS#12 blob from that file, decode it,
// and import the results into the token.
//
// TODO:  handle slots correctly (needs changes in nsPK11Token)
//        set appropriate error values for dialogs
nsresult
nsPKCS12Blob::ImportFromFile(nsILocalFile *file)
{
  nsresult rv;
  SECStatus srv;
  SEC_PKCS12DecoderContext *dcx = NULL;
  PK11SlotInfo *slot = NULL;
  SECItem unicodePw;
  if (!mToken)
    SetToken(NULL); // uses internal slot
  // XXX fix this later by getting it from mToken
  int count = 0;
  slot = PK11_GetInternalKeySlot();
  PK11_Authenticate(slot, PR_TRUE, &count);
#if 0
  // init slot
  rv = mToken->Login(PR_TRUE);
  if (NS_FAILED(rv)) {
    goto finish;
  }
  slot = mToken->GetSlot();
#endif
  // get file password (unicode)
  unicodePw.data = NULL;
  rv = getPKCS12FilePassword(&unicodePw);
  if (NS_FAILED(rv)) {
    goto finish;
  }
  int prerr;
  rv = NS_ERROR_FAILURE;
  // initialize the decoder
  dcx = SEC_PKCS12DecoderStart(&unicodePw, slot, NULL,
                               digest_open, digest_close,
                               digest_read, digest_write,
                               this);
  if (!dcx) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("FAILED starting decoder"));
    prerr = PORT_GetError();
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("got error %d", prerr));
    goto finish;
  }
  // read input file and feed it to the decoder
  rv = inputToDecoder(dcx, file);
  if (NS_FAILED(rv)) {
    goto finish;
  }
  rv = NS_ERROR_FAILURE;
  // verify the blob
  srv = SEC_PKCS12DecoderVerify(dcx);
  if (srv) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("FAILED verify decoder"));
    prerr = PORT_GetError();
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("got error %d", prerr));
    goto finish;
  }
  // validate bags
  srv = SEC_PKCS12DecoderValidateBags(dcx, nickname_collision);
  if (srv) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("FAILED validate bags"));
    prerr = PORT_GetError();
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("got error %d", prerr));
    goto finish;
  }
  // import cert and key
  srv = SEC_PKCS12DecoderImportBags(dcx);
  if (srv) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("FAILED import bags"));
    prerr = PORT_GetError();
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("got error %d", prerr));
    goto finish;
  }
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("finished with import"));
  // Later - check to see if this should become default email cert
  rv = NS_OK;
finish:
  // finish the decoder
  if (dcx)
    SEC_PKCS12DecoderFinish(dcx);
  return rv;
}

// nsPKCS12Blob::LoadCerts
//
// Given an array of certificate nicknames, load the corresponding
// certificates into a local array.
// TODO: Am I using nsISupportsArray correctly?
//       free memory correctly
//       set appropriate error values
nsresult
nsPKCS12Blob::LoadCerts(const PRUnichar **certNames, int numCerts)
{
  nsresult rv;
  if (mCertArray)
    return NS_ERROR_FAILURE;
  rv = NS_NewISupportsArray(getter_AddRefs(mCertArray));
  for (int i=0; i<numCerts; i++) {
    const char *name = NS_ConvertUCS2toUTF8(certNames[i]).get();
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("adding \"%s\" to backup list\n", name));
    char *namecpy = PL_strdup(name); /*sigh*/
    CERTCertificate *nssCert = PK11_FindCertFromNickname(namecpy, NULL);
    if (!nssCert)
      return NS_ERROR_FAILURE; // XXX could handle this better
    nsCOMPtr<nsIX509Cert> cert = new nsNSSCertificate(nssCert);
    if (!cert)
      return NS_ERROR_FAILURE; // XXX could handle this better
    mCertArray->AppendElement(cert);
    CERT_DestroyCertificate(nssCert);
    // clean up the string
  }
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("done adding certs"));
  return NS_OK;
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
nsPKCS12Blob::ExportToFile(nsILocalFile *file)
{
  nsresult rv, nrv;
  SECStatus srv;
  SEC_PKCS12ExportContext *ecx = NULL;
  SEC_PKCS12SafeInfo *certSafe = NULL, *keySafe = NULL;
  PK11SlotInfo *slot = NULL;
  SECItem unicodePw;
  PRUint32 i, numCerts;
  if (!mToken)
    SetToken(NULL); // uses internal slot
  // XXX fix this later by getting it from mToken
  int count = 0;
  slot = PK11_GetInternalKeySlot();
  PK11_Authenticate(slot, PR_TRUE, &count);
#if 0
  // init slot
  rv = mToken->Login(PR_TRUE);
  if (NS_FAILED(rv)) {
    goto finish;
  }
  slot = mToken->GetSlot();
#endif
  // get file password (unicode)
  unicodePw.data = NULL;
  rv = newPKCS12FilePassword(&unicodePw);
  if (NS_FAILED(rv)) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("FAILED creating new password"));
    goto finish;
  }
  // what about slotToUse in psm 1.x ???
  // what are these params???
  ecx = SEC_PKCS12CreateExportContext(NULL, NULL, slot, NULL);
  if (!ecx) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("FAILED creating export context"));
    goto finish;
  }
  rv = NS_ERROR_FAILURE;
  srv = SEC_PKCS12AddPasswordIntegrity(ecx, &unicodePw, SEC_OID_SHA1);
  if (srv) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("FAILED adding password integrity"));
    goto finish;
  }
  nrv = mCertArray->Count(&numCerts);
  if (NS_FAILED(nrv)) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("FAILED counting array"));
    goto finish;
  }
  for (i=0; i<numCerts; i++) {
    nsCOMPtr<nsNSSCertificate> cert;
    nrv = mCertArray->GetElementAt(i, getter_AddRefs(cert));
    if (NS_FAILED(nrv)) {
      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("FAILED getting el %d", i));
      goto finish;
    }
    // XXX this is why, to verify the slot is the same
    // PK11_FindObjectForCert(cert->GetCert(), NULL, slot);
    keySafe = SEC_PKCS12CreateUnencryptedSafe(ecx);
    if (!SEC_PKCS12IsEncryptionAllowed() || PK11_IsFIPS()) {
      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("cheapo encryption", i));
      certSafe = keySafe;
    } else {
      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("powering up", i));
      certSafe = SEC_PKCS12CreatePasswordPrivSafe(ecx, &unicodePw,
                           SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_40_BIT_RC2_CBC);
    }
    if (!certSafe || !keySafe) {
      int prerr = PORT_GetError();
      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("got error %d", prerr));
      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("FAILED creating safes"));
      goto finish;
    }
    srv = SEC_PKCS12AddCertAndKey(ecx, certSafe, NULL, cert->GetCert(),
                                  CERT_GetDefaultCertDB(),
                                  keySafe, NULL, PR_TRUE, &unicodePw,
                      SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_3KEY_TRIPLE_DES_CBC);
    if (srv) {
      int prerr = PORT_GetError();
      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("got error %d", prerr));
      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("FAILED adding cert and key"));
      goto finish;
    }
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("added %s", cert->GetCert()->nickname));
  }
  //  XXX cheating
  this->__mTmp = NULL;
  rv = file->OpenNSPRFileDesc(PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE, 0600,
                              &this->__mTmp);
  if (NS_FAILED(rv)) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("FAILED opening file"));
    goto finish;
  }
  srv = SEC_PKCS12Encode(ecx, write_export_file, this);
  if (srv) {
    int prerr = PORT_GetError();
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("got error %d", prerr));
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("FAILED encoding"));
    goto finish;
  }
  rv = NS_OK;
finish:
  if (ecx)
    SEC_PKCS12DestroyExportContext(ecx);
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
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("Unicode PW:\n"));
  for (i=0; i<(int)item->len; i++)
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("%x\n", item->data[i]));
}

// newPKCS12FilePassword
//
// Launch a dialog requesting the user for a password for a PKCS#12 file.
nsresult
nsPKCS12Blob::newPKCS12FilePassword(SECItem *unicodePw)
{
  nsresult rv = NS_OK;
  PRUnichar *password;
  PRBool canceled;
  nsCOMPtr<nsIInterfaceRequestor> uiContext = new PipUIContext();
  nsCOMPtr<nsICertificateDialogs> certDialogs;
  rv = ::getNSSDialogs(getter_AddRefs(certDialogs), 
                       NS_GET_IID(nsICertificateDialogs));
  if (NS_FAILED(rv)) return rv;
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("got dialog"));
  rv = certDialogs->SetPKCS12FilePassword(uiContext, &password, &canceled);
  if (NS_FAILED(rv) || canceled) return rv;
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("got pass"));
#ifdef DEBUG_mcgreer
  // XXX Aye, why does encode take UTF8 but decode doesn't???
  const char *pass = NS_ConvertUCS2toUTF8(password).get();
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("password is: \"%s\"\n", pass));
#endif
  unicodeToItem(password, unicodePw);
  return NS_OK;
}

// getPKCS12FilePassword
//
// Launch a dialog requesting the user for the password to a PKCS#12 file.
nsresult
nsPKCS12Blob::getPKCS12FilePassword(SECItem *unicodePw)
{
  nsresult rv = NS_OK;
  PRUnichar *password;
  PRBool canceled;
  nsCOMPtr<nsIInterfaceRequestor> uiContext = new PipUIContext();
  nsCOMPtr<nsICertificateDialogs> certDialogs;
  rv = ::getNSSDialogs(getter_AddRefs(certDialogs), 
                       NS_GET_IID(nsICertificateDialogs));
  if (NS_FAILED(rv)) return rv;
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("got dialog"));
  rv = certDialogs->GetPKCS12FilePassword(uiContext, &password, &canceled);
  if (NS_FAILED(rv) || canceled) return rv;
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("got pass"));
#ifdef DEBUG_mcgreer
  // XXX Aye, why does encode take UTF8 but decode doesn't???
  const char *pass = NS_ConvertUCS2toUTF8(password).get();
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("password is: \"%s\"\n", pass));
#endif
  unicodeToItem(password, unicodePw);
  return NS_OK;
}

//#define PIP_PKCS12_BUFFER_SIZE 2048
#define PIP_PKCS12_BUFFER_SIZE 16384

// inputToDecoder
//
// Given a decoder, read bytes from file and input them to the decoder.
// TODO: should I be closing the file?  Done in the destructor, no?
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
#ifdef DEBUG_mcgreer
    char *foo = strdup(pathBuf.get());
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("opened %s", foo));
#endif
    rv = NS_NewFileSpec(getter_AddRefs(tempSpec));
    if (NS_FAILED(rv)) return rv;
    rv = tempSpec->SetNativePath(pathBuf);
    if (NS_FAILED(rv)) return rv;
  }
  nsInputFileStream fileStream(tempSpec);
  while (PR_TRUE) {
    amount = fileStream.read(buf, PIP_PKCS12_BUFFER_SIZE);
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("read %d bytes from file", amount));
    if (amount < 0)
      return NS_ERROR_FAILURE;
    // feed the file data into the decoder
    srv = SEC_PKCS12DecoderUpdate(dcx, buf, amount);
    if (srv) {
      int prerr = PORT_GetError();
      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("got error %d", prerr));
      return NS_ERROR_FAILURE;
    }
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("fed %d bytes to decoder", amount));
    if (amount < PIP_PKCS12_BUFFER_SIZE)
      break;
  }
  return NS_OK;
}

//
// C callback methods
//

// digest_open
// open a temporary file for reading/writing digests
// TODO: use mozilla file i/o?
SECStatus
nsPKCS12Blob::digest_open(void *arg, PRBool reading)
{
  nsPKCS12Blob *cx = (nsPKCS12Blob *)arg;
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("digest open"));
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
    cx->__mTmpFilePath = PL_strdup(pathBuf.get());
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("opened temp %s", cx->__mTmpFilePath));
#if 0
      rv = tmpFile->CreateUnique(PIP_PKCS12_TMPFILENAME, 
                                 nsIFile::NORMAL_FILE_TYPE, 0700);
#endif
  }
#if 0
  // everybody else is doin' it
  nsCOMPtr<nsIFileSpec> tempSpec;
  {
    nsXPIDLCString pathBuf;
    tmpFile->GetPath(getter_Copies(pathBuf));
#ifdef DEBUG_mcgreer
    char *foo = strdup(pathBuf.get());
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("opened temp %s", foo));
#endif
    rv = NS_NewFileSpec(getter_AddRefs(tempSpec));
    if (NS_FAILED(rv)) return SECFailure;
    rv = tempSpec->SetNativePath(pathBuf);
    if (NS_FAILED(rv)) return SECFailure;
  }
  nsInputFileStream fileStream(tempSpec);
#endif
  // Open the file using NSPR
  if (reading) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("for reading"));
    cx->__mTmp = PR_Open(cx->__mTmpFilePath, PR_RDONLY, 0400);
  } else {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("for writing"));
    cx->__mTmp = PR_Open(cx->__mTmpFilePath, 
                         PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE, 0600);
  }
  if (!cx->__mTmp)
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("failed to open digest"));
  return (cx->__mTmp != NULL) ? SECSuccess : SECFailure;
}

// digest_close
// close the temp file opened above
// TODO: use mozilla file i/o?
SECStatus
nsPKCS12Blob::digest_close(void *arg, PRBool remove)
{
  nsPKCS12Blob *cx = (nsPKCS12Blob *)arg;
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("digest close"));
  PR_Close(cx->__mTmp);
  if (remove) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("deleting temp file"));
    PR_Delete(cx->__mTmpFilePath);
    PR_Free(cx->__mTmpFilePath);
    cx->__mTmpFilePath = NULL;
  }
  return SECSuccess;
}

// digest_read
// read bytes from the temp digest file
// TODO: use mozilla file i/o?
int
nsPKCS12Blob::digest_read(void *arg, unsigned char *buf, unsigned long len)
{
  nsPKCS12Blob *cx = (nsPKCS12Blob *)arg;
  return PR_Read(cx->__mTmp, buf, len);
#if 0
  nsresult rv;
  PRUint32 amount, offset;
  offset = 0;
  do {
    rv = cx->mDigestInput->Read(buf + offset, len - offset, &amount);
    if (amount == 0)
      break;
    if (NS_FAILED(rv))
      return -1;
    len -= amount;
    offset += amount;
  } while (len > 0);
  return amount;
#endif
}

// digest_write
// write bytes to the temp digest file
// TODO: use mozilla file i/o?
int
nsPKCS12Blob::digest_write(void *arg, unsigned char *buf, unsigned long len)
{
  nsPKCS12Blob *cx = (nsPKCS12Blob *)arg;
  return PR_Write(cx->__mTmp, buf, len);
#if 0
  PRUint32 amount, offset;
  offset = 0;
  do {
    rv = cx->mDigestOutput->Write(buf + offset, len - offset, &amount);
    if (amount == 0)
      break;
    if (NS_FAILED(rv))
      return -1;
    len -= amount;
    offset += amount;
  } while (len > 0);
#endif
}

// nickname_collision
// what to do when the nickname collides with one already in the db.
// TODO: not handled, throw a dialog allowing the nick to be changed?
SECItem *
nsPKCS12Blob::nickname_collision(SECItem *oldNick, PRBool *cancel, void *wincx)
{
  // not handled yet (wasn't in psm 1.x either)
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("pkcs12 nickname collision"));
  *cancel = PR_TRUE;
  return NULL;
}

// write_export_file
// write bytes to the exported PKCS#12 file
// TODO: use mozilla file i/o?
void
nsPKCS12Blob::write_export_file(void *arg, const char *buf, unsigned long len)
{
  nsPKCS12Blob *cx = (nsPKCS12Blob *)arg;
  // XXX in collusion with cheaters
  PRUint32 nb = PR_Write(cx->__mTmp, buf, len);
}

// pip_ucs2_ascii_conversion_fn
// required to be set by NSS (to do PKCS#12), but since we've already got
// unicode make this a no-op.
// TODO: I'm not leaking a few bytes, am I?
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
#if 0
  // even worse -- the buffer has already been over-alloc'ed.  But I
  // *know* that it didn't use an arena, so I can safely shrink it
  // to prevent a (admittedly small) leak.
  outBuf = (unsigned char *)PORT_Realloc(outBuf, inBufLen);
  // there's no leak -- free doesn't care what item->len is ..., right?
#endif
  *outBufLen = inBufLen;
  memcpy(outBuf, inBuf, inBufLen);
  return PR_TRUE;
}
