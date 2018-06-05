/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SecretDecoderRing.h"

#include "ScopedNSSTypes.h"
#include "mozilla/Base64.h"
#include "mozilla/Casting.h"
#include "mozilla/Services.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/Promise.h"
#include "nsCOMPtr.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIObserverService.h"
#include "nsIServiceManager.h"
#include "nsITokenPasswordDialogs.h"
#include "nsNSSComponent.h"
#include "nsNSSHelper.h"
#include "nsPK11TokenDB.h"
#include "pk11func.h"
#include "pk11sdr.h" // For PK11SDR_Encrypt, PK11SDR_Decrypt
#include "ssl.h" // For SSL_ClearSessionCache

using namespace mozilla;
using dom::Promise;

NS_IMPL_ISUPPORTS(SecretDecoderRing, nsISecretDecoderRing)

void BackgroundSdrEncryptStrings(const nsTArray<nsCString>& plaintexts,
                                 RefPtr<Promise>& aPromise) {
  nsCOMPtr<nsISecretDecoderRing> sdrService =
    do_GetService(NS_SECRETDECODERRING_CONTRACTID);
  InfallibleTArray<nsString> cipherTexts(plaintexts.Length());

  nsresult rv = NS_ERROR_FAILURE;
  for (uint32_t i = 0; i < plaintexts.Length(); ++i) {
    const nsCString& plaintext = plaintexts[i];
    nsCString cipherText;
    rv = sdrService->EncryptString(plaintext, cipherText);

    if (NS_WARN_IF(NS_FAILED(rv))) {
      break;
    }

    cipherTexts.AppendElement(NS_ConvertASCIItoUTF16(cipherText));
  }

  nsCOMPtr<nsIRunnable> runnable(
    NS_NewRunnableFunction("BackgroundSdrEncryptStringsResolve",
                           [rv, aPromise = std::move(aPromise), cipherTexts = std::move(cipherTexts)]() {
                             if (NS_FAILED(rv)) {
                               aPromise->MaybeReject(rv);
                             } else {
                               aPromise->MaybeResolve(cipherTexts);
                             }
                           }));
  NS_DispatchToMainThread(runnable.forget());
}

nsresult
SecretDecoderRing::Encrypt(const nsACString& data, /*out*/ nsACString& result)
{
  UniquePK11SlotInfo slot(PK11_GetInternalKeySlot());
  if (!slot) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  /* Make sure token is initialized. */
  nsCOMPtr<nsIInterfaceRequestor> ctx = new PipUIContext();
  nsresult rv = setPassword(slot.get(), ctx);
  if (NS_FAILED(rv)) {
    return rv;
  }

  /* Force authentication */
  if (PK11_Authenticate(slot.get(), true, ctx) != SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  /* Use default key id */
  SECItem keyid;
  keyid.data = nullptr;
  keyid.len = 0;
  SECItem request;
  request.data = BitwiseCast<unsigned char*, const char*>(data.BeginReading());
  request.len = data.Length();
  ScopedAutoSECItem reply;
  if (PK11SDR_Encrypt(&keyid, &request, &reply, ctx) != SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  result.Assign(BitwiseCast<char*, unsigned char*>(reply.data), reply.len);
  return NS_OK;
}

nsresult
SecretDecoderRing::Decrypt(const nsACString& data, /*out*/ nsACString& result)
{
  /* Find token with SDR key */
  UniquePK11SlotInfo slot(PK11_GetInternalKeySlot());
  if (!slot) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  /* Force authentication */
  nsCOMPtr<nsIInterfaceRequestor> ctx = new PipUIContext();
  if (PK11_Authenticate(slot.get(), true, ctx) != SECSuccess) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  SECItem request;
  request.data = BitwiseCast<unsigned char*, const char*>(data.BeginReading());
  request.len = data.Length();
  ScopedAutoSECItem reply;
  if (PK11SDR_Decrypt(&request, &reply, ctx) != SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  result.Assign(BitwiseCast<char*, unsigned char*>(reply.data), reply.len);
  return NS_OK;
}

NS_IMETHODIMP
SecretDecoderRing::EncryptString(const nsACString& text,
                         /*out*/ nsACString& encryptedBase64Text)
{
  nsAutoCString encryptedText;
  nsresult rv = Encrypt(text, encryptedText);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = Base64Encode(encryptedText, encryptedBase64Text);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_OK;
}

NS_IMETHODIMP
SecretDecoderRing::AsyncEncryptStrings(uint32_t plaintextsCount,
                                       const char16_t** plaintexts,
                                       JSContext* aCx,
                                       Promise** aPromise) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG(plaintextsCount);
  NS_ENSURE_ARG_POINTER(plaintexts);
  NS_ENSURE_ARG_POINTER(aCx);

  nsIGlobalObject* globalObject =
    xpc::NativeGlobal(JS::CurrentGlobalOrNull(aCx));

  if (NS_WARN_IF(!globalObject)) {
    return NS_ERROR_UNEXPECTED;
  }

  ErrorResult result;
  RefPtr<Promise> promise = Promise::Create(globalObject, result);
  if (NS_WARN_IF(result.Failed())) {
    return result.StealNSResult();
  }

  InfallibleTArray<nsCString> plaintextsUtf8(plaintextsCount);
  for (uint32_t i = 0; i < plaintextsCount; ++i) {
    plaintextsUtf8.AppendElement(NS_ConvertUTF16toUTF8(plaintexts[i]));
  }
  nsCOMPtr<nsIRunnable> runnable(
    NS_NewRunnableFunction("BackgroundSdrEncryptStrings",
      [promise, plaintextsUtf8 = std::move(plaintextsUtf8)]() mutable {
        BackgroundSdrEncryptStrings(plaintextsUtf8, promise);
      }));

  nsCOMPtr<nsIThread> encryptionThread;
  nsresult rv = NS_NewNamedThread("AsyncSDRThread",
                                  getter_AddRefs(encryptionThread),
                                  runnable);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  promise.forget(aPromise);
  return NS_OK;
}

NS_IMETHODIMP
SecretDecoderRing::DecryptString(const nsACString& encryptedBase64Text,
                         /*out*/ nsACString& decryptedText)
{
  nsAutoCString encryptedText;
  nsresult rv = Base64Decode(encryptedBase64Text, encryptedText);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = Decrypt(encryptedText, decryptedText);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_OK;
}

NS_IMETHODIMP
SecretDecoderRing::ChangePassword()
{
  UniquePK11SlotInfo slot(PK11_GetInternalKeySlot());
  if (!slot) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // nsPK11Token::nsPK11Token takes its own reference to slot, so we pass a
  // non-owning pointer here.
  nsCOMPtr<nsIPK11Token> token = new nsPK11Token(slot.get());

  nsCOMPtr<nsITokenPasswordDialogs> dialogs;
  nsresult rv = getNSSDialogs(getter_AddRefs(dialogs),
                              NS_GET_IID(nsITokenPasswordDialogs),
                              NS_TOKENPASSWORDSDIALOG_CONTRACTID);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIInterfaceRequestor> ctx = new PipUIContext();
  bool canceled; // Ignored
  return dialogs->SetPassword(ctx, token, &canceled);
}

NS_IMETHODIMP
SecretDecoderRing::Logout()
{
  PK11_LogoutAll();
  SSL_ClearSessionCache();
  return NS_OK;
}

NS_IMETHODIMP
SecretDecoderRing::LogoutAndTeardown()
{
  static NS_DEFINE_CID(kNSSComponentCID, NS_NSSCOMPONENT_CID);

  PK11_LogoutAll();
  SSL_ClearSessionCache();

  nsresult rv;
  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = nssComponent->LogoutAuthenticatedPK11();

  // After we just logged out, we need to prune dead connections to make
  // sure that all connections that should be stopped, are stopped. See
  // bug 517584.
  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os) {
    os->NotifyObservers(nullptr, "net:prune-dead-connections", nullptr);
  }

  return rv;
}
