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
 * J S S _ P K 1 1 _ w r a p C e r t
 *
 * Builds a Certificate wrapper around a CERTCertificate.
 * cert: Will be eaten and erased whether the wrap was successful or not.
 * returns: a new PK11Cert wrapping the CERTCertificate, or NULL if an
 * 		exception was thrown.
 */
jobject
JSS_PK11_wrapCert(JNIEnv *env, CERTCertificate **cert)
{
	jclass certClass;
	jmethodID constructor;
	jbyteArray byteArray;
	jobject Cert=NULL;
    char *className;
    PK11SlotInfo *slot = NULL;

	PR_ASSERT(env!=NULL && cert!=NULL && *cert!=NULL);

	byteArray = JSS_ptrToByteArray(env, *cert);

    /* Is this a user cert? */
    slot = PK11_KeyForCertExists(*cert, NULL /*keyPtr*/, NULL /*wincx*/);

	/*
	 * Lookup the class and constructor
	 */
    if( slot ) {
        if( (*cert)->isperm ) {
            /* it has a slot and it's in the permanent database */
            className = INTERNAL_TOKEN_CERT_CLASS_NAME;
        } else {
            /* it has a slot, but it's not in the permanent database */
            className = TOKEN_CERT_CLASS_NAME;
        }
    } else {
        if( (*cert)->isperm ) {
            /* it is in the permanent database, but has no slot */
            className = INTERNAL_CERT_CLASS_NAME;
        } else {
            /* it is not in the permanent database, and it has no slot */
            className = CERT_CLASS_NAME;
        }
    }

	certClass = (*env)->FindClass(env, className);
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
	Cert = (*env)->NewObject(env, certClass, constructor, byteArray);
	if(Cert==NULL) {
		goto finish;
	}

finish:
	if(Cert==NULL) {
		CERT_DestroyCertificate(*cert);
	}
    if( slot != NULL ) {
        PK11_FreeSlot(slot);
    }
	*cert = NULL;
	return Cert;
}

/**********************************************************************
 * PK11Cert.getOwningToken
 */
JNIEXPORT jobject JNICALL
Java_org_mozilla_jss_pkcs11_PK11Cert_getOwningToken
    (JNIEnv *env, jobject this)
{
    PK11SlotInfo *slot = NULL;
    CERTCertificate *cert;
    jobject token = NULL;

    PR_ASSERT(env!=NULL && this!=NULL);

    /* get the C Certificate structure */
    if( JSS_PK11_getCertPtr(env, this, &cert) != PR_SUCCESS) {
        PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL);
        goto finish;
    }

    /* extract the slot from the certificate */
    slot = cert->slot;
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
        JSS_throwMsg(env, TOKEN_EXCEPTION, "Unable to read ID");
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
 * PK11Cert.getNickname
 */
JNIEXPORT jstring JNICALL
Java_org_mozilla_jss_pkcs11_PK11Cert_getNickname
    (JNIEnv *env, jobject this)
{
    CERTCertificate *cert;

    PR_ASSERT(env!=NULL && this!=NULL);

    if( JSS_PK11_getCertPtr(env, this, &cert) != PR_SUCCESS) {
        return NULL;
    }
    if(cert->nickname == NULL) {
        return NULL;
    } else {
        return (*env)->NewStringUTF(env, cert->nickname);
    }
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
        PR_ASSERT(PR_FALSE);
        return;
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
