/*
 * Softoken version numbers
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _SOFTKVER_H_
#define _SOFTKVER_H_

#ifndef NSS_DISABLE_ECC
#ifdef NSS_ECC_MORE_THAN_SUITE_B
#define SOFTOKEN_ECC_STRING " Extended ECC"
#else
#define SOFTOKEN_ECC_STRING " Basic ECC"
#endif
#else
#define SOFTOKEN_ECC_STRING ""
#endif

/*
 * Softoken's major version, minor version, patch level, build number,
 * and whether this is a beta release.
 *
 * The format of the version string should be
 *     "<major version>.<minor version>[.<patch level>[.<build number>]][ <ECC>][ <Beta>]"
 */
#define SOFTOKEN_VERSION  "3.18" SOFTOKEN_ECC_STRING " Beta"
#define SOFTOKEN_VMAJOR   3
#define SOFTOKEN_VMINOR   18
#define SOFTOKEN_VPATCH   0
#define SOFTOKEN_VBUILD   0
#define SOFTOKEN_BETA     PR_TRUE

#endif /* _SOFTKVER_H_ */
