/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#ifdef DEBUG
static const char CVS_ID[] = "@(#) $RCSfile: symmkey.c,v $ $Revision: 1.1 $ $Date: 2001/07/19 20:41:38 $ $Name:  $";
#endif /* DEBUG */

#ifndef NSSPKI_H
#include "nsspki.h"
#endif /* NSSPKI_H */

PRStatus
NSSSymmetricKey_Destroy
(
  NSSSymmetricKey *mk
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

PRStatus
NSSSymmetricKey_DeleteStoredObject
(
  NSSSymmetricKey *mk,
  NSSCallback *uhh
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

PRUint32
NSSSymmetricKey_GetKeyLength
(
  NSSSymmetricKey *mk
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return -1;
}

PRUint32
NSSSymmetricKey_GetKeyStrength
(
  NSSSymmetricKey *mk
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return -1;
}

PRStatus
NSSSymmetricKey_IsStillPresent
(
  NSSSymmetricKey *mk
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSSTrustDomain *
NSSSymmetricKey_GetTrustDomain
(
  NSSSymmetricKey *mk,
  PRStatus *statusOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSToken *
NSSSymmetricKey_GetToken
(
  NSSSymmetricKey *mk,
  PRStatus *statusOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSSlot *
NSSSymmetricKey_GetSlot
(
  NSSSymmetricKey *mk,
  PRStatus *statusOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSModule *
NSSSymmetricKey_GetModule
(
  NSSSymmetricKey *mk,
  PRStatus *statusOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSItem *
NSSSymmetricKey_Encrypt
(
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

NSSItem *
NSSSymmetricKey_Decrypt
(
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

NSSItem *
NSSSymmetricKey_Sign
(
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

NSSItem *
NSSSymmetricKey_SignRecover
(
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

PRStatus
NSSSymmetricKey_Verify
(
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

NSSItem *
NSSSymmetricKey_VerifyRecover
(
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

NSSItem *
NSSSymmetricKey_WrapSymmetricKey
(
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

NSSItem *
NSSSymmetricKey_WrapPrivateKey
(
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

NSSSymmetricKey *
NSSSymmetricKey_UnwrapSymmetricKey
(
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

NSSPrivateKey *
NSSSymmetricKey_UnwrapPrivateKey
(
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

NSSSymmetricKey *
NSSSymmetricKey_DeriveSymmetricKey
(
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

NSSCryptoContext *
NSSSymmetricKey_CreateCryptoContext
(
  NSSSymmetricKey *mk,
  NSSAlgorithmAndParameters *apOpt,
  NSSCallback *uhh
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

