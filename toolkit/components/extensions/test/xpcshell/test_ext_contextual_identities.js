"use strict";

do_get_profile();

add_task(async function test_contextualIdentities_without_permissions() {
  function backgroundScript() {
    browser.test.assertTrue(!browser.contextualIdentities,
                            "contextualIdentities API is not available when the contextualIdentities permission is not required");
    browser.test.notifyPass("contextualIdentities_permission");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background: `(${backgroundScript})()`,
    manifest: {
      permissions: [],
    },
  });

  await extension.startup();
  await extension.awaitFinish("contextualIdentities_permission");
  await extension.unload();
});


add_task(async function test_contextualIdentity_no_containers() {
  async function backgroundScript() {
    let ci = await browser.contextualIdentities.get("foobar");
    browser.test.assertEq(false, ci, "No identity should be returned here");

    ci = await browser.contextualIdentities.get("firefox-container-1");
    browser.test.assertEq(false, ci, "We don't have any identity");

    let cis = await browser.contextualIdentities.query({});
    browser.test.assertEq(false, cis, "no containers, 0 containers");

    ci = await browser.contextualIdentities.create({name: "foobar", color: "red", icon: "icon"});
    browser.test.assertEq(false, ci, "We don't have any identity");

    ci = await browser.contextualIdentities.update("firefox-container-1", {name: "barfoo", color: "blue", icon: "icon icon"});
    browser.test.assertEq(false, ci, "We don't have any identity");

    ci = await browser.contextualIdentities.remove("firefox-container-1");
    browser.test.assertEq(false, ci, "We have an identity");

    browser.test.notifyPass("contextualIdentities");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background: `(${backgroundScript})()`,
    manifest: {
      permissions: ["contextualIdentities"],
    },
  });

  Services.prefs.setBoolPref("privacy.userContext.enabled", false);

  await extension.startup();
  await extension.awaitFinish("contextualIdentities");
  await extension.unload();

  Services.prefs.clearUserPref("privacy.userContext.enabled");
});

add_task(async function test_contextualIdentity_with_permissions() {
  async function backgroundScript() {
    let ci = await browser.contextualIdentities.get("foobar");
    browser.test.assertEq(null, ci, "No identity should be returned here");

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

  let extension = ExtensionTestUtils.loadExtension({
    background: `(${backgroundScript})()`,
    manifest: {
      permissions: ["contextualIdentities"],
    },
  });

  Services.prefs.setBoolPref("privacy.userContext.enabled", true);

  await extension.startup();
  await extension.awaitFinish("contextualIdentities");
  await extension.unload();

  Services.prefs.clearUserPref("privacy.userContext.enabled");
});
