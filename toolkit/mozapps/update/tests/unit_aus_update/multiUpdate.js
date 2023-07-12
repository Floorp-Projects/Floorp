/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
 * This tests the multiple update downloads per Firefox session feature.
 *
 * This test does some unusual things, compared to the other files in this
 * directory. We want to start the updates with aus.checkForBackgroundUpdates()
 * to ensure that we test the whole flow. Other tests start update with things
 * like aus.downloadUpdate(), but that bypasses some of the exact checks that we
 * are trying to test as part of the multiupdate flow.
 *
 * In order to accomplish all this, we will be using app_update.sjs to serve
 * updates XMLs and MARs. Outside of this test, this is really only done
 * by browser-chrome mochitests (in ../browser). So we have to do some weird
 * things to make it work properly in an xpcshell test. Things like
 * defining URL_HTTP_UPDATE_SJS in testConstants.js so that it can be read by
 * app_update.sjs in order to provide the correct download URL for MARs, but
 * not reading that file here, because URL_HTTP_UPDATE_SJS is already defined
 * (as something else) in xpcshellUtilsAUS.js.
 */

let { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

// These are from testConstants.js, which cannot be loaded by this file, because
// some values are already defined at this point. However, we need these some
// other values to be defined because continueFileHandler in shared.js expects
// them to be.
const REL_PATH_DATA = "";
// This should be URL_HOST, but that conflicts with an existing constant.
const APP_UPDATE_SJS_HOST = "http://127.0.0.1:8888";
const URL_PATH_UPDATE_XML = "/" + REL_PATH_DATA + "app_update.sjs";
// This should be URL_HTTP_UPDATE_SJS, but that conflicts with an existing
// constant.
const APP_UPDATE_SJS_URL = APP_UPDATE_SJS_HOST + URL_PATH_UPDATE_XML;
const CONTINUE_CHECK = "continueCheck";
const CONTINUE_DOWNLOAD = "continueDownload";
const CONTINUE_STAGING = "continueStaging";

const FIRST_UPDATE_VERSION = "999998.0";
const SECOND_UPDATE_VERSION = "999999.0";

/**
 * Downloads an update via aus.checkForBackgroundUpdates()
 * Function returns only after the update has been downloaded.
 *
 * The provided callback will be invoked once during the update download,
 * specifically when onStartRequest is fired.
 *
 * If automatic update downloads are turned off (appUpdateAuto is false), then
 * we listen for the update-available notification and then roughly simulate
 * accepting the prompt by calling:
 *     AppUpdateService.downloadUpdate(update, true);
 * This is what is normally called when the user accepts the update-available
 * prompt.
 */
async function downloadUpdate(appUpdateAuto, onDownloadStartCallback) {
  let downloadFinishedPromise = waitForEvent("update-downloaded");
  let updateAvailablePromise;
  if (!appUpdateAuto) {
    updateAvailablePromise = new Promise(resolve => {
      let observer = (subject, topic, status) => {
        Services.obs.removeObserver(observer, "update-available");
        subject.QueryInterface(Ci.nsIUpdate);
        resolve({ update: subject, status });
      };
      Services.obs.addObserver(observer, "update-available");
    });
  }
  let waitToStartPromise = new Promise(resolve => {
    let listener = {
      onStartRequest: aRequest => {
        gAUS.removeDownloadListener(listener);
        onDownloadStartCallback();
        resolve();
      },
      onProgress: (aRequest, aContext, aProgress, aMaxProgress) => {},
      onStatus: (aRequest, aStatus, aStatusText) => {},
      onStopRequest(request, status) {},
      QueryInterface: ChromeUtils.generateQI([
        "nsIRequestObserver",
        "nsIProgressEventSink",
      ]),
    };
    gAUS.addDownloadListener(listener);
  });

  let updateCheckStarted = gAUS.checkForBackgroundUpdates();
  Assert.ok(updateCheckStarted, "Update check should have started");

  if (!appUpdateAuto) {
    let { update, status } = await updateAvailablePromise;
    Assert.equal(
      status,
      "show-prompt",
      "Should attempt to show the update-available prompt"
    );
    // Simulate accepting the update-available prompt
    await gAUS.downloadUpdate(update, true);
  }

  await continueFileHandler(CONTINUE_DOWNLOAD);
  await waitToStartPromise;
  await downloadFinishedPromise;
  // Wait an extra tick after the download has finished. If we try to check for
  // another update exactly when "update-downloaded" fires,
  // Downloader:onStopRequest won't have finished yet, which it normally would
  // have.
  await TestUtils.waitForTick();
}

/**
 * This is like downloadUpdate. The difference is that downloadUpdate assumes
 * that an update actually will be downloaded. This function instead verifies
 * that we the update wasn't downloaded.
 *
 * downloadUpdate(), above, uses aus.checkForBackgroundUpdates() to download
 * updates to verify that background updates actually will check for subsequent
 * updates, but this function will use some slightly different mechanisms. We
 * can call aus.downloadUpdate() and check its return value to see if it started
 * a download. But this doesn't properly check that the update-available
 * notification isn't shown. So we will do an additional check where we follow
 * the normal flow a bit more closely by forwarding the results that we got from
 * checkForUpdates() to aus.onCheckComplete() and make sure that the update
 * prompt isn't shown.
 */
async function testUpdateDoesNotDownload() {
  let check = gUpdateChecker.checkForUpdates(gUpdateChecker.BACKGROUND_CHECK);
  let result = await check.result;
  Assert.ok(result.checksAllowed, "Should be able to check for updates");
  Assert.ok(result.succeeded, "Update check should have succeeded");

  Assert.equal(
    result.updates.length,
    1,
    "Should have gotten 1 update in update check"
  );
  let update = result.updates[0];

  let downloadStarted = await gAUS.downloadUpdate(update, true);
  Assert.equal(
    downloadStarted,
    false,
    "Expected that we would not start downloading an update"
  );

  let updateAvailableObserved = false;
  let observer = (subject, topic, status) => {
    updateAvailableObserved = true;
  };
  Services.obs.addObserver(observer, "update-available");
  await gAUS.onCheckComplete(result);
  Services.obs.removeObserver(observer, "update-available");
  Assert.equal(
    updateAvailableObserved,
    false,
    "update-available notification should not fire if we aren't going to " +
      "download the update."
  );
}

function testUpdateCheckDoesNotStart() {
  let updateCheckStarted = gAUS.checkForBackgroundUpdates();
  Assert.equal(
    updateCheckStarted,
    false,
    "Update check should not have started"
  );
}

function prepareToDownloadVersion(version, onlyCompleteMar = false) {
  let updateUrl = `${APP_UPDATE_SJS_URL}?useSlowDownloadMar=1&appVersion=${version}`;
  if (onlyCompleteMar) {
    updateUrl += "&completePatchOnly=1";
  }
  setUpdateURL(updateUrl);
}

function startUpdateServer() {
  let httpServer = new HttpServer();
  httpServer.registerContentType("sjs", "sjs");
  httpServer.registerDirectory("/", do_get_cwd());
  httpServer.start(8888);
  registerCleanupFunction(async function cleanup_httpServer() {
    await new Promise(resolve => {
      httpServer.stop(resolve);
    });
  });
}

async function multi_update_test(appUpdateAuto) {
  await UpdateUtils.setAppUpdateAutoEnabled(appUpdateAuto);

  prepareToDownloadVersion(FIRST_UPDATE_VERSION);

  await downloadUpdate(appUpdateAuto, () => {
    Assert.ok(
      !gUpdateManager.readyUpdate,
      "There should not be a ready update yet"
    );
    Assert.ok(
      !!gUpdateManager.downloadingUpdate,
      "First update download should be in downloadingUpdate"
    );
    Assert.equal(
      gUpdateManager.downloadingUpdate.state,
      STATE_DOWNLOADING,
      "downloadingUpdate should be downloading"
    );
    Assert.equal(
      readStatusFile(),
      STATE_DOWNLOADING,
      "Updater state should be downloading"
    );
  });

  Assert.ok(
    !gUpdateManager.downloadingUpdate,
    "First update download should no longer be in downloadingUpdate"
  );
  Assert.ok(
    !!gUpdateManager.readyUpdate,
    "First update download should be in readyUpdate"
  );
  Assert.equal(
    gUpdateManager.readyUpdate.state,
    STATE_PENDING,
    "readyUpdate should be pending"
  );
  Assert.equal(
    gUpdateManager.readyUpdate.appVersion,
    FIRST_UPDATE_VERSION,
    "readyUpdate version should be match the version of the first update"
  );
  Assert.equal(
    readStatusFile(),
    STATE_PENDING,
    "Updater state should be pending"
  );

  let existingUpdate = gUpdateManager.readyUpdate;
  await testUpdateDoesNotDownload();

  Assert.equal(
    gUpdateManager.readyUpdate,
    existingUpdate,
    "readyUpdate should not have changed when no newer update is available"
  );
  Assert.equal(
    gUpdateManager.readyUpdate.state,
    STATE_PENDING,
    "readyUpdate should still be pending"
  );
  Assert.equal(
    gUpdateManager.readyUpdate.appVersion,
    FIRST_UPDATE_VERSION,
    "readyUpdate version should be match the version of the first update"
  );
  Assert.equal(
    readStatusFile(),
    STATE_PENDING,
    "Updater state should still be pending"
  );

  // With only a complete update available, we should not download the newer
  // update when we already have an update ready.
  prepareToDownloadVersion(SECOND_UPDATE_VERSION, true);
  await testUpdateDoesNotDownload();

  Assert.equal(
    gUpdateManager.readyUpdate,
    existingUpdate,
    "readyUpdate should not have changed when no newer partial update is available"
  );
  Assert.equal(
    gUpdateManager.readyUpdate.state,
    STATE_PENDING,
    "readyUpdate should still be pending"
  );
  Assert.equal(
    gUpdateManager.readyUpdate.appVersion,
    FIRST_UPDATE_VERSION,
    "readyUpdate version should be match the version of the first update"
  );
  Assert.equal(
    readStatusFile(),
    STATE_PENDING,
    "Updater state should still be pending"
  );

  prepareToDownloadVersion(SECOND_UPDATE_VERSION);

  await downloadUpdate(appUpdateAuto, () => {
    Assert.ok(
      !!gUpdateManager.downloadingUpdate,
      "Second update download should be in downloadingUpdate"
    );
    Assert.equal(
      gUpdateManager.downloadingUpdate.state,
      STATE_DOWNLOADING,
      "downloadingUpdate should be downloading"
    );
    Assert.ok(
      !!gUpdateManager.readyUpdate,
      "First update download should still be in readyUpdate"
    );
    Assert.equal(
      gUpdateManager.readyUpdate.state,
      STATE_PENDING,
      "readyUpdate should still be pending"
    );
    Assert.equal(
      gUpdateManager.readyUpdate.appVersion,
      FIRST_UPDATE_VERSION,
      "readyUpdate version should be match the version of the first update"
    );
    Assert.equal(
      readStatusFile(),
      STATE_PENDING,
      "Updater state should match the readyUpdate's state"
    );
  });

  Assert.ok(
    !gUpdateManager.downloadingUpdate,
    "Second update download should no longer be in downloadingUpdate"
  );
  Assert.ok(
    !!gUpdateManager.readyUpdate,
    "Second update download should be in readyUpdate"
  );
  Assert.equal(
    gUpdateManager.readyUpdate.state,
    STATE_PENDING,
    "readyUpdate should be pending"
  );
  Assert.equal(
    gUpdateManager.readyUpdate.appVersion,
    SECOND_UPDATE_VERSION,
    "readyUpdate version should be match the version of the second update"
  );
  Assert.equal(
    readStatusFile(),
    STATE_PENDING,
    "Updater state should be pending"
  );

  // Reset the updater to its initial state to test that the complete/partial
  // MAR behavior is correct
  reloadUpdateManagerData(true);

  // Second parameter forces a complete MAR download.
  prepareToDownloadVersion(FIRST_UPDATE_VERSION, true);

  await downloadUpdate(appUpdateAuto, () => {
    Assert.equal(
      gUpdateManager.downloadingUpdate.selectedPatch.type,
      "complete",
      "First update download should be a complete patch"
    );
  });

  Assert.equal(
    gUpdateManager.readyUpdate.selectedPatch.type,
    "complete",
    "First update download should be a complete patch"
  );

  // Even a newer partial update should not be downloaded at this point.
  prepareToDownloadVersion(SECOND_UPDATE_VERSION);
  testUpdateCheckDoesNotStart();
}

add_task(async function all_multi_update_tests() {
  setupTestCommon(true);
  startUpdateServer();

  Services.prefs.setBoolPref(PREF_APP_UPDATE_DISABLEDFORTESTING, false);
  Services.prefs.setBoolPref(PREF_APP_UPDATE_STAGING_ENABLED, false);

  let origAppUpdateAutoVal = await UpdateUtils.getAppUpdateAutoEnabled();
  registerCleanupFunction(async () => {
    await UpdateUtils.setAppUpdateAutoEnabled(origAppUpdateAutoVal);
  });

  await multi_update_test(true);

  // Reset the update system so we can start again from scratch.
  reloadUpdateManagerData(true);

  await multi_update_test(false);

  doTestFinish();
});
