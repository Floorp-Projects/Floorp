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
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
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
#ifndef PK11TEST_H
#define PK11TEST_H

#define REP_SYMKEY_MECHANISM CKM_DES_KEY_GEN

/* symmetric key size in bytes */
#define REP_SYMKEY_SIZE 8

#define REP_PK_KEY_SIZE 1024
#define REP_PLAINTEXT_LEN 8
#define REP_MECHANISM mechanism[testId/2/2%46]
#define REP_USE_CORRECT_PIN UseCorrectPin[testId%2]
#define REP_KEYGEN_ON_TARGET KeyGenOnTarget[testId/2%2]
#define CKM_NO_OP 0x80001111

int testId = 0;

PRBool UseCorrectPin[] = {
	PR_TRUE,
	PR_FALSE
};

PRBool KeyGenOnTarget[] = {
	PR_TRUE,
	PR_FALSE
};

CK_MECHANISM_TYPE mechanism[] = {
	CKM_NO_OP,
	CKM_RSA_PKCS,
	CKM_RSA_9796,
	CKM_RSA_X_509,
	CKM_MD2_RSA_PKCS,
	CKM_MD5_RSA_PKCS,
	CKM_SHA1_RSA_PKCS,
	CKM_DSA,
	CKM_DSA_SHA1,
	CKM_ECDSA,
	CKM_ECDSA_SHA1,
	CKM_RC2_ECB,
	CKM_RC2_CBC,
	CKM_RC4,
	CKM_RC5_ECB,
	CKM_RC5_CBC,
	CKM_DES_ECB,
	CKM_DES_CBC,
	CKM_DES3_ECB,
	CKM_DES3_CBC,
	CKM_CAST_ECB,
	CKM_CAST_CBC,
	CKM_CAST3_ECB,
	CKM_CAST3_CBC,
	CKM_CAST5_ECB,
	CKM_CAST5_CBC,
	CKM_IDEA_ECB,
	CKM_IDEA_CBC,
	CKM_CDMF_ECB,
	CKM_CDMF_CBC,
	CKM_SKIPJACK_ECB64,
	CKM_SKIPJACK_CBC64,
	CKM_SKIPJACK_OFB64,
	CKM_SKIPJACK_CFB64,
	CKM_SKIPJACK_CFB32,
	CKM_SKIPJACK_CFB16,
	CKM_SKIPJACK_CFB8,
	CKM_BATON_ECB128,
	CKM_BATON_ECB96,
	CKM_BATON_CBC128,
	CKM_BATON_COUNTER,
	CKM_BATON_SHUFFLE,
	CKM_JUNIPER_ECB128,
	CKM_JUNIPER_CBC128,
	CKM_JUNIPER_COUNTER,
	CKM_JUNIPER_SHUFFLE
};



#endif
