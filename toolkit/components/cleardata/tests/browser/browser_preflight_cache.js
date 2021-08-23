/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { SiteDataTestUtils } = ChromeUtils.import(
  "resource://testing-common/SiteDataTestUtils.jsm"
);

const uuidGenerator = Cc["@mozilla.org/uuid-generator;1"].getService(
  Ci.nsIUUIDGenerator
);

const ORIGIN_A = "http://example.net";
const ORIGIN_B = "http://example.org";

const PREFLIGHT_URL_PATH =
  "/browser/toolkit/components/cleardata/tests/browser/file_cors_preflight.sjs";

const PREFLIGHT_URL_A = ORIGIN_A + PREFLIGHT_URL_PATH;
const PREFLIGHT_URL_B = ORIGIN_B + PREFLIGHT_URL_PATH;

function testPreflightCached(browser, url, token, isCached) {
  return SpecialPowers.spawn(
    browser,
    [url, token, isCached],
    async (url, token, isCached) => {
      let response = await content.fetch(
        new content.Request(`${url}?token=${token}`, {
          mode: "cors",
          method: "GET",
          headers: [["x-test-header", "check"]],
        })
      );

      let expected = isCached ? "0" : "1";
      is(
        await response.text(),
        expected,
        `Preflight cache for ${url} ${isCached ? "HIT" : "MISS"}.`
      );
    }
  );
}

async function testDeleteAll(
  clearDataFlag,
  { deleteBy = "all", hasUserInput = false } = {}
) {
  await BrowserTestUtils.withNewTab("http://example.com", async browser => {
    let token = uuidGenerator.generateUUID().toString();

    // Populate the preflight cache.
    await testPreflightCached(browser, PREFLIGHT_URL_A, token, false);
    await testPreflightCached(browser, PREFLIGHT_URL_B, token, false);
    // Cache should be populated.
    await testPreflightCached(browser, PREFLIGHT_URL_A, token, true);
    await testPreflightCached(browser, PREFLIGHT_URL_B, token, true);

    await new Promise(resolve => {
      if (deleteBy == "principal") {
        Services.clearData.deleteDataFromPrincipal(
          browser.contentPrincipal,
          hasUserInput,
          clearDataFlag,
          value => {
            Assert.equal(value, 0);
            resolve();
          }
        );
      } else if (deleteBy == "baseDomain") {
        Services.clearData.deleteDataFromBaseDomain(
          browser.contentPrincipal.baseDomain,
          hasUserInput,
          clearDataFlag,
          value => {
            Assert.equal(value, 0);
            resolve();
          }
        );
      } else {
        Services.clearData.deleteData(clearDataFlag, value => {
          Assert.equal(value, 0);
          resolve();
        });
      }
    });

    // The preflight cache cleaner cannot delete by principal or baseDomain
    // (Bug 1727141). If this method is called, it will check whether the used
    // requested the clearing. If the user requested clearing, it will
    // over-clear (clear all data). If the request didn't come from the user,
    // for example from the PurgeTrackerService, it will not clear anything to
    // avoid clearing storage unrelated to the baseDomain or principal.
    let clearedAll = deleteBy == "all" || hasUserInput;

    // Cache should be cleared.
    await testPreflightCached(browser, PREFLIGHT_URL_A, token, !clearedAll);
    await testPreflightCached(browser, PREFLIGHT_URL_B, token, !clearedAll);
  });

  SiteDataTestUtils.clear();
}

add_task(async function test_deleteAll() {
  // The cleaner should be called when we target all cleaners, all cache
  // cleaners, or just the preflight cache.
  let {
    CLEAR_ALL,
    CLEAR_ALL_CACHES,
    CLEAR_PREFLIGHT_CACHE,
  } = Ci.nsIClearDataService;

  for (let flag of [CLEAR_ALL, CLEAR_ALL_CACHES, CLEAR_PREFLIGHT_CACHE]) {
    await testDeleteAll(flag);
  }
});

add_task(async function test_deleteByPrincipal() {
  // The cleaner should be called when we target all cleaners, all cache
  // cleaners, or just the preflight cache.
  let {
    CLEAR_ALL,
    CLEAR_ALL_CACHES,
    CLEAR_PREFLIGHT_CACHE,
  } = Ci.nsIClearDataService;

  for (let flag of [CLEAR_ALL, CLEAR_ALL_CACHES, CLEAR_PREFLIGHT_CACHE]) {
    for (let hasUserInput of [true, false]) {
      await testDeleteAll(flag, { deleteBy: "principal", hasUserInput });
    }
  }
});
