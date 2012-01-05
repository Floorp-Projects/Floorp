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
 * The Original Code is for Maintenance service listing allowed certificates
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
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#include "registrycertificates.h"
#include "pathhash.h"
#include "nsWindowsHelpers.h"
#include "servicebase.h"
#define MAX_KEY_LENGTH 255

/**
 * Verifies if the file path matches any certificate stored in the registry.
 *
 * @param  filePath The file path of the application to check if allowed.
 * @return TRUE if the binary matches any of the allowed certificates.
 */
BOOL
DoesBinaryMatchAllowedCertificates(LPCWSTR basePathForUpdate, LPCWSTR filePath)
{ 
  WCHAR maintenanceServiceKey[MAX_PATH + 1];
  if (!CalculateRegistryPathFromFilePath(basePathForUpdate, 
                                         maintenanceServiceKey)) {
    return FALSE;
  }

  // We use KEY_WOW64_64KEY to always force 64-bit view.
  // The user may have both x86 and x64 applications installed
  // which each register information.  We need a consistent place
  // to put those certificate attributes in and hence why we always
  // force the non redirected registry under Wow6432Node.
  // This flag is ignored on 32bit systems.
  HKEY baseKeyRaw;
  LONG retCode = RegOpenKeyExW(HKEY_LOCAL_MACHINE, 
                               maintenanceServiceKey, 0, 
                               KEY_READ | KEY_WOW64_64KEY, &baseKeyRaw);
  if (retCode != ERROR_SUCCESS) {
    LOG(("Could not open key. (%d)\n", retCode));
    // Our tests run with a different apply directory for each test.
    // We use this registry key on our test slaves to store the 
    // allowed name/issuers.
    retCode = RegOpenKeyExW(HKEY_LOCAL_MACHINE, 
                            TEST_ONLY_FALLBACK_KEY_PATH, 0,
                            KEY_READ | KEY_WOW64_64KEY, &baseKeyRaw);
    if (retCode != ERROR_SUCCESS) {
      LOG(("Could not open fallback key. (%d)\n", retCode));
      return FALSE;
    }
  }
  nsAutoRegKey baseKey(baseKeyRaw);

  // Get the number of subkeys.
  DWORD subkeyCount = 0;
  retCode = RegQueryInfoKeyW(baseKey, NULL, NULL, NULL, &subkeyCount, NULL,
                             NULL, NULL, NULL, NULL, NULL, NULL);
  if (retCode != ERROR_SUCCESS) {
    LOG(("Could not query info key: %d\n", retCode));
    return FALSE;
  }

  // Enumerate the subkeys, each subkey represents an allowed certificate.
  for (DWORD i = 0; i < subkeyCount; i++) { 
    WCHAR subkeyBuffer[MAX_KEY_LENGTH];
    DWORD subkeyBufferCount = MAX_KEY_LENGTH;  
    retCode = RegEnumKeyExW(baseKey, i, subkeyBuffer, 
                            &subkeyBufferCount, NULL, 
                            NULL, NULL, NULL); 
    if (retCode != ERROR_SUCCESS) {
      LOG(("Could not enum Certs: %d\n", retCode));
      return FALSE;
    }

    // Open the subkey for the current certificate
    HKEY subKeyRaw;
    retCode = RegOpenKeyExW(baseKey, 
                            subkeyBuffer, 
                            0, 
                            KEY_READ | KEY_WOW64_64KEY, 
                            &subKeyRaw);
    nsAutoRegKey subKey(subKeyRaw);
    if (retCode != ERROR_SUCCESS) {
      LOG(("Could not open subkey: %d\n", retCode));
      continue; // Try the next subkey
    }

    const int MAX_CHAR_COUNT = 256;
    DWORD valueBufSize = MAX_CHAR_COUNT * sizeof(WCHAR);
    WCHAR name[MAX_CHAR_COUNT] = { L'\0' };
    WCHAR issuer[MAX_CHAR_COUNT] = { L'\0' };

    // Get the name from the registry
    retCode = RegQueryValueExW(subKey, L"name", 0, NULL, 
                               (LPBYTE)name, &valueBufSize);
    if (retCode != ERROR_SUCCESS) {
      LOG(("Could not obtain name from registry: %d\n", retCode));
      continue; // Try the next subkey
    }

    // Get the issuer from the registry
    valueBufSize = MAX_CHAR_COUNT * sizeof(WCHAR);
    retCode = RegQueryValueExW(subKey, L"issuer", 0, NULL, 
                               (LPBYTE)issuer, &valueBufSize);
    if (retCode != ERROR_SUCCESS) {
      LOG(("Could not obtain issuer from registry: %d\n", retCode));
      continue; // Try the next subkey
    }

    CertificateCheckInfo allowedCertificate = {
      name, 
      issuer, 
    };

    retCode = CheckCertificateForPEFile(filePath, allowedCertificate);
    if (retCode != ERROR_SUCCESS) {
      LOG(("Error on certificate check: %d\n", retCode));
      continue; // Try the next subkey
    }

    retCode = VerifyCertificateTrustForFile(filePath);
    if (retCode != ERROR_SUCCESS) {
      LOG(("Error on certificate trust check: %d\n", retCode));
      continue; // Try the next subkey
    }

    // Raise the roof, we found a match!
    return TRUE; 
  }
  
  // No certificates match, :'(
  return FALSE;
}
