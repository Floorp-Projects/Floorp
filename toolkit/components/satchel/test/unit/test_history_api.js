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

var testnum = 0;
var fh;

function run_test()
{
  try {

  // ===== test init =====
  var testfile = do_get_file("toolkit/components/satchel/test/unit/formhistory_apitest.sqlite");
  var profileDir = dirSvc.get("ProfD", Ci.nsIFile);

  // Cleanup from any previous tests or failures.
  var destFile = profileDir.clone();
  destFile.append("formhistory.sqlite");
  if (destFile.exists())
    destFile.remove(false);

  testfile.copyTo(profileDir, "formhistory.sqlite");

  fh = Cc["@mozilla.org/satchel/form-history;1"].
       getService(Ci.nsIFormHistory2);


  // ===== 1 =====
  // Check initial state is as expected
  testnum++;
  do_check_true(fh.hasEntries);
  do_check_true(fh.nameExists("name-A"));
  do_check_true(fh.nameExists("name-B"));
  do_check_true(fh.nameExists("name-C"));
  do_check_true(fh.nameExists("name-D"));
  do_check_true(fh.entryExists("name-A", "value-A"));
  do_check_true(fh.entryExists("name-B", "value-B1"));
  do_check_true(fh.entryExists("name-B", "value-B2"));
  do_check_true(fh.entryExists("name-C", "value-C"));
  do_check_true(fh.entryExists("name-D", "value-D"));
  // time-A/B/C/D checked below.

  // ===== 2 =====
  // Test looking for non-existant / bogus data.
  testnum++;
  do_check_false(fh.nameExists("blah"));
  do_check_false(fh.nameExists(""));
  do_check_false(fh.nameExists(null));
  do_check_false(fh.entryExists("name-A", "blah"));
  do_check_false(fh.entryExists("name-A", ""));
  do_check_false(fh.entryExists("name-A", null));
  do_check_false(fh.entryExists("blah", "value-A"));
  do_check_false(fh.entryExists("", "value-A"));
  do_check_false(fh.entryExists(null, "value-A"));

  // ===== 3 =====
  // Test removeEntriesForName with a single matching value
  testnum++;
  fh.removeEntriesForName("name-A");
  do_check_false(fh.entryExists("name-A", "value-A"));
  do_check_true(fh.entryExists("name-B", "value-B1"));
  do_check_true(fh.entryExists("name-B", "value-B2"));
  do_check_true(fh.entryExists("name-C", "value-C"));
  do_check_true(fh.entryExists("name-D", "value-D"));

  // ===== 4 =====
  // Test removeEntriesForName with multiple matching values
  testnum++;
  fh.removeEntriesForName("name-B");
  do_check_false(fh.entryExists("name-A", "value-A"));
  do_check_false(fh.entryExists("name-B", "value-B1"));
  do_check_false(fh.entryExists("name-B", "value-B2"));
  do_check_true(fh.entryExists("name-C", "value-C"));
  do_check_true(fh.entryExists("name-D", "value-D"));

  // ===== 5 =====
  // Test removing by time range (single entry, not surrounding entries)
  testnum++;
  do_check_true(fh.nameExists("time-A")); // firstUsed=1000, lastUsed=1000
  do_check_true(fh.nameExists("time-B")); // firstUsed=1000, lastUsed=1099
  do_check_true(fh.nameExists("time-C")); // firstUsed=1099, lastUsed=1099
  do_check_true(fh.nameExists("time-D")); // firstUsed=2001, lastUsed=2001
  fh.removeEntriesByTimeframe(1050, 2000);
  do_check_true(fh.nameExists("time-A"));
  do_check_true(fh.nameExists("time-B"));
  do_check_false(fh.nameExists("time-C"));
  do_check_true(fh.nameExists("time-D"));

  // ===== 6 =====
  // Test removing by time range (multiple entries)
  testnum++;
  fh.removeEntriesByTimeframe(1000, 2000);
  do_check_false(fh.nameExists("time-A"));
  do_check_false(fh.nameExists("time-B"));
  do_check_false(fh.nameExists("time-C"));
  do_check_true(fh.nameExists("time-D"));

  // ===== 7 =====
  // test removeAllEntries
  testnum++;
  fh.removeAllEntries();
  do_check_false(fh.hasEntries);
  do_check_false(fh.nameExists("name-C"));
  do_check_false(fh.nameExists("name-D"));
  do_check_false(fh.entryExists("name-C", "value-C"));
  do_check_false(fh.entryExists("name-D", "value-D"));

  // ===== 8 =====
  // Add a single entry back
  testnum++;
  fh.addEntry("newname-A", "newvalue-A");
  do_check_true(fh.hasEntries);
  do_check_true(fh.entryExists("newname-A", "newvalue-A"));

  // ===== 9 =====
  // Remove the single entry
  testnum++;
  fh.removeEntry("newname-A", "newvalue-A");
  do_check_false(fh.hasEntries);
  do_check_false(fh.entryExists("newname-A", "newvalue-A"));

  } catch (e) {
    throw "FAILED in test #" + testnum + " -- " + e;
  }
}
