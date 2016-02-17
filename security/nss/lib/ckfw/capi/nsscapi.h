/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSSCAPI_H
#define NSSCAPI_H

/*
 * NSS CKCAPI Version numbers.
 *
 * These are the version numbers for the capi module packaged with
 * this release on NSS. To determine the version numbers of the builtin
 * module you are using, use the appropriate PKCS #11 calls.
 *
 * These version numbers detail changes to the PKCS #11 interface. They map
 * to the PKCS #11 spec versions.
 */
#define NSS_CKCAPI_CRYPTOKI_VERSION_MAJOR 2
#define NSS_CKCAPI_CRYPTOKI_VERSION_MINOR 20

/* These version numbers detail the changes
 * to the list of trusted certificates.
 *
 * NSS_CKCAPI_LIBRARY_VERSION_MINOR is a CK_BYTE.  It's not clear
 * whether we may use its full range (0-255) or only 0-99 because
 * of the comment in the CK_VERSION type definition.
 */
#define NSS_CKCAPI_LIBRARY_VERSION_MAJOR 1
#define NSS_CKCAPI_LIBRARY_VERSION_MINOR 1
#define NSS_CKCAPI_LIBRARY_VERSION "1.1"

/* These version numbers detail the semantic changes to the ckfw engine. */
#define NSS_CKCAPI_HARDWARE_VERSION_MAJOR 1
#define NSS_CKCAPI_HARDWARE_VERSION_MINOR 0

/* These version numbers detail the semantic changes to ckbi itself
 * (new PKCS #11 objects), etc. */
#define NSS_CKCAPI_FIRMWARE_VERSION_MAJOR 1
#define NSS_CKCAPI_FIRMWARE_VERSION_MINOR 0

#endif /* NSSCKBI_H */
