/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  AppMenuNotifications: "resource://gre/modules/AppMenuNotifications.sys.mjs",
  DownloadUtils: "resource://gre/modules/DownloadUtils.sys.mjs",
  UpdateListener: "resource://gre/modules/UpdateListener.sys.mjs",
});
const { XPIExports } = ChromeUtils.importESModule(
  "resource://gre/modules/addons/XPIExports.sys.mjs"
);

const BIN_SUFFIX = AppConstants.platform == "win" ? ".exe" : "";
const FILE_UPDATER_BIN =
  "updater" + (AppConstants.platform == "macosx" ? ".app" : BIN_SUFFIX);
const FILE_UPDATER_BIN_BAK = FILE_UPDATER_BIN + ".bak";

const LOG_FUNCTION = info;

const MAX_UPDATE_COPY_ATTEMPTS = 10;

const DATA_URI_SPEC =
  "chrome://mochitests/content/browser/toolkit/mozapps/update/tests/browser/";
/* import-globals-from testConstants.js */
Services.scriptloader.loadSubScript(DATA_URI_SPEC + "testConstants.js", this);

var gURLData = URL_HOST + "/" + REL_PATH_DATA;
const URL_MANUAL_UPDATE = gURLData + "downloadPage.html";

const gBadSizeResult = Cr.NS_ERROR_UNEXPECTED.toString();

/* import-globals-from ../data/shared.js */
Services.scriptloader.loadSubScript(DATA_URI_SPEC + "shared.js", this);

let gOriginalUpdateAutoValue = null;

// Some elements append a trailing /. After the chrome tests are removed this
// code can be changed so URL_HOST already has a trailing /.
const gDetailsURL = URL_HOST + "/";

// Set to true to log additional information for debugging. To log additional
// information for individual tests set gDebugTest to false here and to true
// globally in the test.
gDebugTest = false;

// This is to accommodate the TV task which runs the tests with --verify.
requestLongerTimeout(10);

/**
 * Common tasks to perform for all tests before each one has started.
 */
add_setup(async function setupTestCommon() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [PREF_APP_UPDATE_BADGEWAITTIME, 1800],
      [PREF_APP_UPDATE_DOWNLOAD_ATTEMPTS, 0],
      [PREF_APP_UPDATE_DOWNLOAD_MAXATTEMPTS, 2],
      [PREF_APP_UPDATE_LOG, gDebugTest],
      [PREF_APP_UPDATE_PROMPTWAITTIME, 3600],
      [PREF_APP_UPDATE_SERVICE_ENABLED, false],
    ],
  });

  // We need to keep the update sync manager from thinking two instances are
  // running because of the mochitest parent instance, which means we need to
  // override the directory service with a fake executable path and then reset
  // the lock. But leaving the directory service overridden causes problems for
  // these tests, so we need to restore the real service immediately after.
  // To form the path, we'll use the real executable path with a token appended
  // (the path needs to be absolute, but not to point to a real file).
  // This block is loosely copied from adjustGeneralPaths() in another update
  // test file, xpcshellUtilsAUS.js, but this is a much more limited version;
  // it's been copied here both because the full function is overkill and also
  // because making it general enough to run in both xpcshell and mochitest
  // would have been unreasonably difficult.
  let exePath = Services.dirsvc.get(XRE_EXECUTABLE_FILE, Ci.nsIFile);
  let dirProvider = {
    getFile: function AGP_DP_getFile(aProp, aPersistent) {
      // Set the value of persistent to false so when this directory provider is
      // unregistered it will revert back to the original provider.
      aPersistent.value = false;
      switch (aProp) {
        case XRE_EXECUTABLE_FILE:
          exePath.append("browser-test");
          return exePath;
      }
      return null;
    },
    QueryInterface: ChromeUtils.generateQI(["nsIDirectoryServiceProvider"]),
  };
  let ds = Services.dirsvc.QueryInterface(Ci.nsIDirectoryService);
  ds.QueryInterface(Ci.nsIProperties).undefine(XRE_EXECUTABLE_FILE);
  ds.registerProvider(dirProvider);

  let syncManager = Cc["@mozilla.org/updates/update-sync-manager;1"].getService(
    Ci.nsIUpdateSyncManager
  );
  syncManager.resetLock();

  ds.unregisterProvider(dirProvider);

  setUpdateTimerPrefs();
  reloadUpdateManagerData(true);
  removeUpdateFiles(true);
  UpdateListener.reset();
  AppMenuNotifications.removeNotification(/.*/);
  // Most app update mochitest-browser-chrome tests expect auto update to be
  // enabled. Those that don't will explicitly change this.
  await setAppUpdateAutoEnabledHelper(true);
});

/**
 * Common tasks to perform for all tests after each one has finished.
 */
registerCleanupFunction(async () => {
  AppMenuNotifications.removeNotification(/.*/);
  Services.env.set("MOZ_TEST_SKIP_UPDATE_STAGE", "");
  Services.env.set("MOZ_TEST_SLOW_SKIP_UPDATE_STAGE", "");
  Services.env.set("MOZ_TEST_STAGING_ERROR", "");
  UpdateListener.reset();
  AppMenuNotifications.removeNotification(/.*/);
  reloadUpdateManagerData(true);
  // Pass false when the log files are needed for troubleshooting the tests.
  removeUpdateFiles(true);
  // Always try to restore the original updater files. If none of the updater
  // backup files are present then this is just a no-op.
  await finishTestRestoreUpdaterBackup();
  // Reset the update lock once again so that we know the lock we're
  // interested in here will be closed properly (normally that happens during
  // XPCOM shutdown, but that isn't consistent during tests).
  let syncManager = Cc["@mozilla.org/updates/update-sync-manager;1"].getService(
    Ci.nsIUpdateSyncManager
  );
  syncManager.resetLock();
});

/**
 * Overrides the add-ons manager language pack staging with a mocked version.
 * The returned promise resolves when language pack staging begins returning an
 * object with the new appVersion and platformVersion and functions to resolve
 * or reject the install.
 */
function mockLangpackInstall() {
  let original = XPIExports.XPIInstall.stageLangpacksForAppUpdate;
  registerCleanupFunction(() => {
    XPIExports.XPIInstall.stageLangpacksForAppUpdate = original;
  });

  let stagingCall = Promise.withResolvers();
  XPIExports.XPIInstall.stageLangpacksForAppUpdate = (
    appVersion,
    platformVersion
  ) => {
    let result = Promise.withResolvers();
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

/**
 * Creates and locks the app update write test file so it is possible to test
 * when the user doesn't have write access to update. Since this is only
 * possible on Windows the function throws when it is called on other platforms.
 * This uses registerCleanupFunction to remove the lock and the file when the
 * test completes.
 *
 * @throws If the function is called on a platform other than Windows.
 */
function lockWriteTestFile() {
  if (AppConstants.platform != "win") {
    throw new Error("Windows only test function called");
  }
  let file = getUpdateDirFile(FILE_UPDATE_TEST).QueryInterface(
    Ci.nsILocalFileWin
  );
  // Remove the file if it exists just in case.
  if (file.exists()) {
    file.readOnly = false;
    file.remove(false);
  }
  file.create(file.NORMAL_FILE_TYPE, 0o444);
  file.readOnly = true;
  registerCleanupFunction(() => {
    file.readOnly = false;
    file.remove(false);
  });
}

/**
 * Closes the update mutex handle in nsUpdateService.js if it exists and then
 * creates a new update mutex handle so the update code thinks there is another
 * instance of the application handling updates.
 *
 * @throws If the function is called on a platform other than Windows.
 */
function setOtherInstanceHandlingUpdates() {
  if (AppConstants.platform != "win") {
    throw new Error("Windows only test function called");
  }
  gAUS.observe(null, "test-close-handle-update-mutex", "");
  let handle = createMutex(getPerInstallationMutexName());
  registerCleanupFunction(() => {
    closeHandle(handle);
  });
}

/**
 * Gets the update version info for the update url parameters to send to
 * app_update.sjs.
 *
 * @param  aAppVersion (optional)
 *         The application version for the update snippet. If not specified the
 *         current application version will be used.
 * @return The url parameters for the application and platform version to send
 *         to app_update.sjs.
 */
function getVersionParams(aAppVersion) {
  let appInfo = Services.appinfo;
  return "&appVersion=" + (aAppVersion ? aAppVersion : appInfo.version);
}

/**
 * Prevent nsIUpdateTimerManager from notifying nsIApplicationUpdateService
 * to check for updates by setting the app update last update time to the
 * current time minus one minute in seconds and the interval time to 12 hours
 * in seconds.
 */
function setUpdateTimerPrefs() {
  let now = Math.round(Date.now() / 1000) - 60;
  Services.prefs.setIntPref(PREF_APP_UPDATE_LASTUPDATETIME, now);
  Services.prefs.setIntPref(PREF_APP_UPDATE_INTERVAL, 43200);
}

/*
 * Sets the value of the App Auto Update setting and sets it back to the
 * original value at the start of the test when the test finishes.
 *
 * @param  enabled
 *         The value to set App Auto Update to.
 */
async function setAppUpdateAutoEnabledHelper(enabled) {
  if (gOriginalUpdateAutoValue == null) {
    gOriginalUpdateAutoValue = await UpdateUtils.getAppUpdateAutoEnabled();
    registerCleanupFunction(async () => {
      await UpdateUtils.setAppUpdateAutoEnabled(gOriginalUpdateAutoValue);
    });
  }
  await UpdateUtils.setAppUpdateAutoEnabled(enabled);
}

/**
 * Gets the specified button for the notification.
 *
 * @param  win
 *         The window to get the notification button for.
 * @param  notificationId
 *         The ID of the notification to get the button for.
 * @param  button
 *         The anonid of the button to get, or a function to find it.
 * @return The button element.
 */
function getNotificationButton(win, notificationId, button) {
  let notification = win.document.getElementById(
    `appMenu-${notificationId}-notification`
  );
  ok(!notification.hidden, `${notificationId} notification is showing`);
  if (typeof button === "function") {
    return button(notification);
  }
  return notification[button];
}

/**
 * For staging tests the test updater must be used and this restores the backed
 * up real updater if it exists and tries again on failure since Windows debug
 * builds at times leave the file in use. After success moveRealUpdater is
 * called to continue the setup of the test updater.
 */
function setupTestUpdater() {
  return (async function () {
    if (Services.prefs.getBoolPref(PREF_APP_UPDATE_STAGING_ENABLED)) {
      try {
        restoreUpdaterBackup();
      } catch (e) {
        logTestInfo(
          "Attempt to restore the backed up updater failed... " +
            "will try again, Exception: " +
            e
        );
        await TestUtils.waitForTick();
        await setupTestUpdater();
        return;
      }
      await moveRealUpdater();
    }
  })();
}

/**
 * Backs up the real updater and tries again on failure since Windows debug
 * builds at times leave the file in use. After success it will call
 * copyTestUpdater to continue the setup of the test updater.
 */
function moveRealUpdater() {
  return (async function () {
    try {
      // Move away the real updater
      let greBinDir = getGREBinDir();
      let updater = greBinDir.clone();
      updater.append(FILE_UPDATER_BIN);
      updater.moveTo(greBinDir, FILE_UPDATER_BIN_BAK);

      let greDir = getGREDir();
      let updateSettingsIni = greDir.clone();
      updateSettingsIni.append(FILE_UPDATE_SETTINGS_INI);
      if (updateSettingsIni.exists()) {
        updateSettingsIni.moveTo(greDir, FILE_UPDATE_SETTINGS_INI_BAK);
      }

      let precomplete = greDir.clone();
      precomplete.append(FILE_PRECOMPLETE);
      if (precomplete.exists()) {
        precomplete.moveTo(greDir, FILE_PRECOMPLETE_BAK);
      }
    } catch (e) {
      logTestInfo(
        "Attempt to move the real updater out of the way failed... " +
          "will try again, Exception: " +
          e
      );
      await TestUtils.waitForTick();
      await moveRealUpdater();
      return;
    }

    await copyTestUpdater();
  })();
}

/**
 * Copies the test updater and tries again on failure since Windows debug builds
 * at times leave the file in use.
 */
function copyTestUpdater(attempt = 0) {
  return (async function () {
    try {
      // Copy the test updater
      let greBinDir = getGREBinDir();
      let testUpdaterDir = Services.dirsvc.get("CurWorkD", Ci.nsIFile);
      let relPath = REL_PATH_DATA;
      let pathParts = relPath.split("/");
      for (let i = 0; i < pathParts.length; ++i) {
        testUpdaterDir.append(pathParts[i]);
      }

      let testUpdater = testUpdaterDir.clone();
      testUpdater.append(FILE_UPDATER_BIN);
      testUpdater.copyToFollowingLinks(greBinDir, FILE_UPDATER_BIN);

      let greDir = getGREDir();

      // On macOS, update settings is a Framework, not an INI. This was already
      // built into updater-xpcshell using the `UpdateSettings-xpcshell`
      // Framework, so we don't need to do any additional work here.
      if (AppConstants.platform != "macosx") {
        let updateSettingsIni = greDir.clone();
        updateSettingsIni.append(FILE_UPDATE_SETTINGS_INI);
        writeFile(updateSettingsIni, UPDATE_SETTINGS_CONTENTS);
      }

      let precomplete = greDir.clone();
      precomplete.append(FILE_PRECOMPLETE);
      writeFile(precomplete, PRECOMPLETE_CONTENTS);
    } catch (e) {
      if (attempt < MAX_UPDATE_COPY_ATTEMPTS) {
        logTestInfo(
          "Attempt to copy the test updater failed... " +
            "will try again, Exception: " +
            e
        );
        await TestUtils.waitForTick();
        await copyTestUpdater(attempt++);
      }
    }
  })();
}

/**
 * Restores the updater and updater related file that if there a backup exists.
 * This is called in setupTestUpdater before the backup of the real updater is
 * done in case the previous test failed to restore the file when a test has
 * finished. This is also called in finishTestRestoreUpdaterBackup to restore
 * the files when a test finishes.
 */
function restoreUpdaterBackup() {
  let greBinDir = getGREBinDir();
  let updater = greBinDir.clone();
  let updaterBackup = greBinDir.clone();
  updater.append(FILE_UPDATER_BIN);
  updaterBackup.append(FILE_UPDATER_BIN_BAK);
  if (updaterBackup.exists()) {
    if (updater.exists()) {
      updater.remove(true);
    }
    updaterBackup.moveTo(greBinDir, FILE_UPDATER_BIN);
  }

  let greDir = getGREDir();
  let updateSettingsIniBackup = greDir.clone();
  updateSettingsIniBackup.append(FILE_UPDATE_SETTINGS_INI_BAK);
  if (updateSettingsIniBackup.exists()) {
    let updateSettingsIni = greDir.clone();
    updateSettingsIni.append(FILE_UPDATE_SETTINGS_INI);
    if (updateSettingsIni.exists()) {
      updateSettingsIni.remove(false);
    }
    updateSettingsIniBackup.moveTo(greDir, FILE_UPDATE_SETTINGS_INI);
  }

  let precomplete = greDir.clone();
  let precompleteBackup = greDir.clone();
  precomplete.append(FILE_PRECOMPLETE);
  precompleteBackup.append(FILE_PRECOMPLETE_BAK);
  if (precompleteBackup.exists()) {
    if (precomplete.exists()) {
      precomplete.remove(false);
    }
    precompleteBackup.moveTo(greDir, FILE_PRECOMPLETE);
  } else if (precomplete.exists()) {
    if (readFile(precomplete) == PRECOMPLETE_CONTENTS) {
      precomplete.remove(false);
    }
  }
}

/**
 * When a test finishes this will repeatedly attempt to restore the real updater
 * and the other files for the updater if a backup of the file exists.
 */
function finishTestRestoreUpdaterBackup() {
  return (async function () {
    try {
      // Windows debug builds keep the updater file in use for a short period of
      // time after the updater process exits.
      restoreUpdaterBackup();
    } catch (e) {
      logTestInfo(
        "Attempt to restore the backed up updater failed... " +
          "will try again, Exception: " +
          e
      );

      await TestUtils.waitForTick();
      await finishTestRestoreUpdaterBackup();
    }
  })();
}

/**
 * Waits for the About Dialog to load.
 *
 * @return A promise that returns the domWindow for the About Dialog and
 *         resolves when the About Dialog loads.
 */
function waitForAboutDialog() {
  return new Promise(resolve => {
    var listener = {
      onOpenWindow: aXULWindow => {
        debugDump("About dialog shown...");
        Services.wm.removeListener(listener);

        async function aboutDialogOnLoad() {
          domwindow.removeEventListener("load", aboutDialogOnLoad, true);
          let chromeURI = "chrome://browser/content/aboutDialog.xhtml";
          is(
            domwindow.document.location.href,
            chromeURI,
            "About dialog appeared"
          );
          resolve(domwindow);
        }

        var domwindow = aXULWindow.docShell.domWindow;
        domwindow.addEventListener("load", aboutDialogOnLoad, true);
      },
      onCloseWindow: _aXULWindow => {},
    };

    Services.wm.addListener(listener);
    openAboutDialog();
  });
}

/**
 * Return the first UpdatePatch with the given type.
 *
 * @param   type
 *          The type of the patch ("complete" or "partial")
 * @param   update
 *          The nsIUpdate to select a patch from.
 * @return  A nsIUpdatePatch object matching the type specified
 */
function getPatchOfType(type, update) {
  if (update) {
    for (let i = 0; i < update.patchCount; ++i) {
      let patch = update.getPatchAt(i);
      if (patch && patch.type == type) {
        return patch;
      }
    }
  }
  return null;
}

/**
 * Runs a Doorhanger update test. This will set various common prefs for
 * updating and runs the provided list of steps.
 *
 * @param  params
 *         An object containing parameters used to run the test.
 * @param  steps
 *         An array of test steps to perform. A step will either be an object
 *         containing expected conditions and actions or a function to call.
 * @return A promise which will resolve once all of the steps have been run.
 */
function runDoorhangerUpdateTest(params, steps) {
  function processDoorhangerStep(step) {
    if (typeof step == "function") {
      return step();
    }

    const {
      notificationId,
      button,
      checkActiveUpdate,
      pageURLs,
      expectedStateOverride,
    } = step;
    return (async function () {
      if (!params.popupShown && !PanelUI.isNotificationPanelOpen) {
        await BrowserTestUtils.waitForEvent(
          PanelUI.notificationPanel,
          "popupshown"
        );
      }
      const shownNotificationId = AppMenuNotifications.activeNotification.id;
      is(
        shownNotificationId,
        notificationId,
        "The right notification showed up."
      );

      let expectedState = Ci.nsIApplicationUpdateService.STATE_IDLE;
      if (expectedStateOverride) {
        expectedState = expectedStateOverride;
      } else if (notificationId == "update-restart") {
        expectedState = Ci.nsIApplicationUpdateService.STATE_PENDING;
      }
      let actualState = gAUS.currentState;
      is(
        actualState,
        expectedState,
        `The current update state should be ` +
          `"${gAUS.getStateName(expectedState)}". Actual: ` +
          `"${gAUS.getStateName(actualState)}"`
      );

      if (checkActiveUpdate) {
        let activeUpdate = await (checkActiveUpdate.state == STATE_DOWNLOADING
          ? gUpdateManager.getDownloadingUpdate()
          : gUpdateManager.getReadyUpdate());
        ok(!!activeUpdate, "There should be an active update");
        is(
          activeUpdate.state,
          checkActiveUpdate.state,
          `The active update state should equal ${checkActiveUpdate.state}`
        );
      } else {
        ok(
          !(await gUpdateManager.getDownloadingUpdate()),
          "There should not be a downloading update"
        );
        ok(
          !(await gUpdateManager.getReadyUpdate()),
          "There should not be a ready update"
        );
      }

      let buttonEl = getNotificationButton(window, notificationId, button);
      buttonEl.click();

      if (pageURLs && pageURLs.manual !== undefined) {
        await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
        is(
          gBrowser.selectedBrowser.currentURI.spec,
          pageURLs.manual,
          `The page's url should equal ${pageURLs.manual}`
        );
        gBrowser.removeTab(gBrowser.selectedTab);
      }
    })();
  }

  return (async function () {
    if (params.slowStaging) {
      Services.env.set("MOZ_TEST_SLOW_SKIP_UPDATE_STAGE", "1");
    } else {
      Services.env.set("MOZ_TEST_SKIP_UPDATE_STAGE", "1");
    }
    await SpecialPowers.pushPrefEnv({
      set: [
        [PREF_APP_UPDATE_DISABLEDFORTESTING, false],
        [PREF_APP_UPDATE_URL_DETAILS, gDetailsURL],
        [PREF_APP_UPDATE_URL_MANUAL, URL_MANUAL_UPDATE],
      ],
    });

    await setupTestUpdater();

    let baseURL = URL_HTTP_UPDATE_SJS;
    if (params.baseURL) {
      baseURL = params.baseURL;
    }
    let queryString = params.queryString ? params.queryString : "";
    let updateURL =
      baseURL +
      "?detailsURL=" +
      gDetailsURL +
      queryString +
      getVersionParams(params.version);
    setUpdateURL(updateURL);
    if (params.checkAttempts) {
      // Perform a background check doorhanger test.
      executeSoon(() => {
        (async function () {
          // `checkForBackgroundUpdates` is asynchronous, but it's not important
          // for us to `await` on it since we will `await` on the results. And
          // `await`ing on it could cause us to miss the events that we want to
          // see.
          gAUS.checkForBackgroundUpdates();
          for (var i = 0; i < params.checkAttempts - 1; i++) {
            await waitForEvent("update-error", "check-attempt-failed");
            gAUS.checkForBackgroundUpdates();
          }
        })();
      });
    } else {
      // Perform a startup processing doorhanger test.
      writeStatusFile(STATE_FAILED_CRC_ERROR);
      writeUpdatesToXMLFile(getLocalUpdatesXMLString(params.updates), true);
      reloadUpdateManagerData();
      await testPostUpdateProcessing();
    }

    for (let step of steps) {
      await processDoorhangerStep(step);
    }
  })();
}

/**
 * Runs an About Dialog update test. This will set various common prefs for
 * updating and runs the provided list of steps.
 *
 * @param  params
 *         An object containing parameters used to run the test.
 * @param  steps
 *         An array of test steps to perform. A step will either be an object
 *         containing expected conditions and actions or a function to call.
 * @return A promise which will resolve once all of the steps have been run.
 */
function runAboutDialogUpdateTest(params, steps) {
  let aboutDialog;
  function processAboutDialogStep(step) {
    if (typeof step == "function") {
      return step(aboutDialog);
    }

    const {
      panelId,
      checkActiveUpdate,
      continueFile,
      downloadInfo,
      forceApply,
      noContinue,
      expectedStateOverride,
    } = step;
    return (async function () {
      await TestUtils.waitForCondition(
        () =>
          aboutDialog.gAppUpdater &&
          aboutDialog.gAppUpdater.selectedPanel?.id == panelId,
        "Waiting for the expected panel ID: " + panelId,
        undefined,
        200
      ).catch(e => {
        // Instead of throwing let the check below fail the test so the panel
        // ID and the expected panel ID is printed in the log.
        logTestInfo(e);
      });
      let { selectedPanel } = aboutDialog.gAppUpdater;
      is(selectedPanel.id, panelId, "The panel ID should equal " + panelId);
      ok(
        BrowserTestUtils.isVisible(selectedPanel),
        "The panel should be visible"
      );

      if (
        panelId == "downloading" &&
        gAUS.currentState == Ci.nsIApplicationUpdateService.STATE_IDLE
      ) {
        // Now that `AUS.downloadUpdate` is async, we start showing the
        // downloading panel while `AUS.downloadUpdate` is still resolving.
        // But the below checks assume that this resolution has already
        // happened. So we need to wait for things to actually resolve.
        debugDump("Waiting for downloading state to actually start");
        await gAUS.stateTransition;

        // Check that the checks that we made above are still valid.
        selectedPanel = aboutDialog.gAppUpdater.selectedPanel;
        is(selectedPanel.id, panelId, "The panel ID should equal " + panelId);
        ok(
          BrowserTestUtils.isVisible(selectedPanel),
          "The panel should be visible"
        );
      }

      let expectedState = Ci.nsIApplicationUpdateService.STATE_IDLE;
      if (expectedStateOverride) {
        expectedState = expectedStateOverride;
      } else if (panelId == "apply") {
        expectedState = Ci.nsIApplicationUpdateService.STATE_PENDING;
      } else if (panelId == "downloading") {
        expectedState = Ci.nsIApplicationUpdateService.STATE_DOWNLOADING;
      } else if (panelId == "applying") {
        expectedState = Ci.nsIApplicationUpdateService.STATE_STAGING;
      }
      let actualState = gAUS.currentState;
      is(
        actualState,
        expectedState,
        `The current update state should be ` +
          `"${gAUS.getStateName(expectedState)}". Actual: ` +
          `"${gAUS.getStateName(actualState)}"`
      );

      if (checkActiveUpdate) {
        let activeUpdate = await (checkActiveUpdate.state == STATE_DOWNLOADING
          ? gUpdateManager.getDownloadingUpdate()
          : gUpdateManager.getReadyUpdate());
        ok(!!activeUpdate, "There should be an active update");
        is(
          activeUpdate.state,
          checkActiveUpdate.state,
          "The active update state should equal " + checkActiveUpdate.state
        );
      } else {
        ok(
          !(await gUpdateManager.getDownloadingUpdate()),
          "There should not be a downloading update"
        );
        ok(
          !(await gUpdateManager.getReadyUpdate()),
          "There should not be a ready update"
        );
      }

      // Some tests just want to stop at the downloading state. These won't
      // include a continue file in that state.
      if (panelId == "downloading" && continueFile) {
        for (let i = 0; i < downloadInfo.length; ++i) {
          let data = downloadInfo[i];
          await continueFileHandler(continueFile);
          let patch = getPatchOfType(
            data.patchType,
            await gUpdateManager.getDownloadingUpdate()
          );
          // The update is removed early when the last download fails so check
          // that there is a patch before proceeding.
          let isLastPatch = i == downloadInfo.length - 1;
          if (!isLastPatch || patch) {
            let resultName = data.bitsResult ? "bitsResult" : "internalResult";
            patch.QueryInterface(Ci.nsIWritablePropertyBag);
            await TestUtils.waitForCondition(
              () => patch.getProperty(resultName) == data[resultName],
              "Waiting for expected patch property " +
                resultName +
                " value: " +
                data[resultName],
              undefined,
              200
            ).catch(e => {
              // Instead of throwing let the check below fail the test so the
              // property value and the expected property value is printed in
              // the log.
              logTestInfo(e);
            });
            is(
              "" + patch.getProperty(resultName),
              data[resultName],
              "The patch property " +
                resultName +
                " value should equal " +
                data[resultName]
            );

            // Check the download status text.  It should be something like,
            // "1.4 of 1.4 KB".
            let expectedText = DownloadUtils.getTransferTotal(
              data[resultName] == gBadSizeResult ? 0 : patch.size,
              patch.size
            );
            Assert.ok(
              expectedText,
              "Sanity check: Expected download status text should be non-empty"
            );
            if (aboutDialog.document.hasPendingL10nMutations) {
              await BrowserTestUtils.waitForEvent(
                aboutDialog.document,
                "L10nMutationsFinished"
              );
            }
            Assert.equal(
              aboutDialog.document.querySelector(
                `#downloading label[data-l10n-name="download-status"]`
              ).textContent,
              expectedText,
              "Download status text should be correct"
            );
          }
        }
      } else if (continueFile) {
        await continueFileHandler(continueFile);
      }

      let linkPanels = [
        "downloadFailed",
        "manualUpdate",
        "unsupportedSystem",
        "internalError",
      ];
      if (linkPanels.includes(panelId)) {
        // The unsupportedSystem panel uses the update's detailsURL and the
        // downloadFailed and manualUpdate panels use the app.update.url.manual
        // preference.
        let selector = "label.text-link";
        if (selectedPanel.ownerDocument.hasPendingL10nMutations) {
          await BrowserTestUtils.waitForEvent(
            selectedPanel.ownerDocument,
            "L10nMutationsFinished"
          );
        }
        let link = selectedPanel.querySelector(selector);
        is(
          link.href,
          gDetailsURL,
          `The panel's link href should equal ${gDetailsURL}`
        );
        const assertNonEmptyText = (node, description) => {
          let textContent = node.textContent.trim();
          ok(textContent, `${description}, got "${textContent}"`);
        };
        assertNonEmptyText(
          link,
          `The panel's link should have non-empty textContent`
        );
        let linkWrapperClone = link.parentNode.cloneNode(true);
        linkWrapperClone.querySelector(selector).remove();
        assertNonEmptyText(
          linkWrapperClone,
          `The panel's link should have text around the link`
        );
      }

      // Automatically click the download button unless `noContinue` was passed.
      let buttonPanels = ["downloadAndInstall", "apply"];
      if (buttonPanels.includes(panelId) && !noContinue) {
        let buttonEl = selectedPanel.querySelector("button");
        await TestUtils.waitForCondition(
          () => aboutDialog.document.activeElement == buttonEl,
          "The button should receive focus"
        );
        ok(!buttonEl.disabled, "The button should be enabled");
        // Don't click the button on the apply panel since this will restart the
        // application.
        if (panelId != "apply" || forceApply) {
          buttonEl.click();
        }
      }
    })();
  }

  return (async function () {
    Services.env.set("MOZ_TEST_SLOW_SKIP_UPDATE_STAGE", "1");
    await SpecialPowers.pushPrefEnv({
      set: [
        [PREF_APP_UPDATE_DISABLEDFORTESTING, false],
        [PREF_APP_UPDATE_URL_MANUAL, gDetailsURL],
      ],
    });

    await setupTestUpdater();

    let baseURL = URL_HTTP_UPDATE_SJS;
    if (params.baseURL) {
      baseURL = params.baseURL;
    }
    let queryString = params.queryString ? params.queryString : "";
    let updateURL =
      baseURL +
      "?detailsURL=" +
      gDetailsURL +
      queryString +
      getVersionParams(params.version);
    if (params.backgroundUpdate) {
      setUpdateURL(updateURL);
      await gAUS.checkForBackgroundUpdates();
      if (params.continueFile) {
        await continueFileHandler(params.continueFile);
      }
      if (params.waitForUpdateState) {
        let whichUpdateFn =
          params.waitForUpdateState == STATE_DOWNLOADING
            ? "getDownloadingUpdate"
            : "getReadyUpdate";
        let update;
        await TestUtils.waitForCondition(
          async () => {
            update = await gUpdateManager[whichUpdateFn]();
            return update && update.state == params.waitForUpdateState;
          },
          "Waiting for update state: " + params.waitForUpdateState,
          undefined,
          200
        ).catch(e => {
          // Instead of throwing let the check below fail the test so the panel
          // ID and the expected panel ID is printed in the log.
          logTestInfo(e);
        });
        // Display the UI after the update state equals the expected value.
        is(
          update.state,
          params.waitForUpdateState,
          "The update state value should equal " + params.waitForUpdateState
        );
      }
    } else {
      updateURL += "&slowUpdateCheck=1&useSlowDownloadMar=1";
      setUpdateURL(updateURL);
    }

    aboutDialog = await waitForAboutDialog();
    registerCleanupFunction(() => {
      aboutDialog.close();
    });

    for (let step of steps) {
      await processAboutDialogStep(step);
    }
  })();
}

/**
 * Runs an about:preferences update test. This will set various common prefs for
 * updating and runs the provided list of steps.
 *
 * @param  params
 *         An object containing parameters used to run the test.
 * @param  steps
 *         An array of test steps to perform. A step will either be an object
 *         containing expected conditions and actions or a function to call.
 * @return A promise which will resolve once all of the steps have been run.
 */
function runAboutPrefsUpdateTest(params, steps) {
  let tab;
  function processAboutPrefsStep(step) {
    if (typeof step == "function") {
      return step(tab);
    }

    const {
      panelId,
      checkActiveUpdate,
      continueFile,
      downloadInfo,
      forceApply,
      expectedStateOverride,
    } = step;
    return (async function () {
      await SpecialPowers.spawn(
        tab.linkedBrowser,
        [{ panelId }],
        async ({ panelId }) => {
          await ContentTaskUtils.waitForCondition(
            () => content.gAppUpdater.selectedPanel?.id == panelId,
            "Waiting for the expected panel ID: " + panelId,
            undefined,
            200
          ).catch(e => {
            // Instead of throwing let the check below fail the test so the panel
            // ID and the expected panel ID is printed in the log. Use info here
            // instead of logTestInfo since logTestInfo isn't available in the
            // content task.
            info(e);
          });
          is(
            content.gAppUpdater.selectedPanel.id,
            panelId,
            "The panel ID should equal " + panelId
          );
        }
      );

      if (
        panelId == "downloading" &&
        gAUS.currentState == Ci.nsIApplicationUpdateService.STATE_IDLE
      ) {
        // Now that `AUS.downloadUpdate` is async, we start showing the
        // downloading panel while `AUS.downloadUpdate` is still resolving.
        // But the below checks assume that this resolution has already
        // happened. So we need to wait for things to actually resolve.
        debugDump("Waiting for downloading state to actually start");
        await gAUS.stateTransition;

        // Check that the checks that we made above are still valid.
        await SpecialPowers.spawn(
          tab.linkedBrowser,
          [{ panelId }],
          ({ panelId }) => {
            is(
              content.gAppUpdater.selectedPanel.id,
              panelId,
              "The panel ID should equal " + panelId
            );
          }
        );
      }

      let expectedState = Ci.nsIApplicationUpdateService.STATE_IDLE;
      if (expectedStateOverride) {
        expectedState = expectedStateOverride;
      } else if (panelId == "apply") {
        expectedState = Ci.nsIApplicationUpdateService.STATE_PENDING;
      } else if (panelId == "downloading") {
        expectedState = Ci.nsIApplicationUpdateService.STATE_DOWNLOADING;
      } else if (panelId == "applying") {
        expectedState = Ci.nsIApplicationUpdateService.STATE_STAGING;
      }
      let actualState = gAUS.currentState;
      is(
        actualState,
        expectedState,
        `The current update state should be ` +
          `"${gAUS.getStateName(expectedState)}". Actual: ` +
          `"${gAUS.getStateName(actualState)}"`
      );

      if (checkActiveUpdate) {
        let activeUpdate = await (checkActiveUpdate.state == STATE_DOWNLOADING
          ? gUpdateManager.getDownloadingUpdate()
          : gUpdateManager.getReadyUpdate());
        ok(!!activeUpdate, "There should be an active update");
        is(
          activeUpdate.state,
          checkActiveUpdate.state,
          "The active update state should equal " + checkActiveUpdate.state
        );
      } else {
        ok(
          !(await gUpdateManager.getDownloadingUpdate()),
          "There should not be a downloading update"
        );
        ok(
          !(await gUpdateManager.getReadyUpdate()),
          "There should not be a ready update"
        );
      }

      if (panelId == "downloading") {
        if (!downloadInfo) {
          logTestInfo("no downloadinfo, possible error?");
        }
        for (let i = 0; i < downloadInfo.length; ++i) {
          let data = downloadInfo[i];
          // The About Dialog tests always specify a continue file.
          await continueFileHandler(continueFile);
          let patch = getPatchOfType(
            data.patchType,
            await gUpdateManager.getDownloadingUpdate()
          );
          // The update is removed early when the last download fails so check
          // that there is a patch before proceeding.
          let isLastPatch = i == downloadInfo.length - 1;
          if (!isLastPatch || patch) {
            let resultName = data.bitsResult ? "bitsResult" : "internalResult";
            patch.QueryInterface(Ci.nsIWritablePropertyBag);
            await TestUtils.waitForCondition(
              () => patch.getProperty(resultName) == data[resultName],
              "Waiting for expected patch property " +
                resultName +
                " value: " +
                data[resultName],
              undefined,
              200
            ).catch(e => {
              // Instead of throwing let the check below fail the test so the
              // property value and the expected property value is printed in
              // the log.
              logTestInfo(e);
            });
            is(
              "" + patch.getProperty(resultName),
              data[resultName],
              "The patch property " +
                resultName +
                " value should equal " +
                data[resultName]
            );

            // Check the download status text.  It should be something like,
            // "Downloading update — 1.4 of 1.4 KB".  We check only the second
            // part to make sure that the downloaded size is updated correctly.
            let actualText = await SpecialPowers.spawn(
              tab.linkedBrowser,
              [],
              async () => {
                const { document } = content;
                if (document.hasPendingL10nMutations) {
                  await ContentTaskUtils.waitForEvent(
                    document,
                    "L10nMutationsFinished"
                  );
                }
                return document.getElementById("downloading").textContent;
              }
            );
            let expectedSuffix = DownloadUtils.getTransferTotal(
              data[resultName] == gBadSizeResult ? 0 : patch.size,
              patch.size
            );
            Assert.ok(
              expectedSuffix,
              "Sanity check: Expected download status text should be non-empty"
            );
            Assert.ok(
              actualText.endsWith(expectedSuffix),
              "Download status text should end as expected: " +
                JSON.stringify({ actualText, expectedSuffix })
            );
          }
        }
      } else if (continueFile) {
        await continueFileHandler(continueFile);
      }

      await SpecialPowers.spawn(
        tab.linkedBrowser,
        [{ panelId, gDetailsURL, forceApply }],
        async ({ panelId, gDetailsURL, forceApply }) => {
          let linkPanels = [
            "downloadFailed",
            "manualUpdate",
            "unsupportedSystem",
            "internalError",
          ];
          if (linkPanels.includes(panelId)) {
            let { selectedPanel } = content.gAppUpdater;
            // The unsupportedSystem panel uses the update's detailsURL and the
            // downloadFailed and manualUpdate panels use the app.update.url.manual
            // preference.
            let selector = "label.text-link";
            // The downloadFailed panel in about:preferences uses an anchor
            // instead of a label for the link.
            if (selectedPanel.id == "downloadFailed") {
              selector = "a.text-link";
            }
            // The manualUpdate panel in about:preferences uses
            // the moz-support-link element which doesn't have
            // the .text-link class.
            if (selectedPanel.id == "manualUpdate") {
              selector = "a.manualLink";
            }
            if (selectedPanel.ownerDocument.hasPendingL10nMutations) {
              await ContentTaskUtils.waitForEvent(
                selectedPanel.ownerDocument,
                "L10nMutationsFinished"
              );
            }
            let link = selectedPanel.querySelector(selector);
            is(
              link.href,
              gDetailsURL,
              `The panel's link href should equal ${gDetailsURL}`
            );
            const assertNonEmptyText = (node, description) => {
              let textContent = node.textContent.trim();
              ok(textContent, `${description}, got "${textContent}"`);
            };
            assertNonEmptyText(
              link,
              `The panel's link should have non-empty textContent`
            );
            let linkWrapperClone = link.parentNode.cloneNode(true);
            linkWrapperClone.querySelector(selector).remove();
            assertNonEmptyText(
              linkWrapperClone,
              `The panel's link should have text around the link`
            );
          }

          let buttonPanels = ["downloadAndInstall", "apply"];
          if (buttonPanels.includes(panelId)) {
            let { selectedPanel } = content.gAppUpdater;
            let buttonEl = selectedPanel.querySelector("button");
            // Note: The about:preferences doesn't focus the button like the
            // About Dialog does.
            ok(!buttonEl.disabled, "The button should be enabled");
            // Don't click the button on the apply panel since this will restart
            // the application.
            if (selectedPanel.id != "apply" || forceApply) {
              buttonEl.click();
            }
          }
        }
      );
    })();
  }

  return (async function () {
    Services.env.set("MOZ_TEST_SLOW_SKIP_UPDATE_STAGE", "1");
    await SpecialPowers.pushPrefEnv({
      set: [
        [PREF_APP_UPDATE_DISABLEDFORTESTING, false],
        [PREF_APP_UPDATE_URL_MANUAL, gDetailsURL],
      ],
    });

    await setupTestUpdater();

    let baseURL = URL_HTTP_UPDATE_SJS;
    if (params.baseURL) {
      baseURL = params.baseURL;
    }
    let queryString = params.queryString ? params.queryString : "";
    let updateURL =
      baseURL +
      "?detailsURL=" +
      gDetailsURL +
      queryString +
      getVersionParams(params.version);
    if (params.backgroundUpdate) {
      setUpdateURL(updateURL);
      await gAUS.checkForBackgroundUpdates();
      if (params.continueFile) {
        await continueFileHandler(params.continueFile);
      }
      if (params.waitForUpdateState) {
        // Wait until the update state equals the expected value before
        // displaying the UI.
        let whichUpdateFn =
          params.waitForUpdateState == STATE_DOWNLOADING
            ? "getDownloadingUpdate"
            : "getReadyUpdate";
        let update;
        await TestUtils.waitForCondition(
          async () => {
            update = await gUpdateManager[whichUpdateFn]();
            return update && update.state == params.waitForUpdateState;
          },
          "Waiting for update state: " + params.waitForUpdateState,
          undefined,
          200
        ).catch(e => {
          // Instead of throwing let the check below fail the test so the panel
          // ID and the expected panel ID is printed in the log.
          logTestInfo(e);
        });
        is(
          update.state,
          params.waitForUpdateState,
          "The update state value should equal " + params.waitForUpdateState
        );
      }
    } else {
      updateURL += "&slowUpdateCheck=1&useSlowDownloadMar=1";
      setUpdateURL(updateURL);
    }

    tab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      "about:preferences"
    );
    registerCleanupFunction(async () => {
      await BrowserTestUtils.removeTab(tab);
    });

    // Scroll the UI into view so it is easier to troubleshoot tests.
    await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
      content.document.getElementById("updatesCategory").scrollIntoView();
    });

    for (let step of steps) {
      await processAboutPrefsStep(step);
    }
  })();
}

/**
 * Removes the modified update-settings.ini file so the updater will fail to
 * stage an update.
 */
function removeUpdateSettingsIni() {
  if (Services.prefs.getBoolPref(PREF_APP_UPDATE_STAGING_ENABLED)) {
    let greDir = getGREDir();
    let updateSettingsIniBak = greDir.clone();
    updateSettingsIniBak.append(FILE_UPDATE_SETTINGS_INI_BAK);
    if (updateSettingsIniBak.exists()) {
      let updateSettingsIni = greDir.clone();
      updateSettingsIni.append(FILE_UPDATE_SETTINGS_INI);
      updateSettingsIni.remove(false);
    }
  }
}

/**
 * Runs a telemetry update test. This will set various common prefs for
 * updating, checks for an update, and waits for the specified observer
 * notification.
 *
 * @param  updateParams
 *         Params which will be sent to app_update.sjs.
 * @param  event
 *         The observer notification to wait for before proceeding.
 * @param  stageFailure (optional)
 *         Whether to force a staging failure by removing the modified
 *         update-settings.ini file.
 * @return A promise which will resolve after the .
 */
function runTelemetryUpdateTest(updateParams, event, stageFailure = false) {
  return (async function () {
    Services.telemetry.clearScalars();
    Services.env.set("MOZ_TEST_SKIP_UPDATE_STAGE", "1");
    await SpecialPowers.pushPrefEnv({
      set: [[PREF_APP_UPDATE_DISABLEDFORTESTING, false]],
    });

    await setupTestUpdater();

    if (stageFailure) {
      removeUpdateSettingsIni();
    }

    let updateURL =
      URL_HTTP_UPDATE_SJS +
      "?detailsURL=" +
      gDetailsURL +
      updateParams +
      getVersionParams();
    setUpdateURL(updateURL);
    // `checkForBackgroundUpdates` is asynchronous, but it's not important
    // for us to `await` on it since we will `await` on the results. And
    // `await`ing on it could cause us to miss the event that we want to
    // see.
    gAUS.checkForBackgroundUpdates();
    await waitForEvent(event);
  })();
}
