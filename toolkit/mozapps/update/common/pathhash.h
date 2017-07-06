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

#endif
