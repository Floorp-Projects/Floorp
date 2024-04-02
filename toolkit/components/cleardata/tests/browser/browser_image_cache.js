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

// Images are cached per process, so we need to keep tabs open for the
// duration of a test.
let originToTabs = {};

function getTestURLForOrigin(origin) {
  return TEST_ROOT_DIR.replace("chrome://mochitests/content", origin);
}

async function testCached(origin, isCached, testPartitioned = false) {
  let tabs = originToTabs[origin];

  for (let tab of tabs) {
    // For the partition test we inspect the cache state of the iframe.
    let browsingContext = testPartitioned
      ? tab.linkedBrowser.browsingContext.children[0]
      : tab.linkedBrowser.browsingContext;
    let actualCached = await SpecialPowers.spawn(browsingContext, [], () => {
      let imgUrl = content.document.querySelector("img").src;
      let imageCache = SpecialPowers.Cc[
        "@mozilla.org/image/tools;1"
      ].getService(Ci.imgITools);
      let uri = SpecialPowers.Services.io.newURI(imgUrl);
      let properties = imageCache
        .getImgCacheForDocument(content.document)
        .findEntryProperties(uri, content.document);
      return !!properties;
    });

    let msg = `${origin}${isCached ? " " : " not "}cached`;
    if (testPartitioned) {
      msg = "Partitioned under " + msg;
    }

    is(actualCached, isCached, msg);
  }
}

/**
 * Creates tabs and loads images in first party and third party context.
 * @returns {Promise} - Promise which resolves once all tabs are initialized,
 * {@link originToTabs} is populated and (sub-)resources have loaded.
 */
function addTestTabs() {
  // Adding two tabs for ORIGIN_A so we can test clearing for a principal
  // cross-process (non-fission).
  let promises = [
    [ORIGIN_A, ORIGIN_B],
    [ORIGIN_A, ORIGIN_B],
    [ORIGIN_A_SUB, ORIGIN_B_SUB],
    [ORIGIN_A_HTTP, ORIGIN_B_HTTP],
    [ORIGIN_B, ORIGIN_A],
    [ORIGIN_B_SUB, ORIGIN_A_SUB],
    [ORIGIN_B_HTTP, ORIGIN_A_HTTP],
  ].map(async ([firstParty, thirdParty]) => {
    let urlFirstParty =
      getTestURLForOrigin(firstParty) + "file_image_cache.html";
    let urlThirdParty =
      getTestURLForOrigin(thirdParty) + "file_image_cache.html";

    info("Creating new tab for " + urlFirstParty);
    let tab = BrowserTestUtils.addTab(gBrowser, urlFirstParty);
    await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

    info("Creating iframe for " + urlThirdParty);
    await SpecialPowers.spawn(tab.linkedBrowser, [urlThirdParty], async url => {
      let iframe = content.document.createElement("iframe");
      iframe.src = url;

      let loadPromise = ContentTaskUtils.waitForEvent(iframe, "load", false);
      content.document.body.appendChild(iframe);
      await loadPromise;
    });

    let tabs = originToTabs[firstParty];
    if (!tabs) {
      tabs = [];
      originToTabs[firstParty] = tabs;
    }
    tabs.push(tab);
  });

  return Promise.all(promises);
}

function cleanup() {
  Object.values(originToTabs).flat().forEach(BrowserTestUtils.removeTab);
  originToTabs = {};
  let imageCache = Cc["@mozilla.org/image/tools;1"]
    .getService(Ci.imgITools)
    .getImgCacheForDocument(null);
  imageCache.clearCache(false);
  imageCache.clearCache(true);
}

add_setup(function () {
  cleanup();
});

add_task(async function test_deleteByPrincipal() {
  await SpecialPowers.setBoolPref("dom.security.https_first", false);
  await addTestTabs();

  // Clear data for content principal of A
  info("Clearing cache for principal " + ORIGIN_A);
  await new Promise(resolve => {
    Services.clearData.deleteDataFromPrincipal(
      Services.scriptSecurityManager.createContentPrincipalFromOrigin(ORIGIN_A),
      false,
      Ci.nsIClearDataService.CLEAR_IMAGE_CACHE,
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

  // No partitioned cache should have been cleared.
  await testCached(ORIGIN_A, true, true);
  await testCached(ORIGIN_A_SUB, true, true);
  await testCached(ORIGIN_A_HTTP, true, true);
  // TODO: ImageCacheCleaner deleteByPrincipal does not look at the cache key's
  // isolationKey and thus over-clears. In this case it clears cache for A
  // partitioned under B, even though the test principal does not set a
  // partitionKey.
  // See Bug 1713088.
  await testCached(ORIGIN_B, false, true);
  await testCached(ORIGIN_B_SUB, true, true);
  await testCached(ORIGIN_B_HTTP, true, true);

  cleanup();
});

add_task(async function test_deleteByBaseDomain() {
  await SpecialPowers.setBoolPref("dom.security.https_first", false);
  await addTestTabs();

  // Clear data for base domain of A.
  info("Clearing cache for base domain " + BASE_DOMAIN_A);
  await new Promise(resolve => {
    Services.clearData.deleteDataFromBaseDomain(
      BASE_DOMAIN_A,
      false,
      Ci.nsIClearDataService.CLEAR_IMAGE_CACHE,
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

  // All partitioned entries for B under A should have been cleared.
  await testCached(ORIGIN_A, false, true);
  await testCached(ORIGIN_A_SUB, false, true);
  await testCached(ORIGIN_A_HTTP, false, true);

  // All partitioned entries of A under B should have been cleared.
  await testCached(ORIGIN_B, false, true);
  await testCached(ORIGIN_B_SUB, false, true);
  await testCached(ORIGIN_B_HTTP, false, true);

  cleanup();
});

add_task(async function test_deleteAll() {
  await addTestTabs();

  // Clear the whole image cache across processes.
  info("Clearing cache for base domain " + BASE_DOMAIN_A);
  await new Promise(resolve => {
    Services.clearData.deleteData(
      Ci.nsIClearDataService.CLEAR_IMAGE_CACHE,
      resolve
    );
  });

  // All entries should have been cleared.
  await testCached(ORIGIN_A, false);
  await testCached(ORIGIN_A_SUB, false);
  await testCached(ORIGIN_A_HTTP, false);
  await testCached(ORIGIN_B, false);
  await testCached(ORIGIN_B_SUB, false);
  await testCached(ORIGIN_B_HTTP, false);

  // All partitioned entries should have been cleared.
  await testCached(ORIGIN_A, false, true);
  await testCached(ORIGIN_A_SUB, false, true);
  await testCached(ORIGIN_A_HTTP, false, true);
  await testCached(ORIGIN_B, false, true);
  await testCached(ORIGIN_B_SUB, false, true);
  await testCached(ORIGIN_B_HTTP, false, true);

  cleanup();
});
