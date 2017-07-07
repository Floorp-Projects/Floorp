/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#include "registrycertificates.h"
#include "pathhash.h"
#include "updatecommon.h"
#include "updatehelper.h"
#define MAX_KEY_LENGTH 255

/**
 * Verifies if the file path matches any certificate stored in the registry.
 *
 * @param  filePath The file path of the application to check if allowed.
 * @param  allowFallbackKeySkip when this is TRUE the fallback registry key will
 *   be used to skip the certificate check.  This is the default since the
 *   fallback registry key is located under HKEY_LOCAL_MACHINE which can't be
 *   written to by a low integrity process.
 *   Note: the maintenance service binary can be used to perform this check for
 *   testing or troubleshooting.
 * @return TRUE if the binary matches any of the allowed certificates.
 */
BOOL
DoesBinaryMatchAllowedCertificates(LPCWSTR basePathForUpdate, LPCWSTR filePath,
                                   BOOL allowFallbackKeySkip)
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
  HKEY baseKey;
  LONG retCode = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                               maintenanceServiceKey, 0,
                               KEY_READ | KEY_WOW64_64KEY, &baseKey);
  if (retCode != ERROR_SUCCESS) {
    LOG_WARN(("Could not open key.  (%d)", retCode));
    // Our tests run with a different apply directory for each test.
    // We use this registry key on our test slaves to store the
    // allowed name/issuers.
    retCode = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                            TEST_ONLY_FALLBACK_KEY_PATH, 0,
                            KEY_READ | KEY_WOW64_64KEY, &baseKey);
    if (retCode != ERROR_SUCCESS) {
      LOG_WARN(("Could not open fallback key.  (%d)", retCode));
      return FALSE;
    } else if (allowFallbackKeySkip) {
      LOG_WARN(("Fallback key present, skipping VerifyCertificateTrustForFile "
                "check and the certificate attribute registry matching "
                "check."));
      RegCloseKey(baseKey);
      return TRUE;
    }
  }

  // Get the number of subkeys.
  DWORD subkeyCount = 0;
  retCode = RegQueryInfoKeyW(baseKey, nullptr, nullptr, nullptr, &subkeyCount,
                             nullptr, nullptr, nullptr, nullptr, nullptr,
                             nullptr, nullptr);
  if (retCode != ERROR_SUCCESS) {
    LOG_WARN(("Could not query info key.  (%d)", retCode));
    RegCloseKey(baseKey);
    return FALSE;
  }

  // Enumerate the subkeys, each subkey represents an allowed certificate.
  for (DWORD i = 0; i < subkeyCount; i++) {
    WCHAR subkeyBuffer[MAX_KEY_LENGTH];
    DWORD subkeyBufferCount = MAX_KEY_LENGTH;
    retCode = RegEnumKeyExW(baseKey, i, subkeyBuffer,
                            &subkeyBufferCount, nullptr,
                            nullptr, nullptr, nullptr);
    if (retCode != ERROR_SUCCESS) {
      LOG_WARN(("Could not enum certs.  (%d)", retCode));
      RegCloseKey(baseKey);
      return FALSE;
    }

    // Open the subkey for the current certificate
    HKEY subKey;
    retCode = RegOpenKeyExW(baseKey,
                            subkeyBuffer,
                            0,
                            KEY_READ | KEY_WOW64_64KEY,
                            &subKey);
    if (retCode != ERROR_SUCCESS) {
      LOG_WARN(("Could not open subkey.  (%d)", retCode));
      continue; // Try the next subkey
    }

    const int MAX_CHAR_COUNT = 256;
    DWORD valueBufSize = MAX_CHAR_COUNT * sizeof(WCHAR);
    WCHAR name[MAX_CHAR_COUNT] = { L'\0' };
    WCHAR issuer[MAX_CHAR_COUNT] = { L'\0' };

    // Get the name from the registry
    retCode = RegQueryValueExW(subKey, L"name", 0, nullptr,
                               (LPBYTE)name, &valueBufSize);
    if (retCode != ERROR_SUCCESS) {
      LOG_WARN(("Could not obtain name from registry.  (%d)", retCode));
      RegCloseKey(subKey);
      continue; // Try the next subkey
    }

    // Get the issuer from the registry
    valueBufSize = MAX_CHAR_COUNT * sizeof(WCHAR);
    retCode = RegQueryValueExW(subKey, L"issuer", 0, nullptr,
                               (LPBYTE)issuer, &valueBufSize);
    if (retCode != ERROR_SUCCESS) {
      LOG_WARN(("Could not obtain issuer from registry.  (%d)", retCode));
      RegCloseKey(subKey);
      continue; // Try the next subkey
    }

    CertificateCheckInfo allowedCertificate = {
      name,
      issuer,
    };

    retCode = CheckCertificateForPEFile(filePath, allowedCertificate);
    if (retCode != ERROR_SUCCESS) {
      LOG_WARN(("Error on certificate check.  (%d)", retCode));
      RegCloseKey(subKey);
      continue; // Try the next subkey
    }

    retCode = VerifyCertificateTrustForFile(filePath);
    if (retCode != ERROR_SUCCESS) {
      LOG_WARN(("Error on certificate trust check.  (%d)", retCode));
      RegCloseKey(subKey);
      continue; // Try the next subkey
    }

    RegCloseKey(baseKey);
    // Raise the roof, we found a match!
    return TRUE;
  }

  RegCloseKey(baseKey);
  // No certificates match, :'(
  return FALSE;
}
