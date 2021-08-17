/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use-strict";

/**
 * Tests that when receiving the "clear-site-data" header - with dFPI enabled -
 * we clear storage under the correct partition.
 */

const { SiteDataTestUtils } = ChromeUtils.import(
  "resource://testing-common/SiteDataTestUtils.jsm"
);

const HOST_A = "example.com";
const HOST_B = "example.org";
const ORIGIN_A = `https://${HOST_A}`;
const ORIGIN_B = `https://${HOST_B}`;
const CLEAR_SITE_DATA_PATH = `/${TEST_PATH}clearSiteData.sjs`;
const CLEAR_SITE_DATA_URL_ORIGIN_B = ORIGIN_B + CLEAR_SITE_DATA_PATH;
const CLEAR_SITE_DATA_URL_ORIGIN_A = ORIGIN_A + CLEAR_SITE_DATA_PATH;
const THIRD_PARTY_FRAME_ID_ORIGIN_B = "thirdPartyFrame";
const THIRD_PARTY_FRAME_ID_ORIGIN_A = "thirdPartyFrame2";
const STORAGE_KEY = "testKey";

// Skip localStorage tests when using legacy localStorage. The legacy
// localStorage implementation does not support clearing data by principal. See
// Bug 1688221, Bug 1688665.
let skipLocalStorageTests = !Services.prefs.getBoolPref("dom.storage.next_gen");

/**
 * Creates an iframe in the passed browser and waits for it to load.
 * @param {Browser} browser - Browser to create the frame in.
 * @param {String} src - Frame source url.
 * @param {String} id - Frame id.
 * @param {boolean} sandbox - Whether the frame should be sandboxed.
 * @returns {Promise} - Resolves once the frame has loaded.
 */
function createFrame(browser, src, id, sandbox) {
  return SpecialPowers.spawn(
    browser,
    [{ page: src, frameId: id, sandbox }],
    async function(obj) {
      await new content.Promise(resolve => {
        let frame = content.document.createElement("iframe");
        if (obj.sandbox) {
          frame.setAttribute("sandbox", "allow-scripts");
        }
        frame.src = obj.page;
        frame.id = obj.frameId;
        frame.addEventListener("load", resolve, { once: true });
        content.document.body.appendChild(frame);
      });
    }
  );
}

/**
 * Creates a new tab, loads a url and creates an iframe.
 * Callers need to clean up the tab before the test ends.
 * @param {String} firstPartyUrl - Url to load in tab.
 * @param {String} thirdPartyUrl - Url to load in frame.
 * @param {String} frameId - Id of iframe element.
 * @param {boolean} sandbox - Whether the frame should be sandboxed.
 * @returns {Promise} - Resolves with the tab and the frame BrowsingContext once
 * the tab and the frame have loaded.
 */
async function createTabWithFrame(
  firstPartyUrl,
  thirdPartyUrl,
  frameId,
  sandbox
) {
  // Create tab and wait for it to be loaded.
  let tab = BrowserTestUtils.addTab(gBrowser, firstPartyUrl);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  // Create cross origin iframe.
  await createFrame(tab.linkedBrowser, thirdPartyUrl, frameId, sandbox);

  // Return BrowsingContext of created iframe.
  return { tab, frameBC: tab.linkedBrowser.browsingContext.children[0] };
}

/**
 * Test wrapper for the ClearSiteData tests.
 * Loads ORIGIN_A and ORIGIN_B in two tabs and inserts a cross origin pointing
 * to the other iframe each.
 * Both frames ORIGIN_B under ORIGIN_A and ORIGIN_A under ORIGIN_B will be
 * storage partitioned.
 * Depending on the clearDataContext variable we then either navigate ORIGIN_A
 * (as top level) or ORIGIN_B (as third party frame) to the clear-site-data
 * endpoint.
 * @param {function} cbPreClear - Called after initial setup, once top levels
 * and frames have been loaded.
 * @param {function} cbPostClear - Called after data has been cleared via the
 * "Clear-Site-Data" header.
 * @param {("firstParty"|"thirdPartyPartitioned")} clearDataContext - Whether to
 * navigate to the path that sets the "Clear-Site-Data" header with the first or
 * third party.
 * @param {boolean} [sandboxFrame] - Whether the frames should be sandboxed. No
 * sandbox by default.
 */
async function runClearSiteDataTest(
  cbPreClear,
  cbPostClear,
  clearDataContext,
  sandboxFrame = false
) {
  // Create a tabs for origin A and B with cross origins frames B and A
  let [
    { frameBC: frameContextB, tab: tabA },
    { frameBC: frameContextA, tab: tabB },
  ] = await Promise.all([
    createTabWithFrame(
      ORIGIN_A,
      ORIGIN_B,
      THIRD_PARTY_FRAME_ID_ORIGIN_B,
      sandboxFrame
    ),
    createTabWithFrame(
      ORIGIN_B,
      ORIGIN_A,
      THIRD_PARTY_FRAME_ID_ORIGIN_A,
      sandboxFrame
    ),
  ]);

  let browserA = tabA.linkedBrowser;
  let contextA = browserA.browsingContext;
  let browserB = tabB.linkedBrowser;
  let contextB = browserB.browsingContext;

  // Run test callback before clear-site-data
  if (cbPreClear) {
    await cbPreClear(contextA, contextB, frameContextB, frameContextA);
  }

  // Navigate to path with clear-site-data header
  // Depending on the clearDataContext variable we either do this with the
  // top browser or the third party storage partitioned frame (B embedded in A).
  info(`Opening path with clear-site-data-header for ${clearDataContext}`);
  if (clearDataContext == "firstParty") {
    // Open in new tab so we keep our current test tab intact. The
    // post-clear-callback might need it.
    await BrowserTestUtils.withNewTab(CLEAR_SITE_DATA_URL_ORIGIN_A, () => {});
  } else if (clearDataContext == "thirdPartyPartitioned") {
    // Navigate frame to path with clear-site-data header
    await SpecialPowers.spawn(
      browserA,
      [
        {
          page: CLEAR_SITE_DATA_URL_ORIGIN_B,
          frameId: THIRD_PARTY_FRAME_ID_ORIGIN_B,
        },
      ],
      async function(obj) {
        await new content.Promise(resolve => {
          let frame = content.document.getElementById(obj.frameId);
          frame.addEventListener("load", resolve, { once: true });
          frame.src = obj.page;
        });
      }
    );
  } else {
    ok(false, "Invalid context requested for clear-site-data");
  }

  if (cbPostClear) {
    await cbPostClear(contextA, contextB, frameContextB, frameContextA);
  }

  info("Cleaning up.");
  BrowserTestUtils.removeTab(tabA);
  BrowserTestUtils.removeTab(tabB);
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, resolve);
  });
}

/**
 * Create an origin with partitionKey.
 * @param {String} originNoSuffix - Origin without origin attributes.
 * @param {String} [firstParty] - First party to create partitionKey.
 * @returns {String} Origin with suffix. If not passed this will return the
 * umodified origin.
 */
function getOrigin(originNoSuffix, firstParty) {
  let origin = originNoSuffix;
  if (firstParty) {
    let [scheme, host] = firstParty.split("://");
    origin += `^partitionKey=(${scheme},${host})`;
  }
  return origin;
}

/**
 * Sets a storage item for an origin.
 * @param {("cookie"|"localStorage")} storageType - Which storage type to use.
 * @param {String} originNoSuffix - Context to set storage item in.
 * @param {String} [firstParty] - Optional first party domain to partition
 * under.
 * @param {String} key - Key of the entry.
 * @param {String} value  - Value of the entry.
 */
function setStorageEntry(storageType, originNoSuffix, firstParty, key, value) {
  if (storageType != "cookie" && storageType != "localStorage") {
    ok(false, "Invalid storageType passed");
    return;
  }

  let origin = getOrigin(originNoSuffix, firstParty);

  if (storageType == "cookie") {
    SiteDataTestUtils.addToCookies({ origin, name: key, value });
    return;
  }
  // localStorage
  SiteDataTestUtils.addToLocalStorage(origin, key, value);
}

/**
 * Tests whether a host sets a cookie.
 * For the purpose of this test we assume that there is either one or no cookie
 * set.
 * This performs cookie lookups directly via the cookie service.
 * @param {boolean} hasCookie - Whether we expect to see a cookie.
 * @param {String} originNoSuffix - Origin the cookie is stored for.
 * @param {String|null} firstParty - Whether to test for a partitioned cookie.
 * If set this will be used to construct the partitionKey.
 * @param {String} [key] - Expected key / name of the cookie.
 * @param {String} [value] - Expected value of the cookie.
 */
function testHasCookie(hasCookie, originNoSuffix, firstParty, key, value) {
  let origin = getOrigin(originNoSuffix, firstParty);

  let label = `${originNoSuffix}${
    firstParty ? ` (partitioned under ${firstParty})` : ""
  }`;

  if (!hasCookie) {
    ok(
      !SiteDataTestUtils.hasCookies(origin),
      `Cookie for ${label} is not set for key ${key}`
    );
    return;
  }

  ok(
    SiteDataTestUtils.hasCookies(origin, [{ key, value }]),
    `Cookie for ${label} is set ${key}=${value}`
  );
}

/**
 * Tests whether a context has a localStorage entry.
 * @param {boolean} hasEntry - Whether we expect to see an entry.
 * @param {String} originNoSuffix - Origin to test localStorage for.
 * @param {String} [firstParty] - First party context to test under.
 * @param {String} key - key of the localStorage item.
 * @param {String} [expectedValue] - Expected value of the item.
 */
function testHasLocalStorageEntry(
  hasEntry,
  originNoSuffix,
  firstParty,
  key,
  expectedValue
) {
  if (key == null) {
    ok(false, "localStorage key is mandatory");
    return;
  }
  let label = `${originNoSuffix}${
    firstParty ? ` (partitioned under ${firstParty})` : ""
  }`;
  let origin = getOrigin(originNoSuffix, firstParty);
  if (hasEntry) {
    let hasEntry = SiteDataTestUtils.hasLocalStorage(origin, [
      { key, value: expectedValue },
    ]);
    ok(
      hasEntry,
      `localStorage for ${label} has expected value ${key}=${expectedValue}`
    );
  } else {
    let hasEntry = SiteDataTestUtils.hasLocalStorage(origin);
    ok(!hasEntry, `localStorage for ${label} is not set for key ${key}`);
  }
}

/**
 * Sets the initial storage entries used by the storage tests in this file.
 * 1. first party ( A )
 * 2. first party ( B )
 * 3. third party partitioned ( B under A)
 * 4. third party partitioned ( A under B)
 * The entry values reflect which context they are set for.
 * @param {("cookie"|"localStorage")} storageType - Storage type to initialize.
 */
async function setupInitialStorageState(storageType) {
  if (storageType != "cookie" && storageType != "localStorage") {
    ok(false, "Invalid storageType passed");
    return;
  }

  // Set a first party entry
  setStorageEntry(storageType, ORIGIN_A, null, STORAGE_KEY, "firstParty");

  // Set a storage entry in the storage partitioned third party frame
  setStorageEntry(
    storageType,
    ORIGIN_B,
    ORIGIN_A,
    STORAGE_KEY,
    "thirdPartyPartitioned"
  );

  // Set a storage entry in the non storage partitioned third party page
  setStorageEntry(storageType, ORIGIN_B, null, STORAGE_KEY, "thirdParty");

  // Set a storage entry in the second storage partitioned third party frame.
  setStorageEntry(
    storageType,
    ORIGIN_A,
    ORIGIN_B,
    STORAGE_KEY,
    "thirdPartyPartitioned2"
  );

  info("Test that storage entries are set for all contexts");

  if (storageType == "cookie") {
    testHasCookie(true, ORIGIN_A, null, STORAGE_KEY, "firstParty");
    testHasCookie(true, ORIGIN_B, null, STORAGE_KEY, "thirdParty");
    testHasCookie(
      true,
      ORIGIN_B,
      ORIGIN_A,
      STORAGE_KEY,
      "thirdPartyPartitioned"
    );
    testHasCookie(
      true,
      ORIGIN_A,
      ORIGIN_B,
      STORAGE_KEY,
      "thirdPartyPartitioned2"
    );
    return;
  }

  testHasLocalStorageEntry(true, ORIGIN_A, null, STORAGE_KEY, "firstParty");
  testHasLocalStorageEntry(true, ORIGIN_B, null, STORAGE_KEY, "thirdParty");
  testHasLocalStorageEntry(
    true,
    ORIGIN_B,
    ORIGIN_A,
    STORAGE_KEY,
    "thirdPartyPartitioned"
  );
  testHasLocalStorageEntry(
    true,
    ORIGIN_A,
    ORIGIN_B,
    STORAGE_KEY,
    "thirdPartyPartitioned2"
  );
}

add_task(async function setup() {
  info("Starting ClearSiteData test");

  await SpecialPowers.flushPrefEnv();
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.storage_access.enabled", true],
      [
        "network.cookie.cookieBehavior",
        Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
      ],
      [
        "network.cookie.cookieBehavior.pbmode",
        Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
      ],
      ["privacy.trackingprotection.enabled", false],
      ["privacy.trackingprotection.pbmode.enabled", false],
      // Needed for SiteDataTestUtils#hasLocalStorage
      ["dom.storage.client_validation", false],
    ],
  });
});

/**
 * Test clearing partitioned cookies via clear-site-data header
 * (Cleared via the cookie service).
 */

/**
 * Tests that when a storage partitioned third party frame loads a site with
 * "Clear-Site-Data", the cookies are cleared for only that partitioned frame.
 */
add_task(async function cookieClearThirdParty() {
  await runClearSiteDataTest(
    // Pre Clear-Site-Data
    () => setupInitialStorageState("cookie"),
    // Post Clear-Site-Data
    () => {
      info("Testing: First party cookie has not changed");
      testHasCookie(true, ORIGIN_A, null, STORAGE_KEY, "firstParty");
      info("Testing: Unpartitioned cookie has not changed");
      testHasCookie(true, ORIGIN_B, null, STORAGE_KEY, "thirdParty");
      info("Testing: Partitioned cookie for HOST_B (HOST_A) has been cleared");
      testHasCookie(false, ORIGIN_B, ORIGIN_A);
      info("Testing: Partitioned cookie for HOST_A (HOST_B) has not changed");
      testHasCookie(
        true,
        ORIGIN_A,
        ORIGIN_B,
        STORAGE_KEY,
        "thirdPartyPartitioned2"
      );
    },
    // Send clear-site-data header in partitioned third party context.
    "thirdPartyPartitioned"
  );
});

/**
 * Tests that when a sandboxed storage partitioned third party frame loads a
 * site with "Clear-Site-Data", no cookies are cleared and we don't crash.
 * Crash details in Bug 1686938.
 */
add_task(async function cookieClearThirdPartySandbox() {
  await runClearSiteDataTest(
    // Pre Clear-Site-Data
    () => setupInitialStorageState("cookie"),
    // Post Clear-Site-Data
    () => {
      info("Testing: First party cookie has not changed");
      testHasCookie(true, ORIGIN_A, null, STORAGE_KEY, "firstParty");
      info("Testing: Unpartitioned cookie has not changed");
      testHasCookie(true, ORIGIN_B, null, STORAGE_KEY, "thirdParty");
      info("Testing: Partitioned cookie for HOST_B (HOST_A) has not changed");
      testHasCookie(
        true,
        ORIGIN_B,
        ORIGIN_A,
        STORAGE_KEY,
        "thirdPartyPartitioned"
      );
      info("Testing: Partitioned cookie for HOST_A (HOST_B) has not changed");
      testHasCookie(
        true,
        ORIGIN_A,
        ORIGIN_B,
        STORAGE_KEY,
        "thirdPartyPartitioned2"
      );
    },
    // Send clear-site-data header in partitioned third party context.
    "thirdPartyPartitioned",
    true
  );
});

/**
 * Tests that when the we load a path with "Clear-Site-Data" at top level, the
 * cookies are cleared only in the first party context.
 */
add_task(async function cookieClearFirstParty() {
  await runClearSiteDataTest(
    // Pre Clear-Site-Data
    () => setupInitialStorageState("cookie"),
    // Post Clear-Site-Data
    () => {
      info("Testing: First party cookie has been cleared");
      testHasCookie(false, ORIGIN_A, null);
      info("Testing: Unpartitioned cookie has not changed");
      testHasCookie(true, ORIGIN_B, null, STORAGE_KEY, "thirdParty");
      info("Testing: Partitioned cookie for HOST_B (HOST_A) has not changed");
      testHasCookie(
        true,
        ORIGIN_B,
        ORIGIN_A,
        STORAGE_KEY,
        "thirdPartyPartitioned"
      );
      info("Testing: Partitioned cookie for HOST_A (HOST_B) has not changed");
      testHasCookie(
        true,
        ORIGIN_A,
        ORIGIN_B,
        STORAGE_KEY,
        "thirdPartyPartitioned2"
      );
    },
    // Send clear-site-data header in first party context.
    "firstParty"
  );
});

/**
 * Test clearing partitioned localStorage via clear-site-data header
 * (Cleared via the quota manager).
 */

/**
 * Tests that when a storage partitioned third party frame loads a site with
 * "Clear-Site-Data", localStorage is cleared for only that partitioned frame.
 */
add_task(async function localStorageClearThirdParty() {
  // Bug 1688221, Bug 1688665.
  if (skipLocalStorageTests) {
    info("Skipping test");
    return;
  }
  await runClearSiteDataTest(
    // Pre Clear-Site-Data
    () => setupInitialStorageState("localStorage"),
    // Post Clear-Site-Data
    async () => {
      info("Testing: First party localStorage has not changed");
      testHasLocalStorageEntry(true, ORIGIN_A, null, STORAGE_KEY, "firstParty");
      info("Testing: Unpartitioned localStorage has not changed");
      testHasLocalStorageEntry(true, ORIGIN_B, null, STORAGE_KEY, "thirdParty");
      info(
        "Testing: Partitioned localStorage for HOST_B (HOST_A) has been cleared"
      );
      testHasLocalStorageEntry(false, ORIGIN_B, ORIGIN_A, STORAGE_KEY);
      info(
        "Testing: Partitioned localStorage for HOST_A (HOST_B) has not changed"
      );
      testHasLocalStorageEntry(
        true,
        ORIGIN_A,
        ORIGIN_B,
        STORAGE_KEY,
        "thirdPartyPartitioned2"
      );
    },
    // Send clear-site-data header in partitioned third party context.
    "thirdPartyPartitioned"
  );
});

/**
 * Tests that when the we load a path with "Clear-Site-Data" at top level,
 * localStorage is cleared only in the first party context.
 */
add_task(async function localStorageClearFirstParty() {
  // Bug 1688221, Bug 1688665.
  if (skipLocalStorageTests) {
    info("Skipping test");
    return;
  }
  await runClearSiteDataTest(
    // Pre Clear-Site-Data
    () => setupInitialStorageState("localStorage"),
    // Post Clear-Site-Data
    () => {
      info("Testing: First party localStorage has been cleared");
      testHasLocalStorageEntry(false, ORIGIN_A, null, STORAGE_KEY);
      info("Testing: Unpartitioned thirdParty localStorage has not changed");
      testHasLocalStorageEntry(true, ORIGIN_B, null, STORAGE_KEY, "thirdParty");
      info(
        "Testing: Partitioned localStorage for HOST_B (HOST_A) has not changed"
      );
      testHasLocalStorageEntry(
        true,
        ORIGIN_B,
        ORIGIN_A,
        STORAGE_KEY,
        "thirdPartyPartitioned"
      );
      info(
        "Testing: Partitioned localStorage for HOST_A (HOST_B) has not changed"
      );
      testHasLocalStorageEntry(
        true,
        ORIGIN_A,
        ORIGIN_B,
        STORAGE_KEY,
        "thirdPartyPartitioned2"
      );
    },
    // Send clear-site-data header in first party context.
    "firstParty"
  );
});
