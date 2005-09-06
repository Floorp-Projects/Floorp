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
#include "_jni/org_mozilla_jss_CryptoManager.h"

#include <secitem.h>
#include <secmod.h>
#include <cert.h>
#include <certt.h>
#include <key.h>
#include <ocsp.h>
#include <pk11func.h>
#include <nspr.h>
#include <plstr.h>
#include <pkcs11.h>
#include <nss.h>
#include <ssl.h>

#include <jssutil.h>
#include <java_ids.h>
#include <jss_exceptions.h>
#include <jssver.h>

#include "pk11util.h"

#if defined(AIX) || defined(HPUX) || defined(LINUX)
#include <signal.h>
#endif

/********************************************************************/
/* The VERSION Strings should be updated in the following           */
/* files everytime a new release of JSS is generated:               */
/*                                                                  */
/* org/mozilla/jss/CryptoManager.java                               */
/* org/mozilla/jss/CryptoManager.c                                  */
/* org/mozilla/jss/util/jssver.h                                    */
/* lib/manifest.mn                                                  */
/*                                                                  */
/********************************************************************/

static const char* DLL_JSS_VERSION     = "JSS_VERSION = " JSS_VERSION;
static const char* DLL_NSS_VERSION     = "NSS_VERSION = " NSS_VERSION;
static const char* DLL_NSPR_VERSION    = "NSPR_VERSION = " PR_VERSION;

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


int ConfigureOCSP(
        JNIEnv *env,
        jboolean ocspCheckingEnabled,
        jstring ocspResponderURL,
        jstring ocspResponderCertNickname )
{
    char *ocspResponderURL_string=NULL;
    char *ocspResponderCertNickname_string=NULL;
    SECStatus status;
    int result = SECSuccess;
    CERTCertDBHandle *certdb = CERT_GetDefaultCertDB();


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

    CERT_DisableOCSPChecking(certdb);

    /* if they set the default responder, then set it up
     * and enable it
     */
    if (ocspResponderURL) {
        /* if ocspResponderURL is set they must specify the
           ocspResponderCertNickname */
                if (ocspResponderCertNickname == NULL ) {
                JSS_throwMsg(env, GENERAL_SECURITY_EXCEPTION,
                "if OCSP responderURL is set, the Responder Cert nickname must be set");
                        result = SECFailure;
                        goto loser;
                } else {
                        CERTCertificate *cert;
                        /* if the nickname is set */
       cert = CERT_FindCertByNickname(certdb, ocspResponderCertNickname_string);
                        if (cert == NULL) {
                          /*
                           * look for the cert on an external token.
                        */
       cert = PK11_FindCertFromNickname(ocspResponderCertNickname_string, NULL);
                       }
                        if (cert == NULL) {
                                JSS_throwMsg(env, GENERAL_SECURITY_EXCEPTION,
                    "Unable to find the OCSP Responder Certificate nickname.");
                        result = SECFailure;
                        goto loser;
	               }
                        CERT_DestroyCertificate(cert);
	}
        status =
            CERT_SetOCSPDefaultResponder(   certdb,
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
        CERT_EnableOCSPDefaultResponder(certdb);
    }
    else {
        /* if no defaultresponder is set, disable it */
        CERT_DisableOCSPDefaultResponder(certdb);
    }
        

    /* enable OCSP checking if requested */

    if (ocspCheckingEnabled) {
        CERT_EnableOCSPChecking(certdb);
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


/**********************************************************************
 * This is the PasswordCallback object that will be used to login
 * to tokens implicitly.
 */
static jobject globalPasswordCallback = NULL;

/**********************************************************************
 * The Java virtual machine can be used to retrieve the JNI environment
 * pointer from callback functions.
 */
JavaVM * JSS_javaVM;

JNIEXPORT void JNICALL
Java_org_mozilla_jss_CryptoManager_initializeAllNative
    (JNIEnv *env, jclass clazz,
        jstring configDir,
        jstring certPrefix,
        jstring keyPrefix,
        jstring secmodName,
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
    Java_org_mozilla_jss_CryptoManager_initializeAllNative2(
        env,
        clazz,
        configDir,
        certPrefix,
        keyPrefix,
        secmodName,
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
        ocspResponderCertNickname,
        JNI_FALSE /*initializeJavaOnly*/ );
}


JNIEXPORT void JNICALL
Java_org_mozilla_jss_CryptoManager_initializeAllNative2
    (JNIEnv *env, jclass clazz,
        jstring configDir,
        jstring certPrefix,
        jstring keyPrefix,
        jstring secmodName,
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
        jstring ocspResponderCertNickname,
        jboolean initializeJavaOnly )
{
    SECStatus rv = SECFailure;
    JavaVM *VMs[5];
    jint numVMs;
    char *szConfigDir = NULL;
    char *szCertPrefix = NULL;
    char *szKeyPrefix = NULL;
    char *szSecmodName = NULL;
    char *manuChars=NULL;
    char *libraryChars=NULL;
    char *tokChars=NULL;
    char *keyTokChars=NULL;
    char *slotChars=NULL;
    char *keySlotChars=NULL;
    char *fipsChars=NULL;
    char *fipsKeyChars=NULL;
    PRUint32 initFlags;

    /* This is thread-safe because initialize is synchronized */
    static PRBool initialized=PR_FALSE;

    if( configDir == NULL ||
        manuString == NULL ||
        libraryString == NULL ||
        tokString == NULL ||
        keyTokString == NULL ||
        slotString == NULL ||
        keySlotString == NULL ||
        fipsString == NULL ||
        fipsKeyString == NULL )
    {
        JSS_throw(env, NULL_POINTER_EXCEPTION);
        goto finish;
    }

    /* Make sure initialize() completes only once */
    if(initialized) {
        JSS_throw(env, ALREADY_INITIALIZED_EXCEPTION);
        goto finish;
    }

    /*
     * Save the JavaVM pointer so we can retrieve the JNI environment
     * later. This only works if there is only one Java VM.
     */
    if( (*env)->GetJavaVM(env, &JSS_javaVM) != 0 ) {
        JSS_trace(env, JSS_TRACE_ERROR,
                    "Unable to to access Java virtual machine");
        PR_ASSERT(PR_FALSE);
        goto finish;
    }

    /*
     * Initialize the errcode translation table.
     */
    JSS_initErrcodeTranslationTable();

    /*
     * The rest of the initialization (the NSS stuff) is skipped if
     * the initializeJavaOnly flag is set.
     */
    if( initializeJavaOnly) {
        initialized = PR_TRUE;
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
    PK11_ConfigurePKCS11(   manuChars,
                            libraryChars,
                            tokChars,
                            keyTokChars,
                            slotChars,
                            keySlotChars,
                            fipsChars,
                            fipsKeyChars,
                            0, /* minimum pin length */
                            PR_FALSE /* password required */
                        );


    szConfigDir = (char*) (*env)->GetStringUTFChars(env, configDir, NULL);
    if( certPrefix != NULL && keyPrefix != NULL && secmodName != NULL ) {
        /*
        * Set up arguments to NSS_Initialize
        */
        szCertPrefix = (char*) (*env)->GetStringUTFChars(env, certPrefix, NULL);
        szKeyPrefix = (char*) (*env)->GetStringUTFChars(env, keyPrefix, NULL);
        szSecmodName = (char*) (*env)->GetStringUTFChars(env, secmodName, NULL);
        initFlags = 0;
        if( readOnly ) {
            initFlags |= NSS_INIT_READONLY;
        }

        /*
        * Initialize NSS.
        */
        rv = NSS_Initialize(szConfigDir, szCertPrefix, szKeyPrefix,
                szSecmodName, initFlags);
    } else {
        if( readOnly ) {
            rv = NSS_Init(szConfigDir);
        } else {
            rv = NSS_InitReadWrite(szConfigDir);
        }
    }

    if( rv != SECSuccess ) {
        JSS_throwMsg(env, SECURITY_EXCEPTION,
            "Unable to initialize security library");
        goto finish;
    }

    /*
     * Set default password callback.  This is the only place this
     * should ever be called if you are using Ninja.
     */
    PK11_SetPasswordFunc(getPWFromCallback);

    /*
     * Setup NSS to call the specified OCSP responder
     */
    rv = ConfigureOCSP(
        env,
        ocspCheckingEnabled,
        ocspResponderURL,
        ocspResponderCertNickname );

    if (rv != SECSuccess) {
        goto finish;
    }

    /*
     * Set up policy. We're always domestic now. Thanks to the US Government!
     */
    if( NSS_SetDomesticPolicy() != SECSuccess ) {
        JSS_throwMsg(env, SECURITY_EXCEPTION, "Unable to set security policy");
        goto finish;
    }

    initialized = PR_TRUE;

finish:
    /* LET'S BE CAREFUL.  Unbraced if statements ahead. */
    if(szConfigDir)
        (*env)->ReleaseStringUTFChars(env, configDir, szConfigDir);
    if(szCertPrefix)
        (*env)->ReleaseStringUTFChars(env, certPrefix, szCertPrefix);
    if(szKeyPrefix)
        (*env)->ReleaseStringUTFChars(env, keyPrefix, szKeyPrefix);
    if(szSecmodName)
        (*env)->ReleaseStringUTFChars(env, secmodName, szSecmodName);
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
 * JSS_setPasswordCallback
 *
 * Sets the global PasswordCallback object, which will be used to
 * login to tokens implicitly if necessary.
 *
 */
void
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
    if((*JSS_javaVM)->AttachCurrentThread(JSS_javaVM, (void**)&env, NULL) != 0){
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
    constructor = (*env)->GetMethodID(  env,
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
    SECStatus status = SECSuccess;

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
    NSS_Shutdown();
}

/**********************************************************************
* configureOCSPNative
*
* Allows configuration of the OCSP responder during runtime.
*/
JNIEXPORT void JNICALL
Java_org_mozilla_jss_CryptoManager_configureOCSPNative(
        JNIEnv *env, jobject this,
        jboolean ocspCheckingEnabled,
        jstring ocspResponderURL,
        jstring ocspResponderCertNickname )
{
    SECStatus rv = SECFailure;

    rv =  ConfigureOCSP(env,ocspCheckingEnabled,
        ocspResponderURL, ocspResponderCertNickname);

    if (rv != SECSuccess) {
        JSS_throwMsgPrErr(env,
                     GENERAL_SECURITY_EXCEPTION,
                     "Failed to configure OCSP");
    }
}

