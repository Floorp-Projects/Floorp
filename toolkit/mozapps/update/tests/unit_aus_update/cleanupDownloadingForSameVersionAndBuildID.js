/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

function run_test() {
  setupTestCommon();

  debugDump("testing removal of an update download in progress for the " +
            "same version of the application with the same application " +
            "build id on startup (Bug 536547)");

  let patchProps = {state: STATE_DOWNLOADING};
  let patches = getLocalPatchString(patchProps);
  let updateProps = {appVersion: "1.0",
                     buildID: "2007010101"};
  let updates = getLocalUpdateString(updateProps, patches);
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), true);
  writeStatusFile(STATE_DOWNLOADING);

  standardInit();

  Assert.ok(!gUpdateManager.activeUpdate,
            "there should not be an active update");
  Assert.equal(gUpdateManager.updateCount, 0,
               "the update manager update count" + MSG_SHOULD_EQUAL);

  doTestFinish();
}
