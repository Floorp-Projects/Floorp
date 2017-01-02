/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * This file implements the Symkey wrapper and the PKCS context
 * Interfaces.
 */

#include "seccomon.h"
#include "secmod.h"
#include "nssilock.h"
#include "secmodi.h"
#include "secmodti.h"
#include "pkcs11.h"
#include "pk11func.h"
#include "secitem.h"
#include "key.h"
#include "secasn1.h"
#include "sechash.h"
#include "cert.h"
#include "secerr.h"

/*
 * find an RSA public key on a card
 */
static CK_OBJECT_HANDLE
pk11_FindRSAPubKey(PK11SlotInfo *slot)
{
    CK_KEY_TYPE key_type = CKK_RSA;
    CK_OBJECT_CLASS class_type = CKO_PUBLIC_KEY;
    CK_ATTRIBUTE theTemplate[2];
    int template_count = sizeof(theTemplate) / sizeof(theTemplate[0]);
    CK_ATTRIBUTE *attrs = theTemplate;

    PK11_SETATTRS(attrs, CKA_CLASS, &class_type, sizeof(class_type));
    attrs++;
    PK11_SETATTRS(attrs, CKA_KEY_TYPE, &key_type, sizeof(key_type));
    attrs++;
    template_count = attrs - theTemplate;
    PR_ASSERT(template_count <= sizeof(theTemplate) / sizeof(CK_ATTRIBUTE));

    return pk11_FindObjectByTemplate(slot, theTemplate, template_count);
}

PK11SymKey *
pk11_KeyExchange(PK11SlotInfo *slot, CK_MECHANISM_TYPE type,
                 CK_ATTRIBUTE_TYPE operation, CK_FLAGS flags,
                 PRBool isPerm, PK11SymKey *symKey)
{
    PK11SymKey *newSymKey = NULL;
    SECStatus rv;
    /* performance improvement can go here --- use a generated key at startup
     * to generate a per token wrapping key. If it exists, use it, otherwise
     * do a full key exchange. */

    /* find a common Key Exchange algorithm */
    /* RSA */
    if (PK11_DoesMechanism(symKey->slot, CKM_RSA_PKCS) &&
        PK11_DoesMechanism(slot, CKM_RSA_PKCS)) {
        CK_OBJECT_HANDLE pubKeyHandle = CK_INVALID_HANDLE;
        CK_OBJECT_HANDLE privKeyHandle = CK_INVALID_HANDLE;
        SECKEYPublicKey *pubKey = NULL;
        SECKEYPrivateKey *privKey = NULL;
        SECItem wrapData;
        unsigned int symKeyLength = PK11_GetKeyLength(symKey);

        wrapData.data = NULL;

        /* find RSA Public Key on target */
        pubKeyHandle = pk11_FindRSAPubKey(slot);
        if (pubKeyHandle != CK_INVALID_HANDLE) {
            privKeyHandle = PK11_MatchItem(slot, pubKeyHandle, CKO_PRIVATE_KEY);
        }

        /* if no key exists, generate a key pair */
        if (privKeyHandle == CK_INVALID_HANDLE) {
            PK11RSAGenParams rsaParams;

            if (symKeyLength > 53) /* bytes */ {
                /* we'd have to generate an RSA key pair > 512 bits long,
                ** and that's too costly.  Don't even try.
                */
                PORT_SetError(SEC_ERROR_CANNOT_MOVE_SENSITIVE_KEY);
                goto rsa_failed;
            }
            rsaParams.keySizeInBits =
                (symKeyLength > 21 || symKeyLength == 0) ? 512 : 256;
            rsaParams.pe = 0x10001;
            privKey = PK11_GenerateKeyPair(slot, CKM_RSA_PKCS_KEY_PAIR_GEN,
                                           &rsaParams, &pubKey, PR_FALSE, PR_TRUE, symKey->cx);
        } else {
            /* if keys exist, build SECKEY data structures for them */
            privKey = PK11_MakePrivKey(slot, nullKey, PR_TRUE, privKeyHandle,
                                       symKey->cx);
            if (privKey != NULL) {
                pubKey = PK11_ExtractPublicKey(slot, rsaKey, pubKeyHandle);
                if (pubKey && pubKey->pkcs11Slot) {
                    PK11_FreeSlot(pubKey->pkcs11Slot);
                    pubKey->pkcs11Slot = NULL;
                    pubKey->pkcs11ID = CK_INVALID_HANDLE;
                }
            }
        }
        if (privKey == NULL)
            goto rsa_failed;
        if (pubKey == NULL)
            goto rsa_failed;

        wrapData.len = SECKEY_PublicKeyStrength(pubKey);
        if (!wrapData.len)
            goto rsa_failed;
        wrapData.data = PORT_Alloc(wrapData.len);
        if (wrapData.data == NULL)
            goto rsa_failed;

        /* now wrap the keys in and out */
        rv = PK11_PubWrapSymKey(CKM_RSA_PKCS, pubKey, symKey, &wrapData);
        if (rv == SECSuccess) {
            newSymKey = PK11_PubUnwrapSymKeyWithFlagsPerm(privKey,
                                                          &wrapData, type, operation,
                                                          symKeyLength, flags, isPerm);
            /* make sure we wound up where we wanted to be! */
            if (newSymKey && newSymKey->slot != slot) {
                PK11_FreeSymKey(newSymKey);
                newSymKey = NULL;
            }
        }
    rsa_failed:
        if (wrapData.data != NULL)
            PORT_Free(wrapData.data);
        if (privKey != NULL)
            SECKEY_DestroyPrivateKey(privKey);
        if (pubKey != NULL)
            SECKEY_DestroyPublicKey(pubKey);

        return newSymKey;
    }
    PORT_SetError(SEC_ERROR_NO_MODULE);
    return NULL;
}
