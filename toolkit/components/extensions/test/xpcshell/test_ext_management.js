/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function setup() {
  await ExtensionTestUtils.startAddonManager();
});

add_task(async function test_management_getAll() {
  const id1 = "get_all_test1@tests.mozilla.com";
  const id2 = "get_all_test2@tests.mozilla.com";

  function getManifest(id) {
    return {
      applications: {
        gecko: {
          id,
        },
      },
      name: id,
      version: "1.0",
      short_name: id,
      permissions: ["management"],
    };
  }

  async function background() {
    browser.test.onMessage.addListener(async (msg, id) => {
      let addon = await browser.management.get(id);
      browser.test.sendMessage("addon", addon);
    });

    let addons = await browser.management.getAll();
    browser.test.assertEq(addons.length, 3, "management.getAll returned three add-ons.");
    browser.test.sendMessage("addons", addons);
  }

  let extension1 = ExtensionTestUtils.loadExtension({
    manifest: getManifest(id1),
    useAddonManager: "temporary",
  });

  let extension2 = ExtensionTestUtils.loadExtension({
    manifest: getManifest(id2),
    background,
    useAddonManager: "temporary",
  });

  await extension1.startup();
  await extension2.startup();

  let addons = await extension2.awaitMessage("addons");
  for (let id of [id1, id2]) {
    let addon = addons.find(a => { return a.id === id; });
    equal(addon.name, id, `The extension with id ${id} was returned by getAll.`);
    equal(addon.shortName, id, "Additional extension metadata was correct");
  }

  extension2.sendMessage("getAddon", id1);
  let addon = await extension2.awaitMessage("addon");
  equal(addon.name, id1, `The extension with id ${id1} was returned by get.`);
  equal(addon.shortName, id1, "Additional extension metadata was correct");

  extension2.sendMessage("getAddon", id2);
  addon = await extension2.awaitMessage("addon");
  equal(addon.name, id2, `The extension with id ${id2} was returned by get.`);
  equal(addon.shortName, id2, "Additional extension metadata was correct");

  await extension2.unload();
  await extension1.unload();
});
