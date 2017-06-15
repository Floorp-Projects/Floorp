/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* General Update Manager Tests */

function run_test() {
  setupTestCommon();

  debugDump("testing removal of an active update for a channel that is not" +
            "valid due to switching channels (Bug 486275).");

  let patchProps = {state: STATE_DOWNLOADING};
  let patches = getLocalPatchString(patchProps);
  let updateProps = {appVersion: "1.0"};
  let updates = getLocalUpdateString(updateProps, patches);
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), true);
  writeStatusFile(STATE_DOWNLOADING);

  patchProps = {state: STATE_FAILED};
  patches = getLocalPatchString(patchProps);
  updateProps = {name: "Existing"};
  updates = getLocalUpdateString(updateProps, patches);
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), false);

  setUpdateChannel("original_channel");

  standardInit();

  Assert.equal(gUpdateManager.updateCount, 1,
               "the update manager update count" + MSG_SHOULD_EQUAL);
  let update = gUpdateManager.getUpdateAt(0);
  Assert.equal(update.name, "Existing",
               "the update's name" + MSG_SHOULD_EQUAL);

  Assert.ok(!gUpdateManager.activeUpdate,
            "there should not be an active update");
  // Verify that the active-update.xml file has had the update from the old
  // channel removed.
  let file = getUpdatesXMLFile(true);
  Assert.equal(readFile(file), getLocalUpdatesXMLString(""),
               "the contents of active-update.xml" + MSG_SHOULD_EQUAL);

  doTestFinish();
}
