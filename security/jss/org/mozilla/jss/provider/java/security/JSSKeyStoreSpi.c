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
 * The Original Code is Network Security Services for Java.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
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

#include "_jni/org_mozilla_jss_provider_java_security_JSSKeyStoreSpi.h"

#include <nspr.h>
#include <pk11func.h>
#include <jssutil.h>
#include <jss_exceptions.h>
#include <keyhi.h>
#include <cert.h>
#include <certdb.h>
#include <pk11util.h>
#include "java_ids.h"

static PRStatus
getTokenSlotPtr(JNIEnv *env, jobject keyStoreObj, PK11SlotInfo **ptr)
{
    return JSS_getPtrFromProxyOwner(env, keyStoreObj,
        "proxy", "Lorg/mozilla/jss/pkcs11/TokenProxy;", (void**)ptr);
}

typedef enum {
    PRIVKEY=0x01,
    PUBKEY=0x02,
    SYMKEY=0x04,
    CERT=0x08
} TokenObjectType;
#define ALL_OBJECT_TYPES (PRIVKEY | PUBKEY | SYMKEY | CERT )

typedef struct {
    PRStatus status;
    PRBool deleteIt;
    PRBool stopIterating;
} JSSTraversalStatus;
#define INIT_TRAVSTAT {PR_FAILURE, PR_FALSE, PR_FALSE}

/*
 * contract: you must throw an exception if you return PRFailure.
 */
typedef JSSTraversalStatus
(*TokenObjectTraversalCallback)
(JNIEnv* env, PK11SlotInfo *slot, TokenObjectType type, void *obj, void *data);

/*
 * objectType is bitwise-OR of all the object types you want to traverse.
 */
static PRStatus
traverseTokenObjects
    (JNIEnv *env, PK11SlotInfo *slot, TokenObjectTraversalCallback cb,
        int objectTypes, void *data)
{
    PRStatus status = PR_FAILURE;
    JSSTraversalStatus travstat = INIT_TRAVSTAT;
    SECKEYPrivateKeyList* privkList = NULL;
    SECKEYPublicKeyList* pubkList = NULL;
    PK11SymKey *symKey = NULL;
    CERTCertList *certList = NULL;
    SECStatus secstat;

    /*
     * Get all private keys
     */
    if( objectTypes & PRIVKEY ) {
        SECKEYPrivateKeyListNode *node = NULL;

        privkList = PK11_ListPrivKeysInSlot(slot, NULL /*nickname*/,
                            NULL /*wincx*/);
        if( privkList != NULL ) {

            for( node = PRIVKEY_LIST_HEAD(privkList);
                ! PRIVKEY_LIST_END(node, privkList);
                node = PRIVKEY_LIST_NEXT(node) )
            {
                travstat = cb(env, slot, PRIVKEY, (void*) node->key, data);
                if( travstat.status == PR_FAILURE ) {
                    goto finish;
                }
                if( travstat.deleteIt ) {
                    /* Setting "force" to PR_FALSE means that if there is a
                     * matching cert, the key won't be deleted.
                     * If the KeyStore API is being followed, the cert
                     * should have the same nickname as the key. So
                     * both will get deleted when we scan for matching
                     * certs later.
                     */
                    PK11_DeleteTokenPrivateKey(node->key, PR_FALSE /*force*/);
                    node->key = NULL;
                    PR_REMOVE_LINK(&node->links);
                    /* we don't free node because it is allocated from
                     * the list's arena and will get freed when the list
                     * is freed. */
                }
                if( travstat.stopIterating ) {
                    goto stop_early;
                }
            }
        }
    }

    /*
     * Get all symmetric keys
     */
    if(objectTypes & SYMKEY) {
        /* this function returns a chained list of symmetric keys */
        symKey = PK11_ListFixedKeysInSlot(slot, NULL /*nickname*/,
                    NULL/*wincx*/);

        while( symKey != NULL ) {
            PK11SymKey *deadKey;
            travstat = cb(env, slot, SYMKEY, (void*) symKey, data);
            if( travstat.status != PR_SUCCESS ) {
                goto finish;
            }
            if( travstat.deleteIt ) {
                /* this just deletes the PKCS #11 object. The data structure
                 * is NOT deleted. */
                PK11_DeleteTokenSymKey(symKey);
            }
            if( travstat.stopIterating ) {
                goto stop_early;
            }

            deadKey = symKey;
            symKey = PK11_GetNextSymKey(symKey);
            PK11_FreeSymKey(deadKey);
        }
    }

    /*
     * get all public keys
     */
    if( objectTypes & PUBKEY ) {
        SECKEYPublicKeyListNode *node = NULL;

        pubkList = PK11_ListPublicKeysInSlot(slot, NULL /*nickname*/);
        if( pubkList != NULL ) {

            for( node = PUBKEY_LIST_HEAD(pubkList);
                ! PUBKEY_LIST_END(node, pubkList);
                node = PUBKEY_LIST_NEXT(node) )
            {
                if( node->key == NULL ) {
                    /* workaround NSS bug 130699: PK11_ListPublicKeysInSlot
                     * returns NULL if slot contains token symmetric key */
                    continue;
                }
                travstat = cb(env, slot, PUBKEY, (void*) node->key, data);
                if( travstat.status != PR_SUCCESS ) {
                    goto finish;
                }
                if( travstat.deleteIt ) {
                    /* XXX!!!
                     * Workaround for 125408: PK11_DeleteTokenPublic key asserts
                     * Don't delete the public key.

                     * PK11_DeleteTokenPublicKey(node->key);
                     * node->key = NULL;
                     * PR_REMOVE_LINK(&node->links);
                     */
                    /* node is allocated from the list's arena, it will get
                     * freed with the list */
                }
                if( travstat.stopIterating ) {
                    goto stop_early;
                }
            }

            /*
             * XXX!!!
             * Destroy the list before we move on. Why bother, since we'll
             * do it anyway in the finish block? Because of bug 125408.
             * If we delete the cert and private key while traversing certs,
             * we'll delete the public key too, and then we'll crash when
             * we free the same public key while destroying the list.
             */
            SECKEY_DestroyPublicKeyList(pubkList);
            pubkList = NULL;
        }
    }

    /*
     * Get all certs
     */
    if( objectTypes & CERT ) {
        CERTCertListNode *node = NULL;

        certList = PK11_ListCertsInSlot(slot);
        if( certList == NULL ) {
            JSS_throwMsg(env, TOKEN_EXCEPTION,
                "Failed to list certificates on token");
            goto finish;
        }

        for( node = CERT_LIST_HEAD(certList);
             ! CERT_LIST_END(node, certList);
             node = CERT_LIST_NEXT(node) )
        {
            travstat = cb(env, slot, CERT, (void*) node->cert, data);
            if( travstat.status != PR_SUCCESS ) {
                goto finish;
            }
            if( travstat.deleteIt ) {
                /*
                 * Since, in a KeyStore, certs and private keys go together,
                 * remove the private key too, if there is one.
                 *
                 * The hack here is that PK11_DeleteTokenCertAndKey will
                 * not delete the cert if there is no matching private key.
                 * We want to the cert to be deleted even if the key isn't
                 * there. So we only call that function if we're sure the
                 * key is there. Otherwise we delete the cert directly.
                 */
                SECKEYPrivateKey *privKey = PK11_FindKeyByAnyCert(node->cert, 
                    NULL /*wincx*/);
                PRBool keyPresent = (privKey != NULL);
                SECKEY_DestroyPrivateKey(privKey);
                if( keyPresent ) {
                    PK11_DeleteTokenCertAndKey(node->cert, NULL /*wincx*/);
                } else {
                    SEC_DeletePermCertificate(node->cert);
                }
                PR_REMOVE_LINK(&node->links);
                /* node is allocated from the list's arena, it will get freed
                 * with the list */
            }
            if( travstat.stopIterating ) {
                goto stop_early;
            }
        }
    }

stop_early:
    status = PR_SUCCESS;
finish:
    if( privkList != NULL ) {
        SECKEY_DestroyPrivateKeyList(privkList);
    }
    if( pubkList != NULL ) {
        SECKEY_DestroyPublicKeyList(pubkList);
    }
    while( symKey != NULL ) {
        PK11SymKey *deadKey;
        deadKey = symKey;
        symKey = PK11_GetNextSymKey(symKey);
        PK11_FreeSymKey(deadKey);
    }
    if( certList != NULL ) {
        CERT_DestroyCertList(certList);
    }
    return status;
}

/* allocates memory if type != CERT */
static char*
getObjectNick(void *obj, TokenObjectType type)
{
    switch(type) {
      case PRIVKEY:
        /* NOTE: this function allocates memory for the nickname */
        return PK11_GetPrivateKeyNickname((SECKEYPrivateKey*)obj);
      case SYMKEY:
        /* NOTE: this function allocates memory for the nickname */
        return PK11_GetSymKeyNickname((PK11SymKey*)obj);
      case PUBKEY:
        /* NOTE: this function allocates memory for the nickname */
        return PK11_GetPublicKeyNickname((SECKEYPublicKey*)obj);
      case CERT:
        return ((CERTCertificate*)obj)->nickname;
      default:
        PR_ASSERT(PR_FALSE);
        return NULL;
    }
}

static void
freeObjectNick(char *nick, TokenObjectType type)
{
    if( type != CERT && nick != NULL) {
        PR_Free(nick);
    }
}

typedef struct {
    jobject setObj;
    jmethodID setAdd;
} EngineAliasesCBInfo;

static JSSTraversalStatus
engineAliasesTraversalCallback
(JNIEnv *env, PK11SlotInfo *slot, TokenObjectType type, void *obj, void *data)
{
    JSSTraversalStatus travStat = INIT_TRAVSTAT;
    EngineAliasesCBInfo *cbinfo = (EngineAliasesCBInfo*)data;
    char *nickname=NULL;

    nickname = getObjectNick(obj, type);

    if( nickname != NULL ) {
        jstring nickString;

        /* convert it to a string */
        nickString = (*env)->NewStringUTF(env, nickname);
        freeObjectNick(nickname, type);
        if( nickString == NULL ) {
            /* exception was thrown */
            goto finish;
        }

        /* store the string in the vector */
        (*env)->CallBooleanMethod(env, cbinfo->setObj,
            cbinfo->setAdd, nickString);
        if( (*env)->ExceptionOccurred(env) ) {
            goto finish;
        }
    }

    travStat.status = PR_SUCCESS;
finish:
    return travStat;
}

JNIEXPORT jobject JNICALL
Java_org_mozilla_jss_provider_java_security_JSSKeyStoreSpi_getRawAliases
    (JNIEnv *env, jobject this)
{
    PK11SlotInfo *slot = NULL;
    jobject setObj=NULL;
    jmethodID setAdd=NULL;
    EngineAliasesCBInfo cbinfo;

    if( getTokenSlotPtr(env, this, &slot) != PR_SUCCESS ) {
        /* exception was thrown */
        goto finish;
    }

    /*
     * Create the vector object and get its method IDs
     */
    {
        jclass setClass;
        jmethodID setCons;

        setClass = (*env)->FindClass(env, "java/util/HashSet");
        if( setClass == NULL ) {
            /* should have thrown an exception */
            goto finish;
        }
        setCons = (*env)->GetMethodID(env, setClass, "<init>", "()V");
        if( setCons == NULL ) {
            goto finish;
        }

        setObj = (*env)->NewObject(env, setClass, setCons);
        if( setObj == NULL ) {
            goto finish;
        }

        setAdd = (*env)->GetMethodID(env, setClass, "add",
            "(Ljava/lang/Object;)Z");
        if( setAdd == NULL ) {
            goto finish;
        }
    }

    cbinfo.setObj = setObj;
    cbinfo.setAdd = setAdd;

    /*
     * traverse all slot objects
     */
    if( traverseTokenObjects(   env,
                                slot,
                                engineAliasesTraversalCallback,
                                ALL_OBJECT_TYPES,
                                &cbinfo) 
            != PR_SUCCESS )
    {
        goto finish;
    }

finish:
    return setObj;
}

typedef struct {
    const char *targetNickname;
} EngineDeleteEntryCBInfo;

static JSSTraversalStatus
engineDeleteEntryTraversalCallback
    (JNIEnv *env, PK11SlotInfo *slot, TokenObjectType type,
     void *obj, void *data)
{
    EngineDeleteEntryCBInfo *cbinfo = (EngineDeleteEntryCBInfo*)data;
    JSSTraversalStatus status = INIT_TRAVSTAT;
    char *nickname = getObjectNick(obj, type);

    if( nickname != NULL &&
        (PL_strcmp(nickname, cbinfo->targetNickname) == 0) ) {
        status.deleteIt = PR_TRUE;
    }
    freeObjectNick(nickname, type);
    status.status = PR_SUCCESS;
    return status;
}
        

JNIEXPORT void JNICALL
Java_org_mozilla_jss_provider_java_security_JSSKeyStoreSpi_engineDeleteEntry
    (JNIEnv *env, jobject this, jobject aliasString)
{
    EngineDeleteEntryCBInfo cbinfo;
    PK11SlotInfo *slot = NULL;

    cbinfo.targetNickname = NULL;

    if( getTokenSlotPtr(env, this, &slot) != PR_SUCCESS ) {
        /* exception was thrown */
        goto finish;
    }

    /* get the nickname */
    cbinfo.targetNickname = (*env)->GetStringUTFChars(env, aliasString, NULL);
    if( cbinfo.targetNickname == NULL ) {
        goto finish;
    }

    /* traverse */
    /*
     * traverse all slot objects
     */
    traverseTokenObjects(   env,
                            slot,
                            engineDeleteEntryTraversalCallback,
                            ALL_OBJECT_TYPES,
                            (void*) &cbinfo);

finish:
    if( cbinfo.targetNickname != NULL ) {
        (*env)->ReleaseStringUTFChars(env, aliasString, cbinfo.targetNickname);
    }
}

typedef struct {
    const char *targetNickname;
    CERTCertificate *cert;
} EngineGetCertificateCBInfo;

static JSSTraversalStatus
engineGetCertificateTraversalCallback
    (JNIEnv *env, PK11SlotInfo *slot, TokenObjectType type,
     void *obj, void *data)
{
    JSSTraversalStatus travStat = INIT_TRAVSTAT;
    CERTCertificate *cert = (CERTCertificate*) obj;
    EngineGetCertificateCBInfo *cbinfo = (EngineGetCertificateCBInfo*)data;

    PR_ASSERT(type == CERT); /* shouldn't get called otherwise */
    PR_ASSERT(cbinfo->cert == NULL);

    if( cert->nickname != NULL &&
        PL_strcmp(cert->nickname, cbinfo->targetNickname) == 0 )
    {
        cbinfo->cert = CERT_DupCertificate(cert);
        travStat.stopIterating = PR_TRUE;
    }
    travStat.status = PR_SUCCESS;

    return travStat;
}
        
static CERTCertificate*
lookupCertByNickname(JNIEnv *env, jobject this, jstring alias)
{
    PK11SlotInfo *slot;
    EngineGetCertificateCBInfo cbinfo = {NULL,NULL};
    jbyteArray derCertBA = NULL;
    PRStatus status = PR_FAILURE;

    if( alias == NULL ) goto finish;

    if( getTokenSlotPtr(env, this, &slot) != PR_SUCCESS ) {
        /* exception was thrown */
        goto finish;
    }

    cbinfo.targetNickname = (*env)->GetStringUTFChars(env, alias, NULL);
    if(cbinfo.targetNickname == NULL ) goto finish;

    status  = traverseTokenObjects( env,
                                    slot,
                                    engineGetCertificateTraversalCallback,
                                    CERT,
                                    (void*) &cbinfo);

finish:
    if( cbinfo.targetNickname != NULL ) {
        (*env)->ReleaseStringUTFChars(env, alias, cbinfo.targetNickname);
    }
    if( status != PR_SUCCESS && cbinfo.cert != NULL) {
        CERT_DestroyCertificate(cbinfo.cert);
        cbinfo.cert = NULL;
    }
    return cbinfo.cert;
}

JNIEXPORT jobject JNICALL
Java_org_mozilla_jss_provider_java_security_JSSKeyStoreSpi_getCertObject
    (JNIEnv *env, jobject this, jstring alias)
{
    CERTCertificate *cert = NULL;
    jobject certObj = NULL;

    cert = lookupCertByNickname(env, this, alias);
    if( cert == NULL ) {
        goto finish;
    }

    certObj = JSS_PK11_wrapCert(env, &cert);

finish:
    if( cert != NULL ) {
        CERT_DestroyCertificate(cert);
    }
    if( certObj == NULL ) {
        PR_ASSERT( (*env)->ExceptionOccurred(env) );
    }
    return certObj;
}

JNIEXPORT jbyteArray JNICALL
Java_org_mozilla_jss_provider_java_security_JSSKeyStoreSpi_getDERCert
    (JNIEnv *env, jobject this, jstring alias)
{
    CERTCertificate * cert = NULL;
    jbyteArray derCertBA = NULL;

    cert = lookupCertByNickname(env, this, alias);
    if( cert == NULL ) {
        goto finish;
    }

    derCertBA = JSS_SECItemToByteArray(env, &cert->derCert);

finish:
    if( cert != NULL) {
        CERT_DestroyCertificate(cert);
    }
    if( derCertBA == NULL ) {
        PR_ASSERT( (*env)->ExceptionOccurred(env) );
    }
    return derCertBA;
}

JNIEXPORT jstring JNICALL
Java_org_mozilla_jss_provider_java_security_JSSKeyStoreSpi_getCertNickname
    (JNIEnv *env, jobject this, jbyteArray derCertBA)
{
    PK11SlotInfo *slot=NULL;
    SECItem *derCert=NULL;
    CERTCertificate * cert=NULL;
    CERTCertificate searchCert;
    jstring nickString = NULL;

    if( getTokenSlotPtr(env, this, &slot) != PR_SUCCESS ) {
        /* exception was thrown */
        goto finish;
    }

    /* convert derCert to SECItem */
    derCert = JSS_ByteArrayToSECItem(env, derCertBA);
    if( derCert == NULL ) {
        /* exception was thrown */
        goto finish;
    }

    /* lookup cert by derCert */
    searchCert.derCert = *derCert;
    cert = PK11_FindCertFromDERCert(slot, &searchCert, NULL /*wincx*/);
    if( cert == NULL ) {
        /* not found. return NULL */
        goto finish;
    }

    /* convert cert nickname to Java string */
    nickString = (*env)->NewStringUTF(env, cert->nickname);

finish:
    if( derCert != NULL ) {
        SECITEM_FreeItem(derCert, PR_TRUE /*freeit*/);
    }
    if( cert != NULL ) {
        CERT_DestroyCertificate(cert);
    }
    return nickString;
}

typedef struct {
    const char *label;
    SECKEYPrivateKey *privk;
    PK11SymKey *symk;
} FindKeyCBInfo;

static JSSTraversalStatus
findKeyCallback
(JNIEnv *env, PK11SlotInfo *slot, TokenObjectType type, void *obj, void *data)
{
    JSSTraversalStatus status = INIT_TRAVSTAT;
    FindKeyCBInfo *cbinfo = (FindKeyCBInfo*)data;
    const char *objNick = getObjectNick(obj, type);

    status.status = PR_SUCCESS;

    PR_ASSERT(cbinfo->privk==NULL && cbinfo->symk==NULL);

    if( PL_strcmp(objNick, cbinfo->label) == 0 ) {
        /* found it */
        status.stopIterating = PR_TRUE;
        switch( type ) {
          case PRIVKEY:
            cbinfo->privk = (SECKEYPrivateKey*)obj;
            break;
          case SYMKEY:
            cbinfo->symk = (PK11SymKey*)obj;
            break;
          default:
            PR_ASSERT(PR_FALSE);
            status.status = PR_FAILURE;
        }
    }

    freeObjectNick(obj, type);
    return status;
}

typedef struct {
    const char *targetNickname;
    SECKEYPrivateKey *privk;
} GetKeyByCertNickCBInfo;

static JSSTraversalStatus
getKeyByCertNickCallback
    (JNIEnv *env, PK11SlotInfo *slot, TokenObjectType type,
     void *obj, void *data)
{
    JSSTraversalStatus travStat = INIT_TRAVSTAT;
    CERTCertificate *cert = (CERTCertificate*) obj;
    GetKeyByCertNickCBInfo *cbinfo = (GetKeyByCertNickCBInfo*) data;

    PR_ASSERT(type == CERT); /* shouldn't get called otherwise */
    PR_ASSERT(cbinfo->privk == NULL);

    if( cert->nickname != NULL &&
        PL_strcmp(cert->nickname, cbinfo->targetNickname) == 0 )
    {
        travStat.stopIterating = PR_TRUE;

        cbinfo->privk = PK11_FindPrivateKeyFromCert(slot, cert, NULL /*wincx*/);
        if( cbinfo->privk ) {
            printf("Found private key from cert with label '%s'\n",
                cert->nickname);
        }
    }
    travStat.status = PR_SUCCESS;

    return travStat;
}
        

JNIEXPORT jobject JNICALL
Java_org_mozilla_jss_provider_java_security_JSSKeyStoreSpi_engineGetKey
    (JNIEnv *env, jobject this, jstring alias, jcharArray password)
{
    PK11SlotInfo *slot=NULL;
    FindKeyCBInfo keyCbinfo = {NULL, NULL, NULL};
    GetKeyByCertNickCBInfo certCbinfo = {NULL, NULL};
    jobject keyObj = NULL;

    if( getTokenSlotPtr(env, this, &slot) != PR_SUCCESS ) {
        /* exception was thrown */
        goto finish;
    }

    if( alias == NULL ) {
        goto finish;
    }

    /*
     * first look for a matching key
     */

    keyCbinfo.label = (*env)->GetStringUTFChars(env, alias, NULL);
    if( keyCbinfo.label == NULL ) {
        goto finish;
    }

    if( traverseTokenObjects(   env,
                                slot,
                                findKeyCallback,
                                PRIVKEY | SYMKEY,
                                &keyCbinfo)
            != PR_SUCCESS )
    {
        goto finish;
    }

    if(keyCbinfo.privk != NULL ) {
        /* found a matching private key */
        keyObj = JSS_PK11_wrapPrivKey(env, &keyCbinfo.privk);
    }  else if( keyCbinfo.symk != NULL ) {
        /* found a matching symmetric key */
        keyObj = JSS_PK11_wrapSymKey(env, &keyCbinfo.symk);
    }

    if( keyObj == NULL ) {
        /*
         * If we didn't find a matching key, look for a matching cert
         */

        certCbinfo.targetNickname = (*env)->GetStringUTFChars(env, alias, NULL);
        if(certCbinfo.targetNickname == NULL ) goto finish;

        if( traverseTokenObjects(   env,
                                    slot,
                                    getKeyByCertNickCallback,
                                    CERT,
                                    (void*) &certCbinfo)
                != PR_SUCCESS )
        {
            goto finish;
        }

        if( certCbinfo.privk != NULL ) {
            keyObj = JSS_PK11_wrapPrivKey(env, &certCbinfo.privk);
        }

    }

finish:
    if( keyCbinfo.label != NULL ) {
        (*env)->ReleaseStringUTFChars(env, alias, keyCbinfo.label);
    }
    if( certCbinfo.targetNickname != NULL ) {
        (*env)->ReleaseStringUTFChars(env, alias, certCbinfo.targetNickname);
    }
    PR_ASSERT( keyCbinfo.privk==NULL && keyCbinfo.symk==NULL);
    PR_ASSERT( certCbinfo.privk==NULL );

    return keyObj;
}

JNIEXPORT jboolean JNICALL
Java_org_mozilla_jss_provider_java_security_JSSKeyStoreSpi_engineIsCertificateEntry
    (JNIEnv *env, jobject this, jstring alias)
{
    PK11SlotInfo *slot;
    EngineGetCertificateCBInfo cbinfo = {NULL,NULL};
    jboolean retVal = JNI_FALSE;
    SECKEYPrivateKey *privk = NULL;

    if( alias == NULL ) goto finish;

    if( getTokenSlotPtr(env, this, &slot) != PR_SUCCESS ) {
        /* exception was thrown */
        goto finish;
    }

    cbinfo.targetNickname = (*env)->GetStringUTFChars(env, alias, NULL);
    if(cbinfo.targetNickname == NULL ) goto finish;

    if( traverseTokenObjects(   env,
                                slot,
                                engineGetCertificateTraversalCallback,
                                CERT,
                                (void*) &cbinfo)
            != PR_SUCCESS )
    {
        goto finish;
    }

    if( cbinfo.cert != NULL ) {
        unsigned int allTrust;
        CERTCertTrust trust;
        SECStatus status;

        status = CERT_GetCertTrust(cbinfo.cert, &trust);
        if( status != SECSuccess ) {
            goto finish;
        }
        allTrust = trust.sslFlags | trust.emailFlags | trust.objectSigningFlags;

        if( (allTrust & (CERTDB_TRUSTED | CERTDB_TRUSTED_CA |
                         CERTDB_NS_TRUSTED_CA | CERTDB_TRUSTED_CLIENT_CA))
            && !(allTrust & CERTDB_USER) )
        {
            /* It's a trusted cert and not a user cert. */
            retVal = JNI_TRUE;
        }
    }

finish:
    if( cbinfo.targetNickname != NULL ) {
        (*env)->ReleaseStringUTFChars(env, alias, cbinfo.targetNickname);
    }
    if( cbinfo.cert != NULL) {
        CERT_DestroyCertificate(cbinfo.cert);
    }
    return retVal;
}

JNIEXPORT void JNICALL
Java_org_mozilla_jss_provider_java_security_JSSKeyStoreSpi_engineSetKeyEntryNative
    (JNIEnv *env, jobject this, jstring alias, jobject keyObj,
        jcharArray password, jobjectArray certChain)
{
    jclass privkClass, symkClass;
    const char *nickname = NULL;
    SECKEYPrivateKey *tokenPrivk=NULL;
    PK11SymKey *tokenSymk=NULL;

    if( keyObj==NULL || alias==NULL) {
        JSS_throw(env, NULL_POINTER_EXCEPTION);
        goto finish;
    }

    nickname = (*env)->GetStringUTFChars(env, alias, NULL);
    if( nickname == NULL ) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }

    privkClass = (*env)->FindClass(env, PK11PRIVKEY_CLASS_NAME);
    symkClass = (*env)->FindClass(env, PK11SYMKEY_CLASS_NAME);
    if( privkClass==NULL || symkClass==NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }

    if( (*env)->IsInstanceOf(env, keyObj, privkClass) ) {
        SECKEYPrivateKey *privk;

        if( JSS_PK11_getPrivKeyPtr(env, keyObj, &privk) != PR_SUCCESS ) {
            JSS_throwMsg(env, KEYSTORE_EXCEPTION,
                "Failed to extract NSS key from private key object");
            goto finish;
        }

        tokenPrivk = PK11_ConvertSessionPrivKeyToTokenPrivKey(privk, NULL);
        if( tokenPrivk == NULL ) {
            JSS_throwMsgPrErr(env, KEYSTORE_EXCEPTION,
                "Failed to copy private key to permanent token object");
            goto finish;
        }
        if( PK11_SetPrivateKeyNickname(tokenPrivk, nickname) != SECSuccess ) {
            JSS_throwMsg(env, KEYSTORE_EXCEPTION,
                "Failed to set alias of copied private key");
            goto finish;
        }
    } else if( (*env)->IsInstanceOf(env, keyObj, symkClass) ) {
        PK11SymKey *symk;

        if( JSS_PK11_getSymKeyPtr(env, keyObj, &symk) != PR_SUCCESS ) {
            JSS_throwMsg(env, KEYSTORE_EXCEPTION,
                "Failed to extract NSS key from symmetric key object");
            goto finish;
        }

        if( PK11_SetSymKeyNickname(symk, nickname) != SECSuccess ) {
            JSS_throwMsg(env, KEYSTORE_EXCEPTION,
                "Failed to set alias of symmetric key");
            goto finish;
        }
        tokenSymk = PK11_ConvertSessionSymKeyToTokenSymKey(symk, NULL);
        if( tokenSymk == NULL ) {
            JSS_throwMsgPrErr(env, KEYSTORE_EXCEPTION,
                "Failed to copy symmetric key to permanent token object");
            goto finish;
        }
    } else {
        JSS_throwMsg(env, KEYSTORE_EXCEPTION,
            "Unrecognized key type: must be JSS private key (PK11PrivKey) or"
            " JSS symmetric key (PK11SymKey)");
        goto finish;
    }

finish:
    if( nickname != NULL ) {
        (*env)->ReleaseStringUTFChars(env, alias, nickname);
    }
    if( tokenPrivk != NULL ) {
        SECKEY_DestroyPrivateKey(tokenPrivk);
    }
    if( tokenSymk != NULL ) {
        PK11_FreeSymKey(tokenSymk);
    }
}
