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
 * The Original Code is Mozilla Maintenance service base code.
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

#include "servicebase.h"
#include "nsWindowsHelpers.h"

// Shared code between applications and updater.exe
#include "nsWindowsRestart.cpp"

/**
 * Verifies if 2 files are byte for byte equivalent.
 *
 * @param  file1Path   The first file to verify.
 * @param  file2Path   The second file to verify.
 * @param  sameContent Out parameter, TRUE if the files are equal
 * @return TRUE If there was no error checking the files.
 */
BOOL
VerifySameFiles(LPCWSTR file1Path, LPCWSTR file2Path, BOOL &sameContent)
{
  sameContent = FALSE;
  nsAutoHandle file1(CreateFileW(file1Path, GENERIC_READ, FILE_SHARE_READ, 
                                 NULL, OPEN_EXISTING, 0, NULL));
  if (INVALID_HANDLE_VALUE == file1) {
    return FALSE;
  }
  nsAutoHandle file2(CreateFileW(file2Path, GENERIC_READ, FILE_SHARE_READ, 
                                 NULL, OPEN_EXISTING, 0, NULL));
  if (INVALID_HANDLE_VALUE == file2) {
    return FALSE;
  }

  DWORD fileSize1 = GetFileSize(file1, NULL);
  DWORD fileSize2 = GetFileSize(file2, NULL);
  if (INVALID_FILE_SIZE == fileSize1 || INVALID_FILE_SIZE == fileSize2) {
    return FALSE;
  }

  if (fileSize1 != fileSize2) {
    // sameContent is already set to FALSE
    return TRUE;
  }

  char buf1[COMPARE_BLOCKSIZE];
  char buf2[COMPARE_BLOCKSIZE];
  DWORD numBlocks = fileSize1 / COMPARE_BLOCKSIZE;
  DWORD leftOver = fileSize1 % COMPARE_BLOCKSIZE;
  DWORD readAmount;
  for (DWORD i = 0; i < numBlocks; i++) {
    if (!ReadFile(file1, buf1, COMPARE_BLOCKSIZE, &readAmount, NULL) || 
        readAmount != COMPARE_BLOCKSIZE) {
      return FALSE;
    }

    if (!ReadFile(file2, buf2, COMPARE_BLOCKSIZE, &readAmount, NULL) || 
        readAmount != COMPARE_BLOCKSIZE) {
      return FALSE;
    }

    if (memcmp(buf1, buf2, COMPARE_BLOCKSIZE)) {
      // sameContent is already set to FALSE
      return TRUE;
    }
  }

  if (leftOver) {
    if (!ReadFile(file1, buf1, leftOver, &readAmount, NULL) || 
        readAmount != leftOver) {
      return FALSE;
    }

    if (!ReadFile(file2, buf2, leftOver, &readAmount, NULL) || 
        readAmount != leftOver) {
      return FALSE;
    }

    if (memcmp(buf1, buf2, leftOver)) {
      // sameContent is already set to FALSE
      return TRUE;
    }
  }

  sameContent = TRUE;
  return TRUE;
}
