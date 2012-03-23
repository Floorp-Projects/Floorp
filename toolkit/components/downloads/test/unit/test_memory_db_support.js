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
 * Ehsan Akhgari <ehsan.akhgari@gmail.com>.
 * Portions created by the Initial Developer are Copyright (C) 2008
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
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
