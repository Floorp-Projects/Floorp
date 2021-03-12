"use strict";

/* import-globals-from antitracking_head.js */

const TEST_PAGE = TEST_DOMAIN + TEST_PATH + "page.html";
const TEST_REDIRECT_PAGE = TEST_DOMAIN + TEST_PATH + "redirect.sjs";
const TEST_TRACKING_PAGE = TEST_3RD_PARTY_DOMAIN + TEST_PATH + "page.html";
const TEST_TRACKING_REDIRECT_PAGE =
  TEST_3RD_PARTY_DOMAIN + TEST_PATH + "redirect.sjs";
const TEST_ANOTHER_TRACKING_REDIRECT_PAGE =
  TEST_ANOTHER_3RD_PARTY_DOMAIN_HTTPS + TEST_PATH + "redirect.sjs";

const TEST_CASES = [
  // Tracker(Interacted) -> Non-Tracker
  {
    trackersHasUserInteraction: [TEST_3RD_PARTY_DOMAIN],
    redirects: [TEST_TRACKING_REDIRECT_PAGE, TEST_PAGE],
    expectedPermissionNumber: 1,
    expects: [
      [
        TEST_DOMAIN,
        TEST_3RD_PARTY_DOMAIN,
        Ci.nsIPermissionManager.ALLOW_ACTION,
      ],
    ],
  },
  // Tracker(No interaction) -> Non-Tracker
  {
    trackersHasUserInteraction: [],
    redirects: [TEST_TRACKING_REDIRECT_PAGE, TEST_PAGE],
    expectedPermissionNumber: 0,
    expects: [
      [
        TEST_DOMAIN,
        TEST_3RD_PARTY_DOMAIN,
        Ci.nsIPermissionManager.UNKNOWN_ACTION,
      ],
    ],
  },
  // Non-Tracker -> Tracker(Interacted) -> Non-Tracker
  {
    trackersHasUserInteraction: [TEST_3RD_PARTY_DOMAIN],
    redirects: [TEST_REDIRECT_PAGE, TEST_TRACKING_REDIRECT_PAGE, TEST_PAGE],
    expectedPermissionNumber: 1,
    expects: [
      [
        TEST_DOMAIN,
        TEST_3RD_PARTY_DOMAIN,
        Ci.nsIPermissionManager.ALLOW_ACTION,
      ],
    ],
  },
  // Tracker(Interacted) -> Tracker(Interacted) -> Tracker(Interacted)
  {
    trackersHasUserInteraction: [TEST_3RD_PARTY_DOMAIN],
    redirects: [
      TEST_TRACKING_REDIRECT_PAGE,
      TEST_TRACKING_REDIRECT_PAGE,
      TEST_TRACKING_PAGE,
    ],
    expectedPermissionNumber: 0,
    expects: [
      [
        TEST_3RD_PARTY_DOMAIN,
        TEST_3RD_PARTY_DOMAIN,
        Ci.nsIPermissionManager.UNKNOWN_ACTION,
      ],
    ],
  },
  // Tracker1(Interacted) -> Tracker2(Interacted) -> Non-Tracker
  {
    trackersHasUserInteraction: [
      TEST_3RD_PARTY_DOMAIN,
      TEST_ANOTHER_3RD_PARTY_DOMAIN_HTTPS,
    ],
    redirects: [
      TEST_TRACKING_REDIRECT_PAGE,
      TEST_ANOTHER_TRACKING_REDIRECT_PAGE,
      TEST_PAGE,
    ],
    expectedPermissionNumber: 1,
    expects: [
      [
        TEST_DOMAIN,
        TEST_ANOTHER_3RD_PARTY_DOMAIN_HTTPS,
        Ci.nsIPermissionManager.ALLOW_ACTION,
      ],
    ],
  },
  // Tracker1(Interacted) -> Non-Tracker -> Tracker2(No interaction) -> Non-Tracker
  {
    trackersHasUserInteraction: [TEST_3RD_PARTY_DOMAIN],
    redirects: [
      TEST_TRACKING_REDIRECT_PAGE,
      TEST_REDIRECT_PAGE,
      TEST_ANOTHER_TRACKING_REDIRECT_PAGE,
      TEST_PAGE,
    ],
    expectedPermissionNumber: 1,
    expects: [
      [
        TEST_DOMAIN,
        TEST_3RD_PARTY_DOMAIN,
        Ci.nsIPermissionManager.ALLOW_ACTION,
      ],
    ],
  },
  // Tracker1(Interacted) -> Non-Tracker -> Tracker2(Interacted) -> Non-Tracker
  // Note that the result is not quite correct in this case. We are supposed to
  // grant access to the least tracker instead of the first one. But, this is
  // the behavior how we act so far. We would fix this in another bug.
  {
    trackersHasUserInteraction: [
      TEST_3RD_PARTY_DOMAIN,
      TEST_ANOTHER_3RD_PARTY_DOMAIN_HTTPS,
    ],
    redirects: [
      TEST_TRACKING_REDIRECT_PAGE,
      TEST_REDIRECT_PAGE,
      TEST_ANOTHER_TRACKING_REDIRECT_PAGE,
      TEST_PAGE,
    ],
    expectedPermissionNumber: 1,
    expects: [
      [
        TEST_DOMAIN,
        TEST_3RD_PARTY_DOMAIN,
        Ci.nsIPermissionManager.ALLOW_ACTION,
      ],
    ],
  },
];

async function interactWithSpecificTracker(aTracker) {
  let win = await BrowserTestUtils.openNewBrowserWindow();
  await BrowserTestUtils.withNewTab(
    { gBrowser: win.gBrowser, url: aTracker },
    async function(browser) {
      info("Let's interact with the tracker");

      await SpecialPowers.spawn(browser, [], async function() {
        SpecialPowers.wrap(content.document).userInteractionForTesting();
      });
    }
  );
  await BrowserTestUtils.closeWindow(win);
}

function getNumberOfStorageAccessPermissions() {
  let num = 0;
  for (let perm of Services.perms.all) {
    if (perm.type.startsWith("3rdPartyStorage^")) {
      num++;
    }
  }
  return num;
}

async function verifyStorageAccessPermission(aExpects) {
  for (let expect of aExpects) {
    let uri = Services.io.newURI(expect[0]);
    let principal = Services.scriptSecurityManager.createContentPrincipal(
      uri,
      {}
    );
    let access = Services.perms.testPermissionFromPrincipal(
      principal,
      `3rdPartyStorage^${expect[1].slice(0, -1)}`
    );

    is(access, expect[2], "The storage access is set correctly");
  }
}

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["network.cookie.cookieBehavior", BEHAVIOR_REJECT_TRACKER],
      ["privacy.trackingprotection.annotate_channels", true],
      [
        "privacy.restrict3rdpartystorage.userInteractionRequiredForHosts",
        "tracking.example.com,tracking.example.org",
      ],
      ["privacy.restrict3rdpartystorage.heuristic.redirect", true],
    ],
  });

  await UrlClassifierTestUtils.addTestTrackers();

  registerCleanupFunction(_ => {
    Services.perms.removeAll();
  });
});

add_task(async function testETPRedirectHeuristic() {
  info("Starting testing ETP redirect heuristic ...");

  for (const test of TEST_CASES) {
    // First, clear all permissions.
    Services.perms.removeAll();

    for (const tracker of test.trackersHasUserInteraction) {
      info(`Interact with ${tracker} in top-level.`);
      await interactWithSpecificTracker(tracker);
    }

    info("Creating a new tab");
    let tab = BrowserTestUtils.addTab(gBrowser, TEST_PAGE);
    gBrowser.selectedTab = tab;

    let browser = gBrowser.getBrowserForTab(tab);
    await BrowserTestUtils.browserLoaded(browser);

    info("Loading the tracking page and trigger the top-level redirect.");
    SpecialPowers.spawn(browser, [test.redirects], async redirects => {
      let link = content.document.createElement("a");
      link.appendChild(content.document.createTextNode("click me!"));
      link.href = redirects.shift() + "?" + redirects.join("|");
      content.document.body.appendChild(link);
      link.click();
    });

    let finalRedirectDest = test.redirects[test.redirects.length - 1];

    await BrowserTestUtils.browserLoaded(browser, false, finalRedirectDest);

    is(
      getNumberOfStorageAccessPermissions(),
      test.expectedPermissionNumber,
      "The number of storage permissions is correct."
    );

    await verifyStorageAccessPermission(test.expects);

    info("Removing the tab");
    BrowserTestUtils.removeTab(tab);
  }
});
