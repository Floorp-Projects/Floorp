/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { Downloads } = ChromeUtils.import(
  "resource://gre/modules/Downloads.jsm"
);
const { DownloadIntegration } = ChromeUtils.import(
  "resource://gre/modules/DownloadIntegration.jsm"
);

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

const {
  handleInternally,
  saveToDisk,
  useSystemDefault,
  alwaysAsk,
  useHelperApp,
} = Ci.nsIHandlerInfo;

function waitForAcceptButtonToGetEnabled(doc) {
  let dialog = doc.querySelector("#unknownContentType");
  let button = dialog.getButton("accept");
  return TestUtils.waitForCondition(
    () => !button.disabled,
    "Wait for Accept button to get enabled"
  );
}

add_setup(async function() {
  // Remove the security delay for the dialog during the test.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["security.dialog_enable_delay", 0],
      ["browser.helperApps.showOpenOptionForViewableInternally", true],
    ],
  });

  // Restore handlers after the whole test has run
  const registerRestoreHandler = function(type, ext) {
    const mimeInfo = gMimeSvc.getFromTypeAndExtension(type, ext);
    const existed = gHandlerSvc.exists(mimeInfo);
    registerCleanupFunction(() => {
      if (existed) {
        gHandlerSvc.store(mimeInfo);
      } else {
        gHandlerSvc.remove(mimeInfo);
      }
    });
  };
  registerRestoreHandler("image/svg+xml", "svg");
});

const kTestCasesPrefDisabled = [
  {
    description:
      "Saving to disk when internal handling is the default shouldn't change prefs.",
    preDialogState: {
      preferredAction: handleInternally,
      alwaysAskBeforeHandling: false,
    },
    dialogActions(doc) {
      let saveItem = doc.querySelector("#save");
      saveItem.click();
      ok(saveItem.selected, "The 'save' option should now be selected");
    },
    expectTab: false,
    expectLaunch: false,
    expectedPreferredAction: handleInternally,
    expectedAlwaysAskBeforeHandling: false,
  },
  {
    description:
      "Opening externally when internal handling is the default shouldn't change prefs.",
    preDialogState: {
      preferredAction: handleInternally,
      alwaysAskBeforeHandling: false,
    },
    dialogActions(doc) {
      let openItem = doc.querySelector("#open");
      openItem.click();
      ok(openItem.selected, "The 'save' option should now be selected");
    },
    expectTab: false,
    expectLaunch: true,
    expectedPreferredAction: handleInternally,
    expectedAlwaysAskBeforeHandling: false,
  },
  {
    description:
      "Saving to disk when internal handling is the default *should* change prefs if checkbox is ticked.",
    preDialogState: {
      preferredAction: handleInternally,
      alwaysAskBeforeHandling: false,
    },
    dialogActions(doc) {
      let saveItem = doc.querySelector("#save");
      saveItem.click();
      ok(saveItem.selected, "The 'save' option should now be selected");
      let checkbox = doc.querySelector("#rememberChoice");
      checkbox.checked = true;
      checkbox.doCommand();
    },
    expectTab: false,
    expectLaunch: false,
    expectedPreferredAction: saveToDisk,
    expectedAlwaysAskBeforeHandling: false,
  },
  {
    description:
      "Saving to disk when asking is the default should change persisted default.",
    preDialogState: {
      preferredAction: handleInternally,
      alwaysAskBeforeHandling: true,
    },
    dialogActions(doc) {
      let saveItem = doc.querySelector("#save");
      saveItem.click();
      ok(saveItem.selected, "The 'save' option should now be selected");
    },
    expectTab: false,
    expectLaunch: false,
    expectedPreferredAction: saveToDisk,
    expectedAlwaysAskBeforeHandling: true,
  },
  {
    description:
      "Opening externally when asking is the default should change persisted default.",
    preDialogState: {
      preferredAction: handleInternally,
      alwaysAskBeforeHandling: true,
    },
    dialogActions(doc) {
      let openItem = doc.querySelector("#open");
      openItem.click();
      ok(openItem.selected, "The 'save' option should now be selected");
    },
    expectTab: false,
    expectLaunch: true,
    expectedPreferredAction: useSystemDefault,
    expectedAlwaysAskBeforeHandling: true,
  },
];

function ensureMIMEState({ preferredAction, alwaysAskBeforeHandling }) {
  const mimeInfo = gMimeSvc.getFromTypeAndExtension("image/svg+xml", "svg");
  mimeInfo.preferredAction = preferredAction;
  mimeInfo.alwaysAskBeforeHandling = alwaysAskBeforeHandling;
  gHandlerSvc.store(mimeInfo);
}

/**
 * Test that if we have SVGs set to handle internally, and the user chooses to
 * do something else with it, we do not alter the saved state.
 */
add_task(async function test_check_saving_handler_choices() {
  let publicList = await Downloads.getList(Downloads.PUBLIC);
  SpecialPowers.pushPrefEnv({
    set: [
      ["browser.download.improvements_to_download_panel", false],
      ["browser.download.always_ask_before_handling_new_types", true],
    ],
  });
  registerCleanupFunction(async () => {
    await publicList.removeFinished();
  });
  for (let testCase of kTestCasesPrefDisabled) {
    let file = "file_image_svgxml.svg";
    info("Testing with " + file + "; " + testCase.description);
    ensureMIMEState(testCase.preDialogState);

    let dialogWindowPromise = BrowserTestUtils.domWindowOpenedAndLoaded();
    let loadingTab = await BrowserTestUtils.openNewForegroundTab({
      gBrowser,
      opening: TEST_PATH + file,
      waitForLoad: false,
      waitForStateStop: true,
    });
    let dialogWindow = await dialogWindowPromise;
    is(
      dialogWindow.location.href,
      "chrome://mozapps/content/downloads/unknownContentType.xhtml",
      "Should have seen the unknown content dialogWindow."
    );
    let doc = dialogWindow.document;
    let internalHandlerRadio = doc.querySelector("#handleInternally");

    if (Services.focus.activeWindow != dialogWindow) {
      await BrowserTestUtils.waitForEvent(dialogWindow, "activate");
    }

    await waitForAcceptButtonToGetEnabled(doc);

    ok(!internalHandlerRadio.hidden, "The option should be visible for SVG");
    ok(
      internalHandlerRadio.selected,
      "The Firefox option should be selected by default"
    );

    const { expectTab, expectLaunch, description } = testCase;
    // Prep to intercept things so we only see the results we want.
    let tabOpenListener = ev => {
      ok(
        expectTab,
        `A new tab should ${expectTab ? "" : "not "}be opened - ${description}`
      );
      BrowserTestUtils.removeTab(ev.target);
    };
    gBrowser.tabContainer.addEventListener("TabOpen", tabOpenListener);

    let oldLaunchFile = DownloadIntegration.launchFile;
    let fileLaunched = PromiseUtils.defer();
    DownloadIntegration.launchFile = () => {
      ok(
        expectLaunch,
        `The file should ${
          expectLaunch ? "" : "not "
        }be launched with an external application - ${description}`
      );
      fileLaunched.resolve();
    };
    let downloadFinishedPromise = promiseDownloadFinished(publicList);

    await testCase.dialogActions(doc);

    let mainWindowActivatedAndFocused = Promise.all([
      BrowserTestUtils.waitForEvent(window, "activate"),
      BrowserTestUtils.waitForEvent(window, "focus", true),
    ]);
    let dialog = doc.querySelector("#unknownContentType");
    dialog.acceptDialog();
    await mainWindowActivatedAndFocused;

    let download = await downloadFinishedPromise;
    if (expectLaunch) {
      await fileLaunched.promise;
    }
    DownloadIntegration.launchFile = oldLaunchFile;
    gBrowser.tabContainer.removeEventListener("TabOpen", tabOpenListener);

    is(
      (await publicList.getAll()).length,
      1,
      "download should appear in public list"
    );

    // Check mime info:
    const mimeInfo = gMimeSvc.getFromTypeAndExtension("image/svg+xml", "svg");
    gHandlerSvc.fillHandlerInfo(mimeInfo, "");
    is(
      mimeInfo.preferredAction,
      testCase.expectedPreferredAction,
      "preferredAction - " + description
    );
    is(
      mimeInfo.alwaysAskBeforeHandling,
      testCase.expectedAlwaysAskBeforeHandling,
      "alwaysAskBeforeHandling - " + description
    );

    BrowserTestUtils.removeTab(loadingTab);
    await publicList.removeFinished();
    if (download?.target.exists) {
      try {
        await IOUtils.remove(download.target.path);
      } catch (ex) {
        /* ignore */
      }
    }
  }
});

function waitDelay(delay) {
  return new Promise((resolve, reject) => {
    /* eslint-disable mozilla/no-arbitrary-setTimeout */
    window.setTimeout(resolve, delay);
  });
}

function promisePanelOpened() {
  if (DownloadsPanel.panel && DownloadsPanel.panel.state == "open") {
    return Promise.resolve();
  }
  return BrowserTestUtils.waitForEvent(DownloadsPanel.panel, "popupshown");
}

const kTestCasesPrefEnabled = [
  {
    description:
      "Pref enabled - internal handling as default should not change prefs",
    preDialogState: {
      preferredAction: handleInternally,
      alwaysAskBeforeHandling: false,
    },
    expectTab: true,
    expectLaunch: false,
    expectedPreferredAction: handleInternally,
    expectedAlwaysAskBeforeHandling: false,
    expectUCT: false,
  },
  {
    description:
      "Pref enabled - external handling as default should not change prefs",
    preDialogState: {
      preferredAction: useSystemDefault,
      alwaysAskBeforeHandling: false,
    },
    expectTab: false,
    expectLaunch: true,
    expectedPreferredAction: useSystemDefault,
    expectedAlwaysAskBeforeHandling: false,
    expectUCT: false,
  },
  {
    description: "Pref enabled - saveToDisk as default should not change prefs",
    preDialogState: {
      preferredAction: saveToDisk,
      alwaysAskBeforeHandling: false,
    },
    expectTab: false,
    expectLaunch: false,
    expectedPreferredAction: saveToDisk,
    expectedAlwaysAskBeforeHandling: false,
    expectUCT: false,
  },
  {
    description:
      "Pref enabled - choose internal + alwaysAsk default + checkbox should update persisted default",
    preDialogState: {
      preferredAction: alwaysAsk,
      alwaysAskBeforeHandling: false,
    },
    dialogActions(doc) {
      let handleItem = doc.querySelector("#handleInternally");
      handleItem.click();
      ok(handleItem.selected, "The 'open' option should now be selected");
      let checkbox = doc.querySelector("#rememberChoice");
      checkbox.checked = true;
      checkbox.doCommand();
    },
    // new tab will not launch in test environment when alwaysAsk is preferredAction
    expectTab: false,
    expectLaunch: false,
    expectedPreferredAction: handleInternally,
    expectedAlwaysAskBeforeHandling: false,
    expectUCT: true,
  },
  {
    description:
      "Pref enabled - saveToDisk with alwaysAsk default should update persisted default",
    preDialogState: {
      preferredAction: alwaysAsk,
      alwaysAskBeforeHandling: false,
    },
    dialogActions(doc) {
      let saveItem = doc.querySelector("#save");
      saveItem.click();
      ok(saveItem.selected, "The 'save' option should now be selected");
    },
    expectTab: false,
    expectLaunch: false,
    expectedPreferredAction: saveToDisk,
    expectedAlwaysAskBeforeHandling: false,
    expectUCT: true,
  },
];

add_task(
  async function test_check_saving_handler_choices_with_downloads_pref_enabled() {
    SpecialPowers.pushPrefEnv({
      set: [
        ["browser.download.improvements_to_download_panel", true],
        ["browser.download.always_ask_before_handling_new_types", false],
      ],
    });

    let publicList = await Downloads.getList(Downloads.PUBLIC);
    registerCleanupFunction(async () => {
      await publicList.removeFinished();
    });
    let file = "file_image_svgxml.svg";

    for (let testCase of kTestCasesPrefEnabled) {
      info("Testing with " + file + "; " + testCase.description);
      ensureMIMEState(testCase.preDialogState);
      const { expectTab, expectLaunch, description, expectUCT } = testCase;

      let oldLaunchFile = DownloadIntegration.launchFile;
      let fileLaunched = PromiseUtils.defer();
      DownloadIntegration.launchFile = () => {
        ok(
          expectLaunch,
          `The file should ${
            expectLaunch ? "" : "not "
          }be launched with an external application - ${description}`
        );
        fileLaunched.resolve();
      };

      info("Load window and tabs");
      let dialogWindowPromise = BrowserTestUtils.domWindowOpenedAndLoaded();
      let loadingTab = await BrowserTestUtils.openNewForegroundTab({
        gBrowser,
        opening: TEST_PATH + file,
        waitForLoad: false,
        waitForStateStop: true,
      });

      // See if UCT window appears in loaded tab.
      let dialogWindow = await Promise.race([
        waitDelay(1000),
        dialogWindowPromise,
      ]);

      is(
        !!dialogWindow,
        expectUCT,
        `UCT window should${expectUCT ? "" : " not"} have appeared`
      );

      let download;

      if (dialogWindow) {
        is(
          dialogWindow.location.href,
          "chrome://mozapps/content/downloads/unknownContentType.xhtml",
          "Unknown content dialogWindow should be loaded correctly."
        );
        let doc = dialogWindow.document;
        let internalHandlerRadio = doc.querySelector("#handleInternally");

        info("Waiting for accept button to get enabled");
        await waitForAcceptButtonToGetEnabled(doc);

        ok(
          !internalHandlerRadio.hidden,
          "The option should be visible for SVG"
        );

        info("Running UCT dialog options before downloading file");
        await testCase.dialogActions(doc);

        let dialog = doc.querySelector("#unknownContentType");
        dialog.acceptDialog();

        info("Waiting for downloads to finish");
        let downloadFinishedPromise = promiseDownloadFinished(publicList);
        download = await downloadFinishedPromise;
      } else {
        let downloadPanelPromise = promisePanelOpened();
        await downloadPanelPromise;
        is(
          DownloadsPanel.isPanelShowing,
          true,
          "DownloadsPanel should be open"
        );

        info("Skipping UCT dialog options");
        info("Waiting for downloads to finish");
        // Unlike when the UCT window opens, the download immediately starts.
        let downloadList = await publicList;
        [download] = downloadList._downloads;
      }

      if (expectLaunch) {
        info("Waiting for launch to finish");
        await fileLaunched.promise;
      }
      DownloadIntegration.launchFile = oldLaunchFile;

      is(
        download.contentType,
        "image/svg+xml",
        "File contentType should be correct"
      );
      is(
        download.source.url,
        `${TEST_PATH + file}`,
        "File name should be correct."
      );
      is(
        (await publicList.getAll()).length,
        1,
        "download should appear in public list"
      );

      // Check mime info:
      const mimeInfo = gMimeSvc.getFromTypeAndExtension("image/svg+xml", "svg");
      gHandlerSvc.fillHandlerInfo(mimeInfo, "");
      is(
        mimeInfo.preferredAction,
        testCase.expectedPreferredAction,
        "preferredAction - " + description
      );
      is(
        mimeInfo.alwaysAskBeforeHandling,
        testCase.expectedAlwaysAskBeforeHandling,
        "alwaysAskBeforeHandling - " + description
      );

      info("Cleaning up");
      BrowserTestUtils.removeTab(loadingTab);
      // By default, if internal is default with pref enabled, we view the svg file in
      // in a new tab. Close this tab in order for the test case to pass.
      if (expectTab && testCase.preferredAction !== alwaysAsk) {
        BrowserTestUtils.removeTab(gBrowser.selectedTab);
      }
      await publicList.removeFinished();
      if (download?.target.exists) {
        try {
          await IOUtils.remove(download.target.path);
        } catch (ex) {
          /* ignore */
        }
      }
    }
  }
);
