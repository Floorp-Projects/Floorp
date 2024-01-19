/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// This test verifies that the Save To Pocket button appears in reader mode,
// and is toggled hidden and visible when pocket is disabled and enabled.

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.com"
);

async function getPocketButtonsCount(browser) {
  return SpecialPowers.spawn(browser, [], () => {
    return content.document.getElementsByClassName("pocket-button").length;
  });
}

add_task(async function () {
  // set the pocket preference before beginning.
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.pocket.enabled", true]],
  });

  var readerButton = document.getElementById("reader-mode-button");

  let tab1 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_PATH + "readerModeArticleShort.html"
  );

  let promiseTabLoad = promiseTabLoadEvent(tab1);
  readerButton.click();
  await promiseTabLoad;

  let tab2 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_PATH + "readerModeArticleMedium.html"
  );

  promiseTabLoad = promiseTabLoadEvent(tab2);
  readerButton.click();
  await promiseTabLoad;

  is(
    await getPocketButtonsCount(tab1.linkedBrowser),
    1,
    "tab 1 has a pocket button"
  );
  is(
    await getPocketButtonsCount(tab1.linkedBrowser),
    1,
    "tab 2 has a pocket button"
  );

  // Turn off the pocket preference. The Save To Pocket buttons should disappear.
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.pocket.enabled", false]],
  });

  is(
    await getPocketButtonsCount(tab1.linkedBrowser),
    0,
    "tab 1 has no pocket button"
  );
  is(
    await getPocketButtonsCount(tab1.linkedBrowser),
    0,
    "tab 2 has no pocket button"
  );

  // Turn on the pocket preference. The Save To Pocket buttons should reappear again.
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.pocket.enabled", true]],
  });

  is(
    await getPocketButtonsCount(tab1.linkedBrowser),
    1,
    "tab 1 has a pocket button again"
  );
  is(
    await getPocketButtonsCount(tab1.linkedBrowser),
    1,
    "tab 2 has a pocket button again"
  );

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
});

/**
 * Test that the pocket button toggles the pocket popup successfully
 */
add_task(async function () {
  await BrowserTestUtils.withNewTab(
    TEST_PATH + "readerModeArticleShort.html",
    async function (browser) {
      let pageShownPromise = BrowserTestUtils.waitForContentEvent(
        browser,
        "AboutReaderContentReady"
      );
      let readerButton = document.getElementById("reader-mode-button");
      readerButton.click();
      await pageShownPromise;

      await SpecialPowers.spawn(browser, [], async function () {
        content.document.querySelector(".pocket-button").click();
      });
      let panel = gBrowser.selectedBrowser.ownerDocument.querySelector(
        "#customizationui-widget-panel"
      );
      await BrowserTestUtils.waitForMutationCondition(
        panel,
        { attributes: true },
        () => {
          return BrowserTestUtils.isVisible(panel);
        }
      );
      ok(BrowserTestUtils.isVisible(panel), "Panel buttons are visible");

      await SpecialPowers.spawn(browser, [], async function () {
        content.document.querySelector(".pocket-button").click();
      });
      await BrowserTestUtils.waitForMutationCondition(
        panel,
        { attributes: true },
        () => {
          return BrowserTestUtils.isHidden(panel);
        }
      );

      ok(BrowserTestUtils.isHidden(panel), "Panel buttons are hidden");
    }
  );
});
