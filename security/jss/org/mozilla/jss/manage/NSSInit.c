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

#include "_jni/org_mozilla_jss_NSSInit.h"
#include <nspr.h>
#include "jssinit.h"

/********************************************************************/
/* The following VERSION Strings should be updated in the following */
/* files everytime a new release of JSS is generated:               */
/*                                                                  */
/*     jssjava:  ns/ninja/cmd/jssjava/jssjava.c                     */
/*     jss.jar:  ns/ninja/org/mozilla/jss/manage/NSSInit.java      */
/*     jss.dll:  ns/ninja/org/mozilla/jss/manage/NSSInit.c         */
/*                                                                  */
/********************************************************************/

static const char* DLL_JSS_VERSION     = "JSS_VERSION = JSS_2_1   11 Dec 2000";
static const char* DLL_JDK_VERSION     = "JDK_VERSION = JDK 1.2.2";
static const char* DLL_SVRCORE_VERSION = "SVRCORE_VERSION = SVRCORE_2_5_1";
static const char* DLL_NSS_VERSION     = "NSS_VERSION = NSS_2_8_4_RTM";
static const char* DLL_DBM_VERSION     = "DBM_VERSION = DBM_1_54";
static const char* DLL_NSPR_VERSION    = "NSPR_VERSION = v3.5.1";

/***********************************************************************
 * NSSInit.initializeNative
 *
 * This just passes its arguments to CryptoManager.initializeAllNative.
 */
JNIEXPORT void JNICALL
Java_org_mozilla_jss_NSSInit_initializeNative
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
        jstring fipsKeyString )
{
    jboolean ocsp = JNI_FALSE;
    jstring  ocsp_url = NULL;
    jstring  ocsp_nickname = NULL;

    JSS_completeInitialize(
            env,
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
            ocsp,
            ocsp_url,
            ocsp_nickname);
}


/***********************************************************************
 * NSSInit.setPasswordCallback
 *
 * This just passes its arguments to CryptoManager.setNativePasswordCallback
 */
JNIEXPORT void JNICALL
Java_org_mozilla_jss_NSSInit_setPasswordCallback
    (JNIEnv *env, jclass clazz, jobject pwcb)
{
    JSS_setPasswordCallback(env, pwcb);
}
