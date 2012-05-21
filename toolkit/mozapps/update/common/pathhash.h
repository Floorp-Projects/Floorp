/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
