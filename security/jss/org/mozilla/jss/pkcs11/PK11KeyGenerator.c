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
 * The Original Code is the Netscape Security Services for Java.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
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

#include "_jni/org_mozilla_jss_pkcs11_PK11KeyGenerator.h"

#include <nspr.h>
#include <plarena.h>
#include <secmodt.h>
#include <key.h>
#include <certt.h>
#include <secpkcs5.h> /* for hand-generating SHA-1 PBA HMAC key */
#include <pk11pqg.h>

#include "jssutil.h"
#include "pk11util.h"
#include <java_ids.h>
#include <jss_exceptions.h>
#include <Algorithm.h>
#include <pk11func.h>
#include <secoid.h>

PBEBitGenContext *
PBE_CreateContext(SECOidTag hashAlgorithm, PBEBitGenID bitGenPurpose,
        SECItem *pwitem, SECItem *salt, unsigned int bitsNeeded,
        unsigned int iterations);

SECItem *
PBE_GenerateBits(PBEBitGenContext *context);

void
PBE_DestroyContext(PBEBitGenContext *context);

/***********************************************************************
 *
 * PK11KeyGenerator.generateNormal
 *
 * Generates a non-PBE symmetric key on a token.
 *
 */
JNIEXPORT jobject JNICALL
Java_org_mozilla_jss_pkcs11_PK11KeyGenerator_generateNormal
    (JNIEnv *env, jclass clazz, jobject token, jobject alg, jint strength,
    jint opFlags, jboolean temporary)
{
    PK11SlotInfo *slot=NULL;
    PK11SymKey *skey=NULL;
    CK_MECHANISM_TYPE mech;
    PK11AttrFlags attrFlags=0;
    jobject keyObj=NULL;

    PR_ASSERT( env!=NULL && clazz!=NULL && token!=NULL && alg!=NULL );

    /* Get the slot */
    if( JSS_PK11_getTokenSlotPtr(env, token, &slot) != PR_SUCCESS ) {
        goto finish;
    }

    /* Get the algorithm info */
    mech = JSS_getPK11MechFromAlg(env, alg);
    PR_ASSERT(mech != CKM_INVALID_MECHANISM);

    if(!temporary) {
        attrFlags |= (PK11_ATTR_TOKEN | PK11_ATTR_PRIVATE);
    }

    /* generate the key */
    skey = PK11_TokenKeyGenWithFlags(slot, mech, NULL /*param*/,
                    strength/8 /*in bytes*/, NULL /*keyid*/,
                    opFlags, attrFlags, NULL /*wincx*/ );

    if(skey==NULL) {
        JSS_throwMsgPrErr(env, TOKEN_EXCEPTION, "KeyGen failed on token");
        goto finish;
    }

    /* wrap the key. This sets skey to NULL. */
    keyObj = JSS_PK11_wrapSymKey(env, &skey);

finish:
    if(skey!=NULL) {
        /* will only be non-NULL if keygen succeeded but wrapSymKey failed */
        PK11_FreeSymKey(skey);
    }
    return keyObj;
}

/* We do the translation in Java now, but I'll leave this here just in case */
#if 0
/***********************************************************************
 *
 * C o p y P a s s w o r d T o S E C I t e m
 *
 * pass
 *      A Java Password object.
 *
 * RETURNS
 *      A new SECItem containing a copy of the bytes in the password,
 *      or NULL iff an exception occurred. Be sure to zero it when
 *      you free it.
 */
static SECItem*
CopyPasswordToSECItem(JNIEnv *env, jobject pass) {

    jclass passClass=NULL;
    jmethodID byteCopyMethod=NULL;
    jbyteArray pwArray=NULL;
    jbyte *bytes=NULL;
    SECItem *item=NULL;
    int numBytes=0;

    PR_ASSERT(env!=NULL && pass!=NULL);

    /* get password class and method */
    passClass = (*env)->GetObjectClass(env, pass);
    if(passClass == NULL) {
        JSS_trace(env, JSS_TRACE_ERROR, "Failed to find Password class");
        ASSERT_OUTOFMEM(env);
        goto finish;
    }
    byteCopyMethod = (*env)->GetMethodID(   env,
                                            passClass,
                                            PW_GET_BYTE_COPY_NAME,
                                            PW_GET_BYTE_COPY_SIG);
    if(byteCopyMethod==NULL) {
        JSS_trace(env, JSS_TRACE_ERROR, "Failed to find Password manipulation"
                " methods from native implementation");
        ASSERT_OUTOFMEM(env);
        goto finish;
    }

    /* copy to a byte array */
    pwArray = (*env)->CallObjectMethod(env, pass, byteCopyMethod);
    if(pwArray == NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }
    numBytes = (*env)->GetArrayLength(env, pwArray);

    /* copy from the byte array to a jbyte array */
    bytes = (*env)->GetByteArrayElements(env, pwArray, NULL);
    if(bytes == NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }

    /* copy from the jbyte array to a new SECItem */
    item = PR_NEW(SECItem);
    /* last byte is null termination */
    PR_ASSERT( bytes[numBytes-1] == 0 );
    item->len = numBytes - 1;
    item->data = PR_Malloc(item->len);
    if(item->data==NULL) {
        JSS_throw(env, OUT_OF_MEMORY_ERROR);
        goto finish;
    }
    memcpy(item->data, bytes, item->len);

finish:
    if(bytes!=NULL) {
        /* clear the password */
        PR_ASSERT(numBytes > 0);
        memset(bytes, 0, numBytes);
        (*env)->ReleaseByteArrayElements(env, pwArray, bytes, 0);
    } else {
        PR_ASSERT(pwArray!=NULL);
    }
    return item;
}
#endif

static void
print_secitem(SECItem *item) {
    int i;
    int online;

    if(item==NULL) {
        return;
    }

    for(i=0, online=0; i < item->len; i++, online++) {
        if(online > 25) {
            printf("\n");
            online = 0;
        }
        printf("%.2x ", item->data[i]);
    }
}

/***********************************************************************
 *
 * c o n s t r u c t S H A 1 P B A K e y
 *
 * Constructs a PBE key using CKM_PBA_SHA1_WITH_SHA1_HMAC.  This should
 * be supported by NSS automatically, but isn't (bug #336587).
 *
 * RETURNS
 *      A symmetric key from the given password, salt, and iteration count,
 *      or NULL if an exception was thrown.
 * THROWS
 *      TokenException if an error occurs.
 */
static PK11SymKey*
constructSHA1PBAKey(JNIEnv *env, SECItem *pwitem, SECItem *salt,
        int iterationCount)
{
    PBEBitGenContext* pbeCtxt=NULL;
    SECItem *keyBits=NULL;
    PK11SymKey *key=NULL;

    pbeCtxt = PBE_CreateContext( SEC_OID_SHA1, pbeBitGenIntegrityKey,
                    pwitem, salt, 160 /* SHA1 key length */, iterationCount);
    if( pbeCtxt == NULL ) {
        JSS_throwMsg(env, TOKEN_EXCEPTION, "Failed to create PBE context");
        goto finish;
    }

    keyBits = PBE_GenerateBits(pbeCtxt);
    if( keyBits == NULL ) {
        JSS_throwMsg(env, TOKEN_EXCEPTION, "Failed to generate bits from"
                "PBE context");
        goto finish;
    }

    key = PK11_ImportSymKey( PK11_GetInternalSlot(), CKM_SHA_1,
                PK11_OriginGenerated, CKA_SIGN, keyBits, NULL);
    if( key == NULL ) {
        JSS_throwMsg(env, TOKEN_EXCEPTION, "Failed to import PBA key from"
            " PBA-generated bits");
        goto finish;
    }

finish:
    if( pbeCtxt ) {
        PBE_DestroyContext(pbeCtxt);
    }
    /* keyBits == pbeCtxt, so we don't need to free it */
    return key;
}

/***********************************************************************
 *
 * PK11KeyGenerator.generatePBE
 *
 */
JNIEXPORT jobject JNICALL
Java_org_mozilla_jss_pkcs11_PK11KeyGenerator_generatePBE
    (JNIEnv *env, jclass clazz, jobject token, jobject alg, jbyteArray passBA,
    jbyteArray saltBA, jint iterationCount)
{
    PK11SlotInfo *slot=NULL;
    PK11SymKey *skey=NULL;
    SECOidTag oidTag;
    SECAlgorithmID *algid=NULL;
    SECItem *salt=NULL;
    SECItem *pwitem=NULL;
    jobject keyObj=NULL;
    CK_MECHANISM_TYPE mech=CKM_INVALID_MECHANISM;

    PR_ASSERT(env!=NULL && clazz!=NULL && token!=NULL && alg!=NULL
        && passBA!=NULL && saltBA!=NULL);

    /* get the slot */
    if( JSS_PK11_getTokenSlotPtr(env, token, &slot) != PR_SUCCESS) {
        goto finish;
    }

    /* convert salt to SECItem */
    salt = JSS_ByteArrayToSECItem(env, saltBA);
    if(salt == NULL) {
        goto finish;
    }

    /* convert password to SECItem */
    pwitem = JSS_ByteArrayToSECItem(env, passBA);
    if(pwitem==NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }
    /* print_secitem(pwitem); */


    mech = JSS_getPK11MechFromAlg(env, alg);

    if( mech == CKM_PBA_SHA1_WITH_SHA1_HMAC ) {

        /* special case, construct key by hand. Bug #336587 */

        skey = constructSHA1PBAKey(env, pwitem, salt, iterationCount);
        if( skey==NULL ) {
            /* exception was thrown */
            goto finish;
        }

    } else {

        /* get the algorithm info */
        oidTag = JSS_getOidTagFromAlg(env, alg);
        PR_ASSERT(oidTag != SEC_OID_UNKNOWN);

        /* create algid */
        algid = PK11_CreatePBEAlgorithmID(oidTag, iterationCount, salt);
        if( algid == NULL ) {
            JSS_throwMsg(env, TOKEN_EXCEPTION,
                    "Unable to process PBE parameters");
            goto finish;
        }

        /* generate the key */
        skey = PK11_PBEKeyGen(slot, algid, pwitem, PR_FALSE /*faulty3DES*/,
                        NULL /*wincx*/);
        if( skey == NULL ) {
            JSS_throwMsg(env, TOKEN_EXCEPTION, "Failed to generate PBE key");
            goto finish;
        }
    }

    /* wrap the key. This sets skey to NULL. */
    keyObj = JSS_PK11_wrapSymKey(env, &skey);

finish:
    if(algid) {
        SECOID_DestroyAlgorithmID(algid, PR_TRUE /*freeit*/);
    }
    if(salt) {
        SECITEM_FreeItem(salt, PR_TRUE /*freeit*/);
    }
    if(pwitem) {
        SECITEM_ZfreeItem(pwitem, PR_TRUE /*freeit*/);
    }
    if(skey) {
        /* skey will be NULL if everything worked */
        PK11_FreeSymKey(skey);
    }
    return keyObj;
}


/***********************************************************************
 *
 * PK11KeyGenerator.generatePBE_IV
 *
 */
JNIEXPORT jobject JNICALL
Java_org_mozilla_jss_pkcs11_PK11KeyGenerator_generatePBE_1IV
    (JNIEnv *env, jclass clazz, jobject alg, jbyteArray passBA,
    jbyteArray saltBA, jint iterationCount)
{
    SECOidTag oidTag;
    SECAlgorithmID *algid=NULL;
    SECItem *salt=NULL;
    SECItem *pwitem=NULL;
    SECItem *ivItem=NULL;
    jbyteArray ivBA=NULL;

    PR_ASSERT(env!=NULL && clazz!=NULL && alg!=NULL
        && passBA!=NULL && saltBA!=NULL);

    /* get the algorithm info */
    oidTag = JSS_getOidTagFromAlg(env, alg);
    PR_ASSERT(oidTag != SEC_OID_UNKNOWN);

    /* convert salt to SECItem */
    salt = JSS_ByteArrayToSECItem(env, saltBA);
    if(salt == NULL) {
        goto finish;
    }

    /* create algid */
    algid = PK11_CreatePBEAlgorithmID(oidTag, iterationCount, salt);
    if( algid == NULL ) {
        JSS_throwMsg(env, TOKEN_EXCEPTION, "Unable to process PBE parameters");
        goto finish;
    }

    /* convert password to SECItem */
    pwitem = JSS_ByteArrayToSECItem(env, passBA);
    if(pwitem==NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }

    /* generate the IV */
    ivItem = SEC_PKCS5GetIV(algid, pwitem, PR_FALSE /*faulty3DES*/);
    if(ivItem==NULL) {
        JSS_throwMsg(env, TOKEN_EXCEPTION, "Unable to generate PBE "
            "initialization vector");
        goto finish;
    }

    /* convert IV to byte array */
    ivBA = JSS_SECItemToByteArray(env, ivItem);

finish:
    if(algid) {
        SECOID_DestroyAlgorithmID(algid, PR_TRUE /*freeit*/);
    }
    if(salt) {
        SECITEM_FreeItem(salt, PR_TRUE /*freeit*/);
    }
    if(pwitem) {
        SECITEM_ZfreeItem(pwitem, PR_TRUE /*freeit*/);
    }
    if(ivItem) {
        SECITEM_FreeItem(ivItem, PR_TRUE /*freeit*/);
    }
    return ivBA;
}

JNIEXPORT jobject JNICALL
Java_org_mozilla_jss_pkcs11_PK11KeyGenerator_nativeClone
    (JNIEnv *env, jclass clazz, jobject tokenObj, jobject toBeClonedObj)
{
    PK11SlotInfo *slot=NULL;
    PK11SymKey *toBeCloned=NULL;
    PK11SymKey *clone=NULL;
    SECStatus rv;
    jobject cloneObj=NULL;

    PR_ASSERT(env!=NULL && tokenObj!=NULL && toBeClonedObj!=NULL);

    /* get slot */
    if( JSS_PK11_getTokenSlotPtr(env, tokenObj, &slot) != PR_SUCCESS) {
        /* exception was thrown */
        goto finish;
    }

    /* get toBeCloned */
    if( JSS_PK11_getSymKeyPtr(env, toBeClonedObj, &toBeCloned) != PR_SUCCESS) {
        /* exception was thrown */
        goto finish;
    }

    /* extract the key value */
    rv = PK11_ExtractKeyValue(toBeCloned);
    if( rv != SECSuccess ) {
        JSS_throw(env, NOT_EXTRACTABLE_EXCEPTION);
        goto finish;
    }

    clone = PK11_ImportSymKey(
        slot,
        PK11_GetMechanism(toBeCloned),
        PK11_OriginGenerated, /* we don't know this, but it doesn't matter */
        CKA_ENCRYPT, /* !!! Actually we want to enable all operations */
        PK11_GetKeyData(toBeCloned),
        NULL /* wincx */
    );
        

    if( clone == NULL ) {
        JSS_throwMsg(env, TOKEN_EXCEPTION, "Failed to create new symmetric"
            " key object");
        goto finish;
    }

    /* wrap the new key in a Java object */
    cloneObj = JSS_PK11_wrapSymKey(env, &clone);   

finish:
    if( clone!=NULL ) {
        /* clone would be NULL if we completed successfully */
        PK11_FreeSymKey(clone);
    }
    return cloneObj;
}

