/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "stdlib.h"
#include "plstr.h"
#include "plbase64.h"

#include "nsMemory.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsThreadUtils.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIServiceManager.h"
#include "nsITokenPasswordDialogs.h"

#include "nsISecretDecoderRing.h"
#include "nsSDR.h"
#include "nsNSSComponent.h"
#include "nsNSSShutDown.h"
#include "ScopedNSSTypes.h"

#include "pk11func.h"
#include "pk11sdr.h" // For PK11SDR_Encrypt, PK11SDR_Decrypt

#include "ssl.h" // For SSL_ClearSessionCache

using namespace mozilla;

// Standard ISupports implementation
// NOTE: Should these be the thread-safe versions?
NS_IMPL_ISUPPORTS2(nsSecretDecoderRing, nsISecretDecoderRing, nsISecretDecoderRingConfig)

// nsSecretDecoderRing constructor
nsSecretDecoderRing::nsSecretDecoderRing()
{
  // initialize superclass
}

// nsSecretDecoderRing destructor
nsSecretDecoderRing::~nsSecretDecoderRing()
{
}

/* [noscript] long encrypt (in buffer data, in long dataLen, out buffer result); */
NS_IMETHODIMP nsSecretDecoderRing::
Encrypt(unsigned char * data, int32_t dataLen, unsigned char * *result, int32_t *_retval)
{
  nsNSSShutDownPreventionLock locker;
  nsresult rv = NS_OK;
  ScopedPK11SlotInfo slot;
  SECItem keyid;
  SECItem request;
  SECItem reply;
  SECStatus s;
  nsCOMPtr<nsIInterfaceRequestor> ctx = new PipUIContext();

  slot = PK11_GetInternalKeySlot();
  if (!slot) { rv = NS_ERROR_NOT_AVAILABLE; goto loser; }

  /* Make sure token is initialized. */
  rv = setPassword(slot, ctx);
  if (NS_FAILED(rv))
    goto loser;

  /* Force authentication */
  s = PK11_Authenticate(slot, true, ctx);
  if (s != SECSuccess) { rv = NS_ERROR_FAILURE; goto loser; }

  /* Use default key id */
  keyid.data = 0;
  keyid.len = 0;
  request.data = data;
  request.len = dataLen;
  reply.data = 0;
  reply.len = 0;
  s= PK11SDR_Encrypt(&keyid, &request, &reply, ctx);
  if (s != SECSuccess) { rv = NS_ERROR_FAILURE; goto loser; }

  *result = reply.data;
  *_retval = reply.len;

loser:
  return rv;
}

/* [noscript] long decrypt (in buffer data, in long dataLen, out buffer result); */
NS_IMETHODIMP nsSecretDecoderRing::
Decrypt(unsigned char * data, int32_t dataLen, unsigned char * *result, int32_t *_retval)
{
  nsNSSShutDownPreventionLock locker;
  nsresult rv = NS_OK;
  ScopedPK11SlotInfo slot;
  SECStatus s;
  SECItem request;
  SECItem reply;
  nsCOMPtr<nsIInterfaceRequestor> ctx = new PipUIContext();

  *result = 0;
  *_retval = 0;

  /* Find token with SDR key */
  slot = PK11_GetInternalKeySlot();
  if (!slot) { rv = NS_ERROR_NOT_AVAILABLE; goto loser; }

  /* Force authentication */
  if (PK11_Authenticate(slot, true, ctx) != SECSuccess)
  {
    rv = NS_ERROR_NOT_AVAILABLE;
    goto loser;
  }

  request.data = data;
  request.len = dataLen;
  reply.data = 0;
  reply.len = 0;
  s = PK11SDR_Decrypt(&request, &reply, ctx);
  if (s != SECSuccess) { rv = NS_ERROR_FAILURE; goto loser; }

  *result = reply.data;
  *_retval = reply.len;

loser:
  return rv;
}

/* string encryptString (in string text); */
NS_IMETHODIMP nsSecretDecoderRing::
EncryptString(const char *text, char **_retval)
{
  nsNSSShutDownPreventionLock locker;
  nsresult rv = NS_OK;
  unsigned char *encrypted = 0;
  int32_t eLen;

  if (!text || !_retval) {
    rv = NS_ERROR_INVALID_POINTER;
    goto loser;
  }

  rv = Encrypt((unsigned char *)text, PL_strlen(text), &encrypted, &eLen);
  if (rv != NS_OK) { goto loser; }

  rv = encode(encrypted, eLen, _retval);

loser:
  if (encrypted) PORT_Free(encrypted);

  return rv;
}

/* string decryptString (in string crypt); */
NS_IMETHODIMP nsSecretDecoderRing::
DecryptString(const char *crypt, char **_retval)
{
  nsNSSShutDownPreventionLock locker;
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
  r = (char *)nsMemory::Alloc(decryptedLen+1);
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

/* void changePassword(); */
NS_IMETHODIMP nsSecretDecoderRing::
ChangePassword()
{
  nsNSSShutDownPreventionLock locker;
  nsresult rv;
  ScopedPK11SlotInfo slot(PK11_GetInternalKeySlot());
  if (!slot) return NS_ERROR_NOT_AVAILABLE;

  /* Convert UTF8 token name to UCS2 */
  NS_ConvertUTF8toUTF16 tokenName(PK11_GetTokenName(slot));

  /* Get the set password dialog handler imlementation */
  nsCOMPtr<nsITokenPasswordDialogs> dialogs;

  rv = getNSSDialogs(getter_AddRefs(dialogs),
                     NS_GET_IID(nsITokenPasswordDialogs),
                     NS_TOKENPASSWORDSDIALOG_CONTRACTID);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIInterfaceRequestor> ctx = new PipUIContext();
  bool canceled;

  {
    nsPSMUITracker tracker;
    if (tracker.isUIForbidden()) {
      rv = NS_ERROR_NOT_AVAILABLE;
    }
    else {
      rv = dialogs->SetPassword(ctx, tokenName.get(), &canceled);
    }
  }

  /* canceled is ignored */

  return rv;
}

static NS_DEFINE_CID(kNSSComponentCID, NS_NSSCOMPONENT_CID);

NS_IMETHODIMP nsSecretDecoderRing::
Logout()
{
  nsresult rv;
  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));
  if (NS_FAILED(rv))
    return rv;

  {
    nsNSSShutDownPreventionLock locker;
    PK11_LogoutAll();
    SSL_ClearSessionCache();
  }

  return NS_OK;
}

NS_IMETHODIMP nsSecretDecoderRing::
LogoutAndTeardown()
{
  nsresult rv;
  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));
  if (NS_FAILED(rv))
    return rv;

  {
    nsNSSShutDownPreventionLock locker;
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

/* void setWindow(in nsISupports w); */
NS_IMETHODIMP nsSecretDecoderRing::
SetWindow(nsISupports *w)
{
  return NS_OK;
}

// Support routines

nsresult nsSecretDecoderRing::
encode(const unsigned char *data, int32_t dataLen, char **_retval)
{
  nsresult rv = NS_OK;

  char *result = PL_Base64Encode((const char *)data, dataLen, nullptr);
  if (!result) { rv = NS_ERROR_OUT_OF_MEMORY; goto loser; }

  *_retval = NS_strdup(result);
  PR_DELETE(result);
  if (!*_retval) { rv = NS_ERROR_OUT_OF_MEMORY; goto loser; }

loser:
  return rv;
}

nsresult nsSecretDecoderRing::
decode(const char *data, unsigned char **result, int32_t * _retval)
{
  nsresult rv = NS_OK;
  uint32_t len = PL_strlen(data);
  int adjust = 0;

  /* Compute length adjustment */
  if (data[len-1] == '=') {
    adjust++;
    if (data[len-2] == '=') adjust++;
  }

  *result = (unsigned char *)PL_Base64Decode(data, len, nullptr);
  if (!*result) { rv = NS_ERROR_ILLEGAL_VALUE; goto loser; }

  *_retval = (len*3)/4 - adjust;

loser:
  return rv;
}
