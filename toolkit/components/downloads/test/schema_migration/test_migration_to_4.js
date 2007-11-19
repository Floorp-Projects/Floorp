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
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com> (Original Author)
 *   Srirang G Doddihal <brahmana@doddihal.com>
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

// This file tests migration from v3 to v4

function run_test()
{
  // First import the downloads.sqlite file
  importDatabaseFile("v3.sqlite");

  // ok, now it is OK to init the download manager - this will perform the
  // migration!
  var dm = Cc["@mozilla.org/download-manager;1"].
           getService(Ci.nsIDownloadManager);
  var dbConn = dm.DBConnection;
  var stmt = null;

  // check schema version
  do_check_true(dbConn.schemaVersion >= 4);

  // Check that the column exists (statement should not throw)
  stmt = dbConn.createStatement("SELECT entityID FROM moz_downloads");

  // now we check the entries
  stmt = dbConn.createStatement(
    "SELECT name, source, target, startTime, endTime, state, referrer, entityID " +
    "FROM moz_downloads " +
    "WHERE id = 27");
  stmt.executeStep();
  do_check_eq("Firefox 2.0.0.6.dmg", stmt.getString(0));
  do_check_eq("http://ftp-mozilla.netscape.com/pub/mozilla.org/firefox/releases/2.0.0.6/mac/en-US/Firefox%202.0.0.6.dmg",
              stmt.getUTF8String(1));
  do_check_eq("file:///Users/sdwilsh/Desktop/Firefox%202.0.0.6.dmg",
              stmt.getUTF8String(2));
  do_check_eq(1187390974170783, stmt.getInt64(3));
  do_check_eq(1187391001257446, stmt.getInt64(4));
  do_check_eq(1, stmt.getInt32(5));
  do_check_eq("http://www.mozilla.com/en-US/products/download.html?product=firefox-2.0.0.6&os=osx&lang=en-US",stmt.getUTF8String(6));
  do_check_true(stmt.getIsNull(7));
  stmt.reset();

  cleanup();
}

