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

#include "_jni/org_mozilla_jss_pkcs11_PK11Store.h"

#include <plarena.h>
#include <nspr.h>
#include <key.h>
#include <secmod.h>
#include <pk11func.h>
#include <cert.h>
#include <certdb.h>
#include <secasn1.h>

#include <jssutil.h>
#include <Algorithm.h>
#include "pk11util.h"
#include <java_ids.h>
#include <jss_exceptions.h>


/**********************************************************************
 * PK11Store.putKeysInVector
 */
JNIEXPORT void JNICALL
Java_org_mozilla_jss_pkcs11_PK11Store_putKeysInVector
    (JNIEnv *env, jobject this, jobject keyVector)
{
    PK11SlotInfo *slot;
    SECKEYPrivateKeyList *keyList = NULL;
    SECKEYPrivateKey* keyCopy = NULL;
    jobject object = NULL;
    jclass vectorClass;
    jmethodID addElement;
    SECKEYPrivateKeyListNode *node = NULL;

    PR_ASSERT(env!=NULL && this!=NULL && keyVector!=NULL);

    if( JSS_PK11_getStoreSlotPtr(env, this, &slot) != PR_SUCCESS) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }
    PR_ASSERT(slot!=NULL);

    /*
     * Most, if not all, tokens have to be logged in before they allow
     * access to their private keys, so try to login here.  If we're already
     * logged in, this is a no-op.
     * If the login fails, go ahead and try to get the keys anyway, in case
     * this is an exceptionally promiscuous token.
     */
    PK11_Authenticate(slot, PR_TRUE /*load certs*/, NULL /*wincx*/);

    /*
     * Get the list of keys on this token
     */
    keyList = PK11_ListPrivateKeysInSlot(slot);
    if( keyList == NULL ) {
        JSS_throwMsg(env, TOKEN_EXCEPTION, "PK11_ListPrivateKeysInSlot "
            "returned an error");
        goto finish;
    }

    /**************************************************
     * Get JNI ids
     **************************************************/
    vectorClass = (*env)->GetObjectClass(env, keyVector);
    if(vectorClass == NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }
    addElement = (*env)->GetMethodID(env,
                                     vectorClass,
                                     VECTOR_ADD_ELEMENT_NAME,
                                     VECTOR_ADD_ELEMENT_SIG);
    if(addElement == NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }

    for(    node = PRIVKEY_LIST_HEAD(keyList);
            !PRIVKEY_LIST_END(node, keyList);
            node = PRIVKEY_LIST_NEXT(node) )
    {
        /***************************************************
        * Wrap the object
        ***************************************************/
        keyCopy = SECKEY_CopyPrivateKey(node->key);
        object = JSS_PK11_wrapPrivKey(env, &keyCopy);
        if(object == NULL) {
            PR_ASSERT( (*env)->ExceptionOccurred(env) );
            goto finish;
        }

        /***************************************************
        * Insert the key into the vector
        ***************************************************/
        (*env)->CallVoidMethod(env, keyVector, addElement, object);
    }

finish:
    if( keyList != NULL ) {
        SECKEY_DestroyPrivateKeyList(keyList);
    }
    return;
}

/**********************************************************************
 * PK11Store.putCertsInVector
 */
JNIEXPORT void JNICALL
Java_org_mozilla_jss_pkcs11_PK11Store_putCertsInVector
    (JNIEnv *env, jobject this, jobject certVector)
{
    PK11SlotInfo *slot;
    PK11SlotInfo *slotCopy;
    jclass vectorClass;
    jmethodID addElement;
    CERTCertList *certList = NULL;
    CERTCertificate *certCopy;
    CERTCertListNode *node = NULL;
    jobject object;

    PR_ASSERT(env!=NULL && this!=NULL && certVector!=NULL);

    if( JSS_PK11_getStoreSlotPtr(env, this, &slot) != PR_SUCCESS) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }
    PR_ASSERT(slot!=NULL);

    /*
     * log in if the slot does not have publicly readable certs
     */
    if( !PK11_IsFriendly(slot) ) {
        PK11_Authenticate(slot, PR_TRUE /*load certs*/, NULL /*wincx*/);
    }

    certList = PK11_ListCertsInSlot(slot);
    if( certList == NULL ) {
        JSS_throwMsg(env, TOKEN_EXCEPTION, "PK11_ListCertsInSlot "
            "returned an error");
        goto finish;
    }

    /**************************************************
     * Get JNI ids
     **************************************************/
    vectorClass = (*env)->GetObjectClass(env, certVector);
    if(vectorClass == NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }
    addElement = (*env)->GetMethodID(env,
                                     vectorClass,
                                     VECTOR_ADD_ELEMENT_NAME,
                                     VECTOR_ADD_ELEMENT_SIG);
    if(addElement == NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }

    for(    node = CERT_LIST_HEAD(certList);
            !CERT_LIST_END(node, certList);
            node = CERT_LIST_NEXT(node) )
    {
        /***************************************************
        * Wrap the object
        ***************************************************/
        certCopy = CERT_DupCertificate(node->cert);
        slotCopy = PK11_ReferenceSlot(slot);
        object = JSS_PK11_wrapCertAndSlotAndNickname(env,
            &certCopy, &slotCopy, node->appData);
        if(object == NULL) {
            PR_ASSERT( (*env)->ExceptionOccurred(env) );
            goto finish;
        }

        /***************************************************
        * Insert the cert into the vector
        ***************************************************/
        (*env)->CallVoidMethod(env, certVector, addElement, object);
    }

finish:
    if( certList != NULL ) {
        CERT_DestroyCertList(certList);
    }
    return;
}

/************************************************************************
 *
 * J S S _ g e t S t o r e S l o t P t r
 *
 * Retrieve the PK11SlotInfo pointer of the given PK11Store.
 *
 * INPUTS
 *      store
 *          A reference to a Java PK11Store
 *      slot
 *          address of a PK11SlotInfo* that will be loaded with
 *          the PK11SlotInfo pointer of the given token.
 * RETURNS
 *      PR_SUCCESS if the operation was successful, PR_FAILURE if an
 *      exception was thrown.
 */
PRStatus
JSS_PK11_getStoreSlotPtr(JNIEnv *env, jobject store, PK11SlotInfo **slot)
{
    PR_ASSERT(env!=NULL && store!=NULL && slot!=NULL);

    return JSS_getPtrFromProxyOwner(env, store, PK11STORE_PROXY_FIELD,
                PK11STORE_PROXY_SIG, (void**)slot);
}

/**********************************************************************
 * PK11Store.deletePrivateKey
 */
JNIEXPORT void JNICALL
Java_org_mozilla_jss_pkcs11_PK11Store_deletePrivateKey
    (JNIEnv *env, jobject this, jobject key)
{
    PK11SlotInfo *slot;
    SECKEYPrivateKey *privk;

    PR_ASSERT(env!=NULL && this!=NULL);
    if(key == NULL) {
        JSS_throw(env, NO_SUCH_ITEM_ON_TOKEN_EXCEPTION);
        goto finish;
    }

    /**************************************************
     * Get the C structures
     **************************************************/
    if( JSS_PK11_getStoreSlotPtr(env, this, &slot) != PR_SUCCESS) {
        PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL);
        goto finish;
    }

    if( JSS_PK11_getPrivKeyPtr(env, key, &privk) != PR_SUCCESS) {
        PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL);
        goto finish;
    }

    /***************************************************
     * Validate structures
     ***************************************************/

    /* A private key may be temporary, but you can't use this function
     * to delete it.  Instead, just let it be garbage collected */
    if( privk->pkcs11IsTemp ) {
        PR_ASSERT(PR_FALSE);
        JSS_throwMsg(env, TOKEN_EXCEPTION,
            "Private Key is not a permanent PKCS #11 object");
        goto finish;
    }

    if( slot != privk->pkcs11Slot) {
        JSS_throw(env, NO_SUCH_ITEM_ON_TOKEN_EXCEPTION);
        goto finish;
    }

    /***************************************************
     * Perform the destruction
     ***************************************************/
    if( PK11_DestroyTokenObject(privk->pkcs11Slot, privk->pkcs11ID)
        != SECSuccess)
    {
        JSS_throwMsg(env, TOKEN_EXCEPTION, "Unable to actually destroy object");
        goto finish;
    }

finish:
    return;
}

/**********************************************************************
 * PK11Store.deleteCert
 */
JNIEXPORT void JNICALL
Java_org_mozilla_jss_pkcs11_PK11Store_deleteCert
    (JNIEnv *env, jobject this, jobject certObject)
{
    CERTCertificate *cert;
    SECStatus status;

    PR_ASSERT(env!=NULL && this!=NULL);
    if(certObject == NULL) {
        JSS_throw(env, NO_SUCH_ITEM_ON_TOKEN_EXCEPTION);
        goto finish;
    }

    if( JSS_PK11_getCertPtr(env, certObject, &cert) != PR_SUCCESS) {
        PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL);
        goto finish;
    }

    status = PK11_DeleteTokenCertAndKey(cert, NULL);
    status = SEC_DeletePermCertificate(cert);

finish: 
    return;
}

#define DER_DEFAULT_CHUNKSIZE (2048)

int PK11_NumberObjectsFor(PK11SlotInfo*, CK_ATTRIBUTE*, int);

/***********************************************************************
 * importPrivateKey
 */
static void
importPrivateKey
    (   JNIEnv *env,
        jobject this,
        jbyteArray keyArray,
        jobject keyTypeObj,
        PRBool temporary            )
{
    SECItem derPK;
    PK11SlotInfo *slot;
    jthrowable excep;
    KeyType keyType;
    SECStatus status;
    SECItem nickname;

    keyType = JSS_PK11_getKeyType(env, keyTypeObj);
    if( keyType == nullKey ) {
        /* exception was thrown */
        goto finish;
    }

    /*
     * initialize so we can goto finish
     */
    derPK.data = NULL;
    derPK.len = 0;


    PR_ASSERT(env!=NULL && this!=NULL);

    if(keyArray == NULL) {
        JSS_throw(env, NULL_POINTER_EXCEPTION);
        goto finish;
    }

    /*
     * copy the java byte array into a local copy
     */
    derPK.len = (*env)->GetArrayLength(env, keyArray);
    if(derPK.len <= 0) {
        JSS_throwMsg(env, INVALID_KEY_FORMAT_EXCEPTION, "Key array is empty");
        goto finish;
    }
    derPK.data = (unsigned char*)
            (*env)->GetByteArrayElements(env, keyArray, NULL);
    if(derPK.data == NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }

    /*
     * Get the PKCS #11 slot
     */
    if( JSS_PK11_getStoreSlotPtr(env, this, &slot) != PR_SUCCESS) {
        PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL);
        goto finish;
    }

    nickname.len = 0;
    nickname.data = NULL;

    status = PK11_ImportDERPrivateKeyInfo(slot, &derPK, &nickname,
                NULL /*public value*/, PR_TRUE /*isPerm*/,
                PR_TRUE /*isPrivate*/, 0 /*keyUsage*/, NULL /*wincx*/);
    if(status != SECSuccess) {
        JSS_throwMsg(env, TOKEN_EXCEPTION, "Failed to import private key info");
        goto finish;
    }

finish:
    /* Save any exceptions */
    if( (excep=(*env)->ExceptionOccurred(env)) ) {
        (*env)->ExceptionClear(env);
    }
    if(derPK.data != NULL) {
        (*env)->ReleaseByteArrayElements(   env,
                                            keyArray,
                                            (jbyte*) derPK.data,
                                            JNI_ABORT           );
    }
    /* now re-throw the exception */
    if( excep ) {
        (*env)->Throw(env, excep);
    }
}

extern const SEC_ASN1Template SECKEY_EncryptedPrivateKeyInfoTemplate[];

/***********************************************************************
 * PK11Store.importdPrivateKey
 */
JNIEXPORT void JNICALL
Java_org_mozilla_jss_pkcs11_PK11Store_importPrivateKey
    (   JNIEnv *env,
        jobject this,
        jbyteArray keyArray,
        jobject keyTypeObj        )
{
    importPrivateKey(env, this, keyArray,
        keyTypeObj, PR_FALSE /* not temporary */);
}


JNIEXPORT jbyteArray JNICALL
Java_org_mozilla_jss_pkcs11_PK11Store_getEncryptedPrivateKeyInfo
(JNIEnv *env, jobject this, jobject certObj, jobject algObj,
    jobject pwObj, jint iteration)

{
    SECKEYEncryptedPrivateKeyInfo *epki = NULL;
    jbyteArray encodedEpki = NULL;
    PK11SlotInfo *slot = NULL;
    SECOidTag algTag;
    jclass passwordClass = NULL;
    jmethodID getByteCopyMethod = NULL;
    jbyteArray pwArray = NULL;
    jbyte* pwchars = NULL;
    SECItem pwItem;
    CERTCertificate *cert = NULL;
    SECItem epkiItem;

    epkiItem.data = NULL;

    /* get slot */
    if( JSS_PK11_getStoreSlotPtr(env, this, &slot) != PR_SUCCESS) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }
    PR_ASSERT(slot!=NULL);
    
    /* get algorithm */
    algTag = JSS_getOidTagFromAlg(env, algObj);
    if( algTag == SEC_OID_UNKNOWN ) {
        JSS_throwMsg(env, NO_SUCH_ALG_EXCEPTION, "Unrecognized PBE algorithm");
        goto finish;
    }

    /*
     * get password
     */
    passwordClass = (*env)->GetObjectClass(env, pwObj);
    if(passwordClass == NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }
    getByteCopyMethod = (*env)->GetMethodID(
                                            env,
                                            passwordClass,
                                            PW_GET_BYTE_COPY_NAME,
                                            PW_GET_BYTE_COPY_SIG);
    if(getByteCopyMethod==NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }
    pwArray = (*env)->CallObjectMethod( env, pwObj, getByteCopyMethod);
    pwchars = (*env)->GetByteArrayElements(env, pwArray, NULL);
    /* !!! Include the NULL byte or not? */
    pwItem.data = (unsigned char*) pwchars;
    pwItem.len = strlen((const char*)pwchars) + 1;

    /*
     * get cert
     */
    if( JSS_PK11_getCertPtr(env, certObj, &cert) != PR_SUCCESS ) {
        /* exception was thrown */
        goto finish;
    }

    /*
     * export the epki
     */
    epki = PK11_ExportEncryptedPrivateKeyInfo(slot, algTag, &pwItem,
            cert, iteration, NULL /*wincx*/);


    /*
     * DER-encode the epki
     */
    epkiItem.data = NULL;
    epkiItem.len = 0;
    if( SEC_ASN1EncodeItem(NULL, &epkiItem, epki,
        SEC_ASN1_GET(SECKEY_EncryptedPrivateKeyInfoTemplate) )  == NULL ) {
        JSS_throwMsg(env, TOKEN_EXCEPTION, "Failed to ASN1-encode "
            "EncryptedPrivateKeyInfo");
        goto finish;
    }

    /*
     * convert to Java byte array
     */
    encodedEpki = JSS_SECItemToByteArray(env, &epkiItem);

finish:
    if( epki != NULL ) {
        SECKEY_DestroyEncryptedPrivateKeyInfo(epki, PR_TRUE /*freeit*/);
    }
    if( pwchars != NULL ) {
        (*env)->ReleaseByteArrayElements(env, pwArray, pwchars, JNI_ABORT);
    }
    if(epkiItem.data != NULL) {
        PR_Free(epkiItem.data);
    }
    return encodedEpki;
}
