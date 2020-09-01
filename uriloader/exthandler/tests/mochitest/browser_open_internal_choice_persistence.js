/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.import("resource://gre/modules/Downloads.jsm", this);
const { DownloadIntegration } = ChromeUtils.import(
  "resource://gre/modules/DownloadIntegration.jsm"
);

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

function waitForAcceptButtonToGetEnabled(doc) {
  let dialog = doc.querySelector("#unknownContentType");
  let button = dialog.getButton("accept");
  return TestUtils.waitForCondition(
    () => !button.disabled,
    "Wait for Accept button to get enabled"
  );
}

async function waitForPdfJS(browser, url) {
  await SpecialPowers.pushPrefEnv({
    set: [["pdfjs.eventBusDispatchToDOM", true]],
  });
  // Runs tests after all "load" event handlers have fired off
  let loadPromise = BrowserTestUtils.waitForContentEvent(
    browser,
    "documentloaded",
    false,
    null,
    true
  );
  await SpecialPowers.spawn(browser, [url], contentUrl => {
    content.location = contentUrl;
  });
  return loadPromise;
}

add_task(async function setup() {
  // Remove the security delay for the dialog during the test.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["security.dialog_enable_delay", 0],
      ["browser.helperApps.showOpenOptionForPdfJS", true],
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
  registerRestoreHandler("application/pdf", "pdf");
});

const { handleInternally, saveToDisk, useSystemDefault } = Ci.nsIHandlerInfo;

const kTestCases = [
  {
    description:
      "Saving to disk when internal handling is the default shouldn't change prefs.",
    preDialogState: { preferredAction: handleInternally, alwaysAsk: false },
    dialogActions(doc) {
      let saveItem = doc.querySelector("#save");
      saveItem.click();
      ok(saveItem.selected, "The 'save' option should now be selected");
    },
    expectTab: false,
    expectLaunch: false,
    expectedPreferredAction: handleInternally,
    expectedAsk: false,
  },
  {
    description:
      "Opening externally when internal handling is the default shouldn't change prefs.",
    preDialogState: { preferredAction: handleInternally, alwaysAsk: false },
    dialogActions(doc) {
      let openItem = doc.querySelector("#open");
      openItem.click();
      ok(openItem.selected, "The 'save' option should now be selected");
    },
    expectTab: false,
    expectLaunch: true,
    expectedPreferredAction: handleInternally,
    expectedAsk: false,
  },
  {
    description:
      "Saving to disk when internal handling is the default *should* change prefs if checkbox is ticked.",
    preDialogState: { preferredAction: handleInternally, alwaysAsk: false },
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
    expectedAsk: false,
  },
  {
    description:
      "Saving to disk when asking is the default should change persisted default.",
    preDialogState: { preferredAction: handleInternally, alwaysAsk: true },
    dialogActions(doc) {
      let saveItem = doc.querySelector("#save");
      saveItem.click();
      ok(saveItem.selected, "The 'save' option should now be selected");
    },
    expectTab: false,
    expectLaunch: false,
    expectedPreferredAction: saveToDisk,
    expectedAsk: true,
  },
  {
    description:
      "Opening externally when asking is the default should change persisted default.",
    preDialogState: { preferredAction: handleInternally, alwaysAsk: true },
    dialogActions(doc) {
      let openItem = doc.querySelector("#open");
      openItem.click();
      ok(openItem.selected, "The 'save' option should now be selected");
    },
    expectTab: false,
    expectLaunch: true,
    expectedPreferredAction: useSystemDefault,
    expectedAsk: true,
  },
];

function ensureMIMEState({ preferredAction, alwaysAsk }) {
  const mimeInfo = gMimeSvc.getFromTypeAndExtension("application/pdf", "pdf");
  mimeInfo.preferredAction = preferredAction;
  mimeInfo.alwaysAskBeforeHandling = alwaysAsk;
  gHandlerSvc.store(mimeInfo);
}

/**
 * Test that if we have PDFs set to handle internally, and the user chooses to
 * do something else with it, we do not alter the saved state.
 */
add_task(async function test_check_saving_handler_choices() {
  let publicList = await Downloads.getList(Downloads.PUBLIC);
  registerCleanupFunction(async () => {
    await publicList.removeFinished();
  });
  for (let testCase of kTestCases) {
    let file = "file_pdf_application_pdf.pdf";
    info("Testing with " + file + "; " + testCase.description);
    ensureMIMEState(testCase.preDialogState);

    let dialogWindowPromise = BrowserTestUtils.domWindowOpenedAndLoaded();
    let loadingTab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      TEST_PATH + file
    );
    let dialogWindow = await dialogWindowPromise;
    is(
      dialogWindow.location.href,
      "chrome://mozapps/content/downloads/unknownContentType.xhtml",
      "Should have seen the unknown content dialogWindow."
    );
    let doc = dialogWindow.document;
    let internalHandlerRadio = doc.querySelector("#handleInternally");

    await waitForAcceptButtonToGetEnabled(doc);

    ok(!internalHandlerRadio.hidden, "The option should be visible for PDF");
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

    let dialog = doc.querySelector("#unknownContentType");
    dialog.acceptDialog();

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
    const mimeInfo = gMimeSvc.getFromTypeAndExtension("application/pdf", "pdf");
    gHandlerSvc.fillHandlerInfo(mimeInfo, "");
    is(
      mimeInfo.preferredAction,
      testCase.expectedPreferredAction,
      "preferredAction - " + description
    );
    is(
      mimeInfo.alwaysAskBeforeHandling,
      testCase.expectedAsk,
      "alwaysAsk - " + description
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
