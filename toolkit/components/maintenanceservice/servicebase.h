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

#include <windows.h>
#include "updatelogging.h"

BOOL PathAppendSafe(LPWSTR base, LPCWSTR extra);
BOOL VerifySameFiles(LPCWSTR file1Path, LPCWSTR file2Path, BOOL &sameContent);

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
