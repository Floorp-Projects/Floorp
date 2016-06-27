/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSSCKBI_H
#define NSSCKBI_H

/*
 * NSS BUILTINS Version numbers.
 *
 * These are the version numbers for the builtins module packaged with
 * this release on NSS. To determine the version numbers of the builtin
 * module you are using, use the appropriate PKCS #11 calls.
 *
 * These version numbers detail changes to the PKCS #11 interface. They map
 * to the PKCS #11 spec versions.
 */
#define NSS_BUILTINS_CRYPTOKI_VERSION_MAJOR 2
#define NSS_BUILTINS_CRYPTOKI_VERSION_MINOR 20

/* These version numbers detail the changes
 * to the list of trusted certificates.
 *
 * The NSS_BUILTINS_LIBRARY_VERSION_MINOR macro needs to be bumped
 * for each NSS minor release AND whenever we change the list of
 * trusted certificates.  10 minor versions are allocated for each
 * NSS 3.x branch as follows, allowing us to change the list of
 * trusted certificates up to 9 times on each branch.
 *   - NSS 3.5 branch:  3-9
 *   - NSS 3.6 branch:  10-19
 *   - NSS 3.7 branch:  20-29
 *   - NSS 3.8 branch:  30-39
 *   - NSS 3.9 branch:  40-49
 *   - NSS 3.10 branch: 50-59
 *   - NSS 3.11 branch: 60-69
 *     ...
 *   - NSS 3.12 branch: 70-89
 *   - NSS 3.13 branch: 90-99
 *   - NSS 3.14 branch: 100-109
 *     ...
 *   - NSS 3.29 branch: 250-255
 *
 * NSS_BUILTINS_LIBRARY_VERSION_MINOR is a CK_BYTE.  It's not clear
 * whether we may use its full range (0-255) or only 0-99 because
 * of the comment in the CK_VERSION type definition.
 */
#define NSS_BUILTINS_LIBRARY_VERSION_MAJOR 2
#define NSS_BUILTINS_LIBRARY_VERSION_MINOR 8
#define NSS_BUILTINS_LIBRARY_VERSION "2.8"

/* These version numbers detail the semantic changes to the ckfw engine. */
#define NSS_BUILTINS_HARDWARE_VERSION_MAJOR 1
#define NSS_BUILTINS_HARDWARE_VERSION_MINOR 0

/* These version numbers detail the semantic changes to ckbi itself
 * (new PKCS #11 objects), etc. */
#define NSS_BUILTINS_FIRMWARE_VERSION_MAJOR 1
#define NSS_BUILTINS_FIRMWARE_VERSION_MINOR 0

#endif /* NSSCKBI_H */
