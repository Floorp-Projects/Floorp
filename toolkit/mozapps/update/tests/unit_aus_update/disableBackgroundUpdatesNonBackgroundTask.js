/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

/**
 * This test verifies that when Balrog advertises that an update should not
 * be downloaded in the background, but we are not running in the background,
 * the advertisement does not have any effect.
 */

function setup() {
  setupTestCommon();
  start_httpserver();
  setUpdateURL(gURLData + gHTTPHandlerPath);
  setUpdateChannel("test_channel");
}
setup();

add_task(async function disableBackgroundUpdatesBackgroundTask() {
  let patches = getRemotePatchString({});
  let updateString = getRemoteUpdateString(
    { disableBackgroundUpdates: "true" },
    patches
  );
  gResponseBody = getRemoteUpdatesXMLString(updateString);

  let { updates } = await waitForUpdateCheck(true);

  initMockIncrementalDownload();
  gIncrementalDownloadErrorType = 3;

  // This will assert that the download completes successfully.
  await waitForUpdateDownload(updates, Cr.NS_OK);
});

add_task(async function finish() {
  stop_httpserver(doTestFinish);
});
