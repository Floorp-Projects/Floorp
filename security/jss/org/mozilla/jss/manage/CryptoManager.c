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
#include "_jni/org_mozilla_jss_CryptoManager.h"

#include <secitem.h>
#include <mcom_db.h>
#include <secmod.h>
#include <cert.h>
#include <certt.h>
#include <key.h>
#include <ocsp.h>
#include <pk11func.h>
#include <secrng.h>
#include <nspr.h>
#include <plstr.h>
#include <cdbhdl.h>
#include <pkcs11.h>

#include <jssutil.h>
#include <java_ids.h>
#include <jss_exceptions.h>

#include "jssinit.h"
#include "pk11util.h"

#if defined(AIX) || defined(HPUX) || defined(LINUX)
#include <signal.h>
#endif

/*
** NOTE:  We must declare a function "prototype" for the following function
**        since it is defined in the "private" NSPR 2.0 header files,
**        specifically "ns/nspr20/pr/include/private/pprthred.h".
**
** Get this thread's affinity mask.  The affinity mask is a 32 bit quantity
** marking a bit for each processor this process is allowed to run on.
** The processor mask is returned in the mask argument.
** The least-significant-bit represents processor 0.
**
** Returns 0 on success, -1 on failure.
*/
PR_EXTERN(PRInt32)
PR_GetThreadAffinityMask(PRThread *thread, PRUint32 *mask);

static jobject
makePWCBInfo(JNIEnv *env, PK11SlotInfo *slot);

static char*
getPWFromCallback(PK11SlotInfo *slot, PRBool retry, void *arg);

/*************************************************************
 * AIX, HP, and Linux signal handling madness
 *
 * In order for the JVM, kernel, and NSPR to work together, we setup
 * a signal handler for SIGCHLD that does nothing.  This is only done
 * on AIX, HP, and Linux.
 *************************************************************/
#if defined(AIX) || defined(HPUX) || defined(LINUX)

static PRStatus
handleSigChild(JNIEnv *env) {

    struct sigaction action;
    sigset_t signalset;
    int result;

    sigemptyset(&signalset);

    action.sa_handler = SIG_DFL;
    action.sa_mask = signalset;
    action.sa_flags = 0;

    result = sigaction( SIGCHLD, &action, NULL );

    if( result != 0 ) {
        JSS_throwMsg(env, GENERAL_SECURITY_EXCEPTION,
            "Failed to set SIGCHLD handler");
        return PR_FAILURE;
    }

    return PR_SUCCESS;
}

#endif
        

int ConfigureOSCP( 
		JNIEnv *env,
		CERTCertDBHandle *db,
		jboolean ocspCheckingEnabled,
        jstring ocspResponderURL,
        jstring ocspResponderCertNickname )
{
	char *ocspResponderURL_string=NULL;
	char *ocspResponderCertNickname_string=NULL;
	SECStatus status;
	int result = SECSuccess;


	/* if caller specified default responder, get the
	 * strings associated with these args
	 */

	if (ocspResponderURL) {
    	ocspResponderURL_string = 
			(char*) (*env)->GetStringUTFChars(env, ocspResponderURL, NULL);
		if (ocspResponderURL_string == NULL) {
       		JSS_throwMsg(env, GENERAL_SECURITY_EXCEPTION,
            		"OCSP invalid URL");
			result = SECFailure;
			goto loser;
		}
	}

	if (ocspResponderCertNickname) {
    	ocspResponderCertNickname_string = 
			(char*) (*env)->GetStringUTFChars(env, ocspResponderCertNickname, NULL);
		if (ocspResponderCertNickname_string == NULL) {
       		JSS_throwMsg(env, GENERAL_SECURITY_EXCEPTION,
            		"OCSP invalid nickname");
			result = SECFailure;
			goto loser;
		}
	}

	/* first disable OCSP - we'll enable it later */

	CERT_DisableOCSPChecking(db);

	/* if they set the default responder, then set it up
	 * and enable it
	 */
	if (ocspResponderURL) {
		status = 
			CERT_SetOCSPDefaultResponder(	db, 
											ocspResponderURL_string,
											ocspResponderCertNickname_string
										);
		if (status == SECFailure) {
			/* deal with error */
       		JSS_throwMsg(env, GENERAL_SECURITY_EXCEPTION,
            		"OCSP Could not set responder");
			result = SECFailure;
			goto loser;
		}
		CERT_EnableOCSPDefaultResponder(db);
	}
	else {
		/* if no defaultresponder is set, disable it */
		CERT_DisableOCSPDefaultResponder(db);
	}
		

	/* enable OCSP checking if requested */

	if (ocspCheckingEnabled) {
		CERT_EnableOCSPChecking(db);
	}
	
loser:
		
	if (ocspResponderURL_string)  {
        (*env)->ReleaseStringUTFChars(env, 
			ocspResponderURL, ocspResponderURL_string);
	}

	if (ocspResponderCertNickname_string)  {
        (*env)->ReleaseStringUTFChars(env, 
			ocspResponderCertNickname, ocspResponderCertNickname_string);
	}

	return result;

}

/***********************************************************************
 * simpleInitialize
 *
 * Initializes NSPR and the RNG only.
 *
 * RETURNS
 *  PR_SUCCESS for success, PR_FAILURE otherwise.  If not successful,
 *  an exception will be thrown.
 */
static PRStatus
simpleInitialize(JNIEnv *env)
{
    /* initialize is synchronized, so this is thread-safe */
    static PRBool initialized = PR_FALSE;

    /* initialize values used to calculate concurrency */
    PRUint32 mask = 0;
    PRUint32 template = 0x00000001;
    PRUintn  cpus = 0;
    PRUintn  concurrency = 0;

    if(initialized) {
        return PR_SUCCESS;
    }

    /* On AIX, HP, and Linux, we need to do nasty signal handling in order
     * to have NSPR play nice with the JVM and kernel.
     */
#if defined(AIX) || defined(HPUX) || defined(LINUX)
    if( handleSigChild(env) != PR_SUCCESS ) {
        return PR_FAILURE;
    }
#endif

    /* NOTE:  Removed PR_Init() function since NSPR now self-initializes. */
    /* PR_Init(PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 0); */

    /* Obtain the mask containing the number of CPUs */
    if( PR_GetThreadAffinityMask( PR_GetCurrentThread(), &mask ) ) {
        JSS_throwMsg( env, SECURITY_EXCEPTION,
                      "Failed to calculate number of CPUs" );
        return PR_FAILURE;
    }

    /* Count the bits to calculate the number of CPUs in the machine */
    while( mask != 0 ) {
        cpus  += ( mask & template );
        mask >>= 1;
    }

    /* Specify the concurrency */
#if defined(WIN32) && !defined(WIN95)  /* WINNT (fiberous) */
    /* Always specify at least a concurrency of 2 for (fiberous) Windows NT */
    if( cpus <= 1 ) {
        concurrency = 2;
    } else {
        concurrency = cpus;
    }
#else
    if( cpus <= 1 ) {
        concurrency = 1;
    } else {
        concurrency = cpus;
    }
#endif

    /* Set the concurrency */
    PR_SetConcurrency( concurrency );

    RNG_RNGInit();
    RNG_SystemInfoForRNG();

    initialized = PR_TRUE;

    return PR_SUCCESS;
}

/*
 * CryptoManager.initialize
 *
 * Initializes NSPR and the RNG only.
 */
JNIEXPORT void JNICALL
Java_org_mozilla_jss_CryptoManager_initializeNative
  (JNIEnv *env, jclass clazz)
{
    if(simpleInitialize(env) != PR_SUCCESS ) {
        PR_ASSERT( (*env)->ExceptionOccurred(env) );
        return;
    }
}

/*
 * Callback for key database name.  Name is passed in through void* argument.
 */
static char*
keyDBNameCallback(void *arg, int dbVersion)
{
    PR_ASSERT(arg!=NULL);
    if(dbVersion==3) {
        return PL_strdup((char*)arg);
    } else {
        return PL_strdup("");
    }
}

static char*
certDBNameCallback(void *arg, int dbVersion)
{
    PR_ASSERT(arg!=NULL);
    if(dbVersion == 7) {
        return PL_strdup((char*)arg);
    } else {
        return PL_strdup("");
    }
}

/**********************************************************************
 * This is the PasswordCallback object that will be used to login
 * to tokens implicitly.
 */
static jobject globalPasswordCallback = NULL;

/**********************************************************************
 * The Java virtual machine can be used to retrieve the JNI environment
 * pointer from callback functions.
 */
static JavaVM *javaVM = NULL;

/***********************************************************************
 * CryptoManager.initialize
 *
 * Initialize the security library and open all the databases.
 *
 */
JNIEXPORT void JNICALL
Java_org_mozilla_jss_CryptoManager_initializeAllNative
    (JNIEnv *env, jclass clazz,
        jstring modDBName,
        jstring keyDBName,
        jstring certDBName,
        jboolean readOnly,
        jstring manuString,
        jstring libraryString,
        jstring tokString,
        jstring keyTokString,
        jstring slotString,
        jstring keySlotString,
        jstring fipsString,
        jstring fipsKeyString,
		jboolean ocspCheckingEnabled,
		jstring ocspResponderURL,
		jstring ocspResponderCertNickname )
{
    JSS_completeInitialize(env,
            modDBName,
            keyDBName,
            certDBName,
            readOnly,
            manuString,
            libraryString,
            tokString,
            keyTokString,
            slotString,
            keySlotString,
            fipsString,
            fipsKeyString,
			ocspCheckingEnabled,
			ocspResponderURL,
			ocspResponderCertNickname
		);
}

/***********************************************************************
 * JSS_completeInitialize
 *
 * Initialize the security library and open all the databases.
 *
 */
PR_IMPLEMENT( void )
JSS_completeInitialize(JNIEnv *env,
        jstring modDBName,
        jstring keyDBName,
        jstring certDBName,
        jboolean readOnly,
        jstring manuString,
        jstring libraryString,
        jstring tokString,
        jstring keyTokString,
        jstring slotString,
        jstring keySlotString,
        jstring fipsString,
        jstring fipsKeyString,
		jboolean ocspCheckingEnabled,
		jstring ocspResponderURL,
		jstring ocspResponderCertNickname )
{
    CERTCertDBHandle *cdb_handle=NULL;
    SECKEYKeyDBHandle *kdb_handle=NULL;
    SECStatus rv = SECFailure;
    PRStatus status = PR_FAILURE;
    JavaVM *VMs[5];
    jint numVMs;
    char *szDBName = NULL; /* C string version of a database filename */
    char *manuChars=NULL;
    char *libraryChars=NULL;
    char *tokChars=NULL;
    char *keyTokChars=NULL;
    char *slotChars=NULL;
    char *keySlotChars=NULL;
    char *fipsChars=NULL;
    char *fipsKeyChars=NULL;

    /* This is thread-safe because initialize is synchronized */
    static PRBool initialized=PR_FALSE;

    /*
     * Initialize NSPR and the RNG
     */
    if( simpleInitialize(env) != PR_SUCCESS ) {
        PR_ASSERT((*env)->ExceptionOccurred(env));
        return;
    }


    PR_ASSERT(env!=NULL && modDBName!=NULL && certDBName!=NULL
        && keyDBName!=NULL);

    /* Make sure initialize() completes only once */
    if(initialized) {
        JSS_throw(env, ALREADY_INITIALIZED_EXCEPTION);
        return;
    }

    /*
     * Initialize the private key database.
     */
    szDBName = (char*) (*env)->GetStringUTFChars(env, keyDBName, NULL);
    PR_ASSERT(szDBName != NULL);
    /* Bug #299899: OpenKeyDBFilename is broken. */
    kdb_handle = SECKEY_OpenKeyDB(  readOnly,
                                    keyDBNameCallback,
                                    (void*) szDBName);
    (*env)->ReleaseStringUTFChars(env, keyDBName, szDBName);
    if (kdb_handle != NULL) {
        SECKEY_SetDefaultKeyDB(kdb_handle);
    } else {
        char *err;
        PR_smprintf(err, "Unable to open key database %s", szDBName);
        JSS_nativeThrowMsg(env, KEY_DATABASE_EXCEPTION, err);
        PR_smprintf_free(err);
        goto finish;
    }

    /*
     * Initialize the certificate database.
     */
    cdb_handle = PR_NEWZAP(CERTCertDBHandle);
    if(cdb_handle == NULL) {
        JSS_nativeThrowMsg(env,
			OUT_OF_MEMORY_ERROR,
            "creating certificate database handle");
        goto finish;
    }

    szDBName = (char*) (*env)->GetStringUTFChars(env, certDBName, NULL);
    PR_ASSERT(szDBName != NULL);
    /* Bug #299899: OpenCertDBFilename is broken. */
    rv = CERT_OpenCertDB(cdb_handle, readOnly,
            certDBNameCallback, szDBName);
    (*env)->ReleaseStringUTFChars(env, certDBName, szDBName);

    if (rv == SECSuccess) {
        CERT_SetDefaultCertDB(cdb_handle);
    } else {
        char *err;
        PR_smprintf(err, "Unable to open certificate database %s", szDBName);
        JSS_nativeThrowMsg(env, CERT_DATABASE_EXCEPTION, err);
        PR_smprintf_free(err);
        goto finish;
    }

    /*
     * Set the PKCS #11 strings
     */
    manuChars = (char*) (*env)->GetStringUTFChars(env, manuString, NULL);
    libraryChars = (char*) (*env)->GetStringUTFChars(env, libraryString, NULL);
    tokChars = (char*) (*env)->GetStringUTFChars(env, tokString, NULL);
    keyTokChars = (char*) (*env)->GetStringUTFChars(env, keyTokString, NULL);
    slotChars = (char*) (*env)->GetStringUTFChars(env, slotString, NULL);
    keySlotChars = (char*) (*env)->GetStringUTFChars(env, keySlotString, NULL);
    fipsChars = (char*) (*env)->GetStringUTFChars(env, fipsString, NULL);
    fipsKeyChars = (char*) (*env)->GetStringUTFChars(env, fipsKeyString, NULL);
    if( (*env)->ExceptionOccurred(env) ) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }
    PR_ASSERT( strlen(manuChars) == 33 );
    PR_ASSERT( strlen(libraryChars) == 33 );
    PR_ASSERT( strlen(tokChars) == 33 );
    PR_ASSERT( strlen(keyTokChars) == 33 );
    PR_ASSERT( strlen(slotChars) == 65 );
    PR_ASSERT( strlen(keySlotChars) == 65 );
    PR_ASSERT( strlen(fipsChars) == 65 );
    PR_ASSERT( strlen(fipsKeyChars) == 65 );
    PK11_ConfigurePKCS11(   PL_strdup(manuChars),
                            PL_strdup(libraryChars),
                            PL_strdup(tokChars),
                            PL_strdup(keyTokChars),
                            PL_strdup(slotChars),
                            PL_strdup(keySlotChars),
                            PL_strdup(fipsChars),
                            PL_strdup(fipsKeyChars),
                            0, /* minimum pin length */
                            PR_FALSE /* password required */
                        );

    /*
     * Open the PKCS #11 Module database
     */
    szDBName = (char *) (*env)->GetStringUTFChars(env, modDBName, NULL);
    PR_ASSERT(szDBName != NULL);
    SECMOD_init(szDBName);  	
    /* !!! SECMOD_init doesn't return an error code: Bug #262562 */
    (*env)->ReleaseStringUTFChars(env, modDBName, szDBName);

	/*
	 * Set default password callback.  This is the only place this
	 * should ever be called if you are using Ninja.
     */
	PK11_SetPasswordFunc(getPWFromCallback);

	/*
	 * Setup NSS to call the specified OCSP responder
	 */
	rv = ConfigureOSCP( 
		env,
		cdb_handle,
		ocspCheckingEnabled,
        ocspResponderURL,
        ocspResponderCertNickname );

	if (rv != SECSuccess) {
		goto finish;
	}
	

    /*
     * Save the JavaVM pointer so we can retrieve the JNI environment
     * later. This only works if there is only one Java VM.
	 */
    if( JNI_GetCreatedJavaVMs(VMs, 5, &numVMs) != 0) {
        JSS_trace(env, JSS_TRACE_ERROR,
                    "Unable to to access Java virtual machine");
        PR_ASSERT(PR_FALSE);
        goto finish;
    }
    if(numVMs != 1) {
        char *str;
        PR_smprintf(str, "Invalid number of Java VMs: %d", numVMs);
        JSS_trace(env, JSS_TRACE_ERROR, str);
        PR_smprintf_free(str);
        PR_ASSERT(PR_FALSE);
    }
    javaVM = VMs[0];

    initialized = PR_TRUE;

    status = PR_SUCCESS;

finish:
    if(status == PR_FAILURE) {
        if(cdb_handle) {
            if(CERT_GetDefaultCertDB() == cdb_handle) {
                CERT_SetDefaultCertDB(NULL);
            }
            CERT_ClosePermCertDB(cdb_handle);
            PR_Free(cdb_handle);
        }
        if(kdb_handle) {
            if(SECKEY_GetDefaultKeyDB() == kdb_handle) {
                SECKEY_SetDefaultKeyDB(NULL);
            }
            SECKEY_CloseKeyDB(kdb_handle);
            /* CloseKeyDB also frees the handle */
        }
    }

    /* LET'S BE CAREFUL.  Unbraced if statements ahead. */
    if(manuChars)
        (*env)->ReleaseStringUTFChars(env, manuString, manuChars);
    if(libraryChars)
        (*env)->ReleaseStringUTFChars(env, libraryString, libraryChars);
    if(tokChars)
        (*env)->ReleaseStringUTFChars(env, tokString, tokChars);
    if(keyTokChars)
        (*env)->ReleaseStringUTFChars(env, keyTokString, keyTokChars);
    if(slotChars)
        (*env)->ReleaseStringUTFChars(env, slotString, slotChars);
    if(keySlotChars)
        (*env)->ReleaseStringUTFChars(env, keySlotString, keySlotChars);
    if(fipsChars)
        (*env)->ReleaseStringUTFChars(env, fipsString, fipsChars);
    if(fipsKeyChars)
        (*env)->ReleaseStringUTFChars(env, fipsKeyString, fipsKeyChars);

    return;
}





/**********************************************************************
 * 
 * CryptoManager.setNativePasswordCallback
 *
 * Sets the global PasswordCallback object, which will be used to
 * login to tokens implicitly if necessary.
 *
 */
JNIEXPORT void JNICALL
Java_org_mozilla_jss_CryptoManager_setNativePasswordCallback
    (JNIEnv *env, jclass clazz, jobject callback)
{
    JSS_setPasswordCallback(env, callback);
}

/**********************************************************************
 * 
 * JSS_setPasswordCallback
 *
 * Sets the global PasswordCallback object, which will be used to
 * login to tokens implicitly if necessary.
 *
 */
PR_IMPLEMENT( void )
JSS_setPasswordCallback(JNIEnv *env, jobject callback)
{
    PR_ASSERT(env!=NULL && callback!=NULL);

    /* Free the previously-registered password callback */
    if( globalPasswordCallback != NULL ) {
        (*env)->DeleteGlobalRef(env, globalPasswordCallback);
        globalPasswordCallback = NULL;
    }

    /* Store the new password callback */
    globalPasswordCallback = (*env)->NewGlobalRef(env, callback);
    if(globalPasswordCallback == NULL) {
        JSS_throw(env, OUT_OF_MEMORY_ERROR);
    }
}

/********************************************************************
 *
 * g e t P W F r o m C a l l b a c k
 *
 * Extracts a password from a password callback and returns
 * it to PKCS #11.
 *
 * INPUTS
 *      slot
 *          The PK11SlotInfo* for the slot we are logging into.
 *      retry
 *          PR_TRUE if this is the first time we are trying to login,
 *          PR_FALSE if we tried before and our password was wrong.
 *      arg
 *          This can contain a Java PasswordCallback object reference,
 *          or NULL to use the default password callback.
 * RETURNS
 *      The password as extracted from the callback, or NULL if the
 *      callback gives up.
 */
static char*
getPWFromCallback(PK11SlotInfo *slot, PRBool retry, void *arg)
{
	jobject pwcbInfo;
    jobject pwObject;
	jbyteArray pwArray=NULL;
	char* pwchars;
	char* returnchars=NULL;
	jclass callbackClass;
    jclass passwordClass;
	jmethodID getPWMethod;
    jmethodID getByteCopyMethod;
    jmethodID clearMethod;
	jthrowable exception;
    jobject callback;
	JNIEnv *env;

	PR_ASSERT(slot!=NULL);
	if(slot==NULL) {
		return NULL;
	}

    /* Get the callback from the arg, or use the default */
    PR_ASSERT(sizeof(void*) == sizeof(jobject));
    callback = (jobject)arg;
    if(callback == NULL) {
        callback = globalPasswordCallback;
        if(callback == NULL) {
            /* No global password callback set, no way to get a password */
            return NULL;
        }
    }

    /* Get the JNI environment */
    if( (*javaVM)->AttachCurrentThread(javaVM, (void**)&env, NULL) != 0) {
        PR_ASSERT(PR_FALSE);
        goto finish;
    }
	PR_ASSERT(env != NULL);

	/*****************************************
	 * Construct the JSS_PasswordCallbackInfo
 	 *****************************************/
	pwcbInfo = makePWCBInfo(env, slot);
	if(pwcbInfo==NULL) {
		goto finish;
	}

	/*****************************************
	 * Get the callback class and methods
 	 *****************************************/
	callbackClass = (*env)->GetObjectClass(env, callback);
    if(callbackClass == NULL) {
        JSS_trace(env, JSS_TRACE_ERROR, "Failed to find password "
            "callback class");
	    PR_ASSERT(PR_FALSE);
    }
	if(retry) {
		getPWMethod = (*env)->GetMethodID(
						env,
						callbackClass,
						PW_CALLBACK_GET_PW_AGAIN_NAME,
						PW_CALLBACK_GET_PW_AGAIN_SIG);
	} else {
		getPWMethod = (*env)->GetMethodID(
						env,
						callbackClass,
						PW_CALLBACK_GET_PW_FIRST_NAME,
						PW_CALLBACK_GET_PW_FIRST_SIG);
	}
	if(getPWMethod == NULL) {
        JSS_trace(env, JSS_TRACE_ERROR,
            "Failed to find password callback accessor method");
		ASSERT_OUTOFMEM(env);
		goto finish;
	}

	/*****************************************
	 * Get the password from the callback
 	 *****************************************/
	pwObject = (*env)->CallObjectMethod(
										env,
										callback,
										getPWMethod,
										pwcbInfo);
    if( (*env)->ExceptionOccurred(env) != NULL) {
        goto finish;
    }
	if( pwObject == NULL ) {
		JSS_throw(env, GIVE_UP_EXCEPTION);
		goto finish;
	}

	/*****************************************
	 * Get Password class and methods
 	 *****************************************/
    passwordClass = (*env)->GetObjectClass(env, pwObject);
    if(passwordClass == NULL) {
        JSS_trace(env, JSS_TRACE_ERROR, "Failed to find Password class");
        ASSERT_OUTOFMEM(env);
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
        JSS_trace(env, JSS_TRACE_ERROR,
            "Failed to find Password manipulation methods from native "
            "implementation");
        ASSERT_OUTOFMEM(env);
        goto finish;
    }

    /************************************************
     * Get the bytes from the password, then clear it
     ***********************************************/
    pwArray = (*env)->CallObjectMethod( env, pwObject, getByteCopyMethod);
    (*env)->CallVoidMethod(env, pwObject, clearMethod);

	exception = (*env)->ExceptionOccurred(env);
	if(exception == NULL) {
		PR_ASSERT(pwArray != NULL);

		/*************************************************************
	 	 * Copy the characters out of the byte array,
		 * then erase it
 	 	*************************************************************/
		pwchars = (char*) (*env)->GetByteArrayElements(env, pwArray, NULL);
		PR_ASSERT(pwchars!=NULL);

		returnchars = PL_strdup(pwchars);
		JSS_wipeCharArray(pwchars);
		(*env)->ReleaseByteArrayElements(env, pwArray, (jbyte*)pwchars, 0);
	} else {
		returnchars = NULL;
	}

finish:
	if( (exception=(*env)->ExceptionOccurred(env)) != NULL) {
#ifdef DEBUG
        jclass giveupClass;
        jmethodID printStackTrace;
        jclass excepClass;
#endif
		(*env)->ExceptionClear(env);
#ifdef DEBUG
        giveupClass = (*env)->FindClass(env, GIVE_UP_EXCEPTION);
        PR_ASSERT(giveupClass != NULL);
        if( ! (*env)->IsInstanceOf(env, exception, giveupClass) ) {
            excepClass = (*env)->GetObjectClass(env, exception);
            printStackTrace = (*env)->GetMethodID(env, excepClass,
                "printStackTrace", "()V");
            (*env)->CallVoidMethod(env, exception, printStackTrace);
            PR_ASSERT( PR_FALSE );
        }
		PR_ASSERT(returnchars==NULL);
#endif
	}
	return returnchars;
}

/**********************************************************************
 *
 * m a k e P W C B I n f o
 *
 * Creates a Java PasswordCallbackInfo structure from a PKCS #11 token.
 * Returns this object, or NULL if an exception was thrown.
 */
static jobject
makePWCBInfo(JNIEnv *env, PK11SlotInfo *slot)
{
	jclass infoClass;
	jmethodID constructor;
	jstring name;
	jobject pwcbInfo=NULL;

	PR_ASSERT(env!=NULL && slot!=NULL);

	/*****************************************
     * Turn the token name into a Java String
 	 *****************************************/
	name = (*env)->NewStringUTF(env, PK11_GetTokenName(slot));
	if(name == NULL) {
		ASSERT_OUTOFMEM(env);
		goto finish;
	}

	/*****************************************
	 * Look up the class and constructor
 	 *****************************************/
	infoClass = (*env)->FindClass(env, TOKEN_CBINFO_CLASS_NAME);
	if(infoClass == NULL) {
        JSS_trace(env, JSS_TRACE_ERROR, "Unable to find TokenCallbackInfo "
            "class");
		ASSERT_OUTOFMEM(env);
		goto finish;
	}
	constructor = (*env)->GetMethodID(	env,
										infoClass,
										TOKEN_CBINFO_CONSTRUCTOR_NAME,
										TOKEN_CBINFO_CONSTRUCTOR_SIG);
	if(constructor == NULL) {
        JSS_trace(env, JSS_TRACE_ERROR, "Unable to find "
            "TokenCallbackInfo constructor");
		ASSERT_OUTOFMEM(env);
		goto finish;
	}

	/*****************************************
	 * Create the CallbackInfo object
 	 *****************************************/
	pwcbInfo = (*env)->NewObject(env, infoClass, constructor, name);
    if(pwcbInfo == NULL) {
        JSS_trace(env, JSS_TRACE_ERROR, "Unable to create TokenCallbackInfo");
        ASSERT_OUTOFMEM(env);
    }

finish:
	return pwcbInfo;
}

/**********************************************************************
 * CryptoManager.putModulesInVector
 *
 * Wraps all PKCS #11 modules in PK11Module Java objects, then puts
 * these into a Vector.
 */
JNIEXPORT void JNICALL
Java_org_mozilla_jss_CryptoManager_putModulesInVector
    (JNIEnv *env, jobject this, jobject vector)
{
    SECMODListLock *listLock=NULL;
    SECMODModuleList *list;
    SECMODModule *modp=NULL;
    jclass vectorClass;
    jmethodID addElement;
    jobject module;

    PR_ASSERT(env!=NULL && this!=NULL && vector!=NULL);

    /***************************************************
     * Get JNI ids
     ***************************************************/
    vectorClass = (*env)->GetObjectClass(env, vector);
    if(vectorClass == NULL) goto finish;

    addElement = (*env)->GetMethodID(env,
                                     vectorClass,
                                     VECTOR_ADD_ELEMENT_NAME,
                                     VECTOR_ADD_ELEMENT_SIG);
    if(addElement==NULL) goto finish;

    /***************************************************
     * Lock the list
     ***************************************************/
    listLock = SECMOD_GetDefaultModuleListLock();
    PR_ASSERT(listLock!=NULL);

    SECMOD_GetReadLock(listLock);

    /***************************************************
     * Loop over the modules, adding each one to the vector
     ***************************************************/
    for( list = SECMOD_GetDefaultModuleList(); list != NULL; list=list->next) {
        PR_ASSERT(list->module != NULL);

        /** Make a PK11Module **/
        modp = SECMOD_ReferenceModule(list->module);
        module = JSS_PK11_wrapPK11Module(env, &modp);
        PR_ASSERT(modp==NULL);
        if(module == NULL) {
            goto finish;
        }

        /** Stick the PK11Module in the Vector **/
        (*env)->CallVoidMethod(env, vector, addElement, module);
    }

finish:
    /*** Unlock the list ***/
    if(listLock != NULL) {
        SECMOD_ReleaseReadLock(listLock);
    }
    /*** Free this module if it wasn't properly Java-ized ***/
    if(modp!=NULL) {
        SECMOD_DestroyModule(modp);
    }

    return;
}


/**********************************************************************
 * CryptoManager.enableFIPS
 *
 * Enables or disables FIPS mode.
 * INPUTS
 *      fips
 *          true means turn on FIPS mode, false means turn it off.
 * RETURNS
 *      true if a switch happened, false if the library was already
 *      in the requested mode.
 * THROWS
 *      java.security.GeneralSecurityException if an error occurred with
 *      the PKCS #11 library.
 */
JNIEXPORT jboolean JNICALL
Java_org_mozilla_jss_CryptoManager_enableFIPS
    (JNIEnv *env, jclass clazz, jboolean fips)
{
    char *name=NULL;
    jboolean switched = JNI_FALSE;
    SECStatus status;

    if( ((fips==JNI_TRUE)  && !PK11_IsFIPS()) ||
        ((fips==JNI_FALSE) &&  PK11_IsFIPS())  )
    {
        name = PL_strdup(SECMOD_GetInternalModule()->commonName);
        status = SECMOD_DeleteInternalModule(name);
        PR_Free(name);
        switched = JNI_TRUE;
    }

    if(status != SECSuccess) {
        JSS_throwMsg(env,
                     GENERAL_SECURITY_EXCEPTION,
                     "Failed to toggle FIPS mode");
    }

    return switched;
}

/***********************************************************************
 * CryptoManager.FIPSEnabled
 *
 * Returns true if FIPS mode is currently on, false if it ain't.
 */
JNIEXPORT jboolean JNICALL
Java_org_mozilla_jss_CryptoManager_FIPSEnabled(JNIEnv *env, jobject this)
{
    if( PK11_IsFIPS() ) {
        return JNI_TRUE;
    } else {
        return JNI_FALSE;
    }
}

/***********************************************************************
 * DatabaseCloser.closeDatabases
 *
 * Closes the cert and key database, rendering the security library
 * unusable.
 */
JNIEXPORT void JNICALL
Java_org_mozilla_jss_DatabaseCloser_closeDatabases
    (JNIEnv *env, jobject this)
{
    PR_ASSERT( CERT_GetDefaultCertDB() != NULL );
    CERT_ClosePermCertDB( CERT_GetDefaultCertDB() );
    CERT_SetDefaultCertDB( NULL );

    PR_ASSERT( SECKEY_GetDefaultKeyDB() != NULL );
    SECKEY_CloseKeyDB( SECKEY_GetDefaultKeyDB() );
    SECKEY_SetDefaultKeyDB( NULL );
}
