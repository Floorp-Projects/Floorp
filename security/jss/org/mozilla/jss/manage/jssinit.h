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
/* This header depends on:
#include <jni.h>
#include <nspr.h>
*/

#ifndef JSS_INIT_H
#define JSS_INIT_H

/***********************************************************************
 * JSS_completeInitialize
 *
 * Initialize the security library and open all the databases.
 *
 */
PR_EXTERN( void )
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
        jstring ocspResponderCertNickname );


/**********************************************************************
 *
 * JSS_setPasswordCallback
 *
 * Sets the global PasswordCallback object, which will be used to
 * login to tokens implicitly if necessary.
 *
 */
PR_EXTERN( void )
JSS_setPasswordCallback(JNIEnv *env, jobject callback);

#endif
