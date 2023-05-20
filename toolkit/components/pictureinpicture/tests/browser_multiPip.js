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

function getTelemetryMaxPipCount(resetMax = false) {
  const scalarData = Services.telemetry.getSnapshotForScalars(
    "main",
    resetMax
  ).parent;
  return scalarData["pictureinpicture.most_concurrent_players"];
}

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

  await assertVideoIsBeingCloned(secondTab.linkedBrowser, "#with-controls");
  info("Second PiP is still open after first tab close");

  let secondClosed = BrowserTestUtils.domWindowClosed(secondPip);
  BrowserTestUtils.removeTab(secondTab);
  await secondClosed;
  info("Second PiP closed after closing the second tab");
});

/**
 * Check that correct number of pip players are recorded for Telemetry
 * tracking
 */
add_task(async () => {
  // run this to flush recorded values from previous tests
  getTelemetryMaxPipCount(true);

  let firstTab = await createTab();
  let secondTab = await createTab();

  gBrowser.selectedTab = firstTab;

  let firstPip = await triggerPictureInPicture(
    firstTab.linkedBrowser,
    "with-controls"
  );
  ok(firstPip, "Got first PiP window");

  Assert.equal(
    getTelemetryMaxPipCount(true),
    1,
    "There should only be 1 PiP window"
  );

  let secondPip = await triggerPictureInPicture(
    firstTab.linkedBrowser,
    "no-controls"
  );
  ok(secondPip, "Got second PiP window");

  Assert.equal(
    getTelemetryMaxPipCount(true),
    2,
    "There should be 2 PiP windows"
  );

  await ensureMessageAndClosePiP(
    firstTab.linkedBrowser,
    "no-controls",
    secondPip,
    false
  );
  info("Second PiP was open and is now closed");

  gBrowser.selectedTab = secondTab;

  let thirdPip = await triggerPictureInPicture(
    secondTab.linkedBrowser,
    "with-controls"
  );
  ok(thirdPip, "Got third PiP window");

  let fourthPip = await triggerPictureInPicture(
    secondTab.linkedBrowser,
    "no-controls"
  );
  ok(fourthPip, "Got fourth PiP window");

  Assert.equal(
    getTelemetryMaxPipCount(false),
    3,
    "There should now be 3 PiP windows"
  );

  gBrowser.selectedTab = firstTab;

  await ensureMessageAndClosePiP(
    firstTab.linkedBrowser,
    "with-controls",
    firstPip,
    false
  );
  info("First PiP was open, it is now closed.");

  gBrowser.selectedTab = secondTab;

  await ensureMessageAndClosePiP(
    secondTab.linkedBrowser,
    "with-controls",
    thirdPip,
    false
  );
  info("Third PiP was open, it is now closed.");

  await ensureMessageAndClosePiP(
    secondTab.linkedBrowser,
    "no-controls",
    fourthPip,
    false
  );
  info("Fourth PiP was open, it is now closed.");

  Assert.equal(
    getTelemetryMaxPipCount(false),
    3,
    "Max PiP count should still be 3"
  );

  BrowserTestUtils.removeTab(firstTab);
  BrowserTestUtils.removeTab(secondTab);
});
