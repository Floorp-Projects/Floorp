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
 */

#ifndef NSSCKBI_H
#define NSSCKBI_H

/*
 * NSS BUILTINS Version numbers.
 *
 * These are the version numbers for the builtins module packaged with
 * this release on NSS. To determine the version numbers of the builtin
 * module you are using, use the appropriate PKCS #11 calls.
 *
 * These version number details changes to the PKCS #11 interface. They map
 * to the PKCS #11 spec versions.
 */
#define NSS_BUILTINS_CRYPTOKI_VERSION_MAJOR 2
#define NSS_BUILTINS_CRYPTOKI_VERSION_MINOR 1

/* These are the  correct verion numbers that details the changes 
 * to the list of trusted certificates.  */
#define NSS_BUILTINS_LIBRARY_VERSION_MAJOR 1
#define NSS_BUILTINS_LIBRARY_VERSION_MINOR 20

/* These verion numbers that details the semantic changes to the ckfw engine. */
#define NSS_BUILTINS_HARDWARE_VERSION_MAJOR 1
#define NSS_BUILTINS_HARDWARE_VERSION_MINOR 0

/* These verion numbers that details the semantic changes to ckbi itself 
 * (new PKCS #11 objects), etc. */
#define NSS_BUILTINS_FIRMWARE_VERSION_MAJOR 1
#define NSS_BUILTINS_FIRMWARE_VERSION_MINOR 0

#endif /* NSSCKBI_H */
