/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
                                 nullptr, OPEN_EXISTING, 0, nullptr));
  if (INVALID_HANDLE_VALUE == file1) {
    return FALSE;
  }
  nsAutoHandle file2(CreateFileW(file2Path, GENERIC_READ, FILE_SHARE_READ, 
                                 nullptr, OPEN_EXISTING, 0, nullptr));
  if (INVALID_HANDLE_VALUE == file2) {
    return FALSE;
  }

  DWORD fileSize1 = GetFileSize(file1, nullptr);
  DWORD fileSize2 = GetFileSize(file2, nullptr);
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
    if (!ReadFile(file1, buf1, COMPARE_BLOCKSIZE, &readAmount, nullptr) ||
        readAmount != COMPARE_BLOCKSIZE) {
      return FALSE;
    }

    if (!ReadFile(file2, buf2, COMPARE_BLOCKSIZE, &readAmount, nullptr) ||
        readAmount != COMPARE_BLOCKSIZE) {
      return FALSE;
    }

    if (memcmp(buf1, buf2, COMPARE_BLOCKSIZE)) {
      // sameContent is already set to FALSE
      return TRUE;
    }
  }

  if (leftOver) {
    if (!ReadFile(file1, buf1, leftOver, &readAmount, nullptr) ||
        readAmount != leftOver) {
      return FALSE;
    }

    if (!ReadFile(file2, buf2, leftOver, &readAmount, nullptr) ||
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
