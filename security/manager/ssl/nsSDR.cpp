/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "stdlib.h"
#include "plstr.h"
#include "plbase64.h"

#include "mozilla/Base64.h"
#include "mozilla/Services.h"
#include "nsMemory.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsThreadUtils.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIObserverService.h"
#include "nsIServiceManager.h"
#include "nsITokenPasswordDialogs.h"

#include "nsISecretDecoderRing.h"
#include "nsCRT.h"
#include "nsSDR.h"
#include "nsNSSComponent.h"
#include "nsNSSHelper.h"
#include "nsNSSShutDown.h"
#include "ScopedNSSTypes.h"

#include "pk11func.h"
#include "pk11sdr.h" // For PK11SDR_Encrypt, PK11SDR_Decrypt

#include "ssl.h" // For SSL_ClearSessionCache

using namespace mozilla;

// Standard ISupports implementation
// NOTE: Should these be the thread-safe versions?
NS_IMPL_ISUPPORTS(nsSecretDecoderRing, nsISecretDecoderRing, nsISecretDecoderRingConfig)

// nsSecretDecoderRing constructor
nsSecretDecoderRing::nsSecretDecoderRing()
{
  // initialize superclass
}

// nsSecretDecoderRing destructor
nsSecretDecoderRing::~nsSecretDecoderRing()
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return;
  }

  shutdown(calledFromObject);
}

NS_IMETHODIMP
nsSecretDecoderRing::Encrypt(unsigned char* data, int32_t dataLen,
                             unsigned char** result, int32_t* _retval)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCOMPtr<nsIInterfaceRequestor> ctx = new PipUIContext();

  UniquePK11SlotInfo slot(PK11_GetInternalKeySlot());
  if (!slot) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  /* Make sure token is initialized. */
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
  request.data = data;
  request.len = dataLen;
  SECItem reply;
  reply.data = nullptr;
  reply.len = 0;
  if (PK11SDR_Encrypt(&keyid, &request, &reply, ctx) != SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  *result = reply.data;
  *_retval = reply.len;

  return NS_OK;
}

NS_IMETHODIMP
nsSecretDecoderRing::Decrypt(unsigned char* data, int32_t dataLen,
                             unsigned char** result, int32_t* _retval)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCOMPtr<nsIInterfaceRequestor> ctx = new PipUIContext();

  *result = nullptr;
  *_retval = 0;

  /* Find token with SDR key */
  UniquePK11SlotInfo slot(PK11_GetInternalKeySlot());
  if (!slot) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  /* Force authentication */
  if (PK11_Authenticate(slot.get(), true, ctx) != SECSuccess) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  SECItem request;
  request.data = data;
  request.len = dataLen;
  SECItem reply;
  reply.data = nullptr;
  reply.len = 0;
  if (PK11SDR_Decrypt(&request, &reply, ctx) != SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  *result = reply.data;
  *_retval = reply.len;

  return NS_OK;
}

NS_IMETHODIMP
nsSecretDecoderRing::EncryptString(const char* text, char** _retval)
{
  nsresult rv = NS_OK;
  unsigned char *encrypted = 0;
  int32_t eLen;

  if (!text || !_retval) {
    rv = NS_ERROR_INVALID_POINTER;
    goto loser;
  }

  rv = Encrypt((unsigned char *)text, strlen(text), &encrypted, &eLen);
  if (rv != NS_OK) { goto loser; }

  rv = encode(encrypted, eLen, _retval);

loser:
  if (encrypted) PORT_Free(encrypted);

  return rv;
}

NS_IMETHODIMP
nsSecretDecoderRing::DecryptString(const char* crypt, char** _retval)
{
  nsresult rv = NS_OK;
  char *r = 0;
  unsigned char *decoded = 0;
  int32_t decodedLen;
  unsigned char *decrypted = 0;
  int32_t decryptedLen;

  if (!crypt || !_retval) {
    rv = NS_ERROR_INVALID_POINTER;
    goto loser;
  }

  rv = decode(crypt, &decoded, &decodedLen);
  if (rv != NS_OK) goto loser;

  rv = Decrypt(decoded, decodedLen, &decrypted, &decryptedLen);
  if (rv != NS_OK) goto loser;

  // Convert to NUL-terminated string
  r = (char *)moz_xmalloc(decryptedLen+1);
  if (!r) { rv = NS_ERROR_OUT_OF_MEMORY; goto loser; }

  memcpy(r, decrypted, decryptedLen);
  r[decryptedLen] = 0;

  *_retval = r;
  r = 0;

loser:
  if (decrypted) PORT_Free(decrypted);
  if (decoded) PR_DELETE(decoded);

  return rv;
}

NS_IMETHODIMP
nsSecretDecoderRing::ChangePassword()
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
nsSecretDecoderRing::Logout()
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
nsSecretDecoderRing::LogoutAndTeardown()
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

NS_IMETHODIMP
nsSecretDecoderRing::SetWindow(nsISupports*)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

// Support routines

nsresult
nsSecretDecoderRing::encode(const unsigned char* data, int32_t dataLen,
                            char** _retval)
{
  if (dataLen < 0) {
    return NS_ERROR_INVALID_ARG;
  }
  return Base64Encode(BitwiseCast<const char*>(data),
                      AssertedCast<uint32_t>(dataLen), _retval);
}

nsresult
nsSecretDecoderRing::decode(const char* data, unsigned char** result,
                            int32_t* _retval)
{
  uint32_t len = strlen(data);
  int adjust = 0;

  /* Compute length adjustment */
  if (data[len-1] == '=') {
    adjust++;
    if (data[len - 2] == '=') {
      adjust++;
    }
  }

  *result = (unsigned char *)PL_Base64Decode(data, len, nullptr);
  if (!*result) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  *_retval = (len*3)/4 - adjust;

  return NS_OK;
}
