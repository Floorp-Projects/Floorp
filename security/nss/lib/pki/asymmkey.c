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
static const char CVS_ID[] = "@(#) $RCSfile: asymmkey.c,v $ $Revision: 1.1 $ $Date: 2001/07/19 20:41:36 $ $Name:  $";
#endif /* DEBUG */

#ifndef NSSPKI_H
#include "nsspki.h"
#endif /* NSSPKI_H */

PRStatus
NSSPrivateKey_Destroy
(
  NSSPrivateKey *vk
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

PRStatus
NSSPrivateKey_DeleteStoredObject
(
  NSSPrivateKey *vk,
  NSSCallback *uhh
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

PRUint32
NSSPrivateKey_GetSignatureLength
(
  NSSPrivateKey *vk
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return -1;
}

PRUint32
NSSPrivateKey_GetPrivateModulusLength
(
  NSSPrivateKey *vk
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return -1;
}

PRBool
NSSPrivateKey_IsStillPresent
(
  NSSPrivateKey *vk,
  PRStatus *statusOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FALSE;
}

NSSItem *
NSSPrivateKey_Encode
(
  NSSPrivateKey *vk,
  NSSAlgorithmAndParameters *ap,
  NSSItem *passwordOpt, /* NULL will cause a callback; "" for no password */
  NSSCallback *uhhOpt,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSTrustDomain *
NSSPrivateKey_GetTrustDomain
(
  NSSPrivateKey *vk,
  PRStatus *statusOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSToken *
NSSPrivateKey_GetToken
(
  NSSPrivateKey *vk
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSSlot *
NSSPrivateKey_GetSlot
(
  NSSPrivateKey *vk
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSModule *
NSSPrivateKey_GetModule
(
  NSSPrivateKey *vk
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSItem *
NSSPrivateKey_Decrypt
(
  NSSPrivateKey *vk,
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
NSSPrivateKey_Sign
(
  NSSPrivateKey *vk,
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
NSSPrivateKey_SignRecover
(
  NSSPrivateKey *vk,
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

NSSSymmetricKey *
NSSPrivateKey_UnwrapSymmetricKey
(
  NSSPrivateKey *vk,
  NSSAlgorithmAndParameters *apOpt,
  NSSItem *wrappedKey,
  NSSCallback *uhh
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSSymmetricKey *
NSSPrivateKey_DeriveSymmetricKey
(
  NSSPrivateKey *vk,
  NSSPublicKey *bk,
  NSSAlgorithmAndParameters *apOpt,
  NSSOID *target,
  PRUint32 keySizeOpt, /* zero for best allowed */
  NSSOperations operations,
  NSSCallback *uhh
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSPublicKey *
NSSPrivateKey_FindPublicKey
(
  NSSPrivateKey *vk
  /* { don't need the callback here, right? } */
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSCryptoContext *
NSSPrivateKey_CreateCryptoContext
(
  NSSPrivateKey *vk,
  NSSAlgorithmAndParameters *apOpt,
  NSSCallback *uhh
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSCertificate **
NSSPrivateKey_FindCertificates
(
  NSSPrivateKey *vk,
  NSSCertificate *rvOpt[],
  PRUint32 maximumOpt, /* 0 for no max */
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSCertificate *
NSSPrivateKey_FindBestCertificate
(
  NSSPrivateKey *vk,
  NSSTime *timeOpt,
  NSSUsage *usageOpt,
  NSSPolicies *policiesOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

PRStatus
NSSPublicKey_Destroy
(
  NSSPublicKey *bk
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

PRStatus
NSSPublicKey_DeleteStoredObject
(
  NSSPublicKey *bk,
  NSSCallback *uhh
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSSItem *
NSSPublicKey_Encode
(
  NSSPublicKey *bk,
  NSSAlgorithmAndParameters *ap,
  NSSCallback *uhhOpt,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSTrustDomain *
NSSPublicKey_GetTrustDomain
(
  NSSPublicKey *bk,
  PRStatus *statusOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSToken *
NSSPublicKey_GetToken
(
  NSSPublicKey *bk,
  PRStatus *statusOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSSlot *
NSSPublicKey_GetSlot
(
  NSSPublicKey *bk,
  PRStatus *statusOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSModule *
NSSPublicKey_GetModule
(
  NSSPublicKey *bk,
  PRStatus *statusOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSItem *
NSSPublicKey_Encrypt
(
  NSSPublicKey *bk,
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
NSSPublicKey_Verify
(
  NSSPublicKey *bk,
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
NSSPublicKey_VerifyRecover
(
  NSSPublicKey *bk,
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
NSSPublicKey_WrapSymmetricKey
(
  NSSPublicKey *bk,
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

NSSCryptoContext *
NSSPublicKey_CreateCryptoContext
(
  NSSPublicKey *bk,
  NSSAlgorithmAndParameters *apOpt,
  NSSCallback *uhh
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSCertificate **
NSSPublicKey_FindCertificates
(
  NSSPublicKey *bk,
  NSSCertificate *rvOpt[],
  PRUint32 maximumOpt, /* 0 for no max */
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSCertificate *
NSSPublicKey_FindBestCertificate
(
  NSSPublicKey *bk,
  NSSTime *timeOpt,
  NSSUsage *usageOpt,
  NSSPolicies *policiesOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSPrivateKey *
NSSPublicKey_FindPrivateKey
(
  NSSPublicKey *bk,
  NSSCallback *uhh
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

