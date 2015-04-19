/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */


function run_test() {
  setupTestCommon();

  debugDump("testing cleanup of an update download in progress for an " +
            "older version of the application on startup (Bug 485624)");

  let patches = getLocalPatchString(null, null, null, null, null, null,
                                    STATE_DOWNLOADING);
  let updates = getLocalUpdateString(patches, null, null, "version 0.9", "0.9");
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), true);
  writeStatusFile(STATE_DOWNLOADING);

  writeUpdatesToXMLFile(getLocalUpdatesXMLString(""), false);

  standardInit();

  if (IS_TOOLKIT_GONK) {
    // Gonk doesn't resume downloads at boot time, so the update
    // will remain active until the user chooses a new one, at
    // which point, the old update will be removed.
    Assert.ok(!!gUpdateManager.activeUpdate,
              "there should be an active update");
  } else {
    Assert.ok(!gUpdateManager.activeUpdate,
              "there should not be an active update");
  }
  Assert.equal(gUpdateManager.updateCount, 0,
               "the update manager update count" + MSG_SHOULD_EQUAL);

  doTestFinish();
}
