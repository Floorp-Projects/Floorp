/*
 * Softoken version numbers
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

#ifndef _SOFTKVER_H_
#define _SOFTKVER_H_

#ifdef NSS_ENABLE_ECC
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
#define SOFTOKEN_VERSION  "3.12.8.0" SOFTOKEN_ECC_STRING
#define SOFTOKEN_VMAJOR   3
#define SOFTOKEN_VMINOR   12
#define SOFTOKEN_VPATCH   8
#define SOFTOKEN_VBUILD   0
#define SOFTOKEN_BETA     PR_FALSE

#endif /* _SOFTKVER_H_ */
