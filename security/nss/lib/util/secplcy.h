/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __secplcy_h__
#define __secplcy_h__

#include "utilrename.h"

#include "prtypes.h"

/*
** Cipher policy enforcement. This code isn't very pretty, but it accomplishes
** the purpose of obscuring policy information from potential fortifiers. :-)
**
** The following routines are generic and intended for anywhere where cipher
** policy enforcement is to be done, e.g. SSL and PKCS7&12.
*/

#define SEC_CIPHER_NOT_ALLOWED 0
#define SEC_CIPHER_ALLOWED 1
#define SEC_CIPHER_RESTRICTED 2 /* cipher is allowed in limited cases \
                                   e.g. step-up */

/* The length of the header string for each cipher table.
   (It's the same regardless of whether we're using md5 strings or not.) */
#define SEC_POLICY_HEADER_LENGTH 48

/* If we're testing policy stuff, we may want to use the plaintext version */
#define SEC_POLICY_USE_MD5_STRINGS 1

#define SEC_POLICY_THIS_IS_THE \
    "\x2a\x3a\x51\xbf\x2f\x71\xb7\x73\xaa\xca\x6b\x57\x70\xcd\xc8\x9f"
#define SEC_POLICY_STRING_FOR_THE \
    "\x97\x15\xe2\x70\xd2\x8a\xde\xa9\xe7\xa7\x6a\xe2\x83\xe5\xb1\xf6"
#define SEC_POLICY_SSL_TAIL \
    "\x70\x16\x25\xc0\x2a\xb2\x4a\xca\xb6\x67\xb1\x89\x20\xdf\x87\xca"
#define SEC_POLICY_SMIME_TAIL \
    "\xdf\xd4\xe7\x2a\xeb\xc4\x1b\xb5\xd8\xe5\xe0\x2a\x16\x9f\xc4\xb9"
#define SEC_POLICY_PKCS12_TAIL \
    "\x1c\xf8\xa4\x85\x4a\xc6\x8a\xfe\xe6\xca\x03\x72\x50\x1c\xe2\xc8"

#if defined(SEC_POLICY_USE_MD5_STRINGS)

/* We're not testing.
   Use md5 checksums of the strings. */

#define SEC_POLICY_SSL_HEADER \
    SEC_POLICY_THIS_IS_THE SEC_POLICY_STRING_FOR_THE SEC_POLICY_SSL_TAIL

#define SEC_POLICY_SMIME_HEADER \
    SEC_POLICY_THIS_IS_THE SEC_POLICY_STRING_FOR_THE SEC_POLICY_SMIME_TAIL

#define SEC_POLICY_PKCS12_HEADER \
    SEC_POLICY_THIS_IS_THE SEC_POLICY_STRING_FOR_THE SEC_POLICY_PKCS12_TAIL

#else

/* We're testing.
   Use plaintext versions of the strings, for testing purposes. */
#define SEC_POLICY_SSL_HEADER \
    "This is the string for the SSL policy table.    "
#define SEC_POLICY_SMIME_HEADER \
    "This is the string for the PKCS7 policy table.  "
#define SEC_POLICY_PKCS12_HEADER \
    "This is the string for the PKCS12 policy table. "

#endif

/* Local cipher tables have to have these members at the top. */
typedef struct _sec_cp_struct {
    char policy_string[SEC_POLICY_HEADER_LENGTH];
    long unused; /* placeholder for max keybits in pkcs12 struct */
    char num_ciphers;
    char begin_ciphers;
    /* cipher policy settings follow. each is a char. */
} secCPStruct;

struct SECCipherFindStr {
    /* (policy) and (ciphers) are opaque to the outside world */
    void *policy;
    void *ciphers;
    long index;
    PRBool onlyAllowed;
};

typedef struct SECCipherFindStr SECCipherFind;

SEC_BEGIN_PROTOS

SECCipherFind *sec_CipherFindInit(PRBool onlyAllowed,
                                  secCPStruct *policy,
                                  long *ciphers);

long sec_CipherFindNext(SECCipherFind *find);

char sec_IsCipherAllowed(long cipher, secCPStruct *policies,
                         long *ciphers);

void sec_CipherFindEnd(SECCipherFind *find);

SEC_END_PROTOS

#endif /* __SECPLCY_H__ */
