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
#ifndef JAVA_IDS_H
#define JAVA_IDS_H

PR_BEGIN_EXTERN_C

/*
** JNI uses strings to identity Java classes, fields, and methods. Rather
** than put lots of string constants in our JNI code, we define them once
** here.  This has two benefits: it reduces the probability of typos
** (we only have to type the names once), and it allows us to more
** easily change these identifiers.
*/

#define PLAIN_CONSTRUCTOR "<init>"
#define PLAIN_CONSTRUCTOR_SIG "()V"

/*
 * Algorithm
 */
#define ALGORITHM_CLASS_NAME "com/netscape/jss/crypto/Algorithm"
#define OID_INDEX_FIELD_NAME "oidIndex"
#define OID_INDEX_FIELD_SIG "I"

/*
 * BigInteger
 */
#define BIG_INTEGER_CLASS_NAME "java/math/BigInteger"
#define BIG_INTEGER_CONSTRUCTOR_NAME "<init>"
#define BIG_INTEGER_CONSTRUCTOR_SIG "([B)V"

/*
 * CipherContextProxy
 */
#define CIPHER_CONTEXT_PROXY_CLASS_NAME "com/netscape/jss/pkcs11/CipherContextProxy"
#define CIPHER_CONTEXT_PROXY_CONSTRUCTOR_SIG "([B)V"

/*
 * Debug
 */
#define DEBUG_CLASS_NAME "com/netscape/jss/util/Debug"
#define DEBUG_TRACE_NAME "trace"
#define DEBUG_TRACE_SIG "(ILjava/lang/String;)V"

/*
 * KeyPair
 */
#define KEY_PAIR_CLASS_NAME "java/security/KeyPair"
#define KEY_PAIR_CONSTRUCTOR_NAME "<init>"
#define KEY_PAIR_CONSTRUCTOR_SIG "(Ljava/security/PublicKey;Ljava/security/PrivateKey;)V"

/*
 * KeyProxy
 */
#define KEY_PROXY_FIELD "keyProxy"
#define KEY_PROXY_SIG "Lcom/netscape/jss/pkcs11/KeyProxy;"

/*
 * KeyType
 */
#define KEYTYPE_CLASS_NAME "com/netscape/jss/pkcs11/KeyType"
#define NULL_KEYTYPE_FIELD "NULL"
#define RSA_KEYTYPE_FIELD "RSA"
#define DSA_KEYTYPE_FIELD "DSA"
#define FORTEZZA_KEYTYPE_FIELD "FORTEZZA"
#define DH_KEYTYPE_FIELD "DH"
#define KEA_KEYTYPE_FIELD "KEA"
#define KEYTYPE_FIELD_SIG "Lcom/netscape/jss/pkcs11/KeyType;"
#define DES_KEYTYPE_FIELD "DES"
#define DES3_KEYTYPE_FIELD "DES3"
#define RC4_KEYTYPE_FIELD "RC4"
#define RC2_KEYTYPE_FIELD "RC2"
#define SHA1_HMAC_KEYTYPE_FIELD "SHA1_HMAC"

/*
 * NativeProxy
 */
#define NATIVE_PROXY_CLASS_NAME  "com/netscape/jss/util/NativeProxy"
#define NATIVE_PROXY_POINTER_FIELD "mPointer"
#define NATIVE_PROXY_POINTER_SIG "[B"

/*
 * NSSInit
 */
#define NSSINIT_CLASS_NAME "com/netscape/jss/NSSInit"
#define NSSINIT_ISINITIALIZED_NAME "isInitialized"
#define NSSINIT_ISINITIALIZED_SIG "()Z"

/*
 * Password
 */
#define PASSWORD_CLASS_NAME "com/netscape/jss/util/Password"
#define PASSWORD_CONSTRUCTOR_SIG "([C)V"
#define PW_GET_BYTE_COPY_NAME "getByteCopy"
#define PW_GET_BYTE_COPY_SIG "()[B"
#define PW_CLEAR_NAME "clear"
#define PW_CLEAR_SIG "()V"

/*
 * PasswordCallback
 */
#define PW_CALLBACK_GET_PW_FIRST_NAME "getPasswordFirstAttempt"
#define PW_CALLBACK_GET_PW_FIRST_SIG "(Lcom/netscape/jss/util/PasswordCallbackInfo;)Lcom/netscape/jss/util/Password;"
#define PW_CALLBACK_GET_PW_AGAIN_NAME "getPasswordAgain"
#define PW_CALLBACK_GET_PW_AGAIN_SIG "(Lcom/netscape/jss/util/PasswordCallbackInfo;)Lcom/netscape/jss/util/Password;"

/*
 * PK11Cert
 */
#define CERT_CLASS_NAME "com/netscape/jss/pkcs11/PK11Cert"
#define CERT_CONSTRUCTOR_NAME "<init>"
#define CERT_CONSTRUCTOR_SIG "([B)V"
#define CERT_PROXY_FIELD "certProxy"
#define CERT_PROXY_SIG "Lcom/netscape/jss/pkcs11/CertProxy;"

/*
 * PK11InternalCert
 */
#define INTERNAL_CERT_CLASS_NAME "com/netscape/jss/pkcs11/PK11InternalCert"

/*
 * PK11TokenCert
 */
#define TOKEN_CERT_CLASS_NAME "com/netscape/jss/pkcs11/PK11TokenCert"

/*
 * PK11InternalTokenCert
 */
#define INTERNAL_TOKEN_CERT_CLASS_NAME "com/netscape/jss/pkcs11/PK11InternalTokenCert"

/*
 * PK11DSAPublicKey
 */
#define PK11_DSA_PUBKEY_CLASS_NAME "com/netscape/jss/pkcs11/PK11DSAPublicKey"

/*
 * PK11Module
 */
#define PK11MODULE_CLASS_NAME "com/netscape/jss/pkcs11/PK11Module"
#define PK11MODULE_CONSTRUCTOR_SIG "([B)V"
#define PK11MODULE_PROXY_FIELD "moduleProxy"
#define PK11MODULE_PROXY_SIG "Lcom/netscape/jss/pkcs11/ModuleProxy;"

/*
 * PK11PrivKey
 */
#define PK11PRIVKEY_CLASS_NAME "com/netscape/jss/pkcs11/PK11PrivKey"
#define PK11PRIVKEY_CONSTRUCTOR_NAME "<init>"
#define PK11PRIVKEY_CONSTRUCTOR_SIG "([B)V"

/*
 * PK11PubKey
 */
#define PK11PUBKEY_CLASS_NAME "com/netscape/jss/pkcs11/PK11PubKey"
#define PK11PUBKEY_CONSTRUCTOR_NAME "<init>"
#define PK11PUBKEY_CONSTRUCTOR_SIG "([B)V"

/*
 * PK11RSAPublicKey
 */
#define PK11_RSA_PUBKEY_CLASS_NAME "com/netscape/jss/pkcs11/PK11RSAPublicKey"

/*
 * PK11Signature
 */
#define PK11SIGNATURE_CLASS_NAME "com/netscape/jss/pkcs11/PK11Signature"
#define SIG_CONTEXT_PROXY_FIELD "sigContext"
#define SIG_CONTEXT_PROXY_SIG "Lcom/netscape/jss/pkcs11/SigContextProxy;"
#define SIG_ALGORITHM_FIELD "algorithm"
#define SIG_ALGORITHM_SIG "Lcom/netscape/jss/crypto/Algorithm;"
#define SIG_PW_FIELD "pwExtractor"
#define SIG_PW_SIG "Lcom/netscape/jss/crypto/PasswordExtractor;"
#define SIG_KEY_FIELD "key"
#define SIG_KEY_SIG "Lcom/netscape/jss/pkcs11/PK11Key;"

/*
 * PK11Store
 */
#define PK11STORE_PROXY_FIELD "storeProxy"
#define PK11STORE_PROXY_SIG "Lcom/netscape/jss/pkcs11/TokenProxy;"

/*
 * PK11SymKey
 */
#define PK11SYMKEY_CLASS_NAME "com/netscape/jss/pkcs11/PK11SymKey"
#define PK11SYMKEY_CONSTRUCTOR_SIG "([B)V"

/*
 * PK11Token
 */
#define PK11TOKEN_PROXY_FIELD "tokenProxy"
#define PK11TOKEN_PROXY_SIG "Lcom/netscape/jss/pkcs11/TokenProxy;"
#define PK11TOKEN_CLASS_NAME "com/netscape/jss/pkcs11/PK11Token"
#define PK11TOKEN_CONSTRUCTOR_NAME "<init>"
#define PK11TOKEN_CONSTRUCTOR_SIG "([BZZ)V"

/*
 * PrivateKey.KeyType
 */
#define PRIVKEYTYPE_CLASS_NAME "com/netscape/jss/crypto/PrivateKey$Type"
#define PRIVKEYTYPE_SIG "Lcom/netscape/jss/crypto/PrivateKey$Type;"
#define RSA_PRIVKEYTYPE_FIELD "RSA"
#define DSA_PRIVKEYTYPE_FIELD "DSA"

/*
 * PQGParams
 */
#define PQG_PARAMS_CONSTRUCTOR_NAME "<init>"
#define PQG_PARAMS_CONSTRUCTOR_SIG "(Ljava/math/BigInteger;Ljava/math/BigInteger;Ljava/math/BigInteger;Ljava/math/BigInteger;ILjava/math/BigInteger;)V"

/*
 * SigContextProxy
 */
#define SIG_CONTEXT_PROXY_CLASS_NAME "com/netscape/jss/pkcs11/SigContextProxy"
#define SIG_CONTEXT_PROXY_CONSTRUCTOR_NAME "<init>"
#define SIG_CONTEXT_PROXY_CONSTRUCTOR_SIG "([B)V"

/*
 * SymKeyProxy
 */
#define SYM_KEY_PROXY_FIELD "keyProxy"
#define SYM_KEY_PROXY_SIG "Lcom/netscape/jss/pkcs11/SymKeyProxy;"

/*
 * TokenCallbackInfo
 */
#define TOKEN_CBINFO_CLASS_NAME "com/netscape/jss/pkcs11/TokenCallbackInfo"
#define TOKEN_CBINFO_CONSTRUCTOR_NAME "<init>"
#define TOKEN_CBINFO_CONSTRUCTOR_SIG "(Ljava/lang/String;)V"


/*
 * Vector
 */
#define VECTOR_ADD_ELEMENT_NAME "addElement"
#define VECTOR_ADD_ELEMENT_SIG "(Ljava/lang/Object;)V"

/*
 * X509Certificate
 */
#define X509_CERT_CLASS "com/netscape/jss/crypto/X509Certificate"

PR_END_EXTERN_C

#endif
