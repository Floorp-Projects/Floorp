/*
 * softoknt.h - public data structures for the software token library
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
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
/* $Id: softoknt.h,v 1.6 2009/08/03 16:58:28 christophe.ravel.bugs%sun.com Exp $ */

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
