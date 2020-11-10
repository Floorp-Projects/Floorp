/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

async function createTab() {
  return BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: TEST_PAGE,
    waitForLoad: true,
  });
}

/**
 * Set pref for multiple PiP support first
 */
add_task(async () => {
  return SpecialPowers.pushPrefEnv({
    set: [["media.videocontrols.picture-in-picture.allow-multiple", "true"]],
  });
});

/**
 * Tests that multiple PiPs can be opened and closed in a single tab
 */
add_task(async () => {
  await BrowserTestUtils.withNewTab(
    {
      url: TEST_PAGE,
      gBrowser,
    },
    async browser => {
      let firstPip = await triggerPictureInPicture(browser, "with-controls");
      ok(firstPip, "Got first PiP window");

      let secondPip = await triggerPictureInPicture(browser, "no-controls");
      ok(secondPip, "Got second PiP window");

      await ensureMessageAndClosePiP(browser, "with-controls", firstPip, false);
      info("First PiP was still open and is now closed");

      await ensureMessageAndClosePiP(browser, "no-controls", secondPip, false);
      info("Second PiP was still open and is now closed");
    }
  );
});

/**
 * Tests that multiple PiPs can be opened and closed across different tabs
 */
add_task(async () => {
  let firstTab = await createTab();
  let secondTab = await createTab();

  gBrowser.selectedTab = firstTab;

  let firstPip = await triggerPictureInPicture(
    firstTab.linkedBrowser,
    "with-controls"
  );
  ok(firstPip, "Got first PiP window");

  gBrowser.selectedTab = secondTab;

  let secondPip = await triggerPictureInPicture(
    secondTab.linkedBrowser,
    "with-controls"
  );
  ok(secondPip, "Got second PiP window");

  await ensureMessageAndClosePiP(
    firstTab.linkedBrowser,
    "with-controls",
    firstPip,
    false
  );
  info("First Picture-in-Picture window was open and is now closed.");

  await ensureMessageAndClosePiP(
    secondTab.linkedBrowser,
    "with-controls",
    secondPip,
    false
  );
  info("Second Picture-in-Picture window was open and is now closed.");

  BrowserTestUtils.removeTab(firstTab);
  BrowserTestUtils.removeTab(secondTab);
});

/**
 * Tests that when a tab is closed; that only PiPs originating from this tab
 * are closed as well
 */
add_task(async () => {
  let firstTab = await createTab();
  let secondTab = await createTab();

  let firstPip = await triggerPictureInPicture(
    firstTab.linkedBrowser,
    "with-controls"
  );
  ok(firstPip, "Got first PiP window");

  let secondPip = await triggerPictureInPicture(
    secondTab.linkedBrowser,
    "with-controls"
  );
  ok(secondPip, "Got second PiP window");

  let firstClosed = BrowserTestUtils.domWindowClosed(firstPip);
  BrowserTestUtils.removeTab(firstTab);
  await firstClosed;
  info("First PiP closed after closing the first tab");

  await assertVideoIsBeingCloned(secondTab.linkedBrowser, "with-controls");
  info("Second PiP is still open after first tab close");

  let secondClosed = BrowserTestUtils.domWindowClosed(secondPip);
  BrowserTestUtils.removeTab(secondTab);
  await secondClosed;
  info("Second PiP closed after closing the second tab");
});
