/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { Downloads } = ChromeUtils.importESModule(
  "resource://gre/modules/Downloads.sys.mjs"
);
const { DownloadIntegration } = ChromeUtils.importESModule(
  "resource://gre/modules/DownloadIntegration.sys.mjs"
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

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      // Remove the security delay for the dialog during the test.
      ["security.dialog_enable_delay", 0],
      ["browser.helperApps.showOpenOptionForViewableInternally", true],
      // Make sure we don't open a file picker dialog somehow.
      ["browser.download.useDownloadDir", true],
    ],
  });

  // Restore handlers after the whole test has run
  const registerRestoreHandler = function (type, ext) {
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

function ensureMIMEState({ preferredAction, alwaysAskBeforeHandling }) {
  const mimeInfo = gMimeSvc.getFromTypeAndExtension("image/svg+xml", "svg");
  mimeInfo.preferredAction = preferredAction;
  mimeInfo.alwaysAskBeforeHandling = alwaysAskBeforeHandling;
  gHandlerSvc.store(mimeInfo);
}

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
  async function test_check_saving_handler_choices_with_always_ask_before_handling_new_types_pref_enabled() {
    SpecialPowers.pushPrefEnv({
      set: [["browser.download.always_ask_before_handling_new_types", false]],
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
      let fileLaunched = Promise.withResolvers();
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
