/*
 * blapii.h - private data structures and prototypes for the freebl library
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _BLAPII_H_
#define _BLAPII_H_

#include "blapit.h"
#include "mpi.h"

/* max block size of supported block ciphers */
#define MAX_BLOCK_SIZE 16

typedef SECStatus (*freeblCipherFunc)(void *cx, unsigned char *output,
                                      unsigned int *outputLen, unsigned int maxOutputLen,
                                      const unsigned char *input, unsigned int inputLen,
                                      unsigned int blocksize);
typedef void (*freeblDestroyFunc)(void *cx, PRBool freeit);

SEC_BEGIN_PROTOS

#ifndef NSS_FIPS_DISABLED
SECStatus BL_FIPSEntryOK(PRBool freeblOnly);
PRBool BL_POSTRan(PRBool freeblOnly);
#endif

#if defined(XP_UNIX) && !defined(NO_FORK_CHECK)

extern PRBool bl_parentForkedAfterC_Initialize;

#define SKIP_AFTER_FORK(x)                 \
    if (!bl_parentForkedAfterC_Initialize) \
    x

#else

#define SKIP_AFTER_FORK(x) x

#endif

SEC_END_PROTOS

#if defined(NSS_X86_OR_X64)
#define HAVE_UNALIGNED_ACCESS 1
#endif

#if defined(__clang__)
#define HAVE_NO_SANITIZE_ATTR __has_attribute(no_sanitize)
#else
#define HAVE_NO_SANITIZE_ATTR 0
#endif

/* Alignment helpers. */
#if defined(_WINDOWS) && defined(NSS_X86_OR_X64)
#define pre_align __declspec(align(16))
#define post_align
#elif defined(NSS_X86_OR_X64)
#define pre_align
#define post_align __attribute__((aligned(16)))
#else
#define pre_align
#define post_align
#endif

#if defined(HAVE_UNALIGNED_ACCESS) && HAVE_NO_SANITIZE_ATTR
#define NO_SANITIZE_ALIGNMENT __attribute__((no_sanitize("alignment")))
#else
#define NO_SANITIZE_ALIGNMENT
#endif

#undef HAVE_NO_SANITIZE_ATTR

SECStatus RSA_Init();
SECStatus generate_prime(mp_int *prime, int primeLen);

/* Freebl state. */
PRBool aesni_support();
PRBool clmul_support();
PRBool avx_support();
PRBool ssse3_support();
PRBool arm_neon_support();
PRBool arm_aes_support();
PRBool arm_pmull_support();
PRBool arm_sha1_support();
PRBool arm_sha2_support();
PRBool ppc_crypto_support();

#endif /* _BLAPII_H_ */
