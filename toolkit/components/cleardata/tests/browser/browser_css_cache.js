/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const BASE_DOMAIN_A = "example.com";
const ORIGIN_A = `https://${BASE_DOMAIN_A}`;
const ORIGIN_A_HTTP = `http://${BASE_DOMAIN_A}`;
const ORIGIN_A_SUB = `https://test1.${BASE_DOMAIN_A}`;

const BASE_DOMAIN_B = "example.org";
const ORIGIN_B = `https://${BASE_DOMAIN_B}`;
const ORIGIN_B_HTTP = `http://${BASE_DOMAIN_B}`;
const ORIGIN_B_SUB = `https://test1.${BASE_DOMAIN_B}`;

const TEST_ROOT_DIR = getRootDirectory(gTestPath);

// Stylesheets are cached per process, so we need to keep tabs open for the
// duration of a test.
let tabs = {};

function getTestURLForOrigin(origin) {
  return (
    TEST_ROOT_DIR.replace("chrome://mochitests/content", origin) +
    "file_css_cache.html"
  );
}

async function testCached(origin, isCached) {
  let url = getTestURLForOrigin(origin);

  let numParsed;

  let tab = tabs[origin];
  let loadedPromise;
  if (!tab) {
    info("Creating new tab for " + url);
    tab = BrowserTestUtils.addTab(gBrowser, url);
    loadedPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
    tabs[origin] = tab;
  } else {
    loadedPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
    tab.linkedBrowser.reload();
  }
  await loadedPromise;

  numParsed = await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    return SpecialPowers.getDOMWindowUtils(content).parsedStyleSheets;
  });

  // Stylesheets is cached if numParsed is 0.
  is(!numParsed, isCached, `${origin} is${isCached ? " " : " not "}cached`);
}

async function addTestTabs() {
  await testCached(ORIGIN_A, false);
  await testCached(ORIGIN_A_SUB, false);
  await testCached(ORIGIN_A_HTTP, false);
  await testCached(ORIGIN_B, false);
  await testCached(ORIGIN_B_SUB, false);
  await testCached(ORIGIN_B_HTTP, false);
  // Test that the cache has been populated.
  await testCached(ORIGIN_A, true);
  await testCached(ORIGIN_A_SUB, true);
  await testCached(ORIGIN_A_HTTP, true);
  await testCached(ORIGIN_B, true);
  await testCached(ORIGIN_B_SUB, true);
  await testCached(ORIGIN_B_HTTP, true);
}

async function cleanupTestTabs() {
  Object.values(tabs).forEach(BrowserTestUtils.removeTab);
  tabs = {};
}

add_task(async function test_deleteByPrincipal() {
  await SpecialPowers.setBoolPref("dom.security.https_first", false);
  await addTestTabs();

  // Clear data for content principal of A
  info("Clearing cache for principal " + ORIGIN_A);
  await new Promise(resolve => {
    Services.clearData.deleteDataFromPrincipal(
      Services.scriptSecurityManager.createContentPrincipalFromOrigin(ORIGIN_A),
      false,
      Ci.nsIClearDataService.CLEAR_CSS_CACHE,
      resolve
    );
  });

  // Only the cache entry for ORIGIN_A should have been cleared.
  await testCached(ORIGIN_A, false);
  await testCached(ORIGIN_A_SUB, true);
  await testCached(ORIGIN_A_HTTP, true);
  await testCached(ORIGIN_B, true);
  await testCached(ORIGIN_B_SUB, true);
  await testCached(ORIGIN_B_HTTP, true);

  // Cleanup
  cleanupTestTabs();
  ChromeUtils.clearStyleSheetCache();
});

add_task(async function test_deleteByBaseDomain() {
  await addTestTabs();

  // Clear data for base domain of A.
  info("Clearing cache for base domain " + BASE_DOMAIN_A);
  await new Promise(resolve => {
    Services.clearData.deleteDataFromBaseDomain(
      BASE_DOMAIN_A,
      false,
      Ci.nsIClearDataService.CLEAR_CSS_CACHE,
      resolve
    );
  });

  // All entries for A should have been cleared.
  await testCached(ORIGIN_A, false);
  await testCached(ORIGIN_A_SUB, false);
  await testCached(ORIGIN_A_HTTP, false);
  // Entries for B should still exist.
  await testCached(ORIGIN_B, true);
  await testCached(ORIGIN_B_SUB, true);
  await testCached(ORIGIN_B_HTTP, true);

  // Cleanup
  cleanupTestTabs();
  ChromeUtils.clearStyleSheetCache();
});
