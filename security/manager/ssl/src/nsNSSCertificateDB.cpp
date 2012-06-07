/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNSSComponent.h"
#include "nsNSSCertificateDB.h"
#include "nsCOMPtr.h"
#include "nsNSSCertificate.h"
#include "nsNSSHelper.h"
#include "nsNSSCertHelper.h"
#include "nsNSSCertCache.h"
#include "nsCRT.h"
#include "nsICertificateDialogs.h"
#include "nsNSSCertTrust.h"
#include "nsIFile.h"
#include "nsPKCS12Blob.h"
#include "nsPK11TokenDB.h"
#include "nsOCSPResponder.h"
#include "nsReadableUtils.h"
#include "nsIMutableArray.h"
#include "nsArrayUtils.h"
#include "nsNSSShutDown.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsComponentManagerUtils.h"
#include "nsIPrompt.h"
#include "nsThreadUtils.h"

#include "nspr.h"
extern "C" {
#include "pk11func.h"
#include "certdb.h"
#include "cert.h"
#include "secerr.h"
#include "nssb64.h"
#include "secasn1.h"
#include "secder.h"
}
#include "ssl.h"
#include "ocsp.h"
#include "plbase64.h"

#ifdef PR_LOGGING
extern PRLogModuleInfo* gPIPNSSLog;
#endif

#include "nsNSSCleaner.h"
NSSCleanupAutoPtrClass(CERTCertificate, CERT_DestroyCertificate)
NSSCleanupAutoPtrClass(CERTCertList, CERT_DestroyCertList)
NSSCleanupAutoPtrClass(CERTCertificateList, CERT_DestroyCertificateList)
NSSCleanupAutoPtrClass(PK11SlotInfo, PK11_FreeSlot)

static NS_DEFINE_CID(kNSSComponentCID, NS_NSSCOMPONENT_CID);


NS_IMPL_THREADSAFE_ISUPPORTS2(nsNSSCertificateDB, nsIX509CertDB, nsIX509CertDB2)

nsNSSCertificateDB::nsNSSCertificateDB()
{
}

nsNSSCertificateDB::~nsNSSCertificateDB()
{
}

NS_IMETHODIMP
nsNSSCertificateDB::FindCertByNickname(nsISupports *aToken,
                                      const nsAString &nickname,
                                      nsIX509Cert **_rvCert)
{
  nsNSSShutDownPreventionLock locker;
  CERTCertificate *cert = NULL;
  char *asciiname = NULL;
  NS_ConvertUTF16toUTF8 aUtf8Nickname(nickname);
  asciiname = const_cast<char*>(aUtf8Nickname.get());
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("Getting \"%s\"\n", asciiname));
#if 0
  // what it should be, but for now...
  if (aToken) {
    cert = PK11_FindCertFromNickname(asciiname, NULL);
  } else {
    cert = CERT_FindCertByNickname(CERT_GetDefaultCertDB(), asciiname);
  }
#endif
  cert = PK11_FindCertFromNickname(asciiname, NULL);
  if (!cert) {
    cert = CERT_FindCertByNickname(CERT_GetDefaultCertDB(), asciiname);
  }
  if (cert) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("got it\n"));
    nsCOMPtr<nsIX509Cert> pCert = nsNSSCertificate::Create(cert);
    CERT_DestroyCertificate(cert);
    if (pCert) {
      *_rvCert = pCert;
      NS_ADDREF(*_rvCert);
      return NS_OK;
    }
  }
  *_rvCert = nsnull;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP 
nsNSSCertificateDB::FindCertByDBKey(const char *aDBkey, nsISupports *aToken,
                                   nsIX509Cert **_cert)
{
  nsNSSShutDownPreventionLock locker;
  SECItem keyItem = {siBuffer, nsnull, 0};
  SECItem *dummy;
  CERTIssuerAndSN issuerSN;
  //unsigned long moduleID,slotID;
  *_cert = nsnull; 
  if (!aDBkey || !*aDBkey)
    return NS_ERROR_INVALID_ARG;

  dummy = NSSBase64_DecodeBuffer(nsnull, &keyItem, aDBkey,
                                 (PRUint32)PL_strlen(aDBkey)); 
  if (!dummy || keyItem.len < NS_NSS_LONG*4) {
    PR_FREEIF(keyItem.data);
    return NS_ERROR_INVALID_ARG;
  }

  CERTCertificate *cert;
  // someday maybe we can speed up the search using the moduleID and slotID
  // moduleID = NS_NSS_GET_LONG(keyItem.data);
  // slotID = NS_NSS_GET_LONG(&keyItem.data[NS_NSS_LONG]);

  // build the issuer/SN structure
  issuerSN.serialNumber.len = NS_NSS_GET_LONG(&keyItem.data[NS_NSS_LONG*2]);
  issuerSN.derIssuer.len = NS_NSS_GET_LONG(&keyItem.data[NS_NSS_LONG*3]);
  if (issuerSN.serialNumber.len == 0 || issuerSN.derIssuer.len == 0
      || issuerSN.serialNumber.len + issuerSN.derIssuer.len
         != keyItem.len - NS_NSS_LONG*4) {
    PR_FREEIF(keyItem.data);
    return NS_ERROR_INVALID_ARG;
  }
  issuerSN.serialNumber.data= &keyItem.data[NS_NSS_LONG*4];
  issuerSN.derIssuer.data= &keyItem.data[NS_NSS_LONG*4+
                                              issuerSN.serialNumber.len];

  cert = CERT_FindCertByIssuerAndSN(CERT_GetDefaultCertDB(), &issuerSN);
  PR_FREEIF(keyItem.data);
  if (cert) {
    nsNSSCertificate *nssCert = nsNSSCertificate::Create(cert);
    CERT_DestroyCertificate(cert);
    if (nssCert == nsnull)
      return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(nssCert);
    *_cert = static_cast<nsIX509Cert*>(nssCert);
  }
  return NS_OK;
}

NS_IMETHODIMP 
nsNSSCertificateDB::FindCertNicknames(nsISupports *aToken, 
                                     PRUint32      aType,
                                     PRUint32     *_count,
                                     PRUnichar  ***_certNames)
{
  nsNSSShutDownPreventionLock locker;
  nsresult rv = NS_ERROR_FAILURE;
  /*
   * obtain the cert list from NSS
   */
  CERTCertList *certList = NULL;
  PK11CertListType pk11type;
#if 0
  // this would seem right, but it didn't work...
  // oh, I know why - bonks out on internal slot certs
  if (aType == nsIX509Cert::USER_CERT)
    pk11type = PK11CertListUser;
  else 
#endif
    pk11type = PK11CertListUnique;
  certList = PK11_ListCerts(pk11type, NULL);
  if (!certList)
    goto cleanup;
  /*
   * get list of cert names from list of certs
   * XXX also cull the list (NSS only distinguishes based on user/non-user
   */
  getCertNames(certList, aType, _count, _certNames);
  rv = NS_OK;
  /*
   * finish up
   */
cleanup:
  if (certList)
    CERT_DestroyCertList(certList);
  return rv;
}

SECStatus PR_CALLBACK
collect_certs(void *arg, SECItem **certs, int numcerts)
{
  CERTDERCerts *collectArgs;
  SECItem *cert;
  SECStatus rv;

  collectArgs = (CERTDERCerts *)arg;

  collectArgs->numcerts = numcerts;
  collectArgs->rawCerts = (SECItem *) PORT_ArenaZAlloc(collectArgs->arena,
                                           sizeof(SECItem) * numcerts);
  if ( collectArgs->rawCerts == NULL )
    return(SECFailure);

  cert = collectArgs->rawCerts;

  while ( numcerts-- ) {
    rv = SECITEM_CopyItem(collectArgs->arena, cert, *certs);
    if ( rv == SECFailure )
      return(SECFailure);
    cert++;
    certs++;
  }

  return (SECSuccess);
}

CERTDERCerts*
nsNSSCertificateDB::getCertsFromPackage(PRArenaPool *arena, PRUint8 *data, 
                                        PRUint32 length)
{
  nsNSSShutDownPreventionLock locker;
  CERTDERCerts *collectArgs = 
               (CERTDERCerts *)PORT_ArenaZAlloc(arena, sizeof(CERTDERCerts));
  if ( collectArgs == nsnull ) 
    return nsnull;

  collectArgs->arena = arena;
  SECStatus sec_rv = CERT_DecodeCertPackage(reinterpret_cast<char *>(data), 
                                            length, collect_certs, 
                                            (void *)collectArgs);
  if (sec_rv != SECSuccess)
    return nsnull;

  return collectArgs;
}

nsresult
nsNSSCertificateDB::handleCACertDownload(nsIArray *x509Certs,
                                         nsIInterfaceRequestor *ctx)
{
  // First thing we have to do is figure out which certificate we're 
  // gonna present to the user.  The CA may have sent down a list of 
  // certs which may or may not be a chained list of certs.  Until
  // the day we can design some solid UI for the general case, we'll
  // code to the > 90% case.  That case is where a CA sends down a
  // list that is a hierarchy whose root is either the first or 
  // the last cert.  What we're gonna do is compare the first 
  // 2 entries, if the second was signed by the first, we assume
  // the root cert is the first cert and display it.  Otherwise,
  // we compare the last 2 entries, if the second to last cert was
  // signed by the last cert, then we assume the last cert is the
  // root and display it.

  nsNSSShutDownPreventionLock locker;

  PRUint32 numCerts;

  x509Certs->GetLength(&numCerts);
  NS_ASSERTION(numCerts > 0, "Didn't get any certs to import.");
  if (numCerts == 0)
    return NS_OK; // Nothing to import, so nothing to do.

  nsCOMPtr<nsIX509Cert> certToShow;
  nsCOMPtr<nsISupports> isupports;
  PRUint32 selCertIndex;
  if (numCerts == 1) {
    // There's only one cert, so let's show it.
    selCertIndex = 0;
    certToShow = do_QueryElementAt(x509Certs, selCertIndex);
  } else {
    nsCOMPtr<nsIX509Cert> cert0;    // first cert
    nsCOMPtr<nsIX509Cert> cert1;    // second cert
    nsCOMPtr<nsIX509Cert> certn_2;  // second to last cert
    nsCOMPtr<nsIX509Cert> certn_1;  // last cert

    cert0 = do_QueryElementAt(x509Certs, 0);
    cert1 = do_QueryElementAt(x509Certs, 1);
    certn_2 = do_QueryElementAt(x509Certs, numCerts-2);
    certn_1 = do_QueryElementAt(x509Certs, numCerts-1);

    nsXPIDLString cert0SubjectName;
    nsXPIDLString cert1IssuerName;
    nsXPIDLString certn_2IssuerName;
    nsXPIDLString certn_1SubjectName;

    cert0->GetSubjectName(cert0SubjectName);
    cert1->GetIssuerName(cert1IssuerName);
    certn_2->GetIssuerName(certn_2IssuerName);
    certn_1->GetSubjectName(certn_1SubjectName);

    if (cert1IssuerName.Equals(cert0SubjectName)) {
      // In this case, the first cert in the list signed the second,
      // so the first cert is the root.  Let's display it. 
      selCertIndex = 0;
      certToShow = cert0;
    } else 
    if (certn_2IssuerName.Equals(certn_1SubjectName)) { 
      // In this case the last cert has signed the second to last cert.
      // The last cert is the root, so let's display it.
      selCertIndex = numCerts-1;
      certToShow = certn_1;
    } else {
      // It's not a chain, so let's just show the first one in the 
      // downloaded list.
      selCertIndex = 0;
      certToShow = cert0;
    }
  }

  if (!certToShow)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsICertificateDialogs> dialogs;
  nsresult rv = ::getNSSDialogs(getter_AddRefs(dialogs), 
                                NS_GET_IID(nsICertificateDialogs),
                                NS_CERTIFICATEDIALOGS_CONTRACTID);
                       
  if (NS_FAILED(rv))
    return rv;
 
  SECItem der;
  rv=certToShow->GetRawDER(&der.len, (PRUint8 **)&der.data);

  if (NS_FAILED(rv))
    return rv;

  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("Creating temp cert\n"));
  CERTCertificate *tmpCert;
  CERTCertDBHandle *certdb = CERT_GetDefaultCertDB();
  tmpCert = CERT_FindCertByDERCert(certdb, &der);
  if (!tmpCert) {
    tmpCert = CERT_NewTempCertificate(certdb, &der,
                                      nsnull, false, true);
  }
  nsMemory::Free(der.data);
  der.data = nsnull;
  der.len = 0;
  
  if (!tmpCert) {
    NS_ERROR("Couldn't create cert from DER blob");
    return NS_ERROR_FAILURE;
  }

  CERTCertificateCleaner tmpCertCleaner(tmpCert);

  if (!CERT_IsCACert(tmpCert, NULL)) {
    DisplayCertificateAlert(ctx, "NotACACert", certToShow);
    return NS_ERROR_FAILURE;
  }

  if (tmpCert->isperm) {
    DisplayCertificateAlert(ctx, "CaCertExists", certToShow);
    return NS_ERROR_FAILURE;
  }

  PRUint32 trustBits;
  bool allows;
  rv = dialogs->ConfirmDownloadCACert(ctx, certToShow, &trustBits, &allows);
  if (NS_FAILED(rv))
    return rv;

  if (!allows)
    return NS_ERROR_NOT_AVAILABLE;

  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("trust is %d\n", trustBits));
  nsXPIDLCString nickname;
  nickname.Adopt(CERT_MakeCANickname(tmpCert));

  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("Created nick \"%s\"\n", nickname.get()));

  nsNSSCertTrust trust;
  trust.SetValidCA();
  trust.AddCATrust(!!(trustBits & nsIX509CertDB::TRUSTED_SSL),
                   !!(trustBits & nsIX509CertDB::TRUSTED_EMAIL),
                   !!(trustBits & nsIX509CertDB::TRUSTED_OBJSIGN));

  SECStatus srv = CERT_AddTempCertToPerm(tmpCert, 
                                         const_cast<char*>(nickname.get()), 
                                         trust.GetTrust()); 

  if (srv != SECSuccess)
    return NS_ERROR_FAILURE;

  // Import additional delivered certificates that can be verified.

  // build a CertList for filtering
  CERTCertList *certList = CERT_NewCertList();
  if (certList == NULL) {
    return NS_ERROR_FAILURE;
  }

  CERTCertListCleaner listCleaner(certList);

  // get all remaining certs into temp store

  for (PRUint32 i=0; i<numCerts; i++) {
    if (i == selCertIndex) {
      // we already processed that one
      continue;
    }

    certToShow = do_QueryElementAt(x509Certs, i);
    certToShow->GetRawDER(&der.len, (PRUint8 **)&der.data);

    CERTCertificate *tmpCert2 = 
      CERT_NewTempCertificate(certdb, &der, nsnull, false, true);

    nsMemory::Free(der.data);
    der.data = nsnull;
    der.len = 0;

    if (!tmpCert2) {
      NS_ERROR("Couldn't create temp cert from DER blob");
      continue;  // Let's try to import the rest of 'em
    }
    
    CERT_AddCertToListTail(certList, tmpCert2);
  }

  return ImportValidCACertsInList(certList, ctx);
}

/*
 *  [noscript] void importCertificates(in charPtr data, in unsigned long length,
 *                                     in unsigned long type, 
 *                                     in nsIInterfaceRequestor ctx);
 */
NS_IMETHODIMP 
nsNSSCertificateDB::ImportCertificates(PRUint8 * data, PRUint32 length, 
                                       PRUint32 type, 
                                       nsIInterfaceRequestor *ctx)

{
  nsNSSShutDownPreventionLock locker;
  nsresult nsrv;

  PRArenaPool *arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
  if (!arena)
    return NS_ERROR_OUT_OF_MEMORY;

  CERTDERCerts *certCollection = getCertsFromPackage(arena, data, length);
  if (!certCollection) {
    PORT_FreeArena(arena, false);
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsIMutableArray> array =
    do_CreateInstance(NS_ARRAY_CONTRACTID, &nsrv);
  if (NS_FAILED(nsrv)) {
    PORT_FreeArena(arena, false);
    return nsrv;
  }

  // Now let's create some certs to work with
  nsCOMPtr<nsIX509Cert> x509Cert;
  nsNSSCertificate *nssCert;
  SECItem *currItem;
  for (int i=0; i<certCollection->numcerts; i++) {
     currItem = &certCollection->rawCerts[i];
     nssCert = nsNSSCertificate::ConstructFromDER((char*)currItem->data, currItem->len);
     if (!nssCert)
       return NS_ERROR_FAILURE;
     x509Cert = do_QueryInterface((nsIX509Cert*)nssCert);
     array->AppendElement(x509Cert, false);
  }
  switch (type) {
  case nsIX509Cert::CA_CERT:
    nsrv = handleCACertDownload(array, ctx);
    break;
  default:
    // We only deal with import CA certs in this method currently.
     nsrv = NS_ERROR_FAILURE;
     break;
  }  
  PORT_FreeArena(arena, false);
  return nsrv;
}


/*
 *  [noscript] void importEmailCertificates(in charPtr data, in unsigned long length,
 *                                     in nsIInterfaceRequestor ctx);
 */
NS_IMETHODIMP
nsNSSCertificateDB::ImportEmailCertificate(PRUint8 * data, PRUint32 length, 
                                       nsIInterfaceRequestor *ctx)

{
  nsNSSShutDownPreventionLock locker;
  SECStatus srv = SECFailure;
  nsresult nsrv = NS_OK;
  CERTCertDBHandle *certdb;
  CERTCertificate **certArray = NULL;
  CERTCertList *certList = NULL;
  CERTCertListNode *node;
  PRTime now;
  SECCertUsage certusage;
  SECCertificateUsage certificateusage;
  SECItem **rawArray;
  int numcerts;
  int i;
  CERTValOutParam cvout[1];
  cvout[0].type = cert_po_end;

  nsCOMPtr<nsINSSComponent> inss = do_GetService(kNSSComponentCID, &nsrv);
  if (!inss)
    return nsrv;
  nsRefPtr<nsCERTValInParamWrapper> survivingParams;
  nsrv = inss->GetDefaultCERTValInParam(survivingParams);
  if (NS_FAILED(nsrv))
    return nsrv;
 
  PRArenaPool *arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
  if (!arena)
    return NS_ERROR_OUT_OF_MEMORY;

  CERTDERCerts *certCollection = getCertsFromPackage(arena, data, length);
  if (!certCollection) {
    PORT_FreeArena(arena, false);
    return NS_ERROR_FAILURE;
  }

  certdb = CERT_GetDefaultCertDB();
  certusage = certUsageEmailRecipient;
  certificateusage = certificateUsageEmailRecipient;

  numcerts = certCollection->numcerts;

  rawArray = (SECItem **) PORT_Alloc(sizeof(SECItem *) * numcerts);
  if ( !rawArray ) {
    nsrv = NS_ERROR_FAILURE;
    goto loser;
  }

  for (i=0; i < numcerts; i++) {
    rawArray[i] = &certCollection->rawCerts[i];
  }

  srv = CERT_ImportCerts(certdb, certusage, numcerts, rawArray, 
                         &certArray, false, false, NULL);

  PORT_Free(rawArray);
  rawArray = NULL;

  if (srv != SECSuccess) {
    nsrv = NS_ERROR_FAILURE;
    goto loser;
  }

  // build a CertList for filtering
  certList = CERT_NewCertList();
  if (certList == NULL) {
    nsrv = NS_ERROR_FAILURE;
    goto loser;
  }
  for (i=0; i < numcerts; i++) {
    CERTCertificate *cert = certArray[i];
    if (cert)
      cert = CERT_DupCertificate(cert);
    if (cert)
      CERT_AddCertToListTail(certList, cert);
  }

  /* go down the remaining list of certs and verify that they have
   * valid chains, then import them.
   */
  now = PR_Now();

  for (node = CERT_LIST_HEAD(certList);
       !CERT_LIST_END(node,certList);
       node = CERT_LIST_NEXT(node)) {

    bool alert_and_skip = false;

    if (!node->cert) {
      continue;
    }

    if (!nsNSSComponent::globalConstFlagUsePKIXVerification) {
      if (CERT_VerifyCert(certdb, node->cert,
          true, certusage, now, ctx, NULL) != SECSuccess) {
        alert_and_skip = true;
      }
    }
    else {
      if (CERT_PKIXVerifyCert(node->cert, certificateusage,
                              survivingParams->GetRawPointerForNSS(),
                              cvout, ctx)
          != SECSuccess) {
        alert_and_skip = true;
      }
    }

    CERTCertificateList *certChain = nsnull;
    CERTCertificateListCleaner chainCleaner(certChain);

    if (!alert_and_skip) {
      certChain = CERT_CertChainFromCert(node->cert, certusage, false);
      if (!certChain) {
        alert_and_skip = true;
      }
    }

    if (alert_and_skip) {    
      nsCOMPtr<nsIX509Cert> certToShow = nsNSSCertificate::Create(node->cert);
      DisplayCertificateAlert(ctx, "NotImportingUnverifiedCert", certToShow);
      continue;
    }

    /*
     * CertChain returns an array of SECItems, import expects an array of
     * SECItem pointers. Create the SECItem Pointers from the array of
     * SECItems.
     */
    rawArray = (SECItem **) PORT_Alloc(certChain->len * sizeof(SECItem *));
    if (!rawArray) {
      continue;
    }
    for (i=0; i < certChain->len; i++) {
      rawArray[i] = &certChain->certs[i];
    }
    CERT_ImportCerts(certdb, certusage, certChain->len, 
                            rawArray,  NULL, true, false, NULL);

    CERT_SaveSMimeProfile(node->cert, NULL, NULL);

    PORT_Free(rawArray);
  }

loser:
  if (certArray) {
    CERT_DestroyCertArray(certArray, numcerts);
  }
  if (certList) {
    CERT_DestroyCertList(certList);
  }
  if (arena) 
    PORT_FreeArena(arena, true);
  return nsrv;
}

NS_IMETHODIMP
nsNSSCertificateDB::ImportServerCertificate(PRUint8 * data, PRUint32 length, 
                                            nsIInterfaceRequestor *ctx)

{
  nsNSSShutDownPreventionLock locker;
  SECStatus srv = SECFailure;
  nsresult nsrv = NS_OK;
  CERTCertificate * cert;
  SECItem **rawCerts = nsnull;
  int numcerts;
  int i;
  nsNSSCertTrust trust;
  char *serverNickname = nsnull;
 
  PRArenaPool *arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
  if (!arena)
    return NS_ERROR_OUT_OF_MEMORY;

  CERTDERCerts *certCollection = getCertsFromPackage(arena, data, length);
  if (!certCollection) {
    PORT_FreeArena(arena, false);
    return NS_ERROR_FAILURE;
  }
  cert = CERT_NewTempCertificate(CERT_GetDefaultCertDB(), certCollection->rawCerts,
                          (char *)NULL, false, true);
  if (!cert) {
    nsrv = NS_ERROR_FAILURE;
    goto loser;
  }
  numcerts = certCollection->numcerts;
  rawCerts = (SECItem **) PORT_Alloc(sizeof(SECItem *) * numcerts);
  if ( !rawCerts ) {
    nsrv = NS_ERROR_FAILURE;
    goto loser;
  }

  for ( i = 0; i < numcerts; i++ ) {
    rawCerts[i] = &certCollection->rawCerts[i];
  }

  serverNickname = nsNSSCertificate::defaultServerNickname(cert);
  srv = CERT_ImportCerts(CERT_GetDefaultCertDB(), certUsageSSLServer,
             numcerts, rawCerts, NULL, true, false,
             serverNickname);
  PR_FREEIF(serverNickname);
  if ( srv != SECSuccess ) {
    nsrv = NS_ERROR_FAILURE;
    goto loser;
  }

  trust.SetValidServerPeer();
  srv = CERT_ChangeCertTrust(CERT_GetDefaultCertDB(), cert, trust.GetTrust());
  if ( srv != SECSuccess ) {
    nsrv = NS_ERROR_FAILURE;
    goto loser;
  }
loser:
  PORT_Free(rawCerts);
  if (cert)
    CERT_DestroyCertificate(cert);
  if (arena) 
    PORT_FreeArena(arena, true);
  return nsrv;
}

nsresult
nsNSSCertificateDB::ImportValidCACerts(int numCACerts, SECItem *CACerts, nsIInterfaceRequestor *ctx)
{
  CERTCertList *certList = NULL;
  SECItem **rawArray;

  // build a CertList for filtering
  certList = CERT_NewCertList();
  if (certList == NULL) {
    return NS_ERROR_FAILURE;
  }

  CERTCertListCleaner listCleaner(certList);

  // get all certs into temp store
  SECStatus srv = SECFailure;
  CERTCertificate **certArray = NULL;

  rawArray = (SECItem **) PORT_Alloc(sizeof(SECItem *) * numCACerts);
  if ( !rawArray ) {
    return NS_ERROR_FAILURE;
  }

  for (int i=0; i < numCACerts; i++) {
    rawArray[i] = &CACerts[i];
  }

  srv = CERT_ImportCerts(CERT_GetDefaultCertDB(), certUsageAnyCA, numCACerts, rawArray, 
                         &certArray, false, true, NULL);

  PORT_Free(rawArray);
  rawArray = NULL;

  if (srv != SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  for (int i2=0; i2 < numCACerts; i2++) {
    CERTCertificate *cacert = certArray[i2];
    if (cacert)
      cacert = CERT_DupCertificate(cacert);
    if (cacert)
      CERT_AddCertToListTail(certList, cacert);
  }

  CERT_DestroyCertArray(certArray, numCACerts);

  return ImportValidCACertsInList(certList, ctx);
}

nsresult
nsNSSCertificateDB::ImportValidCACertsInList(CERTCertList *certList, nsIInterfaceRequestor *ctx)
{
  SECItem **rawArray;
  nsresult nsrv;
  nsCOMPtr<nsINSSComponent> inss = do_GetService(kNSSComponentCID, &nsrv);
  if (!inss)
    return nsrv;
  nsRefPtr<nsCERTValInParamWrapper> survivingParams;
  nsrv = inss->GetDefaultCERTValInParam(survivingParams);
  if (NS_FAILED(nsrv))
    return nsrv;

  /* filter out the certs we don't want */
  SECStatus srv = CERT_FilterCertListByUsage(certList, certUsageAnyCA, true);
  if (srv != SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  /* go down the remaining list of certs and verify that they have
   * valid chains, if yes, then import.
   */
  CERTCertListNode *node;
  CERTValOutParam cvout[1];
  cvout[0].type = cert_po_end;

  for (node = CERT_LIST_HEAD(certList);
       !CERT_LIST_END(node,certList);
       node = CERT_LIST_NEXT(node)) {

    bool alert_and_skip = false;

    if (!nsNSSComponent::globalConstFlagUsePKIXVerification) {
      if (CERT_VerifyCert(CERT_GetDefaultCertDB(), node->cert, 
          true, certUsageVerifyCA, PR_Now(), ctx, NULL) != SECSuccess) {
        alert_and_skip = true;
      }
    }
    else {
      if (CERT_PKIXVerifyCert(node->cert, certificateUsageVerifyCA,
                              survivingParams->GetRawPointerForNSS(),
                              cvout, ctx)
          != SECSuccess) {
        alert_and_skip = true;
      }
    }

    CERTCertificateList *certChain = nsnull;
    CERTCertificateListCleaner chainCleaner(certChain);

    if (!alert_and_skip) {    
      certChain = CERT_CertChainFromCert(node->cert, certUsageAnyCA, false);
      if (!certChain) {
        alert_and_skip = true;
      }
    }

    if (alert_and_skip) {    
      nsCOMPtr<nsIX509Cert> certToShow = nsNSSCertificate::Create(node->cert);
      DisplayCertificateAlert(ctx, "NotImportingUnverifiedCert", certToShow);
      continue;
    }

    /*
     * CertChain returns an array of SECItems, import expects an array of
     * SECItem pointers. Create the SECItem Pointers from the array of
     * SECItems.
     */
    rawArray = (SECItem **) PORT_Alloc(certChain->len * sizeof(SECItem *));
    if (!rawArray) {
      continue;
    }
    for (int i=0; i < certChain->len; i++) {
      rawArray[i] = &certChain->certs[i];
    }
    CERT_ImportCerts(CERT_GetDefaultCertDB(), certUsageAnyCA, certChain->len, 
                            rawArray,  NULL, true, true, NULL);

    PORT_Free(rawArray);
  }
  
  return NS_OK;
}

void nsNSSCertificateDB::DisplayCertificateAlert(nsIInterfaceRequestor *ctx, 
                                                 const char *stringID, 
                                                 nsIX509Cert *certToShow)
{
  if (!NS_IsMainThread()) {
    NS_ERROR("nsNSSCertificateDB::DisplayCertificateAlert called off the main thread");
    return;
  }

  nsPSMUITracker tracker;
  if (!tracker.isUIForbidden()) {

    nsCOMPtr<nsIInterfaceRequestor> my_ctx = ctx;
    if (!my_ctx)
      my_ctx = new PipUIContext();

    // This shall be replaced by embedding ovverridable prompts
    // as discussed in bug 310446, and should make use of certToShow.

    nsresult rv;
    nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));
    if (NS_SUCCEEDED(rv)) {
      nsAutoString tmpMessage;
      nssComponent->GetPIPNSSBundleString(stringID, tmpMessage);

      nsCOMPtr<nsIPrompt> prompt (do_GetInterface(my_ctx));
      if (!prompt)
        return;
    
      prompt->Alert(nsnull, tmpMessage.get());
    }
  }
}


NS_IMETHODIMP 
nsNSSCertificateDB::ImportUserCertificate(PRUint8 *data, PRUint32 length, nsIInterfaceRequestor *ctx)
{
  if (!NS_IsMainThread()) {
    NS_ERROR("nsNSSCertificateDB::ImportUserCertificate called off the main thread");
    return NS_ERROR_NOT_SAME_THREAD;
  }
  
  nsNSSShutDownPreventionLock locker;
  PK11SlotInfo *slot;
  nsCAutoString nickname;
  nsresult rv = NS_ERROR_FAILURE;
  int numCACerts;
  SECItem *CACerts;
  CERTDERCerts * collectArgs;
  PRArenaPool *arena;
  CERTCertificate * cert=NULL;

  arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
  if ( arena == NULL ) {
    goto loser;
  }

  collectArgs = getCertsFromPackage(arena, data, length);
  if (!collectArgs) {
    goto loser;
  }

  cert = CERT_NewTempCertificate(CERT_GetDefaultCertDB(), collectArgs->rawCerts,
                	       (char *)NULL, false, true);
  if (!cert) {
    goto loser;
  }

  slot = PK11_KeyForCertExists(cert, NULL, ctx);
  if ( slot == NULL ) {
    nsCOMPtr<nsIX509Cert> certToShow = nsNSSCertificate::Create(cert);
    DisplayCertificateAlert(ctx, "UserCertIgnoredNoPrivateKey", certToShow);
    goto loser;
  }
  PK11_FreeSlot(slot);

  /* pick a nickname for the cert */
  if (cert->nickname) {
	/* sigh, we need a call to look up other certs with this subject and
	 * identify nicknames from them. We can no longer walk down internal
	 * database structures  rjr */
  	nickname = cert->nickname;
  }
  else {
    get_default_nickname(cert, ctx, nickname);
  }

  /* user wants to import the cert */
  {
    char *cast_const_away = const_cast<char*>(nickname.get());
    slot = PK11_ImportCertForKey(cert, cast_const_away, ctx);
  }
  if (!slot) {
    goto loser;
  }
  PK11_FreeSlot(slot);

  {
    nsCOMPtr<nsIX509Cert> certToShow = nsNSSCertificate::Create(cert);
    DisplayCertificateAlert(ctx, "UserCertImported", certToShow);
  }
  rv = NS_OK;

  numCACerts = collectArgs->numcerts - 1;
  if (numCACerts) {
    CACerts = collectArgs->rawCerts+1;
    rv = ImportValidCACerts(numCACerts, CACerts, ctx);
  }
  
loser:
  if (arena) {
    PORT_FreeArena(arena, false);
  }
  if ( cert ) {
    CERT_DestroyCertificate(cert);
  }
  return rv;
}

/*
 * void deleteCertificate(in nsIX509Cert aCert);
 */
NS_IMETHODIMP 
nsNSSCertificateDB::DeleteCertificate(nsIX509Cert *aCert)
{
  nsNSSShutDownPreventionLock locker;
  nsCOMPtr<nsIX509Cert2> nssCert = do_QueryInterface(aCert);
  CERTCertificate *cert = nssCert->GetCert();
  if (!cert) return NS_ERROR_FAILURE;
  CERTCertificateCleaner certCleaner(cert);
  SECStatus srv = SECSuccess;

  PRUint32 certType;
  nssCert->GetCertType(&certType);
  if (NS_FAILED(nssCert->MarkForPermDeletion()))
  {
    return NS_ERROR_FAILURE;
  }

  if (cert->slot && certType != nsIX509Cert::USER_CERT) {
    // To delete a cert of a slot (builtin, most likely), mark it as
    // completely untrusted.  This way we keep a copy cached in the
    // local database, and next time we try to load it off of the 
    // external token/slot, we'll know not to trust it.  We don't 
    // want to do that with user certs, because a user may  re-store
    // the cert onto the card again at which point we *will* want to 
    // trust that cert if it chains up properly.
    nsNSSCertTrust trust(0, 0, 0);
    srv = CERT_ChangeCertTrust(CERT_GetDefaultCertDB(), 
                               cert, trust.GetTrust());
  }
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("cert deleted: %d", srv));
  return (srv) ? NS_ERROR_FAILURE : NS_OK;
}

/*
 * void setCertTrust(in nsIX509Cert cert,
 *                   in unsigned long type,
 *                   in unsigned long trust);
 */
NS_IMETHODIMP 
nsNSSCertificateDB::SetCertTrust(nsIX509Cert *cert, 
                                 PRUint32 type,
                                 PRUint32 trusted)
{
  nsNSSShutDownPreventionLock locker;
  SECStatus srv;
  nsNSSCertTrust trust;
  nsCOMPtr<nsIX509Cert2> pipCert = do_QueryInterface(cert);
  if (!pipCert)
    return NS_ERROR_FAILURE;
  CERTCertificate *nsscert = pipCert->GetCert();
  CERTCertificateCleaner certCleaner(nsscert);
  if (type == nsIX509Cert::CA_CERT) {
    // always start with untrusted and move up
    trust.SetValidCA();
    trust.AddCATrust(!!(trusted & nsIX509CertDB::TRUSTED_SSL),
                     !!(trusted & nsIX509CertDB::TRUSTED_EMAIL),
                     !!(trusted & nsIX509CertDB::TRUSTED_OBJSIGN));
    srv = CERT_ChangeCertTrust(CERT_GetDefaultCertDB(), 
                               nsscert,
                               trust.GetTrust());
  } else if (type == nsIX509Cert::SERVER_CERT) {
    // always start with untrusted and move up
    trust.SetValidPeer();
    trust.AddPeerTrust(trusted & nsIX509CertDB::TRUSTED_SSL, 0, 0);
    srv = CERT_ChangeCertTrust(CERT_GetDefaultCertDB(), 
                               nsscert,
                               trust.GetTrust());
  } else if (type == nsIX509Cert::EMAIL_CERT) {
    // always start with untrusted and move up
    trust.SetValidPeer();
    trust.AddPeerTrust(0, !!(trusted & nsIX509CertDB::TRUSTED_EMAIL), 0);
    srv = CERT_ChangeCertTrust(CERT_GetDefaultCertDB(), 
                               nsscert,
                               trust.GetTrust());
  } else {
    // ignore user certs
    return NS_OK;
  }
  return (srv) ? NS_ERROR_FAILURE : NS_OK;
}

NS_IMETHODIMP 
nsNSSCertificateDB::IsCertTrusted(nsIX509Cert *cert, 
                                  PRUint32 certType,
                                  PRUint32 trustType,
                                  bool *_isTrusted)
{
  NS_ENSURE_ARG_POINTER(_isTrusted);
  *_isTrusted = false;

  nsNSSShutDownPreventionLock locker;
  SECStatus srv;
  nsCOMPtr<nsIX509Cert2> pipCert = do_QueryInterface(cert);
  CERTCertificate *nsscert = pipCert->GetCert();
  CERTCertTrust nsstrust;
  srv = CERT_GetCertTrust(nsscert, &nsstrust);
  if (srv != SECSuccess)
    return NS_ERROR_FAILURE;

  nsNSSCertTrust trust(&nsstrust);
  CERT_DestroyCertificate(nsscert);
  if (certType == nsIX509Cert::CA_CERT) {
    if (trustType & nsIX509CertDB::TRUSTED_SSL) {
      *_isTrusted = trust.HasTrustedCA(true, false, false);
    } else if (trustType & nsIX509CertDB::TRUSTED_EMAIL) {
      *_isTrusted = trust.HasTrustedCA(false, true, false);
    } else if (trustType & nsIX509CertDB::TRUSTED_OBJSIGN) {
      *_isTrusted = trust.HasTrustedCA(false, false, true);
    } else {
      return NS_ERROR_FAILURE;
    }
  } else if (certType == nsIX509Cert::SERVER_CERT) {
    if (trustType & nsIX509CertDB::TRUSTED_SSL) {
      *_isTrusted = trust.HasTrustedPeer(true, false, false);
    } else if (trustType & nsIX509CertDB::TRUSTED_EMAIL) {
      *_isTrusted = trust.HasTrustedPeer(false, true, false);
    } else if (trustType & nsIX509CertDB::TRUSTED_OBJSIGN) {
      *_isTrusted = trust.HasTrustedPeer(false, false, true);
    } else {
      return NS_ERROR_FAILURE;
    }
  } else if (certType == nsIX509Cert::EMAIL_CERT) {
    if (trustType & nsIX509CertDB::TRUSTED_SSL) {
      *_isTrusted = trust.HasTrustedPeer(true, false, false);
    } else if (trustType & nsIX509CertDB::TRUSTED_EMAIL) {
      *_isTrusted = trust.HasTrustedPeer(false, true, false);
    } else if (trustType & nsIX509CertDB::TRUSTED_OBJSIGN) {
      *_isTrusted = trust.HasTrustedPeer(false, false, true);
    } else {
      return NS_ERROR_FAILURE;
    }
  } /* user: ignore */
  return NS_OK;
}


NS_IMETHODIMP 
nsNSSCertificateDB::ImportCertsFromFile(nsISupports *aToken, 
                                        nsIFile *aFile,
                                        PRUint32 aType)
{
  NS_ENSURE_ARG(aFile);
  switch (aType) {
    case nsIX509Cert::CA_CERT:
    case nsIX509Cert::EMAIL_CERT:
    case nsIX509Cert::SERVER_CERT:
      // good
      break;
    
    default:
      // not supported (yet)
      return NS_ERROR_FAILURE;
  }

  nsresult rv;
  PRFileDesc *fd = nsnull;

  rv = aFile->OpenNSPRFileDesc(PR_RDONLY, 0, &fd);

  if (NS_FAILED(rv))
    return rv;

  if (!fd)
    return NS_ERROR_FAILURE;

  PRFileInfo file_info;
  if (PR_SUCCESS != PR_GetOpenFileInfo(fd, &file_info))
    return NS_ERROR_FAILURE;
  
  unsigned char *buf = new unsigned char[file_info.size];
  if (!buf)
    return NS_ERROR_OUT_OF_MEMORY;
  
  PRInt32 bytes_obtained = PR_Read(fd, buf, file_info.size);
  PR_Close(fd);
  
  if (bytes_obtained != file_info.size)
    rv = NS_ERROR_FAILURE;
  else {
	  nsCOMPtr<nsIInterfaceRequestor> cxt = new PipUIContext();

    switch (aType) {
      case nsIX509Cert::CA_CERT:
        rv = ImportCertificates(buf, bytes_obtained, aType, cxt);
        break;
        
      case nsIX509Cert::SERVER_CERT:
        rv = ImportServerCertificate(buf, bytes_obtained, cxt);
        break;

      case nsIX509Cert::EMAIL_CERT:
        rv = ImportEmailCertificate(buf, bytes_obtained, cxt);
        break;
      
      default:
        break;
    }
  }

  delete [] buf;
  return rv;  
}

NS_IMETHODIMP 
nsNSSCertificateDB::ImportPKCS12File(nsISupports *aToken, 
                                     nsIFile *aFile)
{
  NS_ENSURE_ARG(aFile);
  nsPKCS12Blob blob;
  nsCOMPtr<nsIPK11Token> token = do_QueryInterface(aToken);
  if (token) {
    blob.SetToken(token);
  }
  return blob.ImportFromFile(aFile);
}

NS_IMETHODIMP 
nsNSSCertificateDB::ExportPKCS12File(nsISupports     *aToken, 
                                     nsIFile          *aFile,
                                     PRUint32          count,
                                     nsIX509Cert     **certs)
                                     //const PRUnichar **aCertNames)
{
  nsNSSShutDownPreventionLock locker;
  NS_ENSURE_ARG(aFile);
  nsPKCS12Blob blob;
  if (count == 0) return NS_OK;
  nsCOMPtr<nsIPK11Token> localRef;
  if (!aToken) {
    PK11SlotInfo *keySlot = PK11_GetInternalKeySlot();
    NS_ASSERTION(keySlot,"Failed to get the internal key slot");
    localRef = new nsPK11Token(keySlot);
    PK11_FreeSlot(keySlot);
  }
  else {
    localRef = do_QueryInterface(aToken);
  }
  blob.SetToken(localRef);
  //blob.LoadCerts(aCertNames, count);
  //return blob.ExportToFile(aFile);
  return blob.ExportToFile(aFile, certs, count);
}


static SECStatus PR_CALLBACK 
GetOCSPResponders (CERTCertificate *aCert,
                   SECItem         *aDBKey,
                   void            *aArg)
{
  nsIMutableArray *array = static_cast<nsIMutableArray*>(aArg);
  PRUnichar* nn = nsnull;
  PRUnichar* url = nsnull;
  char *serviceURL = nsnull;
  char *nickname = nsnull;
  PRUint32 i, count;

  // Are we interested in this cert //
  if (!nsOCSPResponder::IncludeCert(aCert)) {
    return SECSuccess;
  }

  // Get the AIA and nickname //
  serviceURL = CERT_GetOCSPAuthorityInfoAccessLocation(aCert);
  if (serviceURL) {
    url = ToNewUnicode(NS_ConvertUTF8toUTF16(serviceURL));
    PORT_Free(serviceURL);
  }

  nickname = aCert->nickname;
  nn = ToNewUnicode(NS_ConvertUTF8toUTF16(nickname));

  nsCOMPtr<nsIOCSPResponder> new_entry = new nsOCSPResponder(nn, url);
  nsMemory::Free(nn);
  nsMemory::Free(url);

  // Sort the items according to nickname //
  array->GetLength(&count);
  for (i=0; i < count; ++i) {
    nsCOMPtr<nsIOCSPResponder> entry = do_QueryElementAt(array, i);
    if (nsOCSPResponder::CompareEntries(new_entry, entry) < 0) {
      array->InsertElementAt(new_entry, i, false);
      break;
    }
  }
  if (i == count) {
    array->AppendElement(new_entry, false);
  }
  return SECSuccess;
}



/*
 * getOCSPResponders
 *
 * Export a set of certs and keys from the database to a PKCS#12 file.
*/
NS_IMETHODIMP 
nsNSSCertificateDB::GetOCSPResponders(nsIArray ** aResponders)
{
  nsNSSShutDownPreventionLock locker;
  SECStatus sec_rv;
  nsCOMPtr<nsIMutableArray> respondersArray =
    do_CreateInstance(NS_ARRAY_CONTRACTID);
  if (!respondersArray) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  sec_rv = PK11_TraverseSlotCerts(::GetOCSPResponders,
                                  respondersArray,
                                  nsnull);
  if (sec_rv != SECSuccess) {
    goto loser;
  }

  *aResponders = respondersArray;
  NS_IF_ADDREF(*aResponders);
  return NS_OK;
loser:
  return NS_ERROR_FAILURE;
}

/*
 * NSS Helper Routines (private to nsNSSCertificateDB)
 */

#define DELIM '\001'

/*
 * GetSortedNameList
 *
 * Converts a CERTCertList to a list of certificate names
 */
void
nsNSSCertificateDB::getCertNames(CERTCertList *certList,
                                 PRUint32      type, 
                                 PRUint32     *_count,
                                 PRUnichar  ***_certNames)
{
  nsNSSShutDownPreventionLock locker;
  CERTCertListNode *node;
  PRUint32 numcerts = 0, i=0;
  PRUnichar **tmpArray = NULL;
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("List of certs %d:\n", type));
  for (node = CERT_LIST_HEAD(certList);
       !CERT_LIST_END(node, certList);
       node = CERT_LIST_NEXT(node)) {
    if (getCertType(node->cert) == type) {
      numcerts++;
    }
  }
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("num certs: %d\n", numcerts));
  int nc = (numcerts == 0) ? 1 : numcerts;
  tmpArray = (PRUnichar **)nsMemory::Alloc(sizeof(PRUnichar *) * nc);
  if (numcerts == 0) goto finish;
  for (node = CERT_LIST_HEAD(certList);
       !CERT_LIST_END(node, certList);
       node = CERT_LIST_NEXT(node)) {
    if (getCertType(node->cert) == type) {
      nsNSSCertificate pipCert(node->cert);
      char *dbkey = NULL;
      char *namestr = NULL;
      nsAutoString certstr;
      pipCert.GetDbKey(&dbkey);
      nsAutoString keystr = NS_ConvertASCIItoUTF16(dbkey);
      PR_FREEIF(dbkey);
      if (type == nsIX509Cert::EMAIL_CERT) {
        namestr = node->cert->emailAddr;
      } else {
        namestr = node->cert->nickname;
        if (namestr) {
          char *sc = strchr(namestr, ':');
          if (sc) *sc = DELIM;
        }
      }
      nsAutoString certname = NS_ConvertASCIItoUTF16(namestr ? namestr : "");
      certstr.Append(PRUnichar(DELIM));
      certstr += certname;
      certstr.Append(PRUnichar(DELIM));
      certstr += keystr;
      tmpArray[i++] = ToNewUnicode(certstr);
    }
  }
finish:
  *_count = numcerts;
  *_certNames = tmpArray;
}

/* somewhat follows logic of cert_list_include_cert from PSM 1.x */


NS_IMETHODIMP 
nsNSSCertificateDB::GetIsOcspOn(bool *aOcspOn)
{
  nsCOMPtr<nsIPrefBranch> pref = do_GetService(NS_PREFSERVICE_CONTRACTID);

  PRInt32 ocspEnabled;
  pref->GetIntPref("security.OCSP.enabled", &ocspEnabled);
  *aOcspOn = ( ocspEnabled == 0 ) ? false : true; 
  return NS_OK;
}

/* nsIX509Cert getDefaultEmailEncryptionCert (); */
NS_IMETHODIMP
nsNSSCertificateDB::FindEmailEncryptionCert(const nsAString &aNickname, nsIX509Cert **_retval)
{
  if (!_retval)
    return NS_ERROR_FAILURE;

  *_retval = 0;

  if (aNickname.IsEmpty())
    return NS_OK;

  nsNSSShutDownPreventionLock locker;
  nsresult rv = NS_OK;
  CERTCertificate *cert = 0;
  nsCOMPtr<nsIInterfaceRequestor> ctx = new PipUIContext();
  nsNSSCertificate *nssCert = nsnull;
  char *asciiname = NULL;
  NS_ConvertUTF16toUTF8 aUtf8Nickname(aNickname);
  asciiname = const_cast<char*>(aUtf8Nickname.get());

  /* Find a good cert in the user's database */
  cert = CERT_FindUserCertByUsage(CERT_GetDefaultCertDB(), asciiname, 
           certUsageEmailRecipient, true, ctx);

  if (!cert) { goto loser; }  

  nssCert = nsNSSCertificate::Create(cert);
  if (nssCert == nsnull) {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }
  NS_ADDREF(nssCert);

  *_retval = static_cast<nsIX509Cert*>(nssCert);

loser:
  if (cert) CERT_DestroyCertificate(cert);
  return rv;
}

/* nsIX509Cert getDefaultEmailSigningCert (); */
NS_IMETHODIMP
nsNSSCertificateDB::FindEmailSigningCert(const nsAString &aNickname, nsIX509Cert **_retval)
{
  if (!_retval)
    return NS_ERROR_FAILURE;

  *_retval = 0;

  if (aNickname.IsEmpty())
    return NS_OK;

  nsNSSShutDownPreventionLock locker;
  nsresult rv = NS_OK;
  CERTCertificate *cert = 0;
  nsCOMPtr<nsIInterfaceRequestor> ctx = new PipUIContext();
  nsNSSCertificate *nssCert = nsnull;
  char *asciiname = NULL;
  NS_ConvertUTF16toUTF8 aUtf8Nickname(aNickname);
  asciiname = const_cast<char*>(aUtf8Nickname.get());

  /* Find a good cert in the user's database */
  cert = CERT_FindUserCertByUsage(CERT_GetDefaultCertDB(), asciiname, 
           certUsageEmailSigner, true, ctx);

  if (!cert) { goto loser; }  

  nssCert = nsNSSCertificate::Create(cert);
  if (nssCert == nsnull) {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }
  NS_ADDREF(nssCert);

  *_retval = static_cast<nsIX509Cert*>(nssCert);

loser:
  if (cert) CERT_DestroyCertificate(cert);
  return rv;
}

NS_IMETHODIMP
nsNSSCertificateDB::FindCertByEmailAddress(nsISupports *aToken, const char *aEmailAddress, nsIX509Cert **_retval)
{
  nsNSSShutDownPreventionLock locker;
  
  nsCOMPtr<nsINSSComponent> inss;
  nsRefPtr<nsCERTValInParamWrapper> survivingParams;
  nsresult nsrv;
  
  if (nsNSSComponent::globalConstFlagUsePKIXVerification) {
    inss = do_GetService(kNSSComponentCID, &nsrv);
    if (!inss)
      return nsrv;
    nsrv = inss->GetDefaultCERTValInParam(survivingParams);
    if (NS_FAILED(nsrv))
      return nsrv;
  }

  CERTCertList *certlist = PK11_FindCertsFromEmailAddress(aEmailAddress, nsnull);
  if (!certlist)
    return NS_ERROR_FAILURE;  

  // certlist now contains certificates with the right email address,
  // but they might not have the correct usage or might even be invalid

  CERTCertListCleaner listCleaner(certlist);

  if (CERT_LIST_END(CERT_LIST_HEAD(certlist), certlist))
    return NS_ERROR_FAILURE; // no certs found

  CERTCertListNode *node;
  // search for a valid certificate
  for (node = CERT_LIST_HEAD(certlist);
       !CERT_LIST_END(node, certlist);
       node = CERT_LIST_NEXT(node)) {

    if (!nsNSSComponent::globalConstFlagUsePKIXVerification) {
      if (CERT_VerifyCert(CERT_GetDefaultCertDB(), node->cert,
          true, certUsageEmailRecipient, PR_Now(), nsnull, nsnull) == SECSuccess) {
        // found a valid certificate
        break;
      }
    }
    else {
      CERTValOutParam cvout[1];
      cvout[0].type = cert_po_end;
      if (CERT_PKIXVerifyCert(node->cert, certificateUsageEmailRecipient,
                              survivingParams->GetRawPointerForNSS(),
                              cvout, nsnull)
          == SECSuccess) {
        // found a valid certificate
        break;
      }
    }
  }

  if (CERT_LIST_END(node, certlist)) {
    // no valid cert found
    return NS_ERROR_FAILURE;
  }

  // node now contains the first valid certificate with correct usage 
  nsNSSCertificate *nssCert = nsNSSCertificate::Create(node->cert);
  if (!nssCert)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(nssCert);
  *_retval = static_cast<nsIX509Cert*>(nssCert);
  return NS_OK;
}

/* nsIX509Cert constructX509FromBase64 (in string base64); */
NS_IMETHODIMP
nsNSSCertificateDB::ConstructX509FromBase64(const char *base64,
                                            nsIX509Cert **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  // sure would be nice to have a smart pointer class for PL_ allocations
  // unfortunately, we cannot distinguish out-of-memory from bad-input here
  PRUint32 len = PL_strlen(base64);
  char *certDER = PL_Base64Decode(base64, len, NULL);
  if (!certDER)
    return NS_ERROR_ILLEGAL_VALUE;
  if (!*certDER) {
    PL_strfree(certDER);
    return NS_ERROR_ILLEGAL_VALUE;
  }

  // If we get to this point, we know we had well-formed base64 input;
  // therefore the input string cannot have been less than two
  // characters long.  Compute the unpadded length of the decoded data.
  PRUint32 lengthDER = (len * 3) / 4;
  if (base64[len-1] == '=') {
    lengthDER--;
    if (base64[len-2] == '=')
      lengthDER--;
  }

  nsNSSShutDownPreventionLock locker;
  SECItem secitem_cert;
  secitem_cert.type = siDERCertBuffer;
  secitem_cert.data = (unsigned char*)certDER;
  secitem_cert.len = lengthDER;

  CERTCertificate *cert =
    CERT_NewTempCertificate(CERT_GetDefaultCertDB(), &secitem_cert,
                            nsnull, false, true);
  PL_strfree(certDER);

  if (!cert)
    return (PORT_GetError() == SEC_ERROR_NO_MEMORY)
      ? NS_ERROR_OUT_OF_MEMORY : NS_ERROR_FAILURE;

  nsNSSCertificate *nsNSS = nsNSSCertificate::Create(cert);
  CERT_DestroyCertificate(cert);

  if (!nsNSS)
    return NS_ERROR_OUT_OF_MEMORY;

  return CallQueryInterface(nsNSS, _retval);
}

void
nsNSSCertificateDB::get_default_nickname(CERTCertificate *cert, 
                                         nsIInterfaceRequestor* ctx,
                                         nsCString &nickname)
{
  nickname.Truncate();

  nsNSSShutDownPreventionLock locker;
  nsresult rv;
  CK_OBJECT_HANDLE keyHandle;

  CERTCertDBHandle *defaultcertdb = CERT_GetDefaultCertDB();
  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));
  if (NS_FAILED(rv))
    return;

  nsCAutoString username;
  char *temp_un = CERT_GetCommonName(&cert->subject);
  if (temp_un) {
    username = temp_un;
    PORT_Free(temp_un);
    temp_un = nsnull;
  }

  nsCAutoString caname;
  char *temp_ca = CERT_GetOrgName(&cert->issuer);
  if (temp_ca) {
    caname = temp_ca;
    PORT_Free(temp_ca);
    temp_ca = nsnull;
  }

  nsAutoString tmpNickFmt;
  nssComponent->GetPIPNSSBundleString("nick_template", tmpNickFmt);
  NS_ConvertUTF16toUTF8 nickFmt(tmpNickFmt);

  nsCAutoString baseName;
  char *temp_nn = PR_smprintf(nickFmt.get(), username.get(), caname.get());
  if (!temp_nn) {
    return;
  } else {
    baseName = temp_nn;
    PR_smprintf_free(temp_nn);
    temp_nn = nsnull;
  }

  nickname = baseName;

  /*
   * We need to see if the private key exists on a token, if it does
   * then we need to check for nicknames that already exist on the smart
   * card.
   */
  PK11SlotInfo *slot = PK11_KeyForCertExists(cert, &keyHandle, ctx);
  PK11SlotInfoCleaner slotCleaner(slot);
  if (!slot)
    return;

  if (!PK11_IsInternal(slot)) {
    char *tmp = PR_smprintf("%s:%s", PK11_GetTokenName(slot), baseName.get());
    if (!tmp) {
      nickname.Truncate();
      return;
    }
    baseName = tmp;
    PR_smprintf_free(tmp);

    nickname = baseName;
  }

  int count = 1;
  while (true) {
    if ( count > 1 ) {
      char *tmp = PR_smprintf("%s #%d", baseName.get(), count);
      if (!tmp) {
        nickname.Truncate();
        return;
      }
      nickname = tmp;
      PR_smprintf_free(tmp);
    }

    CERTCertificate *dummycert = nsnull;
    CERTCertificateCleaner dummycertCleaner(dummycert);

    if (PK11_IsInternal(slot)) {
      /* look up the nickname to make sure it isn't in use already */
      dummycert = CERT_FindCertByNickname(defaultcertdb, nickname.get());

    } else {
      /*
       * Check the cert against others that already live on the smart 
       * card.
       */
      dummycert = PK11_FindCertFromNickname(nickname.get(), ctx);
      if (dummycert != NULL) {
	/*
	 * Make sure the subject names are different.
	 */ 
	if (CERT_CompareName(&cert->subject, &dummycert->subject) == SECEqual)
	{
	  /*
	   * There is another certificate with the same nickname and
	   * the same subject name on the smart card, so let's use this
	   * nickname.
	   */
	  CERT_DestroyCertificate(dummycert);
	  dummycert = NULL;
	}
      }
    }
    if (!dummycert) 
      break;
    
    count++;
  }
}

NS_IMETHODIMP nsNSSCertificateDB::AddCertFromBase64(const char *aBase64, const char *aTrust, const char *aName)
{
  NS_ENSURE_ARG_POINTER(aBase64);
  nsCOMPtr <nsIX509Cert> newCert;

  nsNSSCertTrust trust;

  // need to calculate the trust bits from the aTrust string.
  nsresult rv = CERT_DecodeTrustString(trust.GetTrust(), /* this is const, but not declared that way */(char *) aTrust);
  NS_ENSURE_SUCCESS(rv, rv); // if bad trust passed in, return error.


  rv = ConstructX509FromBase64(aBase64, getter_AddRefs(newCert));
  NS_ENSURE_SUCCESS(rv, rv);

  SECItem der;
  rv = newCert->GetRawDER(&der.len, (PRUint8 **)&der.data);
  NS_ENSURE_SUCCESS(rv, rv);

  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("Creating temp cert\n"));
  CERTCertificate *tmpCert;
  CERTCertDBHandle *certdb = CERT_GetDefaultCertDB();
  tmpCert = CERT_FindCertByDERCert(certdb, &der);
  if (!tmpCert) 
    tmpCert = CERT_NewTempCertificate(certdb, &der,
                                      nsnull, false, true);
  nsMemory::Free(der.data);
  der.data = nsnull;
  der.len = 0;

  if (!tmpCert) {
    NS_ERROR("Couldn't create cert from DER blob");
    return NS_ERROR_FAILURE;
  }

  if (tmpCert->isperm) {
    CERT_DestroyCertificate(tmpCert);
    return NS_OK;
  }

  CERTCertificateCleaner tmpCertCleaner(tmpCert);

  nsXPIDLCString nickname;
  nickname.Adopt(CERT_MakeCANickname(tmpCert));

  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("Created nick \"%s\"\n", nickname.get()));

  SECStatus srv = CERT_AddTempCertToPerm(tmpCert, 
                                         const_cast<char*>(nickname.get()), 
                                         trust.GetTrust()); 


  return (srv == SECSuccess) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP 
nsNSSCertificateDB::GetCerts(nsIX509CertList **_retval)
{
  CERTCertList *certList;

  nsCOMPtr<nsIInterfaceRequestor> ctx = new PipUIContext();
  nsCOMPtr<nsIX509CertList> nssCertList;
  certList = PK11_ListCerts(PK11CertListUnique, ctx);

  // nsNSSCertList 1) adopts certList, and 2) handles the NULL case fine.
  // (returns an empty list) 
  nssCertList = new nsNSSCertList(certList, true);
  if (!nssCertList) { return NS_ERROR_OUT_OF_MEMORY; }

  *_retval = nssCertList;
  NS_ADDREF(*_retval);
  return NS_OK;
}
