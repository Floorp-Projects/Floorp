/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This tests the switching of the download manager database types between disk
// and memory based databases.  This feature was added in bug 457110.

const nsIDownloadManager = Ci.nsIDownloadManager;
const dm = Cc["@mozilla.org/download-manager;1"].getService(nsIDownloadManager);

function run_test() {
  let observer = dm.QueryInterface(Ci.nsIObserver);

  // make sure the initial disk-based DB is initialized correctly
  let connDisk = dm.DBConnection;
  do_check_true(connDisk.connectionReady);
  do_check_neq(connDisk.databaseFile, null);

  // switch to a disk DB -- should be a no-op
  observer.observe(null, "dlmgr-switchdb", "disk");

  // make sure that the database has not changed
  do_check_true(dm.DBConnection.connectionReady);
  do_check_neq(dm.DBConnection.databaseFile, null);
  do_check_true(connDisk.databaseFile.equals(dm.DBConnection.databaseFile));
  connDisk = dm.DBConnection;
  let oldFile = connDisk.databaseFile;

  // switch to a memory DB
  observer.observe(null, "dlmgr-switchdb", "memory");

  // make sure the DB is has been switched correctly
  let connMemory = dm.DBConnection;
  do_check_true(connMemory.connectionReady);
  do_check_eq(connMemory.databaseFile, null);

  // switch to a memory DB -- should be a no-op
  observer.observe(null, "dlmgr-switchdb", "memory");

  // make sure that the database is still memory based
  connMemory = dm.DBConnection;
  do_check_true(connMemory.connectionReady);
  do_check_eq(connMemory.databaseFile, null);

  // switch back to the disk DB
  observer.observe(null, "dlmgr-switchdb", "disk");

  // make sure that the disk database is initialized correctly
  do_check_true(dm.DBConnection.connectionReady);
  do_check_neq(dm.DBConnection.databaseFile, null);
  do_check_true(oldFile.equals(dm.DBConnection.databaseFile));
}
