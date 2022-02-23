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
#include "mozilla/Logging.h"
#include "mozilla/Services.h"
#include "mozilla/Unused.h"
#include "mozpkix/Time.h"
#include "mozpkix/pkixnss.h"
#include "mozpkix/pkixtypes.h"
#include "nsArray.h"
#include "nsArrayUtils.h"
#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsICertificateDialogs.h"
#include "nsIFile.h"
#include "nsIMutableArray.h"
#include "nsIObserverService.h"
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
#include "secasn1.h"
#include "secder.h"
#include "secerr.h"
#include "ssl.h"

#ifdef XP_WIN
#  include <winsock.h>  // for ntohl
#endif

using namespace mozilla;
using namespace mozilla::psm;

extern LazyLogModule gPIPNSSLog;

NS_IMPL_ISUPPORTS(nsNSSCertificateDB, nsIX509CertDB)

NS_IMETHODIMP
nsNSSCertificateDB::FindCertByDBKey(const nsACString& aDBKey,
                                    /*out*/ nsIX509Cert** _cert) {
  NS_ENSURE_ARG_POINTER(_cert);
  *_cert = nullptr;

  if (aDBKey.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  nsresult rv = BlockUntilLoadableCertsLoaded();
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
  nsCOMPtr<nsIX509Cert> nssCert = new nsNSSCertificate(cert.get());
  nssCert.forget(_cert);
  return NS_OK;
}

nsresult nsNSSCertificateDB::FindCertByDBKey(const nsACString& aDBKey,
                                             UniqueCERTCertificate& cert) {
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
  uint32_t serialNumberLen =
      ntohl((*BitwiseCast<const uint32_t*, const char*>(reader)));
  reader += sizeof(uint32_t);
  uint32_t issuerLen =
      ntohl((*BitwiseCast<const uint32_t*, const char*>(reader)));
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

SECStatus collect_certs(void* arg, SECItem** certs, int numcerts) {
  nsTArray<nsTArray<uint8_t>>* certsArray =
      reinterpret_cast<nsTArray<nsTArray<uint8_t>>*>(arg);

  while (numcerts--) {
    nsTArray<uint8_t> certArray;
    SECItem* cert = *certs;
    certArray.AppendElements(cert->data, cert->len);
    certsArray->AppendElement(std::move(certArray));
    certs++;
  }
  return (SECSuccess);
}

nsresult nsNSSCertificateDB::getCertsFromPackage(
    nsTArray<nsTArray<uint8_t>>& collectArgs, uint8_t* data, uint32_t length) {
  if (CERT_DecodeCertPackage(BitwiseCast<char*, uint8_t*>(data), length,
                             collect_certs, &collectArgs) != SECSuccess) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

// When using the sql-backed softoken, trust settings are authenticated using a
// key in the secret database. Thus, if the user has a password, we need to
// authenticate to the token in order to be able to change trust settings.
SECStatus ChangeCertTrustWithPossibleAuthentication(
    const UniqueCERTCertificate& cert, CERTCertTrust& trust, void* ctx) {
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

static nsresult ImportCertsIntoPermanentStorage(
    const UniqueCERTCertList& certChain) {
  bool encounteredFailure = false;
  PRErrorCode savedErrorCode = 0;
  UniquePK11SlotInfo slot(PK11_GetInternalKeySlot());
  for (CERTCertListNode* chainNode = CERT_LIST_HEAD(certChain);
       !CERT_LIST_END(chainNode, certChain);
       chainNode = CERT_LIST_NEXT(chainNode)) {
    UniquePORTString nickname(CERT_MakeCANickname(chainNode->cert));
    SECStatus srv = PK11_ImportCert(slot.get(), chainNode->cert,
                                    CK_INVALID_HANDLE, nickname.get(),
                                    false);  // this parameter is ignored by NSS
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

nsresult nsNSSCertificateDB::handleCACertDownload(NotNull<nsIArray*> x509Certs,
                                                  nsIInterfaceRequestor* ctx) {
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

  if (numCerts == 0) return NS_OK;  // Nothing to import, so nothing to do.

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
    certn_2 = do_QueryElementAt(x509Certs, numCerts - 2);
    certn_1 = do_QueryElementAt(x509Certs, numCerts - 1);

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
    } else if (certn_2IssuerName.Equals(certn_1SubjectName)) {
      // In this case the last cert has signed the second to last cert.
      // The last cert is the root, so let's display it.
      selCertIndex = numCerts - 1;
      certToShow = certn_1;
    } else {
      // It's not a chain, so let's just show the first one in the
      // downloaded list.
      selCertIndex = 0;
      certToShow = cert0;
    }
  }

  if (!certToShow) return NS_ERROR_FAILURE;

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
  if (NS_FAILED(rv)) return rv;

  if (!allows) return NS_ERROR_NOT_AVAILABLE;

  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("trust is %d\n", trustBits));
  UniquePORTString nickname(CERT_MakeCANickname(tmpCert.get()));

  MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
          ("Created nick \"%s\"\n", nickname.get()));

  nsNSSCertTrust trust;
  trust.SetValidCA();
  trust.AddCATrust(!!(trustBits & nsIX509CertDB::TRUSTED_SSL),
                   !!(trustBits & nsIX509CertDB::TRUSTED_EMAIL));

  UniquePK11SlotInfo slot(PK11_GetInternalKeySlot());
  SECStatus srv = PK11_ImportCert(slot.get(), tmpCert.get(), CK_INVALID_HANDLE,
                                  nickname.get(),
                                  false);  // this parameter is ignored by NSS
  if (srv != SECSuccess) {
    return MapSECStatus(srv);
  }
  srv =
      ChangeCertTrustWithPossibleAuthentication(tmpCert, trust.GetTrust(), ctx);
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

  for (uint32_t i = 0; i < numCerts; i++) {
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

nsresult nsNSSCertificateDB::ConstructCertArrayFromUniqueCertList(
    const UniqueCERTCertList& aCertListIn,
    nsTArray<RefPtr<nsIX509Cert>>& aCertListOut) {
  if (!aCertListIn.get()) {
    return NS_ERROR_INVALID_ARG;
  }

  for (CERTCertListNode* node = CERT_LIST_HEAD(aCertListIn.get());
       !CERT_LIST_END(node, aCertListIn.get()); node = CERT_LIST_NEXT(node)) {
    RefPtr<nsIX509Cert> cert = new nsNSSCertificate(node->cert);
    aCertListOut.AppendElement(cert);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificateDB::ImportCertificates(uint8_t* data, uint32_t length,
                                       uint32_t type,
                                       nsIInterfaceRequestor* ctx) {
  // We currently only handle CA certificates.
  if (type != nsIX509Cert::CA_CERT) {
    return NS_ERROR_FAILURE;
  }

  nsTArray<nsTArray<uint8_t>> certsArray;
  nsresult rv = getCertsFromPackage(certsArray, data, length);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIMutableArray> array = nsArrayBase::Create();
  if (!array) {
    return NS_ERROR_FAILURE;
  }

  // Now let's create some certs to work with
  for (nsTArray<uint8_t>& certDER : certsArray) {
    nsCOMPtr<nsIX509Cert> cert = new nsNSSCertificate(std::move(certDER));
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
 * @param certs
 *        Array in which the decoded certificates are stored as arrays of
 *        unsigned chars.
 * @param temporaryCerts
 *        List of decoded certificates.
 */
static nsresult ImportCertsIntoTempStorage(
    nsTArray<nsTArray<uint8_t>>& certs,
    /*out*/ const UniqueCERTCertList& temporaryCerts) {
  NS_ENSURE_ARG_POINTER(temporaryCerts);

  for (nsTArray<uint8_t>& certDER : certs) {
    CERTCertificate* certificate;
    SECItem certItem;
    certItem.len = certDER.Length();
    certItem.data = certDER.Elements();
    certificate = CERT_NewTempCertificate(CERT_GetDefaultCertDB(), &certItem,
                                          nullptr, false, true);

    UniqueCERTCertificate cert(certificate);
    if (!cert) {
      continue;
    }

    if (CERT_AddCertToListTail(temporaryCerts.get(), cert.get()) ==
        SECSuccess) {
      Unused << cert.release();
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificateDB::ImportEmailCertificate(uint8_t* data, uint32_t length,
                                           nsIInterfaceRequestor* ctx) {
  nsTArray<nsTArray<uint8_t>> certsArray;

  nsresult rv = getCertsFromPackage(certsArray, data, length);
  if (NS_FAILED(rv)) {
    return rv;
  }

  UniqueCERTCertList temporaryCerts(CERT_NewCertList());
  if (!temporaryCerts) {
    return NS_ERROR_FAILURE;
  }

  rv = ImportCertsIntoTempStorage(certsArray, temporaryCerts);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return ImportCertsIntoPermanentStorage(temporaryCerts);
}

nsresult nsNSSCertificateDB::ImportCACerts(nsTArray<nsTArray<uint8_t>>& caCerts,
                                           nsIInterfaceRequestor* ctx) {
  UniqueCERTCertList temporaryCerts(CERT_NewCertList());
  if (!temporaryCerts) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = ImportCertsIntoTempStorage(caCerts, temporaryCerts);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return ImportCertsIntoPermanentStorage(temporaryCerts);
}

void nsNSSCertificateDB::DisplayCertificateAlert(nsIInterfaceRequestor* ctx,
                                                 const char* stringID,
                                                 nsIX509Cert* certToShow) {
  if (!NS_IsMainThread()) {
    NS_ERROR(
        "nsNSSCertificateDB::DisplayCertificateAlert called off the main "
        "thread");
    return;
  }

  nsCOMPtr<nsIInterfaceRequestor> my_ctx = ctx;
  if (!my_ctx) {
    my_ctx = new PipUIContext();
  }

  // This shall be replaced by embedding ovverridable prompts
  // as discussed in bug 310446, and should make use of certToShow.

  nsAutoString tmpMessage;
  GetPIPNSSBundleString(stringID, tmpMessage);
  nsCOMPtr<nsIPrompt> prompt(do_GetInterface(my_ctx));
  if (!prompt) {
    return;
  }

  prompt->Alert(nullptr, tmpMessage.get());
}

NS_IMETHODIMP
nsNSSCertificateDB::ImportUserCertificate(uint8_t* data, uint32_t length,
                                          nsIInterfaceRequestor* ctx) {
  if (!NS_IsMainThread()) {
    NS_ERROR(
        "nsNSSCertificateDB::ImportUserCertificate called off the main thread");
    return NS_ERROR_NOT_SAME_THREAD;
  }

  nsTArray<nsTArray<uint8_t>> certsArray;

  nsresult rv = getCertsFromPackage(certsArray, data, length);
  if (NS_FAILED(rv)) {
    return rv;
  }

  SECItem certItem;

  if (certsArray.IsEmpty()) {
    return NS_OK;
  }

  certItem.len = certsArray.ElementAt(0).Length();
  certItem.data = certsArray.ElementAt(0).Elements();

  UniqueCERTCertificate cert(CERT_NewTempCertificate(
      CERT_GetDefaultCertDB(), &certItem, nullptr, false, true));
  if (!cert) {
    return NS_ERROR_FAILURE;
  }

  UniquePK11SlotInfo slot(PK11_KeyForCertExists(cert.get(), nullptr, ctx));
  if (!slot) {
    nsCOMPtr<nsIX509Cert> certToShow = new nsNSSCertificate(cert.get());
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
    nsCOMPtr<nsIX509Cert> certToShow = new nsNSSCertificate(cert.get());
    DisplayCertificateAlert(ctx, "UserCertImported", certToShow);
  }

  rv = NS_OK;
  if (!certsArray.IsEmpty()) {
    certsArray.RemoveElementAt(0);
    rv = ImportCACerts(certsArray, ctx);
  }

  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (observerService) {
    observerService->NotifyObservers(nullptr, "psm:user-certificate-added",
                                     nullptr);
  }

  return rv;
}

NS_IMETHODIMP
nsNSSCertificateDB::DeleteCertificate(nsIX509Cert* aCert) {
  NS_ENSURE_ARG_POINTER(aCert);
  UniqueCERTCertificate cert(aCert->GetCert());
  if (!cert) {
    return NS_ERROR_FAILURE;
  }

  // Temporary certificates aren't on a slot and will go away when the
  // nsIX509Cert is destructed.
  if (cert->slot) {
    uint32_t certType;
    nsresult rv = aCert->GetCertType(&certType);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    if (certType == nsIX509Cert::USER_CERT) {
      SECStatus srv = PK11_Authenticate(cert->slot, true, nullptr);
      if (srv != SECSuccess) {
        return NS_ERROR_FAILURE;
      }
      srv = PK11_DeleteTokenCertAndKey(cert.get(), nullptr);
      if (srv != SECSuccess) {
        return NS_ERROR_FAILURE;
      }
    } else {
      // For certificates that can't be deleted (e.g. built-in roots), un-set
      // all trust bits.
      nsNSSCertTrust trust(0, 0);
      SECStatus srv = ChangeCertTrustWithPossibleAuthentication(
          cert, trust.GetTrust(), nullptr);
      if (srv != SECSuccess) {
        return NS_ERROR_FAILURE;
      }
      if (!PK11_IsReadOnly(cert->slot)) {
        srv = SEC_DeletePermCertificate(cert.get());
        if (srv != SECSuccess) {
          return NS_ERROR_FAILURE;
        }
      }
    }
  }

  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (observerService) {
    observerService->NotifyObservers(nullptr, "psm:user-certificate-deleted",
                                     nullptr);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificateDB::SetCertTrust(nsIX509Cert* cert, uint32_t type,
                                 uint32_t trusted) {
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
  SECStatus srv = ChangeCertTrustWithPossibleAuthentication(
      nsscert, trust.GetTrust(), nullptr);
  return MapSECStatus(srv);
}

NS_IMETHODIMP
nsNSSCertificateDB::IsCertTrusted(nsIX509Cert* cert, uint32_t certType,
                                  uint32_t trustType, bool* _isTrusted) {
  NS_ENSURE_ARG_POINTER(_isTrusted);
  *_isTrusted = false;

  nsresult rv = BlockUntilLoadableCertsLoaded();
  if (NS_FAILED(rv)) {
    return rv;
  }

  SECStatus srv;
  UniqueCERTCertificate nsscert(cert->GetCert());
  CERTCertTrust nsstrust;
  srv = CERT_GetCertTrust(nsscert.get(), &nsstrust);
  if (srv != SECSuccess) {
    // CERT_GetCertTrust returns SECFailure if given a temporary cert that
    // doesn't have any trust information yet. This isn't an error.
    return NS_OK;
  }

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
nsNSSCertificateDB::ImportCertsFromFile(nsIFile* aFile, uint32_t aType) {
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
nsNSSCertificateDB::ImportPKCS12File(nsIFile* aFile, const nsAString& aPassword,
                                     uint32_t* aError) {
  if (!NS_IsMainThread()) {
    return NS_ERROR_NOT_SAME_THREAD;
  }
  nsresult rv = BlockUntilLoadableCertsLoaded();
  if (NS_FAILED(rv)) {
    return rv;
  }

  NS_ENSURE_ARG(aFile);
  nsPKCS12Blob blob;
  rv = blob.ImportFromFile(aFile, aPassword, *aError);
  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (NS_SUCCEEDED(rv) && observerService) {
    observerService->NotifyObservers(nullptr, "psm:user-certificate-added",
                                     nullptr);
  }

  return rv;
}

NS_IMETHODIMP
nsNSSCertificateDB::ExportPKCS12File(
    nsIFile* aFile, const nsTArray<RefPtr<nsIX509Cert>>& aCerts,
    const nsAString& aPassword, uint32_t* aError) {
  if (!NS_IsMainThread()) {
    return NS_ERROR_NOT_SAME_THREAD;
  }
  nsresult rv = BlockUntilLoadableCertsLoaded();
  if (NS_FAILED(rv)) {
    return rv;
  }

  NS_ENSURE_ARG(aFile);
  if (aCerts.IsEmpty()) {
    return NS_OK;
  }
  nsPKCS12Blob blob;
  return blob.ExportToFile(aFile, aCerts, aPassword, *aError);
}

NS_IMETHODIMP
nsNSSCertificateDB::ConstructX509FromBase64(const nsACString& base64,
                                            /*out*/ nsIX509Cert** _retval) {
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

  return ConstructX509FromSpan(AsBytes(Span(certDER)), _retval);
}

NS_IMETHODIMP
nsNSSCertificateDB::ConstructX509(const nsTArray<uint8_t>& certDER,
                                  nsIX509Cert** _retval) {
  return ConstructX509FromSpan(Span(certDER.Elements(), certDER.Length()),
                               _retval);
}

nsresult nsNSSCertificateDB::ConstructX509FromSpan(
    Span<const uint8_t> aInputSpan, nsIX509Cert** _retval) {
  if (NS_WARN_IF(!_retval)) {
    return NS_ERROR_INVALID_POINTER;
  }

  if (aInputSpan.Length() > std::numeric_limits<unsigned int>::max()) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  SECItem certData;
  certData.type = siDERCertBuffer;
  certData.data = const_cast<unsigned char*>(
      reinterpret_cast<const unsigned char*>(aInputSpan.Elements()));
  certData.len = aInputSpan.Length();

  UniqueCERTCertificate cert(CERT_NewTempCertificate(
      CERT_GetDefaultCertDB(), &certData, nullptr, false, true));
  if (!cert)
    return (PORT_GetError() == SEC_ERROR_NO_MEMORY) ? NS_ERROR_OUT_OF_MEMORY
                                                    : NS_ERROR_FAILURE;

  nsCOMPtr<nsIX509Cert> nssCert = new nsNSSCertificate(cert.get());
  nssCert.forget(_retval);
  return NS_OK;
}

void nsNSSCertificateDB::get_default_nickname(CERTCertificate* cert,
                                              nsIInterfaceRequestor* ctx,
                                              nsCString& nickname) {
  nickname.Truncate();

  CK_OBJECT_HANDLE keyHandle;

  if (NS_FAILED(BlockUntilLoadableCertsLoaded())) {
    return;
  }

  CERTCertDBHandle* defaultcertdb = CERT_GetDefaultCertDB();
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
  GetPIPNSSBundleString("nick_template", tmpNickFmt);
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
  if (!slot) return;

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
    if (count > 1) {
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
        if (CERT_CompareName(&cert->subject, &dummycert->subject) == SECEqual) {
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
                                      nsIX509Cert** addedCertificate) {
  // Base64Decode() doesn't consider a zero length input as an error, and just
  // returns the empty string. We don't want this behavior, so the below check
  // catches this case.
  if (aBase64.Length() < 1) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  nsAutoCString aCertDER;
  nsresult rv = Base64Decode(aBase64, aCertDER);
  if (NS_FAILED(rv)) {
    return rv;
  }
  return AddCert(aCertDER, aTrust, addedCertificate);
}

NS_IMETHODIMP
nsNSSCertificateDB::AddCert(const nsACString& aCertDER,
                            const nsACString& aTrust,
                            nsIX509Cert** addedCertificate) {
  MOZ_ASSERT(addedCertificate);
  if (!addedCertificate) {
    return NS_ERROR_INVALID_ARG;
  }
  *addedCertificate = nullptr;

  nsNSSCertTrust trust;
  if (CERT_DecodeTrustString(&trust.GetTrust(),
                             PromiseFlatCString(aTrust).get()) != SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIX509Cert> newCert;
  nsresult rv =
      ConstructX509FromSpan(AsBytes(Span(aCertDER)), getter_AddRefs(newCert));
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

  MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
          ("Created nick \"%s\"\n", nickname.get()));

  UniquePK11SlotInfo slot(PK11_GetInternalKeySlot());
  SECStatus srv = PK11_ImportCert(slot.get(), tmpCert.get(), CK_INVALID_HANDLE,
                                  nickname.get(),
                                  false);  // this parameter is ignored by NSS
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
nsNSSCertificateDB::SetCertTrustFromString(nsIX509Cert* cert,
                                           const nsACString& trustString) {
  NS_ENSURE_ARG(cert);

  CERTCertTrust trust;
  SECStatus srv =
      CERT_DecodeTrustString(&trust, PromiseFlatCString(trustString).get());
  if (srv != SECSuccess) {
    return MapSECStatus(srv);
  }
  UniqueCERTCertificate nssCert(cert->GetCert());

  srv = ChangeCertTrustWithPossibleAuthentication(nssCert, trust, nullptr);
  return MapSECStatus(srv);
}

NS_IMETHODIMP nsNSSCertificateDB::AsPKCS7Blob(
    const nsTArray<RefPtr<nsIX509Cert>>& certList, nsACString& _retval) {
  if (certList.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  UniqueNSSCMSMessage cmsg(NSS_CMSMessage_Create(nullptr));
  if (!cmsg) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("nsNSSCertificateDB::AsPKCS7Blob - can't create CMS message"));
    return NS_ERROR_OUT_OF_MEMORY;
  }

  UniqueNSSCMSSignedData sigd(nullptr);
  for (const auto& cert : certList) {
    // We need an owning handle when calling nsIX509Cert::GetCert().
    UniqueCERTCertificate nssCert(cert->GetCert());
    if (!sigd) {
      sigd.reset(
          NSS_CMSSignedData_CreateCertsOnly(cmsg.get(), nssCert.get(), false));
      if (!sigd) {
        MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
                ("nsNSSCertificateDB::AsPKCS7Blob - can't create SignedData"));
        return NS_ERROR_FAILURE;
      }
    } else if (NSS_CMSSignedData_AddCertificate(sigd.get(), nssCert.get()) !=
               SECSuccess) {
      MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
              ("nsNSSCertificateDB::AsPKCS7Blob - can't add cert"));
      return NS_ERROR_FAILURE;
    }
  }

  NSSCMSContentInfo* cinfo = NSS_CMSMessage_GetContentInfo(cmsg.get());
  if (NSS_CMSContentInfo_SetContent_SignedData(cmsg.get(), cinfo, sigd.get()) !=
      SECSuccess) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("nsNSSCertificateDB::AsPKCS7Blob - can't attach SignedData"));
    return NS_ERROR_FAILURE;
  }
  // cmsg owns sigd now.
  Unused << sigd.release();

  UniquePLArenaPool arena(PORT_NewArena(1024));
  if (!arena) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("nsNSSCertificateDB::AsPKCS7Blob - out of memory"));
    return NS_ERROR_OUT_OF_MEMORY;
  }

  SECItem certP7 = {siBuffer, nullptr, 0};
  NSSCMSEncoderContext* ecx = NSS_CMSEncoder_Start(
      cmsg.get(), nullptr, nullptr, &certP7, arena.get(), nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr);
  if (!ecx) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("nsNSSCertificateDB::AsPKCS7Blob - can't create encoder"));
    return NS_ERROR_FAILURE;
  }

  if (NSS_CMSEncoder_Finish(ecx) != SECSuccess) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("nsNSSCertificateDB::AsPKCS7Blob - failed to add encoded data"));
    return NS_ERROR_FAILURE;
  }

  _retval.Assign(nsDependentCSubstring(
      reinterpret_cast<const char*>(certP7.data), certP7.len));
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificateDB::GetCerts(nsTArray<RefPtr<nsIX509Cert>>& _retval) {
  nsresult rv = BlockUntilLoadableCertsLoaded();
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = CheckForSmartCardChanges();
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIInterfaceRequestor> ctx = new PipUIContext();
  UniqueCERTCertList certList(PK11_ListCerts(PK11CertListUnique, ctx));
  if (!certList) {
    return NS_ERROR_FAILURE;
  }
  return nsNSSCertificateDB::ConstructCertArrayFromUniqueCertList(certList,
                                                                  _retval);
}

NS_IMETHODIMP
nsNSSCertificateDB::AsyncHasThirdPartyRoots(nsIAsyncBoolCallback* aCallback) {
  NS_ENSURE_ARG_POINTER(aCallback);
  nsMainThreadPtrHandle<nsIAsyncBoolCallback> callback(
      new nsMainThreadPtrHolder<nsIAsyncBoolCallback>("AsyncHasThirdPartyRoots",
                                                      aCallback));

  return NS_DispatchBackgroundTask(
      NS_NewRunnableFunction(
          "nsNSSCertificateDB::AsyncHasThirdPartyRoots",
          [cb = std::move(callback), self = RefPtr{this}] {
            bool hasThirdPartyRoots = [self]() -> bool {
              nsTArray<RefPtr<nsIX509Cert>> certs;
              nsresult rv = self->GetCerts(certs);
              if (NS_FAILED(rv)) {
                return false;
              }

              for (const auto& cert : certs) {
                bool isTrusted = false;
                nsresult rv =
                    self->IsCertTrusted(cert, nsIX509Cert::CA_CERT,
                                        nsIX509CertDB::TRUSTED_SSL, &isTrusted);
                if (NS_FAILED(rv)) {
                  return false;
                }

                if (!isTrusted) {
                  continue;
                }

                bool isBuiltInRoot = false;
                rv = cert->GetIsBuiltInRoot(&isBuiltInRoot);
                if (NS_FAILED(rv)) {
                  return false;
                }

                if (!isBuiltInRoot) {
                  return true;
                }
              }

              return false;
            }();

            NS_DispatchToMainThread(NS_NewRunnableFunction(
                "nsNSSCertificateDB::AsyncHasThirdPartyRoots callback",
                [cb, hasThirdPartyRoots]() {
                  cb->OnResult(hasThirdPartyRoots);
                }));
          }),
      NS_DISPATCH_EVENT_MAY_BLOCK);

  return NS_OK;
}

nsresult VerifyCertAtTime(nsIX509Cert* aCert,
                          int64_t /*SECCertificateUsage*/ aUsage,
                          uint32_t aFlags, const nsACString& aHostname,
                          mozilla::pkix::Time aTime,
                          nsTArray<RefPtr<nsIX509Cert>>& aVerifiedChain,
                          bool* aHasEVPolicy,
                          int32_t* /*PRErrorCode*/ _retval) {
  NS_ENSURE_ARG_POINTER(aCert);
  NS_ENSURE_ARG_POINTER(aHasEVPolicy);
  NS_ENSURE_ARG_POINTER(_retval);

  if (!aVerifiedChain.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  *aHasEVPolicy = false;
  *_retval = PR_UNKNOWN_ERROR;

  RefPtr<SharedCertVerifier> certVerifier(GetDefaultCertVerifier());
  NS_ENSURE_TRUE(certVerifier, NS_ERROR_FAILURE);

  nsTArray<nsTArray<uint8_t>> resultChain;
  EVStatus evStatus;
  mozilla::pkix::Result result;

  nsTArray<uint8_t> certBytes;
  nsresult nsrv = aCert->GetRawDER(certBytes);
  if (NS_FAILED(nsrv)) {
    return nsrv;
  }

  if (!aHostname.IsVoid() && aUsage == certificateUsageSSLServer) {
    result =
        certVerifier->VerifySSLServerCert(certBytes, aTime,
                                          nullptr,  // Assume no context
                                          aHostname, resultChain, aFlags,
                                          Nothing(),  // extraCertificates
                                          Nothing(),  // stapledOCSPResponse
                                          Nothing(),  // sctsFromTLSExtension
                                          Nothing(),  // dcInfo
                                          OriginAttributes(), &evStatus);
  } else {
    const nsCString& flatHostname = PromiseFlatCString(aHostname);
    result = certVerifier->VerifyCert(
        certBytes, aUsage, aTime,
        nullptr,  // Assume no context
        aHostname.IsVoid() ? nullptr : flatHostname.get(), resultChain, aFlags,
        Nothing(),  // extraCertificates
        Nothing(),  // stapledOCSPResponse
        Nothing(),  // sctsFromTLSExtension
        OriginAttributes(), &evStatus);
  }

  if (result == mozilla::pkix::Success) {
    for (auto& certDER : resultChain) {
      RefPtr<nsIX509Cert> cert = new nsNSSCertificate(std::move(certDER));
      aVerifiedChain.AppendElement(cert);
    }

    if (evStatus == EVStatus::EV) {
      *aHasEVPolicy = true;
    }
  }

  *_retval = mozilla::pkix::MapResultToPRErrorCode(result);

  return NS_OK;
}

class VerifyCertAtTimeTask final : public CryptoTask {
 public:
  VerifyCertAtTimeTask(nsIX509Cert* aCert, int64_t aUsage, uint32_t aFlags,
                       const nsACString& aHostname, uint64_t aTime,
                       nsICertVerificationCallback* aCallback)
      : mCert(aCert),
        mUsage(aUsage),
        mFlags(aFlags),
        mHostname(aHostname),
        mTime(aTime),
        mCallback(new nsMainThreadPtrHolder<nsICertVerificationCallback>(
            "nsICertVerificationCallback", aCallback)),
        mPRErrorCode(SEC_ERROR_LIBRARY_FAILURE),
        mHasEVPolicy(false) {}

 private:
  virtual nsresult CalculateResult() override {
    nsCOMPtr<nsIX509CertDB> certDB = do_GetService(NS_X509CERTDB_CONTRACTID);
    if (!certDB) {
      return NS_ERROR_FAILURE;
    }
    return VerifyCertAtTime(mCert, mUsage, mFlags, mHostname,
                            mozilla::pkix::TimeFromEpochInSeconds(mTime),
                            mVerifiedCertList, &mHasEVPolicy, &mPRErrorCode);
  }

  virtual void CallCallback(nsresult rv) override {
    if (NS_FAILED(rv)) {
      nsTArray<RefPtr<nsIX509Cert>> tmp;
      Unused << mCallback->VerifyCertFinished(SEC_ERROR_LIBRARY_FAILURE, tmp,
                                              false);
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
  nsTArray<RefPtr<nsIX509Cert>> mVerifiedCertList;
  bool mHasEVPolicy;
};

NS_IMETHODIMP
nsNSSCertificateDB::AsyncVerifyCertAtTime(
    nsIX509Cert* aCert, int64_t /*SECCertificateUsage*/ aUsage, uint32_t aFlags,
    const nsACString& aHostname, uint64_t aTime,
    nsICertVerificationCallback* aCallback) {
  RefPtr<VerifyCertAtTimeTask> task(new VerifyCertAtTimeTask(
      aCert, aUsage, aFlags, aHostname, aTime, aCallback));
  return task->Dispatch();
}

NS_IMETHODIMP
nsNSSCertificateDB::ClearOCSPCache() {
  RefPtr<SharedCertVerifier> certVerifier(GetDefaultCertVerifier());
  NS_ENSURE_TRUE(certVerifier, NS_ERROR_FAILURE);
  certVerifier->ClearOCSPCache();
  return NS_OK;
}
