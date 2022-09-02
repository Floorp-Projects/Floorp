/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNSSCertificate.h"

#include "CertVerifier.h"
#include "ExtendedValidation.h"
#include "NSSCertDBTrustDomain.h"
#include "X509CertValidity.h"
#include "certdb.h"
#include "ipc/IPCMessageUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/Base64.h"
#include "mozilla/Casting.h"
#include "mozilla/NotNull.h"
#include "mozilla/Span.h"
#include "mozilla/TextUtils.h"
#include "mozilla/Unused.h"
#include "mozilla/ipc/TransportSecurityInfoUtils.h"
#include "mozilla/net/DNS.h"
#include "mozpkix/Result.h"
#include "mozpkix/pkixnss.h"
#include "mozpkix/pkixtypes.h"
#include "mozpkix/pkixutil.h"
#include "nsArray.h"
#include "nsCOMPtr.h"
#include "nsIClassInfoImpl.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsIX509Cert.h"
#include "nsNSSCertHelper.h"
#include "nsNSSCertTrust.h"
#include "nsPK11TokenDB.h"
#include "nsPKCS12Blob.h"
#include "nsProxyRelease.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "nsUnicharUtils.h"
#include "nspr.h"
#include "prerror.h"
#include "secasn1.h"
#include "secder.h"
#include "secerr.h"
#include "ssl.h"

#ifdef XP_WIN
#  include <winsock.h>  // for htonl
#endif

using namespace mozilla;
using namespace mozilla::psm;

extern LazyLogModule gPIPNSSLog;

NS_IMPL_ISUPPORTS(nsNSSCertificate, nsIX509Cert, nsISerializable, nsIClassInfo)

nsNSSCertificate::nsNSSCertificate() : mCert("nsNSSCertificate::mCert") {}

nsNSSCertificate::nsNSSCertificate(CERTCertificate* cert)
    : mCert("nsNSSCertificate::mCert") {
  if (cert) {
    mDER.AppendElements(cert->derCert.data, cert->derCert.len);
    auto lock = mCert.Lock();
    auto& maybeCert = lock.ref();
    maybeCert.emplace(UniqueCERTCertificate(CERT_DupCertificate(cert)));
  }
}

nsNSSCertificate::nsNSSCertificate(nsTArray<uint8_t>&& der)
    : mDER(std::move(der)), mCert("nsNSSCertificate::mCert") {}

UniqueCERTCertificate nsNSSCertificate::GetOrInstantiateCert() {
  auto lock = mCert.Lock();
  auto& maybeCert = lock.ref();
  if (maybeCert.isSome()) {
    return UniqueCERTCertificate(CERT_DupCertificate((*maybeCert).get()));
  }

  if (!EnsureNSSInitializedChromeOrContent()) {
    return nullptr;
  }

  SECItem derItem = {siBuffer, mDER.Elements(),
                     static_cast<unsigned int>(mDER.Length())};
  UniqueCERTCertificate cert(CERT_NewTempCertificate(
      CERT_GetDefaultCertDB(), &derItem, nullptr, false, true));
  if (!cert) {
    return nullptr;
  }
  maybeCert.emplace(std::move(cert));

  return UniqueCERTCertificate(CERT_DupCertificate((*maybeCert).get()));
}

nsresult nsNSSCertificate::GetCertType(uint32_t* aCertType) {
  UniqueCERTCertificate cert(GetOrInstantiateCert());
  if (!cert) {
    return NS_ERROR_FAILURE;
  }
  CERTCertTrust certTrust{0, 0, 0};
  // If there is no stored trust information, CERT_GetCertTrust will return
  // SECFailure. This isn't a failure. In this case, all trust bits will remain
  // unset.
  Unused << CERT_GetCertTrust(cert.get(), &certTrust);
  nsNSSCertTrust trust(&certTrust);
  if (cert->nickname && trust.HasAnyUser()) {
    *aCertType = nsIX509Cert::USER_CERT;
    return NS_OK;
  }
  if (trust.HasAnyCA()) {
    *aCertType = nsIX509Cert::CA_CERT;
    return NS_OK;
  }
  if (trust.HasPeer(true, false)) {
    *aCertType = nsIX509Cert::SERVER_CERT;
    return NS_OK;
  }
  if (trust.HasPeer(false, true) && cert->emailAddr) {
    *aCertType = nsIX509Cert::EMAIL_CERT;
    return NS_OK;
  }
  if (CERT_IsCACert(cert.get(), nullptr)) {
    *aCertType = nsIX509Cert::CA_CERT;
    return NS_OK;
  }
  if (cert->emailAddr) {
    *aCertType = nsIX509Cert::EMAIL_CERT;
    return NS_OK;
  }
  *aCertType = nsIX509Cert::UNKNOWN_CERT;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetIsBuiltInRoot(bool* aIsBuiltInRoot) {
  NS_ENSURE_ARG(aIsBuiltInRoot);

  pkix::Input certInput;
  pkix::Result rv = certInput.Init(mDER.Elements(), mDER.Length());
  if (rv != pkix::Result::Success) {
    return NS_ERROR_FAILURE;
  }
  rv = IsCertBuiltInRoot(certInput, *aIsBuiltInRoot);
  if (rv != pkix::Result::Success) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetDbKey(nsACString& aDbKey) {
  static_assert(sizeof(uint64_t) == 8, "type size consistency check");
  static_assert(sizeof(uint32_t) == 4, "type size consistency check");

  pkix::Input certInput;
  pkix::Result result = certInput.Init(mDER.Elements(), mDER.Length());
  if (result != pkix::Result::Success) {
    return NS_ERROR_INVALID_ARG;
  }
  // NB: since we're not building a trust path, the endEntityOrCA parameter is
  // irrelevant.
  pkix::BackCert cert(certInput, pkix::EndEntityOrCA::MustBeEndEntity, nullptr);
  result = cert.Init();
  if (result != pkix::Result::Success) {
    return NS_ERROR_INVALID_ARG;
  }

  // The format of the key is the base64 encoding of the following:
  // 4 bytes: {0, 0, 0, 0} (this was intended to be the module ID, but it was
  //                        never implemented)
  // 4 bytes: {0, 0, 0, 0} (this was intended to be the slot ID, but it was
  //                        never implemented)
  // 4 bytes: <serial number length in big-endian order>
  // 4 bytes: <DER-encoded issuer distinguished name length in big-endian order>
  // n bytes: <bytes of serial number>
  // m bytes: <DER-encoded issuer distinguished name>
  nsAutoCString buf;
  const char leadingZeroes[] = {0, 0, 0, 0, 0, 0, 0, 0};
  buf.Append(leadingZeroes, sizeof(leadingZeroes));
  uint32_t serialNumberLen = htonl(cert.GetSerialNumber().GetLength());
  buf.Append(BitwiseCast<const char*, const uint32_t*>(&serialNumberLen),
             sizeof(uint32_t));
  uint32_t issuerLen = htonl(cert.GetIssuer().GetLength());
  buf.Append(BitwiseCast<const char*, const uint32_t*>(&issuerLen),
             sizeof(uint32_t));
  buf.Append(BitwiseCast<const char*, const unsigned char*>(
                 cert.GetSerialNumber().UnsafeGetData()),
             cert.GetSerialNumber().GetLength());
  buf.Append(BitwiseCast<const char*, const unsigned char*>(
                 cert.GetIssuer().UnsafeGetData()),
             cert.GetIssuer().GetLength());

  return Base64Encode(buf, aDbKey);
}

NS_IMETHODIMP
nsNSSCertificate::GetDisplayName(nsAString& aDisplayName) {
  aDisplayName.Truncate();

  UniqueCERTCertificate cert(GetOrInstantiateCert());
  if (!cert) {
    return NS_ERROR_FAILURE;
  }

  UniquePORTString commonName(CERT_GetCommonName(&cert->subject));
  UniquePORTString organizationalUnitName(CERT_GetOrgUnitName(&cert->subject));
  UniquePORTString organizationName(CERT_GetOrgName(&cert->subject));

  bool isBuiltInRoot;
  nsresult rv = GetIsBuiltInRoot(&isBuiltInRoot);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Only use the nickname for built-in roots where we already have a hard-coded
  // reasonable display name (unfortunately we have to strip off the leading
  // slot identifier followed by a ':'). Otherwise, attempt to use the following
  // in order:
  //  - the common name, if present
  //  - an organizational unit name, if present
  //  - an organization name, if present
  //  - the entire subject distinguished name, if non-empty
  //  - an email address, if one can be found
  // In the unlikely event that none of these fields are present and non-empty
  // (the subject really shouldn't be empty), an empty string is returned.
  nsAutoCString builtInRootNickname;
  if (isBuiltInRoot) {
    nsAutoCString fullNickname(cert->nickname);
    int32_t index = fullNickname.Find(":");
    if (index != kNotFound) {
      // Substring will gracefully handle the case where index is the last
      // character in the string (that is, if the nickname is just
      // "Builtin Object Token:"). In that case, we'll get an empty string.
      builtInRootNickname =
          Substring(fullNickname, AssertedCast<uint32_t>(index + 1));
    }
  }
  const char* nameOptions[] = {builtInRootNickname.get(),
                               commonName.get(),
                               organizationalUnitName.get(),
                               organizationName.get(),
                               cert->subjectName,
                               cert->emailAddr};

  for (auto nameOption : nameOptions) {
    if (nameOption) {
      size_t len = strlen(nameOption);
      if (len > 0) {
        LossyUTF8ToUTF16(nameOption, len, aDisplayName);
        return NS_OK;
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetEmailAddress(nsAString& aEmailAddress) {
  UniqueCERTCertificate cert(GetOrInstantiateCert());
  if (!cert) {
    return NS_ERROR_FAILURE;
  }
  if (cert->emailAddr) {
    CopyUTF8toUTF16(MakeStringSpan(cert->emailAddr), aEmailAddress);
  } else {
    GetPIPNSSBundleString("CertNoEmailAddress", aEmailAddress);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetEmailAddresses(nsTArray<nsString>& aAddresses) {
  UniqueCERTCertificate cert(GetOrInstantiateCert());
  if (!cert) {
    return NS_ERROR_FAILURE;
  }
  uint32_t length = 0;
  for (const char* aAddr = CERT_GetFirstEmailAddress(cert.get()); aAddr;
       aAddr = CERT_GetNextEmailAddress(cert.get(), aAddr)) {
    ++(length);
  }

  aAddresses.SetCapacity(length);

  for (const char* aAddr = CERT_GetFirstEmailAddress(cert.get()); aAddr;
       aAddr = CERT_GetNextEmailAddress(cert.get(), aAddr)) {
    CopyASCIItoUTF16(MakeStringSpan(aAddr), *aAddresses.AppendElement());
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::ContainsEmailAddress(const nsAString& aEmailAddress,
                                       bool* result) {
  NS_ENSURE_ARG(result);
  *result = false;

  UniqueCERTCertificate cert(GetOrInstantiateCert());
  if (!cert) {
    return NS_ERROR_FAILURE;
  }
  for (const char* aAddr = CERT_GetFirstEmailAddress(cert.get()); aAddr;
       aAddr = CERT_GetNextEmailAddress(cert.get(), aAddr)) {
    nsAutoString certAddr;
    LossyUTF8ToUTF16(aAddr, strlen(aAddr), certAddr);
    ToLowerCase(certAddr);

    nsAutoString testAddr(aEmailAddress);
    ToLowerCase(testAddr);

    if (certAddr == testAddr) {
      *result = true;
      break;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetCommonName(nsAString& aCommonName) {
  aCommonName.Truncate();
  UniqueCERTCertificate cert(GetOrInstantiateCert());
  if (!cert) {
    return NS_ERROR_FAILURE;
  }
  UniquePORTString commonName(CERT_GetCommonName(&cert->subject));
  if (commonName) {
    LossyUTF8ToUTF16(commonName.get(), strlen(commonName.get()), aCommonName);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetOrganization(nsAString& aOrganization) {
  aOrganization.Truncate();
  UniqueCERTCertificate cert(GetOrInstantiateCert());
  if (!cert) {
    return NS_ERROR_FAILURE;
  }
  UniquePORTString organization(CERT_GetOrgName(&cert->subject));
  if (organization) {
    LossyUTF8ToUTF16(organization.get(), strlen(organization.get()),
                     aOrganization);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetIssuerCommonName(nsAString& aCommonName) {
  aCommonName.Truncate();
  UniqueCERTCertificate cert(GetOrInstantiateCert());
  if (!cert) {
    return NS_ERROR_FAILURE;
  }
  UniquePORTString commonName(CERT_GetCommonName(&cert->issuer));
  if (commonName) {
    LossyUTF8ToUTF16(commonName.get(), strlen(commonName.get()), aCommonName);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetIssuerOrganization(nsAString& aOrganization) {
  aOrganization.Truncate();
  UniqueCERTCertificate cert(GetOrInstantiateCert());
  if (!cert) {
    return NS_ERROR_FAILURE;
  }
  UniquePORTString organization(CERT_GetOrgName(&cert->issuer));
  if (organization) {
    LossyUTF8ToUTF16(organization.get(), strlen(organization.get()),
                     aOrganization);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetIssuerOrganizationUnit(nsAString& aOrganizationUnit) {
  aOrganizationUnit.Truncate();
  UniqueCERTCertificate cert(GetOrInstantiateCert());
  if (!cert) {
    return NS_ERROR_FAILURE;
  }
  UniquePORTString organizationUnit(CERT_GetOrgUnitName(&cert->issuer));
  if (organizationUnit) {
    LossyUTF8ToUTF16(organizationUnit.get(), strlen(organizationUnit.get()),
                     aOrganizationUnit);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetOrganizationalUnit(nsAString& aOrganizationalUnit) {
  aOrganizationalUnit.Truncate();
  UniqueCERTCertificate cert(GetOrInstantiateCert());
  if (!cert) {
    return NS_ERROR_FAILURE;
  }
  UniquePORTString orgunit(CERT_GetOrgUnitName(&cert->subject));
  if (orgunit) {
    LossyUTF8ToUTF16(orgunit.get(), strlen(orgunit.get()), aOrganizationalUnit);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetSubjectName(nsAString& _subjectName) {
  _subjectName.Truncate();
  UniqueCERTCertificate cert(GetOrInstantiateCert());
  if (!cert) {
    return NS_ERROR_FAILURE;
  }
  if (cert->subjectName) {
    LossyUTF8ToUTF16(cert->subjectName, strlen(cert->subjectName),
                     _subjectName);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetIssuerName(nsAString& _issuerName) {
  _issuerName.Truncate();
  UniqueCERTCertificate cert(GetOrInstantiateCert());
  if (!cert) {
    return NS_ERROR_FAILURE;
  }
  if (cert->issuerName) {
    LossyUTF8ToUTF16(cert->issuerName, strlen(cert->issuerName), _issuerName);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetSerialNumber(nsAString& _serialNumber) {
  _serialNumber.Truncate();
  UniqueCERTCertificate cert(GetOrInstantiateCert());
  if (!cert) {
    return NS_ERROR_FAILURE;
  }
  UniquePORTString tmpstr(
      CERT_Hexify(&cert->serialNumber, true /* use colon delimiters */));
  if (tmpstr) {
    _serialNumber = NS_ConvertASCIItoUTF16(tmpstr.get());
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

nsresult nsNSSCertificate::GetCertificateHash(nsAString& aFingerprint,
                                              SECOidTag aHashAlg) {
  aFingerprint.Truncate();

  if (!EnsureNSSInitializedChromeOrContent()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsTArray<uint8_t> digestArray;
  nsresult rv =
      Digest::DigestBuf(aHashAlg, mDER.Elements(), mDER.Length(), digestArray);
  if (NS_FAILED(rv)) {
    return rv;
  }
  SECItem digestItem = {siBuffer, digestArray.Elements(),
                        static_cast<unsigned int>(digestArray.Length())};

  UniquePORTString fpStr(
      CERT_Hexify(&digestItem, true /* use colon delimiters */));
  if (!fpStr) {
    return NS_ERROR_FAILURE;
  }

  aFingerprint.AssignASCII(fpStr.get());
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetSha256Fingerprint(nsAString& aSha256Fingerprint) {
  return GetCertificateHash(aSha256Fingerprint, SEC_OID_SHA256);
}

NS_IMETHODIMP
nsNSSCertificate::GetSha1Fingerprint(nsAString& _sha1Fingerprint) {
  return GetCertificateHash(_sha1Fingerprint, SEC_OID_SHA1);
}

NS_IMETHODIMP
nsNSSCertificate::GetTokenName(nsAString& aTokenName) {
  UniqueCERTCertificate cert(GetOrInstantiateCert());
  if (!cert) {
    return NS_ERROR_FAILURE;
  }
  UniquePK11SlotInfo internalSlot(PK11_GetInternalSlot());
  if (!internalSlot) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsIPK11Token> token(
      new nsPK11Token(cert->slot ? cert->slot : internalSlot.get()));
  nsAutoCString tmp;
  nsresult rv = token->GetTokenName(tmp);
  if (NS_FAILED(rv)) {
    return rv;
  }
  aTokenName.Assign(NS_ConvertUTF8toUTF16(tmp));
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetSha256SubjectPublicKeyInfoDigest(
    nsACString& aSha256SPKIDigest) {
  aSha256SPKIDigest.Truncate();

  if (!EnsureNSSInitializedChromeOrContent()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  pkix::Input certInput;
  pkix::Result result = certInput.Init(mDER.Elements(), mDER.Length());
  if (result != pkix::Result::Success) {
    return NS_ERROR_INVALID_ARG;
  }
  // NB: since we're not building a trust path, the endEntityOrCA parameter is
  // irrelevant.
  pkix::BackCert cert(certInput, pkix::EndEntityOrCA::MustBeEndEntity, nullptr);
  result = cert.Init();
  if (result != pkix::Result::Success) {
    return NS_ERROR_INVALID_ARG;
  }
  pkix::Input derPublicKey = cert.GetSubjectPublicKeyInfo();
  nsTArray<uint8_t> digestArray;
  nsresult rv = Digest::DigestBuf(SEC_OID_SHA256, derPublicKey.UnsafeGetData(),
                                  derPublicKey.GetLength(), digestArray);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Base64Encode(nsDependentCSubstring(
                        reinterpret_cast<const char*>(digestArray.Elements()),
                        digestArray.Length()),
                    aSha256SPKIDigest);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetRawDER(nsTArray<uint8_t>& aArray) {
  aArray.SetLength(mDER.Length());
  memcpy(aArray.Elements(), mDER.Elements(), mDER.Length());
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetBase64DERString(nsACString& base64DERString) {
  nsDependentCSubstring derString(
      reinterpret_cast<const char*>(mDER.Elements()), mDER.Length());
  nsresult rv = Base64Encode(derString, base64DERString);
  if (NS_FAILED(rv)) {
    return rv;
  }
  return NS_OK;
}

CERTCertificate* nsNSSCertificate::GetCert() {
  UniqueCERTCertificate cert(GetOrInstantiateCert());
  return cert.release();  // caller takes ownership
}

NS_IMETHODIMP
nsNSSCertificate::GetValidity(nsIX509CertValidity** aValidity) {
  NS_ENSURE_ARG(aValidity);
  pkix::Input certInput;
  pkix::Result rv = certInput.Init(mDER.Elements(), mDER.Length());
  if (rv != pkix::Success) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsIX509CertValidity> validity = new X509CertValidity(certInput);
  validity.forget(aValidity);
  return NS_OK;
}

// NB: Any updates (except disk-only fields) must be kept in sync with
//     |SerializeToIPC|.
NS_IMETHODIMP
nsNSSCertificate::Write(nsIObjectOutputStream* aStream) {
  // This field used to be the cached EV status, but it is no longer necessary.
  nsresult rv = aStream->Write32(0);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = aStream->Write32(mDER.Length());
  if (NS_FAILED(rv)) {
    return rv;
  }
  return aStream->WriteBytes(Span(mDER));
}

// NB: Any updates (except disk-only fields) must be kept in sync with
//     |DeserializeFromIPC|.
NS_IMETHODIMP
nsNSSCertificate::Read(nsIObjectInputStream* aStream) {
  auto lock = mCert.Lock();
  auto& maybeCert = lock.ref();
  if (!mDER.IsEmpty() || maybeCert.isSome()) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }

  // This field is no longer used.
  uint32_t unusedCachedEVStatus;
  nsresult rv = aStream->Read32(&unusedCachedEVStatus);
  if (NS_FAILED(rv)) {
    return rv;
  }

  uint32_t len;
  rv = aStream->Read32(&len);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = aStream->ReadByteArray(len, mDER);
  if (NS_FAILED(rv)) {
    return rv;
  }
  return NS_OK;
}

void nsNSSCertificate::SerializeToIPC(IPC::MessageWriter* aWriter) {
  bool hasCert = !mDER.IsEmpty();
  WriteParam(aWriter, hasCert);

  if (!hasCert) {
    return;
  }

  WriteParam(aWriter, mDER);
}

bool nsNSSCertificate::DeserializeFromIPC(IPC::MessageReader* aReader) {
  auto lock = mCert.Lock();
  auto& maybeCert = lock.ref();
  if (!mDER.IsEmpty() || maybeCert.isSome()) {
    return false;
  }

  bool hasCert = false;
  if (!ReadParam(aReader, &hasCert)) {
    return false;
  }

  if (!hasCert) {
    return true;
  }

  if (!ReadParam(aReader, &mDER)) {
    return false;
  }
  return true;
}

NS_IMETHODIMP
nsNSSCertificate::GetInterfaces(nsTArray<nsIID>& array) {
  array.Clear();
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetScriptableHelper(nsIXPCScriptable** _retval) {
  *_retval = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetContractID(nsACString& aContractID) {
  aContractID.SetIsVoid(true);
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetClassDescription(nsACString& aClassDescription) {
  aClassDescription.SetIsVoid(true);
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetClassID(nsCID** aClassID) {
  *aClassID = (nsCID*)moz_xmalloc(sizeof(nsCID));
  return GetClassIDNoAlloc(*aClassID);
}

NS_IMETHODIMP
nsNSSCertificate::GetFlags(uint32_t* aFlags) {
  *aFlags = nsIClassInfo::THREADSAFE;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetClassIDNoAlloc(nsCID* aClassIDNoAlloc) {
  static NS_DEFINE_CID(kNSSCertificateCID, NS_X509CERT_CID);

  *aClassIDNoAlloc = kNSSCertificateCID;
  return NS_OK;
}
