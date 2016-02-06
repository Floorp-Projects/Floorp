/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var cps = new ContentPrefInstance(null);

function run_test() {
  var uri1 = ContentPrefTest.getURI("http://www.domain1.com/");
  var uri2 = ContentPrefTest.getURI("http://foo.domain1.com/");
  var uri3 = ContentPrefTest.getURI("http://domain1.com/");
  var uri4 = ContentPrefTest.getURI("http://www.domain2.com/");

  cps.setPref(uri1, "one", 1);
  cps.setPref(uri1, "two", 2);
  cps.setPref(uri2, "one", 4);
  cps.setPref(uri3, "three", 8);
  cps.setPref(uri4, "two", 16);

  cps.removePref(uri3, "three"); // uri3 should be removed now
  checkForUnusedGroups();
  checkForUnusedSettings();

  cps.removePrefsByName("two"); // uri4 should be removed now
  checkForUnusedGroups();
  checkForUnusedSettings();

  cps.removeGroupedPrefs();
  checkForUnusedGroups();
  checkForUnusedSettings();
}

function checkForUnusedGroups() {
  var stmt = cps.DBConnection.createStatement(`
               SELECT COUNT(*) AS count FROM groups
               WHERE id NOT IN (SELECT DISTINCT groupID FROM prefs)
             `);
  stmt.executeStep();
  do_check_eq(0, stmt.row.count);
  stmt.reset();
  stmt.finalize();
}

function checkForUnusedSettings() {
  var stmt = cps.DBConnection.createStatement(`
               SELECT COUNT(*) AS count FROM settings
               WHERE id NOT IN (SELECT DISTINCT settingID FROM prefs)
             `);
  stmt.executeStep();
  do_check_eq(0, stmt.row.count);
  stmt.reset();
  stmt.finalize();
}
