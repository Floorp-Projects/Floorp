/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Hubbie Shaw
 *   Doug Turner <dougt@netscape.com>
 *   Mitch Stoltz <mstoltz@netscape.com>
 *   Brian Ryner <bryner@netscape.com>
 */

#include "nsNSSComponent.h"
#include "nsNSSCallbacks.h"
#include "nsNSSIOLayer.h"

#include "nsNetUtil.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryService.h"
#include "nsIStreamListener.h"
#include "nsIStringBundle.h"
#include "nsIDirectoryService.h"
#include "nsCURILoader.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIProxyObjectManager.h"
#include "nsINSSDialogs.h"
#include "nsIX509Cert.h"
#include "nsIX509CertDB.h"
#include "nsIProfileChangeStatus.h"
#include "nsNSSCertificate.h"
#include "nsNSSHelper.h"
#include "prlog.h"

#include "nsIWindowWatcher.h"
#include "nsIPrompt.h"
#include "nsProxiedService.h"
#include "nsICertificatePrincipal.h"
#include "nsReadableUtils.h"

#include "nss.h"
#include "pk11func.h"
#include "ssl.h"
#include "sslproto.h"
#include "secmod.h"
#include "sechash.h"
#include "secmime.h"
#include "ocsp.h"
extern "C" {
#include "pkcs11.h"
#include "pkcs12.h"
#include "p12plcy.h"
}

#ifdef PR_LOGGING
PRLogModuleInfo* gPIPNSSLog = nsnull;
#endif

PRBool nsNSSComponent::mNSSInitialized = PR_FALSE;

#ifdef XP_MAC

OSErr ConvertMacPathToUnixPath(const char *macPath, char **unixPath)
{
  PRIntn len;
  char *cursor;
  
  len = PL_strlen(macPath);
  cursor = (char*)PR_Malloc(len+2);
  if (!cursor)
    return memFullErr;
    
  memcpy(cursor+1, macPath, len+1);
  *unixPath = cursor;
  *cursor = '/';
  while ((cursor = PL_strchr(cursor, ':')) != NULL) {
    *cursor = '/';
    cursor++;
  }
  return noErr;
}
#endif

// XXX tmp callback for slot password
extern char * pk11PasswordPrompt(PK11SlotInfo *slot, PRBool retry, void *arg);

#define PIPNSS_STRBUNDLE_URL "chrome://pipnss/locale/pipnss.properties"


static PLHashNumber PR_CALLBACK certHashtable_keyHash(const void *key)
{
  if (!key)
    return 0;
  
  SECItem *certKey = (SECItem*)key;
  
  // lazy hash function, sum up all char values of SECItem
  
  PLHashNumber hash = 0;
  unsigned int i = 0;
  unsigned char *c = certKey->data;
  
  for (i = 0; i < certKey->len; ++i, ++c) {
    hash += *c;
  }
  
  return hash;
}

static PRIntn PR_CALLBACK certHashtable_keyCompare(const void *k1, const void *k2)
{
  // return type is a bool, answering the question "are the keys equal?"

  if (!k1 || !k2)
    return PR_FALSE;
  
  SECItem *certKey1 = (SECItem*)k1;
  SECItem *certKey2 = (SECItem*)k2;
  
  if (certKey1->len != certKey2->len) {
    return PR_FALSE;
  }
  
  unsigned int i = 0;
  unsigned char *c1 = certKey1->data;
  unsigned char *c2 = certKey2->data;
  
  for (i = 0; i < certKey1->len; ++i, ++c1, ++c2) {
    if (*c1 != *c2) {
      return PR_FALSE;
    }
  }
  
  return PR_TRUE;
}

static PRIntn PR_CALLBACK certHashtable_valueCompare(const void *v1, const void *v2)
{
  // two values are identical if their keys are identical
  
  if (!v1 || !v2)
    return PR_FALSE;
  
  CERTCertificate *cert1 = (CERTCertificate*)v1;
  CERTCertificate *cert2 = (CERTCertificate*)v2;
  
  return certHashtable_keyCompare(&cert1->certKey, &cert2->certKey);
}

static PRIntn PR_CALLBACK certHashtable_clearEntry(PLHashEntry *he, PRIntn /*index*/, void * /*userdata*/)
{
  if (he && he->value) {
    CERT_DestroyCertificate((CERTCertificate*)he->value);
  }
  
  return HT_ENUMERATE_NEXT;
}

nsNSSComponent::nsNSSComponent()
{
  NS_INIT_ISUPPORTS();

  hashTableCerts = PL_NewHashTable( 0, certHashtable_keyHash, certHashtable_keyCompare, 
    certHashtable_valueCompare, 0, 0 );
}

nsNSSComponent::~nsNSSComponent()
{
  if (mPSMContentListener) {
    nsresult rv = NS_ERROR_FAILURE;
      
    nsCOMPtr<nsIURILoader> dispatcher(do_GetService(NS_URI_LOADER_CONTRACTID));
    if (dispatcher) {
      rv = dispatcher->UnRegisterContentListener(mPSMContentListener);
    }
  }
  if (mPref)
    mPref->UnregisterCallback("security.", nsNSSComponent::PrefChangedCallback,
                              (void*) this);

  if (hashTableCerts) {
    PL_HashTableEnumerateEntries(hashTableCerts, certHashtable_clearEntry, 0);
    PL_HashTableDestroy(hashTableCerts);
    hashTableCerts = 0;
  }

  if (mNSSInitialized)
    NSS_Shutdown();  

  nsSSLIOLayerFreeTLSIntolerantSites();
}

#ifdef XP_MAC
#ifdef DEBUG
#define LOADABLE_CERTS_MODULE "NSSckbiDebug.shlb"
#else
#define LOADABLE_CERTS_MODULE "NSSckbi.shlb"
#endif /*DEBUG*/ 
#endif /*XP_MAC*/

static void setOCSPOptions(nsIPref * pref);

NS_IMETHODIMP
nsNSSComponent::PIPBundleFormatStringFromName(const PRUnichar *name,
                                              const PRUnichar **params,
                                              PRUint32 numParams,
                                              PRUnichar **outString)
{
  nsresult rv = NS_ERROR_FAILURE;

  if (mPIPNSSBundle && name) {
    rv = mPIPNSSBundle->FormatStringFromName(name, params, 
                                             numParams, outString);
  }
  return rv;
}

NS_IMETHODIMP
nsNSSComponent::GetPIPNSSBundleString(const PRUnichar *name,
                                      nsString &outString)
{
  PRUnichar *ptrv = nsnull;
  nsresult rv = NS_ERROR_FAILURE;

  outString.SetLength(0);
  if (mPIPNSSBundle && name) {
    rv = mPIPNSSBundle->GetStringFromName(name, &ptrv);
    if (NS_SUCCEEDED(rv)) {
      outString = ptrv;
      rv = NS_OK;
    }
  }
  if (ptrv)
    nsMemory::Free(ptrv);

  return rv;
}

NS_IMETHODIMP
nsNSSComponent::DisableOCSP()
{
  CERTCertDBHandle *certdb = CERT_GetDefaultCertDB();

  SECStatus rv = CERT_DisableOCSPChecking(certdb);
  return (rv == SECSuccess) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsNSSComponent::EnableOCSP()
{
  setOCSPOptions(mPref);
  return NS_OK;
}

void
nsNSSComponent::InstallLoadableRoots()
{
  PRBool hasRoot = PR_FALSE;
  PK11SlotListElement *listElement;
  PK11SlotList *slotList = PK11_GetAllTokens(CKM_INVALID_MECHANISM, 
                                             PR_FALSE, PR_FALSE, nsnull); 
  if (slotList) {
    for (listElement=slotList->head; listElement != NULL; 
         listElement = listElement->next) {
      if (PK11_HasRootCerts(listElement->slot)) {
        hasRoot = PR_TRUE;
        break;
      }    
    }
    PK11_FreeSlotList(slotList);
  }
  if (!hasRoot) {
    nsresult rv;
    nsString modName;
    rv = GetPIPNSSBundleString(NS_LITERAL_STRING("RootCertModuleName").get(),
                               modName);
    if (NS_FAILED(rv)) return;

    nsCOMPtr<nsILocalFile> mozFile;
    nsCOMPtr<nsIProperties> directoryService(do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID));
    if (!directoryService)
      return;
    
    directoryService->Get( NS_XPCOM_CURRENT_PROCESS_DIR,
                           NS_GET_IID(nsIFile), 
                           getter_AddRefs(mozFile));
    
    if (!mozFile) {
      return;
    }
    char *fullModuleName = nsnull;
#ifdef XP_MAC
    char *unixModulePath=nsnull;
    mozFile->Append("Essential Files");
    mozFile->Append(LOADABLE_CERTS_MODULE);
    mozFile->GetPath(&fullModuleName);
    ConvertMacPathToUnixPath(fullModuleName, &unixModulePath);
    nsMemory::Free(fullModuleName);
    fullModuleName = unixModulePath;
#else
    char *processDir = nsnull;
    mozFile->GetPath(&processDir);
    fullModuleName = PR_GetLibraryName(processDir, "nssckbi");
    nsMemory::Free(processDir);
#endif
    /* If a module exists with the same name, delete it. */
    char *modNameCString = ToNewCString(modName);
    int modType;
    SECMOD_DeleteModule(modNameCString, &modType);
    SECMOD_AddNewModule(modNameCString, fullModuleName, 0, 0);
    nsMemory::Free(fullModuleName);
    nsMemory::Free(modNameCString);
  }
}

#define SHORT_PK11_STRING 33
#define LONG_PK11_STRING  65

char *
nsNSSComponent::GetPK11String(const PRUnichar *name, PRUint32 len)
{
  nsresult rv;
  nsString nsstr;
  char *tmpstr = NULL;
  char *str = NULL;
  int tmplen;
  str = (char *)PR_Malloc(len+1);
  rv = GetPIPNSSBundleString(name, nsstr);
  if (NS_FAILED(rv)) return NULL;
  tmpstr = ToNewCString(nsstr);
  if (!tmpstr) return NULL;
  tmplen = strlen(tmpstr);
  if (len > tmplen) {
    memcpy(str, tmpstr, tmplen);
    memset(str + tmplen, ' ', len - tmplen);
  } else {
    memcpy(str, tmpstr, len);
  }
  str[len] = '\0';
  PR_Free(tmpstr);
  return str;
}

nsresult
nsNSSComponent::ConfigureInternalPKCS11Token()
{
  char *manufacturerID             = NULL;
  char *libraryDescription         = NULL;
  char *tokenDescription           = NULL;
  char *privateTokenDescription    = NULL;
  char *slotDescription            = NULL;
  char *privateSlotDescription     = NULL;
  char *fipsSlotDescription        = NULL;
  char *fipsPrivateSlotDescription = NULL; 

  manufacturerID = GetPK11String(NS_LITERAL_STRING("ManufacturerID").get(), 
                                 SHORT_PK11_STRING);
  if (manufacturerID == NULL) goto loser;
  libraryDescription = 
              GetPK11String(NS_LITERAL_STRING("LibraryDescription").get(), 
                            SHORT_PK11_STRING);
  if (libraryDescription == NULL) goto loser;
  tokenDescription = GetPK11String(NS_LITERAL_STRING("TokenDescription").get(), 
                                   SHORT_PK11_STRING);
  if (tokenDescription == NULL) goto loser;
  privateTokenDescription = 
              GetPK11String(NS_LITERAL_STRING("PrivateTokenDescription").get(), 
                            SHORT_PK11_STRING);
  if (privateTokenDescription == NULL) goto loser;
  slotDescription = GetPK11String(NS_LITERAL_STRING("SlotDescription").get(), 
                                  LONG_PK11_STRING);
  if (slotDescription == NULL) goto loser;
  privateSlotDescription = 
              GetPK11String(NS_LITERAL_STRING("PrivateSlotDescription").get(), 
                                   LONG_PK11_STRING);
  if (privateSlotDescription == NULL) goto loser;
  fipsSlotDescription = 
              GetPK11String(NS_LITERAL_STRING("FipsSlotDescription").get(),
                            LONG_PK11_STRING);
  if (fipsSlotDescription == NULL) goto loser;
  fipsPrivateSlotDescription = 
          GetPK11String(NS_LITERAL_STRING("FipsPrivateSlotDescription").get(), 
                        LONG_PK11_STRING);
  if (fipsPrivateSlotDescription == NULL) goto loser;

  PK11_ConfigurePKCS11(manufacturerID, libraryDescription, tokenDescription,
                       privateTokenDescription, slotDescription, 
                       privateSlotDescription, fipsSlotDescription, 
                       fipsPrivateSlotDescription, 0, 0);
  return NS_OK;
loser:
  PR_Free(manufacturerID);
  PR_Free(libraryDescription);
  PR_Free(tokenDescription);
  PR_Free(privateTokenDescription);
  PR_Free(slotDescription);
  PR_Free(privateSlotDescription);
  PR_Free(fipsSlotDescription);
  PR_Free(fipsPrivateSlotDescription); 
  return NS_ERROR_FAILURE;
}

nsresult
nsNSSComponent::InitializePIPNSSBundle()
{
  nsresult rv;
  nsCOMPtr<nsIStringBundleService> bundleService(do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv));
  if (NS_FAILED(rv) || !bundleService) 
    return NS_ERROR_FAILURE;
  
  bundleService->CreateBundle(PIPNSS_STRBUNDLE_URL,
                              getter_AddRefs(mPIPNSSBundle));
  if (!mPIPNSSBundle)
    rv = NS_ERROR_FAILURE;

  return rv;
}

nsresult
nsNSSComponent::RegisterPSMContentListener()
{
  nsresult rv = NS_OK;
  if (!mPSMContentListener) {
    nsCOMPtr<nsIURILoader> dispatcher(do_GetService(NS_URI_LOADER_CONTRACTID));
    if (dispatcher) {
      mPSMContentListener = do_CreateInstance(NS_PSMCONTENTLISTEN_CONTRACTID);
      rv = dispatcher->RegisterContentListener(mPSMContentListener);
    }
  }
  return rv;
}

/* Table of pref names and SSL cipher ID */
typedef struct {
  char* pref;
  long id;
} CipherPref;

static CipherPref CipherPrefs[] = {
/* SSL2 ciphers */
 {"security.ssl2.rc4_128", SSL_EN_RC4_128_WITH_MD5},
 {"security.ssl2.rc2_128", SSL_EN_RC2_128_CBC_WITH_MD5},
 {"security.ssl2.des_ede3_192", SSL_EN_DES_192_EDE3_CBC_WITH_MD5},
 {"security.ssl2.des_64", SSL_EN_DES_64_CBC_WITH_MD5},
 {"security.ssl2.rc4_40", SSL_EN_RC4_128_EXPORT40_WITH_MD5},
 {"security.ssl2.rc2_40", SSL_EN_RC2_128_CBC_EXPORT40_WITH_MD5},
 /* SSL3 ciphers */
 {"security.ssl3.fortezza_fortezza_sha", SSL_FORTEZZA_DMS_WITH_FORTEZZA_CBC_SHA},
 {"security.ssl3.fortezza_rc4_sha", SSL_FORTEZZA_DMS_WITH_RC4_128_SHA},
 {"security.ssl3.rsa_rc4_128_md5", SSL_RSA_WITH_RC4_128_MD5},
 {"security.ssl3.rsa_fips_des_ede3_sha", SSL_RSA_FIPS_WITH_3DES_EDE_CBC_SHA},
 {"security.ssl3.rsa_des_ede3_sha", SSL_RSA_WITH_3DES_EDE_CBC_SHA},
 {"security.ssl3.rsa_fips_des_sha", SSL_RSA_FIPS_WITH_DES_CBC_SHA},
 {"security.ssl3.rsa_des_sha", SSL_RSA_WITH_DES_CBC_SHA},
 {"security.ssl3.rsa_1024_rc4_56_sha", TLS_RSA_EXPORT1024_WITH_RC4_56_SHA},
 {"security.ssl3.rsa_1024_des_cbc_sha", TLS_RSA_EXPORT1024_WITH_DES_CBC_SHA},
 {"security.ssl3.rsa_rc4_40_md5", SSL_RSA_EXPORT_WITH_RC4_40_MD5},
 {"security.ssl3.rsa_rc2_40_md5", SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5},
 {"security.ssl3.fortezza_null_sha", SSL_FORTEZZA_DMS_WITH_NULL_SHA},
 {"security.ssl3.rsa_null_md5", SSL_RSA_WITH_NULL_MD5},
 {NULL, 0} /* end marker */
};

static void setOCSPOptions(nsIPref * pref)
{
  // Set up OCSP //
  PRInt32 ocspEnabled;
  pref->GetIntPref("security.OCSP.enabled", &ocspEnabled);
  switch (ocspEnabled) {
  case 0:
	  CERT_DisableOCSPChecking(CERT_GetDefaultCertDB());
	  CERT_DisableOCSPDefaultResponder(CERT_GetDefaultCertDB());
	  break;
  case 1:
    CERT_EnableOCSPChecking(CERT_GetDefaultCertDB());
    break;
  case 2:
    {
      char *signingCA = nsnull;
      char *url = nsnull;

      // Get the signing CA and service url //
      pref->CopyCharPref("security.OCSP.signingCA", &signingCA);
      pref->CopyCharPref("security.OCSP.URL", &url);

      // Set OCSP up
      CERT_EnableOCSPChecking(CERT_GetDefaultCertDB());
      CERT_SetOCSPDefaultResponder(CERT_GetDefaultCertDB(), url, signingCA);
      CERT_EnableOCSPDefaultResponder(CERT_GetDefaultCertDB());

      nsMemory::Free(signingCA);
      nsMemory::Free(url);
    }
	  break;
  }
}

nsresult
nsNSSComponent::InitializeNSS()
{
  nsresult rv;
  nsXPIDLCString profileStr;
  nsCOMPtr<nsIFile> profilePath;

  if (mNSSInitialized) {
    PR_ASSERT(!"Trying to initialize NSS twice"); // We should never try to 
                                                  // initialize NSS more than
                                                  // once in a process.
    return NS_ERROR_FAILURE;
  }

  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("NSS Initialization beginning\n"));
  rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                              getter_AddRefs(profilePath));
  if (NS_FAILED(rv)) {
    PR_LOG(gPIPNSSLog, PR_LOG_ERROR, ("Unable to get profile directory\n"));
    return rv;
  }
    
  PK11_SetPasswordFunc(PK11PasswordPrompt);
#ifdef XP_MAC
  // On the Mac we place all NSS DBs in the Security
  // Folder in the profile directory.
  profilePath->Append("Security");
  profilePath->Create(nsIFile::DIRECTORY_TYPE, 0); //This is for Mac, don't worry about
                                                   //permissions.
#endif 

  rv = profilePath->GetPath(getter_Copies(profileStr));
  if (NS_FAILED(rv)) 
    return rv;
  
  if (NSS_InitReadWrite(profileStr) != SECSuccess) {
    return NS_ERROR_ABORT;
  }
  
  NSS_SetDomesticPolicy();
  //  SSL_EnableCipher(SSL_RSA_WITH_NULL_MD5, SSL_ALLOWED);

  mPref = do_GetService(NS_PREF_CONTRACTID);

  // Register a callback so we can inform NSS when these prefs change
  mPref->RegisterCallback("security.", nsNSSComponent::PrefChangedCallback,
                          (void*) this);

  PRBool enabled;
  mPref->GetBoolPref("security.enable_ssl2", &enabled);
  SSL_OptionSetDefault(SSL_ENABLE_SSL2, enabled);
  mPref->GetBoolPref("security.enable_ssl3", &enabled);
  SSL_OptionSetDefault(SSL_ENABLE_SSL3, enabled);
  mPref->GetBoolPref("security.enable_tls", &enabled);
  SSL_OptionSetDefault(SSL_ENABLE_TLS, enabled);

  // Set SSL/TLS ciphers
  for (CipherPref* cp = CipherPrefs; cp->pref; ++cp) {
    mPref->GetBoolPref(cp->pref, &enabled);

    SSL_CipherPrefSetDefault(cp->id, enabled);
  }

  // Enable ciphers for PKCS#12
  SEC_PKCS12EnableCipher(PKCS12_RC4_40, 1);
  SEC_PKCS12EnableCipher(PKCS12_RC4_128, 1);
  SEC_PKCS12EnableCipher(PKCS12_RC2_CBC_40, 1);
  SEC_PKCS12EnableCipher(PKCS12_RC2_CBC_128, 1);
  SEC_PKCS12EnableCipher(PKCS12_DES_56, 1);
  SEC_PKCS12EnableCipher(PKCS12_DES_EDE3_168, 1);
  SEC_PKCS12SetPreferredCipher(PKCS12_DES_EDE3_168, 1);
  PORT_SetUCS2_ASCIIConversionFunction(pip_ucs2_ascii_conversion_fn);

  // Set up OCSP //
  setOCSPOptions(mPref);

  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("NSS Initialization done\n"));
  mNSSInitialized = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSComponent::Init()
{
  nsresult rv = NS_OK;
#ifdef PR_LOGGING
  if (!gPIPNSSLog)
    gPIPNSSLog = PR_NewLogModule("pipnss");
#endif

  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("Beginning NSS initialization\n"));
  rv = InitializePIPNSSBundle();
  if (NS_FAILED(rv)) {
    PR_LOG(gPIPNSSLog, PR_LOG_ERROR, ("Unable to create pipnss bundle.\n"));
    return rv;
  }      
  ConfigureInternalPKCS11Token();
  rv = InitializeNSS();
  if (NS_FAILED(rv)) {
    PR_LOG(gPIPNSSLog, PR_LOG_ERROR, ("Unable to Initialize NSS.\n"));
    return rv;
  }
  InstallLoadableRoots();
  RegisterPSMContentListener();
  RegisterProfileChangeObserver();
  return rv;
}

/* nsISupports Implementation for the class */
NS_IMPL_THREADSAFE_ISUPPORTS6(nsNSSComponent,
                              nsISecurityManagerComponent,
                              nsISignatureVerifier,
                              nsIEntropyCollector,
                              nsINSSComponent,
                              nsIObserver,
                              nsISupportsWeakReference);

NS_IMETHODIMP
nsNSSComponent::DisplaySecurityAdvisor()
{
  return NS_ERROR_NOT_IMPLEMENTED; // not implemented
}


//---------------------------------------------
// Functions Implementing nsISignatureVerifier
//---------------------------------------------
NS_IMETHODIMP
nsNSSComponent::HashBegin(PRUint32 alg, HASHContext** id)
{
  *id = HASH_Create((HASH_HashType)alg);
  if (*id) {
    HASH_Begin(*id);
    return NS_OK; 
  } else {
    return NS_ERROR_FAILURE;
  }
}

NS_IMETHODIMP
nsNSSComponent::HashUpdate(HASHContext* ctx, const char* buf, PRUint32 buflen)
{
  HASH_Update(ctx, (const unsigned char*)buf, buflen);
  return NS_OK; 
}

NS_IMETHODIMP
nsNSSComponent::HashEnd(HASHContext* ctx, unsigned char** hash, 
                        PRUint32* hashLen, PRUint32 maxLen)
{
  HASH_End(ctx, *hash, hashLen, maxLen);
  HASH_Destroy(ctx);
  return NS_OK;
}

NS_IMETHODIMP
nsNSSComponent::GetPassword(char **aRet)
{
  // This functionality is only used in wallet.
  // This interface can go away once we get rid of PSM 1.x.
  *aRet = nsnull;
  return NS_OK;
}

/* Callback functions for decoder. For now, use empty/default functions. */
static void ContentCallback(void *arg, 
                                           const char *buf,
                                           unsigned long len)
{
}

static PK11SymKey * GetDecryptKeyCallback(void *arg, 
                                                 SECAlgorithmID *algid)
{
  return nsnull;
}

static PRBool DecryptionAllowedCallback(SECAlgorithmID *algid,  
                                               PK11SymKey *bulkkey)
{
  return SECMIME_DecryptionAllowed(algid, bulkkey);
}

static SECItem * GetPasswordKeyCallback(void *arg,
                                               SECKEYKeyDBHandle *handle)
{
  return NULL;
}

NS_IMETHODIMP
nsNSSComponent::VerifySignature(const char* aRSABuf, PRUint32 aRSABufLen,
                                const char* aPlaintext, PRUint32 aPlaintextLen,
                                PRInt32* aErrorCode,
                                nsIPrincipal** aPrincipal)
{
  SEC_PKCS7DecoderContext * p7_ctxt = nsnull;
  SEC_PKCS7ContentInfo * p7_info = nsnull; 
  unsigned char hash[SHA1_LENGTH]; 
  PRBool rv;

  if (!aPrincipal || !aErrorCode)
    return NS_ERROR_NULL_POINTER;
  *aErrorCode = 0;
  *aPrincipal = nsnull;

  p7_ctxt = SEC_PKCS7DecoderStart(ContentCallback,
                        nsnull,
                        GetPasswordKeyCallback,
                        nsnull,
                        GetDecryptKeyCallback,
                        nsnull,
                        DecryptionAllowedCallback);
  if (!p7_ctxt) {
    return NS_ERROR_FAILURE;
  }

  if (SEC_PKCS7DecoderUpdate(p7_ctxt,aRSABuf, aRSABufLen) != SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  p7_info = SEC_PKCS7DecoderFinish(p7_ctxt); 
  if (!p7_info) {
    return NS_ERROR_FAILURE;
  }

  //-- If a plaintext was provided, hash it.
  SECItem digest;
  digest.data = nsnull;
  digest.len = 0;

  if (aPlaintext) {
    HASHContext* hash_ctxt;
    PRUint32 hashLen = 0;

    hash_ctxt = HASH_Create(HASH_AlgSHA1);
    HASH_Begin(hash_ctxt);
    HASH_Update(hash_ctxt,(const unsigned char*)aPlaintext, aPlaintextLen);
    HASH_End(hash_ctxt, hash, &hashLen, SHA1_LENGTH); 
    HASH_Destroy(hash_ctxt);

    digest.data = hash;
    digest.len = SHA1_LENGTH;
  }

  //-- Verify signature
  rv = SEC_PKCS7VerifyDetachedSignature(p7_info, certUsageObjectSigner, &digest, HASH_AlgSHA1, PR_TRUE);
  if (rv != PR_TRUE) {
    *aErrorCode = PR_GetError();
    return NS_OK;
  }

  // Get the signing cert //
  CERTCertificate *cert = p7_info->content.signedData->signerInfos[0]->cert;
  if (cert) {
    nsresult rv2;
    nsCOMPtr<nsIX509Cert> pCert = new nsNSSCertificate(cert);
    if (!mScriptSecurityManager) {
      mScriptSecurityManager = 
         do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv2);
      if (NS_FAILED(rv2)) return rv2;
    }
    //-- Create a certificate principal with id and organization data
    PRUnichar* fingerprint;
    rv2 = pCert->GetSha1Fingerprint(&fingerprint);
    nsCAutoString fingerprintStr;
    fingerprintStr.AssignWithConversion(fingerprint);
    PR_FREEIF(fingerprint);
    if (NS_FAILED(rv2)) return rv2;
    rv2 = mScriptSecurityManager->GetCertificatePrincipal(fingerprintStr, aPrincipal);
    if (NS_FAILED(rv2) || !*aPrincipal) return rv2;

    nsCOMPtr<nsICertificatePrincipal> certPrincipal = do_QueryInterface(*aPrincipal, &rv2);
    if (NS_FAILED(rv2)) return rv2;
    PRUnichar* orgName;
    rv2 = pCert->GetOrganization(&orgName);
    if (NS_FAILED(rv2)) return rv2;
    nsCAutoString orgNameStr;
    orgNameStr.AssignWithConversion(orgName);
    PR_FREEIF(orgName);
    rv2 = certPrincipal->SetCommonName(orgNameStr);
    if (NS_FAILED(rv2)) return rv2;
  }

  if (p7_info) {
    SEC_PKCS7DestroyContentInfo(p7_info);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNSSComponent::RandomUpdate(void *entropy, PRInt32 bufLen)
{
  if (!mNSSInitialized)
      return NS_ERROR_NOT_INITIALIZED;

  PK11_RandomUpdate(entropy, bufLen);
  return NS_OK;
}

int PR_CALLBACK
nsNSSComponent::PrefChangedCallback(const char* aPrefName, void* data)
{
  nsNSSComponent* nss = NS_STATIC_CAST(nsNSSComponent*, data);
  if (nss)
    nss->PrefChanged(aPrefName);
  return 0;
}

void
nsNSSComponent::PrefChanged(const char* prefName)
{
  PRBool enabled;

  if (!nsCRT::strcmp(prefName, "security.enable_ssl2")) {
    mPref->GetBoolPref("security.enable_ssl2", &enabled);
    SSL_OptionSetDefault(SSL_ENABLE_SSL2, enabled);
  } else if (!nsCRT::strcmp(prefName, "security.enable_ssl3")) {
    mPref->GetBoolPref("security.enable_ssl3", &enabled);
    SSL_OptionSetDefault(SSL_ENABLE_SSL3, enabled);
  } else if (!nsCRT::strcmp(prefName, "security.enable_tls")) {
    mPref->GetBoolPref("security.enable_tls", &enabled);
    SSL_OptionSetDefault(SSL_ENABLE_TLS, enabled);
  } else if (!nsCRT::strcmp(prefName, "security.OCSP.enabled")) {
    setOCSPOptions(mPref);
  } else {
    /* Look through the cipher table and set according to pref setting */
    for (CipherPref* cp = CipherPrefs; cp->pref; ++cp) {
      if (!nsCRT::strcmp(prefName, cp->pref)) {
        mPref->GetBoolPref(cp->pref, &enabled);
        SSL_CipherPrefSetDefault(cp->id, enabled);
        break;
      }
    }
  }
}

#define PROFILE_BEFORE_CHANGE_TOPIC NS_LITERAL_CSTRING("profile-before-change").get()
#define PROFILE_AFTER_CHANGE_TOPIC NS_LITERAL_CSTRING("profile-after-change").get()


NS_IMETHODIMP
nsNSSComponent::Observe(nsISupports *aSubject, const char *aTopic, 
                        const PRUnichar *someData)
{
  if (nsCRT::strcmp(aTopic, PROFILE_BEFORE_CHANGE_TOPIC) == 0) {
    //The profile is about to change, shut down NSS
    NSS_Shutdown();
    mNSSInitialized = PR_FALSE;
  } else if (nsCRT::strcmp(aTopic, PROFILE_AFTER_CHANGE_TOPIC) == 0) {
    InitializeNSS();
    InstallLoadableRoots();
  }

  return NS_OK;
}

nsresult
nsNSSComponent::RegisterProfileChangeObserver()
{
  nsCOMPtr<nsIObserverService> observerService(do_GetService("@mozilla.org/observer-service;1"));
  NS_ASSERTION(observerService, "could not get observer service");
  if (observerService) {
    observerService->AddObserver(this, PROFILE_BEFORE_CHANGE_TOPIC, PR_TRUE);
    observerService->AddObserver(this, PROFILE_AFTER_CHANGE_TOPIC, PR_TRUE);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNSSComponent::RememberCert(CERTCertificate *cert)
{
  if (!hashTableCerts || !cert)
    return NS_OK;
  
  void *found = PL_HashTableLookup(hashTableCerts, (void*)&cert->certKey);
  
  if (found) {
    // we remember that cert already
    return NS_OK;
  }
  
  CERTCertificate *myDupCert = CERT_DupCertificate(cert);
  
  if (!myDupCert)
    return NS_ERROR_OUT_OF_MEMORY;
  
  if (!PL_HashTableAdd(hashTableCerts, (void*)&myDupCert->certKey, myDupCert)) {
    CERT_DestroyCertificate(myDupCert);
  }
  
  return NS_OK;
}





NS_IMPL_ISUPPORTS1(PipUIContext, nsIInterfaceRequestor)

PipUIContext::PipUIContext()
{
  NS_INIT_ISUPPORTS();
}

PipUIContext::~PipUIContext()
{
}

/* void getInterface (in nsIIDRef uuid, [iid_is (uuid), retval] out nsQIResult result); */
NS_IMETHODIMP PipUIContext::GetInterface(const nsIID & uuid, void * *result)
{
  nsresult rv = NS_OK;

  if (uuid.Equals(NS_GET_IID(nsIPrompt))) {
    nsCOMPtr<nsIProxyObjectManager> proxyman(do_GetService(NS_XPCOMPROXY_CONTRACTID));
    if (!proxyman) return NS_ERROR_FAILURE;

    nsCOMPtr<nsIPrompt> prompter;
    nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService("@mozilla.org/embedcomp/window-watcher;1"));
    if (wwatch) {
      wwatch->GetNewPrompter(0, getter_AddRefs(prompter));
      if (prompter) {
        nsCOMPtr<nsIPrompt> proxyPrompt;
        proxyman->GetProxyForObject(NS_UI_THREAD_EVENTQ, NS_GET_IID(nsIPrompt),
                                    prompter, PROXY_SYNC, getter_AddRefs(proxyPrompt));
        if (!proxyPrompt) return NS_ERROR_FAILURE;
        *result = proxyPrompt;
        NS_ADDREF((nsIPrompt*)*result);
      }
    }
  } else {
    rv = NS_ERROR_NO_INTERFACE;
  }

  return rv;
}

static const char *kNSSDialogsContractId = NS_NSSDIALOGS_CONTRACTID;

nsresult 
getNSSDialogs(void **_result, REFNSIID aIID)
{
  nsresult rv;
  nsCOMPtr<nsISupports> result;
  nsCOMPtr<nsISupports> proxiedResult;

  rv = nsServiceManager::GetService(kNSSDialogsContractId, 
                                    NS_GET_IID(nsINSSDialogs),
                                    getter_AddRefs(result));
  if (NS_FAILED(rv)) 
    return rv;

  nsCOMPtr<nsIProxyObjectManager> proxyman(do_GetService(NS_XPCOMPROXY_CONTRACTID));
  if (!proxyman)
    return NS_ERROR_FAILURE;
 
  proxyman->GetProxyForObject(NS_UI_THREAD_EVENTQ,
                              aIID, result, PROXY_SYNC,
                              getter_AddRefs(proxiedResult));

  if (!proxiedResult) {
    return NS_ERROR_FAILURE;
  }

  rv = proxiedResult->QueryInterface(aIID, _result);

  return rv;
}

nsresult
setPassword(PK11SlotInfo *slot, nsIInterfaceRequestor *ctx)
{
  nsresult rv = NS_OK;
  
  if (PK11_NeedUserInit(slot)) {
    nsITokenPasswordDialogs *dialogs;
    PRBool canceled;
    NS_ConvertUTF8toUCS2 tokenName(PK11_GetTokenName(slot));

    rv = getNSSDialogs((void**)&dialogs,
                       NS_GET_IID(nsITokenPasswordDialogs));

    if (NS_FAILED(rv)) goto loser;

    rv = dialogs->SetPassword(ctx,
                              tokenName.get(),
                              &canceled);
    NS_RELEASE(dialogs);
    if (NS_FAILED(rv)) goto loser;

    if (canceled) { rv = NS_ERROR_NOT_AVAILABLE; goto loser; }
  }
 loser:
  return rv;
}

//
// Implementation of an nsIInterfaceRequestor for use
// as context for NSS calls
//
class PSMContentDownloaderContext : public nsIInterfaceRequestor
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIINTERFACEREQUESTOR

  PSMContentDownloaderContext();
  virtual ~PSMContentDownloaderContext();

};

NS_IMPL_ISUPPORTS1(PSMContentDownloaderContext, nsIInterfaceRequestor)

PSMContentDownloaderContext::PSMContentDownloaderContext()
{
  NS_INIT_ISUPPORTS();
}

PSMContentDownloaderContext::~PSMContentDownloaderContext()
{
}

/* void getInterface (in nsIIDRef uuid, [iid_is (uuid), retval] out nsQIResult result); */
NS_IMETHODIMP PSMContentDownloaderContext::GetInterface(const nsIID & uuid, void * *result)
{
  nsresult rv;

  if (uuid.Equals(NS_GET_IID(nsIPrompt))) {
    nsCOMPtr<nsIProxyObjectManager> proxyman(do_GetService(NS_XPCOMPROXY_CONTRACTID));
    if (!proxyman) return NS_ERROR_FAILURE;

    nsCOMPtr<nsIPrompt> prompter;
    nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService("@mozilla.org/embedcomp/window-watcher;1"));
    if (wwatch) {
      wwatch->GetNewPrompter(0, getter_AddRefs(prompter));
      if (prompter) {
        nsCOMPtr<nsIPrompt> proxyPrompt;
        proxyman->GetProxyForObject(NS_UI_THREAD_EVENTQ, NS_GET_IID(nsIPrompt),
                                    prompter, PROXY_SYNC, getter_AddRefs(proxyPrompt));
        if (!proxyPrompt) return NS_ERROR_FAILURE;
        *result = proxyPrompt;
        NS_ADDREF((nsIPrompt*)*result);
      }
    }
  } else {
    rv = NS_ERROR_NO_INTERFACE;
  }

  return rv;
}

class PSMContentDownloader : public nsIStreamListener
{
public:
  PSMContentDownloader() {NS_ASSERTION(PR_FALSE, "don't use this constructor."); }
  PSMContentDownloader(PRUint32 type);
  virtual ~PSMContentDownloader();
  
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER

  enum {UNKNOWN_TYPE = 0};
  enum {X509_CA_CERT  = 1};
  enum {X509_USER_CERT  = 2};
  enum {X509_EMAIL_CERT  = 3};
  enum {X509_SERVER_CERT  = 4};
  enum {PKCS7_CRL = 5};

protected:
  char* mByteData;
  PRInt32 mBufferOffset;
  PRInt32 mContentLength;
  PRUint32 mType;
  nsCOMPtr<nsISecurityManagerComponent> mNSS;
  nsCOMPtr<nsIURI> mURI;
};


PSMContentDownloader::PSMContentDownloader(PRUint32 type)
  : mByteData(nsnull),
    mType(type)
{
  NS_INIT_ISUPPORTS();
  mNSS = do_GetService(PSM_COMPONENT_CONTRACTID);
}

PSMContentDownloader::~PSMContentDownloader()
{
  if (mByteData)
    nsMemory::Free(mByteData);
}

/*NS_IMPL_ISUPPORTS1(CertDownloader, nsIStreamListener)*/
NS_IMPL_ISUPPORTS1(PSMContentDownloader,nsIStreamListener)

const PRInt32 kDefaultCertAllocLength = 2048;

NS_IMETHODIMP
PSMContentDownloader::OnStartRequest(nsIRequest* request, nsISupports* context)
{
  nsresult rv;
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("CertDownloader::OnStartRequest\n"));
  nsCOMPtr<nsIChannel> channel(do_QueryInterface(request));
  if (!channel) return NS_ERROR_FAILURE;

  // Get the URI //
  channel->GetURI(getter_AddRefs(mURI));

  rv = channel->GetContentLength(&mContentLength);
  if (rv != NS_OK || mContentLength == -1)
    mContentLength = kDefaultCertAllocLength;
  
  mBufferOffset = 0;
  mByteData = (char*) nsMemory::Alloc(mContentLength);
  if (!mByteData)
    return NS_ERROR_OUT_OF_MEMORY;
  
  return NS_OK;
}

NS_IMETHODIMP
PSMContentDownloader::OnDataAvailable(nsIRequest* request,
                                nsISupports* context,
                                nsIInputStream *aIStream,
                                PRUint32 aSourceOffset,
                                PRUint32 aLength)
{
  if (!mByteData)
    return NS_ERROR_OUT_OF_MEMORY;
  
  PRUint32 amt;
  nsresult err;
  //Do a check to see if we need to allocate more memory.
  if ((mBufferOffset + (PRInt32)aLength) > mContentLength) {
      size_t newSize = mContentLength + kDefaultCertAllocLength;
      char *newBuffer;
      newBuffer = (char*)nsMemory::Realloc(mByteData, newSize);
      if (newBuffer == nsnull) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      mByteData = newBuffer;
      mContentLength = newSize;
  }
  
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("CertDownloader::OnDataAvailable\n"));
  do {
    err = aIStream->Read(mByteData+mBufferOffset,
                         mContentLength-mBufferOffset, &amt);
    if (amt == 0) break;
    if (NS_FAILED(err)) return err;
    
    aLength -= amt;
    mBufferOffset += amt;
    
  } while (aLength > 0);
  
  return NS_OK;
}

NS_IMETHODIMP
PSMContentDownloader::OnStopRequest(nsIRequest* request,
                              nsISupports* context,
                              nsresult aStatus)
{
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("CertDownloader::OnStopRequest\n"));
  /* this will init NSS if it hasn't happened already */
  nsCOMPtr<nsIX509CertDB> certdb = do_GetService(NS_X509CERTDB_CONTRACTID);

  nsresult rv;
  nsCOMPtr<nsIInterfaceRequestor> ctx = new PSMContentDownloaderContext();

  switch (mType) {
  case PSMContentDownloader::X509_CA_CERT:
    return certdb->ImportCertificates(mByteData, mBufferOffset, mType, ctx); 
  case PSMContentDownloader::X509_USER_CERT:
    return certdb->ImportUserCertificate(mByteData, mBufferOffset, ctx);
    break;
  case PSMContentDownloader::PKCS7_CRL:
    return certdb->ImportCrl(mByteData, mBufferOffset, mURI, SEC_CRL_TYPE);
  default:
    rv = NS_ERROR_FAILURE;
    break;
  }
  
  return rv;
}


/* other mime types that we should handle sometime:
   
   application/x-pkcs7-crl
   application/x-pkcs7-mime
   application/pkcs7-signature
   application/pre-encrypted
   
*/

PRUint32
getPSMContentType(const char * aContentType)
{ 
  if (!nsCRT::strcasecmp(aContentType, "application/x-x509-ca-cert"))
    return PSMContentDownloader::X509_CA_CERT;
  else if (!nsCRT::strcasecmp(aContentType, "application/x-x509-server-cert"))
    return PSMContentDownloader::X509_SERVER_CERT;
  else if (!nsCRT::strcasecmp(aContentType, "application/x-x509-user-cert"))
    return PSMContentDownloader::X509_USER_CERT;
  else if (!nsCRT::strcasecmp(aContentType, "application/x-x509-email-cert"))
    return PSMContentDownloader::X509_EMAIL_CERT;
  else if (!nsCRT::strcasecmp(aContentType, "application/x-pkcs7-crl"))
    return PSMContentDownloader::PKCS7_CRL;
  else if (!nsCRT::strcasecmp(aContentType, "application/x-x509-crl"))
    return PSMContentDownloader::PKCS7_CRL;
  else if (!nsCRT::strcasecmp(aContentType, "application/pkix-crl"))
    return PSMContentDownloader::PKCS7_CRL;
  return PSMContentDownloader::UNKNOWN_TYPE;
}


NS_IMPL_ISUPPORTS2(PSMContentListener,
                   nsIURIContentListener,
                   nsISupportsWeakReference); 

PSMContentListener::PSMContentListener()
{
  NS_INIT_ISUPPORTS();
  mLoadCookie = nsnull;
  mParentContentListener = nsnull;
}

PSMContentListener::~PSMContentListener()
{
}

nsresult
PSMContentListener::init()
{
  return NS_OK;
}

NS_IMETHODIMP
PSMContentListener::OnStartURIOpen(nsIURI *aURI, PRBool *aAbortOpen)
{
  //if we don't want to handle the URI, return PR_TRUE in
  //*aAbortOpen
  return NS_OK;
}

NS_IMETHODIMP
PSMContentListener::IsPreferred(const char * aContentType,
                                 char ** aDesiredContentType,
                                 PRBool * aCanHandleContent)
{
  return CanHandleContent(aContentType, PR_TRUE,
                          aDesiredContentType, aCanHandleContent);
}

NS_IMETHODIMP
PSMContentListener::CanHandleContent(const char * aContentType,
                                      PRBool aIsContentPreferred,
                                      char ** aDesiredContentType,
                                      PRBool * aCanHandleContent)
{
  PRUint32 type;
  type = getPSMContentType(aContentType);
  if (type == nsIX509Cert::UNKNOWN_CERT) {
    *aCanHandleContent = PR_FALSE;
  } else {
    *aCanHandleContent = PR_TRUE;
  }
  return NS_OK;
}

NS_IMETHODIMP
PSMContentListener::DoContent(const char * aContentType,
                               PRBool aIsContentPreferred,
                               nsIRequest * aRequest,
                               nsIStreamListener ** aContentHandler,
                               PRBool * aAbortProcess)
{
  PSMContentDownloader *downLoader;
  PRUint32 type;
  type = getPSMContentType(aContentType);
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("PSMContentListener::DoContent\n"));
  if (type != nsIX509Cert::UNKNOWN_CERT) {
    downLoader = new PSMContentDownloader(type);
    if (downLoader) {
      downLoader->QueryInterface(NS_GET_IID(nsIStreamListener), 
                                            (void **)aContentHandler);
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
PSMContentListener::GetLoadCookie(nsISupports * *aLoadCookie)
{
  *aLoadCookie = mLoadCookie;
  NS_IF_ADDREF(*aLoadCookie);
  return NS_OK;
}

NS_IMETHODIMP
PSMContentListener::SetLoadCookie(nsISupports * aLoadCookie)
{
  mLoadCookie = aLoadCookie;
  return NS_OK;
}

NS_IMETHODIMP
PSMContentListener::GetParentContentListener(nsIURIContentListener ** aContentListener)
{
  *aContentListener = mParentContentListener;
  NS_IF_ADDREF(*aContentListener);
  return NS_OK;
}

NS_IMETHODIMP
PSMContentListener::SetParentContentListener(nsIURIContentListener * aContentListener)
{
  mParentContentListener = aContentListener;
  return NS_OK;
}

