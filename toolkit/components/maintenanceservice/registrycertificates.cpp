/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#include "registrycertificates.h"
#include "pathhash.h"
#include "nsWindowsHelpers.h"
#include "servicebase.h"
#include "updatehelper.h"
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
    LOG_WARN(("Could not open key.  (%d)", retCode));
    // Our tests run with a different apply directory for each test.
    // We use this registry key on our test slaves to store the 
    // allowed name/issuers.
    retCode = RegOpenKeyExW(HKEY_LOCAL_MACHINE, 
                            TEST_ONLY_FALLBACK_KEY_PATH, 0,
                            KEY_READ | KEY_WOW64_64KEY, &baseKeyRaw);
    if (retCode != ERROR_SUCCESS) {
      LOG_WARN(("Could not open fallback key.  (%d)", retCode));
      return FALSE;
    }
  }
  nsAutoRegKey baseKey(baseKeyRaw);

  // Get the number of subkeys.
  DWORD subkeyCount = 0;
  retCode = RegQueryInfoKeyW(baseKey, NULL, NULL, NULL, &subkeyCount, NULL,
                             NULL, NULL, NULL, NULL, NULL, NULL);
  if (retCode != ERROR_SUCCESS) {
    LOG_WARN(("Could not query info key.  (%d)", retCode));
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
      LOG_WARN(("Could not enum certs.  (%d)", retCode));
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
      LOG_WARN(("Could not open subkey.  (%d)", retCode));
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
      LOG_WARN(("Could not obtain name from registry.  (%d)", retCode));
      continue; // Try the next subkey
    }

    // Get the issuer from the registry
    valueBufSize = MAX_CHAR_COUNT * sizeof(WCHAR);
    retCode = RegQueryValueExW(subKey, L"issuer", 0, NULL, 
                               (LPBYTE)issuer, &valueBufSize);
    if (retCode != ERROR_SUCCESS) {
      LOG_WARN(("Could not obtain issuer from registry.  (%d)", retCode));
      continue; // Try the next subkey
    }

    CertificateCheckInfo allowedCertificate = {
      name, 
      issuer, 
    };

    retCode = CheckCertificateForPEFile(filePath, allowedCertificate);
    if (retCode != ERROR_SUCCESS) {
      LOG_WARN(("Error on certificate check.  (%d)", retCode));
      continue; // Try the next subkey
    }

    retCode = VerifyCertificateTrustForFile(filePath);
    if (retCode != ERROR_SUCCESS) {
      LOG_WARN(("Error on certificate trust check.  (%d)", retCode));
      continue; // Try the next subkey
    }

    // Raise the roof, we found a match!
    return TRUE; 
  }
  
  // No certificates match, :'(
  return FALSE;
}
