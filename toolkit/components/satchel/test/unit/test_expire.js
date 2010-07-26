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
 * Portions created by the Initial Developer are Copyright (C) 2009
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
var fh, prefs;

function countAllEntries() {
    let stmt = fh.DBConnection.createStatement("SELECT COUNT(*) as numEntries FROM moz_formhistory");
    do_check_true(stmt.executeStep());
    let numEntries = stmt.row.numEntries;
    stmt.finalize();
    return numEntries;
}

function triggerExpiration() {
    // We can't easily fake a "daily idle" event, so for testing purposes form
    // history listens for another notification to trigger an immediate
    // expiration.
    var os = Cc["@mozilla.org/observer-service;1"].
             getService(Ci.nsIObserverService);
    os.notifyObservers(null, "formhistory-expire-now", null);
}

function run_test()
{
  try {

  // ===== test init =====
  var testfile = do_get_file("formhistory_expire.sqlite");
  var profileDir = dirSvc.get("ProfD", Ci.nsIFile);

  // Cleanup from any previous tests or failures.
  var dbFile = profileDir.clone();
  dbFile.append("formhistory.sqlite");
  if (dbFile.exists())
    dbFile.remove(false);

  testfile.copyTo(profileDir, "formhistory.sqlite");
  do_check_true(dbFile.exists());

  fh = Cc["@mozilla.org/satchel/form-history;1"].
       getService(Ci.nsIFormHistory2);

  prefs = Cc["@mozilla.org/preferences-service;1"].
          getService(Ci.nsIPrefBranch);

  // We're going to clear this at the end, so it better have the default value now.
  do_check_false(prefs.prefHasUserValue("browser.formfill.expire_days"));


  // ===== 1 =====
  testnum++;

  // Sanity check initial state
  do_check_eq(CURRENT_SCHEMA, fh.DBConnection.schemaVersion);
  do_check_eq(508, countAllEntries());
  do_check_true(fh.entryExists("name-A", "value-A")); // lastUsed == distant past
  do_check_true(fh.entryExists("name-B", "value-B")); // lastUsed == distant future

  // Add a new entry
  do_check_false(fh.entryExists("name-C", "value-C"));
  fh.addEntry("name-C", "value-C");
  do_check_true(fh.entryExists("name-C", "value-C"));

  // Check the original db size.
  // Do a vacuum to make sure the db has current page size.
  fh.DBConnection.executeSimpleSQL("VACUUM");
  var oldSize = dbFile.clone().fileSize;

  // Update some existing entries to have ages relative to when the test runs.
  var now = 1000 * Date.now();
  var age181 = now - 181 * 24 * PR_HOURS;
  var age179 = now - 179 * 24 * PR_HOURS;
  var age31  = now -  31 * 24 * PR_HOURS;
  var age29  = now -  29 * 24 * PR_HOURS;
  var age11  = now -  11 * 24 * PR_HOURS;
  var age9   = now -   9 * 24 * PR_HOURS;

  fh.DBConnection.executeSimpleSQL("UPDATE moz_formhistory SET lastUsed=" + age181 + " WHERE lastUsed=181");
  fh.DBConnection.executeSimpleSQL("UPDATE moz_formhistory SET lastUsed=" + age179 + " WHERE lastUsed=179");
  fh.DBConnection.executeSimpleSQL("UPDATE moz_formhistory SET lastUsed=" + age31  + " WHERE lastUsed=31");
  fh.DBConnection.executeSimpleSQL("UPDATE moz_formhistory SET lastUsed=" + age29  + " WHERE lastUsed=29");
  fh.DBConnection.executeSimpleSQL("UPDATE moz_formhistory SET lastUsed=" + age11  + " WHERE lastUsed=9999");
  fh.DBConnection.executeSimpleSQL("UPDATE moz_formhistory SET lastUsed=" + age9   + " WHERE lastUsed=9");


  // ===== 2 =====
  testnum++;

  // Expire history with default pref (180 days)
  do_check_true(fh.entryExists("name-A", "value-A"));
  do_check_true(fh.entryExists("181DaysOld", "foo"));
  do_check_true(fh.entryExists("179DaysOld", "foo"));
  do_check_eq(509, countAllEntries());

  // 2 entries are expected to expire.
  triggerExpiration();

  do_check_false(fh.entryExists("name-A", "value-A"));
  do_check_false(fh.entryExists("181DaysOld", "foo"));
  do_check_true(fh.entryExists("179DaysOld", "foo"));
  do_check_eq(507, countAllEntries());


  // ===== 3 =====
  testnum++;

  // And again. No change expected.
  triggerExpiration();
  do_check_eq(507, countAllEntries());


  // ===== 4 =====
  testnum++;

  // Set formfill pref to 30 days.
  prefs.setIntPref("browser.formfill.expire_days", 30);
  do_check_true(fh.entryExists("179DaysOld", "foo"));
  do_check_true(fh.entryExists("bar", "31days"));
  do_check_true(fh.entryExists("bar", "29days"));
  do_check_eq(507, countAllEntries());

  triggerExpiration();

  do_check_false(fh.entryExists("179DaysOld", "foo"));
  do_check_false(fh.entryExists("bar", "31days"));
  do_check_true(fh.entryExists("bar", "29days"));
  do_check_eq(505, countAllEntries());


  // ===== 5 =====
  testnum++;

  // Set override pref to 10 days and expire. This expires a large batch of
  // entries, and should trigger a VACCUM to reduce file size.
  prefs.setIntPref("browser.formfill.expire_days", 10);

  do_check_true(fh.entryExists("bar", "29days"));
  do_check_true(fh.entryExists("9DaysOld", "foo"));
  do_check_eq(505, countAllEntries());

  triggerExpiration();

  do_check_false(fh.entryExists("bar", "29days"));
  do_check_true(fh.entryExists("9DaysOld", "foo"));
  do_check_true(fh.entryExists("name-B", "value-B"));
  do_check_true(fh.entryExists("name-C", "value-C"));
  do_check_eq(3, countAllEntries());

  // Check that the file size was reduced.
  // Need to clone the nsIFile because the size is being cached on Windows.
  dbFile = dbFile.clone();
  do_check_true(dbFile.fileSize < oldSize);


  } catch (e) {
      throw "FAILED in test #" + testnum + " -- " + e;
  } finally {
      // Make sure we always reset prefs.
      if (prefs.prefHasUserValue("browser.formfill.expire_days"))
        prefs.clearUserPref("browser.formfill.expire_days");
  }
}
