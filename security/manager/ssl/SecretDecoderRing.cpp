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
#include "nsNetCID.h"
#include "nsPK11TokenDB.h"
#include "pk11func.h"
#include "pk11sdr.h"  // For PK11SDR_Encrypt, PK11SDR_Decrypt
#include "ssl.h"      // For SSL_ClearSessionCache

static mozilla::LazyLogModule gSDRLog("sdrlog");

using namespace mozilla;
using dom::Promise;

NS_IMPL_ISUPPORTS(SecretDecoderRing, nsISecretDecoderRing)

void BackgroundSdrEncryptStrings(const nsTArray<nsCString>& plaintexts,
                                 RefPtr<Promise>& aPromise) {
  nsCOMPtr<nsISecretDecoderRing> sdrService =
      do_GetService(NS_SECRETDECODERRING_CONTRACTID);
  nsTArray<nsString> cipherTexts(plaintexts.Length());

  nsresult rv = NS_ERROR_FAILURE;
  for (const auto& plaintext : plaintexts) {
    nsCString cipherText;
    rv = sdrService->EncryptString(plaintext, cipherText);

    if (NS_WARN_IF(NS_FAILED(rv))) {
      break;
    }

    cipherTexts.AppendElement(NS_ConvertASCIItoUTF16(cipherText));
  }

  nsCOMPtr<nsIRunnable> runnable(
      NS_NewRunnableFunction("BackgroundSdrEncryptStringsResolve",
                             [rv, aPromise = std::move(aPromise),
                              cipherTexts = std::move(cipherTexts)]() {
                               if (NS_FAILED(rv)) {
                                 aPromise->MaybeReject(rv);
                               } else {
                                 aPromise->MaybeResolve(cipherTexts);
                               }
                             }));
  NS_DispatchToMainThread(runnable.forget());
}

void BackgroundSdrDecryptStrings(const nsTArray<nsCString>& encryptedStrings,
                                 RefPtr<Promise>& aPromise) {
  nsCOMPtr<nsISecretDecoderRing> sdrService =
      do_GetService(NS_SECRETDECODERRING_CONTRACTID);
  nsTArray<nsString> plainTexts(encryptedStrings.Length());

  for (const auto& encryptedString : encryptedStrings) {
    nsCString plainText;
    nsresult rv = sdrService->DecryptString(encryptedString, plainText);

    if (NS_FAILED(rv)) {
      // Callers of the batch decryptMany in crypto-SDR.js
      // assume there will be equal number usernames and passwords.
      // If decryption fails, use an empty string to keep this assumption true.
      plainTexts.AppendElement(nullptr);
      MOZ_LOG(gSDRLog, LogLevel::Warning,
              ("Couldn't decrypt string: %s", encryptedString.get()));
      continue;
    }

    plainTexts.AppendElement(NS_ConvertUTF8toUTF16(plainText));
  }

  nsCOMPtr<nsIRunnable> runnable(NS_NewRunnableFunction(
      "BackgroundSdrDecryptStringsResolve",
      [aPromise = std::move(aPromise), plainTexts = std::move(plainTexts)]() {
        aPromise->MaybeResolve(plainTexts);
      }));
  NS_DispatchToMainThread(runnable.forget());
}

nsresult SecretDecoderRing::Encrypt(const nsACString& data,
                                    /*out*/ nsACString& result) {
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

nsresult SecretDecoderRing::Decrypt(const nsACString& data,
                                    /*out*/ nsACString& result) {
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
                                 /*out*/ nsACString& encryptedBase64Text) {
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
SecretDecoderRing::AsyncEncryptStrings(const nsTArray<nsCString>& plaintexts,
                                       JSContext* aCx, Promise** aPromise) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG(!plaintexts.IsEmpty());
  NS_ENSURE_ARG_POINTER(aCx);
  NS_ENSURE_ARG_POINTER(aPromise);

  nsIGlobalObject* globalObject = xpc::CurrentNativeGlobal(aCx);
  if (NS_WARN_IF(!globalObject)) {
    return NS_ERROR_UNEXPECTED;
  }

  ErrorResult result;
  RefPtr<Promise> promise = Promise::Create(globalObject, result);
  if (NS_WARN_IF(result.Failed())) {
    return result.StealNSResult();
  }

  // plaintexts are already expected to be UTF-8.
  nsCOMPtr<nsIRunnable> runnable(NS_NewRunnableFunction(
      "BackgroundSdrEncryptStrings", [promise, plaintexts]() mutable {
        BackgroundSdrEncryptStrings(plaintexts, promise);
      }));

  nsCOMPtr<nsIEventTarget> target(
      do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID));
  if (!target) {
    return NS_ERROR_FAILURE;
  }
  nsresult rv = target->Dispatch(runnable, NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  promise.forget(aPromise);
  return NS_OK;
}

NS_IMETHODIMP
SecretDecoderRing::DecryptString(const nsACString& encryptedBase64Text,
                                 /*out*/ nsACString& decryptedText) {
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
SecretDecoderRing::AsyncDecryptStrings(
    const nsTArray<nsCString>& encryptedStrings, JSContext* aCx,
    Promise** aPromise) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG(!encryptedStrings.IsEmpty());
  NS_ENSURE_ARG_POINTER(aCx);
  NS_ENSURE_ARG_POINTER(aPromise);

  nsIGlobalObject* globalObject = xpc::CurrentNativeGlobal(aCx);
  if (NS_WARN_IF(!globalObject)) {
    return NS_ERROR_UNEXPECTED;
  }

  ErrorResult result;
  RefPtr<Promise> promise = Promise::Create(globalObject, result);
  if (NS_WARN_IF(result.Failed())) {
    return result.StealNSResult();
  }

  // encryptedStrings are expected to be base64.
  nsCOMPtr<nsIRunnable> runnable(NS_NewRunnableFunction(
      "BackgroundSdrDecryptStrings", [promise, encryptedStrings]() mutable {
        BackgroundSdrDecryptStrings(encryptedStrings, promise);
      }));

  nsCOMPtr<nsIEventTarget> target(
      do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID));
  if (!target) {
    return NS_ERROR_FAILURE;
  }
  nsresult rv = target->Dispatch(runnable, NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  promise.forget(aPromise);
  return NS_OK;
}

NS_IMETHODIMP
SecretDecoderRing::ChangePassword() {
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
  bool canceled;  // Ignored
  return dialogs->SetPassword(ctx, token, &canceled);
}

NS_IMETHODIMP
SecretDecoderRing::Logout() {
  PK11_LogoutAll();
  SSL_ClearSessionCache();
  return NS_OK;
}

NS_IMETHODIMP
SecretDecoderRing::LogoutAndTeardown() {
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
