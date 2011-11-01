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
 * The Original Code is Satchel Test Code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Justin Dolske <dolske@mozilla.com> (Original Author)
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

function run_test()
{
  try {
  var testnum = 0;

  // ===== test init =====
  var testfile = do_get_file("formhistory_CORRUPT.sqlite");
  var profileDir = dirSvc.get("ProfD", Ci.nsIFile);

  // Cleanup from any previous tests or failures.
  var destFile = profileDir.clone();
  destFile.append("formhistory.sqlite");
  if (destFile.exists())
    destFile.remove(false);

  var bakFile = profileDir.clone();
  bakFile.append("formhistory.sqlite.corrupt");
  if (bakFile.exists())
    bakFile.remove(false);

  testfile.copyTo(profileDir, "formhistory.sqlite");

  // ===== 1 =====
  testnum++;
  // Open the DB, ensure that a backup of the corrupt DB is made.
  do_check_false(bakFile.exists());
  var fh = Cc["@mozilla.org/satchel/form-history;1"].
           getService(Ci.nsIFormHistory2);
  // DB init is done lazily so the DB shouldn't be created yet.
  do_check_false(bakFile.exists());
  // Doing any request to the DB should create it.
  fh.DBConnection;
  do_check_true(bakFile.exists());
  bakFile.remove(false);

  // ===== 2 =====
  testnum++;
  // File should be empty
  do_check_false(fh.hasEntries);
  do_check_false(fh.entryExists("name-A", "value-A"));


  // ===== 3 =====
  testnum++;
  // Try adding an entry
  fh.addEntry("name-A", "value-A");
  do_check_true(fh.hasEntries);
  do_check_true(fh.entryExists("name-A", "value-A"));


  // ===== 4 =====
  testnum++;
  // Try removing an entry
  fh.removeEntry("name-A", "value-A");
  do_check_false(fh.hasEntries);
  do_check_false(fh.entryExists("name-A", "value-A"));


  } catch (e) {
    throw "FAILED in test #" + testnum + " -- " + e;
  }
}
