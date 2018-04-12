/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNSSCertificateDB.h"

#include "CertVerifier.h"
#include "CryptoTask.h"
#include "ExtendedValidation.h"
#include "NSSCertDBTrustDomain.h"
#include "SharedSSLState.h"
#include "certdb.h"
#include "mozilla/Assertions.h"
#include "mozilla/Base64.h"
#include "mozilla/Casting.h"
#include "mozilla/Unused.h"
#include "nsArray.h"
#include "nsArrayUtils.h"
#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsICertificateDialogs.h"
#include "nsIFile.h"
#include "nsIMutableArray.h"
#include "nsIObserverService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsIPrompt.h"
#include "nsNSSCertHelper.h"
#include "nsNSSCertTrust.h"
#include "nsNSSCertificate.h"
#include "nsNSSComponent.h"
#include "nsNSSHelper.h"
#include "nsPKCS12Blob.h"
#include "nsPromiseFlatString.h"
#include "nsProxyRelease.h"
#include "nsReadableUtils.h"
#include "nsThreadUtils.h"
#include "nspr.h"
#include "pkix/Time.h"
#include "pkix/pkixnss.h"
#include "pkix/pkixtypes.h"
#include "secasn1.h"
#include "secder.h"
#include "secerr.h"
#include "ssl.h"

#ifdef XP_WIN
#include <winsock.h> // for ntohl
#endif

using namespace mozilla;
using namespace mozilla::psm;
using mozilla::psm::SharedSSLState;

extern LazyLogModule gPIPNSSLog;

NS_IMPL_ISUPPORTS(nsNSSCertificateDB, nsIX509CertDB)

NS_IMETHODIMP
nsNSSCertificateDB::FindCertByDBKey(const nsACString& aDBKey,
                            /*out*/ nsIX509Cert** _cert)
{
  NS_ENSURE_ARG_POINTER(_cert);
  *_cert = nullptr;

  if (aDBKey.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  nsresult rv = BlockUntilLoadableRootsLoaded();
  if (NS_FAILED(rv)) {
    return rv;
  }

  UniqueCERTCertificate cert;
  rv = FindCertByDBKey(aDBKey, cert);
  if (NS_FAILED(rv)) {
    return rv;
  }
  // If we can't find the certificate, that's not an error. Just return null.
  if (!cert) {
    return NS_OK;
  }
  nsCOMPtr<nsIX509Cert> nssCert = nsNSSCertificate::Create(cert.get());
  if (!nssCert) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nssCert.forget(_cert);
  return NS_OK;
}

nsresult
nsNSSCertificateDB::FindCertByDBKey(const nsACString& aDBKey,
                                    UniqueCERTCertificate& cert)
{
  static_assert(sizeof(uint64_t) == 8, "type size sanity check");
  static_assert(sizeof(uint32_t) == 4, "type size sanity check");
  // (From nsNSSCertificate::GetDbKey)
  // The format of the key is the base64 encoding of the following:
  // 4 bytes: {0, 0, 0, 0} (this was intended to be the module ID, but it was
  //                        never implemented)
  // 4 bytes: {0, 0, 0, 0} (this was intended to be the slot ID, but it was
  //                        never implemented)
  // 4 bytes: <serial number length in big-endian order>
  // 4 bytes: <DER-encoded issuer distinguished name length in big-endian order>
  // n bytes: <bytes of serial number>
  // m bytes: <DER-encoded issuer distinguished name>
  nsAutoCString decoded;
  nsAutoCString tmpDBKey(aDBKey);
  // Filter out any whitespace for backwards compatibility.
  tmpDBKey.StripWhitespace();
  nsresult rv = Base64Decode(tmpDBKey, decoded);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (decoded.Length() < 16) {
    return NS_ERROR_ILLEGAL_INPUT;
  }
  const char* reader = decoded.BeginReading();
  uint64_t zeroes = *BitwiseCast<const uint64_t*, const char*>(reader);
  if (zeroes != 0) {
    return NS_ERROR_ILLEGAL_INPUT;
  }
  reader += sizeof(uint64_t);
  // Note: We surround the ntohl() argument with parentheses to stop the macro
  //       from thinking two arguments were passed.
  uint32_t serialNumberLen = ntohl(
    (*BitwiseCast<const uint32_t*, const char*>(reader)));
  reader += sizeof(uint32_t);
  uint32_t issuerLen = ntohl(
    (*BitwiseCast<const uint32_t*, const char*>(reader)));
  reader += sizeof(uint32_t);
  if (decoded.Length() != 16ULL + serialNumberLen + issuerLen) {
    return NS_ERROR_ILLEGAL_INPUT;
  }
  CERTIssuerAndSN issuerSN;
  issuerSN.serialNumber.len = serialNumberLen;
  issuerSN.serialNumber.data = BitwiseCast<unsigned char*, const char*>(reader);
  reader += serialNumberLen;
  issuerSN.derIssuer.len = issuerLen;
  issuerSN.derIssuer.data = BitwiseCast<unsigned char*, const char*>(reader);
  reader += issuerLen;
  MOZ_ASSERT(reader == decoded.EndReading());

  cert.reset(CERT_FindCertByIssuerAndSN(CERT_GetDefaultCertDB(), &issuerSN));
  return NS_OK;
}

SECStatus
collect_certs(void *arg, SECItem **certs, int numcerts)
{
  CERTDERCerts *collectArgs;
  SECItem *cert;
  SECStatus rv;

  collectArgs = (CERTDERCerts *)arg;

  collectArgs->numcerts = numcerts;
  collectArgs->rawCerts = (SECItem *) PORT_ArenaZAlloc(collectArgs->arena,
                                           sizeof(SECItem) * numcerts);
  if (!collectArgs->rawCerts)
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
nsNSSCertificateDB::getCertsFromPackage(const UniquePLArenaPool& arena,
                                        uint8_t* data, uint32_t length)
{
  CERTDERCerts* collectArgs = PORT_ArenaZNew(arena.get(), CERTDERCerts);
  if (!collectArgs) {
    return nullptr;
  }

  collectArgs->arena = arena.get();
  if (CERT_DecodeCertPackage(BitwiseCast<char*, uint8_t*>(data), length,
                             collect_certs, collectArgs) != SECSuccess) {
    return nullptr;
  }

  return collectArgs;
}

// When using the sql-backed softoken, trust settings are authenticated using a
// key in the secret database. Thus, if the user has a password, we need to
// authenticate to the token in order to be able to change trust settings.
SECStatus
ChangeCertTrustWithPossibleAuthentication(const UniqueCERTCertificate& cert,
                                          CERTCertTrust& trust, void* ctx)
{
  MOZ_ASSERT(cert, "cert must be non-null");
  if (!cert) {
    PR_SetError(SEC_ERROR_LIBRARY_FAILURE, 0);
    return SECFailure;
  }
  // NSS ignores the first argument to CERT_ChangeCertTrust
  SECStatus srv = CERT_ChangeCertTrust(nullptr, cert.get(), &trust);
  if (srv == SECSuccess || PR_GetError() != SEC_ERROR_TOKEN_NOT_LOGGED_IN) {
    return srv;
  }
  if (cert->slot) {
    // If this certificate is on an external PKCS#11 token, we have to
    // authenticate to that token.
    srv = PK11_Authenticate(cert->slot, PR_TRUE, ctx);
  } else {
    // Otherwise, the certificate is on the internal module.
    UniquePK11SlotInfo internalSlot(PK11_GetInternalKeySlot());
    srv = PK11_Authenticate(internalSlot.get(), PR_TRUE, ctx);
  }
  if (srv != SECSuccess) {
    return srv;
  }
  return CERT_ChangeCertTrust(nullptr, cert.get(), &trust);
}

static nsresult
ImportCertsIntoPermanentStorage(const UniqueCERTCertList& certChain)
{
  bool encounteredFailure = false;
  PRErrorCode savedErrorCode = 0;
  UniquePK11SlotInfo slot(PK11_GetInternalKeySlot());
  for (CERTCertListNode* chainNode = CERT_LIST_HEAD(certChain);
       !CERT_LIST_END(chainNode, certChain);
       chainNode = CERT_LIST_NEXT(chainNode)) {
    UniquePORTString nickname(CERT_MakeCANickname(chainNode->cert));
    SECStatus srv = PK11_ImportCert(slot.get(), chainNode->cert,
                                    CK_INVALID_HANDLE, nickname.get(),
                                    false); // this parameter is ignored by NSS
    if (srv != SECSuccess) {
      encounteredFailure = true;
      savedErrorCode = PR_GetError();
    }
  }

  if (encounteredFailure) {
    return GetXPCOMFromNSSError(savedErrorCode);
  }

  return NS_OK;
}

nsresult
nsNSSCertificateDB::handleCACertDownload(NotNull<nsIArray*> x509Certs,
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

  uint32_t numCerts;

  x509Certs->GetLength(&numCerts);
  MOZ_ASSERT(numCerts > 0, "Didn't get any certs to import.");
  if (numCerts == 0)
    return NS_OK; // Nothing to import, so nothing to do.

  nsCOMPtr<nsIX509Cert> certToShow;
  uint32_t selCertIndex;
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

    nsAutoString cert0SubjectName;
    nsAutoString cert1IssuerName;
    nsAutoString certn_2IssuerName;
    nsAutoString certn_1SubjectName;

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
  if (NS_FAILED(rv)) {
    return rv;
  }

  UniqueCERTCertificate tmpCert(certToShow->GetCert());
  if (!tmpCert) {
    return NS_ERROR_FAILURE;
  }

  if (!CERT_IsCACert(tmpCert.get(), nullptr)) {
    DisplayCertificateAlert(ctx, "NotACACert", certToShow);
    return NS_ERROR_FAILURE;
  }

  if (tmpCert->isperm) {
    DisplayCertificateAlert(ctx, "CaCertExists", certToShow);
    return NS_ERROR_FAILURE;
  }

  uint32_t trustBits;
  bool allows;
  rv = dialogs->ConfirmDownloadCACert(ctx, certToShow, &trustBits, &allows);
  if (NS_FAILED(rv))
    return rv;

  if (!allows)
    return NS_ERROR_NOT_AVAILABLE;

  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("trust is %d\n", trustBits));
  UniquePORTString nickname(CERT_MakeCANickname(tmpCert.get()));

  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("Created nick \"%s\"\n", nickname.get()));

  nsNSSCertTrust trust;
  trust.SetValidCA();
  trust.AddCATrust(!!(trustBits & nsIX509CertDB::TRUSTED_SSL),
                   !!(trustBits & nsIX509CertDB::TRUSTED_EMAIL));

  UniquePK11SlotInfo slot(PK11_GetInternalKeySlot());
  SECStatus srv = PK11_ImportCert(slot.get(), tmpCert.get(), CK_INVALID_HANDLE,
                                  nickname.get(),
                                  false); // this parameter is ignored by NSS
  if (srv != SECSuccess) {
    return MapSECStatus(srv);
  }
  srv = ChangeCertTrustWithPossibleAuthentication(tmpCert, trust.GetTrust(),
                                                  ctx);
  if (srv != SECSuccess) {
    return MapSECStatus(srv);
  }

  // Import additional delivered certificates that can be verified.

  // build a CertList for filtering
  UniqueCERTCertList certList(CERT_NewCertList());
  if (!certList) {
    return NS_ERROR_FAILURE;
  }

  // get all remaining certs into temp store

  for (uint32_t i=0; i<numCerts; i++) {
    if (i == selCertIndex) {
      // we already processed that one
      continue;
    }

    nsCOMPtr<nsIX509Cert> remainingCert = do_QueryElementAt(x509Certs, i);
    if (!remainingCert) {
      continue;
    }

    UniqueCERTCertificate tmpCert2(remainingCert->GetCert());
    if (!tmpCert2) {
      continue;  // Let's try to import the rest of 'em
    }

    if (CERT_AddCertToListTail(certList.get(), tmpCert2.get()) != SECSuccess) {
      continue;
    }

    Unused << tmpCert2.release();
  }

  return ImportCertsIntoPermanentStorage(certList);
}

NS_IMETHODIMP
nsNSSCertificateDB::ImportCertificates(uint8_t* data, uint32_t length,
                                       uint32_t type,
                                       nsIInterfaceRequestor* ctx)
{
  // We currently only handle CA certificates.
  if (type != nsIX509Cert::CA_CERT) {
    return NS_ERROR_FAILURE;
  }

  UniquePLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
  if (!arena) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  CERTDERCerts* certCollection = getCertsFromPackage(arena, data, length);
  if (!certCollection) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIMutableArray> array = nsArrayBase::Create();
  if (!array) {
    return NS_ERROR_FAILURE;
  }

  // Now let's create some certs to work with
  for (int i = 0; i < certCollection->numcerts; i++) {
    SECItem* currItem = &certCollection->rawCerts[i];
    nsCOMPtr<nsIX509Cert> cert = nsNSSCertificate::ConstructFromDER(
      BitwiseCast<char*, unsigned char*>(currItem->data), currItem->len);
    if (!cert) {
      return NS_ERROR_FAILURE;
    }
    nsresult rv = array->AppendElement(cert);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  return handleCACertDownload(WrapNotNull(array), ctx);
}

/**
 * Decodes a given array of DER-encoded certificates into temporary storage.
 *
 * @param numcerts
 *        Size of the |certs| array.
 * @param certs
 *        Pointer to array of certs to decode.
 * @param temporaryCerts
 *        List of decoded certificates.
 */
static nsresult
ImportCertsIntoTempStorage(int numcerts, SECItem* certs,
                   /*out*/ const UniqueCERTCertList& temporaryCerts)
{
  NS_ENSURE_ARG_MIN(numcerts, 1);
  NS_ENSURE_ARG_POINTER(certs);
  NS_ENSURE_ARG_POINTER(temporaryCerts);

  // CERT_ImportCerts() expects an array of *pointers* to SECItems, so we have
  // to convert |certs| to such a format first.
  SECItem** ptrArray =
    static_cast<SECItem**>(PORT_Alloc(sizeof(SECItem*) * numcerts));
  if (!ptrArray) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  for (int i = 0; i < numcerts; i++) {
    ptrArray[i] = &certs[i];
  }

  CERTCertificate** importedCerts = nullptr;
  SECStatus srv = CERT_ImportCerts(CERT_GetDefaultCertDB(),
                                   certUsageAnyCA, // this argument is ignored
                                   numcerts, ptrArray, &importedCerts, false,
                                   false, // this argument is ignored
                                   nullptr);
  PORT_Free(ptrArray);
  ptrArray = nullptr;
  if (srv != SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  for (int i = 0; i < numcerts; i++) {
    if (!importedCerts[i]) {
      continue;
    }

    UniqueCERTCertificate cert(CERT_DupCertificate(importedCerts[i]));
    if (!cert) {
      continue;
    }

    if (CERT_AddCertToListTail(temporaryCerts.get(), cert.get()) == SECSuccess) {
      Unused << cert.release();
    }
  }

  CERT_DestroyCertArray(importedCerts, numcerts);

  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificateDB::ImportEmailCertificate(uint8_t* data, uint32_t length,
                                           nsIInterfaceRequestor* ctx)
{
  UniquePLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
  if (!arena) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  CERTDERCerts *certCollection = getCertsFromPackage(arena, data, length);
  if (!certCollection) {
    return NS_ERROR_FAILURE;
  }

  UniqueCERTCertList temporaryCerts(CERT_NewCertList());
  if (!temporaryCerts) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = ImportCertsIntoTempStorage(certCollection->numcerts,
                                           certCollection->rawCerts,
                                           temporaryCerts);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return ImportCertsIntoPermanentStorage(temporaryCerts);
}

nsresult
nsNSSCertificateDB::ImportCACerts(int numCACerts, SECItem* caCerts,
                                  nsIInterfaceRequestor* ctx)
{
  UniqueCERTCertList temporaryCerts(CERT_NewCertList());
  if (!temporaryCerts) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = ImportCertsIntoTempStorage(numCACerts, caCerts, temporaryCerts);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return ImportCertsIntoPermanentStorage(temporaryCerts);
}

void nsNSSCertificateDB::DisplayCertificateAlert(nsIInterfaceRequestor *ctx,
                                                 const char *stringID,
                                                 nsIX509Cert *certToShow)
{
  static NS_DEFINE_CID(kNSSComponentCID, NS_NSSCOMPONENT_CID);

  if (!NS_IsMainThread()) {
    NS_ERROR("nsNSSCertificateDB::DisplayCertificateAlert called off the main thread");
    return;
  }

  nsCOMPtr<nsIInterfaceRequestor> my_ctx = ctx;
  if (!my_ctx) {
    my_ctx = new PipUIContext();
  }

  // This shall be replaced by embedding ovverridable prompts
  // as discussed in bug 310446, and should make use of certToShow.

  nsresult rv;
  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));
  if (NS_SUCCEEDED(rv)) {
    nsAutoString tmpMessage;
    nssComponent->GetPIPNSSBundleString(stringID, tmpMessage);

    nsCOMPtr<nsIPrompt> prompt (do_GetInterface(my_ctx));
    if (!prompt) {
      return;
    }

    prompt->Alert(nullptr, tmpMessage.get());
  }
}

NS_IMETHODIMP
nsNSSCertificateDB::ImportUserCertificate(uint8_t* data, uint32_t length,
                                          nsIInterfaceRequestor* ctx)
{
  if (!NS_IsMainThread()) {
    NS_ERROR("nsNSSCertificateDB::ImportUserCertificate called off the main thread");
    return NS_ERROR_NOT_SAME_THREAD;
  }

  UniquePLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
  if (!arena) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  CERTDERCerts* collectArgs = getCertsFromPackage(arena, data, length);
  if (!collectArgs) {
    return NS_ERROR_FAILURE;
  }

  UniqueCERTCertificate cert(
    CERT_NewTempCertificate(CERT_GetDefaultCertDB(), collectArgs->rawCerts,
                            nullptr, false, true));
  if (!cert) {
    return NS_ERROR_FAILURE;
  }

  UniquePK11SlotInfo slot(PK11_KeyForCertExists(cert.get(), nullptr, ctx));
  if (!slot) {
    nsCOMPtr<nsIX509Cert> certToShow = nsNSSCertificate::Create(cert.get());
    DisplayCertificateAlert(ctx, "UserCertIgnoredNoPrivateKey", certToShow);
    return NS_ERROR_FAILURE;
  }
  slot = nullptr;

  /* pick a nickname for the cert */
  nsAutoCString nickname;
  if (cert->nickname) {
    nickname = cert->nickname;
  } else {
    get_default_nickname(cert.get(), ctx, nickname);
  }

  /* user wants to import the cert */
  slot.reset(PK11_ImportCertForKey(cert.get(), nickname.get(), ctx));
  if (!slot) {
    return NS_ERROR_FAILURE;
  }
  slot = nullptr;

  {
    nsCOMPtr<nsIX509Cert> certToShow = nsNSSCertificate::Create(cert.get());
    DisplayCertificateAlert(ctx, "UserCertImported", certToShow);
  }

  nsresult rv = NS_OK;
  int numCACerts = collectArgs->numcerts - 1;
  if (numCACerts) {
    SECItem* caCerts = collectArgs->rawCerts + 1;
    rv = ImportCACerts(numCACerts, caCerts, ctx);
  }

  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (observerService) {
    observerService->NotifyObservers(nullptr, "psm:user-certificate-added", nullptr);
  }

  return rv;
}

NS_IMETHODIMP
nsNSSCertificateDB::DeleteCertificate(nsIX509Cert *aCert)
{
  NS_ENSURE_ARG_POINTER(aCert);
  UniqueCERTCertificate cert(aCert->GetCert());
  if (!cert) {
    return NS_ERROR_FAILURE;
  }
  SECStatus srv = SECSuccess;

  uint32_t certType;
  aCert->GetCertType(&certType);
  if (NS_FAILED(aCert->MarkForPermDeletion()))
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
    nsNSSCertTrust trust(0, 0);
    srv = ChangeCertTrustWithPossibleAuthentication(cert, trust.GetTrust(),
                                                    nullptr);
  }
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("cert deleted: %d", srv));

  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (observerService) {
    observerService->NotifyObservers(nullptr, "psm:user-certificate-deleted", nullptr);
  }

  return (srv) ? NS_ERROR_FAILURE : NS_OK;
}

NS_IMETHODIMP
nsNSSCertificateDB::SetCertTrust(nsIX509Cert *cert,
                                 uint32_t type,
                                 uint32_t trusted)
{
  NS_ENSURE_ARG_POINTER(cert);
  nsNSSCertTrust trust;
  switch (type) {
    case nsIX509Cert::CA_CERT:
      trust.SetValidCA();
      trust.AddCATrust(!!(trusted & nsIX509CertDB::TRUSTED_SSL),
                       !!(trusted & nsIX509CertDB::TRUSTED_EMAIL));
      break;
    case nsIX509Cert::SERVER_CERT:
      trust.SetValidPeer();
      trust.AddPeerTrust(trusted & nsIX509CertDB::TRUSTED_SSL, false);
      break;
    case nsIX509Cert::EMAIL_CERT:
      trust.SetValidPeer();
      trust.AddPeerTrust(false, !!(trusted & nsIX509CertDB::TRUSTED_EMAIL));
      break;
    default:
      // Ignore any other type of certificate (including invalid types).
      return NS_OK;
  }

  UniqueCERTCertificate nsscert(cert->GetCert());
  SECStatus srv = ChangeCertTrustWithPossibleAuthentication(nsscert,
                                                            trust.GetTrust(),
                                                            nullptr);
  return MapSECStatus(srv);
}

NS_IMETHODIMP
nsNSSCertificateDB::IsCertTrusted(nsIX509Cert *cert,
                                  uint32_t certType,
                                  uint32_t trustType,
                                  bool *_isTrusted)
{
  NS_ENSURE_ARG_POINTER(_isTrusted);
  *_isTrusted = false;

  nsresult rv = BlockUntilLoadableRootsLoaded();
  if (NS_FAILED(rv)) {
    return rv;
  }

  SECStatus srv;
  UniqueCERTCertificate nsscert(cert->GetCert());
  CERTCertTrust nsstrust;
  srv = CERT_GetCertTrust(nsscert.get(), &nsstrust);
  if (srv != SECSuccess)
    return NS_ERROR_FAILURE;

  nsNSSCertTrust trust(&nsstrust);
  if (certType == nsIX509Cert::CA_CERT) {
    if (trustType & nsIX509CertDB::TRUSTED_SSL) {
      *_isTrusted = trust.HasTrustedCA(true, false);
    } else if (trustType & nsIX509CertDB::TRUSTED_EMAIL) {
      *_isTrusted = trust.HasTrustedCA(false, true);
    } else {
      return NS_ERROR_FAILURE;
    }
  } else if (certType == nsIX509Cert::SERVER_CERT) {
    if (trustType & nsIX509CertDB::TRUSTED_SSL) {
      *_isTrusted = trust.HasTrustedPeer(true, false);
    } else if (trustType & nsIX509CertDB::TRUSTED_EMAIL) {
      *_isTrusted = trust.HasTrustedPeer(false, true);
    } else {
      return NS_ERROR_FAILURE;
    }
  } else if (certType == nsIX509Cert::EMAIL_CERT) {
    if (trustType & nsIX509CertDB::TRUSTED_SSL) {
      *_isTrusted = trust.HasTrustedPeer(true, false);
    } else if (trustType & nsIX509CertDB::TRUSTED_EMAIL) {
      *_isTrusted = trust.HasTrustedPeer(false, true);
    } else {
      return NS_ERROR_FAILURE;
    }
  } /* user: ignore */
  return NS_OK;
}


NS_IMETHODIMP
nsNSSCertificateDB::ImportCertsFromFile(nsIFile* aFile, uint32_t aType)
{
  NS_ENSURE_ARG(aFile);
  switch (aType) {
    case nsIX509Cert::CA_CERT:
    case nsIX509Cert::EMAIL_CERT:
      // good
      break;

    default:
      // not supported (yet)
      return NS_ERROR_FAILURE;
  }

  PRFileDesc* fd = nullptr;
  nsresult rv = aFile->OpenNSPRFileDesc(PR_RDONLY, 0, &fd);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (!fd) {
    return NS_ERROR_FAILURE;
  }

  PRFileInfo fileInfo;
  if (PR_GetOpenFileInfo(fd, &fileInfo) != PR_SUCCESS) {
    return NS_ERROR_FAILURE;
  }

  auto buf = MakeUnique<unsigned char[]>(fileInfo.size);
  int32_t bytesObtained = PR_Read(fd, buf.get(), fileInfo.size);
  PR_Close(fd);

  if (bytesObtained != fileInfo.size) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIInterfaceRequestor> cxt = new PipUIContext();

  switch (aType) {
    case nsIX509Cert::CA_CERT:
      return ImportCertificates(buf.get(), bytesObtained, aType, cxt);
    case nsIX509Cert::EMAIL_CERT:
      return ImportEmailCertificate(buf.get(), bytesObtained, cxt);
    default:
      MOZ_ASSERT(false, "Unsupported type should have been filtered out");
      break;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsNSSCertificateDB::ImportPKCS12File(nsIFile* aFile)
{
  if (!NS_IsMainThread()) {
    return NS_ERROR_NOT_SAME_THREAD;
  }
  nsresult rv = BlockUntilLoadableRootsLoaded();
  if (NS_FAILED(rv)) {
    return rv;
  }

  NS_ENSURE_ARG(aFile);
  nsPKCS12Blob blob;
  rv = blob.ImportFromFile(aFile);

  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (NS_SUCCEEDED(rv) && observerService) {
    observerService->NotifyObservers(nullptr, "psm:user-certificate-added", nullptr);
  }

  return rv;
}

NS_IMETHODIMP
nsNSSCertificateDB::ExportPKCS12File(nsIFile* aFile, uint32_t count,
                                     nsIX509Cert** certs)
{
  if (!NS_IsMainThread()) {
    return NS_ERROR_NOT_SAME_THREAD;
  }
  nsresult rv = BlockUntilLoadableRootsLoaded();
  if (NS_FAILED(rv)) {
    return rv;
  }

  NS_ENSURE_ARG(aFile);
  if (count == 0) {
    return NS_OK;
  }
  nsPKCS12Blob blob;
  return blob.ExportToFile(aFile, certs, count);
}

NS_IMETHODIMP
nsNSSCertificateDB::ConstructX509FromBase64(const nsACString& base64,
                                    /*out*/ nsIX509Cert** _retval)
{
  if (!_retval) {
    return NS_ERROR_INVALID_POINTER;
  }

  // Base64Decode() doesn't consider a zero length input as an error, and just
  // returns the empty string. We don't want this behavior, so the below check
  // catches this case.
  if (base64.Length() < 1) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  nsAutoCString certDER;
  nsresult rv = Base64Decode(base64, certDER);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return ConstructX509(certDER, _retval);
}

NS_IMETHODIMP
nsNSSCertificateDB::ConstructX509(const nsACString& certDER,
                                  nsIX509Cert** _retval)
{
  if (NS_WARN_IF(!_retval)) {
    return NS_ERROR_INVALID_POINTER;
  }

  SECItem certData;
  certData.type = siDERCertBuffer;
  certData.data = BitwiseCast<unsigned char*, const char*>(certDER.BeginReading());
  certData.len = certDER.Length();

  UniqueCERTCertificate cert(CERT_NewTempCertificate(CERT_GetDefaultCertDB(),
                                                     &certData, nullptr,
                                                     false, true));
  if (!cert)
    return (PORT_GetError() == SEC_ERROR_NO_MEMORY)
      ? NS_ERROR_OUT_OF_MEMORY : NS_ERROR_FAILURE;

  nsCOMPtr<nsIX509Cert> nssCert = nsNSSCertificate::Create(cert.get());
  if (!nssCert) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nssCert.forget(_retval);
  return NS_OK;
}

void
nsNSSCertificateDB::get_default_nickname(CERTCertificate *cert,
                                         nsIInterfaceRequestor* ctx,
                                         nsCString &nickname)
{
  static NS_DEFINE_CID(kNSSComponentCID, NS_NSSCOMPONENT_CID);

  nickname.Truncate();

  nsresult rv;
  CK_OBJECT_HANDLE keyHandle;

  if (NS_FAILED(BlockUntilLoadableRootsLoaded())) {
    return;
  }

  CERTCertDBHandle *defaultcertdb = CERT_GetDefaultCertDB();
  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));
  if (NS_FAILED(rv))
    return;

  nsAutoCString username;
  UniquePORTString tempCN(CERT_GetCommonName(&cert->subject));
  if (tempCN) {
    username = tempCN.get();
  }

  nsAutoCString caname;
  UniquePORTString tempIssuerOrg(CERT_GetOrgName(&cert->issuer));
  if (tempIssuerOrg) {
    caname = tempIssuerOrg.get();
  }

  nsAutoString tmpNickFmt;
  nssComponent->GetPIPNSSBundleString("nick_template", tmpNickFmt);
  NS_ConvertUTF16toUTF8 nickFmt(tmpNickFmt);

  nsAutoCString baseName;
  baseName.AppendPrintf(nickFmt.get(), username.get(), caname.get());
  if (baseName.IsEmpty()) {
    return;
  }

  nickname = baseName;

  /*
   * We need to see if the private key exists on a token, if it does
   * then we need to check for nicknames that already exist on the smart
   * card.
   */
  UniquePK11SlotInfo slot(PK11_KeyForCertExists(cert, &keyHandle, ctx));
  if (!slot)
    return;

  if (!PK11_IsInternal(slot.get())) {
    nsAutoCString tmp;
    tmp.AppendPrintf("%s:%s", PK11_GetTokenName(slot.get()), baseName.get());
    if (tmp.IsEmpty()) {
      nickname.Truncate();
      return;
    }
    baseName = tmp;
    nickname = baseName;
  }

  int count = 1;
  while (true) {
    if ( count > 1 ) {
      nsAutoCString tmp;
      tmp.AppendPrintf("%s #%d", baseName.get(), count);
      if (tmp.IsEmpty()) {
        nickname.Truncate();
        return;
      }
      nickname = tmp;
    }

    UniqueCERTCertificate dummycert;

    if (PK11_IsInternal(slot.get())) {
      /* look up the nickname to make sure it isn't in use already */
      dummycert.reset(CERT_FindCertByNickname(defaultcertdb, nickname.get()));
    } else {
      // Check the cert against others that already live on the smart card.
      dummycert.reset(PK11_FindCertFromNickname(nickname.get(), ctx));
      if (dummycert) {
	// Make sure the subject names are different.
	if (CERT_CompareName(&cert->subject, &dummycert->subject) == SECEqual)
	{
	  /*
	   * There is another certificate with the same nickname and
	   * the same subject name on the smart card, so let's use this
	   * nickname.
	   */
	  dummycert = nullptr;
	}
      }
    }
    if (!dummycert) {
      break;
    }
    count++;
  }
}

NS_IMETHODIMP
nsNSSCertificateDB::AddCertFromBase64(const nsACString& aBase64,
                                      const nsACString& aTrust,
                                      nsIX509Cert** addedCertificate)
{
  MOZ_ASSERT(addedCertificate);
  if (!addedCertificate) {
    return NS_ERROR_INVALID_ARG;
  }
  *addedCertificate = nullptr;

  nsNSSCertTrust trust;
  if (CERT_DecodeTrustString(&trust.GetTrust(), PromiseFlatCString(aTrust).get())
        != SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIX509Cert> newCert;
  nsresult rv = ConstructX509FromBase64(aBase64, getter_AddRefs(newCert));
  if (NS_FAILED(rv)) {
    return rv;
  }

  UniqueCERTCertificate tmpCert(newCert->GetCert());
  if (!tmpCert) {
    return NS_ERROR_FAILURE;
  }

  // If there's already a certificate that matches this one in the database, we
  // still want to set its trust to the given value.
  if (tmpCert->isperm) {
    rv = SetCertTrustFromString(newCert, aTrust);
    if (NS_FAILED(rv)) {
      return rv;
    }
    newCert.forget(addedCertificate);
    return NS_OK;
  }

  UniquePORTString nickname(CERT_MakeCANickname(tmpCert.get()));

  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("Created nick \"%s\"\n", nickname.get()));

  UniquePK11SlotInfo slot(PK11_GetInternalKeySlot());
  SECStatus srv = PK11_ImportCert(slot.get(), tmpCert.get(), CK_INVALID_HANDLE,
                                  nickname.get(),
                                  false); // this parameter is ignored by NSS
  if (srv != SECSuccess) {
    return MapSECStatus(srv);
  }
  srv = ChangeCertTrustWithPossibleAuthentication(tmpCert, trust.GetTrust(),
                                                  nullptr);
  if (srv != SECSuccess) {
    return MapSECStatus(srv);
  }
  newCert.forget(addedCertificate);
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificateDB::AddCert(const nsACString& aCertDER,
                            const nsACString& aTrust,
                            nsIX509Cert** addedCertificate)
{
  nsCString base64;
  nsresult rv = Base64Encode(aCertDER, base64);
  NS_ENSURE_SUCCESS(rv, rv);
  return AddCertFromBase64(base64, aTrust, addedCertificate);
}

NS_IMETHODIMP
nsNSSCertificateDB::SetCertTrustFromString(nsIX509Cert* cert,
                                           const nsACString& trustString)
{
  NS_ENSURE_ARG(cert);

  CERTCertTrust trust;
  SECStatus srv = CERT_DecodeTrustString(&trust,
                                         PromiseFlatCString(trustString).get());
  if (srv != SECSuccess) {
    return MapSECStatus(srv);
  }
  UniqueCERTCertificate nssCert(cert->GetCert());

  srv = ChangeCertTrustWithPossibleAuthentication(nssCert, trust, nullptr);
  return MapSECStatus(srv);
}

NS_IMETHODIMP
nsNSSCertificateDB::GetCerts(nsIX509CertList **_retval)
{
  nsresult rv = BlockUntilLoadableRootsLoaded();
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = CheckForSmartCardChanges();
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIInterfaceRequestor> ctx = new PipUIContext();
  nsCOMPtr<nsIX509CertList> nssCertList;
  UniqueCERTCertList certList(PK11_ListCerts(PK11CertListUnique, ctx));

  // nsNSSCertList 1) adopts certList, and 2) handles the nullptr case fine.
  // (returns an empty list)
  nssCertList = new nsNSSCertList(Move(certList));

  nssCertList.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificateDB::GetEnterpriseRoots(nsIX509CertList** enterpriseRoots)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!NS_IsMainThread()) {
    return NS_ERROR_NOT_SAME_THREAD;
  }

  NS_ENSURE_ARG_POINTER(enterpriseRoots);

#ifdef XP_WIN
  nsCOMPtr<nsINSSComponent> psm(do_GetService(PSM_COMPONENT_CONTRACTID));
  if (!psm) {
    return NS_ERROR_FAILURE;
  }
  return psm->GetEnterpriseRoots(enterpriseRoots);
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

nsresult
VerifyCertAtTime(nsIX509Cert* aCert,
                 int64_t /*SECCertificateUsage*/ aUsage,
                 uint32_t aFlags,
                 const nsACString& aHostname,
                 mozilla::pkix::Time aTime,
                 nsIX509CertList** aVerifiedChain,
                 bool* aHasEVPolicy,
                 int32_t* /*PRErrorCode*/ _retval)
{
  NS_ENSURE_ARG_POINTER(aCert);
  NS_ENSURE_ARG_POINTER(aHasEVPolicy);
  NS_ENSURE_ARG_POINTER(aVerifiedChain);
  NS_ENSURE_ARG_POINTER(_retval);

  *aVerifiedChain = nullptr;
  *aHasEVPolicy = false;
  *_retval = PR_UNKNOWN_ERROR;

  UniqueCERTCertificate nssCert(aCert->GetCert());
  if (!nssCert) {
    return NS_ERROR_INVALID_ARG;
  }

  RefPtr<SharedCertVerifier> certVerifier(GetDefaultCertVerifier());
  NS_ENSURE_TRUE(certVerifier, NS_ERROR_FAILURE);

  UniqueCERTCertList resultChain;
  SECOidTag evOidPolicy;
  mozilla::pkix::Result result;

  if (!aHostname.IsVoid() && aUsage == certificateUsageSSLServer) {
    result = certVerifier->VerifySSLServerCert(nssCert,
                                               nullptr, // stapledOCSPResponse
                                               nullptr, // sctsFromTLSExtension
                                               aTime,
                                               nullptr, // Assume no context
                                               aHostname,
                                               resultChain,
                                               false, // don't save intermediates
                                               aFlags,
                                               OriginAttributes(),
                                               &evOidPolicy);
  } else {
    const nsCString& flatHostname = PromiseFlatCString(aHostname);
    result = certVerifier->VerifyCert(nssCert.get(), aUsage, aTime,
                                      nullptr, // Assume no context
                                      aHostname.IsVoid() ? nullptr
                                                         : flatHostname.get(),
                                      resultChain,
                                      aFlags,
                                      nullptr, // stapledOCSPResponse
                                      nullptr, // sctsFromTLSExtension
                                      OriginAttributes(),
                                      &evOidPolicy);
  }

  nsCOMPtr<nsIX509CertList> nssCertList;
  // This adopts the list
  nssCertList = new nsNSSCertList(Move(resultChain));
  NS_ENSURE_TRUE(nssCertList, NS_ERROR_FAILURE);

  *_retval = mozilla::pkix::MapResultToPRErrorCode(result);
  if (result == mozilla::pkix::Success && evOidPolicy != SEC_OID_UNKNOWN) {
    *aHasEVPolicy = true;
  }
  nssCertList.forget(aVerifiedChain);

  return NS_OK;
}

class VerifyCertAtTimeTask final : public CryptoTask
{
public:
  VerifyCertAtTimeTask(nsIX509Cert* aCert, int64_t aUsage, uint32_t aFlags,
                       const nsACString& aHostname, uint64_t aTime,
                       nsICertVerificationCallback* aCallback)
    : mCert(aCert)
    , mUsage(aUsage)
    , mFlags(aFlags)
    , mHostname(aHostname)
    , mTime(aTime)
    , mCallback(new nsMainThreadPtrHolder<nsICertVerificationCallback>(
        "nsICertVerificationCallback", aCallback))
    , mPRErrorCode(SEC_ERROR_LIBRARY_FAILURE)
    , mVerifiedCertList(nullptr)
    , mHasEVPolicy(false)
  {
  }

private:
  virtual nsresult CalculateResult() override
  {
    nsCOMPtr<nsIX509CertDB> certDB = do_GetService(NS_X509CERTDB_CONTRACTID);
    if (!certDB) {
      return NS_ERROR_FAILURE;
    }
    return VerifyCertAtTime(mCert, mUsage, mFlags, mHostname,
                            mozilla::pkix::TimeFromEpochInSeconds(mTime),
                            getter_AddRefs(mVerifiedCertList),
                            &mHasEVPolicy, &mPRErrorCode);
  }

  virtual void CallCallback(nsresult rv) override
  {
    if (NS_FAILED(rv)) {
      Unused << mCallback->VerifyCertFinished(SEC_ERROR_LIBRARY_FAILURE,
                                              nullptr, false);
    } else {
      Unused << mCallback->VerifyCertFinished(mPRErrorCode, mVerifiedCertList,
                                              mHasEVPolicy);
    }
  }

  nsCOMPtr<nsIX509Cert> mCert;
  int64_t mUsage;
  uint32_t mFlags;
  nsCString mHostname;
  uint64_t mTime;
  nsMainThreadPtrHandle<nsICertVerificationCallback> mCallback;
  int32_t mPRErrorCode;
  nsCOMPtr<nsIX509CertList> mVerifiedCertList;
  bool mHasEVPolicy;
};

NS_IMETHODIMP
nsNSSCertificateDB::AsyncVerifyCertAtTime(nsIX509Cert* aCert,
                                          int64_t /*SECCertificateUsage*/ aUsage,
                                          uint32_t aFlags,
                                          const nsACString& aHostname,
                                          uint64_t aTime,
                                          nsICertVerificationCallback* aCallback)
{
  RefPtr<VerifyCertAtTimeTask> task(new VerifyCertAtTimeTask(aCert, aUsage,
                                                             aFlags, aHostname,
                                                             aTime, aCallback));
  return task->Dispatch("VerifyCert");
}

NS_IMETHODIMP
nsNSSCertificateDB::ClearOCSPCache()
{
  RefPtr<SharedCertVerifier> certVerifier(GetDefaultCertVerifier());
  NS_ENSURE_TRUE(certVerifier, NS_ERROR_FAILURE);
  certVerifier->ClearOCSPCache();
  return NS_OK;
}
