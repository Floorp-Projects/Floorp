/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  getMostRecentUrls:
    "resource://gre/modules/contentrelevancy/private/InputUtils.sys.mjs",
  getTopFrecentUrls:
    "resource://gre/modules/contentrelevancy/private/InputUtils.sys.mjs",
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
  PlacesTestUtils: "resource://testing-common/PlacesTestUtils.sys.mjs",
});

const FRECENCY_SCORE_FOR_ONE_VISIT = 100;
const TEST_VISITS = [
  "http://test-1.com/",
  "http://test-2.com/",
  "http://test-3.com/",
  "http://test-4.com/",
];

add_task(async function test_GetTopFrecentUrls() {
  await PlacesUtils.history.clear();
  let urls = new Set(await getTopFrecentUrls(3, FRECENCY_SCORE_FOR_ONE_VISIT));

  Assert.strictEqual(urls.size, 0, "Should have no top frecent links.");

  await PlacesTestUtils.addVisits(TEST_VISITS);
  urls = new Set(await getTopFrecentUrls(3, FRECENCY_SCORE_FOR_ONE_VISIT));

  Assert.strictEqual(urls.size, 3, "Should fetch the expected links");
  urls.forEach(url => {
    Assert.ok(TEST_VISITS.includes(url), "Should be a link of the test visits");
  });
});

add_task(async function test_GetMostRecentUrls() {
  await PlacesUtils.history.clear();
  let urls = new Set(await getMostRecentUrls(3));

  Assert.strictEqual(urls.size, 0, "Should have no recent links.");

  // Add visits and page mete data.
  await PlacesTestUtils.addVisits(TEST_VISITS);
  for (let url of TEST_VISITS) {
    await PlacesUtils.history.update({
      description: "desc",
      previewImageURL: "https://image/",
      url,
    });
  }

  urls = new Set(await getMostRecentUrls(3));

  Assert.strictEqual(urls.size, 3, "Should fetch the expected links");
  urls.forEach(url => {
    Assert.ok(TEST_VISITS.includes(url), "Should be a link of the test visits");
  });
});
