/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "AppMenuNotifications",
  "resource://gre/modules/AppMenuNotifications.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "UpdateListener",
  "resource://gre/modules/UpdateListener.jsm"
);
const { XPIInstall } = ChromeUtils.import(
  "resource://gre/modules/addons/XPIInstall.jsm"
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
const gDefaultWhatsNewURL = URL_HTTP_UPDATE_SJS + "?uiURL=DETAILS";

// Set to true to log additional information for debugging. To log additional
// information for individual tests set gDebugTest to false here and to true
// globally in the test.
gDebugTest = false;

// This is to accommodate the TV task which runs the tests with --verify.
requestLongerTimeout(10);

/**
 * Common tasks to perform for all tests before each one has started.
 */
add_task(async function setupTestCommon() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [PREF_APP_UPDATE_BADGEWAITTIME, 1800],
      [PREF_APP_UPDATE_DOWNLOAD_ATTEMPTS, 0],
      [PREF_APP_UPDATE_DOWNLOAD_MAXATTEMPTS, 2],
      [PREF_APP_UPDATE_LOG, gDebugTest],
      [PREF_APP_UPDATE_PROMPTWAITTIME, 3600],
      [PREF_APP_UPDATE_SERVICE_ENABLED, false],
      // Disable activity stream to prevent errors when opening pages during
      // TV runs. See bug 1548422 for an example.
      ["browser.library.activity-stream.enabled", false],
    ],
  });

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
  gEnv.set("MOZ_TEST_SKIP_UPDATE_STAGE", "");
  gEnv.set("MOZ_TEST_SLOW_SKIP_UPDATE_STAGE", "");
  gEnv.set("MOZ_TEST_STAGING_ERROR", "");
  UpdateListener.reset();
  AppMenuNotifications.removeNotification(/.*/);
  reloadUpdateManagerData(true);
  // Pass false when the log files are needed for troubleshooting the tests.
  removeUpdateFiles(true);
  // Always try to restore the original updater files. If none of the updater
  // backup files are present then this is just a no-op.
  await finishTestRestoreUpdaterBackup();
});

/**
 * Overrides the add-ons manager language pack staging with a mocked version.
 * The returned promise resolves when language pack staging begins returning an
 * object with the new appVersion and platformVersion and functions to resolve
 * or reject the install.
 */
function mockLangpackInstall() {
  let original = XPIInstall.stageLangpacksForAppUpdate;
  registerCleanupFunction(() => {
    XPIInstall.stageLangpacksForAppUpdate = original;
  });

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

/**
 * Creates the continue file used to signal that update staging or the mock http
 * server should continue. The delay this creates allows the tests to verify the
 * user interfaces before they auto advance to other phases of an update. The
 * continue file for staging will be deleted by the test updater and the
 * continue file for the update check and update download requests will be
 * deleted by the test http server handler implemented in app_update.sjs. The
 * test returns a promise so the test can wait on the deletion of the continue
 * file when necessary. If the continue file still exists at the end of a test
 * it will be removed to prevent it from affecting tests that run after the test
 * that created it.
 *
 * @param  leafName
 *         The leafName of the file to create. This should be one of the
 *         folowing constants that are defined in testConstants.js:
 *         CONTINUE_CHECK
 *         CONTINUE_DOWNLOAD
 *         CONTINUE_STAGING
 * @return Promise
 *         Resolves when the file is deleted or if the file is not deleted when
 *         the check for the file's existence times out. If the file isn't
 *         deleted before the check for the file's existence times out it will
 *         be deleted when the test ends so it doesn't affect tests that run
 *         after the test that created the continue file.
 * @throws If the file already exists.
 */
async function continueFileHandler(leafName) {
  // The total time to wait with 300 retries and the default interval of 100 is
  // approximately 30 seconds.
  let interval = 100;
  let retries = 300;
  let continueFile;
  if (leafName == CONTINUE_STAGING) {
    // The total time to wait with 600 retries and an interval of 200 is
    // approximately 120 seconds.
    interval = 200;
    retries = 600;
    continueFile = getGREBinDir();
    if (AppConstants.platform == "macosx") {
      continueFile = continueFile.parent.parent;
    }
    continueFile.append(leafName);
  } else {
    continueFile = Services.dirsvc.get("CurWorkD", Ci.nsIFile);
    let continuePath = REL_PATH_DATA + leafName;
    let continuePathParts = continuePath.split("/");
    for (let i = 0; i < continuePathParts.length; ++i) {
      continueFile.append(continuePathParts[i]);
    }
  }
  if (continueFile.exists()) {
    logTestInfo(
      "The continue file should not exist, path: " + continueFile.path
    );
    continueFile.remove(false);
  }
  debugDump("Creating continue file, path: " + continueFile.path);
  continueFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, PERMS_FILE);
  // If for whatever reason the continue file hasn't been removed when a test
  // has finished remove it during cleanup so it doesn't affect tests that run
  // after the test that created it.
  registerCleanupFunction(() => {
    if (continueFile.exists()) {
      logTestInfo(
        "Removing continue file during test cleanup, path: " + continueFile.path
      );
      continueFile.remove(false);
    }
  });
  return TestUtils.waitForCondition(
    () => !continueFile.exists(),
    "Waiting for file to be deleted, path: " + continueFile.path,
    interval,
    retries
  ).catch(e => {
    logTestInfo(
      "Continue file was not removed after checking " +
        retries +
        " times, path: " +
        continueFile.path
    );
  });
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
    file.fileAttributesWin |= file.WFA_READWRITE;
    file.fileAttributesWin &= ~file.WFA_READONLY;
    file.remove(false);
  }
  file.create(file.NORMAL_FILE_TYPE, 0o444);
  file.fileAttributesWin |= file.WFA_READONLY;
  file.fileAttributesWin &= ~file.WFA_READWRITE;
  registerCleanupFunction(() => {
    file.fileAttributesWin |= file.WFA_READWRITE;
    file.fileAttributesWin &= ~file.WFA_READONLY;
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
 *         The anonid of the button to get.
 * @return The button element.
 */
function getNotificationButton(win, notificationId, button) {
  let notification = win.document.getElementById(
    `appMenu-${notificationId}-notification`
  );
  ok(!notification.hidden, `${notificationId} notification is showing`);
  return notification[button];
}

/**
 * Ensures that the "What's new" link with the provided ID is displayed and
 * matches the url parameter provided.
 *
 * @param  win
 *         The window to get the "What's new" link for.
 * @param  id
 *         The ID of the "What's new" link element.
 * @param  url
 *         The URL to check against.
 */
function checkWhatsNewLink(win, id, url) {
  let whatsNewLink = win.document.getElementById(id);
  ok(!whatsNewLink.hidden, "What's new link is not hidden.");
  is(whatsNewLink.href, url, `What's new link href should equal ${url}`);
}

/**
 * For staging tests the test updater must be used and this restores the backed
 * up real updater if it exists and tries again on failure since Windows debug
 * builds at times leave the file in use. After success moveRealUpdater is
 * called to continue the setup of the test updater.
 */
function setupTestUpdater() {
  return (async function() {
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
  return (async function() {
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
  return (async function() {
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
      let updateSettingsIni = greDir.clone();
      updateSettingsIni.append(FILE_UPDATE_SETTINGS_INI);
      writeFile(updateSettingsIni, UPDATE_SETTINGS_CONTENTS);

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
  return (async function() {
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
      onCloseWindow: aXULWindow => {},
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
 * @return  A nsIUpdatePatch object matching the type specified
 */
function getPatchOfType(type) {
  let update = gUpdateManager.activeUpdate;
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

    const { notificationId, button, checkActiveUpdate, pageURLs } = step;
    return (async function() {
      if (!params.popupShown) {
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

      if (checkActiveUpdate) {
        ok(!!gUpdateManager.activeUpdate, "There should be an active update");
        is(
          gUpdateManager.activeUpdate.state,
          checkActiveUpdate.state,
          `The active update state should equal ${checkActiveUpdate.state}`
        );
      } else {
        ok(
          !gUpdateManager.activeUpdate,
          "There should not be an active update"
        );
      }

      if (pageURLs && pageURLs.whatsNew !== undefined) {
        checkWhatsNewLink(
          window,
          `${notificationId}-whats-new`,
          pageURLs.whatsNew
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

  return (async function() {
    gEnv.set("MOZ_TEST_SKIP_UPDATE_STAGE", "1");
    await SpecialPowers.pushPrefEnv({
      set: [
        [PREF_APP_UPDATE_DISABLEDFORTESTING, false],
        [PREF_APP_UPDATE_URL_DETAILS, gDetailsURL],
        [PREF_APP_UPDATE_URL_MANUAL, URL_MANUAL_UPDATE],
      ],
    });

    await setupTestUpdater();

    let queryString = params.queryString ? params.queryString : "";
    let updateURL =
      URL_HTTP_UPDATE_SJS +
      "?detailsURL=" +
      gDetailsURL +
      queryString +
      getVersionParams();
    setUpdateURL(updateURL);

    if (params.checkAttempts) {
      // Perform a background check doorhanger test.
      executeSoon(() => {
        (async function() {
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
      testPostUpdateProcessing();
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

    const { panelId, checkActiveUpdate, continueFile, downloadInfo } = step;
    return (async function() {
      let updateDeck = aboutDialog.document.getElementById("updateDeck");
      // Also continue if the selected panel ID is 'apply' since there are no
      // other panels after 'apply'.
      await TestUtils.waitForCondition(
        () =>
          updateDeck.selectedPanel &&
          (updateDeck.selectedPanel.id == panelId ||
            updateDeck.selectedPanel.id == "apply"),
        "Waiting for the expected panel ID: " + panelId,
        undefined,
        200
      ).catch(e => {
        // Instead of throwing let the check below fail the test so the panel
        // ID and the expected panel ID is printed in the log.
        logTestInfo(e);
      });
      let selectedPanel = updateDeck.selectedPanel;
      is(selectedPanel.id, panelId, "The panel ID should equal " + panelId);

      if (checkActiveUpdate) {
        ok(!!gUpdateManager.activeUpdate, "There should be an active update");
        is(
          gUpdateManager.activeUpdate.state,
          checkActiveUpdate.state,
          "The active update state should equal " + checkActiveUpdate.state
        );
      } else {
        ok(
          !gUpdateManager.activeUpdate,
          "There should not be an active update"
        );
      }

      if (panelId == "downloading") {
        for (let i = 0; i < downloadInfo.length; ++i) {
          let data = downloadInfo[i];
          // The About Dialog tests always specify a continue file.
          await continueFileHandler(continueFile);
          let patch = getPatchOfType(data.patchType);
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
          }
        }
      } else if (continueFile) {
        await continueFileHandler(continueFile);
      }

      let linkPanels = ["downloadFailed", "manualUpdate", "unsupportedSystem"];
      if (linkPanels.includes(panelId)) {
        // The unsupportedSystem panel uses the update's detailsURL and the
        // downloadFailed and manualUpdate panels use the app.update.url.manual
        // preference.
        let link = selectedPanel.querySelector("label.text-link");
        is(
          link.href,
          gDetailsURL,
          `The panel's link href should equal ${gDetailsURL}`
        );
      }

      let buttonPanels = ["downloadAndInstall", "apply"];
      if (buttonPanels.includes(panelId)) {
        let buttonEl = selectedPanel.querySelector("button");
        await TestUtils.waitForCondition(
          () => aboutDialog.document.activeElement == buttonEl,
          "The button should receive focus"
        );
        ok(!buttonEl.disabled, "The button should be enabled");
        // Don't click the button on the apply panel since this will restart the
        // application.
        if (panelId != "apply") {
          buttonEl.click();
        }
      }
    })();
  }

  return (async function() {
    gEnv.set("MOZ_TEST_SLOW_SKIP_UPDATE_STAGE", "1");
    await SpecialPowers.pushPrefEnv({
      set: [
        [PREF_APP_UPDATE_DISABLEDFORTESTING, false],
        [PREF_APP_UPDATE_URL_MANUAL, gDetailsURL],
      ],
    });

    await setupTestUpdater();

    let queryString = params.queryString ? params.queryString : "";
    let updateURL =
      URL_HTTP_UPDATE_SJS +
      "?detailsURL=" +
      gDetailsURL +
      queryString +
      getVersionParams();
    if (params.backgroundUpdate) {
      setUpdateURL(updateURL);
      gAUS.checkForBackgroundUpdates();
      if (params.continueFile) {
        await continueFileHandler(params.continueFile);
      }
      if (params.waitForUpdateState) {
        await TestUtils.waitForCondition(
          () =>
            gUpdateManager.activeUpdate &&
            gUpdateManager.activeUpdate.state == params.waitForUpdateState,
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
          gUpdateManager.activeUpdate.state,
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

    const { panelId, checkActiveUpdate, continueFile, downloadInfo } = step;
    return (async function() {
      await SpecialPowers.spawn(
        tab.linkedBrowser,
        [{ panelId }],
        async ({ panelId }) => {
          let updateDeck = content.document.getElementById("updateDeck");
          // Also continue if the selected panel ID is 'apply' since there are no
          // other panels after 'apply'.
          await ContentTaskUtils.waitForCondition(
            () =>
              updateDeck.selectedPanel &&
              (updateDeck.selectedPanel.id == panelId ||
                updateDeck.selectedPanel.id == "apply"),
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
            updateDeck.selectedPanel.id,
            panelId,
            "The panel ID should equal " + panelId
          );
        }
      );

      if (checkActiveUpdate) {
        ok(!!gUpdateManager.activeUpdate, "There should be an active update");
        is(
          gUpdateManager.activeUpdate.state,
          checkActiveUpdate.state,
          "The active update state should equal " + checkActiveUpdate.state
        );
      } else {
        ok(
          !gUpdateManager.activeUpdate,
          "There should not be an active update"
        );
      }

      if (panelId == "downloading") {
        for (let i = 0; i < downloadInfo.length; ++i) {
          let data = downloadInfo[i];
          // The About Dialog tests always specify a continue file.
          await continueFileHandler(continueFile);
          let patch = getPatchOfType(data.patchType);
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
          }
        }
      } else if (continueFile) {
        await continueFileHandler(continueFile);
      }

      await SpecialPowers.spawn(
        tab.linkedBrowser,
        [{ panelId, gDetailsURL }],
        async ({ panelId, gDetailsURL }) => {
          let linkPanels = [
            "downloadFailed",
            "manualUpdate",
            "unsupportedSystem",
          ];
          if (linkPanels.includes(panelId)) {
            let selectedPanel = content.document.getElementById("updateDeck")
              .selectedPanel;
            // The unsupportedSystem panel uses the update's detailsURL and the
            // downloadFailed and manualUpdate panels use the app.update.url.manual
            // preference.
            let selector = "label.text-link";
            // The downloadFailed panel in about:preferences uses an anchor
            // instead of a label for the link.
            if (selectedPanel.id == "downloadFailed") {
              selector = "a.text-link";
            }
            let link = selectedPanel.querySelector(selector);
            is(
              link.href,
              gDetailsURL,
              `The panel's link href should equal ${gDetailsURL}`
            );
          }

          let buttonPanels = ["downloadAndInstall", "apply"];
          if (buttonPanels.includes(panelId)) {
            let selectedPanel = content.document.getElementById("updateDeck")
              .selectedPanel;
            let buttonEl = selectedPanel.querySelector("button");
            // Note: The about:preferences doesn't focus the button like the
            // About Dialog does.
            ok(!buttonEl.disabled, "The button should be enabled");
            // Don't click the button on the apply panel since this will restart
            // the application.
            if (selectedPanel.id != "apply") {
              buttonEl.click();
            }
          }
        }
      );
    })();
  }

  return (async function() {
    gEnv.set("MOZ_TEST_SLOW_SKIP_UPDATE_STAGE", "1");
    await SpecialPowers.pushPrefEnv({
      set: [
        [PREF_APP_UPDATE_DISABLEDFORTESTING, false],
        [PREF_APP_UPDATE_URL_MANUAL, gDetailsURL],
      ],
    });

    await setupTestUpdater();

    let queryString = params.queryString ? params.queryString : "";
    let updateURL =
      URL_HTTP_UPDATE_SJS +
      "?detailsURL=" +
      gDetailsURL +
      queryString +
      getVersionParams();
    if (params.backgroundUpdate) {
      setUpdateURL(updateURL);
      gAUS.checkForBackgroundUpdates();
      if (params.continueFile) {
        await continueFileHandler(params.continueFile);
      }
      if (params.waitForUpdateState) {
        // Wait until the update state equals the expected value before
        // displaying the UI.
        await TestUtils.waitForCondition(
          () =>
            gUpdateManager.activeUpdate &&
            gUpdateManager.activeUpdate.state == params.waitForUpdateState,
          "Waiting for update state: " + params.waitForUpdateState,
          undefined,
          200
        ).catch(e => {
          // Instead of throwing let the check below fail the test so the panel
          // ID and the expected panel ID is printed in the log.
          logTestInfo(e);
        });
        is(
          gUpdateManager.activeUpdate.state,
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
  return (async function() {
    Services.telemetry.clearScalars();
    gEnv.set("MOZ_TEST_SKIP_UPDATE_STAGE", "1");
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
    gAUS.checkForBackgroundUpdates();
    await waitForEvent(event);
  })();
}
