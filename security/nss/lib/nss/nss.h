/*
 * NSS utility functions
 *
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
 *
 * $Id: nss.h,v 1.6 2001/02/04 05:52:42 wtc%netscape.com Exp $
 */

#ifndef __nss_h_
#define __nss_h_

#include "seccomon.h"

SEC_BEGIN_PROTOS

/*
 * NSS's major version, minor version, patch level, and whether
 * this is a beta release.
 *
 * The format of the version string should be
 *     "<major version>.<minor version>[.<patch level>] [<Beta>]"
 */
#define NSS_VERSION  "3.2 Beta"
#define NSS_VMAJOR   3
#define NSS_VMINOR   2
#define NSS_VPATCH   0
#define NSS_BETA     PR_TRUE

/*
 * Return a boolean that indicates whether the underlying library
 * will perform as the caller expects.
 *
 * The only argument is a string, which should be the verson
 * identifier of the NSS library. That string will be compared
 * against a string that represents the actual build version of
 * the NSS library.  It also invokes the version checking functions
 * of the dependent libraries such as NSPR.
 */
extern PRBool NSS_VersionCheck(const char *importedVersion);

/*
 * Open the Cert, Key, and Security Module databases, read only.
 * Initialize the Random Number Generator.
 * Does not initialize the cipher policies or enables.
 * Default policy settings disallow all ciphers.
 */
extern SECStatus NSS_Init(const char *configdir);

/*
 * Open the Cert, Key, and Security Module databases, read/write.
 * Initialize the Random Number Generator.
 * Does not initialize the cipher policies or enables.
 * Default policy settings disallow all ciphers.
 */
extern SECStatus NSS_InitReadWrite(const char *configdir);

/*
 * Open the Cert, Key, and Security Module databases, read/write.
 * Initialize the Random Number Generator.
 * Does not initialize the cipher policies or enables.
 * Default policy settings disallow all ciphers.
 *
 * This allows using application defined prefixes for the cert and key db's
 * and an alternate name for the secmod database. NOTE: In future releases,
 * the database prefixes my not necessarily map to database names.
 *
 * Also NOTE: This is not the recommended method for initializing NSS. 
 * The prefered method is NSS_init().
 */
extern SECStatus NSS_Initialize(const char *configdir, 
	const char *certPrefix, const char *keyPrefix, const char *secmodName,
	PRBool readOnly);

/*
 * initialize NSS without a creating cert db's, key db's, or secmod db's.
 */
SECStatus NSS_NoDB_Init(const char *configdir);

/* 
 * Close the Cert, Key databases.
 */
extern void NSS_Shutdown(void);

SEC_END_PROTOS

#endif /* __nss_h_ */
