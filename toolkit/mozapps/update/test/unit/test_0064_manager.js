/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* General Update Manager Tests */

function run_test() {
  do_test_pending();
  do_register_cleanup(end_test);

  logTestInfo("testing removal of an update download in progress for the " +
              "same version of the application with the same application " +
              "build id on startup (bug 536547)");
  removeUpdateDirsAndFiles();
  setUpdateChannel();

  var patches, updates;

  patches = getLocalPatchString(null, null, null, null, null, null,
                                STATE_DOWNLOADING);
  updates = getLocalUpdateString(patches, null, null, "version 1.0", "1.0", null,
                                 "2007010101");
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), true);
  writeStatusFile(STATE_DOWNLOADING);

  writeUpdatesToXMLFile(getLocalUpdatesXMLString(""), false);

  standardInit();

  do_check_eq(gUpdateManager.activeUpdate, null);
  do_check_eq(gUpdateManager.updateCount, 0);

  do_test_finished();
}

function end_test() {
  cleanUp();
}
