/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNSSCertificate.h"

#include "prmem.h"
#include "prerror.h"
#include "prprf.h"
#include "CertVerifier.h"
#include "ExtendedValidation.h"
#include "pkix/pkixnss.h"
#include "pkix/pkixtypes.h"
#include "pkix/ScopedPtr.h"
#include "nsNSSComponent.h" // for PIPNSS string bundle calls.
#include "nsNSSCleaner.h"
#include "nsCOMPtr.h"
#include "nsIMutableArray.h"
#include "nsNSSCertValidity.h"
#include "nsPKCS12Blob.h"
#include "nsPK11TokenDB.h"
#include "nsIX509Cert.h"
#include "nsIClassInfoImpl.h"
#include "nsNSSASN1Object.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsIURI.h"
#include "nsCRT.h"
#include "nsUsageArrayHelper.h"
#include "nsICertificateDialogs.h"
#include "nsNSSCertHelper.h"
#include "nsISupportsPrimitives.h"
#include "nsUnicharUtils.h"
#include "nsThreadUtils.h"
#include "nsCertVerificationThread.h"
#include "nsIObjectOutputStream.h"
#include "nsIObjectInputStream.h"
#include "nsIProgrammingLanguage.h"
#include "nsXULAppAPI.h"
#include "nsProxyRelease.h"
#include "mozilla/Base64.h"
#include "NSSCertDBTrustDomain.h"
#include "nspr.h"
#include "certdb.h"
#include "pkix/pkixtypes.h"
#include "secerr.h"
#include "nssb64.h"
#include "secasn1.h"
#include "secder.h"
#include "ssl.h"
#include "plbase64.h"

using namespace mozilla;
using namespace mozilla::psm;

#ifdef PR_LOGGING
extern PRLogModuleInfo* gPIPNSSLog;
#endif

NSSCleanupAutoPtrClass_WithParam(PLArenaPool, PORT_FreeArena, FalseParam, false)

// This is being stored in an uint32_t that can otherwise
// only take values from nsIX509Cert's list of cert types.
// As nsIX509Cert is frozen, we choose a value not contained
// in the list to mean not yet initialized.
#define CERT_TYPE_NOT_YET_INITIALIZED (1 << 30)

NS_IMPL_ISUPPORTS(nsNSSCertificate,
                  nsIX509Cert,
                  nsIIdentityInfo,
                  nsISerializable,
                  nsIClassInfo)

/*static*/ nsNSSCertificate*
nsNSSCertificate::Create(CERTCertificate* cert, SECOidTag* evOidPolicy)
{
  if (GeckoProcessType_Default != XRE_GetProcessType()) {
    NS_ERROR("Trying to initialize nsNSSCertificate in a non-chrome process!");
    return nullptr;
  }
  if (cert)
    return new nsNSSCertificate(cert, evOidPolicy);
  else
    return new nsNSSCertificate();
}

nsNSSCertificate*
nsNSSCertificate::ConstructFromDER(char* certDER, int derLen)
{
  // On non-chrome process prevent instantiation
  if (GeckoProcessType_Default != XRE_GetProcessType())
    return nullptr;

  nsNSSCertificate* newObject = nsNSSCertificate::Create();
  if (newObject && !newObject->InitFromDER(certDER, derLen)) {
    delete newObject;
    newObject = nullptr;
  }

  return newObject;
}

bool
nsNSSCertificate::InitFromDER(char* certDER, int derLen)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return false;

  if (!certDER || !derLen)
    return false;

  CERTCertificate* aCert = CERT_DecodeCertFromPackage(certDER, derLen);

  if (!aCert)
    return false;

  if (!aCert->dbhandle)
  {
    aCert->dbhandle = CERT_GetDefaultCertDB();
  }

  mCert = aCert;
  return true;
}

nsNSSCertificate::nsNSSCertificate(CERTCertificate* cert,
                                   SECOidTag* evOidPolicy)
  : mCert(nullptr)
  , mPermDelete(false)
  , mCertType(CERT_TYPE_NOT_YET_INITIALIZED)
  , mCachedEVStatus(ev_status_unknown)
{
#if defined(DEBUG)
  if (GeckoProcessType_Default != XRE_GetProcessType())
    NS_ERROR("Trying to initialize nsNSSCertificate in a non-chrome process!");
#endif

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return;

  if (cert) {
    mCert = CERT_DupCertificate(cert);
    if (evOidPolicy) {
      if (*evOidPolicy == SEC_OID_UNKNOWN) {
        mCachedEVStatus =  ev_status_invalid;
      }
      else {
        mCachedEVStatus = ev_status_valid;
      }
      mCachedEVOidTag = *evOidPolicy;
    }
  }
}

nsNSSCertificate::nsNSSCertificate() :
  mCert(nullptr),
  mPermDelete(false),
  mCertType(CERT_TYPE_NOT_YET_INITIALIZED),
  mCachedEVStatus(ev_status_unknown)
{
  if (GeckoProcessType_Default != XRE_GetProcessType())
    NS_ERROR("Trying to initialize nsNSSCertificate in a non-chrome process!");
}

nsNSSCertificate::~nsNSSCertificate()
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return;
  }
  destructorSafeDestroyNSSReference();
  shutdown(calledFromObject);
}

void nsNSSCertificate::virtualDestroyNSSReference()
{
  destructorSafeDestroyNSSReference();
}

void nsNSSCertificate::destructorSafeDestroyNSSReference()
{
  if (mPermDelete) {
    if (mCertType == nsNSSCertificate::USER_CERT) {
      nsCOMPtr<nsIInterfaceRequestor> cxt = new PipUIContext();
      PK11_DeleteTokenCertAndKey(mCert.get(), cxt);
    } else if (!PK11_IsReadOnly(mCert->slot)) {
      // If the list of built-ins does contain a non-removable
      // copy of this certificate, our call will not remove
      // the certificate permanently, but rather remove all trust.
      SEC_DeletePermCertificate(mCert.get());
    }
  }

  mCert = nullptr;
}

nsresult
nsNSSCertificate::GetCertType(uint32_t* aCertType)
{
  if (mCertType == CERT_TYPE_NOT_YET_INITIALIZED) {
     // only determine cert type once and cache it
     mCertType = getCertType(mCert.get());
  }
  *aCertType = mCertType;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetIsSelfSigned(bool* aIsSelfSigned)
{
  NS_ENSURE_ARG(aIsSelfSigned);

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  *aIsSelfSigned = mCert->isRoot;
  return NS_OK;
}

nsresult
nsNSSCertificate::MarkForPermDeletion()
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  // make sure user is logged in to the token
  nsCOMPtr<nsIInterfaceRequestor> ctx = new PipUIContext();

  if (PK11_NeedLogin(mCert->slot)
      && !PK11_NeedUserInit(mCert->slot)
      && !PK11_IsInternal(mCert->slot))
  {
    if (SECSuccess != PK11_Authenticate(mCert->slot, true, ctx))
    {
      return NS_ERROR_FAILURE;
    }
  }

  mPermDelete = true;
  return NS_OK;
}

nsresult
GetKeyUsagesString(CERTCertificate* cert, nsINSSComponent* nssComponent,
                   nsString& text)
{
  text.Truncate();

  SECItem keyUsageItem;
  keyUsageItem.data = nullptr;
  keyUsageItem.len = 0;

  SECStatus srv;

  // There is no extension, v1 or v2 certificate
  if (!cert->extensions)
    return NS_OK;


  srv = CERT_FindKeyUsageExtension(cert, &keyUsageItem);
  if (srv == SECFailure) {
    if (PORT_GetError () == SEC_ERROR_EXTENSION_NOT_FOUND)
      return NS_OK;
    else
      return NS_ERROR_FAILURE;
  }
  unsigned char keyUsage = 0;
  if (keyUsageItem.len) {
    keyUsage = keyUsageItem.data[0];
  }

  nsAutoString local;
  nsresult rv;
  const char16_t comma = ',';

  if (keyUsage & KU_DIGITAL_SIGNATURE) {
    rv = nssComponent->GetPIPNSSBundleString("CertDumpKUSign", local);
    if (NS_SUCCEEDED(rv)) {
      if (!text.IsEmpty()) text.Append(comma);
      text.Append(local.get());
    }
  }
  if (keyUsage & KU_NON_REPUDIATION) {
    rv = nssComponent->GetPIPNSSBundleString("CertDumpKUNonRep", local);
    if (NS_SUCCEEDED(rv)) {
      if (!text.IsEmpty()) text.Append(comma);
      text.Append(local.get());
    }
  }
  if (keyUsage & KU_KEY_ENCIPHERMENT) {
    rv = nssComponent->GetPIPNSSBundleString("CertDumpKUEnc", local);
    if (NS_SUCCEEDED(rv)) {
      if (!text.IsEmpty()) text.Append(comma);
      text.Append(local.get());
    }
  }
  if (keyUsage & KU_DATA_ENCIPHERMENT) {
    rv = nssComponent->GetPIPNSSBundleString("CertDumpKUDEnc", local);
    if (NS_SUCCEEDED(rv)) {
      if (!text.IsEmpty()) text.Append(comma);
      text.Append(local.get());
    }
  }
  if (keyUsage & KU_KEY_AGREEMENT) {
    rv = nssComponent->GetPIPNSSBundleString("CertDumpKUKA", local);
    if (NS_SUCCEEDED(rv)) {
      if (!text.IsEmpty()) text.Append(comma);
      text.Append(local.get());
    }
  }
  if (keyUsage & KU_KEY_CERT_SIGN) {
    rv = nssComponent->GetPIPNSSBundleString("CertDumpKUCertSign", local);
    if (NS_SUCCEEDED(rv)) {
      if (!text.IsEmpty()) text.Append(comma);
      text.Append(local.get());
    }
  }
  if (keyUsage & KU_CRL_SIGN) {
    rv = nssComponent->GetPIPNSSBundleString("CertDumpKUCRLSign", local);
    if (NS_SUCCEEDED(rv)) {
      if (!text.IsEmpty()) text.Append(comma);
      text.Append(local.get());
    }
  }

  PORT_Free (keyUsageItem.data);
  return NS_OK;
}

nsresult
nsNSSCertificate::FormatUIStrings(const nsAutoString& nickname,
                                  nsAutoString& nickWithSerial,
                                  nsAutoString& details)
{
  static NS_DEFINE_CID(kNSSComponentCID, NS_NSSCOMPONENT_CID);

  if (!NS_IsMainThread()) {
    NS_ERROR("nsNSSCertificate::FormatUIStrings called off the main thread");
    return NS_ERROR_NOT_SAME_THREAD;
  }

  nsresult rv = NS_OK;

  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));

  if (NS_FAILED(rv) || !nssComponent) {
    return NS_ERROR_FAILURE;
  }

  nsAutoString info;
  nsAutoString temp1;

  nickWithSerial.Append(nickname);

  if (NS_SUCCEEDED(nssComponent->GetPIPNSSBundleString("CertInfoIssuedFor", info))) {
    details.Append(info);
    details.Append(char16_t(' '));
    if (NS_SUCCEEDED(GetSubjectName(temp1)) && !temp1.IsEmpty()) {
      details.Append(temp1);
    }
    details.Append(char16_t('\n'));
  }

  if (NS_SUCCEEDED(GetSerialNumber(temp1)) && !temp1.IsEmpty()) {
    details.AppendLiteral("  ");
    if (NS_SUCCEEDED(nssComponent->GetPIPNSSBundleString("CertDumpSerialNo", info))) {
      details.Append(info);
      details.AppendLiteral(": ");
    }
    details.Append(temp1);

    nickWithSerial.AppendLiteral(" [");
    nickWithSerial.Append(temp1);
    nickWithSerial.Append(char16_t(']'));

    details.Append(char16_t('\n'));
  }

  nsCOMPtr<nsIX509CertValidity> validity;
  rv = GetValidity(getter_AddRefs(validity));
  if (NS_SUCCEEDED(rv) && validity) {
    details.AppendLiteral("  ");
    if (NS_SUCCEEDED(nssComponent->GetPIPNSSBundleString("CertInfoValid", info))) {
      details.Append(info);
    }

    if (NS_SUCCEEDED(validity->GetNotBeforeLocalTime(temp1)) && !temp1.IsEmpty()) {
      details.Append(char16_t(' '));
      if (NS_SUCCEEDED(nssComponent->GetPIPNSSBundleString("CertInfoFrom", info))) {
        details.Append(info);
        details.Append(char16_t(' '));
      }
      details.Append(temp1);
    }

    if (NS_SUCCEEDED(validity->GetNotAfterLocalTime(temp1)) && !temp1.IsEmpty()) {
      details.Append(char16_t(' '));
      if (NS_SUCCEEDED(nssComponent->GetPIPNSSBundleString("CertInfoTo", info))) {
        details.Append(info);
        details.Append(char16_t(' '));
      }
      details.Append(temp1);
    }

    details.Append(char16_t('\n'));
  }

  if (NS_SUCCEEDED(GetKeyUsagesString(mCert.get(), nssComponent, temp1)) &&
      !temp1.IsEmpty()) {
    details.AppendLiteral("  ");
    if (NS_SUCCEEDED(nssComponent->GetPIPNSSBundleString("CertDumpKeyUsage", info))) {
      details.Append(info);
      details.AppendLiteral(": ");
    }
    details.Append(temp1);
    details.Append(char16_t('\n'));
  }

  nsAutoString firstEmail;
  const char* aWalkAddr;
  for (aWalkAddr = CERT_GetFirstEmailAddress(mCert.get())
        ;
        aWalkAddr
        ;
        aWalkAddr = CERT_GetNextEmailAddress(mCert.get(), aWalkAddr))
  {
    NS_ConvertUTF8toUTF16 email(aWalkAddr);
    if (email.IsEmpty())
      continue;

    if (firstEmail.IsEmpty()) {
      // If the first email address from the subject DN is also present
      // in the subjectAltName extension, GetEmailAddresses() will return
      // it twice (as received from NSS). Remember the first address so that
      // we can filter out duplicates later on.
      firstEmail = email;

      details.AppendLiteral("  ");
      if (NS_SUCCEEDED(nssComponent->GetPIPNSSBundleString("CertInfoEmail", info))) {
        details.Append(info);
        details.AppendLiteral(": ");
      }
      details.Append(email);
    }
    else {
      // Append current address if it's different from the first one.
      if (!firstEmail.Equals(email)) {
        details.AppendLiteral(", ");
        details.Append(email);
      }
    }
  }

  if (!firstEmail.IsEmpty()) {
    // We got at least one email address, so we want a newline
    details.Append(char16_t('\n'));
  }

  if (NS_SUCCEEDED(nssComponent->GetPIPNSSBundleString("CertInfoIssuedBy", info))) {
    details.Append(info);
    details.Append(char16_t(' '));

    if (NS_SUCCEEDED(GetIssuerName(temp1)) && !temp1.IsEmpty()) {
      details.Append(temp1);
    }

    details.Append(char16_t('\n'));
  }

  if (NS_SUCCEEDED(nssComponent->GetPIPNSSBundleString("CertInfoStoredIn", info))) {
    details.Append(info);
    details.Append(char16_t(' '));

    if (NS_SUCCEEDED(GetTokenName(temp1)) && !temp1.IsEmpty()) {
      details.Append(temp1);
    }
  }

  // the above produces the following output:
  //
  //   Issued to: $subjectName
  //   Serial number: $serialNumber
  //   Valid from: $starting_date to $expiration_date
  //   Certificate Key usage: $usages
  //   Email: $address(es)
  //   Issued by: $issuerName
  //   Stored in: $token

  return rv;
}

NS_IMETHODIMP
nsNSSCertificate::GetDbKey(char** aDbKey)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  SECItem key;

  NS_ENSURE_ARG(aDbKey);
  *aDbKey = nullptr;
  key.len = NS_NSS_LONG*4+mCert->serialNumber.len+mCert->derIssuer.len;
  key.data = (unsigned char*) nsMemory::Alloc(key.len);
  if (!key.data)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_NSS_PUT_LONG(0,key.data); // later put moduleID
  NS_NSS_PUT_LONG(0,&key.data[NS_NSS_LONG]); // later put slotID
  NS_NSS_PUT_LONG(mCert->serialNumber.len,&key.data[NS_NSS_LONG*2]);
  NS_NSS_PUT_LONG(mCert->derIssuer.len,&key.data[NS_NSS_LONG*3]);
  memcpy(&key.data[NS_NSS_LONG*4], mCert->serialNumber.data,
         mCert->serialNumber.len);
  memcpy(&key.data[NS_NSS_LONG*4+mCert->serialNumber.len],
         mCert->derIssuer.data, mCert->derIssuer.len);

  *aDbKey = NSSBase64_EncodeItem(nullptr, nullptr, 0, &key);
  nsMemory::Free(key.data); // SECItem is a 'c' type without a destrutor
  return (*aDbKey) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsNSSCertificate::GetWindowTitle(nsAString& aWindowTitle)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  aWindowTitle.Truncate();

  if (!mCert) {
    NS_ERROR("Somehow got nullptr for mCert in nsNSSCertificate.");
    return NS_ERROR_FAILURE;
  }

  mozilla::pkix::ScopedPtr<char, mozilla::psm::PORT_Free_string>
    commonName(CERT_GetCommonName(&mCert->subject));

  const char* titleOptions[] = {
    mCert->nickname,
    commonName.get(),
    mCert->subjectName,
    mCert->emailAddr
  };

  nsAutoCString titleOption;
  for (size_t i = 0; i < ArrayLength(titleOptions); i++) {
    titleOption = titleOptions[i];
    if (titleOption.Length() > 0 && IsUTF8(titleOption)) {
      CopyUTF8toUTF16(titleOption, aWindowTitle);
      return NS_OK;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetNickname(nsAString& aNickname)
{
  static NS_DEFINE_CID(kNSSComponentCID, NS_NSSCOMPONENT_CID);

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  if (mCert->nickname) {
    CopyUTF8toUTF16(mCert->nickname, aNickname);
  } else {
    nsresult rv;
    nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));
    if (NS_FAILED(rv) || !nssComponent) {
      return NS_ERROR_FAILURE;
    }
    nssComponent->GetPIPNSSBundleString("CertNoNickname", aNickname);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetEmailAddress(nsAString& aEmailAddress)
{
  static NS_DEFINE_CID(kNSSComponentCID, NS_NSSCOMPONENT_CID);

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  if (mCert->emailAddr) {
    CopyUTF8toUTF16(mCert->emailAddr, aEmailAddress);
  } else {
    nsresult rv;
    nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));
    if (NS_FAILED(rv) || !nssComponent) {
      return NS_ERROR_FAILURE;
    }
    nssComponent->GetPIPNSSBundleString("CertNoEmailAddress", aEmailAddress);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetEmailAddresses(uint32_t* aLength, char16_t*** aAddresses)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  NS_ENSURE_ARG(aLength);
  NS_ENSURE_ARG(aAddresses);

  *aLength = 0;

  const char* aAddr;
  for (aAddr = CERT_GetFirstEmailAddress(mCert.get())
       ;
       aAddr
       ;
       aAddr = CERT_GetNextEmailAddress(mCert.get(), aAddr))
  {
    ++(*aLength);
  }

  *aAddresses = (char16_t**) nsMemory::Alloc(sizeof(char16_t*) * (*aLength));
  if (!*aAddresses)
    return NS_ERROR_OUT_OF_MEMORY;

  uint32_t iAddr;
  for (aAddr = CERT_GetFirstEmailAddress(mCert.get()), iAddr = 0
       ;
       aAddr
       ;
       aAddr = CERT_GetNextEmailAddress(mCert.get(), aAddr), ++iAddr)
  {
    (*aAddresses)[iAddr] = ToNewUnicode(NS_ConvertUTF8toUTF16(aAddr));
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::ContainsEmailAddress(const nsAString& aEmailAddress,
                                       bool* result)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  NS_ENSURE_ARG(result);
  *result = false;

  const char* aAddr = nullptr;
  for (aAddr = CERT_GetFirstEmailAddress(mCert.get())
       ;
       aAddr
       ;
       aAddr = CERT_GetNextEmailAddress(mCert.get(), aAddr))
  {
    NS_ConvertUTF8toUTF16 certAddr(aAddr);
    ToLowerCase(certAddr);

    nsAutoString testAddr(aEmailAddress);
    ToLowerCase(testAddr);

    if (certAddr == testAddr)
    {
      *result = true;
      break;
    }

  }

  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetCommonName(nsAString& aCommonName)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  aCommonName.Truncate();
  if (mCert) {
    char* commonName = CERT_GetCommonName(&mCert->subject);
    if (commonName) {
      aCommonName = NS_ConvertUTF8toUTF16(commonName);
      PORT_Free(commonName);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetOrganization(nsAString& aOrganization)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  aOrganization.Truncate();
  if (mCert) {
    char* organization = CERT_GetOrgName(&mCert->subject);
    if (organization) {
      aOrganization = NS_ConvertUTF8toUTF16(organization);
      PORT_Free(organization);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetIssuerCommonName(nsAString& aCommonName)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  aCommonName.Truncate();
  if (mCert) {
    char* commonName = CERT_GetCommonName(&mCert->issuer);
    if (commonName) {
      aCommonName = NS_ConvertUTF8toUTF16(commonName);
      PORT_Free(commonName);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetIssuerOrganization(nsAString& aOrganization)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  aOrganization.Truncate();
  if (mCert) {
    char* organization = CERT_GetOrgName(&mCert->issuer);
    if (organization) {
      aOrganization = NS_ConvertUTF8toUTF16(organization);
      PORT_Free(organization);
    } else {
      return GetIssuerCommonName(aOrganization);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetIssuerOrganizationUnit(nsAString& aOrganizationUnit)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  aOrganizationUnit.Truncate();
  if (mCert) {
    char* organizationUnit = CERT_GetOrgUnitName(&mCert->issuer);
    if (organizationUnit) {
      aOrganizationUnit = NS_ConvertUTF8toUTF16(organizationUnit);
      PORT_Free(organizationUnit);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetIssuer(nsIX509Cert** aIssuer)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  NS_ENSURE_ARG(aIssuer);
  *aIssuer = nullptr;

  nsCOMPtr<nsIArray> chain;
  nsresult rv;
  rv = GetChain(getter_AddRefs(chain));
  NS_ENSURE_SUCCESS(rv, rv);
  uint32_t length;
  if (!chain || NS_FAILED(chain->GetLength(&length)) || length == 0) {
    return NS_ERROR_UNEXPECTED;
  }
  if (length == 1) { // No known issuer
    return NS_OK;
  }
  nsCOMPtr<nsIX509Cert> cert;
  chain->QueryElementAt(1, NS_GET_IID(nsIX509Cert), getter_AddRefs(cert));
  if (!cert) {
    return NS_ERROR_UNEXPECTED;
  }
  *aIssuer = cert;
  NS_ADDREF(*aIssuer);
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetOrganizationalUnit(nsAString& aOrganizationalUnit)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  aOrganizationalUnit.Truncate();
  if (mCert) {
    char* orgunit = CERT_GetOrgUnitName(&mCert->subject);
    if (orgunit) {
      aOrganizationalUnit = NS_ConvertUTF8toUTF16(orgunit);
      PORT_Free(orgunit);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetChain(nsIArray** _rvChain)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  NS_ENSURE_ARG(_rvChain);
  nsresult rv;
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("Getting chain for \"%s\"\n", mCert->nickname));

  mozilla::pkix::Time now(mozilla::pkix::Now());

  ScopedCERTCertList nssChain;
  RefPtr<SharedCertVerifier> certVerifier(GetDefaultCertVerifier());
  NS_ENSURE_TRUE(certVerifier, NS_ERROR_UNEXPECTED);

  // We want to test all usages, but we start with server because most of the
  // time Firefox users care about server certs.
  if (certVerifier->VerifyCert(mCert.get(), certificateUsageSSLServer, now,
                               nullptr, /*XXX fixme*/
                               nullptr, /* hostname */
                               CertVerifier::FLAG_LOCAL_ONLY,
                               nullptr, /* stapledOCSPResponse */
                               &nssChain) != SECSuccess) {
    nssChain = nullptr;
    // keep going
  }

  // This is the whitelist of all non-SSLServer usages that are supported by
  // verifycert.
  const int otherUsagesToTest = certificateUsageSSLClient |
                                certificateUsageSSLCA |
                                certificateUsageEmailSigner |
                                certificateUsageEmailRecipient |
                                certificateUsageObjectSigner |
                                certificateUsageStatusResponder;
  for (int usage = certificateUsageSSLClient;
       usage < certificateUsageAnyCA && !nssChain;
       usage = usage << 1) {
    if ((usage & otherUsagesToTest) == 0) {
      continue;
    }
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG,
           ("pipnss: PKIX attempting chain(%d) for '%s'\n",
            usage, mCert->nickname));
    if (certVerifier->VerifyCert(mCert.get(), usage, now,
                                 nullptr, /*XXX fixme*/
                                 nullptr, /*hostname*/
                                 CertVerifier::FLAG_LOCAL_ONLY,
                                 nullptr, /* stapledOCSPResponse */
                                 &nssChain) != SECSuccess) {
      nssChain = nullptr;
      // keep going
    }
  }

  if (!nssChain) {
    // There is not verified path for the chain, howeever we still want to 
    // present to the user as much of a possible chain as possible, in the case
    // where there was a problem with the cert or the issuers.
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG,
           ("pipnss: getchain :CertVerify failed to get chain for '%s'\n",
            mCert->nickname));
    nssChain = CERT_GetCertChainFromCert(mCert.get(), PR_Now(),
                                         certUsageSSLClient);
  } 

  if (!nssChain) {
    return NS_ERROR_FAILURE;
  }

  // enumerate the chain for scripting purposes
  nsCOMPtr<nsIMutableArray> array =
    do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    goto done;
  }
  CERTCertListNode* node;
  for (node = CERT_LIST_HEAD(nssChain.get());
       !CERT_LIST_END(node, nssChain.get());
       node = CERT_LIST_NEXT(node)) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG,
           ("adding %s to chain\n", node->cert->nickname));
    nsCOMPtr<nsIX509Cert> cert = nsNSSCertificate::Create(node->cert);
    array->AppendElement(cert, false);
  }
  *_rvChain = array;
  NS_IF_ADDREF(*_rvChain);
  rv = NS_OK;
done:
  return rv;
}

NS_IMETHODIMP
nsNSSCertificate::GetAllTokenNames(uint32_t* aLength, char16_t*** aTokenNames)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  NS_ENSURE_ARG(aLength);
  NS_ENSURE_ARG(aTokenNames);
  *aLength = 0;
  *aTokenNames = nullptr;

  // Get the slots from NSS
  ScopedPK11SlotList slots;
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("Getting slots for \"%s\"\n", mCert->nickname));
  slots = PK11_GetAllSlotsForCert(mCert.get(), nullptr);
  if (!slots) {
    if (PORT_GetError() == SEC_ERROR_NO_TOKEN)
      return NS_OK; // List of slots is empty, return empty array
    else
      return NS_ERROR_FAILURE;
  }

  // read the token names from slots
  PK11SlotListElement* le;

  for (le = slots->head; le; le = le->next) {
    ++(*aLength);
  }

  *aTokenNames = (char16_t**) nsMemory::Alloc(sizeof(char16_t*) * (*aLength));
  if (!*aTokenNames) {
    *aLength = 0;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  uint32_t iToken;
  for (le = slots->head, iToken = 0; le; le = le->next, ++iToken) {
    char* token = PK11_GetTokenName(le->slot);
    (*aTokenNames)[iToken] = ToNewUnicode(NS_ConvertUTF8toUTF16(token));
    if (!(*aTokenNames)[iToken]) {
      NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(iToken, *aTokenNames);
      *aLength = 0;
      *aTokenNames = nullptr;
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetSubjectName(nsAString& _subjectName)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  _subjectName.Truncate();
  if (mCert->subjectName) {
    _subjectName = NS_ConvertUTF8toUTF16(mCert->subjectName);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsNSSCertificate::GetIssuerName(nsAString& _issuerName)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  _issuerName.Truncate();
  if (mCert->issuerName) {
    _issuerName = NS_ConvertUTF8toUTF16(mCert->issuerName);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsNSSCertificate::GetSerialNumber(nsAString& _serialNumber)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  _serialNumber.Truncate();
  char* tmpstr = CERT_Hexify(&mCert->serialNumber, 1);
  if (tmpstr) {
    _serialNumber = NS_ConvertASCIItoUTF16(tmpstr);
    PORT_Free(tmpstr);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

nsresult
nsNSSCertificate::GetCertificateHash(nsAString& aFingerprint, SECOidTag aHashAlg)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  aFingerprint.Truncate();
  Digest digest;
  nsresult rv = digest.DigestBuf(aHashAlg, mCert->derCert.data,
                                 mCert->derCert.len);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // CERT_Hexify's second argument is an int that is interpreted as a boolean
  char* fpStr = CERT_Hexify(const_cast<SECItem*>(&digest.get()), 1);
  if (!fpStr) {
    return NS_ERROR_FAILURE;
  }

  aFingerprint.AssignASCII(fpStr);
  PORT_Free(fpStr);
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetSha256Fingerprint(nsAString& aSha256Fingerprint)
{
  return GetCertificateHash(aSha256Fingerprint, SEC_OID_SHA256);
}

NS_IMETHODIMP
nsNSSCertificate::GetSha1Fingerprint(nsAString& _sha1Fingerprint)
{
  return GetCertificateHash(_sha1Fingerprint, SEC_OID_SHA1);
}

NS_IMETHODIMP
nsNSSCertificate::GetTokenName(nsAString& aTokenName)
{
  static NS_DEFINE_CID(kNSSComponentCID, NS_NSSCOMPONENT_CID);

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  aTokenName.Truncate();
  if (mCert) {
    // HACK alert
    // When the trust of a builtin cert is modified, NSS copies it into the
    // cert db.  At this point, it is now "managed" by the user, and should
    // not be listed with the builtins.  However, in the collection code
    // used by PK11_ListCerts, the cert is found in the temp db, where it
    // has been loaded from the token.  Though the trust is correct (grabbed
    // from the cert db), the source is wrong.  I believe this is a safe
    // way to work around this.
    if (mCert->slot) {
      char* token = PK11_GetTokenName(mCert->slot);
      if (token) {
        aTokenName = NS_ConvertUTF8toUTF16(token);
      }
    } else {
      nsresult rv;
      nsAutoString tok;
      nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));
      if (NS_FAILED(rv)) return rv;
      rv = nssComponent->GetPIPNSSBundleString("InternalToken", tok);
      if (NS_SUCCEEDED(rv))
        aTokenName = tok;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetSha256SubjectPublicKeyInfoDigest(nsACString& aSha256SPKIDigest)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  aSha256SPKIDigest.Truncate();
  Digest digest;
  nsresult rv = digest.DigestBuf(SEC_OID_SHA256, mCert->derPublicKey.data,
                                 mCert->derPublicKey.len);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  rv = Base64Encode(nsDependentCSubstring(
                      reinterpret_cast<const char*> (digest.get().data),
                      digest.get().len),
                    aSha256SPKIDigest);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetRawDER(uint32_t* aLength, uint8_t** aArray)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  if (mCert) {
    *aArray = (uint8_t*)nsMemory::Alloc(mCert->derCert.len);
    if (*aArray) {
      memcpy(*aArray, mCert->derCert.data, mCert->derCert.len);
      *aLength = mCert->derCert.len;
      return NS_OK;
    }
  }
  *aLength = 0;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsNSSCertificate::ExportAsCMS(uint32_t chainMode,
                              uint32_t* aLength, uint8_t** aArray)
{
  NS_ENSURE_ARG(aLength);
  NS_ENSURE_ARG(aArray);

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  if (!mCert)
    return NS_ERROR_FAILURE;

  switch (chainMode) {
    case nsIX509Cert::CMS_CHAIN_MODE_CertOnly:
    case nsIX509Cert::CMS_CHAIN_MODE_CertChain:
    case nsIX509Cert::CMS_CHAIN_MODE_CertChainWithRoot:
      break;
    default:
      return NS_ERROR_INVALID_ARG;
  };

  PLArenaPool* arena = PORT_NewArena(1024);
  PLArenaPoolCleanerFalseParam arenaCleaner(arena);
  if (!arena) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG,
           ("nsNSSCertificate::ExportAsCMS - out of memory\n"));
    return NS_ERROR_OUT_OF_MEMORY;
  }

  ScopedNSSCMSMessage cmsg(NSS_CMSMessage_Create(nullptr));
  if (!cmsg) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG,
           ("nsNSSCertificate::ExportAsCMS - can't create CMS message\n"));
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // first, create SignedData with the certificate only (no chain)
  ScopedNSSCMSSignedData sigd(
    NSS_CMSSignedData_CreateCertsOnly(cmsg, mCert.get(), false));
  if (!sigd) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG,
           ("nsNSSCertificate::ExportAsCMS - can't create SignedData\n"));
    return NS_ERROR_FAILURE;
  }

  // Calling NSS_CMSSignedData_CreateCertsOnly() will not allow us
  // to specify the inclusion of the root, but CERT_CertChainFromCert() does.
  // Since CERT_CertChainFromCert() also includes the certificate itself,
  // we have to start at the issuing cert (to avoid duplicate certs
  // in the SignedData).
  if (chainMode == nsIX509Cert::CMS_CHAIN_MODE_CertChain ||
      chainMode == nsIX509Cert::CMS_CHAIN_MODE_CertChainWithRoot) {
    ScopedCERTCertificate issuerCert(
        CERT_FindCertIssuer(mCert.get(), PR_Now(), certUsageAnyCA));
    // the issuerCert of a self signed root is the cert itself,
    // so make sure we're not adding duplicates, again
    if (issuerCert && issuerCert != mCert.get()) {
      bool includeRoot =
        (chainMode == nsIX509Cert::CMS_CHAIN_MODE_CertChainWithRoot);
      ScopedCERTCertificateList certChain(
          CERT_CertChainFromCert(issuerCert, certUsageAnyCA, includeRoot));
      if (certChain) {
        if (NSS_CMSSignedData_AddCertList(sigd, certChain) == SECSuccess) {
          certChain.forget();
        }
        else {
          PR_LOG(gPIPNSSLog, PR_LOG_DEBUG,
                 ("nsNSSCertificate::ExportAsCMS - can't add chain\n"));
          return NS_ERROR_FAILURE;
        }
      }
      else {
        // try to add the issuerCert, at least
        if (NSS_CMSSignedData_AddCertificate(sigd, issuerCert)
            == SECSuccess) {
          issuerCert.forget();
        }
        else {
          PR_LOG(gPIPNSSLog, PR_LOG_DEBUG,
                 ("nsNSSCertificate::ExportAsCMS - can't add issuer cert\n"));
          return NS_ERROR_FAILURE;
        }
      }
    }
  }

  NSSCMSContentInfo* cinfo = NSS_CMSMessage_GetContentInfo(cmsg);
  if (NSS_CMSContentInfo_SetContent_SignedData(cmsg, cinfo, sigd)
       == SECSuccess) {
    sigd.forget();
  }
  else {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG,
           ("nsNSSCertificate::ExportAsCMS - can't attach SignedData\n"));
    return NS_ERROR_FAILURE;
  }

  SECItem certP7 = { siBuffer, nullptr, 0 };
  NSSCMSEncoderContext* ecx = NSS_CMSEncoder_Start(cmsg, nullptr, nullptr,
                                                   &certP7, arena, nullptr,
                                                   nullptr, nullptr, nullptr,
                                                   nullptr, nullptr);
  if (!ecx) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG,
           ("nsNSSCertificate::ExportAsCMS - can't create encoder context\n"));
    return NS_ERROR_FAILURE;
  }

  if (NSS_CMSEncoder_Finish(ecx) != SECSuccess) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG,
           ("nsNSSCertificate::ExportAsCMS - failed to add encoded data\n"));
    return NS_ERROR_FAILURE;
  }

  *aArray = (uint8_t*)nsMemory::Alloc(certP7.len);
  if (!*aArray)
    return NS_ERROR_OUT_OF_MEMORY;

  memcpy(*aArray, certP7.data, certP7.len);
  *aLength = certP7.len;
  return NS_OK;
}

CERTCertificate*
nsNSSCertificate::GetCert()
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return nullptr;

  return (mCert) ? CERT_DupCertificate(mCert.get()) : nullptr;
}

NS_IMETHODIMP
nsNSSCertificate::GetValidity(nsIX509CertValidity** aValidity)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  NS_ENSURE_ARG(aValidity);
  nsX509CertValidity* validity = new nsX509CertValidity(mCert.get());

  NS_ADDREF(validity);
  *aValidity = static_cast<nsIX509CertValidity*>(validity);
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetUsagesArray(bool localOnly,
                                 uint32_t* _verified,
                                 uint32_t* _count,
                                 char16_t*** _usages)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  nsresult rv;
  const int max_usages = 13;
  char16_t* tmpUsages[max_usages];
  const char* suffix = "";
  uint32_t tmpCount;
  nsUsageArrayHelper uah(mCert.get());
  rv = uah.GetUsagesArray(suffix, localOnly, max_usages, _verified, &tmpCount,
                          tmpUsages);
  NS_ENSURE_SUCCESS(rv,rv);
  if (tmpCount > 0) {
    *_usages = (char16_t**) nsMemory::Alloc(sizeof(char16_t*) * tmpCount);
    if (!*_usages)
      return NS_ERROR_OUT_OF_MEMORY;
    for (uint32_t i=0; i<tmpCount; i++) {
      (*_usages)[i] = tmpUsages[i];
    }
    *_count = tmpCount;
    return NS_OK;
  }
  *_usages = (char16_t**) nsMemory::Alloc(sizeof(char16_t*));
  if (!*_usages)
    return NS_ERROR_OUT_OF_MEMORY;
  *_count = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::RequestUsagesArrayAsync(
  nsICertVerificationListener* aResultListener)
{
  NS_ENSURE_TRUE(NS_IsMainThread(), NS_ERROR_NOT_SAME_THREAD);

  if (!aResultListener)
    return NS_ERROR_FAILURE;

  nsCertVerificationJob* job = new nsCertVerificationJob;

  job->mCert = this;
  job->mListener =
    new nsMainThreadPtrHolder<nsICertVerificationListener>(aResultListener);

  nsresult rv = nsCertVerificationThread::addJob(job);
  if (NS_FAILED(rv))
    delete job;

  return rv;
}

NS_IMETHODIMP
nsNSSCertificate::GetUsagesString(bool localOnly, uint32_t* _verified,
                                  nsAString& _usages)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  nsresult rv;
  const int max_usages = 13;
  char16_t* tmpUsages[max_usages];
  const char* suffix = "_p";
  uint32_t tmpCount;
  nsUsageArrayHelper uah(mCert.get());
  rv = uah.GetUsagesArray(suffix, localOnly, max_usages, _verified, &tmpCount,
                          tmpUsages);
  NS_ENSURE_SUCCESS(rv,rv);
  _usages.Truncate();
  for (uint32_t i=0; i<tmpCount; i++) {
    if (i>0) _usages.Append(',');
    _usages.Append(tmpUsages[i]);
    nsMemory::Free(tmpUsages[i]);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetASN1Structure(nsIASN1Object** aASN1Structure)
{
  NS_ENSURE_ARG_POINTER(aASN1Structure);
  return CreateASN1Struct(aASN1Structure);
}

NS_IMETHODIMP
nsNSSCertificate::Equals(nsIX509Cert* other, bool* result)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  NS_ENSURE_ARG(other);
  NS_ENSURE_ARG(result);

  ScopedCERTCertificate cert(other->GetCert());
  *result = (mCert.get() == cert.get());
  return NS_OK;
}

#ifndef MOZ_NO_EV_CERTS

nsresult
nsNSSCertificate::hasValidEVOidTag(SECOidTag& resultOidTag, bool& validEV)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  EnsureIdentityInfoLoaded();

  RefPtr<mozilla::psm::SharedCertVerifier>
    certVerifier(mozilla::psm::GetDefaultCertVerifier());
  NS_ENSURE_TRUE(certVerifier, NS_ERROR_UNEXPECTED);

  validEV = false;
  resultOidTag = SEC_OID_UNKNOWN;

  uint32_t flags = mozilla::psm::CertVerifier::FLAG_LOCAL_ONLY |
    mozilla::psm::CertVerifier::FLAG_MUST_BE_EV;
  SECStatus rv = certVerifier->VerifyCert(mCert.get(),
    certificateUsageSSLServer, mozilla::pkix::Now(),
    nullptr /* XXX pinarg */,
    nullptr /* hostname */,
    flags, nullptr /* stapledOCSPResponse */ , nullptr, &resultOidTag);

  if (rv != SECSuccess) {
    resultOidTag = SEC_OID_UNKNOWN;
  }
  if (resultOidTag != SEC_OID_UNKNOWN) {
    validEV = true;
  }
  return NS_OK;
}

nsresult
nsNSSCertificate::getValidEVOidTag(SECOidTag& resultOidTag, bool& validEV)
{
  if (mCachedEVStatus != ev_status_unknown) {
    validEV = (mCachedEVStatus == ev_status_valid);
    if (validEV) {
      resultOidTag = mCachedEVOidTag;
    }
    return NS_OK;
  }

  nsresult rv = hasValidEVOidTag(resultOidTag, validEV);
  if (NS_SUCCEEDED(rv)) {
    if (validEV) {
      mCachedEVOidTag = resultOidTag;
    }
    mCachedEVStatus = validEV ? ev_status_valid : ev_status_invalid;
  }
  return rv;
}

#endif // MOZ_NO_EV_CERTS

NS_IMETHODIMP
nsNSSCertificate::GetIsExtendedValidation(bool* aIsEV)
{
#ifdef MOZ_NO_EV_CERTS
  *aIsEV = false;
  return NS_OK;
#else
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  NS_ENSURE_ARG(aIsEV);
  *aIsEV = false;

  if (mCachedEVStatus != ev_status_unknown) {
    *aIsEV = (mCachedEVStatus == ev_status_valid);
    return NS_OK;
  }

  SECOidTag oid_tag;
  return getValidEVOidTag(oid_tag, *aIsEV);
#endif
}

NS_IMETHODIMP
nsNSSCertificate::GetValidEVPolicyOid(nsACString& outDottedOid)
{
  outDottedOid.Truncate();

#ifndef MOZ_NO_EV_CERTS
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  SECOidTag oid_tag;
  bool valid;
  nsresult rv = getValidEVOidTag(oid_tag, valid);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (valid) {
    SECOidData* oid_data = SECOID_FindOIDByTag(oid_tag);
    if (!oid_data) {
      return NS_ERROR_FAILURE;
    }

    char* oid_str = CERT_GetOidString(&oid_data->oid);
    if (!oid_str) {
      return NS_ERROR_FAILURE;
    }

    outDottedOid.Assign(oid_str);
    PR_smprintf_free(oid_str);
  }
#endif

  return NS_OK;
}

namespace mozilla {

// TODO(bug 1036065): It seems like we only construct CERTCertLists for the
// purpose of constructing nsNSSCertLists, so maybe we should change this
// function to output an nsNSSCertList instead.
SECStatus
ConstructCERTCertListFromReversedDERArray(
  const mozilla::pkix::DERArray& certArray,
  /*out*/ ScopedCERTCertList& certList)
{
  certList = CERT_NewCertList();
  if (!certList) {
    return SECFailure;
  }

  CERTCertDBHandle* certDB(CERT_GetDefaultCertDB()); // non-owning

  size_t numCerts = certArray.GetLength();
  for (size_t i = 0; i < numCerts; ++i) {
    SECItem certDER(UnsafeMapInputToSECItem(*certArray.GetDER(i)));
    ScopedCERTCertificate cert(CERT_NewTempCertificate(certDB, &certDER,
                                                       nullptr, false, true));
    if (!cert) {
      return SECFailure;
    }
    // certArray is ordered with the root first, but we want the resulting
    // certList to have the root last.
    if (CERT_AddCertToListHead(certList, cert) != SECSuccess) {
      return SECFailure;
    }
    cert.forget(); // cert is now owned by certList.
  }

  return SECSuccess;
}

} // namespace mozilla

NS_IMPL_CLASSINFO(nsNSSCertList,
                  nullptr,
                  // inferred from nsIX509Cert
                  nsIClassInfo::THREADSAFE,
                  NS_X509CERTLIST_CID)

NS_IMPL_ISUPPORTS_CI(nsNSSCertList,
                     nsIX509CertList,
                     nsISerializable)

nsNSSCertList::nsNSSCertList(ScopedCERTCertList& certList,
                             const nsNSSShutDownPreventionLock& proofOfLock)
{
  if (certList) {
    mCertList = certList.forget();
  } else {
    mCertList = CERT_NewCertList();
  }
}

nsNSSCertList::nsNSSCertList()
{
  mCertList = CERT_NewCertList();
}

nsNSSCertList::~nsNSSCertList()
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return;
  }
  destructorSafeDestroyNSSReference();
  shutdown(calledFromObject);
}

void nsNSSCertList::virtualDestroyNSSReference()
{
  destructorSafeDestroyNSSReference();
}

void nsNSSCertList::destructorSafeDestroyNSSReference()
{
  if (mCertList) {
    mCertList = nullptr;
  }
}

NS_IMETHODIMP
nsNSSCertList::AddCert(nsIX509Cert* aCert)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  CERTCertificate* cert = aCert->GetCert();
  if (!cert) {
    NS_ERROR("Somehow got nullptr for mCertificate in nsNSSCertificate.");
    return NS_ERROR_FAILURE;
  }

  if (!mCertList) {
    NS_ERROR("Somehow got nullptr for mCertList in nsNSSCertList.");
    return NS_ERROR_FAILURE;
  }
  // XXX: check return value!
  CERT_AddCertToListTail(mCertList.get(), cert);
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertList::DeleteCert(nsIX509Cert* aCert)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  CERTCertificate* cert = aCert->GetCert();
  CERTCertListNode* node;

  if (!cert) {
    NS_ERROR("Somehow got nullptr for mCertificate in nsNSSCertificate.");
    return NS_ERROR_FAILURE;
  }

  if (!mCertList) {
    NS_ERROR("Somehow got nullptr for mCertList in nsNSSCertList.");
    return NS_ERROR_FAILURE;
  }

  for (node = CERT_LIST_HEAD(mCertList.get());
       !CERT_LIST_END(node, mCertList.get()); node = CERT_LIST_NEXT(node)) {
    if (node->cert == cert) {
	CERT_RemoveCertListNode(node);
        return NS_OK;
    }
  }
  return NS_OK; // XXX Should we fail if we couldn't find it?
}

CERTCertList*
nsNSSCertList::DupCertList(CERTCertList* aCertList,
                           const nsNSSShutDownPreventionLock& /*proofOfLock*/)
{
  if (!aCertList) {
    return nullptr;
  }

  CERTCertList* newList = CERT_NewCertList();

  if (!newList) {
    return nullptr;
  }

  CERTCertListNode* node;
  for (node = CERT_LIST_HEAD(aCertList); !CERT_LIST_END(node, aCertList);
                                              node = CERT_LIST_NEXT(node)) {
    CERTCertificate* cert = CERT_DupCertificate(node->cert);
    CERT_AddCertToListTail(newList, cert);
  }
  return newList;
}

void*
nsNSSCertList::GetRawCertList()
{
  // This function should only be called after adquiring a
  // nsNSSShutDownPreventionLock
  return mCertList.get();
}

NS_IMETHODIMP
nsNSSCertList::Write(nsIObjectOutputStream* aStream)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  NS_ENSURE_STATE(mCertList);
  nsresult rv = NS_OK;

  // First, enumerate the certs to get the length of the list
  uint32_t certListLen = 0;
  CERTCertListNode* node = nullptr;
  for (node = CERT_LIST_HEAD(mCertList);
       !CERT_LIST_END(node, mCertList);
       node = CERT_LIST_NEXT(node), ++certListLen) {
  }

  // Write the length of the list
  rv = aStream->Write32(certListLen);

  // Repeat the loop, and serialize each certificate
  node = nullptr;
  for (node = CERT_LIST_HEAD(mCertList);
       !CERT_LIST_END(node, mCertList);
       node = CERT_LIST_NEXT(node))
  {
    nsCOMPtr<nsIX509Cert> cert = nsNSSCertificate::Create(node->cert);
    if (!cert) {
      rv = NS_ERROR_OUT_OF_MEMORY;
      break;
    }

    nsCOMPtr<nsISerializable> serializableCert = do_QueryInterface(cert);
    rv = aStream->WriteCompoundObject(serializableCert, NS_GET_IID(nsIX509Cert), true);
    if (NS_FAILED(rv)) {
      break;
    }
  }

  return rv;
}

NS_IMETHODIMP
nsNSSCertList::Read(nsIObjectInputStream* aStream)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  NS_ENSURE_STATE(mCertList);
  nsresult rv = NS_OK;

  uint32_t certListLen;
  rv = aStream->Read32(&certListLen);
  if (NS_FAILED(rv)) {
    return rv;
  }

  for(uint32_t i = 0; i < certListLen; ++i) {
    nsCOMPtr<nsISupports> certSupports;
    rv = aStream->ReadObject(true, getter_AddRefs(certSupports));
    if (NS_FAILED(rv)) {
      break;
    }

    nsCOMPtr<nsIX509Cert> cert = do_QueryInterface(certSupports);
    rv = AddCert(cert);
    if (NS_FAILED(rv)) {
      break;
    }
  }

  return rv;
}

NS_IMETHODIMP
nsNSSCertList::GetEnumerator(nsISimpleEnumerator** _retval)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  nsCOMPtr<nsISimpleEnumerator> enumerator =
    new nsNSSCertListEnumerator(mCertList.get(), locker);

  *_retval = enumerator;
  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertList::Equals(nsIX509CertList* other, bool* result)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  NS_ENSURE_ARG(result);
  *result = true;

  nsresult rv;

  nsCOMPtr<nsISimpleEnumerator> selfEnumerator;
  rv = GetEnumerator(getter_AddRefs(selfEnumerator));
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsISimpleEnumerator> otherEnumerator;
  rv = other->GetEnumerator(getter_AddRefs(otherEnumerator));
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsISupports> selfSupports;
  nsCOMPtr<nsISupports> otherSupports;
  while (NS_SUCCEEDED(selfEnumerator->GetNext(getter_AddRefs(selfSupports)))) {
    if (NS_SUCCEEDED(otherEnumerator->GetNext(getter_AddRefs(otherSupports)))) {
      nsCOMPtr<nsIX509Cert> selfCert = do_QueryInterface(selfSupports);
      nsCOMPtr<nsIX509Cert> otherCert = do_QueryInterface(otherSupports);

      bool certsEqual = false;
      rv = selfCert->Equals(otherCert, &certsEqual);
      if (NS_FAILED(rv)) {
        return rv;
      }
      if (!certsEqual) {
        *result = false;
        break;
      }
    } else {
      // other is shorter than self
      *result = false;
      break;
    }
  }

  // Make sure self is the same length as other
  bool otherHasMore = false;
  rv = otherEnumerator->HasMoreElements(&otherHasMore);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (otherHasMore) {
    *result = false;
  }

  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsNSSCertListEnumerator, nsISimpleEnumerator)

nsNSSCertListEnumerator::nsNSSCertListEnumerator(
  CERTCertList* certList, const nsNSSShutDownPreventionLock& proofOfLock)
{
  mCertList = nsNSSCertList::DupCertList(certList, proofOfLock);
}

nsNSSCertListEnumerator::~nsNSSCertListEnumerator()
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return;
  }
  destructorSafeDestroyNSSReference();
  shutdown(calledFromObject);
}

void nsNSSCertListEnumerator::virtualDestroyNSSReference()
{
  destructorSafeDestroyNSSReference();
}

void nsNSSCertListEnumerator::destructorSafeDestroyNSSReference()
{
  if (mCertList) {
    mCertList = nullptr;
  }
}

NS_IMETHODIMP
nsNSSCertListEnumerator::HasMoreElements(bool* _retval)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  NS_ENSURE_TRUE(mCertList, NS_ERROR_FAILURE);

  *_retval = !CERT_LIST_EMPTY(mCertList);
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertListEnumerator::GetNext(nsISupports** _retval)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  NS_ENSURE_TRUE(mCertList, NS_ERROR_FAILURE);

  CERTCertListNode* node = CERT_LIST_HEAD(mCertList);
  if (CERT_LIST_END(node, mCertList)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIX509Cert> nssCert = nsNSSCertificate::Create(node->cert);
  if (!nssCert) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  *_retval = nssCert;
  NS_ADDREF(*_retval);

  CERT_RemoveCertListNode(node);
  return NS_OK;
}

// NB: This serialization must match that of nsNSSCertificateFakeTransport.
NS_IMETHODIMP
nsNSSCertificate::Write(nsIObjectOutputStream* aStream)
{
  NS_ENSURE_STATE(mCert);
  nsresult rv = aStream->Write32(static_cast<uint32_t>(mCachedEVStatus));
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = aStream->Write32(mCert->derCert.len);
  if (NS_FAILED(rv)) {
    return rv;
  }
  return aStream->WriteByteArray(mCert->derCert.data, mCert->derCert.len);
}

NS_IMETHODIMP
nsNSSCertificate::Read(nsIObjectInputStream* aStream)
{
  NS_ENSURE_STATE(!mCert);

  uint32_t cachedEVStatus;
  nsresult rv = aStream->Read32(&cachedEVStatus);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (cachedEVStatus == static_cast<uint32_t>(ev_status_unknown)) {
    mCachedEVStatus = ev_status_unknown;
  } else if (cachedEVStatus == static_cast<uint32_t>(ev_status_valid)) {
    mCachedEVStatus = ev_status_valid;
  } else if (cachedEVStatus == static_cast<uint32_t>(ev_status_invalid)) {
    mCachedEVStatus = ev_status_invalid;
  } else {
    return NS_ERROR_UNEXPECTED;
  }

  uint32_t len;
  rv = aStream->Read32(&len);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsXPIDLCString str;
  rv = aStream->ReadBytes(len, getter_Copies(str));
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (!InitFromDER(const_cast<char*>(str.get()), len)) {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetInterfaces(uint32_t* count, nsIID*** array)
{
  *count = 0;
  *array = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetHelperForLanguage(uint32_t language,
                                       nsISupports** _retval)
{
  *_retval = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetContractID(char** aContractID)
{
  *aContractID = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetClassDescription(char** aClassDescription)
{
  *aClassDescription = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetClassID(nsCID** aClassID)
{
  *aClassID = (nsCID*) nsMemory::Alloc(sizeof(nsCID));
  if (!*aClassID)
    return NS_ERROR_OUT_OF_MEMORY;
  return GetClassIDNoAlloc(*aClassID);
}

NS_IMETHODIMP
nsNSSCertificate::GetImplementationLanguage(uint32_t* aImplementationLanguage)
{
  *aImplementationLanguage = nsIProgrammingLanguage::CPLUSPLUS;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetFlags(uint32_t* aFlags)
{
  *aFlags = nsIClassInfo::THREADSAFE;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetClassIDNoAlloc(nsCID* aClassIDNoAlloc)
{
  static NS_DEFINE_CID(kNSSCertificateCID, NS_X509CERT_CID);

  *aClassIDNoAlloc = kNSSCertificateCID;
  return NS_OK;
}
