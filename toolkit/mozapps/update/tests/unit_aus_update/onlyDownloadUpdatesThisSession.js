/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

function setup() {
  setupTestCommon();
  start_httpserver();
  setUpdateURL(gURLData + gHTTPHandlerPath);
  setUpdateChannel("test_channel");
}
setup();

/**
 * Checks for updates and makes sure that the update process does not proceed
 * beyond the downloading stage.
 */
async function downloadUpdate() {
  let patches = getRemotePatchString({});
  let updateString = getRemoteUpdateString({}, patches);
  gResponseBody = getRemoteUpdatesXMLString(updateString);

  let { updates } = await waitForUpdateCheck(true);

  initMockIncrementalDownload();
  gIncrementalDownloadErrorType = 3;

  let downloadRestrictionHitPromise = new Promise(resolve => {
    let downloadRestrictionHitListener = (subject, topic) => {
      Services.obs.removeObserver(downloadRestrictionHitListener, topic);
      resolve();
    };
    Services.obs.addObserver(
      downloadRestrictionHitListener,
      "update-download-restriction-hit"
    );
  });

  let bestUpdate = await gAUS.selectUpdate(updates);
  let result = await gAUS.downloadUpdate(bestUpdate, false);
  Assert.equal(
    result,
    Ci.nsIApplicationUpdateService.DOWNLOAD_SUCCESS,
    "Update download should have started"
  );
  return downloadRestrictionHitPromise;
}

add_task(async function onlyDownloadUpdatesThisSession() {
  gAUS.onlyDownloadUpdatesThisSession = true;

  await downloadUpdate();

  Assert.ok(
    !(await gUpdateManager.getReadyUpdate()),
    "There should not be a ready update. The update should still be downloading"
  );
  const downloadingUpdate = await gUpdateManager.getDownloadingUpdate();
  Assert.ok(!!downloadingUpdate, "A downloading update should exist");
  Assert.equal(
    downloadingUpdate.state,
    STATE_DOWNLOADING,
    "The downloading update should still be in the downloading state"
  );
});

add_task(async function finish() {
  stop_httpserver(doTestFinish);
});
