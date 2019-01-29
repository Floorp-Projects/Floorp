/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const {ExtensionPermissions} = ChromeUtils.import("resource://gre/modules/ExtensionPermissions.jsm");

var gManagerWindow;

function get_test_items() {
  var items = {};

  for (let item of gManagerWindow.document.getElementById("addon-list").childNodes) {
    items[item.mAddon.id] = item;
  }

  return items;
}

function get(aId) {
  return gManagerWindow.document.getElementById(aId);
}

async function hasPrivateAllowed(id) {
  let perms = await ExtensionPermissions.get(id);
  return perms.permissions.length == 1 &&
         perms.permissions[0] == "internal:privateBrowsingAllowed";
}

add_task(async function test_addon() {
  await SpecialPowers.pushPrefEnv({set: [["extensions.allowPrivateBrowsingByDefault", false]]});

  let addons = new Map([
    ["@test-default", {
      useAddonManager: "temporary",
      manifest: {
        applications: {
          gecko: {id: "@test-default"},
        },
      },
    }],
    ["@test-override", {
      useAddonManager: "temporary",
      manifest: {
        applications: {
          gecko: {id: "@test-override"},
        },
      },
      incognitoOverride: "spanning",
    }],
    ["@test-override-permanent", {
      useAddonManager: "permanent",
      manifest: {
        applications: {
          gecko: {id: "@test-override-permanent"},
        },
      },
      incognitoOverride: "spanning",
    }],
    ["@test-not-allowed", {
      useAddonManager: "temporary",
      manifest: {
        applications: {
          gecko: {id: "@test-not-allowed"},
        },
        incognito: "not_allowed",
      },
    }],
  ]);
  let extensions = [];
  for (let definition of addons.values()) {
    let extension = ExtensionTestUtils.loadExtension(definition);
    extensions.push(extension);
    await extension.startup();
  }

  gManagerWindow = await open_manager("addons://list/extension");
  let doc = gManagerWindow.document;
  let items = get_test_items();
  for (let [id, definition] of addons.entries()) {
    ok(items[id], `${id} listed`);
    let badge = doc.getAnonymousElementByAttribute(items[id], "anonid", "privateBrowsing");
    if (definition.incognitoOverride == "spanning") {
      is_element_visible(badge, `private browsing badge is visible`);
    } else {
      is_element_hidden(badge, `private browsing badge is hidden`);
    }
  }
  await close_manager(gManagerWindow);

  for (let [id, definition] of addons.entries()) {
    gManagerWindow = await open_manager("addons://detail/" + encodeURIComponent(id));
    ok(true, `==== ${id} detail opened`);
    if (definition.manifest.incognito == "not_allowed") {
      is_element_hidden(get("detail-privateBrowsing-row"), "Private browsing should be hidden");
      is_element_hidden(get("detail-privateBrowsing-row-footer"), "Private browsing footer should be hidden");
      ok(!await hasPrivateAllowed(id), "Private browsing permission not set");
    } else {
      is_element_visible(get("detail-privateBrowsing-row"), "Private browsing should be visible");
      is_element_visible(get("detail-privateBrowsing-row-footer"), "Private browsing footer should be visible");
      let privateBrowsing = gManagerWindow.document.getElementById("detail-privateBrowsing");
      if (definition.incognitoOverride == "spanning") {
        is(privateBrowsing.value, "1", "Private browsing should be on");
        ok(await hasPrivateAllowed(id), "Private browsing permission set");
        EventUtils.synthesizeMouseAtCenter(privateBrowsing.lastChild, { clickCount: 1 }, gManagerWindow);
        await TestUtils.waitForCondition(() => privateBrowsing.value == "0");
        is(privateBrowsing.value, "0", "Private browsing should be off");
        ok(!await hasPrivateAllowed(id), "Private browsing permission removed");
      } else {
        is(privateBrowsing.value, "0", "Private browsing should be off");
        ok(!await hasPrivateAllowed(id), "Private browsing permission not set");
        EventUtils.synthesizeMouseAtCenter(privateBrowsing.firstChild, { clickCount: 1 }, gManagerWindow);
        await TestUtils.waitForCondition(() => privateBrowsing.value == "1");
        is(privateBrowsing.value, "1", "Private browsing should be on");
        ok(await hasPrivateAllowed(id), "Private browsing permission set");
      }
    }
    await close_manager(gManagerWindow);
  }

  for (let extension of extensions) {
    await extension.unload();
  }
  Services.prefs.clearUserPref("extensions.allowPrivateBrowsingByDefault");
});
