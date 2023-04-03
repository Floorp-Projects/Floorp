/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);

const PIP_URLBAR_EVENTS = [
  {
    category: "pictureinpicture",
    method: "opened_method",
    object: "urlBar",
  },
];

add_task(async function test_urlbar_toggle_multiple_contexts() {
  await BrowserTestUtils.withNewTab(
    {
      url: TEST_PAGE_MULTIPLE_CONTEXTS,
      gBrowser,
    },
    async browser => {
      Services.telemetry.clearEvents();
      await ensureVideosReady(browser);
      await ensureVideosReady(browser.browsingContext.children[0]);

      await TestUtils.waitForCondition(
        () => PictureInPicture.getEligiblePipVideoCount(browser) === 2,
        "Waiting for videos to register"
      );

      let totalPipCount = PictureInPicture.getEligiblePipVideoCount(browser);
      is(totalPipCount, 2, "Total PiP count is 2");

      let pipUrlbarToggle = document.getElementById(
        "picture-in-picture-button"
      );
      ok(
        BrowserTestUtils.is_hidden(pipUrlbarToggle),
        "PiP urlbar toggle is hidden because there is more than 1 video"
      );

      // Remove one video from page so urlbar toggle will show
      await SpecialPowers.spawn(browser, [], async () => {
        let video = content.document.getElementById("with-controls");
        video.remove();
      });

      await BrowserTestUtils.waitForMutationCondition(
        pipUrlbarToggle,
        { attributeFilter: ["hidden"] },
        () => BrowserTestUtils.is_visible(pipUrlbarToggle)
      );

      ok(
        BrowserTestUtils.is_visible(pipUrlbarToggle),
        "PiP urlbar toggle is visible"
      );

      totalPipCount = PictureInPicture.getEligiblePipVideoCount(browser);
      is(totalPipCount, 1, "Total PiP count is 1");

      let domWindowOpened = BrowserTestUtils.domWindowOpenedAndLoaded(null);
      pipUrlbarToggle.click();
      let win = await domWindowOpened;
      ok(win, "A Picture-in-Picture window opened.");

      await assertVideoIsBeingCloned(
        browser.browsingContext.children[0],
        "video"
      );

      let filter = {
        category: "pictureinpicture",
        method: "opened_method",
        object: "urlBar",
      };
      await waitForTelemeryEvents(filter, PIP_URLBAR_EVENTS.length, "content");

      TelemetryTestUtils.assertEvents(PIP_URLBAR_EVENTS, filter, {
        clear: true,
        process: "content",
      });

      let domWindowClosed = BrowserTestUtils.domWindowClosed(win);
      pipUrlbarToggle.click();
      await domWindowClosed;

      await SpecialPowers.spawn(browser, [], async () => {
        let iframe = content.document.getElementById("iframe");
        iframe.remove();
      });

      await BrowserTestUtils.waitForMutationCondition(
        pipUrlbarToggle,
        { attributeFilter: ["hidden"] },
        () => BrowserTestUtils.is_hidden(pipUrlbarToggle)
      );

      ok(
        BrowserTestUtils.is_hidden(pipUrlbarToggle),
        "PiP urlbar toggle is hidden because there are no videos on the page"
      );

      totalPipCount = PictureInPicture.getEligiblePipVideoCount(browser);
      is(totalPipCount, 0, "Total PiP count is 0");
    }
  );
});
