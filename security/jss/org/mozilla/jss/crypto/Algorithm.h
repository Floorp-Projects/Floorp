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

/* These headers must be included before this header:
#include <secoidt.h>
#include <pkcs11t.h>
#include <jni.h>
#include <Policy.h>
*/

#ifndef JSS_ALGORITHM_H
#define JSS_ALGORITHM_H

PR_BEGIN_EXTERN_C

typedef enum JSS_AlgType {
    PK11_MECH,      /* CK_MECHANISM_TYPE    */
    SEC_OID_TAG     /* SECOidTag            */
} JSS_AlgType;

typedef struct JSS_AlgInfoStr {
    unsigned long val; /* either a CK_MECHANISM_TYPE or a SECOidTag */
    JSS_AlgType type;
} JSS_AlgInfo;

#define NUM_ALGS 44

extern JSS_AlgInfo JSS_AlgTable[];
extern CK_ULONG JSS_symkeyUsage[];

/***********************************************************************
 *
 * J S S _ g e t O i d T a g F r o m A l g
 *
 * INPUTS
 *      alg
 *          An org.mozilla.jss.Algorithm object. Must not be NULL.
 * RETURNS
 *      SECOidTag corresponding to this algorithm, or SEC_OID_UNKNOWN
 *      if none was found.
 */
SECOidTag
JSS_getOidTagFromAlg(JNIEnv *env, jobject alg);

/***********************************************************************
 *
 * J S S _ g e t P K 1 1 M e c h F r o m A l g
 *
 * INPUTS
 *      alg
 *          An org.mozilla.jss.Algorithm object. Must not be NULL.
 * RETURNS
 *          CK_MECHANISM_TYPE corresponding to this algorithm, or
 *          CKM_INVALID_MECHANISM if none was found.
 */
CK_MECHANISM_TYPE
JSS_getPK11MechFromAlg(JNIEnv *env, jobject alg);

PR_END_EXTERN_C

#endif
