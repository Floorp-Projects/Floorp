/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_MEMORY
#define MOZ_MEMORY_IMPL
#include "mozmemory_wrap.h"
#define MALLOC_FUNCS MALLOC_FUNCS_MALLOC
// See mozmemory_wrap.h for more details. This file is part of libmozglue, so
// it needs to use _impl suffixes.
#define MALLOC_DECL(name, return_type, ...) \
  extern "C" MOZ_MEMORY_API return_type name ## _impl(__VA_ARGS__);
#include "malloc_decls.h"
#include "mozilla/mozalloc.h"
#endif

#include "Authenticode.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/TypeTraits.h"
#include "mozilla/UniquePtr.h"

#include <windows.h>
#include <softpub.h>
#include <wincrypt.h>
#include <wintrust.h>

namespace {

struct CertStoreDeleter
{
  typedef HCERTSTORE pointer;
  void operator()(pointer aStore)
  {
    ::CertCloseStore(aStore, 0);
  }
};

struct CryptMsgDeleter
{
  typedef HCRYPTMSG pointer;
  void operator()(pointer aMsg)
  {
    ::CryptMsgClose(aMsg);
  }
};

struct CertContextDeleter
{
  void operator()(PCCERT_CONTEXT aCertContext)
  {
    ::CertFreeCertificateContext(aCertContext);
  }
};

typedef mozilla::UniquePtr<HCERTSTORE, CertStoreDeleter> CertStoreUniquePtr;
typedef mozilla::UniquePtr<HCRYPTMSG, CryptMsgDeleter> CryptMsgUniquePtr;
typedef mozilla::UniquePtr<const CERT_CONTEXT, CertContextDeleter> CertContextUniquePtr;

static const DWORD kEncodingTypes = X509_ASN_ENCODING | PKCS_7_ASN_ENCODING;

class SignedBinary
{
public:
  explicit SignedBinary(const wchar_t* aFilePath);

  explicit operator bool() const
  {
    return mCertStore && mCryptMsg && mCertCtx;
  }

  mozilla::UniquePtr<wchar_t[]> GetOrgName();

  SignedBinary(const SignedBinary&) = delete;
  SignedBinary(SignedBinary&&) = delete;
  SignedBinary& operator=(const SignedBinary&) = delete;
  SignedBinary& operator=(SignedBinary&&) = delete;

private:
  bool VerifySignature(const wchar_t* aFilePath);

private:
  CertStoreUniquePtr    mCertStore;
  CryptMsgUniquePtr     mCryptMsg;
  CertContextUniquePtr  mCertCtx;
};

SignedBinary::SignedBinary(const wchar_t* aFilePath)
{
  if (!VerifySignature(aFilePath)) {
    return;
  }

  DWORD encodingType, contentType, formatType;
  HCERTSTORE rawCertStore;
  HCRYPTMSG rawCryptMsg;
  BOOL result = CryptQueryObject(CERT_QUERY_OBJECT_FILE, aFilePath,
                                 CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
                                 CERT_QUERY_FORMAT_FLAG_BINARY, 0,
                                 &encodingType, &contentType, &formatType,
                                 &rawCertStore, &rawCryptMsg, nullptr);
  if (!result) {
    return;
  }

  mCertStore.reset(rawCertStore);
  mCryptMsg.reset(rawCryptMsg);

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

  PCCERT_CONTEXT certCtx = CertFindCertificateInStore(mCertStore.get(),
                                                      kEncodingTypes, 0,
                                                      CERT_FIND_SUBJECT_CERT,
                                                      certInfo, nullptr);
  if (!certCtx) {
    return;
  }

  mCertCtx.reset(certCtx);
}

bool
SignedBinary::VerifySignature(const wchar_t* aFilePath)
{
  WINTRUST_FILE_INFO fileInfo = {sizeof(fileInfo)};
  fileInfo.pcwszFilePath = aFilePath;

  WINTRUST_DATA trustData = {sizeof(trustData)};
  trustData.dwUIChoice = WTD_UI_NONE;
  trustData.fdwRevocationChecks = WTD_REVOKE_NONE;
  trustData.dwUnionChoice = WTD_CHOICE_FILE;
  trustData.pFile = &fileInfo;
  trustData.dwStateAction = WTD_STATEACTION_VERIFY;
  trustData.dwProvFlags = WTD_CACHE_ONLY_URL_RETRIEVAL;

  const HWND hwnd = (HWND) INVALID_HANDLE_VALUE;
  GUID policyGUID = WINTRUST_ACTION_GENERIC_VERIFY_V2;
  LONG result = WinVerifyTrust(hwnd, &policyGUID, &trustData);

  trustData.dwStateAction = WTD_STATEACTION_CLOSE;
  WinVerifyTrust(hwnd, &policyGUID, &trustData);

  return result == ERROR_SUCCESS;
}

mozilla::UniquePtr<wchar_t[]>
SignedBinary::GetOrgName()
{
  DWORD charCount = CertGetNameStringW(mCertCtx.get(),
                                       CERT_NAME_SIMPLE_DISPLAY_TYPE, 0,
                                       nullptr, nullptr, 0);
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

} // anonymous namespace

namespace mozilla {

class AuthenticodeImpl : public Authenticode
{
public:
  virtual UniquePtr<wchar_t[]> GetBinaryOrgName(const wchar_t* aFilePath) override;
};

UniquePtr<wchar_t[]>
AuthenticodeImpl::GetBinaryOrgName(const wchar_t* aFilePath)
{
  SignedBinary bin(aFilePath);
  if (!bin) {
    return nullptr;
  }

  return bin.GetOrgName();
}

static AuthenticodeImpl sAuthenticodeImpl;

Authenticode*
GetAuthenticode()
{
  return &sAuthenticodeImpl;
}

} // namespace mozilla

