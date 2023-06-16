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

async function testHasEntry(originFirstParty, isSet, originThirdParty) {
  let tabs = originToTabs[originFirstParty];

  for (let tab of tabs) {
    // For the partition test we inspect the sessionStorage of the iframe.
    let browsingContext = originThirdParty
      ? tab.linkedBrowser.browsingContext.children[0]
      : tab.linkedBrowser.browsingContext;
    let actualSet = await SpecialPowers.spawn(
      browsingContext,
      [
        getTestEntryName(
          originThirdParty || originFirstParty,
          !!originThirdParty
        ),
      ],
      key => {
        return !!content.sessionStorage.getItem(key);
      }
    );

    let msg = `${originFirstParty}${isSet ? " " : " not "}set`;
    if (originThirdParty) {
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
  Object.values(originToTabs).flat().forEach(BrowserTestUtils.removeTab);
  originToTabs = {};
  Services.obs.notifyObservers(null, "browser:purge-sessionStorage");
}

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["network.cookie.cookieBehavior", 5]],
  });
  cleanup();
});

add_task(async function test_deleteByBaseDomain() {
  await addTestTabs();

  info("Clearing sessionStorage for base domain A " + BASE_DOMAIN_A);
  await new Promise(resolve => {
    Services.clearData.deleteDataFromBaseDomain(
      BASE_DOMAIN_A,
      false,
      Ci.nsIClearDataService.CLEAR_DOM_QUOTA,
      resolve
    );
  });

  info("All entries for A should have been cleared.");
  await testHasEntry(ORIGIN_A, false);
  await testHasEntry(ORIGIN_A_SUB, false);
  await testHasEntry(ORIGIN_A_HTTP, false);

  info("Entries for B should still exist.");
  await testHasEntry(ORIGIN_B, true);
  await testHasEntry(ORIGIN_B_SUB, true);
  await testHasEntry(ORIGIN_B_HTTP, true);

  info("All partitioned entries for B under A should have been cleared.");
  await testHasEntry(ORIGIN_A, false, ORIGIN_B);
  await testHasEntry(ORIGIN_A_SUB, false, ORIGIN_B_SUB);
  await testHasEntry(ORIGIN_A_HTTP, false, ORIGIN_B_HTTP);

  info("All partitioned entries of A under B should have been cleared.");
  await testHasEntry(ORIGIN_B, false, ORIGIN_A);
  await testHasEntry(ORIGIN_B_SUB, false, ORIGIN_A_SUB);
  await testHasEntry(ORIGIN_B_HTTP, false, ORIGIN_A_HTTP);

  cleanup();
});

add_task(async function test_deleteAll() {
  await addTestTabs();

  info("Clearing sessionStorage for base domain A " + BASE_DOMAIN_A);
  await new Promise(resolve => {
    Services.clearData.deleteData(
      Ci.nsIClearDataService.CLEAR_DOM_QUOTA,
      resolve
    );
  });

  info("All entries should have been cleared.");
  await testHasEntry(ORIGIN_A, false);
  await testHasEntry(ORIGIN_A_SUB, false);
  await testHasEntry(ORIGIN_A_HTTP, false);
  await testHasEntry(ORIGIN_B, false);
  await testHasEntry(ORIGIN_B_SUB, false);
  await testHasEntry(ORIGIN_B_HTTP, false);

  info("All partitioned entries should have been cleared.");
  await testHasEntry(ORIGIN_A, false, ORIGIN_B);
  await testHasEntry(ORIGIN_A_SUB, false, ORIGIN_B_SUB);
  await testHasEntry(ORIGIN_A_HTTP, false, ORIGIN_B_HTTP);
  await testHasEntry(ORIGIN_B, false, ORIGIN_A);
  await testHasEntry(ORIGIN_B_SUB, false, ORIGIN_A_SUB);
  await testHasEntry(ORIGIN_B_HTTP, false, ORIGIN_A_HTTP);

  cleanup();
});

add_task(async function test_deleteFromPrincipal() {
  await addTestTabs();

  info("Clearing sessionStorage for partitioned principal A " + BASE_DOMAIN_A);

  let principalA = Services.scriptSecurityManager.createContentPrincipal(
    Services.io.newURI(ORIGIN_A),
    { partitionKey: `(https,${BASE_DOMAIN_B})` }
  );

  info("principal: " + principalA.origin);
  info("principal partitionKey " + principalA.originAttributes.partitionKey);
  await new Promise(resolve => {
    Services.clearData.deleteDataFromPrincipal(
      principalA,
      false,
      Ci.nsIClearDataService.CLEAR_DOM_QUOTA,
      resolve
    );
  });

  info("Unpartitioned entries should still exist.");
  await testHasEntry(ORIGIN_A, true);
  await testHasEntry(ORIGIN_A_SUB, true);
  await testHasEntry(ORIGIN_A_HTTP, true);
  await testHasEntry(ORIGIN_B, true);
  await testHasEntry(ORIGIN_B_SUB, true);
  await testHasEntry(ORIGIN_B_HTTP, true);

  info("Only entries of principal should have been cleared.");
  await testHasEntry(ORIGIN_A, true, ORIGIN_B);
  await testHasEntry(ORIGIN_A_SUB, true, ORIGIN_B_SUB);
  await testHasEntry(ORIGIN_A_HTTP, true, ORIGIN_B_HTTP);

  await testHasEntry(ORIGIN_B, false, ORIGIN_A);

  await testHasEntry(ORIGIN_B_SUB, true, ORIGIN_A_SUB);
  await testHasEntry(ORIGIN_B_HTTP, true, ORIGIN_A_HTTP);

  cleanup();
});
