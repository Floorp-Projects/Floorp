ChromeUtils.import("resource://gre/modules/FileUtils.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

ChromeUtils.defineModuleGetter(this, "AppMenuNotifications",
                               "resource://gre/modules/AppMenuNotifications.jsm");
ChromeUtils.defineModuleGetter(this, "UpdateListener",
                               "resource://gre/modules/UpdateListener.jsm");

const IS_MACOSX = ("nsILocalFileMac" in Ci);
const IS_WIN = ("@mozilla.org/windows-registry-key;1" in Cc);

const BIN_SUFFIX = (IS_WIN ? ".exe" : "");
const FILE_UPDATER_BIN = "updater" + (IS_MACOSX ? ".app" : BIN_SUFFIX);
const FILE_UPDATER_BIN_BAK = FILE_UPDATER_BIN + ".bak";

const PREF_APP_UPDATE_INTERVAL = "app.update.interval";
const PREF_APP_UPDATE_LASTUPDATETIME = "app.update.lastUpdateTime.background-update-timer";

const DATA_URI_SPEC =  "chrome://mochitests/content/browser/toolkit/mozapps/update/tests/browser/";

var DEBUG_AUS_TEST = true;

const LOG_FUNCTION = info;

const MAX_UPDATE_COPY_ATTEMPTS = 10;

/* import-globals-from testConstants.js */
Services.scriptloader.loadSubScript(DATA_URI_SPEC + "testConstants.js", this);
/* import-globals-from ../data/shared.js */
Services.scriptloader.loadSubScript(DATA_URI_SPEC + "shared.js", this);

var gURLData = URL_HOST + "/" + REL_PATH_DATA;
const URL_MANUAL_UPDATE = gURLData + "downloadPage.html";

const gEnv = Cc["@mozilla.org/process/environment;1"].
             getService(Ci.nsIEnvironment);

let gOriginalUpdateAutoValue = null;

/**
 * Creates the continue file used to signal that update staging or the mock http
 * server should continue. The delay this creates allows the tests to verify the
 * user interfaces before they auto advance to phases of an update. The continue
 * file for staging will be deleted by the test updater and the continue file
 * for update check and update download requests will be deleted by the test
 * http server handler implemented in app_update.sjs. The test returns a promise
 * so the test can wait on the deletion of the continue file when necessary.
 *
 * @param  leafName
 *         The leafName of the file to create. This should be one of the
 *         folowing constants that are defined in testConstants.js:
 *         CONTINUE_CHECK
 *         CONTINUE_DOWNLOAD
 *         CONTINUE_STAGING
 * @return Promise
 *         Resolves when the file is deleted.
 *         Rejects if timeout is exceeded or condition ever throws.
 * @throws If the file already exists.
 */
async function continueFileHandler(leafName) {
  // The default number of retries of 50 in TestUtils.waitForCondition is
  // sufficient for test http server requests. The total time to wait with the
  // default interval of 100 is approximately 5 seconds.
  let retries = undefined;
  let continueFile;
  if (leafName == CONTINUE_STAGING) {
    debugDump("creating " + leafName + " file for slow update staging");
    // Use 100 retries for staging requests to lessen the likelihood of tests
    // intermittently failing on debug builds due to launching the updater. The
    // total time to wait with the default interval of 100 is approximately 10
    // seconds. The test updater uses the same values.
    retries = 100;
    continueFile = getUpdatesPatchDir();
    continueFile.append(leafName);
  } else {
    debugDump("creating " + leafName + " file for slow http server requests");
    continueFile = Services.dirsvc.get("CurWorkD", Ci.nsIFile);
    let continuePath = REL_PATH_DATA + leafName;
    let continuePathParts = continuePath.split("/");
    for (let i = 0; i < continuePathParts.length; ++i) {
      continueFile.append(continuePathParts[i]);
    }
  }
  if (continueFile.exists()) {
    throw new Error("The continue file should not exist, path: " +
                    continueFile.path);
  }
  continueFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, PERMS_FILE);
  return BrowserTestUtils.waitForCondition(() =>
    (!continueFile.exists()),
    "Waiting for file to be deleted, path: " + continueFile.path,
    undefined, retries);

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
  if (!IS_WIN) {
    throw new Error("Windows only test function called");
  }
  let file = getUpdatesRootDir();
  file.append(FILE_UPDATE_TEST);
  file.QueryInterface(Ci.nsILocalFileWin);
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

function setOtherInstanceHandlingUpdates() {
  if (!IS_WIN) {
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
 * Clean up updates list and the updates directory.
 */
function cleanUpUpdates() {
  reloadUpdateManagerData(true);
  removeUpdateDirsAndFiles();
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
 * In addition to changing the value of the Auto Update setting, this function
 * also takes care of cleaning up after itself.
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

add_task(async function setDefaults() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [PREF_APP_UPDATE_LOG, DEBUG_AUS_TEST],
      // See bug 1505790 - uses a very large value to prevent the sync code
      // from running since it has nothing to do with these tests.
      ["services.sync.autoconnectDelay", 600000],
    ]});
  // Most tests in this directory expect auto update to be enabled. Those that
  // don't will explicitly change this.
  await setAppUpdateAutoEnabledHelper(true);
});

/**
 * Runs a typical update test. Will set various common prefs for using the
 * updater doorhanger, runs the provided list of steps, and makes sure
 * everything is cleaned up afterwards.
 *
 * @param  updateParams
 *         Params which will be sent to app_update.sjs.
 * @param  checkAttempts
 *         How many times to check for updates. Useful for testing the UI
 *         for check failures.
 * @param  steps
 *         A list of test steps to perform, specifying expected doorhangers
 *         and additional validation/cleanup callbacks.
 * @return A promise which will resolve once all of the steps have been run
 *         and cleanup has been performed.
 */
function runUpdateTest(updateParams, checkAttempts, steps) {
  return (async function() {
    registerCleanupFunction(() => {
      gEnv.set("MOZ_TEST_SKIP_UPDATE_STAGE", "");
      UpdateListener.reset();
      cleanUpUpdates();
    });

    gEnv.set("MOZ_TEST_SKIP_UPDATE_STAGE", "1");
    setUpdateTimerPrefs();
    removeUpdateDirsAndFiles();
    await SpecialPowers.pushPrefEnv({
      set: [
        [PREF_APP_UPDATE_DOWNLOADPROMPTATTEMPTS, 0],
        [PREF_APP_UPDATE_DISABLEDFORTESTING, false],
        [PREF_APP_UPDATE_IDLETIME, 0],
        [PREF_APP_UPDATE_URL_MANUAL, URL_MANUAL_UPDATE],
      ]});

    await setupTestUpdater();

    let url = URL_HTTP_UPDATE_SJS +
              "?" + updateParams +
              getVersionParams();

    setUpdateURL(url);

    executeSoon(() => {
      (async function() {
        gAUS.checkForBackgroundUpdates();
        for (var i = 0; i < checkAttempts - 1; i++) {
          await waitForEvent("update-error", "check-attempt-failed");
          gAUS.checkForBackgroundUpdates();
        }
      })();
    });

    for (let step of steps) {
      await processStep(step);
    }

    await finishTestRestoreUpdaterBackup();
  })();
}

/**
 * Runs a test which processes an update. Similar to runUpdateTest.
 *
 * @param  updates
 *         A list of updates to process.
 * @param  steps
 *         A list of test steps to perform, specifying expected doorhangers
 *         and additional validation/cleanup callbacks.
 * @return A promise which will resolve once all of the steps have been run
 *         and cleanup has been performed.
 */
function runUpdateProcessingTest(updates, steps) {
  return (async function() {
    registerCleanupFunction(() => {
      gEnv.set("MOZ_TEST_SKIP_UPDATE_STAGE", "");
      UpdateListener.reset();
      cleanUpUpdates();
    });

    gEnv.set("MOZ_TEST_SKIP_UPDATE_STAGE", "1");
    setUpdateTimerPrefs();
    removeUpdateDirsAndFiles();
    await SpecialPowers.pushPrefEnv({
      set: [
        [PREF_APP_UPDATE_DOWNLOADPROMPTATTEMPTS, 0],
        [PREF_APP_UPDATE_DISABLEDFORTESTING, false],
        [PREF_APP_UPDATE_IDLETIME, 0],
        [PREF_APP_UPDATE_URL_MANUAL, URL_MANUAL_UPDATE],
      ]});

    await setupTestUpdater();

    writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), true);

    writeUpdatesToXMLFile(getLocalUpdatesXMLString(""), false);
    writeStatusFile(STATE_FAILED_CRC_ERROR);
    reloadUpdateManagerData();

    testPostUpdateProcessing();

    for (let step of steps) {
      await processStep(step);
    }

    await finishTestRestoreUpdaterBackup();
  })();
}

function processStep(step) {
  if (typeof(step) == "function") {
    return step();
  }

  const {notificationId, button, beforeClick, cleanup} = step;
  return (async function() {

    await BrowserTestUtils.waitForEvent(PanelUI.notificationPanel, "popupshown");
    const shownNotification = AppMenuNotifications.activeNotification.id;

    is(shownNotification, notificationId, "The right notification showed up.");
    if (shownNotification != notificationId) {
      if (cleanup) {
        await cleanup();
      }
      return;
    }

    let buttonEl = getNotificationButton(window, notificationId, button);
    if (beforeClick) {
      await beforeClick();
    }


    buttonEl.click();

    if (cleanup) {
      await cleanup();
    }
  })();
}

/**
 * Waits for the specified topic and (optionally) status.
 * @param  topic
 *         String representing the topic to wait for.
 * @param  status
 *         Optional String representing the status on said topic to wait for.
 * @return A promise which will resolve the first time an event occurs on the
 *         specified topic, and (optionally) with the specified status.
 */
function waitForEvent(topic, status = null) {
  return new Promise(resolve => Services.obs.addObserver({
    observe(subject, innerTopic, innerStatus) {
      if (!status || status == innerStatus) {
        Services.obs.removeObserver(this, topic);
        resolve(innerStatus);
      }
    },
  }, topic));
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
  let notification = win.document.getElementById(`appMenu-${notificationId}-notification`);
  is(notification.hidden, false, `${notificationId} notification is showing`);
  return win.document.getAnonymousElementByAttribute(notification, "anonid", button);
}

/**
 * Ensures that the "What's new" link with the provided ID is displayed and
 * matches the url parameter provided. If no URL is provided, it will instead
 * ensure that the link matches the default link URL.
 *
 * @param  win
 *         The window to get the "What's new" link for.
 * @param  id
 *         The ID of the "What's new" link element.
 * @param  url (optional)
 *         The URL to check against. If none is provided, a default will be used.
 */
function checkWhatsNewLink(win, id, url) {
  let whatsNewLink = win.document.getElementById(id);
  is(whatsNewLink.href,
     url || URL_HTTP_UPDATE_SJS + "?uiURL=DETAILS",
     "What's new link points to the test_details URL");
  is(whatsNewLink.hidden, false, "What's new link is not hidden.");
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
        logTestInfo("Attempt to restore the backed up updater failed... " +
                    "will try again, Exception: " + e);
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
      let baseAppDir = getAppBaseDir();
      let updater = baseAppDir.clone();
      updater.append(FILE_UPDATER_BIN);
      updater.moveTo(baseAppDir, FILE_UPDATER_BIN_BAK);
    } catch (e) {
      logTestInfo("Attempt to move the real updater out of the way failed... " +
                  "will try again, Exception: " + e);
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
      let baseAppDir = getAppBaseDir();
      let testUpdaterDir = Services.dirsvc.get("CurWorkD", Ci.nsIFile);
      let relPath = REL_PATH_DATA;
      let pathParts = relPath.split("/");
      for (let i = 0; i < pathParts.length; ++i) {
        testUpdaterDir.append(pathParts[i]);
      }

      let testUpdater = testUpdaterDir.clone();
      testUpdater.append(FILE_UPDATER_BIN);

      testUpdater.copyToFollowingLinks(baseAppDir, FILE_UPDATER_BIN);
    } catch (e) {
      if (attempt < MAX_UPDATE_COPY_ATTEMPTS) {
        logTestInfo("Attempt to copy the test updater failed... " +
                    "will try again, Exception: " + e);
        await TestUtils.waitForTick();
        await copyTestUpdater(attempt + 1);
      }
    }
  })();
}

/**
 * Restores the updater that was backed up. This is called in setupTestUpdater
 * before the backup of the real updater is done in case the previous test
 * failed to restore the updater when the test has finished.
 */
function restoreUpdaterBackup() {
  let baseAppDir = getAppBaseDir();
  let updater = baseAppDir.clone();
  let updaterBackup = baseAppDir.clone();
  updater.append(FILE_UPDATER_BIN);
  updaterBackup.append(FILE_UPDATER_BIN_BAK);
  if (updaterBackup.exists()) {
    if (updater.exists()) {
      updater.remove(true);
    }
    updaterBackup.moveTo(baseAppDir, FILE_UPDATER_BIN);
  }
}

/**
 * When a staging test finishes this will repeatedly attempt to restore the real
 * updater.
 */
function finishTestRestoreUpdaterBackup() {
  return (async function() {
    if (Services.prefs.getBoolPref(PREF_APP_UPDATE_STAGING_ENABLED)) {
      try {
        // Windows debug builds keep the updater file in use for a short period of
        // time after the updater process exits.
        restoreUpdaterBackup();
      } catch (e) {
        logTestInfo("Attempt to restore the backed up updater failed... " +
                    "will try again, Exception: " + e);

        await TestUtils.waitForTick();
        await finishTestRestoreUpdaterBackup();
      }
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
          let chromeURI = "chrome://browser/content/aboutDialog.xul";
          is(domwindow.document.location.href, chromeURI, "About dialog appeared");
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
 * Runs an About Dialog update test. This will set various common prefs for
 * updating and runs the provided list of steps.
 *
 * @param  updateParams
 *         Params which will be sent to app_update.sjs.
 * @param  backgroundUpdate
 *         If true a background check will be performed before opening the About
 *         Dialog.
 * @param  steps
 *         An array of test steps to perform. A step will either be an object
 *         containing expected conditions and actions or a function to call.
 * @return A promise which will resolve once all of the steps have been run.
 */
function runAboutDialogUpdateTest(updateParams, backgroundUpdate, steps) {
  // Some elements append a trailing /. After the chrome tests are removed this
  // code can be changed so URL_HOST already has a trailing /.
  let detailsURL = URL_HOST + "/";
  let aboutDialog;
  function processAboutDialogStep(step) {
    if (typeof(step) == "function") {
      return step();
    }

    const {panelId, checkActiveUpdate, continueFile} = step;
    return (async function() {
      let updateDeck = aboutDialog.document.getElementById("updateDeck");
      await BrowserTestUtils.waitForCondition(() =>
        (updateDeck.selectedPanel && updateDeck.selectedPanel.id == panelId),
        "Waiting for expected panel ID - got: \"" +
        updateDeck.selectedPanel.id + "\", expected \"" + panelId + "\"");
      let selectedPanel = updateDeck.selectedPanel;
      is(selectedPanel.id, panelId, "The panel ID should equal " + panelId);

      if (checkActiveUpdate) {
        ok(!!gUpdateManager.activeUpdate, "There should be an active update");
        is(gUpdateManager.activeUpdate.state, checkActiveUpdate.state,
           "The active update state should equal " + checkActiveUpdate.state);
      } else {
        ok(!gUpdateManager.activeUpdate,
           "There should not be an active update");
      }

      if (continueFile) {
        await continueFileHandler(continueFile);
      }

      let linkPanels = ["downloadFailed", "manualUpdate", "unsupportedSystem"];
      if (linkPanels.includes(panelId)) {
        // The unsupportedSystem panel uses the update's detailsURL and the
        // downloadFailed and manualUpdate panels use the app.update.url.manual
        // preference.
        let link = selectedPanel.querySelector("label.text-link");
        is(link.href, detailsURL,
           "The panel's link href should equal the expected value");
      }

      let buttonPanels = ["downloadAndInstall", "apply"];
      if (buttonPanels.includes(panelId)) {
        let buttonEl = selectedPanel.querySelector("button");
        await BrowserTestUtils.waitForCondition(() =>
          (aboutDialog.document.activeElement == buttonEl),
          "The button should receive focus");
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
    await SpecialPowers.pushPrefEnv({
      set: [
        [PREF_APP_UPDATE_SERVICE_ENABLED, false],
        [PREF_APP_UPDATE_DISABLEDFORTESTING, false],
        [PREF_APP_UPDATE_URL_MANUAL, detailsURL],
      ],
    });
    registerCleanupFunction(() => {
      gEnv.set("MOZ_TEST_SLOW_SKIP_UPDATE_STAGE", "");
      UpdateListener.reset();
      cleanUpUpdates();
    });

    gEnv.set("MOZ_TEST_SLOW_SKIP_UPDATE_STAGE", "1");
    setUpdateTimerPrefs();
    removeUpdateDirsAndFiles();

    await setupTestUpdater();
    registerCleanupFunction(async () => {
      await finishTestRestoreUpdaterBackup();
    });

    let updateURL = URL_HTTP_UPDATE_SJS + "?detailsURL=" + detailsURL +
                    updateParams + getVersionParams();
    if (backgroundUpdate) {
      setUpdateURL(updateURL);
      if (Services.prefs.getBoolPref(PREF_APP_UPDATE_STAGING_ENABLED)) {
        // Don't wait on the deletion of the continueStaging file
        continueFileHandler(CONTINUE_STAGING);
      }
      gAUS.checkForBackgroundUpdates();
      await waitForEvent("update-downloaded");
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
 * @param  updateParams
 *         Params which will be sent to app_update.sjs.
 * @param  backgroundUpdate
 *         If true a background check will be performed before opening the About
 *         Dialog.
 * @param  steps
 *         An array of test steps to perform. A step will either be an object
 *         containing expected conditions and actions or a function to call.
 * @return A promise which will resolve once all of the steps have been run.
 */
function runAboutPrefsUpdateTest(updateParams, backgroundUpdate, steps) {
  // Some elements append a trailing /. After the chrome tests are removed this
  // code can be changed so URL_HOST already has a trailing /.
  let detailsURL = URL_HOST + "/";
  let tab;
  function processAboutPrefsStep(step) {
    if (typeof(step) == "function") {
      return step();
    }

    const {panelId, checkActiveUpdate, continueFile} = step;
    return (async function() {
      await ContentTask.spawn(tab.linkedBrowser, {panelId}, async ({panelId}) => {
        let updateDeck = content.document.getElementById("updateDeck");
        await ContentTaskUtils.waitForCondition(() =>
          (updateDeck.selectedPanel && updateDeck.selectedPanel.id == panelId),
          "Waiting for expected panel ID - got: \"" +
          updateDeck.selectedPanel.id + "\", expected \"" + panelId + "\"");
        is(updateDeck.selectedPanel.id, panelId,
           "The panel ID should equal " + panelId);
      });

      if (checkActiveUpdate) {
        ok(!!gUpdateManager.activeUpdate, "There should be an active update");
        is(gUpdateManager.activeUpdate.state, checkActiveUpdate.state,
           "The active update state should equal " + checkActiveUpdate.state);
      } else {
        ok(!gUpdateManager.activeUpdate,
           "There should not be an active update");
      }

      if (continueFile) {
        await continueFileHandler(continueFile);
      }

      await ContentTask.spawn(tab.linkedBrowser, {panelId, detailsURL}, async ({panelId, detailsURL}) => {
        let linkPanels = ["downloadFailed", "manualUpdate", "unsupportedSystem"];
        if (linkPanels.includes(panelId)) {
          let selectedPanel =
            content.document.getElementById("updateDeck").selectedPanel;
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
          is(link.href, detailsURL,
             "The panel's link href should equal the expected value");
        }

        let buttonPanels = ["downloadAndInstall", "apply"];
        if (buttonPanels.includes(panelId)) {
          let selectedPanel = content.document.getElementById("updateDeck").selectedPanel;
          let buttonEl = selectedPanel.querySelector("button");
          // Note: The about:preferences doesn't focus the button like the
          // About Dialog does.
          ok(!buttonEl.disabled, "The button should be enabled");
          // Don't click the button on the apply panel since this will restart the
          // application.
          if (selectedPanel.id != "apply") {
            buttonEl.click();
          }
        }
      });
    })();
  }

  return (async function() {
    await SpecialPowers.pushPrefEnv({
      set: [
        [PREF_APP_UPDATE_SERVICE_ENABLED, false],
        [PREF_APP_UPDATE_DISABLEDFORTESTING, false],
        [PREF_APP_UPDATE_URL_MANUAL, detailsURL],
      ],
    });
    registerCleanupFunction(() => {
      gEnv.set("MOZ_TEST_SLOW_SKIP_UPDATE_STAGE", "");
      UpdateListener.reset();
      cleanUpUpdates();
    });

    gEnv.set("MOZ_TEST_SLOW_SKIP_UPDATE_STAGE", "1");
    setUpdateTimerPrefs();
    removeUpdateDirsAndFiles();

    await setupTestUpdater();
    registerCleanupFunction(async () => {
      await finishTestRestoreUpdaterBackup();
    });

    let updateURL = URL_HTTP_UPDATE_SJS + "?detailsURL=" + detailsURL +
                    updateParams + getVersionParams();
    if (backgroundUpdate) {
      setUpdateURL(updateURL);
      if (Services.prefs.getBoolPref(PREF_APP_UPDATE_STAGING_ENABLED)) {
        // Don't wait on the deletion of the continueStaging file
        continueFileHandler(CONTINUE_STAGING);
      }
      gAUS.checkForBackgroundUpdates();
      await waitForEvent("update-downloaded");
    } else {
      updateURL += "&slowUpdateCheck=1&useSlowDownloadMar=1";
      setUpdateURL(updateURL);
    }

    tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:preferences");
    registerCleanupFunction(async () => {
      await BrowserTestUtils.removeTab(tab);
    });

    for (let step of steps) {
      await processAboutPrefsStep(step);
    }
  })();
}
