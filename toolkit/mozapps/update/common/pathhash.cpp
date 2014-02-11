/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>
#include <wincrypt.h>
#include "pathhash.h"


/**
 * Converts a binary sequence into a hex string
 *
 * @param hash      The binary data sequence
 * @param hashSize  The size of the binary data sequence
 * @param hexString A buffer to store the hex string, must be of 
 *                  size 2 * @hashSize
*/
static void
BinaryDataToHexString(const BYTE *hash, DWORD &hashSize, 
                      LPWSTR hexString)
{
  WCHAR *p = hexString;
  for (DWORD i = 0; i < hashSize; ++i) {
    wsprintfW(p, L"%.2x", hash[i]);
    p += 2;
  }
}

/**
 * Calculates an MD5 hash for the given input binary data
 *
 * @param  data     Any sequence of bytes
 * @param  dataSize The number of bytes inside @data
 * @param  hash     Output buffer to store hash, must be freed by the caller
 * @param  hashSize The number of bytes in the output buffer
 * @return TRUE on success
*/
static BOOL
CalculateMD5(const char *data, DWORD dataSize, 
             BYTE **hash, DWORD &hashSize)
{
  HCRYPTPROV hProv = 0;
  HCRYPTHASH hHash = 0;

  if (!CryptAcquireContext(&hProv, nullptr, nullptr, PROV_RSA_FULL,
                           CRYPT_VERIFYCONTEXT)) {
    if (NTE_BAD_KEYSET != GetLastError()) {
      return FALSE;
    }
 
    // Maybe it doesn't exist, try to create it.
    if (!CryptAcquireContext(&hProv, nullptr, nullptr, PROV_RSA_FULL, 
                             CRYPT_VERIFYCONTEXT | CRYPT_NEWKEYSET)) {
      return FALSE;
    }
  }

  if (!CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash)) {
    return FALSE;
  }

  if (!CryptHashData(hHash, reinterpret_cast<const BYTE*>(data), 
                    dataSize, 0)) {
    return FALSE;
  }

  DWORD dwCount = sizeof(DWORD);
  if (!CryptGetHashParam(hHash, HP_HASHSIZE, (BYTE *)&hashSize, 
                        &dwCount, 0)) {
    return FALSE;
  }
  
  *hash = new BYTE[hashSize];
  ZeroMemory(*hash, hashSize);
  if (!CryptGetHashParam(hHash, HP_HASHVAL, *hash, &hashSize, 0)) {
    return FALSE;
  }

  if (hHash) {
    CryptDestroyHash(hHash);
  }

  if (hProv) {
    CryptReleaseContext(hProv,0);
  }

  return TRUE;
}

/**
 * Converts a file path into a unique registry location for cert storage
 *
 * @param  filePath     The input file path to get a registry path from
 * @param  registryPath A buffer to write the registry path to, must 
 *                      be of size in WCHARs MAX_PATH + 1
 * @return TRUE if successful
*/
BOOL
CalculateRegistryPathFromFilePath(const LPCWSTR filePath, 
                                  LPWSTR registryPath)
{
  size_t filePathLen = wcslen(filePath); 
  if (!filePathLen) {
    return FALSE;
  }

  // If the file path ends in a slash, ignore that character
  if (filePath[filePathLen -1] == L'\\' || 
      filePath[filePathLen - 1] == L'/') {
    filePathLen--;
  }

  // Copy in the full path into our own buffer.
  // Copying in the extra slash is OK because we calculate the hash
  // based on the filePathLen which excludes the slash.
  // +2 to account for the possibly trailing slash and the null terminator.
  WCHAR *lowercasePath = new WCHAR[filePathLen + 2];
  memset(lowercasePath, 0, (filePathLen + 2) * sizeof(WCHAR));
  wcsncpy(lowercasePath, filePath, filePathLen + 1);
  _wcslwr(lowercasePath);

  BYTE *hash;
  DWORD hashSize = 0;
  if (!CalculateMD5(reinterpret_cast<const char*>(lowercasePath), 
                    filePathLen * 2, 
                    &hash, hashSize)) {
    delete[] lowercasePath;
    return FALSE;
  }
  delete[] lowercasePath;

  LPCWSTR baseRegPath = L"SOFTWARE\\Mozilla\\"
    L"MaintenanceService\\";
  wcsncpy(registryPath, baseRegPath, MAX_PATH);
  BinaryDataToHexString(hash, hashSize, 
                        registryPath + wcslen(baseRegPath));
  delete[] hash;
  return TRUE;
}
