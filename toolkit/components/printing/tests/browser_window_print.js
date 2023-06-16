/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

const TEST_PATH_SITE = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://test1.example.com"
);

add_task(async function test_print_blocks() {
  is(
    document.querySelector(".printPreviewBrowser"),
    null,
    "There shouldn't be any print preview browser"
  );

  await BrowserTestUtils.withNewTab(
    `${TEST_PATH}file_window_print.html`,
    async function (browser) {
      info(
        "Waiting for the first window.print() to run and ensure we're showing the preview..."
      );

      let helper = new PrintHelper(browser);
      await helper.waitForDialog();

      {
        let [before, afterFirst] = await SpecialPowers.spawn(
          browser,
          [],
          () => {
            return [
              !!content.document.getElementById("before-print"),
              !!content.document.getElementById("after-first-print"),
            ];
          }
        );

        ok(before, "Content before printing should be in the DOM");
        ok(!afterFirst, "Shouldn't have returned yet from window.print()");
      }

      gBrowser.getTabDialogBox(browser).abortAllDialogs();

      await helper.waitForDialog();

      {
        let [before, afterFirst, afterSecond] = await SpecialPowers.spawn(
          browser,
          [],
          () => {
            return [
              !!content.document.getElementById("before-print"),
              !!content.document.getElementById("after-first-print"),
              !!content.document.getElementById("after-second-print"),
            ];
          }
        );

        ok(before, "Content before printing should be in the DOM");
        ok(afterFirst, "Should be in the second print already");
        ok(afterSecond, "Shouldn't have blocked if we have mozPrintCallbacks");
      }
    }
  );
});

add_task(async function test_print_delayed_during_load() {
  is(
    document.querySelector(".printPreviewBrowser"),
    null,
    "There shouldn't be any print preview browser"
  );

  await BrowserTestUtils.withNewTab(
    `${TEST_PATH}file_window_print_delayed_during_load.html`,
    async function (browser) {
      info(
        "Waiting for the first window.print() to run and ensure we're showing the preview..."
      );

      let helper = new PrintHelper(browser);
      await helper.waitForDialog();

      // The print dialog is open, should be open after onload.
      {
        let duringLoad = await SpecialPowers.spawn(browser, [], () => {
          return !!content.document.getElementById("added-during-load");
        });
        ok(duringLoad, "Print should've been delayed");
      }

      gBrowser.getTabDialogBox(browser).abortAllDialogs();

      is(typeof browser.isConnected, "boolean");
      await BrowserTestUtils.waitForCondition(() => !browser.isConnected);
      ok(true, "Tab should've been closed after printing");
    }
  );
});

add_task(async function test_print_on_sandboxed_frame() {
  is(
    document.querySelector(".printPreviewBrowser"),
    null,
    "There shouldn't be any print preview browser"
  );

  await BrowserTestUtils.withNewTab(
    `${TEST_PATH}file_window_print_sandboxed_iframe.html`,
    async function (browser) {
      info(
        "Waiting for the first window.print() to run and ensure we're showing the preview..."
      );

      let helper = new PrintHelper(browser);
      await helper.waitForDialog();

      isnot(
        document.querySelector(".printPreviewBrowser"),
        null,
        "Should open the print preview correctly"
      );
      gBrowser.getTabDialogBox(browser).abortAllDialogs();
    }
  );
});

add_task(async function test_print_another_iframe_and_remove() {
  is(
    document.querySelector(".printPreviewBrowser"),
    null,
    "There shouldn't be any print preview browser"
  );

  await BrowserTestUtils.withNewTab(
    `${TEST_PATH}file_window_print_another_iframe_and_remove.html`,
    async function (browser) {
      let firstFrame = browser.browsingContext.children[0];
      info("Clicking on the button in the first iframe");
      BrowserTestUtils.synthesizeMouse("button", 0, 0, {}, firstFrame);

      await new PrintHelper(browser).waitForDialog();

      isnot(
        document.querySelector(".printPreviewBrowser"),
        null,
        "Should open the print preview correctly"
      );
      gBrowser.getTabDialogBox(browser).abortAllDialogs();
    }
  );
});

add_task(async function test_window_print_coop_site() {
  for (const base of [TEST_PATH, TEST_PATH_SITE]) {
    const url = `${base}file_coop_header2.html`;
    is(
      document.querySelector(".printPreviewBrowser"),
      null,
      "There shouldn't be any print preview browser"
    );
    await BrowserTestUtils.withNewTab(url, async function (browser) {
      await new PrintHelper(browser).waitForDialog();

      ok(true, "Shouldn't crash");
      gBrowser.getTabDialogBox(browser).abortAllDialogs();
    });
  }
});

add_task(async function test_window_print_iframe_remove_on_afterprint() {
  ok(
    !document.querySelector(".printPreviewBrowser"),
    "There shouldn't be any print preview browser"
  );
  await BrowserTestUtils.withNewTab(
    `${TEST_PATH}file_window_print_iframe_remove_on_afterprint.html`,
    async function (browser) {
      await new PrintHelper(browser).waitForDialog();
      let modalBefore = await SpecialPowers.spawn(browser, [], () => {
        return content.windowUtils.isInModalState();
      });

      ok(modalBefore, "The tab should be in modal state");

      // Clear the dialog.
      gBrowser.getTabDialogBox(browser).abortAllDialogs();

      let [modalAfter, hasIframe] = await SpecialPowers.spawn(
        browser,
        [],
        () => {
          return [
            content.windowUtils.isInModalState(),
            !!content.document.querySelector("iframe"),
          ];
        }
      );

      ok(!modalAfter, "Should've cleared the modal state properly");
      ok(!hasIframe, "Iframe should've been removed from the DOM");
    }
  );
});

// FIXME(emilio): This test doesn't use window.print(), why is it on this file?
add_task(async function test_focused_browsing_context() {
  await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    `${TEST_PATH}longerArticle.html`
  );

  let tabCount = gBrowser.tabs.length;
  document.getElementById("cmd_newNavigatorTab").doCommand();
  await TestUtils.waitForCondition(() => gBrowser.tabs.length == tabCount + 1);
  let newTabBrowser = gBrowser.selectedBrowser;
  is(newTabBrowser.documentURI.spec, "about:newtab", "newtab is loaded");

  let menuButton = document.getElementById("PanelUI-menu-button");
  menuButton.click();
  await BrowserTestUtils.waitForEvent(window.PanelUI.mainView, "ViewShown");

  let printButtonID = "appMenu-print-button2";

  document.getElementById(printButtonID).click();

  let dialog = await TestUtils.waitForCondition(
    () =>
      gBrowser
        .getTabDialogBox(newTabBrowser)
        .getTabDialogManager()
        ._dialogs.find(dlg => dlg._box.querySelector(".printSettingsBrowser")),
    "Wait for dialog"
  );
  await dialog._dialogReady;
  ok(dialog, "Dialog is available");
  await dialog._frame.contentWindow._initialized;
  await dialog.close();

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function test_print_with_oop_iframe() {
  is(
    document.querySelector(".printPreviewBrowser"),
    null,
    "There shouldn't be any print preview browser"
  );

  await BrowserTestUtils.withNewTab(
    `${TEST_PATH}file_window_print_oop_iframe.html`,
    async function (browser) {
      info(
        "Waiting for window.print() to run and ensure we're showing the preview..."
      );

      let helper = new PrintHelper(browser);
      await helper.waitForDialog();

      isnot(
        document.querySelector(".printPreviewBrowser"),
        null,
        "Should open the print preview correctly"
      );
      gBrowser.getTabDialogBox(browser).abortAllDialogs();
    }
  );
});

add_task(async function test_base_uri_srcdoc() {
  is(
    document.querySelector(".printPreviewBrowser"),
    null,
    "There shouldn't be any print preview browser"
  );

  const PARENT_URI = `${TEST_PATH}file_window_print_srcdoc_base_uri.html`;
  await BrowserTestUtils.withNewTab(PARENT_URI, async function (browser) {
    info(
      "Waiting for window.print() to run and ensure we're showing the preview..."
    );

    let helper = new PrintHelper(browser);
    await helper.waitForDialog();

    let previewBrowser = document.querySelector(".printPreviewBrowser");
    isnot(previewBrowser, null, "Should open the print preview correctly");

    let baseURI = await SpecialPowers.spawn(previewBrowser, [], () => {
      return content.document.baseURI;
    });

    is(baseURI, PARENT_URI, "srcdoc print document base uri should be right");

    gBrowser.getTabDialogBox(browser).abortAllDialogs();
  });
});

add_task(async function test_print_reentrant() {
  is(
    document.querySelector(".printPreviewBrowser"),
    null,
    "There shouldn't be any print preview browser"
  );

  const URI = `${TEST_PATH}file_window_print_reentrant.html`;
  await BrowserTestUtils.withNewTab(URI, async function (browser) {
    info(
      "Waiting for window.print() to run and ensure we're showing the preview..."
    );

    let helper = new PrintHelper(browser);
    await helper.waitForDialog();

    let previewBrowser = document.querySelector(".printPreviewBrowser");
    isnot(previewBrowser, null, "Should open the print preview correctly");

    gBrowser.getTabDialogBox(browser).abortAllDialogs();

    let count = await SpecialPowers.spawn(browser, [], () => {
      return parseInt(
        content.document.getElementById("before-print-count").innerText
      );
    });

    is(count, 1, "Should've fired beforeprint just once");
  });
});
