/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LocalCertService.h"

#include "CryptoTask.h"
#include "ScopedNSSTypes.h"
#include "cert.h"
#include "mozilla/Casting.h"
#include "mozilla/ModuleUtils.h"
#include "mozilla/RefPtr.h"
#include "nsIPK11Token.h"
#include "nsIPK11TokenDB.h"
#include "nsIX509Cert.h"
#include "nsIX509CertValidity.h"
#include "nsLiteralString.h"
#include "nsProxyRelease.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "pk11pub.h"

namespace mozilla {

// Given a name, searches the internal certificate/key database for a
// self-signed certificate with subject and issuer distinguished name equal to
// "CN={name}". This assumes that the user has already authenticated to the
// internal DB if necessary.
static nsresult
FindLocalCertByName(const nsACString& aName,
            /*out*/ UniqueCERTCertificate& aResult)
{
  aResult.reset(nullptr);
  NS_NAMED_LITERAL_CSTRING(commonNamePrefix, "CN=");
  nsAutoCString expectedDistinguishedName(commonNamePrefix + aName);
  UniquePK11SlotInfo slot(PK11_GetInternalKeySlot());
  if (!slot) {
    return mozilla::psm::GetXPCOMFromNSSError(PR_GetError());
  }
  UniqueCERTCertList certList(PK11_ListCertsInSlot(slot.get()));
  if (!certList) {
    return NS_ERROR_UNEXPECTED;
  }
  for (const CERTCertListNode* node = CERT_LIST_HEAD(certList);
       !CERT_LIST_END(node, certList); node = CERT_LIST_NEXT(node)) {
    // If this isn't a self-signed cert, it's not what we're interested in.
    if (!node->cert->isRoot) {
      continue;
    }
    if (!expectedDistinguishedName.Equals(node->cert->subjectName)) {
      continue; // Subject should match nickname
    }
    if (!expectedDistinguishedName.Equals(node->cert->issuerName)) {
      continue; // Issuer should match nickname
    }
    // We found a match.
    aResult.reset(CERT_DupCertificate(node->cert));
    return NS_OK;
  }
  return NS_OK;
}

class LocalCertTask : public CryptoTask
{
protected:
  explicit LocalCertTask(const nsACString& aNickname)
    : mNickname(aNickname)
  {
  }

  nsresult RemoveExisting()
  {
    // Search for any existing self-signed certs with this name and remove them
    for (;;) {
      UniqueCERTCertificate cert;
      nsresult rv = FindLocalCertByName(mNickname, cert);
      if (NS_FAILED(rv)) {
        return rv;
      }
      // If we didn't find a match, we're done.
      if (!cert) {
        return NS_OK;
      }
      rv = MapSECStatus(PK11_DeleteTokenCertAndKey(cert.get(), nullptr));
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
  }

  nsCString mNickname;
};

class LocalCertGetTask final : public LocalCertTask
{
public:
  LocalCertGetTask(const nsACString& aNickname,
                   nsILocalCertGetCallback* aCallback)
    : LocalCertTask(aNickname)
    , mCallback(new nsMainThreadPtrHolder<nsILocalCertGetCallback>(aCallback))
    , mCert(nullptr)
  {
  }

private:
  virtual nsresult CalculateResult() override
  {
    // Try to lookup an existing cert in the DB
    nsresult rv = GetFromDB();
    // Make a new one if getting fails
    if (NS_FAILED(rv)) {
      rv = Generate();
    }
    // If generation fails, we're out of luck
    if (NS_FAILED(rv)) {
      return rv;
    }

    // Validate cert, make a new one if it fails
    rv = Validate();
    if (NS_FAILED(rv)) {
      rv = Generate();
    }
    // If generation fails, we're out of luck
    if (NS_FAILED(rv)) {
      return rv;
    }

    return NS_OK;
  }

  nsresult Generate()
  {
    nsresult rv;

    // Get the key slot for generation later
    UniquePK11SlotInfo slot(PK11_GetInternalKeySlot());
    if (!slot) {
      return mozilla::psm::GetXPCOMFromNSSError(PR_GetError());
    }

    // Remove existing certs with this name (if any)
    rv = RemoveExisting();
    if (NS_FAILED(rv)) {
      return rv;
    }

    // Generate a new cert
    NS_NAMED_LITERAL_CSTRING(commonNamePrefix, "CN=");
    nsAutoCString subjectNameStr(commonNamePrefix + mNickname);
    UniqueCERTName subjectName(CERT_AsciiToName(subjectNameStr.get()));
    if (!subjectName) {
      return mozilla::psm::GetXPCOMFromNSSError(PR_GetError());
    }

    // Use the well-known NIST P-256 curve
    SECOidData* curveOidData = SECOID_FindOIDByTag(SEC_OID_SECG_EC_SECP256R1);
    if (!curveOidData) {
      return mozilla::psm::GetXPCOMFromNSSError(PR_GetError());
    }

    // Get key params from the curve
    ScopedAutoSECItem keyParams(2 + curveOidData->oid.len);
    keyParams.data[0] = SEC_ASN1_OBJECT_ID;
    keyParams.data[1] = curveOidData->oid.len;
    memcpy(keyParams.data + 2, curveOidData->oid.data, curveOidData->oid.len);

    // Generate cert key pair
    SECKEYPublicKey* tempPublicKey;
    UniqueSECKEYPrivateKey privateKey(
      PK11_GenerateKeyPair(slot.get(), CKM_EC_KEY_PAIR_GEN, &keyParams,
                           &tempPublicKey, true /* token */,
                           true /* sensitive */, nullptr));
    UniqueSECKEYPublicKey publicKey(tempPublicKey);
    tempPublicKey = nullptr;
    if (!privateKey || !publicKey) {
      return mozilla::psm::GetXPCOMFromNSSError(PR_GetError());
    }

    // Create subject public key info and cert request
    UniqueCERTSubjectPublicKeyInfo spki(
      SECKEY_CreateSubjectPublicKeyInfo(publicKey.get()));
    if (!spki) {
      return mozilla::psm::GetXPCOMFromNSSError(PR_GetError());
    }
    UniqueCERTCertificateRequest certRequest(
      CERT_CreateCertificateRequest(subjectName.get(), spki.get(), nullptr));
    if (!certRequest) {
      return mozilla::psm::GetXPCOMFromNSSError(PR_GetError());
    }

    // Valid from one day before to 1 year after
    static const PRTime oneDay = PRTime(PR_USEC_PER_SEC)
                               * PRTime(60)  // sec
                               * PRTime(60)  // min
                               * PRTime(24); // hours

    PRTime now = PR_Now();
    PRTime notBefore = now - oneDay;
    PRTime notAfter = now + (PRTime(365) * oneDay);
    UniqueCERTValidity validity(CERT_CreateValidity(notBefore, notAfter));
    if (!validity) {
      return mozilla::psm::GetXPCOMFromNSSError(PR_GetError());
    }

    // Generate random serial
    unsigned long serial;
    // This serial in principle could collide, but it's unlikely
    rv = MapSECStatus(PK11_GenerateRandomOnSlot(
           slot.get(), BitwiseCast<unsigned char*, unsigned long*>(&serial),
           sizeof(serial)));
    if (NS_FAILED(rv)) {
      return rv;
    }

    // Create the cert from these pieces
    UniqueCERTCertificate cert(
      CERT_CreateCertificate(serial, subjectName.get(), validity.get(),
                             certRequest.get()));
    if (!cert) {
      return mozilla::psm::GetXPCOMFromNSSError(PR_GetError());
    }

    // Update the cert version to X509v3
    if (!cert->version.data) {
      return NS_ERROR_INVALID_POINTER;
    }
    *(cert->version.data) = SEC_CERTIFICATE_VERSION_3;
    cert->version.len = 1;

    // Set cert signature algorithm
    PLArenaPool* arena = cert->arena;
    if (!arena) {
      return NS_ERROR_INVALID_POINTER;
    }
    rv = MapSECStatus(
           SECOID_SetAlgorithmID(arena, &cert->signature,
                                 SEC_OID_ANSIX962_ECDSA_SHA256_SIGNATURE, 0));
    if (NS_FAILED(rv)) {
      return rv;
    }

    // Encode and self-sign the cert
    UniqueSECItem certDER(
      SEC_ASN1EncodeItem(nullptr, nullptr, cert.get(),
                         SEC_ASN1_GET(CERT_CertificateTemplate)));
    if (!certDER) {
      return mozilla::psm::GetXPCOMFromNSSError(PR_GetError());
    }
    rv = MapSECStatus(
           SEC_DerSignData(arena, &cert->derCert, certDER->data, certDER->len,
                           privateKey.get(),
                           SEC_OID_ANSIX962_ECDSA_SHA256_SIGNATURE));
    if (NS_FAILED(rv)) {
      return rv;
    }

    // Create a CERTCertificate from the signed data
    UniqueCERTCertificate certFromDER(
      CERT_NewTempCertificate(CERT_GetDefaultCertDB(), &cert->derCert, nullptr,
                              true /* perm */, true /* copyDER */));
    if (!certFromDER) {
      return mozilla::psm::GetXPCOMFromNSSError(PR_GetError());
    }

    // Save the cert in the DB
    rv = MapSECStatus(PK11_ImportCert(slot.get(), certFromDER.get(),
                                      CK_INVALID_HANDLE, mNickname.get(),
                                      false /* unused */));
    if (NS_FAILED(rv)) {
      return rv;
    }

    // We should now have cert in the DB, read it back in nsIX509Cert form
    return GetFromDB();
  }

  nsresult GetFromDB()
  {
    UniqueCERTCertificate cert;
    nsresult rv = FindLocalCertByName(mNickname, cert);
    if (NS_FAILED(rv)) {
      return rv;
    }
    if (!cert) {
      return NS_ERROR_FAILURE;
    }
    mCert = nsNSSCertificate::Create(cert.get());
    return NS_OK;
  }

  nsresult Validate()
  {
    // Verify cert is self-signed
    bool selfSigned;
    nsresult rv = mCert->GetIsSelfSigned(&selfSigned);
    if (NS_FAILED(rv)) {
      return rv;
    }
    if (!selfSigned) {
      return NS_ERROR_FAILURE;
    }

    // Check that subject and issuer match nickname
    nsXPIDLString subjectName;
    nsXPIDLString issuerName;
    mCert->GetSubjectName(subjectName);
    mCert->GetIssuerName(issuerName);
    if (!subjectName.Equals(issuerName)) {
      return NS_ERROR_FAILURE;
    }
    NS_NAMED_LITERAL_STRING(commonNamePrefix, "CN=");
    nsAutoString subjectNameFromNickname(
      commonNamePrefix + NS_ConvertASCIItoUTF16(mNickname));
    if (!subjectName.Equals(subjectNameFromNickname)) {
      return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIX509CertValidity> validity;
    mCert->GetValidity(getter_AddRefs(validity));

    PRTime notBefore, notAfter;
    validity->GetNotBefore(&notBefore);
    validity->GetNotAfter(&notAfter);

    // Ensure cert will last at least one more day
    static const PRTime oneDay = PRTime(PR_USEC_PER_SEC)
                               * PRTime(60)  // sec
                               * PRTime(60)  // min
                               * PRTime(24); // hours
    PRTime now = PR_Now();
    if (notBefore > now ||
        notAfter < (now - oneDay)) {
      return NS_ERROR_FAILURE;
    }

    return NS_OK;
  }

  virtual void ReleaseNSSResources() override {}

  virtual void CallCallback(nsresult rv) override
  {
    (void) mCallback->HandleCert(mCert, rv);
  }

  nsMainThreadPtrHandle<nsILocalCertGetCallback> mCallback;
  nsCOMPtr<nsIX509Cert> mCert; // out
};

class LocalCertRemoveTask final : public LocalCertTask
{
public:
  LocalCertRemoveTask(const nsACString& aNickname,
                      nsILocalCertCallback* aCallback)
    : LocalCertTask(aNickname)
    , mCallback(new nsMainThreadPtrHolder<nsILocalCertCallback>(aCallback))
  {
  }

private:
  virtual nsresult CalculateResult() override
  {
    return RemoveExisting();
  }

  virtual void ReleaseNSSResources() override {}

  virtual void CallCallback(nsresult rv) override
  {
    (void) mCallback->HandleResult(rv);
  }

  nsMainThreadPtrHandle<nsILocalCertCallback> mCallback;
};

NS_IMPL_ISUPPORTS(LocalCertService, nsILocalCertService)

LocalCertService::LocalCertService()
{
}

LocalCertService::~LocalCertService()
{
}

nsresult
LocalCertService::LoginToKeySlot()
{
  nsresult rv;

  // Get access to key slot
  UniquePK11SlotInfo slot(PK11_GetInternalKeySlot());
  if (!slot) {
    return mozilla::psm::GetXPCOMFromNSSError(PR_GetError());
  }

  // If no user password yet, set it an empty one
  if (PK11_NeedUserInit(slot.get())) {
    rv = MapSECStatus(PK11_InitPin(slot.get(), "", ""));
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  // If user has a password set, prompt to login
  if (PK11_NeedLogin(slot.get()) && !PK11_IsLoggedIn(slot.get(), nullptr)) {
    // Switching to XPCOM to get the UI prompt that PSM owns
    nsCOMPtr<nsIPK11TokenDB> tokenDB =
      do_GetService(NS_PK11TOKENDB_CONTRACTID);
    if (!tokenDB) {
      return NS_ERROR_FAILURE;
    }
    nsCOMPtr<nsIPK11Token> keyToken;
    tokenDB->GetInternalKeyToken(getter_AddRefs(keyToken));
    if (!keyToken) {
      return NS_ERROR_FAILURE;
    }
    // Prompt the user to login
    return keyToken->Login(false /* force */);
  }

  return NS_OK;
}

NS_IMETHODIMP
LocalCertService::GetOrCreateCert(const nsACString& aNickname,
                                  nsILocalCertGetCallback* aCallback)
{
  if (NS_WARN_IF(aNickname.IsEmpty())) {
    return NS_ERROR_INVALID_ARG;
  }
  if (NS_WARN_IF(!aCallback)) {
    return NS_ERROR_INVALID_POINTER;
  }

  // Before sending off the task, login to key slot if needed
  nsresult rv = LoginToKeySlot();
  if (NS_FAILED(rv)) {
    aCallback->HandleCert(nullptr, rv);
    return NS_OK;
  }

  RefPtr<LocalCertGetTask> task(new LocalCertGetTask(aNickname, aCallback));
  return task->Dispatch("LocalCertGet");
}

NS_IMETHODIMP
LocalCertService::RemoveCert(const nsACString& aNickname,
                             nsILocalCertCallback* aCallback)
{
  if (NS_WARN_IF(aNickname.IsEmpty())) {
    return NS_ERROR_INVALID_ARG;
  }
  if (NS_WARN_IF(!aCallback)) {
    return NS_ERROR_INVALID_POINTER;
  }

  // Before sending off the task, login to key slot if needed
  nsresult rv = LoginToKeySlot();
  if (NS_FAILED(rv)) {
    aCallback->HandleResult(rv);
    return NS_OK;
  }

  RefPtr<LocalCertRemoveTask> task(
    new LocalCertRemoveTask(aNickname, aCallback));
  return task->Dispatch("LocalCertRm");
}

NS_IMETHODIMP
LocalCertService::GetLoginPromptRequired(bool* aRequired)
{
  nsresult rv;

  // Get access to key slot
  UniquePK11SlotInfo slot(PK11_GetInternalKeySlot());
  if (!slot) {
    return mozilla::psm::GetXPCOMFromNSSError(PR_GetError());
  }

  // If no user password yet, set it an empty one
  if (PK11_NeedUserInit(slot.get())) {
    rv = MapSECStatus(PK11_InitPin(slot.get(), "", ""));
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  *aRequired = PK11_NeedLogin(slot.get()) &&
               !PK11_IsLoggedIn(slot.get(), nullptr);
  return NS_OK;
}

#define LOCALCERTSERVICE_CID \
{ 0x47402be2, 0xe653, 0x45d0, \
  { 0x8d, 0xaa, 0x9f, 0x0d, 0xce, 0x0a, 0xc1, 0x48 } }

NS_GENERIC_FACTORY_CONSTRUCTOR(LocalCertService)

NS_DEFINE_NAMED_CID(LOCALCERTSERVICE_CID);

static const Module::CIDEntry kLocalCertServiceCIDs[] = {
  { &kLOCALCERTSERVICE_CID, false, nullptr, LocalCertServiceConstructor },
  { nullptr }
};

static const Module::ContractIDEntry kLocalCertServiceContracts[] = {
  { LOCALCERTSERVICE_CONTRACTID, &kLOCALCERTSERVICE_CID },
  { nullptr }
};

static const Module kLocalCertServiceModule = {
  Module::kVersion,
  kLocalCertServiceCIDs,
  kLocalCertServiceContracts
};

NSMODULE_DEFN(LocalCertServiceModule) = &kLocalCertServiceModule;

} // namespace mozilla
