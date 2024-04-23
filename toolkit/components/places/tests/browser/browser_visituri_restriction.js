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
      ["places.history.floodingPrevention.restrictionExpireSeconds", 4],
      // Not apply flooding prevention until some seconds elapse after user
      // interaction begins.
      [
        "places.history.floodingPrevention.maxSecondsFromLastUserInteraction",
        4,
      ],
      // To enable UserActivation by EventUtils.synthesizeMouseAtCenter() in
      // ContentTask.spawn() in synthesizeVisitByUser().
      ["test.events.async.enabled", true],
    ],
  });
  await clearHistoryAndHistoryCache();
});

add_task(async function basic() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: TEST_PAGE },
    async browser => {
      info("Sanity check");
      await assertLinkVisitedStatus(browser, "https://example.com/", {
        visitCount: 0,
        isVisited: false,
      });
      await assertLinkVisitedStatus(browser, "https://example.com/", {
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

      info(
        "Visit https://example.com/ by script within maxSecondsFromLastUserInteraction"
      );
      await synthesizeVisitByScript(browser, "https://example.com/");
      await synthesizeVisitByScript(browser, "https://example.com/");
      await assertLinkVisitedStatus(browser, "https://example.com/", {
        visitCount: 3,
        isVisited: true,
      });

      await waitForPrefSeconds("maxSecondsFromLastUserInteraction");

      info(
        "Visit https://example.com/ by script without maxSecondsFromLastUserInteraction"
      );
      await synthesizeVisitByScript(browser, "https://example.com/");
      await assertLinkVisitedStatus(browser, "https://example.com/", {
        visitCount: 4,
        isVisited: true,
      });

      info("Visit again, but it should be restricted");
      await synthesizeVisitByScript(browser, "https://example.com/");
      await assertLinkVisitedStatus(browser, "https://example.com/", {
        visitCount: 4,
        isVisited: true,
      });

      info("Check other");
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
    }
  );

  await clearHistoryAndHistoryCache();
});

add_task(async function expireRestriction() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: TEST_PAGE },
    async browser => {
      info("Visit https://example.com/ by user");
      await synthesizeVisitByUser(browser, "https://example.com/");
      await assertLinkVisitedStatus(browser, "https://example.com/", {
        visitCount: 1,
        isVisited: true,
      });

      info(
        "Visit https://example.com/ by script within maxSecondsFromLastUserInteraction"
      );
      await synthesizeVisitByScript(browser, "https://example.com/");
      await synthesizeVisitByScript(browser, "https://example.com/");
      await assertLinkVisitedStatus(browser, "https://example.com/", {
        visitCount: 3,
        isVisited: true,
      });

      await waitForPrefSeconds("maxSecondsFromLastUserInteraction");

      info(
        "Visit https://example.com/ by script without maxSecondsFromLastUserInteraction"
      );
      await synthesizeVisitByScript(browser, "https://example.com/");
      await assertLinkVisitedStatus(browser, "https://example.com/", {
        visitCount: 4,
        isVisited: true,
      });

      await waitForPrefSeconds("restrictionExpireSeconds");

      info("Visit again, it should not be restricted");
      await synthesizeVisitByScript(browser, "https://example.com/");
      await assertLinkVisitedStatus(browser, "https://example.com/", {
        visitCount: 5,
        isVisited: true,
      });
    }
  );

  await clearHistoryAndHistoryCache();
});

add_task(async function userInputAlwaysAcceptable() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: TEST_PAGE },
    async browser => {
      info("Visit by user");
      await synthesizeVisitByUser(browser, "https://example.com/");
      await assertLinkVisitedStatus(browser, "https://example.com/", {
        visitCount: 1,
        isVisited: true,
      });
      await synthesizeVisitByUser(browser, "https://example.com/");
      await assertLinkVisitedStatus(browser, "https://example.com/", {
        visitCount: 2,
        isVisited: true,
      });

      await waitForPrefSeconds("maxSecondsFromLastUserInteraction");

      info("Visit by user input");
      await synthesizeVisitByUser(browser, "https://example.com/");
      await assertLinkVisitedStatus(browser, "https://example.com/", {
        visitCount: 3,
        isVisited: true,
      });
      await synthesizeVisitByUser(browser, "https://example.com/");
      await assertLinkVisitedStatus(browser, "https://example.com/", {
        visitCount: 4,
        isVisited: true,
      });
    }
  );

  await clearHistoryAndHistoryCache();
});

add_task(async function disable() {
  await SpecialPowers.pushPrefEnv({
    set: [["places.history.floodingPrevention.enabled", false]],
  });

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: TEST_PAGE },
    async browser => {
      info("Any visits are stored");
      for (let i = 0; i < 3; i++) {
        await synthesizeVisitByScript(browser, "https://example.com/");
        await assertLinkVisitedStatus(browser, "https://example.com/", {
          visitCount: i + 1,
          isVisited: true,
        });
      }
    }
  );

  await clearHistoryAndHistoryCache();
});

async function waitForPrefSeconds(pref) {
  info(`Wait until elapsing ${pref}`);
  return new Promise(r =>
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    setTimeout(
      r,
      Services.prefs.getIntPref(`places.history.floodingPrevention.${pref}`) *
        1000 +
        100
    )
  );
}
