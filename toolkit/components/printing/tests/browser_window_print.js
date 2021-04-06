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
  // window.print() only shows print preview when print.tab_modal.enabled is
  // true.
  await SpecialPowers.pushPrefEnv({
    set: [["print.tab_modal.enabled", true]],
  });

  is(
    document.querySelector(".printPreviewBrowser"),
    null,
    "There shouldn't be any print preview browser"
  );

  await BrowserTestUtils.withNewTab(
    `${TEST_PATH}file_window_print.html`,
    async function(browser) {
      info(
        "Waiting for the first window.print() to run and ensure we're showing the preview..."
      );

      await BrowserTestUtils.waitForCondition(
        () => !!document.querySelector(".printPreviewBrowser")
      );

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

      await BrowserTestUtils.waitForCondition(
        () => !!document.querySelector(".printPreviewBrowser")
      );

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
  // window.print() only shows print preview when print.tab_modal.enabled is
  // true.
  await SpecialPowers.pushPrefEnv({
    set: [["print.tab_modal.enabled", true]],
  });

  is(
    document.querySelector(".printPreviewBrowser"),
    null,
    "There shouldn't be any print preview browser"
  );

  await BrowserTestUtils.withNewTab(
    `${TEST_PATH}file_window_print_delayed_during_load.html`,
    async function(browser) {
      info(
        "Waiting for the first window.print() to run and ensure we're showing the preview..."
      );

      await BrowserTestUtils.waitForCondition(
        () => !!document.querySelector(".printPreviewBrowser")
      );

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
  // window.print() only shows print preview when print.tab_modal.enabled is
  // true.
  await SpecialPowers.pushPrefEnv({
    set: [["print.tab_modal.enabled", true]],
  });

  is(
    document.querySelector(".printPreviewBrowser"),
    null,
    "There shouldn't be any print preview browser"
  );

  await BrowserTestUtils.withNewTab(
    `${TEST_PATH}file_window_print_sandboxed_iframe.html`,
    async function(browser) {
      info(
        "Waiting for the first window.print() to run and ensure we're showing the preview..."
      );

      await BrowserTestUtils.waitForCondition(
        () => !!document.querySelector(".printPreviewBrowser")
      );

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
  await SpecialPowers.pushPrefEnv({
    set: [["print.tab_modal.enabled", true]],
  });

  is(
    document.querySelector(".printPreviewBrowser"),
    null,
    "There shouldn't be any print preview browser"
  );

  await BrowserTestUtils.withNewTab(
    `${TEST_PATH}file_window_print_another_iframe_and_remove.html`,
    async function(browser) {
      let firstFrame = browser.browsingContext.children[0];
      info("Clicking on the button in the first iframe");
      BrowserTestUtils.synthesizeMouse("button", 0, 0, {}, firstFrame);

      info("Waiting for dialog");
      await BrowserTestUtils.waitForCondition(
        () => !!document.querySelector(".printPreviewBrowser")
      );

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
  await SpecialPowers.pushPrefEnv({
    set: [["print.tab_modal.enabled", true]],
  });

  for (const base of [TEST_PATH, TEST_PATH_SITE]) {
    const url = `${base}file_coop_header2.html`;
    is(
      document.querySelector(".printPreviewBrowser"),
      null,
      "There shouldn't be any print preview browser"
    );
    await BrowserTestUtils.withNewTab(url, async function(browser) {
      info("Waiting for dialog");
      await BrowserTestUtils.waitForCondition(
        () => !!document.querySelector(".printPreviewBrowser")
      );

      ok(true, "Shouldn't crash");
      gBrowser.getTabDialogBox(browser).abortAllDialogs();
    });
  }
});

// FIXME(emilio): This test doesn't use window.print(), why is it on this file?
add_task(async function test_focused_browsing_context() {
  await SpecialPowers.pushPrefEnv({
    set: [["print.tab_modal.enabled", true]],
  });

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

  let printButtonID = PanelUI.protonAppMenuEnabled
    ? "appMenu-print-button2"
    : "appMenu-print-button";

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
