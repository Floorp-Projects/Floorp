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
 * The Original Code is Netscape Security Services for Java.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.
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

#include "_jni/org_mozilla_jss_crypto_SecretDecoderRing.h"
#include <nspr.h>
#include <secitem.h>
#include <pk11sdr.h>
#include <jss_exceptions.h>
#include <jssutil.h>

typedef enum {SDR_ENCRYPT, SDR_DECRYPT} SDROp;

static jbyteArray
doSDR(JNIEnv *env, jobject this, jbyteArray inputBA, SDROp optype)
{
    SECStatus status;
    jbyteArray outputBA = NULL;
    SECItem keyID = {siBuffer, NULL, 0};
    SECItem *input= NULL;
    SECItem output = {siBuffer, NULL, 0};

    /* convert input to SECItem */
    if( inputBA == NULL ) {
        JSS_throw(env, NULL_POINTER_EXCEPTION);
        goto finish;
    }
    input = JSS_ByteArrayToSECItem(env, inputBA);
    if( input == NULL) {
        /* exception was thrown */
        goto finish;
    }

    /* perform the operation*/
    if( optype == SDR_ENCRYPT ) {
        status = PK11SDR_Encrypt(&keyID, input, &output, NULL /*cx*/);
    } else {
        PR_ASSERT( optype == SDR_DECRYPT);
        status = PK11SDR_Decrypt(input, &output, NULL /*cx*/);
    }
    if(status != SECSuccess) {
        JSS_throwMsg(env, TOKEN_EXCEPTION, "Operation failed");
        goto finish;
    }

    /* convert output to byte array */
    outputBA = JSS_SECItemToByteArray(env, &output);

finish:
    if( input != NULL) {
        SECITEM_FreeItem(input, PR_TRUE /* freeit */);
    }
    SECITEM_FreeItem(&output, PR_FALSE /*freeit*/);
    return outputBA;
}

JNIEXPORT jbyteArray JNICALL
Java_org_mozilla_jss_crypto_SecretDecoderRing_encrypt(
    JNIEnv *env, jobject this, jbyteArray plaintextBA)
{
    return doSDR(env, this, plaintextBA, SDR_ENCRYPT);
}

JNIEXPORT jbyteArray JNICALL
Java_org_mozilla_jss_crypto_SecretDecoderRing_decrypt(
    JNIEnv *env, jobject this, jbyteArray ciphertextBA)
{
    return doSDR(env, this, ciphertextBA, SDR_DECRYPT);
}
