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
 * The Original Code is Maintenance service prefetch cleaning.
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

#include <shlwapi.h>
#include "servicebase.h"
#include "updatehelper.h"
#include "nsWindowsHelpers.h"
#define MAX_KEY_LENGTH 255

/**
 * Writes a registry DWORD with a value of 1 at BASE_SERVICE_REG_KEY
 * if the prefetch was cleared successfully.
 *
 * @return TRUE if successful.
*/
BOOL
WritePrefetchClearedReg()
{
  HKEY baseKeyRaw;
  LONG retCode = RegOpenKeyExW(HKEY_LOCAL_MACHINE, 
                               BASE_SERVICE_REG_KEY, 0,
                               KEY_WRITE | KEY_WOW64_64KEY, &baseKeyRaw);
  if (retCode != ERROR_SUCCESS) {
    LOG(("Could not open key for prefetch. (%d)\n", retCode));
    return FALSE;
  }
  nsAutoRegKey baseKey(baseKeyRaw);
  DWORD disabledValue = 1;
  if (RegSetValueExW(baseKey, L"FFPrefetchDisabled", 0, REG_DWORD,
                     reinterpret_cast<LPBYTE>(&disabledValue),
                     sizeof(disabledValue)) != ERROR_SUCCESS) {
    LOG(("Could not write prefetch cleared value to registry. (%d)\n",
         GetLastError()));
    return FALSE;
  }
  return TRUE;
}

static BOOL
PrefetchFileAlreadyCleared(LPCWSTR prefetchPath)
{
  DWORD attributes = GetFileAttributes(prefetchPath);
  BOOL alreadyCleared = attributes != INVALID_FILE_ATTRIBUTES &&
                        attributes & FILE_ATTRIBUTE_READONLY;

  nsAutoHandle prefetchFile(CreateFile(prefetchPath, GENERIC_READ, 0, NULL,
                                       OPEN_EXISTING, 0, NULL));
  LARGE_INTEGER fileSize = { 0, 0 };
  alreadyCleared &= prefetchFile != INVALID_HANDLE_VALUE &&
                    GetFileSizeEx(prefetchFile, &fileSize) &&
                    fileSize.QuadPart == 0;
  return alreadyCleared;
}

/** 
  * We found that prefetch actually causes large applications like Firefox
  * to startup slower.  This will get rid of the Windows prefetch files for
  * applications like firefox (FIREFOX-*.pf files) and instead replace them
  * with 0 byte files which are read only.  Windows will not use prefetch
  * if the associated prefetch file is a 0 byte read only file.
  * Some of the problems with prefetch relating to Windows and Firefox:
  * - Prefetch reads in a lot before we even run (fonts, plugins, etc...)
  * - Prefetch happens before our code runs, so it delays UI showing.
  * - Prefetch does not use windows readahead.
  * Previous test data on an acer i7 laptop with a 5400rpm HD showed startup
  * time difference was 1.6s (without prefetch) vs 2.6s+ with prefetch.
  * The "Windows Internals" book mentions that prefetch can be disabled for 
  * the whole system only, so there is no other application specific way to
  * disable it.
  *
  * @param prefetchProcessName The name of the process who's prefetch files
  *                            should be cleared.
  * @return TRUE if no errors occurred during the clear operation.
*/
BOOL
ClearPrefetch(LPCWSTR prefetchProcessName)
{
  LOG(("Clearing prefetch files...\n"));
  size_t prefetchProcessNameLength = wcslen(prefetchProcessName);

  // Make sure we were passed in a safe value for the prefetch process name
  // because it will be appended to a filepath and deleted.
  // We check for < 2 to avoid things like "." as the path 
  // We check for a max path of MAX_PATH - 10 because we add \\ and '.EXE-*.pf'
  if (wcsstr(prefetchProcessName, L"..") ||
      wcsstr(prefetchProcessName, L"\\") ||
      wcsstr(prefetchProcessName, L"*") ||
      wcsstr(prefetchProcessName, L"/") ||
      prefetchProcessNameLength < 2 ||
      prefetchProcessNameLength >= (MAX_PATH - 10)) {
    LOG(("Prefetch path to clear is not safe\n"));
    return FALSE;
  }

  // Quick shortcut to make sure we don't try to clear multiple times for
  // different FIREFOX installs.
  static WCHAR lastPrefetchProcessName[MAX_PATH - 10] = { '\0' };
  if (!wcscmp(lastPrefetchProcessName, prefetchProcessName)) {
    LOG(("Already processed process name\n"));
    return FALSE;
  }
  wcscpy(lastPrefetchProcessName, prefetchProcessName);

  // Obtain the windows prefetch directory path.
  WCHAR prefetchPath[MAX_PATH + 1];
  if (!GetWindowsDirectoryW(prefetchPath,
                            sizeof(prefetchPath) / 
                            sizeof(prefetchPath[0]))) {
    LOG(("Could not obtain windows directory\n"));
    return FALSE;
  }
  if (!PathAppendSafe(prefetchPath, L"prefetch")) {
    LOG(("Could not obtain prefetch directory\n"));
    return FALSE;
  }

  size_t prefetchDirLen = wcslen(prefetchPath);
  WCHAR prefetchSearchFile[MAX_PATH + 1];
  // We know this is safe based on the check at the start of this function
  wsprintf(prefetchSearchFile, L"\\%s.EXE-*.pf", prefetchProcessName);
  // Append the search file to the full path
  wcscpy(prefetchPath + prefetchDirLen, prefetchSearchFile);

  // Find the first path matching and get a find handle for future calls.
  WIN32_FIND_DATAW findFileData;
  HANDLE findHandle = FindFirstFileW(prefetchPath, &findFileData);
  if (INVALID_HANDLE_VALUE == findHandle) {
    if (GetLastError() == ERROR_FILE_NOT_FOUND) {
      LOG(("No files matching firefox.exe prefetch path.\n"));
      return TRUE;
    } else {
      LOG(("Error finding firefox.exe prefetch files. (%d)\n",
           GetLastError()));
      return FALSE;
    }
  }
  
  BOOL deletedAllFFPrefetch = TRUE;
  size_t deletedCount = 0;
  do {
    // Reset back to the prefetch directory, we know from an above check that
    // we aren't exceeding MAX_PATH + 1 characters with prefetchDirLen + 1.
    // From above we know: prefetchPath[prefetchDirLen] == L'\\';
    prefetchPath[prefetchDirLen + 1] = L'\0';

    // Get the new file's prefetch path
    LPWSTR filenameOffsetInBuffer = prefetchPath + prefetchDirLen + 1;
    if (wcslen(findFileData.cFileName) + prefetchDirLen + 1 > MAX_PATH) {
      LOG(("Error appending prefetch path %ls, path is too long. (%d)\n",
           findFileData.cFileName, GetLastError()));
      deletedAllFFPrefetch = FALSE;
      continue;
    }
    if (!PathAppendSafe(filenameOffsetInBuffer, findFileData.cFileName)) {
      LOG(("Error appending prefetch path %ls. (%d)\n", findFileData.cFileName,
           GetLastError()));
      deletedAllFFPrefetch = FALSE;
      continue;
    }

    if (PrefetchFileAlreadyCleared(prefetchPath)) {
      ++deletedCount;
      LOG(("Prefetch file already cleared: %ls\n", prefetchPath));
      continue;
    }

    // Delete the prefetch file and replace it with a blank read only file
    HANDLE prefetchFile =
      CreateFile(prefetchPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (INVALID_HANDLE_VALUE == prefetchFile) {
      LOG(("Error replacing prefetch path %ls. (%d)\n", findFileData.cFileName, 
           GetLastError()));
      deletedAllFFPrefetch = FALSE;
      continue;
    }

    CloseHandle(prefetchFile);

    DWORD attributes = GetFileAttributes(prefetchPath);
    if (INVALID_FILE_ATTRIBUTES == attributes) {
      LOG(("Could not get/set attributes on prefetch file: %ls. (%d)\n", 
           findFileData.cFileName, GetLastError()));
      continue;
    }

    if (!SetFileAttributes(prefetchPath, 
                          attributes | FILE_ATTRIBUTE_READONLY)) {
      LOG(("Could not set read only on prefetch file: %ls. (%d)\n", 
           findFileData.cFileName, GetLastError()));
      continue;
    } 

    ++deletedCount;
    LOG(("Prefetch file cleared and set to read-only successfully: %ls\n", 
         prefetchPath));
  } while (FindNextFileW(findHandle, &findFileData));
  LOG(("Done searching prefetch paths. (%d)\n", GetLastError()));

  // Cleanup after ourselves.
  FindClose(findHandle);

  if (deletedAllFFPrefetch && deletedCount > 0) {
    WritePrefetchClearedReg();
  }

  return deletedAllFFPrefetch;
}

/**
 * Clears all prefetch files specified in the installation dirctories
 * @return FALSE if there was an error clearing the known prefetch directories
*/
BOOL
ClearKnownPrefetch()
{
  // The service always uses the 64-bit registry key
  HKEY baseKeyRaw;
  LONG retCode = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                               BASE_SERVICE_REG_KEY, 0,
                               KEY_READ | KEY_WOW64_64KEY, &baseKeyRaw);
  if (retCode != ERROR_SUCCESS) {
    LOG(("Could not open maintenance service base key. (%d)\n", retCode));
    return FALSE;
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

  // Enumerate the subkeys, each subkey represents an install
  for (DWORD i = 0; i < subkeyCount; i++) { 
    WCHAR subkeyBuffer[MAX_KEY_LENGTH];
    DWORD subkeyBufferCount = MAX_KEY_LENGTH;  
    retCode = RegEnumKeyExW(baseKey, i, subkeyBuffer, 
                            &subkeyBufferCount, NULL, 
                            NULL, NULL, NULL); 
    if (retCode != ERROR_SUCCESS) {
      LOG(("Could not enum installations: %d\n", retCode));
      return FALSE;
    }

    // Open the subkey
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
    WCHAR prefetchProcessName[MAX_CHAR_COUNT] = { L'\0' };

    // Get the prefetch process name from the registry
    retCode = RegQueryValueExW(subKey, L"prefetchProcessName", 0, NULL, 
                               (LPBYTE)prefetchProcessName, &valueBufSize);
    if (retCode != ERROR_SUCCESS) {
      LOG(("Could not obtain process name from registry: %d\n", retCode));
      continue; // Try the next subkey
    }

    // The value for prefetch process name comes from HKLM so is trusted.
    // We will do some sanity checks on it though inside ClearPrefetch.
    ClearPrefetch(prefetchProcessName);
  }

  return TRUE;
}
