/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Netscape Security Services for Java.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "_jni/org_mozilla_jss_SecretDecoderRing_KeyManager.h"
#include <nspr.h>
#include <secitem.h>
#include <jss_exceptions.h>
#include <jssutil.h>
#include <pk11func.h>
#include <pk11util.h>
#include <Algorithm.h>

JNIEXPORT void JNICALL
Java_org_mozilla_jss_SecretDecoderRing_KeyManager_generateKeyNative
    (JNIEnv *env, jobject this, jobject tokenObj, jobject algObj,
    jbyteArray keyIDba, jint keySize)
{
    PK11SlotInfo *slot = NULL;
    CK_MECHANISM_TYPE mech;
    PK11SymKey *symk = NULL;
    SECItem *keyID = NULL;

    /* get the slot */
    if( JSS_PK11_getTokenSlotPtr(env, tokenObj, &slot) != PR_SUCCESS ) {
        goto finish;
    }

    if( PK11_Authenticate(slot, PR_TRUE /*load certs*/, NULL /*wincx*/)
        != SECSuccess)
    {
        JSS_throwMsgPrErr(env, TOKEN_EXCEPTION,
            "Failed to login to token");
        goto finish;
    }

    /* get the key ID */
    keyID = JSS_ByteArrayToSECItem(env, keyIDba);
    if( keyID == NULL ) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }

    /* get the algorithm */
    mech = JSS_getPK11MechFromAlg(env, algObj);
    if( mech == CKM_INVALID_MECHANISM) {
        JSS_throwMsgPrErr(env, TOKEN_EXCEPTION, "Failed to find PKCS #11 "
            "mechanism for key generation algorithm");
        goto finish;
    }

    /* generate the key */
    symk = PK11_TokenKeyGen(slot, mech, NULL /*param*/, keySize, keyID,
        PR_TRUE /* isToken */, NULL /*wincx*/);
    if( symk == NULL ) {
        JSS_throwMsgPrErr(env, TOKEN_EXCEPTION,
            "Failed to generate token symmetric key");
        goto finish;
    }


finish:
    if( symk != NULL ) {
        PK11_FreeSymKey(symk);
    }
    if( keyID != NULL ) {
        SECITEM_FreeItem(keyID, PR_TRUE /*freeit*/);
    }
    return;
}

JNIEXPORT jobject JNICALL
Java_org_mozilla_jss_SecretDecoderRing_KeyManager_lookupKeyNative
    (JNIEnv *env, jobject this, jobject tokenObj, jobject algObj,
    jbyteArray keyIDba)
{
    PK11SlotInfo *slot = NULL;
    PK11SymKey *symk = NULL;
    SECItem *keyID = NULL;
    jobject symkObj = NULL;
    CK_MECHANISM_TYPE mech;

    /* get the slot */
    if( JSS_PK11_getTokenSlotPtr(env, tokenObj, &slot) != PR_SUCCESS ) {
        goto finish;
    }

    if( PK11_Authenticate(slot, PR_TRUE /*load certs*/, NULL /*wincx*/)
        != SECSuccess)
    {
        JSS_throwMsgPrErr(env, TOKEN_EXCEPTION,
            "Failed to login to token");
        goto finish;
    }

    /* get the key ID */
    keyID = JSS_ByteArrayToSECItem(env, keyIDba);
    if( keyID == NULL ) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }

    /* get the algorithm */
    mech = JSS_getPK11MechFromAlg(env, algObj);
    if( mech == CKM_INVALID_MECHANISM) {
        JSS_throwMsgPrErr(env, TOKEN_EXCEPTION, "Failed to find PKCS #11 "
            "mechanism for key generation algorithm");
        goto finish;
    }

    symk = PK11_FindFixedKey(slot, mech, keyID, NULL /*wincx*/);
    if( symk != NULL ) {
        symkObj = JSS_PK11_wrapSymKey(env, &symk);
    }

finish:
    if( symk != NULL ) {
        PK11_FreeSymKey(symk);
    }
    if( keyID != NULL ) {
        SECITEM_FreeItem(keyID, PR_TRUE /*freeit*/);
    }
    return symkObj;
}

JNIEXPORT void JNICALL
Java_org_mozilla_jss_SecretDecoderRing_KeyManager_deleteKeyNative
    (JNIEnv *env, jobject this, jobject tokenObj, jobject key)
{
    PK11SlotInfo *slot = NULL;
    PK11SymKey *symk = NULL;
    SECStatus status;

    /* get the slot */
    if( JSS_PK11_getTokenSlotPtr(env, tokenObj, &slot) != PR_SUCCESS ) {
        goto finish;
    }

    if( PK11_Authenticate(slot, PR_TRUE /*load certs*/, NULL /*wincx*/)
        != SECSuccess)
    {
        JSS_throwMsgPrErr(env, TOKEN_EXCEPTION,
            "Failed to login to token");
        goto finish;
    }

    /* get the key pointer */
    if( JSS_PK11_getSymKeyPtr(env, key, &symk) != PR_SUCCESS) {
        goto finish;
    }

    if( PK11_DeleteTokenSymKey(symk) != SECSuccess ) {
        JSS_throwMsgPrErr(env, TOKEN_EXCEPTION,
            "Failed to delete token symmetric key");
        goto finish;
    }

finish:
    /* don't free symk or slot, they are owned by their Java objects */
    return;
}
