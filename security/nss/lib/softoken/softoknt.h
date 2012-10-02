/*
 * softoknt.h - public data structures for the software token library
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* $Id: softoknt.h,v 1.7 2012/04/25 14:50:10 gerv%gerv.net Exp $ */

#ifndef _SOFTOKNT_H_
#define _SOFTOKNT_H_

/*
 * RSA block types
 *
 * The actual values are important -- they are fixed, *not* arbitrary.
 * The explicit value assignments are not needed (because C would give
 * us those same values anyway) but are included as a reminder...
 */
typedef enum {
    RSA_BlockPrivate0 = 0,	/* unused, really */
    RSA_BlockPrivate = 1,	/* pad for a private-key operation */
    RSA_BlockPublic = 2,	/* pad for a public-key operation */
    RSA_BlockOAEP = 3,		/* use OAEP padding */
				/* XXX is this only for a public-key
				   operation? If so, add "Public" */
    RSA_BlockRaw = 4,		/* simply justify the block appropriately */
    RSA_BlockTotal
} RSA_BlockType;

#define NSS_SOFTOKEN_DEFAULT_CHUNKSIZE   2048

/*
 * FIPS 140-2 auditing
 */
typedef enum {
    NSS_AUDIT_ERROR = 3,    /* errors */
    NSS_AUDIT_WARNING = 2,  /* warning messages */
    NSS_AUDIT_INFO = 1      /* informational messages */
} NSSAuditSeverity;

typedef enum {
    NSS_AUDIT_ACCESS_KEY = 0,
    NSS_AUDIT_CHANGE_KEY,
    NSS_AUDIT_COPY_KEY,
    NSS_AUDIT_CRYPT,
    NSS_AUDIT_DERIVE_KEY,
    NSS_AUDIT_DESTROY_KEY,
    NSS_AUDIT_DIGEST_KEY,
    NSS_AUDIT_FIPS_STATE,
    NSS_AUDIT_GENERATE_KEY,
    NSS_AUDIT_INIT_PIN,
    NSS_AUDIT_INIT_TOKEN,
    NSS_AUDIT_LOAD_KEY,
    NSS_AUDIT_LOGIN,
    NSS_AUDIT_LOGOUT,
    NSS_AUDIT_SELF_TEST,
    NSS_AUDIT_SET_PIN,
    NSS_AUDIT_UNWRAP_KEY,
    NSS_AUDIT_WRAP_KEY
} NSSAuditType;

#endif /* _SOFTOKNT_H_ */
