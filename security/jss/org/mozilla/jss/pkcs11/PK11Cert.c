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

#include "_jni/org_mozilla_jss_pkcs11_CertProxy.h"

#include <nspr.h>
#include <plarena.h>
#include <seccomon.h>
#include <secmodt.h>
#include <certt.h>
#include <cert.h>
#include <pk11func.h>
#include <secerr.h>
#include <secder.h>
#include <key.h>

#include <jss_exceptions.h>
#include <jss_bigint.h>
#include <java_ids.h>
#include "pk11util.h"
#include <jssutil.h>


/*
 * Class:     org_mozilla_jss_pkcs11_PK11Cert
 * Method:    getEncoded
 * Signature: ()[B
 */
JNIEXPORT jbyteArray JNICALL Java_org_mozilla_jss_pkcs11_PK11Cert_getEncoded
  (JNIEnv *env, jobject this)
{
	PRThread *pThread;
	CERTCertificate *cert;
	SECItem *derCert;
	jbyteArray derArray=NULL;
	jbyte *pByte;

	pThread = PR_AttachThread(PR_SYSTEM_THREAD, 0, NULL);
	PR_ASSERT(pThread != NULL);

	PR_ASSERT(env!=NULL && this!=NULL);

	/*
	 * extract the DER cert from the CERTCertificate*
	 */
	if( JSS_PK11_getCertPtr(env, this, &cert) != PR_SUCCESS) {
		PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL);
		goto finish;
	}
	PR_ASSERT(cert != NULL);

	derCert = &cert->derCert;
	/* the SECItem type does not have to be siDERCertBuffer */
	if(derCert->data==NULL || derCert->len<1) {
		JSS_throw(env, CERTIFICATE_ENCODING_EXCEPTION);
		goto finish;
	}

	/*
	 * Copy the DER data to a new Java byte array
	 */
	derArray = (*env)->NewByteArray(env, derCert->len);
	if(derArray==NULL) {
		JSS_throw(env, OUT_OF_MEMORY_ERROR);
		goto finish;
	}
	pByte = (*env)->GetByteArrayElements(env, derArray, NULL);
	if(pByte==NULL) {
		JSS_throw(env, OUT_OF_MEMORY_ERROR);
		goto finish;
	}
	memcpy(pByte, derCert->data, derCert->len);
	(*env)->ReleaseByteArrayElements(env, derArray, pByte, 0);

finish:
	PR_DetachThread();
	return derArray;
}

/*
 * Class:     org_mozilla_jss_pkcs11_PK11Cert
 * Method:    getVersion
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_mozilla_jss_pkcs11_PK11Cert_getVersion
  (JNIEnv *env, jobject this)
{
	PRThread *pThread;
	CERTCertificate *cert;
	long lVersion;

	pThread = PR_AttachThread(PR_SYSTEM_THREAD, 0, NULL);
	PR_ASSERT(pThread != NULL);

	PR_ASSERT(env!=NULL && this!=NULL);

	/*
	 * Get the version from the CERTCertificate *
	 */
	if( JSS_PK11_getCertPtr(env, this, &cert) != PR_SUCCESS) {
		PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL);
		goto finish;
	}
	PR_ASSERT(cert!=NULL);

	if(cert->version.data==NULL || cert->version.len<=0) {
        /* default value is 0 */
        lVersion = 0;
        goto finish;
    }

	lVersion = DER_GetInteger(&cert->version);

	/* jint is 2s complement 32 bits.  The max value is 0111...111. */
	PR_ASSERT( (lVersion >= 0L) && (lVersion < (long)0x7fffffff) );

finish:
	PR_DetachThread();
	return (jint) lVersion;
}

/******************************************************************
 *
 * C e r t P r o x y . g e t P u b l i c K e y 
 *
 * Extracts the SECKEYPublicKey from the CERTCertificate, wraps it
 * in a Java wrapper, and returns it.
 */
JNIEXPORT jobject JNICALL
Java_org_mozilla_jss_pkcs11_PK11Cert_getPublicKey
	(JNIEnv *env, jobject this)
{
	CERTCertificate *cert;
	SECKEYPublicKey *pubk=NULL;
	PRThread *pThread;
	jobject pubKey=NULL;

	PR_ASSERT(env!=NULL && this!=NULL);

	pThread = PR_AttachThread(PR_SYSTEM_THREAD, 0, NULL);
	PR_ASSERT(pThread != NULL);

	if( JSS_PK11_getCertPtr(env, this, &cert) != PR_SUCCESS) {
		PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL);
		goto finish;
	}

	pubk = CERT_ExtractPublicKey(cert);
	if(pubk==NULL) {
		PR_ASSERT( PR_GetError() == SEC_ERROR_NO_MEMORY);
		JSS_throw(env, OUT_OF_MEMORY_ERROR);
		goto finish;
	}

	pubKey = JSS_PK11_wrapPubKey(env, &pubk);
	if(pubKey == NULL) {
		PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL);
		goto finish;
	}

finish:
	if(pubk!=NULL) {
		SECKEY_DestroyPublicKey(pubk);
	}
	PR_DetachThread();
	return pubKey;
}

/******************************************************************
 *
 * C e r t P r o x y . r e l e a s e N a t i v e R e s o u r c e s
 *
 * Calls CERT_DestroyCertificate on the underlying CERTCertificate.
 */
JNIEXPORT void JNICALL
Java_org_mozilla_jss_pkcs11_CertProxy_releaseNativeResources
  (JNIEnv *env, jobject this)
{
	CERTCertificate *cert;
	PRThread *pThread;

	PR_ASSERT(env!=NULL && this!=NULL);

	pThread = PR_AttachThread(PR_SYSTEM_THREAD, 0, NULL);
	PR_ASSERT(pThread != NULL);

	/* Get the CERTCertificate structure */
	if(JSS_getPtrFromProxy(env, this, (void**)&cert) != PR_SUCCESS) {
		PR_ASSERT( PR_FALSE );
		goto finish;
	}
	PR_ASSERT(cert != NULL);

	CERT_DestroyCertificate(cert);

finish:
	PR_DetachThread();
}
	

/******************************************************************
 *
 * J S S _ P K 1 1 _ g e t C e r t P t r
 *
 * Given a Certificate object, extracts the CERTCertificate* and
 * stores it at the given address.
 *
 * certObject: A JNI reference to a JSS Certificate object.
 * ptr: Address of a CERTCertificate* that will receive the pointer.
 * Returns: PR_SUCCESS for success, PR_FAILURE if an exception was thrown.
 */
PRStatus
JSS_PK11_getCertPtr(JNIEnv *env, jobject certObject, CERTCertificate **ptr)
{
	PR_ASSERT(env!=NULL && certObject!=NULL && ptr!=NULL);

	/* Get the pointer from the cert proxy */
	return JSS_getPtrFromProxyOwner(env, certObject, CERT_PROXY_FIELD,
			CERT_PROXY_SIG, (void**)ptr);
}

/******************************************************************
 *
 * J S S _ P K 1 1 _ g e t C e r t S l o t P t r
 *
 * Given a Certificate object, extracts the PK11SlotInfo* and
 * stores it at the given address.
 *
 * certObject: A JNI reference to a JSS Certificate object.
 * ptr: Address of a PK11SlotInfo* that will receive the pointer.
 * Returns: PR_SUCCESS for success, PR_FAILURE if an exception was thrown.
 */
PRStatus
JSS_PK11_getCertSlotPtr(JNIEnv *env, jobject certObject, PK11SlotInfo **ptr)
{
	PR_ASSERT(env!=NULL && certObject!=NULL && ptr!=NULL);

	/* Get the pointer from the token proxy */
	return JSS_getPtrFromProxyOwner(env, certObject, PK11TOKEN_PROXY_FIELD,
			PK11TOKEN_PROXY_SIG, (void**)ptr);
}

/*
 * This is a shady way of deciding if the cert is a user cert.
 * Hopefully it will work. What we used to do was check for cert->slot.
 */
#define isUserCert(cert) \
    ( ((cert)->trust->sslFlags           & CERTDB_USER) || \
      ((cert)->trust->emailFlags         & CERTDB_USER) || \
      ((cert)->trust->objectSigningFlags & CERTDB_USER) )

/****************************************************************
 *
 * f i n d S l o t B y T o k e n N a m e A n d C e r t
 *
 * Find the slot containing the token with the given name
 * and cert.
 */
static PK11SlotInfo *
findSlotByTokenNameAndCert(char *name, CERTCertificate *cert)
{
    PK11SlotList *list;
    PK11SlotListElement *le;
    PK11SlotInfo *slot = NULL;

    list = PK11_GetAllTokens(CKM_INVALID_MECHANISM, PR_FALSE, PR_FALSE, NULL);
    if(list == NULL) {
        return NULL;
    }

    for(le = list->head; le; le = le->next) {
        if( (PORT_Strcmp(PK11_GetTokenName(le->slot),name) == 0) &&
                (PK11_FindCertInSlot(le->slot,cert,NULL) !=
                CK_INVALID_HANDLE)) {
            slot = PK11_ReferenceSlot(le->slot);
            break;
        }
    }
    PK11_FreeSlotList(list);

    if(slot == NULL) {
        PORT_SetError(SEC_ERROR_NO_TOKEN);
    }

    return slot;
}


/*************************************************************************
 *
 * J S S _ P K 1 1 _ f i n d C e r t A n d S l o t F r o m N i c k n a m e
 *
 * A variant of NSS's PK11_FindCertFromNickname function that also
 * returns a PK11SlotInfo* in *ppSlot.
 *
 * If nickname is of the format "token:nickname", the slot that
 * contains the specified token is returned.  Otherwise the internal
 * key slot (which contains the permanent database token) is returned.
 */
CERTCertificate *
JSS_PK11_findCertAndSlotFromNickname(char *nickname, void *wincx,
    PK11SlotInfo **ppSlot)
{
    CERTCertificate *cert;

    cert = PK11_FindCertFromNickname(nickname, wincx);
    if(cert == NULL) {
        return NULL;
    }
    if( PORT_Strchr(nickname, ':')) {
        char* tokenname = PORT_Strdup(nickname);
        char* colon = PORT_Strchr(tokenname, ':');
        *colon = '\0';
        *ppSlot = findSlotByTokenNameAndCert(tokenname, cert);
        PORT_Free(tokenname);
        if(*ppSlot == NULL) {
            /* The token containing the cert was just removed. */
            CERT_DestroyCertificate(cert);
            return NULL;
        }
    } else {
        *ppSlot = PK11_GetInternalKeySlot();
    }
    return cert;
}


/***************************************************************************
 *
 * J S S _ P K 1 1 _ f i n d C e r t s A n d S l o t F r o m N i c k n a m e
 *
 * A variant of NSS's PK11_FindCertsFromNickname function that also
 * returns a PK11SlotInfo* in *ppSlot.
 *
 * If nickname is of the format "token:nickname", the slot that
 * contains the specified token is returned.  Otherwise the internal
 * key slot (which contains the permanent database token) is returned.
 */
CERTCertList *
JSS_PK11_findCertsAndSlotFromNickname(char *nickname, void *wincx,
    PK11SlotInfo **ppSlot)
{
    CERTCertList *certList;

    certList = PK11_FindCertsFromNickname(nickname, wincx);
    if(certList == NULL) {
        return NULL;
    }
    if( PORT_Strchr(nickname, ':')) {
        char* tokenname = PORT_Strdup(nickname);
        char* colon = PORT_Strchr(tokenname, ':');
        CERTCertListNode *head = CERT_LIST_HEAD(certList);
        *colon = '\0';
        *ppSlot = findSlotByTokenNameAndCert(tokenname, head->cert);
        PORT_Free(tokenname);
        if(*ppSlot == NULL) {
            /* The token containing the certs was just removed. */
            CERT_DestroyCertList(certList);
            return NULL;
        }
    } else {
        *ppSlot = PK11_GetInternalKeySlot();
    }
    return certList;
}


/***********************************************************************
 *
 * J S S _ P K 1 1 _ w r a p C e r t A n d S l o t A n d N i c k n a m e
 *
 * Builds a Certificate wrapper around a CERTCertificate, a
 *		PK11SlotInfo, and a nickname.
 * cert: Will be eaten and erased whether the wrap was successful or not.
 * slot: Will be eaten and erased whether the wrap was successful or not.
 * nickname: the cert instance's nickname
 * returns: a new PK11Cert wrapping the CERTCertificate, PK11SlotInfo,
 *		and nickname, or NULL if an exception was thrown.
 */
jobject
JSS_PK11_wrapCertAndSlotAndNickname(JNIEnv *env, CERTCertificate **cert,
    PK11SlotInfo **slot, const char *nickname)
{
	jclass certClass;
	jmethodID constructor;
	jbyteArray certPtr;
	jbyteArray slotPtr;
	jstring jnickname = NULL;
	jobject Cert=NULL;

	PR_ASSERT(env!=NULL && cert!=NULL && *cert!=NULL
		&& slot!=NULL);

	certPtr = JSS_ptrToByteArray(env, *cert);
	slotPtr = JSS_ptrToByteArray(env, *slot);
	if (nickname) {
		jnickname = (*env)->NewStringUTF(env, nickname);
	}

	certClass = (*env)->FindClass(env, INTERNAL_TOKEN_CERT_CLASS_NAME);
	if(certClass == NULL) {
		ASSERT_OUTOFMEM(env);
		goto finish;
	}

	constructor = (*env)->GetMethodID(
							env,
							certClass,
							PLAIN_CONSTRUCTOR,
							CERT_CONSTRUCTOR_SIG);
	if(constructor == NULL) {
		ASSERT_OUTOFMEM(env);
		goto finish;
	}

	/* Call the constructor */
	Cert = (*env)->NewObject(env, certClass, constructor, certPtr,
		slotPtr, jnickname);
	if(Cert==NULL) {
		goto finish;
	}

finish:
	if(Cert==NULL) {
		CERT_DestroyCertificate(*cert);
		if(*slot!=NULL) {
			PK11_FreeSlot(*slot);
		}
	}
	*cert = NULL;
	*slot = NULL;
	return Cert;
}

/****************************************************************
 *
 * J S S _ P K 1 1 _ w r a p C e r t A n d S l o t
 *
 * Builds a Certificate wrapper around a CERTCertificate and a
 *		PK11SlotInfo.
 * cert: Will be eaten and erased whether the wrap was successful or not.
 * slot: Will be eaten and erased whether the wrap was successful or not.
 * returns: a new PK11Cert wrapping the CERTCertificate and PK11SlotInfo,
 *		or NULL if an exception was thrown.
 */
jobject
JSS_PK11_wrapCertAndSlot(JNIEnv *env, CERTCertificate **cert,
    PK11SlotInfo **slot)
{
	return JSS_PK11_wrapCertAndSlotAndNickname(env, cert, slot,
			(*cert)->nickname);
}

/****************************************************************
 *
 * J S S _ P K 1 1 _ w r a p C e r t
 *
 * Builds a Certificate wrapper around a CERTCertificate.
 * cert: Will be eaten and erased whether the wrap was successful or not.
 * returns: a new PK11Cert wrapping the CERTCertificate, or NULL if an
 * 		exception was thrown.
 *
 * Use JSS_PK11_wrapCertAndSlot instead if it is important for the PK11Cert
 * object to have the correct slot pointer or the slot pointer is readily
 * available.
 */
jobject
JSS_PK11_wrapCert(JNIEnv *env, CERTCertificate **cert)
{
	PK11SlotInfo *slot = (*cert)->slot;
	if(slot != NULL) {
		slot = PK11_ReferenceSlot(slot);
	}
	return JSS_PK11_wrapCertAndSlot(env, cert, &slot);
}

/**********************************************************************
 * PK11Cert.getOwningToken
 */
JNIEXPORT jobject JNICALL
Java_org_mozilla_jss_pkcs11_PK11Cert_getOwningToken
    (JNIEnv *env, jobject this)
{
    PK11SlotInfo *slot;
    jobject token = NULL;

    PR_ASSERT(env!=NULL && this!=NULL);

    /* get the C PK11SlotInfo structure */
    if( JSS_PK11_getCertSlotPtr(env, this, &slot) != PR_SUCCESS) {
        PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL);
        goto finish;
    }

    PR_ASSERT(slot != NULL);

    /* wrap the slot in a Java PK11Token */
    token = JSS_PK11_wrapPK11Token(env, &slot);
    if(token == NULL) {
        PR_ASSERT( (*env)->ExceptionOccurred(env) );
        goto finish;
    }

finish:
    return token;
}

/*
 * workaround for bug 100791: misspelled function prototypes in pk11func.h
 */
SECItem*
PK11_GetLowLevelKeyIDForCert(PK11SlotInfo*,CERTCertificate*,void*);

/**********************************************************************
 * PK11Cert.getUniqueID
 */
JNIEXPORT jbyteArray JNICALL
Java_org_mozilla_jss_pkcs11_PK11Cert_getUniqueID
    (JNIEnv *env, jobject this)
{
    CERTCertificate *cert;
    SECItem *id = NULL;
    jbyteArray byteArray=NULL;

    PR_ASSERT(env!=NULL && this!=NULL);

    /**************************************************
     * Get the CERTCertificate structure
     **************************************************/
    if( JSS_PK11_getCertPtr(env, this, &cert) != PR_SUCCESS) {
        goto finish;
    }

    /***************************************************
     * Get the id
     ***************************************************/
    id = PK11_GetLowLevelKeyIDForCert(NULL /*slot*/, cert, NULL/*pinarg*/);
    if( id == NULL ) {
        PR_ASSERT(PR_FALSE);
        goto finish;
    }

    /***************************************************
     * Write the id to a new byte array
     ***************************************************/
    byteArray = (*env)->NewByteArray(env, id->len);
    if(byteArray == NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }
    (*env)->SetByteArrayRegion(env, byteArray, 0, id->len, (jbyte*)id->data);
    if( (*env)->ExceptionOccurred(env) != NULL) {
        PR_ASSERT(PR_FALSE);
        goto finish;
    }

finish:
    if( id != NULL ) {
        SECITEM_FreeItem(id, PR_TRUE /*freeit*/);
    }

    return byteArray;
}

/**********************************************************************
 * This is what used to be PK11Cert.getNickname.  Kept merely to
 * satisfy the symbol export file jss.def.
 */
JNIEXPORT jstring JNICALL
Java_org_mozilla_jss_pkcs11_PK11Cert_getNickname
    (JNIEnv *env, jobject this)
{
    PR_NOT_REACHED("a stub function");
    return NULL;
}

/**********************************************************************
 * PK11Cert.setTrust
 */
JNIEXPORT void JNICALL
Java_org_mozilla_jss_pkcs11_PK11Cert_setTrust
    (JNIEnv *env, jobject this, jint type, jint newTrust)
{
    CERTCertificate *cert;
    CERTCertTrust trust;

    PR_ASSERT(env!=NULL && this!=NULL);

    if( JSS_PK11_getCertPtr(env, this, &cert) != PR_SUCCESS) {
        return;
    }

    if( CERT_GetCertTrust( cert, &trust ) != SECSuccess) {
        /* cert doesn't have any trust yet, so initialize to 0 */
        memset(&trust, 0, sizeof(trust));
    }

    switch(type) {
    case 0: /* SSL */
        trust.sslFlags = newTrust;
        break;
    case 1: /* email */
        trust.emailFlags = newTrust;
        break;
    case 2: /* object signing */
        trust.objectSigningFlags = newTrust;
        break;
    default:
        PR_ASSERT(PR_FALSE);
        return;
    }

    if( CERT_ChangeCertTrust(CERT_GetDefaultCertDB(), cert, &trust)
            != SECSuccess)
    {
        PR_ASSERT(PR_FALSE);
        return;
    }
    return;
}

/**********************************************************************
 * PK11Cert.getTrust
 */
JNIEXPORT jint JNICALL
Java_org_mozilla_jss_pkcs11_PK11Cert_getTrust
    (JNIEnv *env, jobject this, jint type)
{
    CERTCertificate *cert;
    CERTCertTrust trust;

    PR_ASSERT(env!=NULL && this!=NULL);

    if( JSS_PK11_getCertPtr(env, this, &cert) != PR_SUCCESS) {
        return 0;
    }

    if( CERT_GetCertTrust( cert, &trust ) != SECSuccess) {
        PR_ASSERT(PR_FALSE);
        return 0;
    }

    switch(type) {
    case 0: /* SSL */
        return trust.sslFlags;
    case 1: /* email */
        return trust.emailFlags;
    case 2: /* object signing */
        return trust.objectSigningFlags;
    default:
        PR_ASSERT(PR_FALSE);
        return 0;
    }
}

/**********************************************************************
 * PK11Cert.getSerialNumberByteArray
 */
JNIEXPORT jbyteArray JNICALL
Java_org_mozilla_jss_pkcs11_PK11Cert_getSerialNumberByteArray
    (JNIEnv *env, jobject this)
{
    CERTCertificate *cert;

    if( JSS_PK11_getCertPtr(env, this, &cert) != PR_SUCCESS) {
        return NULL;
    }

    PR_ASSERT(cert->serialNumber.len > 0);
    PR_ASSERT(cert->serialNumber.data != NULL);

    return JSS_OctetStringToByteArray(env, &cert->serialNumber);
}


/**********************************************************************
 * PK11Cert.getSubjectDNString
 */
JNIEXPORT jstring JNICALL
Java_org_mozilla_jss_pkcs11_PK11Cert_getSubjectDNString
    (JNIEnv *env, jobject this)
{
    CERTCertificate *cert;
    char *ascii;

    if( JSS_PK11_getCertPtr(env, this, &cert) != PR_SUCCESS) {
        return NULL;
    }

    ascii = CERT_NameToAscii(&cert->subject);

    if( ascii ) {
        return (*env)->NewStringUTF(env, ascii);
    } else {
        return NULL;
    }
}

/**********************************************************************
 * PK11Cert.getIssuerDNString
 */
JNIEXPORT jstring JNICALL
Java_org_mozilla_jss_pkcs11_PK11Cert_getIssuerDNString
    (JNIEnv *env, jobject this)
{
    CERTCertificate *cert;
    char *ascii;

    if( JSS_PK11_getCertPtr(env, this, &cert) != PR_SUCCESS) {
        return NULL;
    }

    ascii = CERT_NameToAscii(&cert->issuer);

    if( ascii ) {
        return (*env)->NewStringUTF(env, ascii);
    } else {
        return NULL;
    }
}
