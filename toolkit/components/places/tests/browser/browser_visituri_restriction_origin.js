/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

const TEST_PAGE = `data:text/html,
 <a href="https://example.com/" target="_blank">https://example.com</a>
 <a href="http://example.com/" target="_blank">http://example.com</a>
 <a href="https://www.example.com/" target="_blank">https://www.example.com</a>
 <a href="https://example.org/" target="_blank">https://example.org</a>`;

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      // Enable restriction feature.
      ["places.history.floodingPrevention.enabled", true],
      // Restrict from the second visit.
      ["places.history.floodingPrevention.restrictionCount", 1],
      // Stop expiring.
      ["places.history.floodingPrevention.restrictionExpireSeconds", 100],
      // Always appply flooding preveition.
      [
        "places.history.floodingPrevention.maxSecondsFromLastUserInteraction",
        0,
      ],
      // To enable UserActivation by EventUtils.synthesizeMouseAtCenter() in ContentTask.spawn() in synthesizeVisitByUser().
      ["test.events.async.enabled", true],
    ],
  });
});

add_task(async function same_origin_but_other() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: TEST_PAGE },
    async browser => {
      info("Sanity check");
      await assertLinkVisitedStatus(browser, "https://example.com/", {
        visitCount: 0,
        isVisited: false,
      });
      // eslint-disable-next-line @microsoft/sdl/no-insecure-url
      await assertLinkVisitedStatus(browser, "http://example.com/", {
        visitCount: 0,
        isVisited: false,
      });
      await assertLinkVisitedStatus(browser, "https://www.example.com/", {
        visitCount: 0,
        isVisited: false,
      });
      await assertLinkVisitedStatus(browser, "https://example.org/", {
        visitCount: 0,
        isVisited: false,
      });

      info("Visit https://example.com/ by user");
      await synthesizeVisitByUser(browser, "https://example.com/");
      await assertLinkVisitedStatus(browser, "https://example.com/", {
        visitCount: 1,
        isVisited: true,
      });

      info("Visit others by Scripts");
      // eslint-disable-next-line @microsoft/sdl/no-insecure-url
      await synthesizeVisitByScript(browser, "http://example.com/");
      await synthesizeVisitByScript(browser, "https://www.example.com/");
      await synthesizeVisitByScript(browser, "https://example.org/");

      info("Check the status");
      await assertLinkVisitedStatus(browser, "https://example.com/", {
        visitCount: 1,
        isVisited: true,
      });
      // eslint-disable-next-line @microsoft/sdl/no-insecure-url
      await assertLinkVisitedStatus(browser, "http://example.com/", {
        visitCount: 0,
        isVisited: true,
      });
      await assertLinkVisitedStatus(browser, "https://www.example.com/", {
        visitCount: 0,
        isVisited: true,
      });
      await assertLinkVisitedStatus(browser, "https://example.org/", {
        visitCount: 1,
        isVisited: true,
      });
    }
  );

  await clearHistoryAndHistoryCache();
});
