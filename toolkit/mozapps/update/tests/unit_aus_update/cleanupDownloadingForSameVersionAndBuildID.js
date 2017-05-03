/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

function run_test() {
  setupTestCommon();

  debugDump("testing removal of an update download in progress for the " +
            "same version of the application with the same application " +
            "build id on startup (Bug 536547)");

  let patches = getLocalPatchString(null, null, null, null, null, null,
                                    STATE_DOWNLOADING);
  let updates = getLocalUpdateString(patches, null, null, "version 1.0", "1.0",
                                     "2007010101");
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), true);
  writeStatusFile(STATE_DOWNLOADING);

  writeUpdatesToXMLFile(getLocalUpdatesXMLString(""), false);

  standardInit();

  Assert.ok(!gUpdateManager.activeUpdate,
            "there should not be an active update");
  Assert.equal(gUpdateManager.updateCount, 0,
               "the update manager update count" + MSG_SHOULD_EQUAL);

  doTestFinish();
}
