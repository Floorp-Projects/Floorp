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
 * The Original Code is the Netscape Security Services for Java.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998-2000 Netscape Communications Corporation.  All
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

#include "_jni/com_netscape_jss_pkcs11_PK11Store.h"

#include <plarena.h>
#include <nspr.h>
#include <key.h>
#include <secmod.h>
#include <pk11func.h>
#include <cert.h>
#include <certdb.h>
#include <secasn1.h>

#include <jssutil.h>
#include "pk11util.h"
#include <java_ids.h>
#include <jss_exceptions.h>

/***********************************************************************
 * HACKS which belong in pk11cert.c
 ***********************************************************************/
typedef struct pk11KeyCallbackStr {
        SECStatus (* callback)(SECKEYPrivateKey *,void *);
        void *callbackArg;
        void *wincx;
} pk11KeyCallback;

/* Traverse slots callback */
typedef struct pk11TraverseSlotStr {
    SECStatus (*callback)(PK11SlotInfo *,CK_OBJECT_HANDLE, void *);
    void *callbackArg;
    CK_ATTRIBUTE *findTemplate;
    int templateCount;
} pk11TraverseSlot;

SECStatus pk11_DoKeys(PK11SlotInfo*, CK_OBJECT_HANDLE, void*);
SECStatus PK11_TraverseSlot(PK11SlotInfo *, void*);

/***********************************************************************
 * PK11_TraversePrivateKeysInSlot
 *
 * This is an HCL hack that traverses all the private keys on a slot.
 *
 * INPUTS
 *      slot
 *          The PKCS #11 slot whose private keys you want to traverse.
 *      callback
 *          A callback function that will be called for each key.
 *      arg
 *          An argument that will be passed to the callback function.
 */
static SECStatus
PK11_TraversePrivateKeysInSlot( PK11SlotInfo *slot,
    SECStatus(* callback)(SECKEYPrivateKey*, void*), void *arg)
{
    pk11KeyCallback perKeyCB;
    pk11TraverseSlot perObjectCB;
    CK_OBJECT_CLASS privkClass = CKO_PRIVATE_KEY;
    CK_ATTRIBUTE theTemplate[1];
    int templateSize = 1;

    theTemplate[0].type = CKA_CLASS;
    theTemplate[0].pValue = &privkClass;
    theTemplate[0].ulValueLen = sizeof(privkClass);

    if(slot==NULL) {
#ifdef DEBUG
        PR_fprintf(PR_STDERR,
            "Null slot passed to PK11_TraversePrivateKeysInSlot\n");
        PR_ASSERT(PR_FALSE);
#endif
        return SECSuccess;
    }

    perObjectCB.callback = pk11_DoKeys;
    perObjectCB.callbackArg = &perKeyCB;
    perObjectCB.findTemplate = theTemplate;
    perObjectCB.templateCount = templateSize;
    perKeyCB.callback = callback;
    perKeyCB.callbackArg = arg;
    perKeyCB.wincx = NULL;

    return PK11_TraverseSlot(slot, &perObjectCB);
}

/**********************************************************************
 * Callback information for keyTraversalCallback
 */
typedef enum {
    CERT_OBJECT,
    KEY_OBJECT
} ObjectType;
typedef struct {
    JNIEnv *env;
    jobject vector;
    ObjectType type;
} TraversalCallbackInfo;

/**********************************************************************
 * traversalCallback
 *
 * Generic callback that does the job for both private keys
 * and certificates.
 *
 */
static SECStatus
traversalCallback(void *keyCert, void *arg)
{
    TraversalCallbackInfo *info;
    jclass vectorClass;
    jmethodID addElement;
    jobject object=NULL;
    jobject vector;
    JNIEnv *env;
    SECKEYPrivateKey *key=NULL;
    SECKEYPrivateKey *keyCopy=NULL;
    CERTCertificate *cert=NULL;
    CERTCertificate *certCopy=NULL;
    SECStatus status = SECFailure;

    /**************************************************
     * Get the callback data
     **************************************************/
    if(arg == NULL) {
        JSS_trace(env, JSS_TRACE_ERROR,
            "traversalCallback called with NULL argument");
        PR_ASSERT(PR_FALSE);
        goto finish;
    }
    info = (TraversalCallbackInfo*)arg;
    env = info->env;
    vector = info->vector;
    if(info->type == KEY_OBJECT) {
        key = (SECKEYPrivateKey*)keyCert;
    } else {
        PR_ASSERT(info->type == CERT_OBJECT);
        cert = (CERTCertificate*)keyCert;
    }
    if(env==NULL || vector==NULL) {
        PR_ASSERT(PR_FALSE);
        goto finish;
    }

    /**************************************************
     * Get JNI ids
     **************************************************/
    vectorClass = (*env)->GetObjectClass(env, vector);
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

    /***************************************************
     * Wrap the object
     ***************************************************/
    if(info->type == KEY_OBJECT) {

      /** Private keys may be temporary now...
        if(key->pkcs11IsTemp) {
            JSS_trace(env, JSS_TRACE_ERROR,
                "Private Key passed to keyTraversalCallback is a"
                " temporary object");
            PR_ASSERT(PR_FALSE);
            goto finish;
        }
      */

        keyCopy = SECKEY_CopyPrivateKey(key);
        object = JSS_PK11_wrapPrivKey(env, &keyCopy);
    } else {
        PR_ASSERT( info->type == CERT_OBJECT );

        certCopy = CERT_DupCertificate(cert);
        object = JSS_PK11_wrapCert(env, &certCopy);
    }
    if(object == NULL) {
        PR_ASSERT( (*env)->ExceptionOccurred(env) );
        goto finish;
    }
        

    /***************************************************
     * Insert the key into the vector
     ***************************************************/
    (*env)->CallVoidMethod(env, vector, addElement, object);

    status = SECSuccess;

finish:
    if(object==NULL) {
        if(keyCopy!=NULL) {
            SECKEY_DestroyPrivateKey(keyCopy);
        }
        if(certCopy!=NULL) {
            CERT_DestroyCertificate(certCopy);
        }
    }
    return status;
}

/**********************************************************************
 * keyTraversalCallback
 *
 * Given a private key and vector, inserts the private key into the vector.
 *
 */
static SECStatus
keyTraversalCallback(SECKEYPrivateKey *key, void *arg)
{
    PR_ASSERT( ((TraversalCallbackInfo*)arg)->type == KEY_OBJECT);
    return traversalCallback( (void*)key, arg);
}

/**********************************************************************
 * certTraversalCallback
 *
 * Given a certificate and vector, inserts the certificate into the vector.
 *
 */
static SECStatus
certTraversalCallback(CERTCertificate *cert, void *arg)
{
    PR_ASSERT( ((TraversalCallbackInfo*)arg)->type == CERT_OBJECT);
    return traversalCallback( (void*)cert, arg);
}


/**********************************************************************
 * PK11Store.putKeysInVector
 */
JNIEXPORT void JNICALL
Java_com_netscape_jss_pkcs11_PK11Store_putKeysInVector
    (JNIEnv *env, jobject this, jobject keyVector)
{
    PK11SlotInfo *slot;
    TraversalCallbackInfo info;

    PR_ASSERT(env!=NULL && this!=NULL && keyVector!=NULL);

    if( JSS_PK11_getStoreSlotPtr(env, this, &slot) != PR_SUCCESS) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }
    PR_ASSERT(slot!=NULL);

    info.env = env;
    info.vector = keyVector;
    info.type = KEY_OBJECT;

    /*
     * Most, if not all, tokens have to be logged in before they allow
     * access to their private keys, so try to login here.  If we're already
     * logged in, this is a no-op.
     * If the login fails, go ahead and try to get the keys anyway, in case
     * this is an exceptionally promiscuous token.
     */
    PK11_Authenticate(slot, PR_TRUE /*load certs*/, NULL /*wincx*/);

    if( PK11_TraversePrivateKeysInSlot(slot,
                                       keyTraversalCallback,
                                       (void*)&info) != SECSuccess)
    {
        if( ! (*env)->ExceptionOccurred(env) ) {
            JSS_throwMsg(env, TOKEN_EXCEPTION,
                "PK11_TraverseSlot returned an error");
        }
        goto finish;
    }

finish:
    return;
}

/**********************************************************************
 * PK11Store.putCertsInVector
 */
JNIEXPORT void JNICALL
Java_com_netscape_jss_pkcs11_PK11Store_putCertsInVector
    (JNIEnv *env, jobject this, jobject certVector)
{
    PK11SlotInfo *slot;
    TraversalCallbackInfo info;

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

    info.env = env;
    info.vector = certVector;
    info.type = CERT_OBJECT;

    if( PK11_TraverseCertsInSlot(slot,
                                 certTraversalCallback,
                                 (void*)&info) != SECSuccess)
    {
        if( ! (*env)->ExceptionOccurred(env) ) {
            JSS_throwMsg(env, TOKEN_EXCEPTION,
                "PK11_TraverseSlot returned an error");
        }
        goto finish;
    }

finish:
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
PR_IMPLEMENT( PRStatus )
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
Java_com_netscape_jss_pkcs11_PK11Store_deletePrivateKey
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
Java_com_netscape_jss_pkcs11_PK11Store_deleteCert
    (JNIEnv *env, jobject this, jobject certObject)
{
    PK11SlotInfo *slot, *certSlot=NULL;
    CK_OBJECT_HANDLE certID;
    CERTCertificate *cert;

    PR_ASSERT(env!=NULL && this!=NULL);
    if(certObject == NULL) {
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

    if( JSS_PK11_getCertPtr(env, certObject, &cert) != PR_SUCCESS) {
        PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL);
        goto finish;
    }

    certID = PK11_FindObjectForCert(cert, NULL, &certSlot);

    /***************************************************
     * Validate structures
     ***************************************************/
    if( slot != certSlot) {
        if( certSlot == NULL ) {
            /* try deleting from internal cert database */
            if( SEC_DeletePermCertificate(cert) != SECSuccess) {
                JSS_throwMsg(env, TOKEN_EXCEPTION,
                    "Unable to delete certificate from internal database");
            } 
        } else {
            JSS_throw(env, NO_SUCH_ITEM_ON_TOKEN_EXCEPTION);
        }
        goto finish; /* in any case we're done */
    }

    /***************************************************
     * Perform the destruction
     ***************************************************/
    if( PK11_Authenticate(certSlot, PR_TRUE /*loadCerts*/, NULL /*wincx*/)
            != SECSuccess)
    {
        JSS_throwMsg(env, TOKEN_EXCEPTION, "Unable to login to token");
        goto finish;
    }
    if( PK11_DestroyTokenObject(certSlot, certID) != SECSuccess)
    {
        JSS_throwMsg(env, TOKEN_EXCEPTION, "Unable to actually destroy object");
        goto finish;
    }

    if ((cert->istemp != PR_TRUE) && (cert->istemp != PR_FALSE)) {
      /* the cloning feature somehow doesn't have istemp initialized */ 
      cert->istemp = PR_FALSE;
    }

    /* This call returns SECSuccess if cert istemp is PR_FALSE */
    if (cert->istemp == PR_TRUE) {
	if( CERT_DeleteTempCertificate(cert) != SECSuccess ) {
        JSS_throwMsg(env, TOKEN_EXCEPTION, "Unable to delete temporary cert");
        goto finish;
	}
    }

finish: 
    if(certSlot != NULL) {
        PK11_FreeSlot(certSlot);
    }
    return;
}

#define DER_DEFAULT_CHUNKSIZE (2048)

/***********************************************************************
 * passwordToSecitem
 * 
 * Converts a Java Password object to a SECItem, first hashing with
 * global salt. The Java Password object will be cleared.
 * Returns NULL iff an exception was thrown.
 */
static SECItem*
passwordToSecitem(JNIEnv *env, jobject pwObject, jbyteArray globalSaltArray)
{
    jclass passwordClass;
    jmethodID getByteCopyMethod;
    jmethodID clearMethod;
    jbyteArray pwArray;
    SECItem *ret=NULL;
    jbyte *pwChars;
    jthrowable excep;
    SECItem *salt = NULL;

    PR_ASSERT(env!=NULL && pwObject!=NULL);

    ret = (SECItem*) PR_NEW(SECItem);
    if(ret == NULL) {
        JSS_throw(env, OUT_OF_MEMORY_ERROR);
        goto finish;
    }

    /*****************************************
     * Get Password class and methods
     *****************************************/
    passwordClass = (*env)->GetObjectClass(env, pwObject);
    if(passwordClass == NULL) {
        goto finish;
    }
    getByteCopyMethod = (*env)->GetMethodID(
                                            env,
                                            passwordClass,
                                            PW_GET_BYTE_COPY_NAME,
                                            PW_GET_BYTE_COPY_SIG);
    clearMethod = (*env)->GetMethodID(  env,
                                        passwordClass,
                                        PW_CLEAR_NAME,
                                        PW_CLEAR_SIG);
    if(getByteCopyMethod==NULL || clearMethod==NULL) {
        goto finish;
    }

    /***************************************************
     * Get the salt
     ***************************************************/
    salt = PR_NEW(SECItem);
    if( salt == NULL ) {
        JSS_throw(env, OUT_OF_MEMORY_ERROR);
        goto finish;
    }
    salt->len = (*env)->GetArrayLength(env, globalSaltArray);
    PR_ASSERT(salt->len > 0);
    salt->data = (unsigned char*)
                    (*env)->GetByteArrayElements(env, globalSaltArray, NULL);
    if( salt->data == NULL ) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }

    /************************************************
     * Get the bytes from the password, then clear it
     ***********************************************/
    pwArray = (*env)->CallObjectMethod( env, pwObject, getByteCopyMethod);
    (*env)->CallVoidMethod(env, pwObject, clearMethod);
    if(pwArray == NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }

    /*************************************************************
     * Copy the characters out of the byte array,
     *************************************************************/
    pwChars = (*env)->GetByteArrayElements(env, pwArray, NULL);
    if(pwChars == NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }
    /* hash the password into a SECItem */
    ret = SECKEY_HashPassword( (char*) pwChars, salt);

    /***************************************************
     * Clear the array.
     ***************************************************/
    memset(pwChars, 0, ret->len);
    (*env)->ReleaseByteArrayElements(env, pwArray, pwChars, 0);

finish:
    if( (excep=(*env)->ExceptionOccurred(env)) ) {
        (*env)->ExceptionClear(env);
    }
    if(salt) {
        if(salt->data) {
            (*env)->ReleaseByteArrayElements(env, globalSaltArray,
                                             (jbyte*) salt->data, JNI_ABORT);
        }
        PR_Free(salt);
    }
    if( excep ) {
        (*env)->Throw(env, excep);
    }
    return ret;
}

typedef enum {
    SUCCESS=0,
    TOKEN_FAILURE,
    DECODE_FAILURE,
    LOGIN_FAILURE,
    OUT_OF_MEM,
    KEYID_FAILURE,
    KEY_EXISTS
} ImportResult;



SECKEYLowPrivateKey *
seckey_decrypt_private_key(SECKEYEncryptedPrivateKeyInfo *epki,
               SECItem *pwitem);

SECKEYEncryptedPrivateKeyInfo *
seckey_encrypt_private_key(
        SECKEYLowPrivateKey *pk, SECItem *pwitem, SECKEYKeyDBHandle *keydb,
        SECOidTag algorithm);

int PK11_NumberObjectsFor(PK11SlotInfo*, CK_ATTRIBUTE*, int);


/***********************************************************************
 * decoding part copied from keydb.c seckey_decode_encrypted_private_key
 
 */
static ImportResult
decodeAndImportEncryptedKey(SECKEYDBKey *dbkey, SECItem *pwitem,
                        PK11SlotInfo *slot, KeyType keyType, PRBool temporary)
{
    SECKEYLowPrivateKey *pk = NULL;
    SECKEYEncryptedPrivateKeyInfo *epki=NULL;
    SECKEYEncryptedPrivateKeyInfo *newepki=NULL;
    PRArenaPool *temparena = NULL;
    SECStatus rv = SECFailure;
    SECOidTag algorithm;
    SECItem *publicValue = NULL;
    SECItem nickname;
    ImportResult result=DECODE_FAILURE;

    PR_ASSERT(dbkey!=NULL && pwitem!=NULL);

    temparena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if(temparena == NULL) {
        return OUT_OF_MEM;
    }

    epki = (SECKEYEncryptedPrivateKeyInfo *)
        PORT_ArenaZAlloc(temparena, sizeof(SECKEYEncryptedPrivateKeyInfo));
    if(epki == NULL) {
        goto loser;
    }

    rv = SEC_ASN1DecodeItem(temparena, epki,
                            SECKEY_EncryptedPrivateKeyInfoTemplate,
                            &(dbkey->derPK));
    if(rv != SECSuccess) {
        goto loser;
    }

    algorithm = SECOID_GetAlgorithmTag(&(epki->algorithm));
    switch(algorithm)
    {
      case SEC_OID_RC4:
        rv = SECITEM_CopyItem(temparena, &(epki->algorithm.parameters),
                              &(dbkey->salt));
        break;
      default:
        break;
    }

    pk = seckey_decrypt_private_key(epki, pwitem);
    if(pk == NULL) {
        result = DECODE_FAILURE;
        goto loser;
    }

    if(pk->keyType == dsaKey) {
        publicValue = &pk->u.dsa.publicValue;
    } else {
        PR_ASSERT( pk->keyType == rsaKey );
        publicValue = &pk->u.rsa.modulus;
    }

    newepki = seckey_encrypt_private_key(pk, pwitem, NULL /*keydb*/,
                SEC_OID_PKCS12_PBE_WITH_SHA1_AND_128_BIT_RC4);
    if(newepki == NULL) {
        result = DECODE_FAILURE;
        goto loser;
    }

    /***************************************************
     * Login to the token if necessary
     ***************************************************/
    if( PK11_Authenticate(slot, PR_TRUE /*loadcerts*/, NULL /*wincx*/)
            != SECSuccess)
    {
        result = LOGIN_FAILURE;
        goto loser;
    }


    /**************************************************
     * HACK to workaround bug #323494
     * On internal token, must call C_FindObjectsInit before creating
     * any token objects.
     *************************************************/
    if( slot==PK11_GetInternalSlot() || slot==PK11_GetInternalKeySlot() ) {
        CK_ATTRIBUTE attr;
        CK_OBJECT_CLASS class = CKO_PRIVATE_KEY;

        attr.type = CKA_CLASS;
        attr.pValue = &class;
        attr.ulValueLen = sizeof(class);

        PK11_NumberObjectsFor(slot, &attr, 1);
    }
    
    nickname.len = 0;
    nickname.data = NULL;
    rv = PK11_ImportEncryptedPrivateKeyInfo(slot, newepki, pwitem, &nickname,
            publicValue, !temporary, PR_TRUE /*private*/, keyType,
            0 /*default key usage*/, NULL /*wincx*/);
    if(rv == SECSuccess) {
        result = SUCCESS;
    } else {
        result = TOKEN_FAILURE;
    }

loser:
    if(pk) {
        SECKEY_LowDestroyPrivateKey(pk);
    }
    if(newepki) {
        SECKEY_DestroyEncryptedPrivateKeyInfo(newepki, PR_TRUE /* freeit */);
    }
    PORT_FreeArena(temparena, PR_TRUE);
    return result;
}

/***********************************************************************
 * decoding part copied from keydb.c seckey_decode_encrypted_private_key
 
 */
static ImportResult
decodeAndImportKey(SECItem *dervalue,
                  PK11SlotInfo *slot, KeyType keyType, PRBool temporary)
{
    SECKEYLowPrivateKey *pk = NULL;
    SECKEYPrivateKeyInfo *pki = NULL;
    PRArenaPool *temparena = NULL;
    PRArenaPool *pkarena = NULL;
    SECStatus rv = SECFailure;
    SECOidTag algorithm;
    SECItem *publicValue = NULL;
    SECItem *keyid = NULL;
	SECKEYPrivateKey *existingpk = NULL;
    SECItem nickname;
    ImportResult result=DECODE_FAILURE;

    PR_ASSERT(dervalue!=NULL);

    temparena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    pkarena   = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);

    if(temparena == NULL) {
        return OUT_OF_MEM;
    }
    if(pkarena == NULL) {
        return OUT_OF_MEM;
    }

    pki = (SECKEYPrivateKeyInfo *)
        PORT_ArenaZAlloc(temparena, sizeof(SECKEYPrivateKeyInfo));
    if(pki == NULL) {
        result = DECODE_FAILURE;
        goto loser;
    }

    pk = (SECKEYLowPrivateKey *)
        PORT_ArenaZAlloc(temparena, sizeof(SECKEYLowPrivateKey));
    if(pk == NULL) {
        result = DECODE_FAILURE;
        goto loser;
    }

    rv = SEC_ASN1DecodeItem(temparena, pki,
                            SECKEY_PrivateKeyInfoTemplate,
                            dervalue);
    if(rv != SECSuccess) {
        goto loser;
    }


	/* decode the PublicKey from inside the PublicKeyInfo structure */
	/* the format of this encoding is defined by the algorithm
       in the publickeyinfo, so we have to use different ASN
	   templates depending on which key is which */

    switch(SECOID_GetAlgorithmTag(&pki->algorithm)) {
		case SEC_OID_X500_RSA_ENCRYPTION:
		case SEC_OID_PKCS1_RSA_ENCRYPTION:
			pk->keyType = rsaKey;
			rv = SEC_ASN1DecodeItem(pkarena, pk,
									SECKEY_RSAPrivateKeyTemplate,
									&pki->privateKey);
			break;
		case SEC_OID_ANSIX9_DSA_SIGNATURE:
			pk->keyType = dsaKey;
			rv = SEC_ASN1DecodeItem(pkarena, pk,
									SECKEY_DSAPrivateKeyTemplate,
									&pki->privateKey);
			if (rv != SECSuccess)
                    goto loser;
			rv = SEC_ASN1DecodeItem(pkarena, &pk->u.dsa.params,
									SECKEY_PQGParamsTemplate,
									&pki->algorithm.parameters);
                break;
		default:
			rv = SECFailure;
			break;
		}

 	/* pull out public key */

    if(pk->keyType == dsaKey) {
        publicValue = &pk->u.dsa.publicValue;
    } else {
        PR_ASSERT( pk->keyType == rsaKey );
        publicValue = &pk->u.rsa.modulus;
    }

    /***************************************************
     * Login to the token if necessary
     ***************************************************/
    if( PK11_Authenticate(slot, PR_TRUE /*loadcerts*/, NULL /*wincx*/)
            != SECSuccess)
    {
        result = LOGIN_FAILURE;
        goto loser;
    }

    /***************************************************
     * Throw a 'key exists' exception if the key is
	 * already in the token
     ***************************************************/
	/* first make the key id - a 'nickname' for the key
	   derived from the public key
	*/
	keyid = PK11_MakeIDFromPubKey(publicValue);
	if (keyid == NULL) {
		result = KEYID_FAILURE;
		goto loser;
	}
	existingpk = PK11_FindKeyByKeyID(slot,keyid,NULL);
	if (existingpk != NULL) {
		result = KEY_EXISTS;
		goto loser;
	}

    /**************************************************
     * HACK to workaround bug #323494
     * On internal token, must call C_FindObjectsInit before creating
     * any token objects.
     *************************************************/
    if( slot==PK11_GetInternalSlot() || slot==PK11_GetInternalKeySlot() ) {
        CK_ATTRIBUTE attr;
        CK_OBJECT_CLASS class = CKO_PRIVATE_KEY;

        attr.type = CKA_CLASS;
        attr.pValue = &class;
        attr.ulValueLen = sizeof(class);

        PK11_NumberObjectsFor(slot, &attr, 1);
    }
    
    nickname.len = 0;
    nickname.data = NULL;

    rv = PK11_ImportPrivateKeyInfo(slot, pki, &nickname,
		publicValue, PR_TRUE /*PERM*/, PR_TRUE /*PRIVATE*/, 
		0 /*keyusage*/, NULL /*wincx*/ );

    if(rv == SECSuccess) {
        result = SUCCESS;
    } else {
        result = TOKEN_FAILURE;
		/* if the public key import failed, we are responsible
		   freeing this memory - otherwise it's the
		   responsiblity of the owner of the key */
    	PORT_FreeArena(pkarena, PR_TRUE);
    }

loser:
    if(existingpk) {
		SECKEY_DestroyPrivateKey(existingpk);
	}
    if(keyid) {
        SECITEM_FreeItem(keyid,PR_TRUE);
    }
    if(pk) {
        SECKEY_LowDestroyPrivateKey(pk);
    }
    if(pki) {
        SECKEY_DestroyPrivateKeyInfo(pki, PR_TRUE /* freeit */);
    }
    PORT_FreeArena(temparena, PR_TRUE);
    return result;
}
 

/***********************************************************************
 * importEncryptedPrivateKey
 */
static void
importEncryptedPrivateKey
    (   JNIEnv *env,
        jobject this,
        jbyteArray encodedKeyArray,
        jobject passwordObject,
        jbyteArray saltArray,
        jbyteArray globalSaltArray,
        jobject keyTypeObj,
        PRBool temporary            )
{
    SECKEYDBKey dbkey;
    SECItem *pwitem=NULL;
    ImportResult result;
    PK11SlotInfo *slot;
    jthrowable excep;
    KeyType keyType;


    keyType = JSS_PK11_getKeyType(env, keyTypeObj);
    if( keyType == nullKey ) {
        /* exception was thrown */
        goto finish;
    }

    /*
     * initialize so we can goto finish
     */
    dbkey.arena = NULL;
    dbkey.version = 0;
    dbkey.nickname = NULL;
    dbkey.salt.data = NULL;
    dbkey.salt.len = 0;
    dbkey.derPK.data = NULL;
    dbkey.derPK.len = 0;


    PR_ASSERT(env!=NULL && this!=NULL);

    if(encodedKeyArray == NULL || passwordObject==NULL || saltArray==NULL) {
        JSS_throw(env, NULL_POINTER_EXCEPTION);
        goto finish;
    }

    /*
     * Extract the encoded key into the DBKEY
     */
    dbkey.derPK.len = (*env)->GetArrayLength(env, encodedKeyArray);
    if(dbkey.derPK.len <= 0) {
        JSS_throwMsg(env, INVALID_KEY_FORMAT_EXCEPTION, "Key array is empty");
        goto finish;
    }
    dbkey.derPK.data = (unsigned char*)
            (*env)->GetByteArrayElements(env, encodedKeyArray, NULL);
    if(dbkey.derPK.data == NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }

    /*
     * Extract the salt into the DBKEY
     */
    dbkey.salt.len = (*env)->GetArrayLength(env, saltArray);
    if(dbkey.salt.len <= 0) {
        JSS_throwMsg(env, INVALID_KEY_FORMAT_EXCEPTION, "Salt array is empty");
        goto finish;
    }
    dbkey.salt.data = (unsigned char*)
            (*env)->GetByteArrayElements(env, saltArray, NULL);
    if(dbkey.salt.data == NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }

    /*
     * Extract the password into a SECItem, which has the side effect
     * of clearing the password.
     */
    pwitem = passwordToSecitem(env, passwordObject, globalSaltArray);
    if(pwitem == NULL) {
        PR_ASSERT( (*env)->ExceptionOccurred(env) );
        goto finish;
    }

    /*
     * Get the PKCS #11 slot
     */
    if( JSS_PK11_getStoreSlotPtr(env, this, &slot) != PR_SUCCESS) {
        PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL);
        goto finish;
    }

    /*
     * Now decode and import
     */
    result = decodeAndImportEncryptedKey(&dbkey, pwitem, slot, keyType, temporary);
    if( result == TOKEN_FAILURE ) {
        JSS_throwMsg(env, TOKEN_EXCEPTION, "Failed to import key to token");
    } else if( result == DECODE_FAILURE ) {
        JSS_throwMsg(env, INVALID_KEY_FORMAT_EXCEPTION,
            "Failed to decode key");
    } else if( result == LOGIN_FAILURE ) {
        JSS_throwMsg(env, TOKEN_EXCEPTION, "Failed to login to token");
    } else {
        PR_ASSERT( result == SUCCESS );
    }

finish:
    /* Save any exceptions */
    if( (excep=(*env)->ExceptionOccurred(env)) ) {
        (*env)->ExceptionClear(env);
    }
    if(dbkey.derPK.data != NULL) {
        (*env)->ReleaseByteArrayElements(   env,
                                            encodedKeyArray,
                                            (jbyte*) dbkey.derPK.data,
                                            JNI_ABORT           );
    }
    if(dbkey.salt.data != NULL) {
        (*env)->ReleaseByteArrayElements(   env,
                                            saltArray,
                                            (jbyte*) dbkey.salt.data,
                                            JNI_ABORT           );
    }
    if(pwitem) {
        SECITEM_ZfreeItem(pwitem, PR_TRUE);
    }
    /* now re-throw the exception */
    if( excep ) {
        (*env)->Throw(env, excep);
    }
}


/***********************************************************************
 * PK11Store.importEncryptedPrivateKey
 */
JNIEXPORT void JNICALL
Java_com_netscape_jss_pkcs11_PK11Store_importEncryptedPrivateKey
    (   JNIEnv *env,
        jobject this,
        jbyteArray encodedKeyArray,
        jobject passwordObject,
        jbyteArray saltArray,
        jbyteArray globalSaltArray,
        jobject keyTypeObj        )
{
    importEncryptedPrivateKey(env, this, encodedKeyArray, passwordObject,
        saltArray, globalSaltArray, keyTypeObj, PR_FALSE /* not temporary */);
}

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
    ImportResult result;
    PK11SlotInfo *slot;
    jthrowable excep;
    KeyType keyType;


    keyType = JSS_PK11_getKeyType(env, keyTypeObj);
    if( keyType == nullKey ) {
        /* exception was thrown */
        goto finish;
    }

    /*
     * initialize so we can goto finish
     */
    /* dbkey.arena = NULL; */
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

    /*
     * Now decode and import
     */
    result = decodeAndImportKey(&derPK, slot, keyType, temporary);
    if( result == TOKEN_FAILURE ) {
        JSS_throwMsg(env, TOKEN_EXCEPTION, "Failed to import key to token");
    } else if( result == DECODE_FAILURE ) {
        JSS_throwMsg(env, INVALID_KEY_FORMAT_EXCEPTION,
            "Failed to decode key");
    } else if( result == KEY_EXISTS) {
        JSS_throwMsg(env, KEY_EXISTS_EXCEPTION, "Key already in token");
    } else if( result == KEYID_FAILURE ) {
        JSS_throwMsg(env, TOKEN_EXCEPTION, "Error creating Key ID");
    } else if( result == LOGIN_FAILURE ) {
        JSS_throwMsg(env, TOKEN_EXCEPTION, "Failed to login to token");
    } else {
        PR_ASSERT( result == SUCCESS );
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


/***********************************************************************
 * PK11Store.importdPrivateKey
 */
JNIEXPORT void JNICALL
Java_com_netscape_jss_pkcs11_PK11Store_importPrivateKey
    (   JNIEnv *env,
        jobject this,
        jbyteArray keyArray,
        jobject keyTypeObj        )
{
    importPrivateKey(env, this, keyArray,
        keyTypeObj, PR_FALSE /* not temporary */);
}

/***********************************************************************
 * PK11Store.importTemporaryEncryptedPrivateKey
 */
JNIEXPORT void JNICALL
Java_com_netscape_jss_pkcs11_PK11Store_importTemporaryEncryptedPrivateKey
    (   JNIEnv *env,
        jobject this,
        jbyteArray encodedKeyArray,
        jobject passwordObject,
        jbyteArray saltArray,
        jbyteArray globalSaltArray,
        jobject keyTypeObj        )
{
    importEncryptedPrivateKey(env, this, encodedKeyArray, passwordObject,
        saltArray, globalSaltArray, keyTypeObj, PR_FALSE /* temporary */);
}
