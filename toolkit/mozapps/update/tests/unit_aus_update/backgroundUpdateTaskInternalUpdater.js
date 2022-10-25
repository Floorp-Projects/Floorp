/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

/**
 * This test ensures that we don't resume an update download with the internal
 * downloader when we are running background updates. Normally, the background
 * update task won't even run if we can't use BITS. But it is possible for us to
 * fall back from BITS to the internal downloader. Background update should
 * prevent this fallback and just abort.
 *
 * But interactive Firefox allows that fallback. And once the internal
 * download has started, the background update task must leave that download
 * untouched and allow it to finish.
 */

var TEST_MAR_CONTENTS = "Arbitrary MAR contents";

add_task(async function setup() {
  setupTestCommon();
  start_httpserver();
  setUpdateURL(gURLData + gHTTPHandlerPath);
  setUpdateChannel("test_channel");

  // Pretend that this is a background task.
  const bts = Cc["@mozilla.org/backgroundtasks;1"].getService(
    Ci.nsIBackgroundTasks
  );
  bts.overrideBackgroundTaskNameForTesting("test-task");

  // No need for cleanup needed for changing update files. These will be cleaned
  // up by removeUpdateFiles.
  const downloadingMarFile = getUpdateDirFile(FILE_UPDATE_MAR, DIR_DOWNLOADING);
  await IOUtils.writeUTF8(downloadingMarFile.path, TEST_MAR_CONTENTS);

  writeStatusFile(STATE_DOWNLOADING);

  let patchProps = {
    state: STATE_DOWNLOADING,
    bitsResult: Cr.NS_ERROR_FAILURE,
  };
  let patches = getLocalPatchString(patchProps);
  let updateProps = { appVersion: "1.0" };
  let updates = getLocalUpdateString(updateProps, patches);
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), true);
});

add_task(async function backgroundUpdate() {
  let patches = getRemotePatchString({});
  let updateString = getRemoteUpdateString({}, patches);
  gResponseBody = getRemoteUpdatesXMLString(updateString);

  let { updates } = await waitForUpdateCheck(true);
  let bestUpdate = gAUS.selectUpdate(updates);
  let success = gAUS.downloadUpdate(bestUpdate, false);
  Assert.equal(
    success,
    false,
    "We should not attempt to download an update in the background when an " +
      "internal update download is already in progress."
  );
  Assert.equal(
    readStatusFile(),
    STATE_DOWNLOADING,
    "Background update during an internally downloading update should not " +
      "change update status"
  );
  const downloadingMarFile = getUpdateDirFile(FILE_UPDATE_MAR, DIR_DOWNLOADING);
  Assert.ok(
    await IOUtils.exists(downloadingMarFile.path),
    "Downloading MAR should still exist"
  );
  Assert.equal(
    await IOUtils.readUTF8(downloadingMarFile.path),
    TEST_MAR_CONTENTS,
    "Downloading MAR should not have been modified"
  );
});

add_task(async function finish() {
  stop_httpserver(doTestFinish);
});
