/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef DEBUG
static const char CVS_ID[] = "@(#) $RCSfile: symmkey.c,v $ $Revision: 1.7 $ $Date: 2012/04/25 14:50:07 $";
#endif /* DEBUG */

#ifndef NSSPKI_H
#include "nsspki.h"
#endif /* NSSPKI_H */

extern const NSSError NSS_ERROR_NOT_FOUND;

NSS_IMPLEMENT PRStatus
NSSSymmetricKey_Destroy (
  NSSSymmetricKey *mk
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSS_IMPLEMENT PRStatus
NSSSymmetricKey_DeleteStoredObject (
  NSSSymmetricKey *mk,
  NSSCallback *uhh
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSS_IMPLEMENT PRUint32
NSSSymmetricKey_GetKeyLength (
  NSSSymmetricKey *mk
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return -1;
}

NSS_IMPLEMENT PRUint32
NSSSymmetricKey_GetKeyStrength (
  NSSSymmetricKey *mk
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return -1;
}

NSS_IMPLEMENT PRStatus
NSSSymmetricKey_IsStillPresent (
  NSSSymmetricKey *mk
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSS_IMPLEMENT NSSTrustDomain *
NSSSymmetricKey_GetTrustDomain (
  NSSSymmetricKey *mk,
  PRStatus *statusOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSToken *
NSSSymmetricKey_GetToken (
  NSSSymmetricKey *mk,
  PRStatus *statusOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSSlot *
NSSSymmetricKey_GetSlot (
  NSSSymmetricKey *mk,
  PRStatus *statusOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSModule *
NSSSymmetricKey_GetModule (
  NSSSymmetricKey *mk,
  PRStatus *statusOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSItem *
NSSSymmetricKey_Encrypt (
  NSSSymmetricKey *mk,
  NSSAlgorithmAndParameters *apOpt,
  NSSItem *data,
  NSSCallback *uhh,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSItem *
NSSSymmetricKey_Decrypt (
  NSSSymmetricKey *mk,
  NSSAlgorithmAndParameters *apOpt,
  NSSItem *encryptedData,
  NSSCallback *uhh,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSItem *
NSSSymmetricKey_Sign (
  NSSSymmetricKey *mk,
  NSSAlgorithmAndParameters *apOpt,
  NSSItem *data,
  NSSCallback *uhh,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSItem *
NSSSymmetricKey_SignRecover (
  NSSSymmetricKey *mk,
  NSSAlgorithmAndParameters *apOpt,
  NSSItem *data,
  NSSCallback *uhh,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT PRStatus
NSSSymmetricKey_Verify (
  NSSSymmetricKey *mk,
  NSSAlgorithmAndParameters *apOpt,
  NSSItem *data,
  NSSItem *signature,
  NSSCallback *uhh
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSS_IMPLEMENT NSSItem *
NSSSymmetricKey_VerifyRecover (
  NSSSymmetricKey *mk,
  NSSAlgorithmAndParameters *apOpt,
  NSSItem *signature,
  NSSCallback *uhh,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSItem *
NSSSymmetricKey_WrapSymmetricKey (
  NSSSymmetricKey *wrappingKey,
  NSSAlgorithmAndParameters *apOpt,
  NSSSymmetricKey *keyToWrap,
  NSSCallback *uhh,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSItem *
NSSSymmetricKey_WrapPrivateKey (
  NSSSymmetricKey *wrappingKey,
  NSSAlgorithmAndParameters *apOpt,
  NSSPrivateKey *keyToWrap,
  NSSCallback *uhh,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSSymmetricKey *
NSSSymmetricKey_UnwrapSymmetricKey (
  NSSSymmetricKey *wrappingKey,
  NSSAlgorithmAndParameters *apOpt,
  NSSItem *wrappedKey,
  NSSOID *target,
  PRUint32 keySizeOpt,
  NSSOperations operations,
  NSSCallback *uhh
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSPrivateKey *
NSSSymmetricKey_UnwrapPrivateKey (
  NSSSymmetricKey *wrappingKey,
  NSSAlgorithmAndParameters *apOpt,
  NSSItem *wrappedKey,
  NSSUTF8 *labelOpt,
  NSSItem *keyIDOpt,
  PRBool persistant,
  PRBool sensitive,
  NSSToken *destinationOpt,
  NSSCallback *uhh
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSSymmetricKey *
NSSSymmetricKey_DeriveSymmetricKey (
  NSSSymmetricKey *originalKey,
  NSSAlgorithmAndParameters *apOpt,
  NSSOID *target,
  PRUint32 keySizeOpt,
  NSSOperations operations,
  NSSCallback *uhh
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSCryptoContext *
NSSSymmetricKey_CreateCryptoContext (
  NSSSymmetricKey *mk,
  NSSAlgorithmAndParameters *apOpt,
  NSSCallback *uhh
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

