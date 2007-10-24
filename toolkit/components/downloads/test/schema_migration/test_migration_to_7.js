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
 * The Original Code is Download Manager Test Code.
 *
 * The Initial Developer of the Original Code is
 * Edward Lee <edward.lee@engineering.uiuc.edu>.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
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
 * decision by declaring the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not declare
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

function run_test()
{
  // We're testing migration to this version from one version below
  var targetVersion = 7;

  // First import the downloads.sqlite file
  importDatabaseFile("v" + (targetVersion - 1) + ".sqlite");

  // Init the download manager which will try migrating to the new version
  var dm = Cc["@mozilla.org/download-manager;1"].
           getService(Ci.nsIDownloadManager);
  var dbConn = dm.DBConnection;

  // Check schema version
  do_check_true(dbConn.schemaVersion >= targetVersion);

  // Make sure all the columns are there
  var stmt = dbConn.createStatement(
    "SELECT name, source, target, tempPath, startTime, endTime, state, " +
           "referrer, entityID, currBytes, maxBytes, mimeType, " +
           "preferredApplication, preferredAction " +
    "FROM moz_downloads " +
    "WHERE id = 28");
  stmt.executeStep();

  // This data is based on the original values in the table
  var data = [
    "firefox-3.0a9pre.en-US.linux-i686.tar.bz2",
    "http://ftp.mozilla.org/pub/mozilla.org/firefox/nightly/latest-trunk/firefox-3.0a9pre.en-US.linux-i686.tar.bz2",
    "file:///Users/Ed/Desktop/firefox-3.0a9pre.en-US.linux-i686.tar.bz2",
    "/Users/Ed/Desktop/+EZWafFQ.bz2.part",
    1192469856209164,
    1192469877017396,
    4,
    "http://ftp.mozilla.org/pub/mozilla.org/firefox/nightly/latest-trunk/",
    "%2210e66c1-8a2d6b-9b33f380%22/9055595/Mon, 15 Oct 2007 11:45:34 GMT",
    1210772,
    9055595,
    // For the new columns added, check for null or default values
    true,
    true,
    0,
  ];

  // Make sure the values are correct after the migration
  var i = 0;
  do_check_eq(data[i], stmt.getString(i++));
  do_check_eq(data[i], stmt.getUTF8String(i++));
  do_check_eq(data[i], stmt.getUTF8String(i++));
  do_check_eq(data[i], stmt.getString(i++));
  do_check_eq(data[i], stmt.getInt64(i++));
  do_check_eq(data[i], stmt.getInt64(i++));
  do_check_eq(data[i], stmt.getInt32(i++));
  do_check_eq(data[i], stmt.getUTF8String(i++));
  do_check_eq(data[i], stmt.getUTF8String(i++));
  do_check_eq(data[i], stmt.getInt64(i++));
  do_check_eq(data[i], stmt.getInt64(i++));
  do_check_eq(data[i], stmt.getIsNull(i++));
  do_check_eq(data[i], stmt.getIsNull(i++));
  do_check_eq(data[i], stmt.getInt32(i++));

  stmt.reset();
  stmt.finalize();

  cleanup();
}
