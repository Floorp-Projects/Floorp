"use strict";

do_get_profile();

XPCOMUtils.defineLazyModuleGetter(this, "AddonManager",
                                  "resource://modules/AddonManager.jsm");
add_task(async function startup() {
  await ExtensionTestUtils.startAddonManager();
});

add_task(async function test_contextualIdentities_without_permissions() {
  function background() {
    browser.test.assertTrue(!browser.contextualIdentities,
                            "contextualIdentities API is not available when the contextualIdentities permission is not required");
    browser.test.notifyPass("contextualIdentities_without_permission");
  }

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",
    background,
    manifest: {
      applications: {
        gecko: {id: "testing@thing.com"},
      },
      permissions: [],
    },
  });

  await extension.startup();
  await extension.awaitFinish("contextualIdentities_without_permission");
  await extension.unload();
});

add_task(async function test_contextualIdentity_events() {
  const CONTAINERS_PREF = "privacy.userContext.enabled";
  async function background() {
    function createOneTimeListener(type) {
      return new Promise((resolve, reject) => {
        try {
          browser.test.assertTrue(type in browser.contextualIdentities, `Found API object browser.contextualIdentities.${type}`);
          const listener = (change) => {
            browser.test.assertTrue("contextualIdentity" in change, `Found identity in change`);
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
      for (let key of Object.keys(container)) {
        browser.test.assertTrue(key in expected, `found property ${key}`);
        browser.test.assertEq(expected[key], container[key], `property value for ${key} is correct`);
      }
      browser.test.assertEq(Object.keys(expected).length, Object.keys(container).length, "all expected properties found");
    }

    let onCreatePromise = createOneTimeListener("onCreated");

    let containerObj = {name: "foobar", color: "red", icon: "icon"};
    let ci = await browser.contextualIdentities.create(containerObj);
    browser.test.assertTrue(!!ci, "We have an identity");
    const onCreateListenerResponse = await onCreatePromise;
    const cookieStoreId = ci.cookieStoreId;
    assertExpected(onCreateListenerResponse.contextualIdentity, Object.assign(containerObj, {cookieStoreId}));

    let onUpdatedPromise = createOneTimeListener("onUpdated");
    let updateContainerObj = {name: "testing", color: "blue", icon: "thing"};
    ci = await browser.contextualIdentities.update(cookieStoreId, updateContainerObj);
    browser.test.assertTrue(!!ci, "We have an update identity");
    const onUpdatedListenerResponse = await onUpdatedPromise;
    assertExpected(onUpdatedListenerResponse.contextualIdentity, Object.assign(updateContainerObj, {cookieStoreId}));

    let onRemovePromise = createOneTimeListener("onRemoved");
    ci = await browser.contextualIdentities.remove(updateContainerObj.cookieStoreId);
    browser.test.assertTrue(!!ci, "We have an remove identity");
    const onRemoveListenerResponse = await onRemovePromise;
    assertExpected(onRemoveListenerResponse.contextualIdentity, Object.assign(updateContainerObj, {cookieStoreId}));

    browser.test.notifyPass("contextualIdentities_events");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    useAddonManager: "temporary",
    manifest: {
      applications: {
        gecko: {id: "testing@thing.com"},
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
  const CONTAINERS_PREF = "privacy.userContext.enabled";
  const initial = Services.prefs.getBoolPref(CONTAINERS_PREF);

  async function background(ver) {
    let ci;
    await browser.test.assertRejects(browser.contextualIdentities.get("foobar"), "Invalid contextual identitiy: foobar", "API should reject here");
    await browser.test.assertRejects(browser.contextualIdentities.update("foobar", {name: "testing"}), "Invalid contextual identitiy: foobar", "API should reject for unknown updates");
    await browser.test.assertRejects(browser.contextualIdentities.remove("foobar"), "Invalid contextual identitiy: foobar", "API should reject for removing unknown containers");

    ci = await browser.contextualIdentities.get("firefox-container-1");
    browser.test.assertTrue(!!ci, "We have an identity");
    browser.test.assertTrue("name" in ci, "We have an identity.name");
    browser.test.assertTrue("color" in ci, "We have an identity.color");
    browser.test.assertTrue("icon" in ci, "We have an identity.icon");
    browser.test.assertEq("Personal", ci.name, "identity.name is correct");
    browser.test.assertEq("firefox-container-1", ci.cookieStoreId, "identity.cookieStoreId is correct");

    let cis = await browser.contextualIdentities.query({});
    browser.test.assertEq(4, cis.length, "by default we should have 4 containers");

    cis = await browser.contextualIdentities.query({name: "Personal"});
    browser.test.assertEq(1, cis.length, "by default we should have 1 container called Personal");

    cis = await browser.contextualIdentities.query({name: "foobar"});
    browser.test.assertEq(0, cis.length, "by default we should have 0 container called foobar");

    ci = await browser.contextualIdentities.create({name: "foobar", color: "red", icon: "icon"});
    browser.test.assertTrue(!!ci, "We have an identity");
    browser.test.assertEq("foobar", ci.name, "identity.name is correct");
    browser.test.assertEq("red", ci.color, "identity.color is correct");
    browser.test.assertEq("icon", ci.icon, "identity.icon is correct");
    browser.test.assertTrue(!!ci.cookieStoreId, "identity.cookieStoreId is correct");

    ci = await browser.contextualIdentities.get(ci.cookieStoreId);
    browser.test.assertTrue(!!ci, "We have an identity");
    browser.test.assertEq("foobar", ci.name, "identity.name is correct");
    browser.test.assertEq("red", ci.color, "identity.color is correct");
    browser.test.assertEq("icon", ci.icon, "identity.icon is correct");

    cis = await browser.contextualIdentities.query({});
    browser.test.assertEq(5, cis.length, "now we have 5 identities");

    ci = await browser.contextualIdentities.update(ci.cookieStoreId, {name: "barfoo", color: "blue", icon: "icon icon"});
    browser.test.assertTrue(!!ci, "We have an identity");
    browser.test.assertEq("barfoo", ci.name, "identity.name is correct");
    browser.test.assertEq("blue", ci.color, "identity.color is correct");
    browser.test.assertEq("icon icon", ci.icon, "identity.icon is correct");

    ci = await browser.contextualIdentities.get(ci.cookieStoreId);
    browser.test.assertTrue(!!ci, "We have an identity");
    browser.test.assertEq("barfoo", ci.name, "identity.name is correct");
    browser.test.assertEq("blue", ci.color, "identity.color is correct");
    browser.test.assertEq("icon icon", ci.icon, "identity.icon is correct");

    ci = await browser.contextualIdentities.remove(ci.cookieStoreId);
    browser.test.assertTrue(!!ci, "We have an identity");
    browser.test.assertEq("barfoo", ci.name, "identity.name is correct");
    browser.test.assertEq("blue", ci.color, "identity.color is correct");
    browser.test.assertEq("icon icon", ci.icon, "identity.icon is correct");

    cis = await browser.contextualIdentities.query({});
    browser.test.assertEq(4, cis.length, "we are back to 4 identities");

    browser.test.notifyPass("contextualIdentities");
  }
  function makeExtension(id) {
    return ExtensionTestUtils.loadExtension({
      useAddonManager: "temporary",
      background,
      manifest: {
        applications: {
          gecko: {id},
        },
        permissions: ["contextualIdentities"],
      },
    });
  }

  let extension = makeExtension("containers-test@mozilla.org");
  await extension.startup();
  await extension.awaitFinish("contextualIdentities");
  equal(Services.prefs.getBoolPref(CONTAINERS_PREF), true, "Pref should now be enabled, whatever it's initial state");
  await extension.unload();
  equal(Services.prefs.getBoolPref(CONTAINERS_PREF), initial, "Pref should now be initial state");

  Services.prefs.clearUserPref(CONTAINERS_PREF);
});

add_task(async function test_contextualIdentity_extensions_enable_containers() {
  const CONTAINERS_PREF = "privacy.userContext.enabled";
  const initial = Services.prefs.getBoolPref(CONTAINERS_PREF);
  async function background(ver) {
    let ci = await browser.contextualIdentities.get("firefox-container-1");
    browser.test.assertTrue(!!ci, "We have an identity");

    browser.test.notifyPass("contextualIdentities");
  }
  function makeExtension(id) {
    return ExtensionTestUtils.loadExtension({
      useAddonManager: "temporary",
      background,
      manifest: {
        applications: {
          gecko: {id},
        },
        permissions: ["contextualIdentities"],
      },
    });
  }

  function waitForPrefChange(pref) {
    return new Promise(resolve => {
      function observeChange() {
        Services.prefs.removeObserver(pref, observeChange);
        resolve();
      }

      Services.prefs.addObserver(pref, observeChange);
    });
  }

  let extension = makeExtension("containers-test@mozilla.org");
  await extension.startup();
  await extension.awaitFinish("contextualIdentities");
  equal(Services.prefs.getBoolPref(CONTAINERS_PREF), true, "Pref should now be enabled, whatever it's initial state");
  const prefChange = waitForPrefChange(CONTAINERS_PREF);
  await extension.unload();
  // If pref was false we should wait for the pref to change back here.
  if (initial === false) {
    await prefChange;
  }
  equal(Services.prefs.getBoolPref(CONTAINERS_PREF), initial, "Pref should now be initial state");

  // Lets set containers explicitly to be off and test we keep it that way after removal
  Services.prefs.setBoolPref(CONTAINERS_PREF, false);
  let extension1 = makeExtension("containers-test-1@mozilla.org");
  await extension1.startup();
  await extension1.awaitFinish("contextualIdentities");
  equal(Services.prefs.getBoolPref(CONTAINERS_PREF), true, "Pref should now be enabled, whatever it's initial state");
  const prefChange1 = waitForPrefChange(CONTAINERS_PREF);
  await extension1.unload();
  // We explicitly have a pref that was set off, extensions turned it on
  // Lets wait until the pref flips back to the user set value of off
  await prefChange1;
  equal(Services.prefs.getBoolPref(CONTAINERS_PREF), false, "Pref should now be disabled, whatever it's initial state");

  // Lets set containers explicitly to be on and test we keep it that way after removal
  Services.prefs.setBoolPref(CONTAINERS_PREF, true);

  let extension2 = makeExtension("containers-test-2@mozilla.org");
  let extension3 = makeExtension("containers-test-3@mozilla.org");
  await extension2.startup();
  await extension2.awaitFinish("contextualIdentities");
  await extension3.startup();
  await extension3.awaitFinish("contextualIdentities");

  // Flip the ordering to check it's still enabled
  equal(Services.prefs.getBoolPref(CONTAINERS_PREF), true, "Pref should now be enabled 1");
  await extension3.unload();
  equal(Services.prefs.getBoolPref(CONTAINERS_PREF), true, "Pref should now be enabled 2");
  await extension2.unload();
  equal(Services.prefs.getBoolPref(CONTAINERS_PREF), true, "Pref should now be enabled 3");

  Services.prefs.clearUserPref(CONTAINERS_PREF);
});
