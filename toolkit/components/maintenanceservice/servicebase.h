/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>
#include "updatecommon.h"

BOOL PathAppendSafe(LPWSTR base, LPCWSTR extra);
BOOL VerifySameFiles(LPCWSTR file1Path, LPCWSTR file2Path, BOOL& sameContent);

// 32KiB for comparing files at a time seems reasonable.
// The bigger the better for speed, but this will be used
// on the stack so I don't want it to be too big.
#define COMPARE_BLOCKSIZE 32768

// The following string resource value is used to uniquely identify the signed
// Mozilla application as an updater.  Before the maintenance service will
// execute the updater it must have this updater identity string in its string
// table.  No other signed Mozilla product will have this string table value.
#define UPDATER_IDENTITY_STRING \
  "moz-updater.exe-4cdccec4-5ee0-4a06-9817-4cd899a9db49"
#define IDS_UPDATER_IDENTITY 1006
