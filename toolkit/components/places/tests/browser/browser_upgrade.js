/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-disable @microsoft/sdl/no-insecure-url */

"use strict";

// This test checks that when a upgrade is happening through HTTPS-Only,
// only a history entry for the https url of the site is added visibly for
// the user, while the http version gets added as a hidden entry.

async function assertIsPlaceHidden(url, expectHidden) {
  const hidden = await PlacesTestUtils.getDatabaseValue(
    "moz_places",
    "hidden",
    { url }
  );
  Assert.ok(hidden !== undefined, `We should have saved a visit to ${url}`);
  Assert.equal(
    hidden,
    expectHidden ? 1 : 0,
    `Check if the visit to ${url} is hidden`
  );
}

async function assertVisitFromAndType(
  url,
  expectedFromVisitURL,
  expectedVisitType
) {
  const db = await PlacesUtils.promiseDBConnection();
  const rows = await db.execute(
    `SELECT v1.visit_type FROM moz_historyvisits v1
     JOIN moz_places p1 ON p1.id = v1.place_id
     WHERE p1.url = :url
     AND from_visit IN
       (SELECT v2.id FROM moz_historyvisits v2
        JOIN moz_places p2 ON p2.id = v2.place_id
        WHERE p2.url = :expectedFromVisitURL)
    `,
    { url, expectedFromVisitURL }
  );
  Assert.equal(
    rows.length,
    1,
    `There should be a single visit to ${url} with "from_visit" set to the visit id of ${expectedFromVisitURL}`
  );
  Assert.equal(
    rows[0].getResultByName("visit_type"),
    expectedVisitType,
    `The visit to ${url} should have a visit type of ${expectedVisitType}`
  );
}

function waitForVisitNotifications(urls) {
  return Promise.all(
    urls.map(url =>
      PlacesTestUtils.waitForNotification("page-visited", events =>
        events.some(e => e.url === url)
      )
    )
  );
}

add_task(async function test_upgrade() {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.security.https_only_mode", true]],
  });

  await PlacesUtils.history.clear();

  registerCleanupFunction(async function () {
    await PlacesUtils.history.clear();
  });

  const visitPromise = waitForVisitNotifications([
    "http://example.com/",
    "https://example.com/",
  ]);

  info("Opening http://example.com/");
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "http://example.com/",
      waitForLoad: true,
    },
    async browser => {
      Assert.equal(
        browser.currentURI.scheme,
        "https",
        "We should have been upgraded"
      );
      info("Waiting for page visits to reach places database");
      await visitPromise;
      info("Checking places database");
      await assertIsPlaceHidden("http://example.com/", true);
      await assertIsPlaceHidden("https://example.com/", false);
      await assertVisitFromAndType(
        "https://example.com/",
        "http://example.com/",
        Ci.nsINavHistoryService.TRANSITION_REDIRECT_PERMANENT
      );
    }
  );
});
