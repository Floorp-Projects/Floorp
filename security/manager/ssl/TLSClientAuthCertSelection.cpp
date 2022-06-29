/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Implements the client authentication certificate selection callback for NSS.
// nsNSSIOLayer.cpp sets the callback by calling SSL_GetClientAuthDataHook and
// identifying nsNSS_SSLGetClientAuthData as the function to call when a TLS
// server requests a client authentication certificate.
//
// In the general case, nsNSS_SSLGetClientAuthData (running on the socket
// thread), dispatches an event to the main thread to ask the user to select a
// client authentication certificate. It then blocks, preventing all other
// networking from continuing. When the user selects a client certificate (or
// opts not to send one), nsNSS_SSLGetClientAuthData gathers the results from
// the event and returns, allowing networking to proceed again.
//
// When networking occurs on the socket process, nsNSS_SSLGetClientAuthData
// executes a synchronous ipc call to the main process to ask the user to
// select a certificate. Again this blocks until a response is received.

#include "TLSClientAuthCertSelection.h"
#include "cert_storage/src/cert_storage.h"
#include "mozilla/net/SocketProcessChild.h"
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

class ClientAuthDataRunnable : public SyncRunnableBase {
 public:
  ClientAuthDataRunnable(ClientAuthInfo&& info,
                         const UniqueCERTCertificate& serverCert,
                         nsTArray<nsTArray<uint8_t>>&& collectedCANames)
      : mInfo(std::move(info)),
        mServerCert(serverCert.get()),
        mCollectedCANames(std::move(collectedCANames)),
        mSelectedCertificate(nullptr) {}

  virtual mozilla::pkix::Result BuildChainForCertificate(
      CERTCertificate* cert, UniqueCERTCertList& builtChain);

  // Take the selected certificate. Will be null if none was selected or if an
  // error prevented selecting one.
  UniqueCERTCertificate TakeSelectedCertificate() {
    return std::move(mSelectedCertificate);
  }

 protected:
  virtual void RunOnTargetThread() override;

  ClientAuthInfo mInfo;
  CERTCertificate* const mServerCert;
  nsTArray<nsTArray<uint8_t>> mCollectedCANames;
  nsTArray<nsTArray<uint8_t>> mEnterpriseCertificates;
  UniqueCERTCertificate mSelectedCertificate;
};

class RemoteClientAuthDataRunnable : public ClientAuthDataRunnable {
 public:
  RemoteClientAuthDataRunnable(ClientAuthInfo&& info,
                               const UniqueCERTCertificate& serverCert,
                               nsTArray<nsTArray<uint8_t>>&& collectedCANames)
      : ClientAuthDataRunnable(std::move(info), serverCert,
                               std::move(collectedCANames)) {}

  virtual mozilla::pkix::Result BuildChainForCertificate(
      CERTCertificate* cert, UniqueCERTCertList& builtChain) override;

 protected:
  virtual void RunOnTargetThread() override;

  CopyableTArray<ByteArray> mBuiltChain;
};

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

// This callback function is used to pull client certificate
// information upon server request
//
// - arg: SSL data connection
// - socket: SSL socket we're dealing with
// - caNames: list of CA names to use as a hint for selecting potential client
//            certificates (may be empty)
// - pRetCert: returns a pointer to a pointer to a valid certificate if
//             successful; otherwise nullptr
// - pRetKey: returns a pointer to a pointer to the corresponding key if
//            successful; otherwise nullptr
SECStatus nsNSS_SSLGetClientAuthData(void* arg, PRFileDesc* socket,
                                     CERTDistNames* caNames,
                                     CERTCertificate** pRetCert,
                                     SECKEYPrivateKey** pRetKey) {
  if (!socket || !caNames || !pRetCert || !pRetKey) {
    PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
    return SECFailure;
  }

  *pRetCert = nullptr;
  *pRetKey = nullptr;

  RefPtr<nsNSSSocketInfo> info(
      BitwiseCast<nsNSSSocketInfo*, PRFilePrivate*>(socket->higher->secret));

  UniqueCERTCertificate serverCert(SSL_PeerCertificate(socket));
  if (!serverCert) {
    MOZ_ASSERT_UNREACHABLE(
        "Missing server cert should have been detected during server cert "
        "auth.");
    PR_SetError(SSL_ERROR_NO_CERTIFICATE, 0);
    return SECFailure;
  }

  if (info->GetDenyClientCert()) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("[%p] Not returning client cert due to denyClientCert attribute\n",
             socket));
    return SECSuccess;
  }

  if (info->GetJoined()) {
    // We refuse to send a client certificate when there are multiple hostnames
    // joined on this connection, because we only show the user one hostname
    // (mHostName) in the client certificate UI.

    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("[%p] Not returning client cert due to previous join\n", socket));
    return SECSuccess;
  }

  ClientAuthInfo authInfo(info->GetHostName(), info->GetOriginAttributes(),
                          info->GetPort(), info->GetProviderFlags(),
                          info->GetProviderTlsFlags());
  nsTArray<nsTArray<uint8_t>> collectedCANames(CollectCANames(caNames));

  UniqueCERTCertificate selectedCertificate;
  UniqueCERTCertList builtChain;
  SECStatus status = DoGetClientAuthData(std::move(authInfo), serverCert,
                                         std::move(collectedCANames),
                                         selectedCertificate, builtChain);
  if (status != SECSuccess) {
    return status;
  }

  // Currently, the IPC client certs module only refreshes its view of
  // available certificates and keys if the platform issues a search for all
  // certificates or keys. In the socket process, such a search may not have
  // happened, so this ensures it has.
  if (XRE_IsSocketProcess()) {
    UniqueCERTCertList certList(FindClientCertificatesWithPrivateKeys());
    Unused << certList;
  }

  if (selectedCertificate) {
    UniqueSECKEYPrivateKey selectedKey(
        PK11_FindKeyByAnyCert(selectedCertificate.get(), nullptr));
    if (selectedKey) {
      if (builtChain) {
        info->SetClientCertChain(std::move(builtChain));
      } else {
        MOZ_LOG(
            gPIPNSSLog, LogLevel::Debug,
            ("[%p] couldn't determine chain for selected client cert", socket));
      }
      *pRetCert = selectedCertificate.release();
      *pRetKey = selectedKey.release();
      // Make joinConnection prohibit joining after we've sent a client cert
      info->SetSentClientCert();
      if (info->GetSSLVersionUsed() == nsISSLSocketControl::TLS_VERSION_1_3) {
        Telemetry::Accumulate(Telemetry::TLS_1_3_CLIENT_AUTH_USES_PHA,
                              info->IsHandshakeCompleted());
      }
    }
  }

  return SECSuccess;
}

SECStatus DoGetClientAuthData(ClientAuthInfo&& info,
                              const UniqueCERTCertificate& serverCert,
                              nsTArray<nsTArray<uint8_t>>&& collectedCANames,
                              UniqueCERTCertificate& outCert,
                              UniqueCERTCertList& outBuiltChain) {
  // XXX: This should be done asynchronously; see bug 696976
  RefPtr<ClientAuthDataRunnable> runnable =
      XRE_IsSocketProcess()
          ? new RemoteClientAuthDataRunnable(std::move(info), serverCert,
                                             std::move(collectedCANames))
          : new ClientAuthDataRunnable(std::move(info), serverCert,
                                       std::move(collectedCANames));

  nsresult rv = runnable->DispatchToMainThreadAndWait();
  if (NS_FAILED(rv)) {
    PR_SetError(SEC_ERROR_NO_MEMORY, 0);
    return SECFailure;
  }

  outCert = runnable->TakeSelectedCertificate();
  if (outCert) {
    mozilla::pkix::Result result =
        runnable->BuildChainForCertificate(outCert.get(), outBuiltChain);
    if (result != Success) {
      outBuiltChain.reset(nullptr);
    }
  }

  return SECSuccess;
}

// This TrustDomain only exists to facilitate the mozilla::pkix path building
// algorithm. It considers any certificate with an issuer distinguished name in
// the set of given CA names to be a trust anchor. It does essentially no
// validation or verification (in particular, the signature checking function
// always returns "Success").
class ClientAuthCertNonverifyingTrustDomain final : public TrustDomain {
 public:
  ClientAuthCertNonverifyingTrustDomain(
      nsTArray<nsTArray<uint8_t>>& collectedCANames,
      nsTArray<nsTArray<uint8_t>>& thirdPartyCertificates)
      : mCollectedCANames(collectedCANames),
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

  UniqueCERTCertList TakeBuiltChain() { return std::move(mBuiltChain); }

 private:
  nsTArray<nsTArray<uint8_t>>& mCollectedCANames;  // non-owning
  nsCOMPtr<nsICertStorage> mCertStorage;
  nsTArray<nsTArray<uint8_t>>& mThirdPartyCertificates;  // non-owning
  UniqueCERTCertList mBuiltChain;
};

mozilla::pkix::Result ClientAuthCertNonverifyingTrustDomain::GetCertTrust(
    EndEntityOrCA endEntityOrCA, const CertPolicyId& policy,
    Input candidateCertDER,
    /*out*/ TrustLevel& trustLevel) {
  // If the server did not specify any CA names, all client certificates are
  // acceptable.
  if (mCollectedCANames.Length() == 0) {
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
  for (const auto& caName : mCollectedCANames) {
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
  mBuiltChain = UniqueCERTCertList(CERT_NewCertList());
  if (!mBuiltChain) {
    return MapPRErrorCodeToResult(PR_GetError());
  }

  CERTCertDBHandle* certDB(CERT_GetDefaultCertDB());  // non-owning

  size_t numCerts = certArray.GetLength();
  for (size_t i = 0; i < numCerts; ++i) {
    SECItem certDER(UnsafeMapInputToSECItem(*certArray.GetDER(i)));
    UniqueCERTCertificate cert(
        CERT_NewTempCertificate(certDB, &certDER, nullptr, false, true));
    if (!cert) {
      return MapPRErrorCodeToResult(PR_GetError());
    }
    // certArray is ordered with the root first, but we want the resulting
    // certList to have the root last.
    if (CERT_AddCertToListHead(mBuiltChain.get(), cert.get()) != SECSuccess) {
      return MapPRErrorCodeToResult(PR_GetError());
    }
    Unused << cert.release();  // cert is now owned by mBuiltChain.
  }

  return Success;
}

mozilla::pkix::Result ClientAuthDataRunnable::BuildChainForCertificate(
    CERTCertificate* cert, UniqueCERTCertList& builtChain) {
  ClientAuthCertNonverifyingTrustDomain trustDomain(mCollectedCANames,
                                                    mEnterpriseCertificates);
  Input certDER;
  mozilla::pkix::Result result =
      certDER.Init(cert->derCert.data, cert->derCert.len);
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
        builtChain = trustDomain.TakeBuiltChain();
        return Success;
      }
      MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
              ("client cert non-validation returned %d for '%s'",
               static_cast<int>(result), cert->subjectName));
    }
  }
  return mozilla::pkix::Result::ERROR_UNKNOWN_ISSUER;
}

void ClientAuthDataRunnable::RunOnTargetThread() {
  // We check the value of a pref in this runnable, so this runnable should only
  // be run on the main thread.
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
    UniqueCERTCertList unusedBuiltChain;
    mozilla::pkix::Result result =
        BuildChainForCertificate(n->cert, unusedBuiltChain);
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
          mSelectedCertificate.reset(CERT_DupCertificate(node->cert));
          return;
        }
      }
      if (PR_GetError() == SEC_ERROR_BAD_PASSWORD) {
        // problem with password: bail
        break;
      }
    }

    if (lowPrioNonrepCert) {
      mSelectedCertificate = std::move(lowPrioNonrepCert);
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
    nsCOMPtr<nsIX509Cert> cert(new nsNSSCertificate(mServerCert));
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
        nsNSSCertificate* objCert =
            BitwiseCast<nsNSSCertificate*, nsIX509Cert*>(foundCert.get());
        if (NS_WARN_IF(!objCert)) {
          return;
        }
        mSelectedCertificate.reset(objCert->GetCert());
        if (NS_WARN_IF(!mSelectedCertificate)) {
          return;
        }
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

  if (certChosen) {
    nsCOMPtr<nsIX509Cert> selectedCert =
        do_QueryElementAt(certArray, selectedIndex);
    if (NS_WARN_IF(!selectedCert)) {
      return;
    }
    mSelectedCertificate.reset(selectedCert->GetCert());
    if (NS_WARN_IF(!mSelectedCertificate)) {
      return;
    }
  }

  if (cars && wantRemember) {
    nsCOMPtr<nsIX509Cert> serverCert(new nsNSSCertificate(mServerCert));
    nsCOMPtr<nsIX509Cert> clientCert;
    if (certChosen) {
      clientCert = new nsNSSCertificate(mSelectedCertificate.get());
    }
    rv = cars->RememberDecision(hostname, mInfo.OriginAttributesRef(),
                                serverCert, clientCert);
    Unused << NS_WARN_IF(NS_FAILED(rv));
  }
}

mozilla::pkix::Result RemoteClientAuthDataRunnable::BuildChainForCertificate(
    CERTCertificate*, UniqueCERTCertList& builtChain) {
  builtChain.reset(CERT_NewCertList());
  if (!builtChain) {
    return mozilla::pkix::Result::FATAL_ERROR_NO_MEMORY;
  }

  for (auto& certBytes : mBuiltChain) {
    SECItem certDER = {siBuffer, certBytes.data().Elements(),
                       static_cast<unsigned int>(certBytes.data().Length())};
    UniqueCERTCertificate cert(CERT_NewTempCertificate(
        CERT_GetDefaultCertDB(), &certDER, nullptr, false, true));
    if (!cert) {
      return mozilla::pkix::Result::ERROR_BAD_DER;
    }

    if (CERT_AddCertToListTail(builtChain.get(), cert.get()) != SECSuccess) {
      return mozilla::pkix::Result::FATAL_ERROR_NO_MEMORY;
    }
    Unused << cert.release();
  }

  return Success;
}

void RemoteClientAuthDataRunnable::RunOnTargetThread() {
  MOZ_ASSERT(NS_IsMainThread());

  const ByteArray serverCertSerialized = CopyableTArray<uint8_t>{
      mServerCert->derCert.data, mServerCert->derCert.len};

  nsTArray<ByteArray> collectedCANames;
  for (auto& name : mCollectedCANames) {
    collectedCANames.AppendElement(std::move(name));
  }

  bool succeeded = false;
  ByteArray cert;
  mozilla::net::SocketProcessChild::GetSingleton()->SendGetTLSClientCert(
      nsCString(mInfo.HostName()), mInfo.OriginAttributesRef(), mInfo.Port(),
      mInfo.ProviderFlags(), mInfo.ProviderTlsFlags(), serverCertSerialized,
      collectedCANames, &succeeded, &cert, &mBuiltChain);

  if (!succeeded) {
    return;
  }

  SECItem certItem = {siBuffer, const_cast<uint8_t*>(cert.data().Elements()),
                      static_cast<unsigned int>(cert.data().Length())};
  mSelectedCertificate.reset(CERT_NewTempCertificate(
      CERT_GetDefaultCertDB(), &certItem, nullptr, false, true));
}
