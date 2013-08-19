/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 sts=2
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
  if (oldDownloadManagerDisabled()) {
    return;
  }

  for (let i = 0; i < tests.length; i++) {
    dm.cleanUp();
    tests[i]();
  }
}
