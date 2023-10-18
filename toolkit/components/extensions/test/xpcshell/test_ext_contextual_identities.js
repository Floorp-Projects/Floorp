"use strict";

ChromeUtils.defineESModuleGetters(this, {
  ContextualIdentityService:
    "resource://gre/modules/ContextualIdentityService.sys.mjs",
  ExtensionPreferencesManager:
    "resource://gre/modules/ExtensionPreferencesManager.sys.mjs",
  AddonManager: "resource://gre/modules/AddonManager.sys.mjs",
});

const CONTAINERS_PREF = "privacy.userContext.enabled";

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "42",
  "42"
);

add_task(async function startup() {
  await AddonTestUtils.promiseStartupManager();
});

add_task(async function test_contextualIdentities_without_permissions() {
  function background() {
    browser.test.assertTrue(
      !browser.contextualIdentities,
      "contextualIdentities API is not available when the contextualIdentities permission is not required"
    );
    browser.test.notifyPass("contextualIdentities_without_permission");
  }

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",
    background,
    manifest: {
      browser_specific_settings: {
        gecko: { id: "testing@thing.com" },
      },
      permissions: [],
    },
  });

  await extension.startup();
  await extension.awaitFinish("contextualIdentities_without_permission");
  await extension.unload();
});

add_task(async function test_contextualIdentity_events() {
  async function background() {
    function createOneTimeListener(type) {
      return new Promise((resolve, reject) => {
        try {
          browser.test.assertTrue(
            type in browser.contextualIdentities,
            `Found API object browser.contextualIdentities.${type}`
          );
          const listener = change => {
            browser.test.assertTrue(
              "contextualIdentity" in change,
              `Found identity in change`
            );
            browser.contextualIdentities[type].removeListener(listener);
            resolve(change);
          };
          browser.contextualIdentities[type].addListener(listener);
        } catch (e) {
          reject(e);
        }
      });
    }

    function assertExpected(expected, container) {
      // Number of keys that are added by the APIs
      const createdCount = 2;
      for (let key of Object.keys(container)) {
        browser.test.assertTrue(key in expected, `found property ${key}`);
        browser.test.assertEq(
          expected[key],
          container[key],
          `property value for ${key} is correct`
        );
      }
      const hexMatch = /^#[0-9a-f]{6}$/;
      browser.test.assertTrue(
        hexMatch.test(expected.colorCode),
        "Color code property was expected Hex shape"
      );
      const iconMatch = /^resource:\/\/usercontext-content\/[a-z]+[.]svg$/;
      browser.test.assertTrue(
        iconMatch.test(expected.iconUrl),
        "Icon url property was expected shape"
      );
      browser.test.assertEq(
        Object.keys(expected).length,
        Object.keys(container).length + createdCount,
        "all expected properties found"
      );
    }

    let onCreatePromise = createOneTimeListener("onCreated");

    let containerObj = { name: "foobar", color: "red", icon: "circle" };
    let ci = await browser.contextualIdentities.create(containerObj);
    browser.test.assertTrue(!!ci, "We have an identity");
    const onCreateListenerResponse = await onCreatePromise;
    const cookieStoreId = ci.cookieStoreId;
    assertExpected(
      onCreateListenerResponse.contextualIdentity,
      Object.assign(containerObj, { cookieStoreId })
    );

    let onUpdatedPromise = createOneTimeListener("onUpdated");
    let updateContainerObj = { name: "testing", color: "blue", icon: "dollar" };
    ci = await browser.contextualIdentities.update(
      cookieStoreId,
      updateContainerObj
    );
    browser.test.assertTrue(!!ci, "We have an update identity");
    const onUpdatedListenerResponse = await onUpdatedPromise;
    assertExpected(
      onUpdatedListenerResponse.contextualIdentity,
      Object.assign(updateContainerObj, { cookieStoreId })
    );

    let onRemovePromise = createOneTimeListener("onRemoved");
    ci = await browser.contextualIdentities.remove(
      updateContainerObj.cookieStoreId
    );
    browser.test.assertTrue(!!ci, "We have an remove identity");
    const onRemoveListenerResponse = await onRemovePromise;
    assertExpected(
      onRemoveListenerResponse.contextualIdentity,
      Object.assign(updateContainerObj, { cookieStoreId })
    );

    browser.test.notifyPass("contextualIdentities_events");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    useAddonManager: "temporary",
    manifest: {
      browser_specific_settings: {
        gecko: { id: "testing@thing.com" },
      },
      permissions: ["contextualIdentities"],
    },
  });

  Services.prefs.setBoolPref(CONTAINERS_PREF, true);

  await extension.startup();
  await extension.awaitFinish("contextualIdentities_events");
  await extension.unload();

  Services.prefs.clearUserPref(CONTAINERS_PREF);
});

add_task(async function test_contextualIdentity_with_permissions() {
  async function background() {
    let ci;
    await browser.test.assertRejects(
      browser.contextualIdentities.get("foobar"),
      "Invalid contextual identity: foobar",
      "API should reject here"
    );
    await browser.test.assertRejects(
      browser.contextualIdentities.update("foobar", { name: "testing" }),
      "Invalid contextual identity: foobar",
      "API should reject for unknown updates"
    );
    await browser.test.assertRejects(
      browser.contextualIdentities.remove("foobar"),
      "Invalid contextual identity: foobar",
      "API should reject for removing unknown containers"
    );

    ci = await browser.contextualIdentities.get("firefox-container-1");
    browser.test.assertTrue(!!ci, "We have an identity");
    browser.test.assertTrue("name" in ci, "We have an identity.name");
    browser.test.assertTrue("color" in ci, "We have an identity.color");
    browser.test.assertTrue("icon" in ci, "We have an identity.icon");
    browser.test.assertEq("Personal", ci.name, "identity.name is correct");
    browser.test.assertEq(
      "firefox-container-1",
      ci.cookieStoreId,
      "identity.cookieStoreId is correct"
    );

    function listenForMessage(messageName, stateChangeBool) {
      return new Promise(resolve => {
        browser.test.onMessage.addListener(function listener(msg) {
          browser.test.log(`Got message from background: ${msg}`);
          if (msg === messageName + "-response") {
            browser.test.onMessage.removeListener(listener);
            resolve();
          }
        });
        browser.test.log(
          `Sending message to background: ${messageName} ${stateChangeBool}`
        );
        browser.test.sendMessage(messageName, stateChangeBool);
      });
    }

    await listenForMessage("containers-state-change", false);

    browser.test.assertRejects(
      browser.contextualIdentities.query({}),
      "Contextual identities are currently disabled",
      "Throws when containers are disabled"
    );

    await listenForMessage("containers-state-change", true);

    let cis = await browser.contextualIdentities.query({});
    browser.test.assertEq(
      4,
      cis.length,
      "by default we should have 4 containers"
    );

    cis = await browser.contextualIdentities.query({ name: "Personal" });
    browser.test.assertEq(
      1,
      cis.length,
      "by default we should have 1 container called Personal"
    );

    cis = await browser.contextualIdentities.query({ name: "foobar" });
    browser.test.assertEq(
      0,
      cis.length,
      "by default we should have 0 container called foobar"
    );

    ci = await browser.contextualIdentities.create({
      name: "foobar",
      color: "red",
      icon: "gift",
    });
    browser.test.assertTrue(!!ci, "We have an identity");
    browser.test.assertEq("foobar", ci.name, "identity.name is correct");
    browser.test.assertEq("red", ci.color, "identity.color is correct");
    browser.test.assertEq("gift", ci.icon, "identity.icon is correct");
    browser.test.assertTrue(
      !!ci.cookieStoreId,
      "identity.cookieStoreId is correct"
    );

    browser.test.assertRejects(
      browser.contextualIdentities.create({
        name: "foobar",
        color: "red",
        icon: "firefox",
      }),
      "Invalid icon firefox for container",
      "Create container called with an invalid icon"
    );

    browser.test.assertRejects(
      browser.contextualIdentities.create({
        name: "foobar",
        color: "firefox-orange",
        icon: "gift",
      }),
      "Invalid color name firefox-orange for container",
      "Create container called with an invalid color"
    );

    cis = await browser.contextualIdentities.query({});
    browser.test.assertEq(
      5,
      cis.length,
      "we should still have have 5 containers"
    );

    ci = await browser.contextualIdentities.get(ci.cookieStoreId);
    browser.test.assertTrue(!!ci, "We have an identity");
    browser.test.assertEq("foobar", ci.name, "identity.name is correct");
    browser.test.assertEq("red", ci.color, "identity.color is correct");
    browser.test.assertEq("gift", ci.icon, "identity.icon is correct");

    browser.test.assertRejects(
      browser.contextualIdentities.update(ci.cookieStoreId, {
        name: "foobar",
        color: "red",
        icon: "firefox",
      }),
      "Invalid icon firefox for container",
      "Create container called with an invalid icon"
    );

    browser.test.assertRejects(
      browser.contextualIdentities.update(ci.cookieStoreId, {
        name: "foobar",
        color: "firefox-orange",
        icon: "gift",
      }),
      "Invalid color name firefox-orange for container",
      "Create container called with an invalid color"
    );

    cis = await browser.contextualIdentities.query({});
    browser.test.assertEq(5, cis.length, "now we have 5 identities");

    ci = await browser.contextualIdentities.update(ci.cookieStoreId, {
      name: "barfoo",
      color: "blue",
      icon: "cart",
    });
    browser.test.assertTrue(!!ci, "We have an identity");
    browser.test.assertEq("barfoo", ci.name, "identity.name is correct");
    browser.test.assertEq("blue", ci.color, "identity.color is correct");
    browser.test.assertEq("cart", ci.icon, "identity.icon is correct");

    ci = await browser.contextualIdentities.get(ci.cookieStoreId);
    browser.test.assertTrue(!!ci, "We have an identity");
    browser.test.assertEq("barfoo", ci.name, "identity.name is correct");
    browser.test.assertEq("blue", ci.color, "identity.color is correct");
    browser.test.assertEq("cart", ci.icon, "identity.icon is correct");

    ci = await browser.contextualIdentities.remove(ci.cookieStoreId);
    browser.test.assertTrue(!!ci, "We have an identity");
    browser.test.assertEq("barfoo", ci.name, "identity.name is correct");
    browser.test.assertEq("blue", ci.color, "identity.color is correct");
    browser.test.assertEq("cart", ci.icon, "identity.icon is correct");

    cis = await browser.contextualIdentities.query({});
    browser.test.assertEq(4, cis.length, "we are back to 4 identities");

    browser.test.notifyPass("contextualIdentities");
  }

  function makeExtension(id) {
    return ExtensionTestUtils.loadExtension({
      useAddonManager: "temporary",
      background,
      manifest: {
        browser_specific_settings: {
          gecko: { id },
        },
        permissions: ["contextualIdentities"],
      },
    });
  }

  let extension = makeExtension("containers-test@mozilla.org");

  extension.onMessage("containers-state-change", stateBool => {
    Cu.reportError(`Got message "containers-state-change", ${stateBool}`);
    Services.prefs.setBoolPref(CONTAINERS_PREF, stateBool);
    Cu.reportError("Changed pref");
    extension.sendMessage("containers-state-change-response");
  });

  await extension.startup();
  await extension.awaitFinish("contextualIdentities");
  equal(
    Services.prefs.getBoolPref(CONTAINERS_PREF),
    true,
    "Pref should now be enabled, whatever it's initial state"
  );
  await extension.unload();
  equal(
    Services.prefs.getBoolPref(CONTAINERS_PREF),
    true,
    "Pref should remain enabled"
  );

  Services.prefs.clearUserPref(CONTAINERS_PREF);
});

add_task(async function test_contextualIdentity_extensions_enable_containers() {
  async function background() {
    let ci = await browser.contextualIdentities.get("firefox-container-1");
    browser.test.assertTrue(!!ci, "We have an identity");

    browser.test.notifyPass("contextualIdentities");
  }
  function makeExtension(id) {
    return ExtensionTestUtils.loadExtension({
      useAddonManager: "permanent",
      background,
      manifest: {
        browser_specific_settings: {
          gecko: { id },
        },
        permissions: ["contextualIdentities"],
      },
    });
  }
  async function testSetting(expect, message) {
    let setting = await ExtensionPreferencesManager.getSetting(
      "privacy.containers"
    );
    if (expect === null) {
      equal(setting, null, message);
    } else {
      equal(setting.value, expect, message);
    }
  }
  function testPref(expect, message) {
    equal(Services.prefs.getBoolPref(CONTAINERS_PREF), expect, message);
  }

  let extension = makeExtension("containers-test@mozilla.org");
  await extension.startup();
  await extension.awaitFinish("contextualIdentities");
  equal(
    Services.prefs.getBoolPref(CONTAINERS_PREF),
    true,
    "Pref should now be enabled, whatever it's initial state"
  );
  await extension.unload();
  await testSetting(null, "setting should be unset");
  equal(
    Services.prefs.getBoolPref(CONTAINERS_PREF),
    true,
    "Pref should remain enabled"
  );

  // Lets set containers explicitly to be off and test we keep it that way after removal
  Services.prefs.setBoolPref(CONTAINERS_PREF, false);

  let extension1 = makeExtension("containers-test-1@mozilla.org");
  await extension1.startup();
  await extension1.awaitFinish("contextualIdentities");
  await testSetting(extension1.id, "setting should be controlled");
  testPref(true, "Pref should now be enabled, whatever it's initial state");

  // Test that disabling leaves containers on, and that re-enabling with containers off
  // will re-enable containers.
  const addon = await AddonManager.getAddonByID(extension1.id);
  await addon.disable();
  await testSetting(undefined, "setting should not be an addon");
  testPref(true, "Pref should remain enabled, whatever it's initial state");

  Services.prefs.setBoolPref(CONTAINERS_PREF, false);

  await addon.enable();
  await testSetting(extension1.id, "setting should be controlled");
  testPref(true, "Pref should be enabled");

  await extension1.unload();
  await testSetting(null, "setting should be unset");
  equal(
    Services.prefs.getBoolPref(CONTAINERS_PREF),
    true,
    "Pref should remain enabled"
  );

  // Lets set containers explicitly to be on and test we keep it that way after removal.
  Services.prefs.setBoolPref(CONTAINERS_PREF, true);

  let extension2 = makeExtension("containers-test-2@mozilla.org");
  let extension3 = makeExtension("containers-test-3@mozilla.org");
  await extension2.startup();
  await extension2.awaitFinish("contextualIdentities");
  await extension3.startup();
  await extension3.awaitFinish("contextualIdentities");

  // Flip the ordering to check it's still enabled
  await testSetting(extension3.id, "setting should still be controlled by 3");
  testPref(true, "Pref should now be enabled 1");
  await extension3.unload();
  await testSetting(extension2.id, "setting should still be controlled by 2");
  testPref(true, "Pref should now be enabled 2");
  await extension2.unload();
  await testSetting(null, "setting should be unset");
  testPref(true, "Pref should now be enabled 3");

  Services.prefs.clearUserPref(CONTAINERS_PREF);
});

add_task(async function test_contextualIdentity_preference_change() {
  async function background() {
    let extensionInfo = await browser.management.getSelf();
    if (extensionInfo.version == "1.0.0") {
      const containers = await browser.contextualIdentities.query({});
      browser.test.assertEq(
        containers.length,
        4,
        "We still have the original containers"
      );
      await browser.contextualIdentities.create({
        name: "foobar",
        color: "red",
        icon: "circle",
      });
    }
    const containers = await browser.contextualIdentities.query({});
    browser.test.assertEq(containers.length, 5, "We have a new container");
    if (extensionInfo.version == "1.1.0") {
      await browser.contextualIdentities.remove(containers[4].cookieStoreId);
    }
    browser.test.notifyPass("contextualIdentities");
  }
  function makeExtension(id, version) {
    return ExtensionTestUtils.loadExtension({
      useAddonManager: "temporary",
      background,
      manifest: {
        version,
        browser_specific_settings: {
          gecko: { id },
        },
        permissions: ["contextualIdentities"],
      },
    });
  }

  Services.prefs.setBoolPref(CONTAINERS_PREF, false);
  let extension = makeExtension("containers-pref-test@mozilla.org", "1.0.0");
  await extension.startup();
  await extension.awaitFinish("contextualIdentities");
  equal(
    Services.prefs.getBoolPref(CONTAINERS_PREF),
    true,
    "Pref should now be enabled, whatever it's initial state"
  );

  let extension2 = makeExtension("containers-pref-test@mozilla.org", "1.1.0");
  await extension2.startup();
  await extension2.awaitFinish("contextualIdentities");

  await extension.unload();
  equal(
    Services.prefs.getBoolPref(CONTAINERS_PREF),
    true,
    "Pref should remain enabled"
  );

  Services.prefs.clearUserPref(CONTAINERS_PREF);
});

add_task(
  { pref_set: [["extensions.eventPages.enabled", true]] },
  async function test_contextualIdentity_event_page() {
    let extension = ExtensionTestUtils.loadExtension({
      useAddonManager: "permanent",
      manifest: {
        browser_specific_settings: {
          gecko: { id: "eventpage@mochitest" },
        },
        permissions: ["contextualIdentities"],
        background: { persistent: false },
      },
      background() {
        browser.contextualIdentities.onCreated.addListener(() => {
          browser.test.sendMessage("created");
        });
        browser.contextualIdentities.onUpdated.addListener(() => {});
        browser.contextualIdentities.onRemoved.addListener(() => {
          browser.test.sendMessage("removed");
        });
        browser.test.sendMessage("ready");
      },
    });

    const EVENTS = ["onCreated", "onUpdated", "onRemoved"];

    await extension.startup();
    await extension.awaitMessage("ready");
    for (let event of EVENTS) {
      assertPersistentListeners(extension, "contextualIdentities", event, {
        primed: false,
      });
    }

    await extension.terminateBackground();
    for (let event of EVENTS) {
      assertPersistentListeners(extension, "contextualIdentities", event, {
        primed: true,
      });
    }

    // test events waken background
    let identity = ContextualIdentityService.create("foobar", "circle", "red");

    await extension.awaitMessage("ready");
    await extension.awaitMessage("created");
    for (let event of EVENTS) {
      assertPersistentListeners(extension, "contextualIdentities", event, {
        primed: false,
      });
    }

    ContextualIdentityService.remove(identity.userContextId);
    await extension.awaitMessage("removed");

    // check primed listeners after startup
    await AddonTestUtils.promiseRestartManager();
    await extension.awaitStartup();

    for (let event of EVENTS) {
      assertPersistentListeners(extension, "contextualIdentities", event, {
        primed: true,
      });
    }

    await extension.unload();
  }
);
