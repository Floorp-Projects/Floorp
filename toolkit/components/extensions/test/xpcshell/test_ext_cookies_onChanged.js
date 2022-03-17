"use strict";

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();

// In this test, we want to check the behavior of extensions without private
// browsing access. Privileged add-ons automatically have private browsing
// access, so make sure that the test add-ons are not privileged.
AddonTestUtils.usePrivilegedSignatures = false;

add_task(async function setup() {
  await ExtensionTestUtils.startAddonManager();

  Services.prefs.setBoolPref("extensions.eventPages.enabled", true);
});

function createTestExtension({ privateAllowed }) {
  return ExtensionTestUtils.loadExtension({
    incognitoOverride: privateAllowed ? "spanning" : null,
    manifest: {
      permissions: ["cookies"],
      host_permissions: ["https://example.com/"],
      background: { persistent: false },
    },
    background() {
      browser.cookies.onChanged.addListener(changeInfo => {
        browser.test.sendMessage("cookie-event", changeInfo);
      });
    },
  });
}

function addAndRemoveCookie({ isPrivate }) {
  const cookie = {
    name: "cookname",
    value: "cookvalue",
    domain: "example.com",
    hostOnly: true,
    path: "/",
    secure: true,
    httpOnly: false,
    sameSite: "lax",
    session: false,
    firstPartyDomain: "",
    partitionKey: null,
    expirationDate: Date.now() + 3600000,
    storeId: isPrivate ? "firefox-private" : "firefox-default",
  };
  const originAttributes = { privateBrowsingId: isPrivate ? 1 : 0 };
  Services.cookies.add(
    cookie.domain,
    cookie.path,
    cookie.name,
    cookie.value,
    cookie.secure,
    cookie.httpOnly,
    cookie.session,
    cookie.expirationDate,
    originAttributes,
    Ci.nsICookie.SAMESITE_LAX,
    Ci.nsICookie.SCHEME_HTTPS
  );
  Services.cookies.remove(
    cookie.domain,
    cookie.name,
    cookie.path,
    originAttributes
  );
  return cookie;
}

add_task(async function test_onChanged_event_page() {
  let nonPrivateExtension = createTestExtension({ privateAllowed: false });
  let privateExtension = createTestExtension({ privateAllowed: true });
  await privateExtension.startup();
  await nonPrivateExtension.startup();
  assertPersistentListeners(privateExtension, "cookies", "onChanged", {
    primed: false,
  });
  assertPersistentListeners(nonPrivateExtension, "cookies", "onChanged", {
    primed: false,
  });

  // Suspend both event pages.
  await privateExtension.terminateBackground();
  assertPersistentListeners(privateExtension, "cookies", "onChanged", {
    primed: true,
  });
  await nonPrivateExtension.terminateBackground();
  assertPersistentListeners(nonPrivateExtension, "cookies", "onChanged", {
    primed: true,
  });

  // Modifying a private cookie should wake up the private extension, but not
  // the other one that does not have access to private browsing data.
  let privateCookie = addAndRemoveCookie({ isPrivate: true });

  Assert.deepEqual(
    await privateExtension.awaitMessage("cookie-event"),
    { removed: false, cookie: privateCookie, cause: "explicit" },
    "cookies.onChanged for private cookie creation"
  );
  Assert.deepEqual(
    await privateExtension.awaitMessage("cookie-event"),
    { removed: true, cookie: privateCookie, cause: "explicit" },
    "cookies.onChanged for private cookie removal"
  );
  // Private extension should have awakened...
  assertPersistentListeners(privateExtension, "cookies", "onChanged", {
    primed: false,
  });
  // ... but the non-private extension should still be sound asleep.
  assertPersistentListeners(nonPrivateExtension, "cookies", "onChanged", {
    primed: true,
  });

  // A non-private cookie modification should notify both extensions.
  let nonPrivateCookie = addAndRemoveCookie({ isPrivate: false });
  Assert.deepEqual(
    await privateExtension.awaitMessage("cookie-event"),
    { removed: false, cookie: nonPrivateCookie, cause: "explicit" },
    "cookies.onChanged for cookie creation in privateExtension"
  );
  Assert.deepEqual(
    await privateExtension.awaitMessage("cookie-event"),
    { removed: true, cookie: nonPrivateCookie, cause: "explicit" },
    "cookies.onChanged for cookie removal in privateExtension"
  );
  Assert.deepEqual(
    await nonPrivateExtension.awaitMessage("cookie-event"),
    { removed: false, cookie: nonPrivateCookie, cause: "explicit" },
    "cookies.onChanged for cookie creation in nonPrivateExtension"
  );
  Assert.deepEqual(
    await nonPrivateExtension.awaitMessage("cookie-event"),
    { removed: true, cookie: nonPrivateCookie, cause: "explicit" },
    "cookies.onChanged for cookie removal in nonPrivateCookie"
  );

  await privateExtension.unload();
  await nonPrivateExtension.unload();
});
