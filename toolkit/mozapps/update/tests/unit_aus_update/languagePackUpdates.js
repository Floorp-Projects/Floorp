const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);
const { getAppInfo } = ChromeUtils.import(
  "resource://testing-common/AppInfo.jsm"
);
const { XPIInstall } = ChromeUtils.import(
  "resource://gre/modules/addons/XPIInstall.jsm"
);
const { PromiseUtils } = ChromeUtils.import(
  "resource://gre/modules/PromiseUtils.jsm"
);
const { setTimeout } = ChromeUtils.import("resource://gre/modules/Timer.jsm");

gDebugTest = true;

AddonTestUtils.init(this);
setupTestCommon();
AddonTestUtils.appInfo = getAppInfo();
start_httpserver();
setUpdateURL(gURLData + gHTTPHandlerPath);
setUpdateChannel("test_channel");
Services.prefs.setBoolPref(PREF_APP_UPDATE_LANGPACK_ENABLED, true);

/**
 * Checks for updates and waits for the update to download.
 */
async function downloadUpdate() {
  let patches = getRemotePatchString({});
  let updateString = getRemoteUpdateString({}, patches);
  gResponseBody = getRemoteUpdatesXMLString(updateString);

  let { updates } = await waitForUpdateCheck(true);

  initMockIncrementalDownload();
  gIncrementalDownloadErrorType = 3;

  await waitForUpdateDownload(updates, Cr.NS_OK);
}

/**
 * Returns a promise that will resolve when the add-ons manager attempts to
 * stage langpack updates. The returned object contains the appVersion and
 * platformVersion parameters as well as resolve and reject functions to
 * complete the mocked langpack update.
 */
function mockLangpackUpdate() {
  let stagingCall = PromiseUtils.defer();
  XPIInstall.stageLangpacksForAppUpdate = (appVersion, platformVersion) => {
    let result = PromiseUtils.defer();
    stagingCall.resolve({
      appVersion,
      platformVersion,
      resolve: result.resolve,
      reject: result.reject,
    });

    return result.promise;
  };

  return stagingCall.promise;
}

add_task(async function init() {
  await AddonTestUtils.promiseStartupManager();
});

add_task(async function testLangpackUpdateSuccess() {
  let updateDownloadNotified = false;
  let notified = waitForEvent("update-downloaded").then(
    () => (updateDownloadNotified = true)
  );

  let stagingCall = mockLangpackUpdate();

  await downloadUpdate();

  // We have to wait for UpdateService's onStopRequest to run far enough that
  // the notification will have been sent if the language pack update completed.
  await TestUtils.waitForCondition(() => readStatusFile() == "pending");

  Assert.ok(
    !updateDownloadNotified,
    "Should not have seen the notification yet."
  );

  let { appVersion, platformVersion, resolve } = await stagingCall;
  Assert.equal(
    appVersion,
    DEFAULT_UPDATE_VERSION,
    "Should see the right app version"
  );
  Assert.equal(
    platformVersion,
    DEFAULT_UPDATE_VERSION,
    "Should see the right platform version"
  );

  resolve();

  await notified;
});

add_task(async function testLangpackUpdateFails() {
  let updateDownloadNotified = false;
  let notified = waitForEvent("update-downloaded").then(
    () => (updateDownloadNotified = true)
  );

  let stagingCall = mockLangpackUpdate();

  await downloadUpdate();

  // We have to wait for UpdateService's onStopRequest to run far enough that
  // the notification will have been sent if the language pack update completed.
  await TestUtils.waitForCondition(() => readStatusFile() == "pending");

  Assert.ok(
    !updateDownloadNotified,
    "Should not have seen the notification yet."
  );

  let { appVersion, platformVersion, reject } = await stagingCall;
  Assert.equal(
    appVersion,
    DEFAULT_UPDATE_VERSION,
    "Should see the right app version"
  );
  Assert.equal(
    platformVersion,
    DEFAULT_UPDATE_VERSION,
    "Should see the right platform version"
  );

  reject();

  await notified;
});

add_task(async function testLangpackStaged() {
  let updateStagedNotified = false;
  let notified = waitForEvent("update-staged").then(
    () => (updateStagedNotified = true)
  );

  let stagingCall = mockLangpackUpdate();

  Services.prefs.setBoolPref(PREF_APP_UPDATE_STAGING_ENABLED, true);
  copyTestUpdaterToBinDir();

  let greDir = getGREDir();
  let updateSettingsIni = greDir.clone();
  updateSettingsIni.append(FILE_UPDATE_SETTINGS_INI);
  writeFile(updateSettingsIni, UPDATE_SETTINGS_CONTENTS);

  await downloadUpdate();

  // We have to wait for the update to be applied and then check that the
  // notification hasn't been sent.
  await TestUtils.waitForCondition(() => readStatusFile() == "applied");

  Assert.ok(
    !updateStagedNotified,
    "Should not have seen the notification yet."
  );

  let { appVersion, platformVersion, resolve } = await stagingCall;
  Assert.equal(
    appVersion,
    DEFAULT_UPDATE_VERSION,
    "Should see the right app version"
  );
  Assert.equal(
    platformVersion,
    DEFAULT_UPDATE_VERSION,
    "Should see the right platform version"
  );

  resolve();

  await notified;
});

add_task(async function finish() {
  stop_httpserver(doTestFinish);
});
