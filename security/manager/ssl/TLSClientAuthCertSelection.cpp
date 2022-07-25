/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Implements the client authentication certificate selection callback for NSS.
// nsNSSIOLayer.cpp sets the callback by calling SSL_GetClientAuthDataHook and
// identifying SSLGetClientAuthDataHook as the function to call when a TLS
// server requests a client authentication certificate.
//
// In the general case, SSLGetClientAuthDataHook (running on the socket thread),
// dispatches an event to the main thread to ask the user to select a client
// authentication certificate. Meanwhile, it returns SECWouldBlock so that other
// network I/O can occur. When the user selects a client certificate (or opts
// not to send one), an event is dispatched to the socket thread that gives NSS
// the appropriate information to proceed with the TLS connection.
//
// If networking is being done on the socket process, SSLGetClientAuthDataHook
// sends an IPC call to the parent process to ask the user to select a
// certificate. Meanwhile, it again returns SECWouldBlock so other network I/O
// can occur. When a certificate (or no certificate) has been selected, the
// parent process sends an IPC call back to the socket process, which causes an
// event to be dispatched to the socket thread to continue to the TLS
// connection.

#include "TLSClientAuthCertSelection.h"
#include "cert_storage/src/cert_storage.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/psm/SelectTLSClientAuthCertChild.h"
#include "mozilla/psm/SelectTLSClientAuthCertParent.h"
#include "nsArray.h"
#include "nsArrayUtils.h"
#include "nsIClientAuthDialogs.h"
#include "nsIMutableArray.h"
#include "nsIX509CertDB.h"

// Possible behaviors for choosing a cert for client auth.
enum class UserCertChoice {
  // Ask the user to choose a cert.
  Ask = 0,
  // Automatically choose a cert.
  Auto = 1,
};

// Returns the most appropriate user cert choice based on the value of the
// security.default_personal_cert preference.
UserCertChoice nsGetUserCertChoice() {
  nsAutoCString value;
  nsresult rv =
      Preferences::GetCString("security.default_personal_cert", value);
  if (NS_FAILED(rv)) {
    return UserCertChoice::Ask;
  }

  // There are three cases for what the preference could be set to:
  //   1. "Select Automatically" -> Auto.
  //   2. "Ask Every Time" -> Ask.
  //   3. Something else -> Ask. This might be a nickname from a migrated cert,
  //      but we no longer support this case.
  return value.EqualsLiteral("Select Automatically") ? UserCertChoice::Auto
                                                     : UserCertChoice::Ask;
}

static bool hasExplicitKeyUsageNonRepudiation(CERTCertificate* cert) {
  // There is no extension, v1 or v2 certificate
  if (!cert->extensions) return false;

  SECStatus srv;
  SECItem keyUsageItem;
  keyUsageItem.data = nullptr;

  srv = CERT_FindKeyUsageExtension(cert, &keyUsageItem);
  if (srv == SECFailure) return false;

  unsigned char keyUsage = keyUsageItem.data[0];
  PORT_Free(keyUsageItem.data);

  return !!(keyUsage & KU_NON_REPUDIATION);
}

// This class is used to store the needed information for invoking the client
// cert selection UI.
class ClientAuthInfo final {
 public:
  explicit ClientAuthInfo(const nsACString& hostName,
                          const mozilla::OriginAttributes& originAttributes,
                          int32_t port, uint32_t providerFlags,
                          uint32_t providerTlsFlags);
  ~ClientAuthInfo() = default;
  ClientAuthInfo(ClientAuthInfo&& aOther) noexcept;

  const nsACString& HostName() const;
  const mozilla::OriginAttributes& OriginAttributesRef() const;
  int32_t Port() const;
  uint32_t ProviderFlags() const;
  uint32_t ProviderTlsFlags() const;

  ClientAuthInfo(const ClientAuthInfo&) = delete;
  void operator=(const ClientAuthInfo&) = delete;

 private:
  nsCString mHostName;
  mozilla::OriginAttributes mOriginAttributes;
  int32_t mPort;
  uint32_t mProviderFlags;
  uint32_t mProviderTlsFlags;
};

ClientAuthInfo::ClientAuthInfo(const nsACString& hostName,
                               const OriginAttributes& originAttributes,
                               int32_t port, uint32_t providerFlags,
                               uint32_t providerTlsFlags)
    : mHostName(hostName),
      mOriginAttributes(originAttributes),
      mPort(port),
      mProviderFlags(providerFlags),
      mProviderTlsFlags(providerTlsFlags) {}

ClientAuthInfo::ClientAuthInfo(ClientAuthInfo&& aOther) noexcept
    : mHostName(std::move(aOther.mHostName)),
      mOriginAttributes(std::move(aOther.mOriginAttributes)),
      mPort(aOther.mPort),
      mProviderFlags(aOther.mProviderFlags),
      mProviderTlsFlags(aOther.mProviderTlsFlags) {}

const nsACString& ClientAuthInfo::HostName() const { return mHostName; }

const OriginAttributes& ClientAuthInfo::OriginAttributesRef() const {
  return mOriginAttributes;
}

int32_t ClientAuthInfo::Port() const { return mPort; }

uint32_t ClientAuthInfo::ProviderFlags() const { return mProviderFlags; }

uint32_t ClientAuthInfo::ProviderTlsFlags() const { return mProviderTlsFlags; }

nsTArray<nsTArray<uint8_t>> CollectCANames(CERTDistNames* caNames) {
  MOZ_ASSERT(caNames);

  nsTArray<nsTArray<uint8_t>> collectedCANames;
  if (!caNames) {
    return collectedCANames;
  }

  for (int i = 0; i < caNames->nnames; i++) {
    nsTArray<uint8_t> caName;
    caName.AppendElements(caNames->names[i].data, caNames->names[i].len);
    collectedCANames.AppendElement(std::move(caName));
  }
  return collectedCANames;
}

// This TrustDomain only exists to facilitate the mozilla::pkix path building
// algorithm. It considers any certificate with an issuer distinguished name in
// the set of given CA names to be a trust anchor. It does essentially no
// validation or verification (in particular, the signature checking function
// always returns "Success").
class ClientAuthCertNonverifyingTrustDomain final : public TrustDomain {
 public:
  ClientAuthCertNonverifyingTrustDomain(
      nsTArray<nsTArray<uint8_t>>& caNames,
      nsTArray<nsTArray<uint8_t>>& thirdPartyCertificates)
      : mCANames(caNames),
        mCertStorage(do_GetService(NS_CERT_STORAGE_CID)),
        mThirdPartyCertificates(thirdPartyCertificates) {}

  virtual mozilla::pkix::Result GetCertTrust(
      EndEntityOrCA endEntityOrCA, const CertPolicyId& policy,
      Input candidateCertDER,
      /*out*/ TrustLevel& trustLevel) override;
  virtual mozilla::pkix::Result FindIssuer(Input encodedIssuerName,
                                           IssuerChecker& checker,
                                           Time time) override;

  virtual mozilla::pkix::Result CheckRevocation(
      EndEntityOrCA endEntityOrCA, const CertID& certID, Time time,
      mozilla::pkix::Duration validityDuration,
      /*optional*/ const Input* stapledOCSPresponse,
      /*optional*/ const Input* aiaExtension,
      /*optional*/ const Input* sctExtension) override {
    return Success;
  }

  virtual mozilla::pkix::Result IsChainValid(
      const DERArray& certChain, Time time,
      const CertPolicyId& requiredPolicy) override;

  virtual mozilla::pkix::Result CheckSignatureDigestAlgorithm(
      DigestAlgorithm digestAlg, EndEntityOrCA endEntityOrCA,
      Time notBefore) override {
    return Success;
  }
  virtual mozilla::pkix::Result CheckRSAPublicKeyModulusSizeInBits(
      EndEntityOrCA endEntityOrCA, unsigned int modulusSizeInBits) override {
    return Success;
  }
  virtual mozilla::pkix::Result VerifyRSAPKCS1SignedData(
      Input data, DigestAlgorithm, Input signature,
      Input subjectPublicKeyInfo) override {
    return Success;
  }
  virtual mozilla::pkix::Result VerifyRSAPSSSignedData(
      Input data, DigestAlgorithm, Input signature,
      Input subjectPublicKeyInfo) override {
    return Success;
  }
  virtual mozilla::pkix::Result CheckECDSACurveIsAcceptable(
      EndEntityOrCA endEntityOrCA, NamedCurve curve) override {
    return Success;
  }
  virtual mozilla::pkix::Result VerifyECDSASignedData(
      Input data, DigestAlgorithm, Input signature,
      Input subjectPublicKeyInfo) override {
    return Success;
  }
  virtual mozilla::pkix::Result CheckValidityIsAcceptable(
      Time notBefore, Time notAfter, EndEntityOrCA endEntityOrCA,
      KeyPurposeId keyPurpose) override {
    return Success;
  }
  virtual mozilla::pkix::Result NetscapeStepUpMatchesServerAuth(
      Time notBefore,
      /*out*/ bool& matches) override {
    matches = true;
    return Success;
  }
  virtual void NoteAuxiliaryExtension(AuxiliaryExtension extension,
                                      Input extensionData) override {}
  virtual mozilla::pkix::Result DigestBuf(Input item, DigestAlgorithm digestAlg,
                                          /*out*/ uint8_t* digestBuf,
                                          size_t digestBufLen) override {
    return DigestBufNSS(item, digestAlg, digestBuf, digestBufLen);
  }

  nsTArray<nsTArray<uint8_t>> TakeBuiltChain() {
    return std::move(mBuiltChain);
  }

 private:
  nsTArray<nsTArray<uint8_t>>& mCANames;  // non-owning
  nsCOMPtr<nsICertStorage> mCertStorage;
  nsTArray<nsTArray<uint8_t>>& mThirdPartyCertificates;  // non-owning
  nsTArray<nsTArray<uint8_t>> mBuiltChain;
};

mozilla::pkix::Result ClientAuthCertNonverifyingTrustDomain::GetCertTrust(
    EndEntityOrCA endEntityOrCA, const CertPolicyId& policy,
    Input candidateCertDER,
    /*out*/ TrustLevel& trustLevel) {
  // If the server did not specify any CA names, all client certificates are
  // acceptable.
  if (mCANames.Length() == 0) {
    trustLevel = TrustLevel::TrustAnchor;
    return Success;
  }
  BackCert cert(candidateCertDER, endEntityOrCA, nullptr);
  mozilla::pkix::Result rv = cert.Init();
  if (rv != Success) {
    return rv;
  }
  // If this certificate's issuer distinguished name is in the set of acceptable
  // CA names, we say this is a trust anchor so that the client certificate
  // issued from this certificate will be presented as an option for the user.
  // We also check the certificate's subject distinguished name to account for
  // the case where client certificates that have the id-kp-OCSPSigning EKU
  // can't be trust anchors according to mozilla::pkix, and thus we may be
  // looking directly at the issuer.
  Input issuer(cert.GetIssuer());
  Input subject(cert.GetSubject());
  for (const auto& caName : mCANames) {
    Input caNameInput;
    rv = caNameInput.Init(caName.Elements(), caName.Length());
    if (rv != Success) {
      continue;  // probably too big
    }
    if (InputsAreEqual(issuer, caNameInput) ||
        InputsAreEqual(subject, caNameInput)) {
      trustLevel = TrustLevel::TrustAnchor;
      return Success;
    }
  }
  trustLevel = TrustLevel::InheritsTrust;
  return Success;
}

// In theory this implementation should only need to consider intermediate
// certificates, since in theory it should only need to look at the issuer
// distinguished name of each certificate to determine if the client
// certificate is considered acceptable to the server.
// However, because we need to account for client certificates with the
// id-kp-OCSPSigning EKU, and because mozilla::pkix doesn't allow such
// certificates to be trust anchors, we need to consider the issuers of such
// certificates directly. These issuers could be roots, so we have to consider
// roots here.
mozilla::pkix::Result ClientAuthCertNonverifyingTrustDomain::FindIssuer(
    Input encodedIssuerName, IssuerChecker& checker, Time time) {
  // First try all relevant certificates known to Gecko, which avoids calling
  // CERT_CreateSubjectCertList, because that can be expensive.
  Vector<Input> geckoCandidates;
  if (!mCertStorage) {
    return mozilla::pkix::Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
  nsTArray<uint8_t> subject;
  subject.AppendElements(encodedIssuerName.UnsafeGetData(),
                         encodedIssuerName.GetLength());
  nsTArray<nsTArray<uint8_t>> certs;
  nsresult rv = mCertStorage->FindCertsBySubject(subject, certs);
  if (NS_FAILED(rv)) {
    return mozilla::pkix::Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
  for (auto& cert : certs) {
    Input certDER;
    mozilla::pkix::Result rv = certDER.Init(cert.Elements(), cert.Length());
    if (rv != Success) {
      continue;  // probably too big
    }
    if (!geckoCandidates.append(certDER)) {
      return mozilla::pkix::Result::FATAL_ERROR_NO_MEMORY;
    }
  }

  for (const auto& thirdPartyCertificate : mThirdPartyCertificates) {
    Input thirdPartyCertificateInput;
    mozilla::pkix::Result rv = thirdPartyCertificateInput.Init(
        thirdPartyCertificate.Elements(), thirdPartyCertificate.Length());
    if (rv != Success) {
      continue;  // probably too big
    }
    if (!geckoCandidates.append(thirdPartyCertificateInput)) {
      return mozilla::pkix::Result::FATAL_ERROR_NO_MEMORY;
    }
  }

  bool keepGoing = true;
  for (Input candidate : geckoCandidates) {
    mozilla::pkix::Result rv = checker.Check(candidate, nullptr, keepGoing);
    if (rv != Success) {
      return rv;
    }
    if (!keepGoing) {
      return Success;
    }
  }

  SECItem encodedIssuerNameItem = UnsafeMapInputToSECItem(encodedIssuerName);
  // NSS seems not to differentiate between "no potential issuers found" and
  // "there was an error trying to retrieve the potential issuers." We assume
  // there was no error if CERT_CreateSubjectCertList returns nullptr.
  UniqueCERTCertList candidates(CERT_CreateSubjectCertList(
      nullptr, CERT_GetDefaultCertDB(), &encodedIssuerNameItem, 0, false));
  Vector<Input> nssCandidates;
  if (candidates) {
    for (CERTCertListNode* n = CERT_LIST_HEAD(candidates);
         !CERT_LIST_END(n, candidates); n = CERT_LIST_NEXT(n)) {
      Input certDER;
      mozilla::pkix::Result rv =
          certDER.Init(n->cert->derCert.data, n->cert->derCert.len);
      if (rv != Success) {
        continue;  // probably too big
      }
      if (!nssCandidates.append(certDER)) {
        return mozilla::pkix::Result::FATAL_ERROR_NO_MEMORY;
      }
    }
  }

  for (Input candidate : nssCandidates) {
    mozilla::pkix::Result rv = checker.Check(candidate, nullptr, keepGoing);
    if (rv != Success) {
      return rv;
    }
    if (!keepGoing) {
      return Success;
    }
  }
  return Success;
}

mozilla::pkix::Result ClientAuthCertNonverifyingTrustDomain::IsChainValid(
    const DERArray& certArray, Time, const CertPolicyId&) {
  mBuiltChain.Clear();

  size_t numCerts = certArray.GetLength();
  for (size_t i = 0; i < numCerts; ++i) {
    nsTArray<uint8_t> certBytes;
    const Input* certInput = certArray.GetDER(i);
    MOZ_ASSERT(certInput != nullptr);
    if (!certInput) {
      return mozilla::pkix::Result::FATAL_ERROR_LIBRARY_FAILURE;
    }
    certBytes.AppendElements(certInput->UnsafeGetData(),
                             certInput->GetLength());
    mBuiltChain.AppendElement(std::move(certBytes));
  }

  return Success;
}

void ClientAuthCertificateSelectedBase::SetSelectedClientAuthData(
    nsTArray<uint8_t>&& selectedCertBytes,
    nsTArray<nsTArray<uint8_t>>&& selectedCertChainBytes) {
  mSelectedCertBytes = std::move(selectedCertBytes);
  mSelectedCertChainBytes = std::move(selectedCertChainBytes);
}

NS_IMETHODIMP
ClientAuthCertificateSelected::Run() {
  mSocketInfo->ClientAuthCertificateSelected(mSelectedCertBytes,
                                             mSelectedCertChainBytes);
  return NS_OK;
}

// Helper runnable to select a client authentication certificate. Gets created
// on the socket thread or an IPC thread, runs on the main thread, and then runs
// its continuation on the socket thread.
class SelectClientAuthCertificate : public Runnable {
 public:
  SelectClientAuthCertificate(ClientAuthInfo&& info,
                              UniqueCERTCertificate&& serverCert,
                              nsTArray<nsTArray<uint8_t>>&& caNames,
                              ClientAuthCertificateSelectedBase* continuation)
      : Runnable("SelectClientAuthCertificate"),
        mInfo(std::move(info)),
        mServerCert(std::move(serverCert)),
        mCANames(std::move(caNames)),
        mContinuation(continuation) {}

  NS_IMETHOD Run() override;

 private:
  mozilla::pkix::Result BuildChainForCertificate(
      nsTArray<uint8_t>& certBytes,
      nsTArray<nsTArray<uint8_t>>& certChainBytes);
  void DoSelectClientAuthCertificate();

  ClientAuthInfo mInfo;
  UniqueCERTCertificate mServerCert;
  nsTArray<nsTArray<uint8_t>> mCANames;
  RefPtr<ClientAuthCertificateSelectedBase> mContinuation;

  nsTArray<nsTArray<uint8_t>> mEnterpriseCertificates;
  nsTArray<uint8_t> mSelectedCertBytes;
};

NS_IMETHODIMP
SelectClientAuthCertificate::Run() {
  DoSelectClientAuthCertificate();
  if (mSelectedCertBytes.Length() > 0) {
    nsTArray<nsTArray<uint8_t>> selectedCertChainBytes;
    if (BuildChainForCertificate(mSelectedCertBytes, selectedCertChainBytes) !=
        Success) {
      selectedCertChainBytes.Clear();
    }
    mContinuation->SetSelectedClientAuthData(std::move(mSelectedCertBytes),
                                             std::move(selectedCertChainBytes));
  }
  nsCOMPtr<nsIEventTarget> socketThread =
      do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID);
  if (NS_WARN_IF(!socketThread)) {
    return NS_ERROR_FAILURE;
  }
  nsresult rv = socketThread->Dispatch(mContinuation, NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

// Helper function to build a certificate chain from the given certificate to a
// trust anchor in the set indicated by the peer (mCANames). This is essentially
// best-effort, so no signature verification occurs.
mozilla::pkix::Result SelectClientAuthCertificate::BuildChainForCertificate(
    nsTArray<uint8_t>& certBytes, nsTArray<nsTArray<uint8_t>>& certChainBytes) {
  ClientAuthCertNonverifyingTrustDomain trustDomain(mCANames,
                                                    mEnterpriseCertificates);
  Input certDER;
  mozilla::pkix::Result result =
      certDER.Init(certBytes.Elements(), certBytes.Length());
  if (result != Success) {
    return result;
  }
  // Client certificates shouldn't be CAs, but for interoperability reasons we
  // attempt to build a path with each certificate as an end entity and then as
  // a CA if that fails.
  const EndEntityOrCA kEndEntityOrCAParams[] = {EndEntityOrCA::MustBeEndEntity,
                                                EndEntityOrCA::MustBeCA};
  // mozilla::pkix rejects certificates with id-kp-OCSPSigning unless it is
  // specifically required. A client certificate should never have this EKU.
  // Unfortunately, there are some client certificates in private PKIs that
  // have this EKU. For interoperability, we attempt to work around this
  // restriction in mozilla::pkix by first building the certificate chain with
  // no particular EKU required and then again with id-kp-OCSPSigning required
  // if that fails.
  const KeyPurposeId kKeyPurposeIdParams[] = {KeyPurposeId::anyExtendedKeyUsage,
                                              KeyPurposeId::id_kp_OCSPSigning};
  for (const auto& endEntityOrCAParam : kEndEntityOrCAParams) {
    for (const auto& keyPurposeIdParam : kKeyPurposeIdParams) {
      mozilla::pkix::Result result =
          BuildCertChain(trustDomain, certDER, Now(), endEntityOrCAParam,
                         KeyUsage::noParticularKeyUsageRequired,
                         keyPurposeIdParam, CertPolicyId::anyPolicy, nullptr);
      if (result == Success) {
        certChainBytes = trustDomain.TakeBuiltChain();
        return Success;
      }
    }
  }
  return mozilla::pkix::Result::ERROR_UNKNOWN_ISSUER;
}

void SelectClientAuthCertificate::DoSelectClientAuthCertificate() {
  // We check the value of a pref, so this should only be run on the main
  // thread.
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsINSSComponent> component(do_GetService(PSM_COMPONENT_CONTRACTID));
  if (NS_WARN_IF(!component)) {
    return;
  }
  nsresult rv = component->GetEnterpriseIntermediates(mEnterpriseCertificates);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }
  nsTArray<nsTArray<uint8_t>> enterpriseRoots;
  rv = component->GetEnterpriseRoots(enterpriseRoots);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }
  mEnterpriseCertificates.AppendElements(std::move(enterpriseRoots));

  if (NS_WARN_IF(NS_FAILED(CheckForSmartCardChanges()))) {
    return;
  }

  UniqueCERTCertList certList(FindClientCertificatesWithPrivateKeys());
  if (!certList) {
    return;
  }

  CERTCertListNode* n = CERT_LIST_HEAD(certList);
  while (!CERT_LIST_END(n, certList)) {
    nsTArray<nsTArray<uint8_t>> unusedBuiltChain;
    nsTArray<uint8_t> certBytes;
    certBytes.AppendElements(n->cert->derCert.data, n->cert->derCert.len);
    mozilla::pkix::Result result =
        BuildChainForCertificate(certBytes, unusedBuiltChain);
    if (result != Success) {
      MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
              ("removing cert '%s'", n->cert->subjectName));
      CERTCertListNode* toRemove = n;
      n = CERT_LIST_NEXT(n);
      CERT_RemoveCertListNode(toRemove);
      continue;
    }
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("keeping cert '%s'\n", n->cert->subjectName));
    n = CERT_LIST_NEXT(n);
  }

  if (CERT_LIST_EMPTY(certList)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("no client certificates available after filtering by CA"));
    return;
  }

  // find valid user cert and key pair
  if (nsGetUserCertChoice() == UserCertChoice::Auto) {
    // automatically find the right cert
    UniqueCERTCertificate lowPrioNonrepCert;
    // loop through the list until we find a cert with a key
    for (CERTCertListNode* node = CERT_LIST_HEAD(certList);
         !CERT_LIST_END(node, certList); node = CERT_LIST_NEXT(node)) {
      UniqueSECKEYPrivateKey tmpKey(PK11_FindKeyByAnyCert(node->cert, nullptr));
      if (tmpKey) {
        if (hasExplicitKeyUsageNonRepudiation(node->cert)) {
          // Not a preferred cert
          if (!lowPrioNonrepCert) {  // did not yet find a low prio cert
            lowPrioNonrepCert.reset(CERT_DupCertificate(node->cert));
          }
        } else {
          // this is a good cert to present
          mSelectedCertBytes.AppendElements(node->cert->derCert.data,
                                            node->cert->derCert.len);
          return;
        }
      }
      if (PR_GetError() == SEC_ERROR_BAD_PASSWORD) {
        // problem with password: bail
        break;
      }
    }

    if (lowPrioNonrepCert) {
      mSelectedCertBytes.AppendElements(lowPrioNonrepCert->derCert.data,
                                        lowPrioNonrepCert->derCert.len);
    }
    return;
  }

  // Not Auto => ask
  // Get the SSL Certificate
  const nsACString& hostname = mInfo.HostName();
  nsCOMPtr<nsIClientAuthRememberService> cars = nullptr;

  if (mInfo.ProviderTlsFlags() == 0) {
    cars = do_GetService(NS_CLIENTAUTHREMEMBERSERVICE_CONTRACTID);
  }

  if (cars) {
    nsCString rememberedDBKey;
    bool found;
    nsCOMPtr<nsIX509Cert> cert(new nsNSSCertificate(mServerCert.get()));
    nsresult rv = cars->HasRememberedDecision(
        hostname, mInfo.OriginAttributesRef(), cert, rememberedDBKey, &found);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }
    if (found) {
      // An empty dbKey indicates that the user chose not to use a certificate
      // and chose to remember this decision
      if (rememberedDBKey.IsEmpty()) {
        return;
      }
      nsCOMPtr<nsIX509CertDB> certdb = do_GetService(NS_X509CERTDB_CONTRACTID);
      if (NS_WARN_IF(!certdb)) {
        return;
      }
      nsCOMPtr<nsIX509Cert> foundCert;
      nsresult rv =
          certdb->FindCertByDBKey(rememberedDBKey, getter_AddRefs(foundCert));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return;
      }
      if (foundCert) {
        Unused << NS_WARN_IF(
            NS_FAILED(foundCert->GetRawDER(mSelectedCertBytes)));
        return;
      }
    }
  }

  // ask the user to select a certificate
  nsCOMPtr<nsIClientAuthDialogs> dialogs;
  UniquePORTString corg(CERT_GetOrgName(&mServerCert->subject));
  nsAutoCString org(corg.get());

  UniquePORTString cissuer(CERT_GetOrgName(&mServerCert->issuer));
  nsAutoCString issuer(cissuer.get());

  nsCOMPtr<nsIMutableArray> certArray = nsArrayBase::Create();
  if (NS_WARN_IF(!certArray)) {
    return;
  }

  for (CERTCertListNode* node = CERT_LIST_HEAD(certList);
       !CERT_LIST_END(node, certList); node = CERT_LIST_NEXT(node)) {
    nsCOMPtr<nsIX509Cert> tempCert = new nsNSSCertificate(node->cert);
    nsresult rv = certArray->AppendElement(tempCert);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }
  }

  // Throw up the client auth dialog and get back the index of the selected
  // cert
  rv = getNSSDialogs(getter_AddRefs(dialogs), NS_GET_IID(nsIClientAuthDialogs),
                     NS_CLIENTAUTHDIALOGS_CONTRACTID);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  uint32_t selectedIndex = 0;
  bool certChosen = false;

  // even if the user has canceled, we want to remember that, to avoid
  // repeating prompts
  bool wantRemember = false;
  rv =
      dialogs->ChooseCertificate(hostname, mInfo.Port(), org, issuer, certArray,
                                 &selectedIndex, &wantRemember, &certChosen);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  nsCOMPtr<nsIX509Cert> selectedCert;
  if (certChosen) {
    selectedCert = do_QueryElementAt(certArray, selectedIndex);
    if (NS_WARN_IF(!selectedCert)) {
      return;
    }
    if (NS_WARN_IF(NS_FAILED(selectedCert->GetRawDER(mSelectedCertBytes)))) {
      return;
    }
  }

  if (cars && wantRemember) {
    nsCOMPtr<nsIX509Cert> serverCert(new nsNSSCertificate(mServerCert.get()));
    rv = cars->RememberDecision(hostname, mInfo.OriginAttributesRef(),
                                serverCert, selectedCert);
    Unused << NS_WARN_IF(NS_FAILED(rv));
  }
}

SECStatus SSLGetClientAuthDataHook(void* arg, PRFileDesc* socket,
                                   CERTDistNames* caNamesDecoded,
                                   CERTCertificate** pRetCert,
                                   SECKEYPrivateKey** pRetKey) {
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
          ("[%p] SSLGetClientAuthDataHook", socket));

  if (!arg || !socket || !caNamesDecoded || !pRetCert || !pRetKey) {
    PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
    return SECFailure;
  }

  *pRetCert = nullptr;
  *pRetKey = nullptr;

  RefPtr<nsNSSSocketInfo> info(static_cast<nsNSSSocketInfo*>(arg));
  Telemetry::ScalarAdd(Telemetry::ScalarID::SECURITY_CLIENT_AUTH_CERT_USAGE,
                       u"requested"_ns, 1);

  if (info->GetDenyClientCert()) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("[%p] Not returning client cert due to denyClientCert attribute",
             socket));
    return SECSuccess;
  }

  if (info->GetJoined()) {
    // We refuse to send a client certificate when there are multiple hostnames
    // joined on this connection, because we only show the user one hostname
    // (mHostName) in the client certificate UI.
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("[%p] Not returning client cert due to previous join", socket));
    return SECSuccess;
  }

  UniqueCERTCertificate serverCert(SSL_PeerCertificate(socket));
  if (!serverCert) {
    PR_SetError(SSL_ERROR_NO_CERTIFICATE, 0);
    return SECFailure;
  }

  nsTArray<nsTArray<uint8_t>> caNames(CollectCANames(caNamesDecoded));

  RefPtr<ClientAuthCertificateSelected> continuation(
      new ClientAuthCertificateSelected(info));
  // If this is the socket process, dispatch an IPC call to select a client
  // authentication certificate in the parent process.
  // Otherwise, dispatch an event to the main thread to do the selection.
  // When those events finish, they will run the continuation, which gives the
  // appropriate information to the nsNSSSocketInfo, which then calls
  // SSL_ClientCertCallbackComplete to continue the connection.
  if (XRE_IsSocketProcess()) {
    // Currently, the IPC client certs module only refreshes its view of
    // available certificates and keys if the platform issues a search for all
    // certificates or keys. In the socket process, such a search may not have
    // happened, so this ensures it has.
    UniqueCERTCertList certList(FindClientCertificatesWithPrivateKeys());
    Unused << certList;

    mozilla::ipc::PBackgroundChild* actorChild = mozilla::ipc::BackgroundChild::
        GetOrCreateForSocketParentBridgeForCurrentThread();
    if (!actorChild) {
      PR_SetError(SEC_ERROR_LIBRARY_FAILURE, 0);
      return SECFailure;
    }
    RefPtr<SelectTLSClientAuthCertChild> selectClientAuthCertificate(
        new SelectTLSClientAuthCertChild(continuation));
    nsAutoCString hostname(info->GetHostName());
    nsTArray<uint8_t> serverCertBytes;
    nsTArray<ByteArray> caNamesBytes;
    for (const auto& caName : caNames) {
      caNamesBytes.AppendElement(ByteArray(std::move(caName)));
    }
    serverCertBytes.AppendElements(serverCert->derCert.data,
                                   serverCert->derCert.len);
    if (!actorChild->SendPSelectTLSClientAuthCertConstructor(
            selectClientAuthCertificate, hostname, info->GetOriginAttributes(),
            info->GetPort(), info->GetProviderFlags(),
            info->GetProviderTlsFlags(), ByteArray(serverCertBytes),
            caNamesBytes)) {
      PR_SetError(SEC_ERROR_LIBRARY_FAILURE, 0);
      return SECFailure;
    }
  } else {
    ClientAuthInfo authInfo(info->GetHostName(), info->GetOriginAttributes(),
                            info->GetPort(), info->GetProviderFlags(),
                            info->GetProviderTlsFlags());
    RefPtr<SelectClientAuthCertificate> selectClientAuthCertificate(
        new SelectClientAuthCertificate(std::move(authInfo),
                                        std::move(serverCert),
                                        std::move(caNames), continuation));
    if (NS_FAILED(NS_DispatchToMainThread(selectClientAuthCertificate))) {
      PR_SetError(SEC_ERROR_LIBRARY_FAILURE, 0);
      return SECFailure;
    }
  }

  // Meanwhile, tell NSS this connection is blocking for now.
  PR_SetError(PR_WOULD_BLOCK_ERROR, 0);
  return SECWouldBlock;
}

using mozilla::ipc::AssertIsOnBackgroundThread;
using mozilla::ipc::IsOnBackgroundThread;

// Helper continuation for when a client authentication certificate has been
// selected in the parent process and the information needs to be sent to the
// socket process.
class RemoteClientAuthCertificateSelected
    : public ClientAuthCertificateSelectedBase {
 public:
  explicit RemoteClientAuthCertificateSelected(
      SelectTLSClientAuthCertParent* selectTLSClientAuthCertParent)
      : mSelectTLSClientAuthCertParent(selectTLSClientAuthCertParent),
        mEventTarget(NS_GetCurrentThread()) {}

  NS_IMETHOD Run() override;

 private:
  RefPtr<SelectTLSClientAuthCertParent> mSelectTLSClientAuthCertParent;
  nsCOMPtr<nsIEventTarget> mEventTarget;
};

NS_IMETHODIMP
RemoteClientAuthCertificateSelected::Run() {
  // When this runs, it dispatches an event to the IPC thread it originally came
  // from in order to send the IPC call to the socket process that a client
  // authentication certificate has been selected.
  return mEventTarget->Dispatch(
      NS_NewRunnableFunction(
          "psm::RemoteClientAuthCertificateSelected::Run",
          [parent(mSelectTLSClientAuthCertParent),
           certBytes(std::move(mSelectedCertBytes)),
           builtCertChain(std::move(mSelectedCertChainBytes))]() mutable {
            parent->TLSClientAuthCertSelected(certBytes,
                                              std::move(builtCertChain));
          }),
      NS_DISPATCH_NORMAL);
}

namespace mozilla::psm {

// Given some information from the socket process about a connection that
// requested a client authentication certificate, this function dispatches an
// event to the main thread to ask the user to select one. When the user does so
// (or selects no certificate), the continuation runs and sends the information
// back via IPC.
bool SelectTLSClientAuthCertParent::Dispatch(
    const nsACString& aHostName, const OriginAttributes& aOriginAttributes,
    const int32_t& aPort, const uint32_t& aProviderFlags,
    const uint32_t& aProviderTlsFlags, const ByteArray& aServerCertBytes,
    nsTArray<ByteArray>&& aCANames) {
  RefPtr<ClientAuthCertificateSelectedBase> continuation(
      new RemoteClientAuthCertificateSelected(this));
  ClientAuthInfo authInfo(aHostName, aOriginAttributes, aPort, aProviderFlags,
                          aProviderTlsFlags);
  SECItem serverCertItem{
      siBuffer,
      const_cast<uint8_t*>(aServerCertBytes.data().Elements()),
      static_cast<unsigned int>(aServerCertBytes.data().Length()),
  };
  UniqueCERTCertificate serverCert(CERT_NewTempCertificate(
      CERT_GetDefaultCertDB(), &serverCertItem, nullptr, false, true));
  if (!serverCert) {
    return false;
  }
  nsTArray<nsTArray<uint8_t>> caNames;
  for (auto& caName : aCANames) {
    caNames.AppendElement(std::move(caName.data()));
  }
  RefPtr<SelectClientAuthCertificate> selectClientAuthCertificate(
      new SelectClientAuthCertificate(std::move(authInfo),
                                      std::move(serverCert), std::move(caNames),
                                      continuation));
  return NS_SUCCEEDED(NS_DispatchToMainThread(selectClientAuthCertificate));
}

void SelectTLSClientAuthCertParent::TLSClientAuthCertSelected(
    const nsTArray<uint8_t>& aSelectedCertBytes,
    nsTArray<nsTArray<uint8_t>>&& aSelectedCertChainBytes) {
  AssertIsOnBackgroundThread();

  if (!CanSend()) {
    return;
  }

  nsTArray<ByteArray> selectedCertChainBytes;
  for (auto& certBytes : aSelectedCertChainBytes) {
    selectedCertChainBytes.AppendElement(ByteArray(certBytes));
  }

  Unused << SendTLSClientAuthCertSelected(aSelectedCertBytes,
                                          selectedCertChainBytes);
  Unused << Send__delete__(this);
}

void SelectTLSClientAuthCertParent::ActorDestroy(
    mozilla::ipc::IProtocol::ActorDestroyReason aWhy) {}

SelectTLSClientAuthCertChild::SelectTLSClientAuthCertChild(
    ClientAuthCertificateSelected* continuation)
    : mContinuation(continuation) {}

// When the user has selected (or not) a client authentication certificate in
// the parent, this function receives that information in the socket process and
// dispatches a continuation to the socket process to continue the connection.
ipc::IPCResult SelectTLSClientAuthCertChild::RecvTLSClientAuthCertSelected(
    ByteArray&& aSelectedCertBytes,
    nsTArray<ByteArray>&& aSelectedCertChainBytes) {
  nsTArray<uint8_t> selectedCertBytes(std::move(aSelectedCertBytes.data()));
  nsTArray<nsTArray<uint8_t>> selectedCertChainBytes;
  for (auto& certBytes : aSelectedCertChainBytes) {
    selectedCertChainBytes.AppendElement(std::move(certBytes.data()));
  }
  mContinuation->SetSelectedClientAuthData(std::move(selectedCertBytes),
                                           std::move(selectedCertChainBytes));

  nsCOMPtr<nsIEventTarget> socketThread =
      do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID);
  if (NS_WARN_IF(!socketThread)) {
    return IPC_OK();
  }
  nsresult rv = socketThread->Dispatch(mContinuation, NS_DISPATCH_NORMAL);
  Unused << NS_WARN_IF(NS_FAILED(rv));

  return IPC_OK();
}

}  // namespace mozilla::psm
