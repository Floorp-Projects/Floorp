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

// Returns true if the timestamp is within 30 seconds of now.
function is_about_now(timestamp) {
  var delta = Math.abs(timestamp - 1000 * Date.now());
  var seconds = 30 * 1000000;
  return delta < seconds;
}

var testnum = 0;
var fh;
var timesUsed, firstUsed, lastUsed;

function run_test()
{
  try {

  // ===== test init =====
  var testfile = do_get_file("formhistory_v0v1.sqlite");
  var profileDir = dirSvc.get("ProfD", Ci.nsIFile);

  // Cleanup from any previous tests or failures.
  var destFile = profileDir.clone();
  destFile.append("formhistory.sqlite");
  if (destFile.exists())
    destFile.remove(false);

  testfile.copyTo(profileDir, "formhistory.sqlite");
  do_check_eq(0, getDBVersion(testfile));

  fh = Cc["@mozilla.org/satchel/form-history;1"].
       getService(Ci.nsIFormHistory2);


  // ===== 1 =====
  testnum++;
  // Check for expected contents.
  do_check_true(fh.entryExists("name-A", "value-A"));
  do_check_true(fh.entryExists("name-B", "value-B"));
  do_check_true(fh.entryExists("name-C", "value-C1"));
  do_check_true(fh.entryExists("name-C", "value-C2"));
  do_check_true(fh.entryExists("name-D", "value-D"));
  // check for upgraded schema.
  do_check_eq(CURRENT_SCHEMA, fh.DBConnection.schemaVersion);

  // ===== 2 =====
  testnum++;

  // The name-D entry was added by v0 code, so no timestamps were set. Make
  // sure the upgrade set timestamps on that entry.
  var query = "SELECT timesUsed, firstUsed, lastUsed " +
              "FROM moz_formhistory WHERE fieldname = 'name-D'";
  var stmt = fh.DBConnection.createStatement(query);
  stmt.executeStep();

  timesUsed = stmt.getInt32(0);
  firstUsed = stmt.getInt64(1);
  lastUsed  = stmt.getInt64(2);
  stmt.finalize();

  do_check_eq(1, timesUsed);
  do_check_true(firstUsed == lastUsed);
  // Upgraded entries timestamped 24 hours in the past.
  do_check_true(is_about_now(firstUsed + 24 * PR_HOURS));


  // ===== 3 =====
  testnum++;

  // Check to make sure the existing timestamps are unmodified.
  var query = "SELECT timesUsed, firstUsed, lastUsed " +
              "FROM moz_formhistory WHERE fieldname = 'name-A'";
  var stmt = fh.DBConnection.createStatement(query);
  stmt.executeStep();

  timesUsed = stmt.getInt32(0);
  firstUsed = stmt.getInt64(1);
  lastUsed  = stmt.getInt64(2);
  stmt.finalize();

  do_check_eq(1, timesUsed);
  do_check_eq(lastUsed,  1231984073012182);
  do_check_eq(firstUsed, 1231984073012182);

  } catch (e) {
    throw "FAILED in test #" + testnum + " -- " + e;
  }
}
