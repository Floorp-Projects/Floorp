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

#include "_jni/org_mozilla_jss_pkcs11_PK11Token.h"

#include <plarena.h>
#include <secmodt.h>
#include <pk11func.h>
#include <pk11pqg.h>
#include <secerr.h>
#include <nspr.h>
#include <key.h>
#include <secasn1.h>
#include <base64.h>
#include <cert.h>
#include <cryptohi.h>
#include <pqgutil.h>

#include <jssutil.h>
#include <jss_exceptions.h>
#include <jss_bigint.h>
#include <Algorithm.h>

#include <secitem.h>
#include "java_ids.h"

#include "pk11util.h"
#include <plstr.h>

#define ERRX 1
#define KEYTYPE_DSA_STRING "dsa"
#define KEYTYPE_RSA_STRING "rsa"
#define KEYTYPE_DSA 1
#define KEYTYPE_RSA 2

static CERTCertificateRequest* make_cert_request(JNIEnv *env, 
			 const char *subject, SECKEYPublicKey *pubk);
void GenerateCertRequest(JNIEnv *env, unsigned int ktype, const char *subject,
			 int keysize, PK11SlotInfo *slot,
			 unsigned char **b64request, PQGParams *dsaParams);
static SECStatus GenerateKeyPair(JNIEnv *env, unsigned int ktype,
				 PK11SlotInfo *slot,
			 SECKEYPublicKey **pubk,
			 SECKEYPrivateKey **privk, int keysize, PQGParams *dsaParams);

/* these values are taken from PK11KeyPairGenerator.java */
#define DEFAULT_RSA_KEY_SIZE 2048
#define DEFAULT_RSA_PUBLIC_EXPONENT 0x10001

/***********************************************************************
 *
 * J S S _ P K 1 1 _ w r a p P K 1 1 T o k e n
 *
 * Create a PK11Token object from a PKCS #11 slot.
 *
 * slot is a pointer to a PKCS #11 slot, which must not be NULL.  It will
 *  be eaten by the wrapper, so you can't use it after you call this.
 *
 * Returns a new PK11Token object, or NULL if an exception was thrown.
 */
jobject
JSS_PK11_wrapPK11Token(JNIEnv *env, PK11SlotInfo **slot)
{
    jclass tokenClass;
    jmethodID constructor;
    jbyteArray byteArray;
	jobject Token=NULL;
    jboolean internal;
    jboolean keyStorage;

    PR_ASSERT(env!=NULL && slot!=NULL && *slot!=NULL);

    internal = (*slot == PK11_GetInternalSlot());
    keyStorage = (*slot == PK11_GetInternalKeySlot());

    byteArray = JSS_ptrToByteArray(env, (void*)*slot);

    /*
     * Lookup the class and constructor
     */
    tokenClass = (*env)->FindClass(env, PK11TOKEN_CLASS_NAME);
    if(tokenClass == NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }

    constructor = (*env)->GetMethodID(
						env,
						tokenClass,
						PK11TOKEN_CONSTRUCTOR_NAME,
						PK11TOKEN_CONSTRUCTOR_SIG);
    if(constructor == NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }

    /* Call the constructor */
    Token = (*env)->NewObject(env,
                              tokenClass,
                              constructor,
                              byteArray,
                              internal,
                              keyStorage);

finish:
	if(Token==NULL) {
		PK11_FreeSlot(*slot);
	}
	*slot = NULL;
    return Token;
}

#if 0
/*
 * PK11Token.needsLogin
 * Returns true if this token needs to be logged in before it can be used.
 */
JNIEXPORT jboolean JNICALL Java_org_mozilla_jss_pkcs11_PK11Token_needsLogin
  (JNIEnv *env, jobject this)
{
    PK11SlotInfo *slot;
    jboolean retval;

    PR_ASSERT(env!=NULL && this!=NULL);

    if(JSS_PK11_getTokenSlotPtr(env, this, &slot) != PR_SUCCESS) {
        PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL);
        /* an exception was thrown */
        retval = JNI_FALSE;
        goto finish;
    }
    PR_ASSERT(slot != NULL);

    if(PK11_NeedLogin(slot) == PR_TRUE) {
        retval = JNI_TRUE;
    } else {
        retval = JNI_FALSE;
    }

finish:
    return retval;
}
#endif

/************************************************************************
 *
 * P K 1 1 T o k e n . i s L o g g e d I n
 *
 * Returns true if this token is logged in.
 */
JNIEXPORT jboolean JNICALL Java_org_mozilla_jss_pkcs11_PK11Token_isLoggedIn
  (JNIEnv *env, jobject this)
{
    PK11SlotInfo *slot;
    jboolean retval;

    PR_ASSERT(env!=NULL && this!=NULL);

    if(JSS_PK11_getTokenSlotPtr(env, this, &slot) != PR_SUCCESS) {
        PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL);
        /* an exception was thrown */
        retval =  JNI_FALSE;
        goto finish;
    }
    PR_ASSERT(slot != NULL);

    if(PK11_IsLoggedIn(slot, NULL) == PR_TRUE) {
        retval = JNI_TRUE;
    } else {
        retval = JNI_FALSE;
    }

finish:
    return retval;
}

/************************************************************************
 *
 * P K 1 1 T o k e n . n a t i v e L o g i n
 *
 * Attempts to login to the token with the given password callback.
 * Throws NotInitializedException if the token is not initialized.
 * Throws IncorrectPINException if the supplied PIN is incorrect.
 */
JNIEXPORT void JNICALL Java_org_mozilla_jss_pkcs11_PK11Token_nativeLogin
  (JNIEnv *env, jobject this, jobject callback)
{
    PK11SlotInfo *slot;

    PR_ASSERT(env!=NULL && this!=NULL && callback!=NULL);

    if(JSS_PK11_getTokenSlotPtr(env, this, &slot) != PR_SUCCESS) {
        PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL);
        /* an exception was thrown */
        goto finish;
    }
    PR_ASSERT(slot != NULL);

    /* Is the token initialized? */
    if(PK11_NeedUserInit(slot)) {
        JSS_nativeThrow(
            env,
			TOKEN_NOT_INITIALIZED_EXCEPTION
        );
        goto finish;
    }

    if(PK11_Authenticate(slot, PR_TRUE /*loadCerts*/, (void*)callback)
                            != SECSuccess)
    {
        JSS_nativeThrow(env, INCORRECT_PASSWORD_EXCEPTION);
        goto finish;
    }

    /* Success! */
finish:
    return;
}

/************************************************************************
 *
 * P K 1 1 T o k e n . l o g o u t
 */
JNIEXPORT void JNICALL Java_org_mozilla_jss_pkcs11_PK11Token_logout
  (JNIEnv *env, jobject this)
{
    PK11SlotInfo *slot;

    PR_ASSERT(env!=NULL && this!=NULL);


    if( JSS_PK11_getTokenSlotPtr(env, this, &slot) != PR_SUCCESS) {
        PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL);
        /* an exception occurred */
        goto finish;
    }
    PR_ASSERT(slot != NULL);

    if( PK11_Logout(slot) != SECSuccess) {
        JSS_nativeThrowMsg( env,
			TOKEN_EXCEPTION,
            "Unable to logout token"
        );
        goto finish;
    }

finish:
    return;
}

/************************************************************************
 *
 * P K 1 1 T o k e n . g e t L o g i n M o d e
 *
 * Returns the login mode (ONE_TIME, TIMEOUT, EVERY_TIME) for this token.
 * These constants must sync with the ones defined in CryptoToken.
 */
#define ONE_TIME 0
#define TIMEOUT 1
#define EVERY_TIME 2

/************************************************************************
 *
 * P K 1 1 T o k e n . g e t L o g i n M o d e
 *
 */
JNIEXPORT jint JNICALL
Java_org_mozilla_jss_pkcs11_PK11Token_getLoginMode
  (JNIEnv *env, jobject this)
{
    PK11SlotInfo *slot;
    jint mode=ONE_TIME;
    int askpw;
    int timeout;

    PR_ASSERT(env!=NULL && this!=NULL);

    if( JSS_PK11_getTokenSlotPtr(env, this, &slot) != PR_SUCCESS) {
        PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL);
        goto finish;
    }
    PR_ASSERT(slot!=NULL);

    PK11_GetSlotPWValues(slot, &askpw, &timeout);

    if(askpw == -1) {
        mode = EVERY_TIME;
    } else if(askpw == 0) {
        mode = ONE_TIME;
    } else if(askpw == 1) {
        mode = TIMEOUT;
    } else {
        JSS_throw(env, TOKEN_EXCEPTION);
        goto finish;
    }

finish:
    return mode;
}

/************************************************************************
 *
 * P K 1 1 T o k e n . g e t L o g i n T i m e o u t M i n u t e s
 */
JNIEXPORT jint JNICALL
Java_org_mozilla_jss_pkcs11_PK11Token_getLoginTimeoutMinutes
  (JNIEnv *env, jobject this)
{
    PK11SlotInfo *slot;
    int askpw;
    int timeout=0;

    PR_ASSERT(env!=NULL && this!=NULL);

    if( JSS_PK11_getTokenSlotPtr(env, this, &slot) != PR_SUCCESS) {
        PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL);
        goto finish;
    }
    PR_ASSERT(slot!=NULL);

    PK11_GetSlotPWValues(slot, &askpw, &timeout);

finish:
    return timeout;
}


/************************************************************************
 *
 * P K 1 1 T o k e n . s e t L o g i n M o d e
 *
 */
JNIEXPORT void JNICALL
Java_org_mozilla_jss_pkcs11_PK11Token_setLoginMode
  (JNIEnv *env, jobject this, jint mode)
{
    PK11SlotInfo *slot;
    int askpw, timeout;

    PR_ASSERT(env!=NULL && this!=NULL);

    if( JSS_PK11_getTokenSlotPtr(env, this, &slot) != PR_SUCCESS) {
        PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL);
        goto finish;
    }
    PR_ASSERT(slot!=NULL);

    PK11_GetSlotPWValues(slot, &askpw, &timeout);

    if(mode == EVERY_TIME) {
        askpw = -1;
    } else if(mode == ONE_TIME) {
        askpw = 0;
    } else if(mode == TIMEOUT) {
        askpw = 1;
    } else {
        JSS_throw(env, TOKEN_EXCEPTION);
        goto finish;
    }

    PK11_SetSlotPWValues(slot, askpw, timeout);

finish:
    ;
}

/************************************************************************
 *
 * P K 1 1 T o k e n . s e t L o g i n T i m e o u t M i n u t e s
 *
 */
JNIEXPORT void JNICALL
Java_org_mozilla_jss_pkcs11_PK11Token_setLoginTimeoutMinutes
  (JNIEnv *env, jobject this, jint newTimeout)
{
    PK11SlotInfo *slot;
    int askpw, timeout;

    PR_ASSERT(env!=NULL && this!=NULL);

    if( JSS_PK11_getTokenSlotPtr(env, this, &slot) != PR_SUCCESS) {
        PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL);
        return;
    }
    PR_ASSERT(slot!=NULL);

    PK11_GetSlotPWValues(slot, &askpw, &timeout);
    PK11_SetSlotPWValues(slot, askpw, newTimeout);

}

/************************************************************************
 *
 * P K 1 1 T o k e n . i s P r e s e n t
 *
 * Returns true if this token is present in the slot
 */
JNIEXPORT jboolean JNICALL Java_org_mozilla_jss_pkcs11_PK11Token_isPresent
  (JNIEnv *env, jobject this)
{
    PK11SlotInfo *slot;
    jboolean retval;

    PR_ASSERT(env!=NULL && this!=NULL);

    if(JSS_PK11_getTokenSlotPtr(env, this, &slot) != PR_SUCCESS) {
        PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL);
        /* an exception was thrown */
        retval = JNI_FALSE;
        goto finish;
    }
    PR_ASSERT(slot != NULL);

    if(PK11_IsPresent(slot) == PR_TRUE) {
        retval = JNI_TRUE;
    } else {
        retval = JNI_FALSE;
    }

finish:
    return retval;
}

/************************************************************************
 *
 * P K 1 1 T o k e n . P W I n i t a b l e
 *
 * Make sure this token can be initialized.  Currently we only check the
 * internal module, since it can't be initialized more than once.
 * Presumably most tokens can have their PINs initialized arbitrarily
 * many times.
 */
JNIEXPORT jboolean JNICALL Java_org_mozilla_jss_pkcs11_PK11Token_PWInitable
	(JNIEnv *env, jobject this)
{
	PK11SlotInfo *slot=NULL;
	jboolean initable=JNI_FALSE;

	PR_ASSERT(env!=NULL && this!=NULL);

    /*
	 * Convert Java objects to native objects
	 */
    if(JSS_PK11_getTokenSlotPtr(env, this, &slot) != PR_SUCCESS) {
        PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL);
        /* an exception was thrown */
        goto finish;
    }
	PR_ASSERT(slot!=NULL);

	if(slot != PK11_GetInternalKeySlot()) {
		/* We don't know about other tokens */
		initable = JNI_TRUE;
	} else {
		if(PK11_NeedUserInit(slot)) {
			initable = JNI_TRUE;
		} else {
			/* Internal module only allows one init, after this you
			 * need to do a changePIN */
			initable = JNI_FALSE;
		}
	}

finish:
	return initable;
}

/************************************************************************
 *
 * P K 1 1 T o k e n . i n i t P a s s w o r d
 * 
 */
JNIEXPORT void JNICALL Java_org_mozilla_jss_pkcs11_PK11Token_initPassword
  (JNIEnv *env, jobject this, jbyteArray ssopw, jbyteArray userpw)
{
    PK11SlotInfo *slot=NULL;
    char *szSsopw=NULL, *szUserpw=NULL;
	jboolean ssoIsCopy, userIsCopy;
    SECStatus initResult;
    PRErrorCode error;

    PR_ASSERT(env!=NULL && this!=NULL && ssopw!=NULL && userpw!=NULL);

    /*
	 * Convert Java objects to native objects
	 */
    if(JSS_PK11_getTokenSlotPtr(env, this, &slot) != PR_SUCCESS) {
        PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL);
        /* an exception was thrown */
        goto finish;
    }
    szSsopw = (char*) (*env)->GetByteArrayElements(env, ssopw, &ssoIsCopy);
    szUserpw = (char*) (*env)->GetByteArrayElements(env, userpw, &userIsCopy);
    PR_ASSERT(slot!=NULL && szSsopw!=NULL && szUserpw!=NULL);

	/*
	 * If we're on the internal module, make sure we can still be 
	 * initialized.
	 */
	if(slot == PK11_GetInternalKeySlot() && !PK11_NeedUserInit(slot)) {
		JSS_nativeThrowMsg(env, ALREADY_INITIALIZED_EXCEPTION,
			"Netscape Internal Key Token is already initialized");
		goto finish;
	}

    /*
	 * Initialize the PIN
	 */
    initResult = PK11_InitPin(slot, szSsopw, szUserpw);
    if(initResult != SECSuccess) {
        error = PR_GetError();
    }

    /*
	 * Throw exception if an error occurred
	 */
    if(initResult != SECSuccess) {
        if(error == SEC_ERROR_BAD_PASSWORD) {
            JSS_nativeThrowMsg(
                env,
				INCORRECT_PASSWORD_EXCEPTION,
                "Incorrect security officer PIN"
            );
        } else {
            JSS_nativeThrowMsg(
                env,
				TOKEN_EXCEPTION,
                "Unable to initialize PIN"
            );
        }
    }

finish:
    /*
	 * Free native objects
	 */
    if(szSsopw) {
		if(ssoIsCopy) {
			JSS_wipeCharArray(szSsopw);
		}
		/* JNI_ABORT means don't copy back changes */
		(*env)->ReleaseByteArrayElements(env, ssopw, (jbyte*)szSsopw,
										JNI_ABORT);
    }
    if(szUserpw) {
		if(userIsCopy) {
			JSS_wipeCharArray(szUserpw);
		}
		(*env)->ReleaseByteArrayElements(env, userpw, (jbyte*)szUserpw,
										JNI_ABORT);
    }

    return;
}

/************************************************************************
 *
 * P K 1 1 T o k e n . p a s s w o r d I s I n i t i a l i z e d
 *
 */
JNIEXPORT jboolean JNICALL
Java_org_mozilla_jss_pkcs11_PK11Token_passwordIsInitialized
  (JNIEnv *env, jobject this)
{
	PK11SlotInfo *slot=NULL;
	jboolean isInitialized = JNI_FALSE;

	PR_ASSERT(env!=NULL && this!=NULL);

	/*
	 * Convert Java arguments to C
	 */
	if( JSS_PK11_getTokenSlotPtr(env, this, &slot) != PR_SUCCESS) {
		PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL);
		goto finish;
	}
	PR_ASSERT(slot != NULL);

	if(slot == PK11_GetInternalKeySlot()) {
		/* special case for our Key slot */
		isInitialized = ! PK11_NeedPWInit();
	} else {
		isInitialized = ! PK11_NeedUserInit(slot);
	}

finish:
	return isInitialized;
}


/************************************************************************
 *
 * P K 1 1 T o k e n . c h a n g e P a s s w o r d
 *
 */
JNIEXPORT void JNICALL Java_org_mozilla_jss_pkcs11_PK11Token_changePassword
  (JNIEnv *env, jobject this, jbyteArray oldPIN, jbyteArray newPIN)
{
    PK11SlotInfo *slot=NULL;
    char *szOldPIN=NULL, *szNewPIN=NULL;
	jboolean oldIsCopy, newIsCopy;
    SECStatus changeResult;
    PRErrorCode error;

    PR_ASSERT(env!=NULL && this!=NULL && oldPIN!=NULL && newPIN!=NULL);

    /*
	 * Convert Java objects to native objects
	 */
    if(JSS_PK11_getTokenSlotPtr(env, this, &slot) != PR_SUCCESS) {
        PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL);
        /* an exception was thrown */
        goto finish;
    }
    szOldPIN= (char*) (*env)->GetByteArrayElements(env, oldPIN, &oldIsCopy);
    szNewPIN= (char*) (*env)->GetByteArrayElements(env, newPIN, &newIsCopy);
    PR_ASSERT(slot!=NULL && szOldPIN!=NULL && szNewPIN!=NULL);

    /*
	 * Change the PIN
	 */
    changeResult = PK11_ChangePW(slot, szOldPIN, szNewPIN);
    if(changeResult != SECSuccess) {
        error = PR_GetError();
    }

    /*
	 * Throw exception if an error occurred
	 */
    if(changeResult != SECSuccess) {
        if(error == SEC_ERROR_BAD_PASSWORD) {
            JSS_nativeThrowMsg(
                env,
				INCORRECT_PASSWORD_EXCEPTION,
                "Incorrect PIN"
            );
        } else {
            JSS_nativeThrowMsg(
                env,
				TOKEN_EXCEPTION,
                "Unable to change PIN"
            );
        }
    }

finish:
    /* Free native objects */
    if(szOldPIN) {
		if(oldIsCopy) {
			JSS_wipeCharArray(szOldPIN);
		}
		/* JNI_ABORT means don't copy back changes */
        (*env)->ReleaseByteArrayElements(env, oldPIN, (jbyte*)szOldPIN,
										JNI_ABORT);
    }
    if(szNewPIN) {
		if(newIsCopy) {
			JSS_wipeCharArray(szNewPIN);
		}
        (*env)->ReleaseByteArrayElements(env, newPIN, (jbyte*)szNewPIN,
										JNI_ABORT);
    }

    return;
}

typedef enum { SSOPW, USERPW } pwType;

/************************************************************************
 *
 * p a s s w o r d I s C o r r e c t
 *
 * Checks the given password, which could be an SSO or a user password.
 * Returns JNI_TRUE if the password is correct, JNI_FALSE if it isn't,
 * and may throw an exception if something goes wrong on the token.
 */
static jboolean
passwordIsCorrect
  (JNIEnv *env, jobject this, jbyteArray password, pwType type)
{
	PK11SlotInfo *slot = NULL;
	char *pwBytes=NULL;
	jboolean isCopy;
	jboolean pwIsCorrect = JNI_FALSE;
	SECStatus status;
	int error;

	PR_ASSERT(env!=NULL && this!=NULL && password!=NULL);

	/*
	 * Convert Java args to C
	 */
	if( JSS_PK11_getTokenSlotPtr(env, this, &slot) != PR_SUCCESS) {
		PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL);
		goto finish;
	}
	pwBytes = (char*) (*env)->GetByteArrayElements(env, password, &isCopy);
	PR_ASSERT(slot!=NULL && pwBytes != NULL);

	/*
	 * Do the check
	 */
	if(type == SSOPW) {
		status = PK11_CheckSSOPassword(slot, pwBytes);
	} else {
		status = PK11_CheckUserPassword(slot, pwBytes);
	}
	if(status == SECSuccess) {
		pwIsCorrect = JNI_TRUE;
	} else {
		error = PR_GetError();
		if(error == SEC_ERROR_BAD_PASSWORD) {
			/* just wrong password */
			pwIsCorrect = JNI_FALSE;
		} else {
			/* some token problem */
			JSS_nativeThrowMsg(env, TOKEN_EXCEPTION,
				"Unable to check password");
			goto finish;
		}
	}

finish:
	/* Free native objects */
	if(pwBytes != NULL) {
		if(isCopy) {
			JSS_wipeCharArray(pwBytes);
		}
		/* JNI_ABORT means don't copy back changes */
		(*env)->ReleaseByteArrayElements(env, password, (jbyte*)pwBytes,
											JNI_ABORT);
	}

	return pwIsCorrect;
}

/************************************************************************
 *
 * P K 1 1 T o k e n . u s e r P a s s w o r d I s C o r r e c t
 *
 */
JNIEXPORT jboolean JNICALL
Java_org_mozilla_jss_pkcs11_PK11Token_userPasswordIsCorrect
  (JNIEnv *env, jobject this, jbyteArray password)
{
	return passwordIsCorrect(env, this, password, USERPW);
}

/************************************************************************
 *
 * P K 1 1 T o k e n . S S O P a s s w o r d I s C o r r e c t
 *
 */
JNIEXPORT jboolean JNICALL
Java_org_mozilla_jss_pkcs11_PK11Token_SSOPasswordIsCorrect
  (JNIEnv *env, jobject this, jbyteArray password)
{
	return  passwordIsCorrect(env, this, password, SSOPW);
}

/************************************************************************
 *
 * P K 1 1 T o k e n . g e t N a m e
 *
 * Returns the name of this token.
 */
JNIEXPORT jstring JNICALL Java_org_mozilla_jss_pkcs11_PK11Token_getName
  (JNIEnv *env, jobject this)
{
    char *szName;
    PK11SlotInfo *slot;
    jstring name;
    jstring retval;

    PR_ASSERT(env!=NULL && this!=NULL);

    if(JSS_PK11_getTokenSlotPtr(env, this, &slot) != PR_SUCCESS) {
        PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL );
        /* an exception was thrown */
        retval = NULL;
        goto finish;
    }
    PR_ASSERT(slot != NULL);

    szName = PK11_GetTokenName(slot);
    /* NULL C string converts to empty Java string */
    if(szName == NULL) {
        szName = "";
    }

    name = (*env)->NewStringUTF(env, szName);

    /* name could be NULL, if an OutOfMemoryError occurred. In that case,
     * we just return NULL and let the exception be thrown in Java */
#ifdef DEBUG
    if(name == NULL) {
        /* Make sure there is an exception before we return NULL */
        PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL);
    }
#endif

    retval = name;

finish:
    return retval;
}

/************************************************************************
 *
 * T o k e n P r o x y . r e l e a s e N a t i v e R e s o u r c e s
 *
 * Free the PK11SlotInfo structure that underlies my token.
 */
JNIEXPORT void JNICALL
Java_org_mozilla_jss_pkcs11_TokenProxy_releaseNativeResources
  (JNIEnv *env, jobject this)
{
    PK11SlotInfo *slot;

    PR_ASSERT(env!=NULL && this!=NULL);

    if(JSS_getPtrFromProxy(env, this, (void**)&slot) != PR_SUCCESS) {
        PR_ASSERT( PR_FALSE );
        goto finish;
    }
    PR_ASSERT(slot != NULL);

    /* this actually only frees the data structure if we were the last
     * reference to it */
    PK11_FreeSlot(slot);

finish:
    return;
}

/************************************************************************
 *
 * J S S _ g e t T o k e n S l o t P t r
 *
 * Retrieve the PK11SlotInfo pointer of the given token.
 *
 * tokenObject: A reference to a Java PK11Token
 * ptr: address of a PK11SlotInfo* that will be loaded with the PK11SlotInfo
 *      pointer of the given token.
 * returns: PR_SUCCESS if the operation was successful, PR_FAILURE if an
 *      exception was thrown. 
 */
PRStatus
JSS_PK11_getTokenSlotPtr(JNIEnv *env, jobject tokenObject, PK11SlotInfo **ptr)
{
    PR_ASSERT(env != NULL && tokenObject != NULL && ptr != NULL);

    /*
     * Get the pointer from the PK11Token, which has a Token Proxy member
     */
    PR_ASSERT(sizeof(PK11SlotInfo*) == sizeof(void*));
    return JSS_getPtrFromProxyOwner(env, tokenObject, PK11TOKEN_PROXY_FIELD,
		PK11TOKEN_PROXY_SIG, (void**)ptr);
}

/**********************************************************************
 *
 * PK11Token.doesAlgorithm
 *
 */
JNIEXPORT jboolean JNICALL
Java_org_mozilla_jss_pkcs11_PK11Token_doesAlgorithm
  (JNIEnv *env, jobject this, jobject alg)
{
	PK11SlotInfo *slot;
    CK_MECHANISM_TYPE mech;
	jboolean doesMech = JNI_FALSE;

	if( JSS_PK11_getTokenSlotPtr(env, this, &slot) != PR_SUCCESS) {
		PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL);
		goto finish;
	}

    mech = JSS_getPK11MechFromAlg(env, alg);
    PR_ASSERT( mech != CKM_INVALID_MECHANISM );

	if( PK11_DoesMechanism(slot, mech) == PR_TRUE) {
		doesMech = JNI_TRUE;
	}

    /* HACK...exceptions */
    if( PK11_IsInternal(slot) && mech == CKM_PBA_SHA1_WITH_SHA1_HMAC ) {
        /* Although the internal slot doesn't actually do this mechanism,
         * we do it by hand in PK11KeyGenerator.c */
        doesMech = JNI_TRUE;
    }

finish:
	return doesMech;
}

/***********************************************************************
 *
 * PK11Token.isWritable
 *
 */
JNIEXPORT jboolean JNICALL
Java_org_mozilla_jss_pkcs11_PK11Token_isWritable
    (JNIEnv *env, jobject this)
{
    PK11SlotInfo *slot;

    if( JSS_PK11_getTokenSlotPtr(env, this, &slot) != PR_SUCCESS) {
        return JNI_FALSE; /* an exception was thrown, so it doesn't matter */
    }

    return ! PK11_IsReadOnly(slot);
}

#define ZERO_SECITEM(item) {(item).len=0; (item).data=NULL;}
int PK11_NumberObjectsFor(PK11SlotInfo*, CK_ATTRIBUTE*, int);

/************************************************************************
 *
 * P K 1 1 T o k e n . getCertRequest
 */
JNIEXPORT jstring JNICALL Java_org_mozilla_jss_pkcs11_PK11Token_generatePK10
  (JNIEnv *env, jobject this, jstring subject, jint keysize,
   jstring keyType, jbyteArray P, jbyteArray Q, jbyteArray G)
{
    PK11SlotInfo *slot;
    const char* c_subject;
    jboolean isCopy;
    unsigned char *b64request;
    SECItem p, q, g;
    PQGParams *dsaParams=NULL;
    const char* c_keyType;
    jboolean k_isCopy;
    unsigned int ktype = 0;

    PR_ASSERT(env!=NULL && this!=NULL);
    /* get keytype */
    c_keyType = (*env)->GetStringUTFChars(env, keyType, &k_isCopy);

    if (0 == PL_strcasecmp(c_keyType, KEYTYPE_DSA_STRING)) {
      ktype = KEYTYPE_DSA;
    } else if (0 == PL_strcasecmp(c_keyType, KEYTYPE_RSA_STRING)) {
      ktype = KEYTYPE_RSA;
    } else {
      JSS_throw(env, INVALID_PARAMETER_EXCEPTION);
    }

    if (ktype == KEYTYPE_DSA) {
      if (P==NULL || Q==NULL || G ==NULL) {
	/* shouldn't happen */
	JSS_throw(env, INVALID_PARAMETER_EXCEPTION);
      } else {
	/* caller supplies PQG */
	/* zero these so we can free them indiscriminately later */
	ZERO_SECITEM(p);
	ZERO_SECITEM(q);
	ZERO_SECITEM(g);

	if( JSS_ByteArrayToOctetString(env, P, &p) ||
	    JSS_ByteArrayToOctetString(env, Q, &q) ||
	    JSS_ByteArrayToOctetString(env, G, &g) )
	  {
	    PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL);
	    goto finish;
	  }
	dsaParams = PK11_PQG_NewParams(&p, &q, &g);
	if(dsaParams == NULL) {
	  JSS_throw(env, OUT_OF_MEMORY_ERROR);
	  goto finish;
	}
	
      }
    } /* end ktype == KEYTYPE_DSA */

    if( JSS_PK11_getTokenSlotPtr(env, this, &slot) != PR_SUCCESS) {
        PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL);
        /* an exception occurred */
        goto finish;
    }

    PR_ASSERT(slot != NULL); 
    if(PK11_IsLoggedIn(slot, NULL) != PR_TRUE) {
      JSS_nativeThrowMsg(env, TOKEN_EXCEPTION,
			 "token not logged in");
    }

    /* get subject */
    c_subject = (*env)->GetStringUTFChars(env, subject, &isCopy);

    /* get keysize */

    /* call GenerateCertRequest() */
    GenerateCertRequest(env, ktype, c_subject, (int) keysize, slot, &b64request,
			dsaParams);

finish:
    if (isCopy == JNI_TRUE) {
      /* release memory */
      (*env)->ReleaseStringUTFChars(env, subject, c_subject);
    }

    if (k_isCopy == JNI_TRUE) {
      /* release memory */
      (*env)->ReleaseStringUTFChars(env, keyType, c_keyType);
    }

    if (ktype == KEYTYPE_DSA) {
      SECITEM_FreeItem(&p, PR_FALSE);
      SECITEM_FreeItem(&q, PR_FALSE);
      SECITEM_FreeItem(&g, PR_FALSE);
      PK11_PQG_DestroyParams(dsaParams);
    }

    if (b64request == NULL) {
      return NULL;
    } else {
      /* convert to String */
      return (*env)->NewStringUTF(env, (const char *)b64request);
    }
}

/*
 *  generate keypairs and further the request
 *  the real thing will need keytype and pqg numbers (for DSA)
 */
void
GenerateCertRequest(JNIEnv *env, 
		    unsigned int ktype, const char *subject, int keysize,
		    PK11SlotInfo *slot,
		     unsigned char **b64request,
		    PQGParams *dsaParams) {

	CERTCertificateRequest *req;

	SECKEYPrivateKey *privk = NULL;
	SECKEYPublicKey *pubk = NULL;
	SECStatus rv;
	PRArenaPool *arena;
	SECItem result_der, result;
	SECItem *blob;

#ifdef DEBUG      
	printf("in GenerateCertRequest(), subject=%s, keysize = %d",
	       subject, keysize);
#endif

	if( GenerateKeyPair(env, ktype, slot, &pubk, &privk, keysize,
			    dsaParams) != SECSuccess) {
#ifdef DEBUG      
		printf("Error generating keypair.");
#endif
	}
#ifdef DEBUG      
	printf("before make_cert_request");
#endif

	req = make_cert_request (env, subject, pubk);

#ifdef DEBUG      
	printf("after make_cert_request");
#endif
	if (req == NULL) {
	  return;
	}

	result_der.len = 0;
	result_der.data = NULL;
#ifdef DEBUG      
	printf("before SEC_ASN1EncodeItem");
#endif

	/* der encode the request */
	blob = SEC_ASN1EncodeItem(req->arena, &result_der, req,
				  SEC_ASN1_GET(CERT_CertificateRequestTemplate));

	/* sign the request */
	arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
	if ( !arena ) {
	  JSS_throw(env, OUT_OF_MEMORY_ERROR);
	  return;
	}

	rv = SEC_DerSignData(arena, &result, result_der.data, result_der.len,
			     privk,
                         (ktype==KEYTYPE_RSA)? SEC_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION:SEC_OID_ANSIX9_DSA_SIGNATURE_WITH_SHA1_DIGEST);
	if (rv) {
	  JSS_nativeThrowMsg(env, TOKEN_EXCEPTION,
			     "signing of data failed");
	  return;
	}

	*b64request = (unsigned char*) BTOA_DataToAscii(result.data, result.len);
#ifdef DEBUG      
	printf("data = %s\n", *b64request);
#endif
}

/******************************************************************
 *
 * m a k e _ c e r t _ r e q u e s t
 */
static CERTCertificateRequest*
make_cert_request(JNIEnv *env, const char *subject, SECKEYPublicKey *pubk)
{
	CERTName *subj;
	CERTSubjectPublicKeyInfo *spki;

	CERTCertificateRequest *req;

	/* Create info about public key */
	spki = SECKEY_CreateSubjectPublicKeyInfo(pubk);
	if (!spki) {
	  JSS_nativeThrowMsg(env, TOKEN_EXCEPTION,
			     "unable to create subject public key");
	  return NULL;
	}

	subj = CERT_AsciiToName ((char *)subject);    
	if(subj == NULL) {
	  JSS_nativeThrowMsg(env, TOKEN_EXCEPTION,
		"Invalid data in certificate description");
	}

	/* Generate certificate request */
	req = CERT_CreateCertificateRequest(subj, spki, 0);
	if (!req) {
	  JSS_nativeThrowMsg(env, TOKEN_EXCEPTION,
			     "unable to make certificate request");
	  return NULL;
	}  

#ifdef DEBUG      
	printf("certificate request generated\n");
#endif

	return req;
}


/******************************************************************
 *
 * G e n e r a t e K e y P a i r
 */
static SECStatus
GenerateKeyPair(JNIEnv *env, unsigned int ktype, PK11SlotInfo *slot, SECKEYPublicKey **pubk,
	SECKEYPrivateKey **privk, int keysize, PQGParams *dsaParams)
{

  PK11RSAGenParams rsaParams;

  if (ktype == KEYTYPE_RSA) {
    if( keysize == -1 ) {
      rsaParams.keySizeInBits = DEFAULT_RSA_KEY_SIZE;
    } else {
      rsaParams.keySizeInBits = keysize;
    }
    rsaParams.pe = DEFAULT_RSA_PUBLIC_EXPONENT;
  }
  if(PK11_Authenticate( slot, PR_FALSE /*loadCerts*/, NULL /*wincx*/)
     != SECSuccess) {
    JSS_nativeThrowMsg(env, TOKEN_EXCEPTION,
		       "failure authenticating to key database");
    return SECFailure;
  }

  if(PK11_NeedUserInit(slot)) {
    JSS_nativeThrowMsg(env, TOKEN_EXCEPTION,
		       "token not initialized with password");
    return SECFailure;
  }

#ifdef DEBUG      
  printf("key type == %d", ktype);
#endif
  if (ktype == KEYTYPE_RSA) {
    *privk = PK11_GenerateKeyPair (slot,
		   CKM_RSA_PKCS_KEY_PAIR_GEN, &rsaParams, 
				   pubk, PR_TRUE /*isPerm*/, PR_TRUE /*isSensitive*/, NULL);
  } else { /* dsa */
    *privk = PK11_GenerateKeyPair (slot,
		   CKM_DSA_KEY_PAIR_GEN, (void *)dsaParams, 
		   pubk, PR_TRUE /*isPerm*/, PR_TRUE /*isSensitive*/, NULL);
    }
    if( *privk == NULL ) {
      int errLength;
      char *errBuf;
      char *msgBuf;

      errLength = PR_GetErrorTextLength();
      if(errLength > 0) {
	errBuf = PR_Malloc(errLength);
	if(errBuf == NULL) {
	  JSS_throw(env, OUT_OF_MEMORY_ERROR);
	  return SECFailure;
	}
	PR_GetErrorText(errBuf);
      }
      msgBuf = PR_smprintf("Keypair Generation failed on token: %s",
			   errLength>0? errBuf : "");
      if(errLength>0) {
	PR_Free(errBuf);
      }

      JSS_throwMsg(env, TOKEN_EXCEPTION, msgBuf);
      PR_Free(msgBuf);
      return SECFailure;
    }


    if (*privk != NULL && *pubk != NULL) {
#ifdef DEBUG      
      printf("generated public/private key pair\n");
#endif
    } else {
      JSS_nativeThrowMsg(env, TOKEN_EXCEPTION,
			 "failure generating key pair");
      return SECFailure;
    }
    
    return SECSuccess;
}
