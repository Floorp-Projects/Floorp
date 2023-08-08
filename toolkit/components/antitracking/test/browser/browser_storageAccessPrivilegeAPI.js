//
// Bug 1724376 - Tests for the privilege requestStorageAccessForOrigin API.
//

/* import-globals-from storageAccessAPIHelpers.js */

"use strict";

const TEST_ANOTHER_TRACKER_DOMAIN = "https://itisatracker.org/";
const TEST_ANOTHER_TRACKER_PAGE =
  TEST_ANOTHER_TRACKER_DOMAIN + TEST_PATH + "3rdParty.html";
const TEST_ANOTHER_4TH_PARTY_DOMAIN = "https://test1.example.org/";
const TEST_ANOTHER_4TH_PARTY_PAGE =
  TEST_ANOTHER_4TH_PARTY_DOMAIN + TEST_PATH + "3rdParty.html";

// Insert an iframe with the given id into the content.
async function insertSubFrame(browser, url, id) {
  return SpecialPowers.spawn(browser, [url, id], async (url, id) => {
    let ifr = content.document.createElement("iframe");
    ifr.setAttribute("id", id);

    let loaded = ContentTaskUtils.waitForEvent(ifr, "load", false);
    content.document.body.appendChild(ifr);
    ifr.src = url;
    await loaded;
  });
}

// Run the given script in the iframe with the given id.
function runScriptInSubFrame(browser, id, script) {
  return SpecialPowers.spawn(
    browser,
    [{ callback: script.toString(), id }],
    async obj => {
      await new content.Promise(resolve => {
        let ifr = content.document.getElementById(obj.id);

        content.addEventListener("message", function msg(event) {
          if (event.data.type == "finish") {
            content.removeEventListener("message", msg);
            resolve();
            return;
          }

          if (event.data.type == "ok") {
            ok(event.data.what, event.data.msg);
            return;
          }

          if (event.data.type == "info") {
            info(event.data.msg);
            return;
          }

          ok(false, "Unknown message");
        });

        ifr.contentWindow.postMessage(obj.callback, "*");
      });
    }
  );
}

function waitStoragePermission(trackingOrigin) {
  return TestUtils.topicObserved("perm-changed", (aSubject, aData) => {
    let permission = aSubject.QueryInterface(Ci.nsIPermission);
    let uri = Services.io.newURI(TEST_DOMAIN);
    return (
      permission.type == `3rdPartyStorage^${trackingOrigin}` &&
      permission.principal.equalsURI(uri)
    );
  });
}

function clearStoragePermission(trackingOrigin) {
  return SpecialPowers.removePermission(
    `3rdPartyStorage^${trackingOrigin}`,
    TEST_TOP_PAGE
  );
}

function triggerCommand(button) {
  let notifications = PopupNotifications.panel.children;
  let notification = notifications[0];
  EventUtils.synthesizeMouseAtCenter(notification[button], {});
}

function triggerMainCommand() {
  triggerCommand("button");
}

function triggerSecondaryCommand() {
  triggerCommand("secondaryButton");
}

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.storage_access.auto_grants", true],
      ["dom.storage_access.auto_grants.delayed", false],
      ["dom.storage_access.enabled", true],
      ["dom.storage_access.prompt.testing", false],
      ["privacy.trackingprotection.enabled", false],
      ["privacy.trackingprotection.pbmode.enabled", false],
      ["privacy.trackingprotection.annotate_channels", true],
      // Bug 1617611: Fix all the tests broken by "cookies SameSite=lax by default"
      ["network.cookie.sameSite.laxByDefault", false],
    ],
  });

  await UrlClassifierTestUtils.addTestTrackers();

  registerCleanupFunction(() => {
    SpecialPowers.clearUserPref("network.cookie.sameSite.laxByDefault");
    UrlClassifierTestUtils.cleanupTestTrackers();
  });
});

add_task(async function test_api_only_available_in_privilege_scope() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_TOP_PAGE
  );
  let browser = tab.linkedBrowser;

  await SpecialPowers.spawn(browser, [], async _ => {
    ok(
      content.document.requestStorageAccessForOrigin,
      "The privilege API is available in system privilege code."
    );
  });

  // Open an iframe and check if the privilege is not available in content code.
  await insertSubFrame(browser, TEST_3RD_PARTY_PAGE, "test");
  await runScriptInSubFrame(browser, "test", async function check() {
    ok(
      !document.requestStorageAccessForOrigin,
      "The privilege API is not available in content code."
    );
  });

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_privilege_api_with_reject_tracker() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "network.cookie.cookieBehavior",
        Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER,
      ],
      [
        "network.cookie.cookieBehavior.pbmode",
        Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER,
      ],
    ],
  });

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_TOP_PAGE
  );
  let browser = tab.linkedBrowser;

  await insertSubFrame(browser, TEST_3RD_PARTY_PAGE, "test");

  // Verify that the third party tracker doesn't have storage access at
  // beginning.
  await runScriptInSubFrame(browser, "test", async _ => {
    await noStorageAccessInitially();

    is(document.cookie, "", "No cookies for me");
    document.cookie = "name=value";
    is(document.cookie, "", "Setting cookie is blocked");
  });

  let storagePermissionPromise = waitStoragePermission(
    "https://tracking.example.org"
  );

  // Verify if the prompt has been shown.
  let shownPromise = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );

  // Call the privilege API.
  let callAPIPromise = SpecialPowers.spawn(browser, [], async _ => {
    // The privilege API requires user activation. So, we set the user
    // activation flag before we call the API.
    content.document.notifyUserGestureActivation();

    try {
      await content.document.requestStorageAccessForOrigin(
        "https://tracking.example.org/"
      );
    } catch (e) {
      ok(false, "The API shouldn't throw.");
    }

    content.document.clearUserGestureActivation();
  });

  await shownPromise;

  // Accept the prompt
  triggerMainCommand();

  await callAPIPromise;

  // Verify if the storage access permission is set correctly.
  await storagePermissionPromise;

  // Verify if the existing third-party tracker iframe gains the storage
  // access.
  await runScriptInSubFrame(browser, "test", async _ => {
    await hasStorageAccessInitially();

    is(document.cookie, "", "Still no cookies for me");
    document.cookie = "name=value";
    is(document.cookie, "name=value", "Successfully set cookies.");
  });

  // Insert another third-party tracker iframe and check if it has storage access.
  await insertSubFrame(browser, TEST_3RD_PARTY_PAGE, "test2");
  await runScriptInSubFrame(browser, "test2", async _ => {
    await hasStorageAccessInitially();

    is(document.cookie, "name=value", "Some cookies for me");
  });

  // Insert another iframe with different third-party tracker and check it has
  // no storage access.
  await insertSubFrame(browser, TEST_ANOTHER_TRACKER_PAGE, "test3");
  await runScriptInSubFrame(browser, "test3", async _ => {
    await noStorageAccessInitially();

    is(document.cookie, "", "No cookies for me");
    document.cookie = "name=value";
    is(document.cookie, "", "Setting cookie is blocked for another tracker.");
  });

  await clearStoragePermission("https://tracking.example.org");
  Services.cookies.removeAll();
  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_privilege_api_with_dFPI() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "network.cookie.cookieBehavior",
        Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
      ],
      [
        "network.cookie.cookieBehavior.pbmode",
        Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
      ],
    ],
  });

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_TOP_PAGE
  );
  let browser = tab.linkedBrowser;

  await insertSubFrame(browser, TEST_4TH_PARTY_PAGE_HTTPS, "test");

  // Verify that the third-party context doesn't have storage access at
  // beginning.
  await runScriptInSubFrame(browser, "test", async _ => {
    await noStorageAccessInitially();

    is(document.cookie, "", "No cookies for me");
    document.cookie = "name=partitioned";
    is(
      document.cookie,
      "name=partitioned",
      "Setting cookie in partitioned context."
    );
  });

  let storagePermissionPromise = waitStoragePermission(
    "https://not-tracking.example.com"
  );

  // Verify if the prompt has been shown.
  let shownPromise = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );

  // Call the privilege API.
  let callAPIPromise = SpecialPowers.spawn(browser, [], async _ => {
    // The privilege API requires a user gesture. So, we set the user handling
    // flag before we call the API.
    content.document.notifyUserGestureActivation();

    try {
      await content.document.requestStorageAccessForOrigin(
        "https://not-tracking.example.com/"
      );
    } catch (e) {
      ok(false, "The API shouldn't throw.");
    }

    content.document.clearUserGestureActivation();
  });

  await shownPromise;

  // Accept the prompt
  triggerMainCommand();

  await callAPIPromise;

  // Verify if the storage access permission is set correctly.
  await storagePermissionPromise;

  // Verify if the existing third-party iframe gains the storage access.
  await runScriptInSubFrame(browser, "test", async _ => {
    await hasStorageAccessInitially();

    is(document.cookie, "", "No unpartitioned cookies");
    document.cookie = "name=unpartitioned";
    is(document.cookie, "name=unpartitioned", "Successfully set cookies.");
  });

  // Insert another third-party content iframe and check if it has storage access.
  await insertSubFrame(browser, TEST_4TH_PARTY_PAGE_HTTPS, "test2");
  await runScriptInSubFrame(browser, "test2", async _ => {
    await hasStorageAccessInitially();

    is(
      document.cookie,
      "name=unpartitioned",
      "Some cookies for unpartitioned context"
    );
  });

  // Insert another iframe with different third-party content and check it has
  // no storage access.
  await insertSubFrame(browser, TEST_ANOTHER_4TH_PARTY_PAGE, "test3");
  await runScriptInSubFrame(browser, "test3", async _ => {
    await noStorageAccessInitially();

    is(document.cookie, "", "No cookies for me");
    document.cookie = "name=value";
    is(document.cookie, "name=value", "Setting cookie to partitioned context.");
  });

  await clearStoragePermission("https://not-tracking.example.com");
  Services.cookies.removeAll();
  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_prompt() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.storage_access.auto_grants", false],
      [
        "network.cookie.cookieBehavior",
        Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER,
      ],
      [
        "network.cookie.cookieBehavior.pbmode",
        Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER,
      ],
    ],
  });

  for (const allow of [false, true]) {
    let tab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      TEST_TOP_PAGE
    );
    let browser = tab.linkedBrowser;

    // Verify if the prompt has been shown.
    let shownPromise = BrowserTestUtils.waitForEvent(
      PopupNotifications.panel,
      "popupshown"
    );

    let hiddenPromise = BrowserTestUtils.waitForEvent(
      PopupNotifications.panel,
      "popuphidden"
    );

    // Call the privilege API.
    let callAPIPromise = SpecialPowers.spawn(browser, [allow], async allow => {
      // The privilege API requires a user gesture. So, we set the user handling
      // flag before we call the API.
      content.document.notifyUserGestureActivation();
      let isThrown = false;

      try {
        await content.document.requestStorageAccessForOrigin(
          "https://tracking.example.org"
        );
      } catch (e) {
        isThrown = true;
      }

      is(isThrown, !allow, `The API ${allow ? "shouldn't" : "should"} throw.`);

      content.document.clearUserGestureActivation();
    });

    await shownPromise;

    let notification = await TestUtils.waitForCondition(_ =>
      PopupNotifications.getNotification("storage-access", browser)
    );
    ok(notification, "Should have gotten the notification");

    // Click the popup button.
    if (allow) {
      triggerMainCommand();
    } else {
      triggerSecondaryCommand();
    }

    // Wait until the popup disappears.
    await hiddenPromise;

    // Wait until the API finishes.
    await callAPIPromise;

    await insertSubFrame(browser, TEST_3RD_PARTY_PAGE, "test");

    if (allow) {
      await runScriptInSubFrame(browser, "test", async _ => {
        await hasStorageAccessInitially();

        is(document.cookie, "", "Still no cookies for me");
        document.cookie = "name=value";
        is(document.cookie, "name=value", "Successfully set cookies.");
      });
    } else {
      await runScriptInSubFrame(browser, "test", async _ => {
        await noStorageAccessInitially();

        is(document.cookie, "", "Still no cookies for me");
        document.cookie = "name=value";
        is(document.cookie, "", "No cookie after setting.");
      });
    }

    BrowserTestUtils.removeTab(tab);
  }

  await clearStoragePermission("https://tracking.example.org");
  Services.cookies.removeAll();
});

// Tests that the priviledged rSA method should show a prompt when auto grants
// are enabled, but we don't have user activation. When requiring user
// activation, rSA should still reject.
add_task(async function test_prompt_no_user_activation() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.storage_access.auto_grants", true],
      [
        "network.cookie.cookieBehavior",
        Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER,
      ],
      [
        "network.cookie.cookieBehavior.pbmode",
        Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER,
      ],
    ],
  });

  for (let requireUserActivation of [false, true]) {
    let tab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      TEST_TOP_PAGE
    );
    let browser = tab.linkedBrowser;

    let shownPromise, hiddenPromise;

    // Verify if the prompt has been shown.
    if (!requireUserActivation) {
      shownPromise = BrowserTestUtils.waitForEvent(
        PopupNotifications.panel,
        "popupshown"
      );

      hiddenPromise = BrowserTestUtils.waitForEvent(
        PopupNotifications.panel,
        "popuphidden"
      );
    }

    // Call the privilege API.
    let callAPIPromise = SpecialPowers.spawn(
      browser,
      [requireUserActivation],
      async requireUserActivation => {
        let isThrown = false;

        try {
          await content.document.requestStorageAccessForOrigin(
            "https://tracking.example.org",
            requireUserActivation
          );
        } catch (e) {
          isThrown = true;
        }

        is(
          isThrown,
          requireUserActivation,
          `The API ${requireUserActivation ? "shouldn't" : "should"} throw.`
        );
      }
    );

    if (!requireUserActivation) {
      await shownPromise;

      let notification = await TestUtils.waitForCondition(_ =>
        PopupNotifications.getNotification("storage-access", browser)
      );
      ok(notification, "Should have gotten the notification");

      // Click the popup button.
      triggerMainCommand();

      // Wait until the popup disappears.
      await hiddenPromise;
    }

    // Wait until the API finishes.
    await callAPIPromise;

    await insertSubFrame(browser, TEST_3RD_PARTY_PAGE, "test");

    if (!requireUserActivation) {
      await runScriptInSubFrame(browser, "test", async _ => {
        await hasStorageAccessInitially();

        is(document.cookie, "", "Still no cookies for me");
        document.cookie = "name=value";
        is(document.cookie, "name=value", "Successfully set cookies.");
      });
    } else {
      await runScriptInSubFrame(browser, "test", async _ => {
        await noStorageAccessInitially();

        is(document.cookie, "", "Still no cookies for me");
        document.cookie = "name=value";
        is(document.cookie, "", "No cookie after setting.");
      });
    }

    BrowserTestUtils.removeTab(tab);
    await clearStoragePermission("https://tracking.example.org");
    Services.cookies.removeAll();
  }
});

add_task(async function test_invalid_input() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_TOP_PAGE
  );
  let browser = tab.linkedBrowser;

  await SpecialPowers.spawn(browser, [], async _ => {
    let isThrown = false;
    try {
      await content.document.requestStorageAccessForOrigin(
        "https://tracking.example.org"
      );
    } catch (e) {
      isThrown = true;
    }
    ok(isThrown, "The API should throw without user gesture.");

    content.document.notifyUserGestureActivation();
    isThrown = false;
    try {
      await content.document.requestStorageAccessForOrigin();
    } catch (e) {
      isThrown = true;
    }
    ok(isThrown, "The API should throw with no input.");

    content.document.notifyUserGestureActivation();
    isThrown = false;
    try {
      await content.document.requestStorageAccessForOrigin("");
    } catch (e) {
      isThrown = true;
      is(e.name, "NS_ERROR_MALFORMED_URI", "The input is not a valid url");
    }
    ok(isThrown, "The API should throw with empty string.");

    content.document.notifyUserGestureActivation();
    isThrown = false;
    try {
      await content.document.requestStorageAccessForOrigin("invalid url");
    } catch (e) {
      isThrown = true;
      is(e.name, "NS_ERROR_MALFORMED_URI", "The input is not a valid url");
    }
    ok(isThrown, "The API should throw with invalid url.");

    content.document.clearUserGestureActivation();
  });

  BrowserTestUtils.removeTab(tab);
});
