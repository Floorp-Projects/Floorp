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
#include "nsCOMPtr.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIObserverService.h"
#include "nsIServiceManager.h"
#include "nsITokenPasswordDialogs.h"
#include "nsNSSComponent.h"
#include "nsNSSHelper.h"
#include "pk11func.h"
#include "pk11sdr.h" // For PK11SDR_Encrypt, PK11SDR_Decrypt
#include "ssl.h" // For SSL_ClearSessionCache

using namespace mozilla;

// NOTE: Should these be the thread-safe versions?
NS_IMPL_ISUPPORTS(SecretDecoderRing, nsISecretDecoderRing)

SecretDecoderRing::SecretDecoderRing()
{
}

SecretDecoderRing::~SecretDecoderRing()
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return;
  }

  shutdown(ShutdownCalledFrom::Object);
}

nsresult
SecretDecoderRing::Encrypt(const nsACString& data, /*out*/ nsACString& result)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  UniquePK11SlotInfo slot(PK11_GetInternalKeySlot());
  if (!slot) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  /* Make sure token is initialized. */
  nsCOMPtr<nsIInterfaceRequestor> ctx = new PipUIContext();
  nsresult rv = setPassword(slot.get(), ctx, locker);
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
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

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
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  UniquePK11SlotInfo slot(PK11_GetInternalKeySlot());
  if (!slot) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  NS_ConvertUTF8toUTF16 tokenName(PK11_GetTokenName(slot.get()));

  nsCOMPtr<nsITokenPasswordDialogs> dialogs;
  nsresult rv = getNSSDialogs(getter_AddRefs(dialogs),
                              NS_GET_IID(nsITokenPasswordDialogs),
                              NS_TOKENPASSWORDSDIALOG_CONTRACTID);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIInterfaceRequestor> ctx = new PipUIContext();
  bool canceled; // Ignored
  return dialogs->SetPassword(ctx, tokenName.get(), &canceled);
}

NS_IMETHODIMP
SecretDecoderRing::Logout()
{
  static NS_DEFINE_CID(kNSSComponentCID, NS_NSSCOMPONENT_CID);

  nsresult rv;
  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));
  if (NS_FAILED(rv))
    return rv;

  {
    nsNSSShutDownPreventionLock locker;
    if (isAlreadyShutDown()) {
      return NS_ERROR_NOT_AVAILABLE;
    }

    PK11_LogoutAll();
    SSL_ClearSessionCache();
  }

  return NS_OK;
}

NS_IMETHODIMP
SecretDecoderRing::LogoutAndTeardown()
{
  static NS_DEFINE_CID(kNSSComponentCID, NS_NSSCOMPONENT_CID);

  nsresult rv;
  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));
  if (NS_FAILED(rv))
    return rv;

  {
    nsNSSShutDownPreventionLock locker;
    if (isAlreadyShutDown()) {
      return NS_ERROR_NOT_AVAILABLE;
    }

    PK11_LogoutAll();
    SSL_ClearSessionCache();
  }

  rv = nssComponent->LogoutAuthenticatedPK11();

  // After we just logged out, we need to prune dead connections to make
  // sure that all connections that should be stopped, are stopped. See
  // bug 517584.
  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os)
    os->NotifyObservers(nullptr, "net:prune-dead-connections", nullptr);

  return rv;
}
