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
 * The Original Code is the Application Update Service.
 *
 * The Initial Developer of the Original Code is
 * Robert Strong <robert.bugzilla@gmail.com>.
 *
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Mozilla Foundation <http://www.mozilla.org/>. All Rights Reserved.
 *
 * Contributor(s):
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
 * ***** END LICENSE BLOCK *****
 */

/* General Update Service Tests */

function run_test() {
  do_test_pending();
  do_register_cleanup(end_test);

  // Verify write access to the custom app dir
  logTestInfo("testing write access to the application directory");
  removeUpdateDirsAndFiles();
  var testFile = getCurrentProcessDir();
  testFile.append("update_write_access_test");
  testFile.create(AUS_Ci.nsIFile.NORMAL_FILE_TYPE, 0644);
  do_check_true(testFile.exists());
  testFile.remove(false);
  do_check_false(testFile.exists());

  standardInit();

  // Check if available updates can be checked for
  logTestInfo("testing nsIApplicationUpdateService:canCheckForUpdates");
  do_check_true(gAUS.canCheckForUpdates);
  // Check if updates can be applied
  logTestInfo("testing nsIApplicationUpdateService:canApplyUpdates");
  do_check_true(gAUS.canApplyUpdates);

  do_test_finished();
}

function end_test() {
  cleanUp();
}
