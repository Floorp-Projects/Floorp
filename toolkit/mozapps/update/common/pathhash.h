/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is for Maintenance service path hashing 
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian R. Bondy <netzen@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions /PGM and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _PATHHASH_H_
#define _PATHHASH_H_

/**
 * Converts a file path into a unique registry location for cert storage
 *
 * @param  filePath     The input file path to get a registry path from
 * @param  registryPath A buffer to write the registry path to, must 
 *                      be of size in WCHARs MAX_PATH + 1
 * @return TRUE if successful
*/
BOOL CalculateRegistryPathFromFilePath(const LPCWSTR filePath, 
                                       LPWSTR registryPath);

// The test only fallback key, as its name implies, is only present on machines
// that will use automated tests.  Since automated tests always run from a 
// different directory for each test, the presence of this key bypasses the
// "This is a valid installation directory" check.  This key also stores
// the allowed name and issuer for cert checks so that the cert check
// code can still be run unchanged.
#define TEST_ONLY_FALLBACK_KEY_PATH \
  L"SOFTWARE\\Mozilla\\MaintenanceService\\3932ecacee736d366d6436db0f55bce4"

#endif
