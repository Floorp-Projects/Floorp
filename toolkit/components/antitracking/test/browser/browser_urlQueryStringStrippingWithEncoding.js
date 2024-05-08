/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_THIRD_PARTY_DOMAIN = TEST_DOMAIN_2;
const TEST_URI = TEST_DOMAIN + TEST_PATH + "file_stripping.html";
const TEST_THIRD_PARTY_URI =
  TEST_THIRD_PARTY_DOMAIN + TEST_PATH + "file_stripping.html";

async function verifyQueryString(browser, expected) {
  await SpecialPowers.spawn(browser, [expected], expected => {
    // Strip the first question mark.
    let search = content.location.search.slice(1);

    is(search, expected, "The query string is correct.");
  });
}

let listService;

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.query_stripping.strip_list", "paramToStrip1 paramToStrip2"],
      ["privacy.query_stripping.redirect", true],
      ["privacy.query_stripping.enabled", true],
      ["privacy.query_stripping.listService.logLevel", "Debug"],
      ["privacy.query_stripping.strip_on_share.enabled", false],
    ],
  });

  // Get the list service so we can wait for it to be fully initialized before running tests.
  listService = Cc["@mozilla.org/query-stripping-list-service;1"].getService(
    Ci.nsIURLQueryStrippingListService
  );
  // Here we don't care about the actual enabled state, we just want any init to be done so we get reliable starting conditions.
  await listService.testWaitForInit();
});

add_task(async function testRedirectWithStrippingMultipleTimes() {
  info(
    "Start testing query stripping for redirect link with multiple query paramaters"
  );

  const NESTED_QUERY = "paramToStrip1=123&paramToKeep=123";
  const NESTED_QUERY_STRIPPED = "paramToKeep=123";
  const INITIAL_QUERY = "paramToStrip2=123";

  let encodedURI = encodeURIComponent(
    `${TEST_THIRD_PARTY_URI}?${NESTED_QUERY}`
  );

  let testThirdPartyURI = `${TEST_URI}?redirect=${encodedURI}&${INITIAL_QUERY}`;
  let testThirdPartyURIStrippedQuery = `redirect=${encodedURI}`;
  let targetURI = `${TEST_THIRD_PARTY_URI}?${NESTED_QUERY_STRIPPED}`;

  // 1. Open a new tab with the redirect link
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    testThirdPartyURI
  );
  let browser = tab.linkedBrowser;

  // 2. Ensure the initial query parameter is stripped before the redirect
  await verifyQueryString(browser, testThirdPartyURIStrippedQuery);

  // Create the promise to wait for the location change.
  let targetURLLoadedPromise = BrowserTestUtils.browserLoaded(
    browser,
    false,
    targetURI
  );

  // 4. Trigger redirect by decoding the embedded URI
  await SpecialPowers.spawn(browser, [], async function () {
    let url = new URL(content.location);
    let value = url.searchParams.get("redirect");
    let decodedValue = decodeURIComponent(value);

    content.location.href = decodedValue;
  });

  // 5. Wait for the location change
  await targetURLLoadedPromise;

  // 6. Verify that the query parameters in the nested link have been stripped
  await verifyQueryString(browser, NESTED_QUERY_STRIPPED);

  BrowserTestUtils.removeTab(tab);
});
