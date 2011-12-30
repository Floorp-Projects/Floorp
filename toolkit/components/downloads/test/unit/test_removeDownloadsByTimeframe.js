/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 sts=2
 * ***** BEGIN LICENSE BLOCK *****
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
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com> (Original Author)
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

/**
 * Test added with bug 462964 to test the behavior of the new API that was added
 * to remove downloads by a given time frame.
 */

////////////////////////////////////////////////////////////////////////////////
//// Constants

let dm = Cc["@mozilla.org/download-manager;1"].
         getService(Ci.nsIDownloadManager);

const START_TIME = Date.now() * 1000;
const END_TIME = START_TIME + (60 * 1000000); // one minute range

const DOWNLOAD_FINISHED = Ci.nsIDownloadManager.DOWNLOAD_FINISHED;
const DOWNLOAD_DOWNLOADING = Ci.nsIDownloadManager.DOWNLOAD_DOWNLOADING;

const REMOVED_TOPIC = "download-manager-remove-download";

////////////////////////////////////////////////////////////////////////////////
//// Utility Functions

/**
 * Adds a download to the database.
 *
 * @param aStartTimeInRange
 *        Indicates if the new download's start time should be in range.
 * @param aEndTimeInRange
 *        Indicates if the new download's end time should be in range.
 * @returns the inserted id.
 */
let id = 1;
function add_download_to_db(aStartTimeInRange, aEndTimeInRange, aState)
{
  let db = dm.DBConnection;
  let stmt = db.createStatement(
    "INSERT INTO moz_downloads (id, startTime, endTime, state) " +
    "VALUES (:id, :startTime, :endTime, :state)"
  );
  stmt.params.id = id;
  stmt.params.startTime = aStartTimeInRange ? START_TIME + 1 : START_TIME - 1;
  stmt.params.endTime = aEndTimeInRange ? END_TIME - 1 : END_TIME + 1;
  stmt.params.state = aState;
  stmt.execute();
  stmt.finalize();

  return id++;
}

/**
 * Checks to see the downloads with the specified id exist or not.
 *
 * @param aIDs
 *        The ids of the downloads to check.
 * @param aExpected
 *        True if we expect the download to exist, false if we do not.
 */
function check_existence(aIDs, aExpected)
{
  let db = dm.DBConnection;
  let stmt = db.createStatement(
    "SELECT * " +
    "FROM moz_downloads " +
    "WHERE id = :id"
  );

  let checkFunc = aExpected ? do_check_true : do_check_false;
  for (let i = 0; i < aIDs.length; i++) {
    stmt.params.id = aIDs[i];
    checkFunc(stmt.executeStep());
    stmt.reset();
  }
  stmt.finalize();
}

////////////////////////////////////////////////////////////////////////////////
//// Test Functions

function test_download_start_in_range()
{
  let id = add_download_to_db(true, false, DOWNLOAD_FINISHED);
  dm.removeDownloadsByTimeframe(START_TIME, END_TIME);
  check_existence([id], false);
}

function test_download_end_in_range()
{
  let id = add_download_to_db(false, true, DOWNLOAD_FINISHED);
  dm.removeDownloadsByTimeframe(START_TIME, END_TIME);
  check_existence([id], true);
}

function test_multiple_downloads_in_range()
{
  let ids = [];
  ids.push(add_download_to_db(true, false, DOWNLOAD_FINISHED));
  ids.push(add_download_to_db(true, false, DOWNLOAD_FINISHED));
  dm.removeDownloadsByTimeframe(START_TIME, END_TIME);
  check_existence(ids, false);
}

function test_no_downloads_in_range()
{
  let ids = [];
  ids.push(add_download_to_db(false, true, DOWNLOAD_FINISHED));
  ids.push(add_download_to_db(false, true, DOWNLOAD_FINISHED));
  dm.removeDownloadsByTimeframe(START_TIME, END_TIME);
  check_existence(ids, true);
}

function test_active_download_in_range()
{
  let id = add_download_to_db(true, false, DOWNLOAD_DOWNLOADING);
  dm.removeDownloadsByTimeframe(START_TIME, END_TIME);
  check_existence([id], true);
}

function test_observer_dispatched()
{
  let observer = {
    notified: false,
    observe: function(aSubject, aTopic, aData)
    {
      this.notified = true;
      do_check_eq(aSubject, null);
      do_check_eq(aTopic, REMOVED_TOPIC);
      do_check_eq(aData, null);
    }
  };
  let os = Cc["@mozilla.org/observer-service;1"].
           getService(Ci.nsIObserverService);
  os.addObserver(observer, REMOVED_TOPIC, false);

  add_download_to_db(true, false, DOWNLOAD_FINISHED);
  dm.removeDownloadsByTimeframe(START_TIME, END_TIME);
  do_check_true(observer.notified);

  os.removeObserver(observer, REMOVED_TOPIC);
}

let tests = [
  test_download_start_in_range,
  test_download_end_in_range,
  test_multiple_downloads_in_range,
  test_no_downloads_in_range,
  test_active_download_in_range,
  test_observer_dispatched,
];

function run_test()
{
  for (let i = 0; i < tests.length; i++) {
    dm.cleanUp();
    tests[i]();
  }
}
