/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// We need Windows 8 functions and structures to be able to verify SHA-256.
#if defined(_WIN32_WINNT)
#  undef _WIN32_WINNT
#  define _WIN32_WINNT _WIN32_WINNT_WIN8
#endif  // defined(_WIN32_WINNT)
#if defined(NTDDI_VERSION)
#  undef NTDDI_VERSION
#  define NTDDI_VERSION NTDDI_WIN8
#endif  // defined(NTDDI_VERSION)

#include "Authenticode.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/DynamicallyLinkedFunctionPtr.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/WindowsVersion.h"
#include "nsWindowsHelpers.h"

#include <windows.h>
#include <softpub.h>
#include <wincrypt.h>
#include <wintrust.h>
#include <mscat.h>

#include <string.h>

namespace {

struct CertStoreDeleter {
  typedef HCERTSTORE pointer;
  void operator()(pointer aStore) { ::CertCloseStore(aStore, 0); }
};

struct CryptMsgDeleter {
  typedef HCRYPTMSG pointer;
  void operator()(pointer aMsg) { ::CryptMsgClose(aMsg); }
};

struct CertContextDeleter {
  void operator()(PCCERT_CONTEXT aCertContext) {
    ::CertFreeCertificateContext(aCertContext);
  }
};

struct CATAdminContextDeleter {
  typedef HCATADMIN pointer;
  void operator()(pointer aCtx) {
    static const mozilla::StaticDynamicallyLinkedFunctionPtr<
        decltype(&::CryptCATAdminReleaseContext)>
        pCryptCATAdminReleaseContext(L"wintrust.dll",
                                     "CryptCATAdminReleaseContext");

    MOZ_ASSERT(!!pCryptCATAdminReleaseContext);
    if (!pCryptCATAdminReleaseContext) {
      return;
    }

    pCryptCATAdminReleaseContext(aCtx, 0);
  }
};

typedef mozilla::UniquePtr<HCERTSTORE, CertStoreDeleter> CertStoreUniquePtr;
typedef mozilla::UniquePtr<HCRYPTMSG, CryptMsgDeleter> CryptMsgUniquePtr;
typedef mozilla::UniquePtr<const CERT_CONTEXT, CertContextDeleter>
    CertContextUniquePtr;
typedef mozilla::UniquePtr<HCATADMIN, CATAdminContextDeleter>
    CATAdminContextUniquePtr;

static const DWORD kEncodingTypes = X509_ASN_ENCODING | PKCS_7_ASN_ENCODING;

class SignedBinary final {
 public:
  SignedBinary(const wchar_t* aFilePath, mozilla::AuthenticodeFlags aFlags);

  explicit operator bool() const { return mCertStore && mCryptMsg && mCertCtx; }

  mozilla::UniquePtr<wchar_t[]> GetOrgName();

  SignedBinary(const SignedBinary&) = delete;
  SignedBinary(SignedBinary&&) = delete;
  SignedBinary& operator=(const SignedBinary&) = delete;
  SignedBinary& operator=(SignedBinary&&) = delete;

 private:
  bool VerifySignature(const wchar_t* aFilePath);
  bool QueryObject(const wchar_t* aFilePath);
  static bool VerifySignatureInternal(WINTRUST_DATA& aTrustData);

 private:
  enum class TrustSource { eNone, eEmbedded, eCatalog };

 private:
  const mozilla::AuthenticodeFlags mFlags;
  TrustSource mTrustSource;
  CertStoreUniquePtr mCertStore;
  CryptMsgUniquePtr mCryptMsg;
  CertContextUniquePtr mCertCtx;
};

SignedBinary::SignedBinary(const wchar_t* aFilePath,
                           mozilla::AuthenticodeFlags aFlags)
    : mFlags(aFlags), mTrustSource(TrustSource::eNone) {
  if (!VerifySignature(aFilePath)) {
    return;
  }

  DWORD certInfoLen = 0;
  BOOL ok = CryptMsgGetParam(mCryptMsg.get(), CMSG_SIGNER_CERT_INFO_PARAM, 0,
                             nullptr, &certInfoLen);
  if (!ok) {
    return;
  }

  auto certInfoBuf = mozilla::MakeUnique<char[]>(certInfoLen);

  ok = CryptMsgGetParam(mCryptMsg.get(), CMSG_SIGNER_CERT_INFO_PARAM, 0,
                        certInfoBuf.get(), &certInfoLen);
  if (!ok) {
    return;
  }

  auto certInfo = reinterpret_cast<CERT_INFO*>(certInfoBuf.get());

  PCCERT_CONTEXT certCtx =
      CertFindCertificateInStore(mCertStore.get(), kEncodingTypes, 0,
                                 CERT_FIND_SUBJECT_CERT, certInfo, nullptr);
  if (!certCtx) {
    return;
  }

  mCertCtx.reset(certCtx);
}

bool SignedBinary::QueryObject(const wchar_t* aFilePath) {
  DWORD encodingType, contentType, formatType;
  HCERTSTORE rawCertStore;
  HCRYPTMSG rawCryptMsg;
  BOOL result = ::CryptQueryObject(CERT_QUERY_OBJECT_FILE, aFilePath,
                                   CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
                                   CERT_QUERY_FORMAT_FLAG_BINARY, 0,
                                   &encodingType, &contentType, &formatType,
                                   &rawCertStore, &rawCryptMsg, nullptr);
  if (!result) {
    return false;
  }

  mCertStore.reset(rawCertStore);
  mCryptMsg.reset(rawCryptMsg);

  return true;
}

/**
 * @param aTrustData must be a WINTRUST_DATA structure that has been zeroed out
 *                   and then populated at least with its |cbStruct|,
 *                   |dwUnionChoice|, and appropriate union field. This function
 *                   will then populate the remaining fields as appropriate.
 */
/* static */
bool SignedBinary::VerifySignatureInternal(WINTRUST_DATA& aTrustData) {
  aTrustData.dwUIChoice = WTD_UI_NONE;
  aTrustData.fdwRevocationChecks = WTD_REVOKE_NONE;
  aTrustData.dwStateAction = WTD_STATEACTION_VERIFY;
  aTrustData.dwProvFlags = WTD_CACHE_ONLY_URL_RETRIEVAL;

  const HWND hwnd = (HWND)INVALID_HANDLE_VALUE;
  GUID policyGUID = WINTRUST_ACTION_GENERIC_VERIFY_V2;
  LONG result = ::WinVerifyTrust(hwnd, &policyGUID, &aTrustData);

  aTrustData.dwStateAction = WTD_STATEACTION_CLOSE;
  ::WinVerifyTrust(hwnd, &policyGUID, &aTrustData);

  return result == ERROR_SUCCESS;
}

bool SignedBinary::VerifySignature(const wchar_t* aFilePath) {
  // First, try the binary itself
  if (QueryObject(aFilePath)) {
    mTrustSource = TrustSource::eEmbedded;
    if (mFlags & mozilla::AuthenticodeFlags::SkipTrustVerification) {
      return true;
    }

    WINTRUST_FILE_INFO fileInfo = {sizeof(fileInfo)};
    fileInfo.pcwszFilePath = aFilePath;

    WINTRUST_DATA trustData = {sizeof(trustData)};
    trustData.dwUnionChoice = WTD_CHOICE_FILE;
    trustData.pFile = &fileInfo;

    return VerifySignatureInternal(trustData);
  }

  // We didn't find anything in the binary, so now try a catalog file.

  // First, we open a catalog admin context.
  HCATADMIN rawCatAdmin;

  // Windows 7 also exports the CryptCATAdminAcquireContext2 API, but it does
  // *not* sign its binaries with SHA-256, so we use the old API in that case.
  if (mozilla::IsWin8OrLater()) {
    static const mozilla::StaticDynamicallyLinkedFunctionPtr<
        decltype(&::CryptCATAdminAcquireContext2)>
        pCryptCATAdminAcquireContext2(L"wintrust.dll",
                                      "CryptCATAdminAcquireContext2");
    if (!pCryptCATAdminAcquireContext2) {
      return false;
    }

    CERT_STRONG_SIGN_PARA policy = {sizeof(policy)};
    policy.dwInfoChoice = CERT_STRONG_SIGN_OID_INFO_CHOICE;
    policy.pszOID = const_cast<char*>(
        szOID_CERT_STRONG_SIGN_OS_CURRENT);  // -Wwritable-strings

    if (!pCryptCATAdminAcquireContext2(&rawCatAdmin, nullptr,
                                       BCRYPT_SHA256_ALGORITHM, &policy, 0)) {
      return false;
    }
  } else {
    static const mozilla::StaticDynamicallyLinkedFunctionPtr<
        decltype(&::CryptCATAdminAcquireContext)>
        pCryptCATAdminAcquireContext(L"wintrust.dll",
                                     "CryptCATAdminAcquireContext");

    if (!pCryptCATAdminAcquireContext ||
        !pCryptCATAdminAcquireContext(&rawCatAdmin, nullptr, 0)) {
      return false;
    }
  }

  CATAdminContextUniquePtr catAdmin(rawCatAdmin);

  // Now we need to hash the file at aFilePath.
  // Since we're hashing this file, let's open it with a sequential scan hint.
  HANDLE rawFile =
      ::CreateFileW(aFilePath, GENERIC_READ,
                    FILE_SHARE_READ | FILE_SHARE_DELETE | FILE_SHARE_WRITE,
                    nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
  if (rawFile == INVALID_HANDLE_VALUE) {
    return false;
  }

  nsAutoHandle file(rawFile);
  DWORD hashLen = 0;
  mozilla::UniquePtr<BYTE[]> hashBuf;

  static const mozilla::StaticDynamicallyLinkedFunctionPtr<
      decltype(&::CryptCATAdminCalcHashFromFileHandle2)>
      pCryptCATAdminCalcHashFromFileHandle2(
          L"wintrust.dll", "CryptCATAdminCalcHashFromFileHandle2");
  if (pCryptCATAdminCalcHashFromFileHandle2) {
    if (!pCryptCATAdminCalcHashFromFileHandle2(rawCatAdmin, rawFile, &hashLen,
                                               nullptr, 0) &&
        ::GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
      return false;
    }

    hashBuf = mozilla::MakeUnique<BYTE[]>(hashLen);

    if (!pCryptCATAdminCalcHashFromFileHandle2(rawCatAdmin, rawFile, &hashLen,
                                               hashBuf.get(), 0)) {
      return false;
    }
  } else {
    static const mozilla::StaticDynamicallyLinkedFunctionPtr<
        decltype(&::CryptCATAdminCalcHashFromFileHandle)>
        pCryptCATAdminCalcHashFromFileHandle(
            L"wintrust.dll", "CryptCATAdminCalcHashFromFileHandle");

    if (!pCryptCATAdminCalcHashFromFileHandle) {
      return false;
    }

    if (!pCryptCATAdminCalcHashFromFileHandle(rawFile, &hashLen, nullptr, 0) &&
        ::GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
      return false;
    }

    hashBuf = mozilla::MakeUnique<BYTE[]>(hashLen);

    if (!pCryptCATAdminCalcHashFromFileHandle(rawFile, &hashLen, hashBuf.get(),
                                              0)) {
      return false;
    }
  }

  // Now that we've hashed the file, query the catalog system to see if any
  // catalogs reference a binary with our hash.

  static const mozilla::StaticDynamicallyLinkedFunctionPtr<
      decltype(&::CryptCATAdminEnumCatalogFromHash)>
      pCryptCATAdminEnumCatalogFromHash(L"wintrust.dll",
                                        "CryptCATAdminEnumCatalogFromHash");
  if (!pCryptCATAdminEnumCatalogFromHash) {
    return false;
  }

  static const mozilla::StaticDynamicallyLinkedFunctionPtr<
      decltype(&::CryptCATAdminReleaseCatalogContext)>
      pCryptCATAdminReleaseCatalogContext(L"wintrust.dll",
                                          "CryptCATAdminReleaseCatalogContext");
  if (!pCryptCATAdminReleaseCatalogContext) {
    return false;
  }

  HCATINFO catInfoHdl = pCryptCATAdminEnumCatalogFromHash(
      rawCatAdmin, hashBuf.get(), hashLen, 0, nullptr);
  if (!catInfoHdl) {
    return false;
  }

  // We can't use UniquePtr for this because the deleter function requires two
  // parameters.
  auto cleanCatInfoHdl =
      mozilla::MakeScopeExit([rawCatAdmin, catInfoHdl]() -> void {
        pCryptCATAdminReleaseCatalogContext(rawCatAdmin, catInfoHdl, 0);
      });

  // We found a catalog! Now query for the path to the catalog file.

  static const mozilla::StaticDynamicallyLinkedFunctionPtr<
      decltype(&::CryptCATCatalogInfoFromContext)>
      pCryptCATCatalogInfoFromContext(L"wintrust.dll",
                                      "CryptCATCatalogInfoFromContext");
  if (!pCryptCATCatalogInfoFromContext) {
    return false;
  }

  CATALOG_INFO_ catInfo = {sizeof(catInfo)};
  if (!pCryptCATCatalogInfoFromContext(catInfoHdl, &catInfo, 0)) {
    return false;
  }

  if (!QueryObject(catInfo.wszCatalogFile)) {
    return false;
  }

  mTrustSource = TrustSource::eCatalog;

  if (mFlags & mozilla::AuthenticodeFlags::SkipTrustVerification) {
    return true;
  }

  // WINTRUST_CATALOG_INFO::pcwszMemberTag is commonly set to the string
  // representation of the file hash, so we build that here.

  DWORD strHashBufLen = (hashLen * 2) + 1;
  auto strHashBuf = mozilla::MakeUnique<wchar_t[]>(strHashBufLen);
  if (!::CryptBinaryToStringW(hashBuf.get(), hashLen,
                              CRYPT_STRING_HEXRAW | CRYPT_STRING_NOCRLF,
                              strHashBuf.get(), &strHashBufLen)) {
    return false;
  }

  // Ensure that the tag is uppercase for WinVerifyTrust
  // NB: CryptBinaryToStringW overwrites strHashBufLen with the length excluding
  //     the null terminator, so we need to add it back for this call.
  if (_wcsupr_s(strHashBuf.get(), strHashBufLen + 1)) {
    return false;
  }

  // Now, given the path to the catalog, and the path to the member (ie, the
  // binary whose hash we are validating), we may now validate. If the
  // validation is successful, we then QueryObject on the *catalog file*
  // instead of the binary.

  WINTRUST_CATALOG_INFO wtCatInfo = {sizeof(wtCatInfo)};
  wtCatInfo.pcwszCatalogFilePath = catInfo.wszCatalogFile;
  wtCatInfo.pcwszMemberTag = strHashBuf.get();
  wtCatInfo.pcwszMemberFilePath = aFilePath;
  wtCatInfo.hMemberFile = rawFile;
  if (mozilla::IsWin8OrLater()) {
    wtCatInfo.hCatAdmin = rawCatAdmin;
  }

  WINTRUST_DATA trustData = {sizeof(trustData)};
  trustData.dwUnionChoice = WTD_CHOICE_CATALOG;
  trustData.pCatalog = &wtCatInfo;

  return VerifySignatureInternal(trustData);
}

mozilla::UniquePtr<wchar_t[]> SignedBinary::GetOrgName() {
  DWORD charCount = CertGetNameStringW(
      mCertCtx.get(), CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, nullptr, nullptr, 0);
  if (charCount <= 1) {
    // Not found
    return nullptr;
  }

  auto result = mozilla::MakeUnique<wchar_t[]>(charCount);
  charCount = CertGetNameStringW(mCertCtx.get(), CERT_NAME_SIMPLE_DISPLAY_TYPE,
                                 0, nullptr, result.get(), charCount);
  MOZ_ASSERT(charCount > 1);

  return result;
}

}  // anonymous namespace

namespace mozilla {

class AuthenticodeImpl : public Authenticode {
 public:
  virtual UniquePtr<wchar_t[]> GetBinaryOrgName(
      const wchar_t* aFilePath,
      AuthenticodeFlags aFlags = AuthenticodeFlags::Default) override;
};

UniquePtr<wchar_t[]> AuthenticodeImpl::GetBinaryOrgName(
    const wchar_t* aFilePath, AuthenticodeFlags aFlags) {
  SignedBinary bin(aFilePath, aFlags);
  if (!bin) {
    return nullptr;
  }

  return bin.GetOrgName();
}

static AuthenticodeImpl sAuthenticodeImpl;

Authenticode* GetAuthenticode() { return &sAuthenticodeImpl; }

}  // namespace mozilla
