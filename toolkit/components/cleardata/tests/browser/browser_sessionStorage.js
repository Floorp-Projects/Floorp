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

// Session storage is only valid for the lifetime of a tab, so we need to keep
// tabs open for the duration of a test.
let originToTabs = {};

function getTestURLForOrigin(origin) {
  return TEST_ROOT_DIR.replace("chrome://mochitests/content", origin);
}

function getTestEntryName(origin, partitioned) {
  return `${origin}${partitioned ? "_partitioned" : ""}`;
}

async function testHasEntry(origin, isSet, testPartitioned = false) {
  let tabs = originToTabs[origin];

  for (let tab of tabs) {
    // For the partition test we inspect the sessionStorage of the iframe.
    let browsingContext = testPartitioned
      ? tab.linkedBrowser.browsingContext.children[0]
      : tab.linkedBrowser.browsingContext;
    let actualSet = await SpecialPowers.spawn(
      browsingContext,
      [getTestEntryName(origin, testPartitioned)],
      key => {
        return !!content.sessionStorage.getItem(key);
      }
    );

    let msg = `${origin}${isSet ? " " : " not "}set`;
    if (testPartitioned) {
      msg = "Partitioned under " + msg;
    }

    is(actualSet, isSet, msg);
  }
}

/**
 * Creates tabs and sets sessionStorage entries in first party and third party
 * context.
 * @returns {Promise} - Promise which resolves once all tabs are initialized,
 * {@link originToTabs} is populated and (sub-)resources have loaded.
 */
function addTestTabs() {
  let promises = [
    [ORIGIN_A, ORIGIN_B],
    [ORIGIN_A_SUB, ORIGIN_B_SUB],
    [ORIGIN_A_HTTP, ORIGIN_B_HTTP],
    [ORIGIN_B, ORIGIN_A],
    [ORIGIN_B_SUB, ORIGIN_A_SUB],
    [ORIGIN_B_HTTP, ORIGIN_A_HTTP],
  ].map(async ([firstParty, thirdParty]) => {
    info("Creating new tab for " + firstParty);
    let tab = BrowserTestUtils.addTab(gBrowser, firstParty);
    await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

    info("Creating iframe for " + thirdParty);
    await SpecialPowers.spawn(
      tab.linkedBrowser,
      [getTestEntryName(firstParty, false), thirdParty],
      async (storageKey, url) => {
        // Set unpartitioned sessionStorage for firstParty.
        content.sessionStorage.setItem(storageKey, "foo");

        let iframe = content.document.createElement("iframe");
        iframe.src = url;

        let loadPromise = ContentTaskUtils.waitForEvent(iframe, "load", false);
        content.document.body.appendChild(iframe);
        await loadPromise;
      }
    );

    await SpecialPowers.spawn(
      tab.linkedBrowser.browsingContext.children[0],
      [getTestEntryName(thirdParty, true)],
      async storageKey => {
        // Set sessionStorage in partitioned third-party iframe.
        content.sessionStorage.setItem(storageKey, "foo");
      }
    );

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
  Object.values(originToTabs)
    .flat()
    .forEach(BrowserTestUtils.removeTab);
  originToTabs = {};
  Services.obs.notifyObservers(null, "browser:purge-sessionStorage");
}

add_task(function setup() {
  cleanup();
});

add_task(async function test_deleteByBaseDomain() {
  await addTestTabs();

  // Clear data for base domain of A.
  info("Clearing sessionStorage for base domain " + BASE_DOMAIN_A);
  await new Promise(resolve => {
    Services.clearData.deleteDataFromBaseDomain(
      BASE_DOMAIN_A,
      false,
      Ci.nsIClearDataService.CLEAR_DOM_QUOTA,
      resolve
    );
  });

  // All entries for A should have been cleared.
  await testHasEntry(ORIGIN_A, false);
  await testHasEntry(ORIGIN_A_SUB, false);
  await testHasEntry(ORIGIN_A_HTTP, false);
  // Entries for B should still exist.
  await testHasEntry(ORIGIN_B, true);
  await testHasEntry(ORIGIN_B_SUB, true);
  await testHasEntry(ORIGIN_B_HTTP, true);

  // All partitioned entries for B under A should have been cleared.
  await testHasEntry(ORIGIN_A, false, true);
  await testHasEntry(ORIGIN_A_SUB, false, true);
  await testHasEntry(ORIGIN_A_HTTP, false, true);

  // All partitioned entries of A under B should have been cleared.
  await testHasEntry(ORIGIN_B, false, true);
  await testHasEntry(ORIGIN_B_SUB, false, true);
  await testHasEntry(ORIGIN_B_HTTP, false, true);

  cleanup();
});

add_task(async function test_deleteAll() {
  await addTestTabs();

  // Clear all sessionStorage.
  info("Clearing sessionStorage for base domain " + BASE_DOMAIN_A);
  await new Promise(resolve => {
    Services.clearData.deleteData(
      Ci.nsIClearDataService.CLEAR_DOM_QUOTA,
      resolve
    );
  });

  // All entries should have been cleared.
  await testHasEntry(ORIGIN_A, false);
  await testHasEntry(ORIGIN_A_SUB, false);
  await testHasEntry(ORIGIN_A_HTTP, false);
  await testHasEntry(ORIGIN_B, false);
  await testHasEntry(ORIGIN_B_SUB, false);
  await testHasEntry(ORIGIN_B_HTTP, false);

  // All partitioned entries should have been cleared.
  await testHasEntry(ORIGIN_A, false, true);
  await testHasEntry(ORIGIN_A_SUB, false, true);
  await testHasEntry(ORIGIN_A_HTTP, false, true);
  await testHasEntry(ORIGIN_B, false, true);
  await testHasEntry(ORIGIN_B_SUB, false, true);
  await testHasEntry(ORIGIN_B_HTTP, false, true);

  cleanup();
});
