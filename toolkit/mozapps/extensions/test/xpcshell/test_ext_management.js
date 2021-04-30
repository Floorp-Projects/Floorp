"use strict";

add_task(async function setup() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "48", "48");
  await promiseStartupManager();
});

/* eslint-disable no-undef */
// Shared background function for getSelf tests
function backgroundGetSelf() {
  browser.management.getSelf().then(
    extInfo => {
      let url = browser.runtime.getURL("*");
      extInfo.hostPermissions = extInfo.hostPermissions.filter(i => i != url);
      extInfo.url = browser.runtime.getURL("");
      browser.test.sendMessage("management-getSelf", extInfo);
    },
    error => {
      browser.test.notifyFail(`getSelf rejected with error: ${error}`);
    }
  );
}
/* eslint-enable no-undef */

add_task(async function test_management_get_self_complete() {
  const id = "get_self_test_complete@tests.mozilla.com";
  const permissions = ["management", "cookies"];
  const hostPermissions = ["*://example.org/*", "https://foo.example.org/*"];

  let manifest = {
    applications: {
      gecko: {
        id,
        update_url: "https://updates.mozilla.com/",
      },
    },
    name: "test extension name",
    short_name: "test extension short name",
    description: "test extension description",
    version: "1.0",
    homepage_url: "http://www.example.com/",
    options_ui: {
      page: "get_self_options.html",
    },
    icons: {
      "16": "icons/icon-16.png",
      "48": "icons/icon-48.png",
    },
    permissions: [...permissions, ...hostPermissions],
  };

  let extension = ExtensionTestUtils.loadExtension({
    manifest,
    background: backgroundGetSelf,
    useAddonManager: "temporary",
  });
  await extension.startup();
  let extInfo = await extension.awaitMessage("management-getSelf");

  equal(extInfo.id, id, "getSelf returned the expected id");
  equal(
    extInfo.installType,
    "development",
    "getSelf returned the expected installType"
  );
  for (let prop of ["name", "description", "version"]) {
    equal(
      extInfo[prop],
      manifest[prop],
      `getSelf returned the expected ${prop}`
    );
  }
  equal(
    extInfo.shortName,
    manifest.short_name,
    "getSelf returned the expected shortName"
  );
  equal(
    extInfo.mayDisable,
    true,
    "getSelf returned the expected value for mayDisable"
  );
  equal(
    extInfo.enabled,
    true,
    "getSelf returned the expected value for enabled"
  );
  equal(
    extInfo.homepageUrl,
    manifest.homepage_url,
    "getSelf returned the expected homepageUrl"
  );
  equal(
    extInfo.updateUrl,
    manifest.applications.gecko.update_url,
    "getSelf returned the expected updateUrl"
  );
  ok(
    extInfo.optionsUrl.endsWith(manifest.options_ui.page),
    "getSelf returned the expected optionsUrl"
  );
  for (let [index, size] of Object.keys(manifest.icons)
    .sort()
    .entries()) {
    let iconUrl = `${extInfo.url}${manifest.icons[size]}`;
    equal(
      extInfo.icons[index].size,
      +size,
      "getSelf returned the expected icon size"
    );
    equal(
      extInfo.icons[index].url,
      iconUrl,
      "getSelf returned the expected icon url"
    );
  }
  deepEqual(
    extInfo.permissions.sort(),
    permissions.sort(),
    "getSelf returned the expected permissions"
  );
  deepEqual(
    extInfo.hostPermissions.sort(),
    hostPermissions.sort(),
    "getSelf returned the expected hostPermissions"
  );
  equal(
    extInfo.installType,
    "development",
    "getSelf returned the expected installType"
  );
  await extension.unload();
});

add_task(async function test_management_get_self_minimal() {
  const id = "get_self_test_minimal@tests.mozilla.com";

  let manifest = {
    applications: {
      gecko: {
        id,
      },
    },
    name: "test extension name",
    version: "1.0",
  };

  let extension = ExtensionTestUtils.loadExtension({
    manifest,
    background: backgroundGetSelf,
    useAddonManager: "temporary",
  });
  await extension.startup();
  let extInfo = await extension.awaitMessage("management-getSelf");

  equal(extInfo.id, id, "getSelf returned the expected id");
  equal(
    extInfo.installType,
    "development",
    "getSelf returned the expected installType"
  );
  for (let prop of ["name", "version"]) {
    equal(
      extInfo[prop],
      manifest[prop],
      `getSelf returned the expected ${prop}`
    );
  }
  for (let prop of ["shortName", "description", "optionsUrl"]) {
    equal(extInfo[prop], "", `getSelf returned the expected ${prop}`);
  }
  for (let prop of ["homepageUrl", " updateUrl", "icons"]) {
    equal(
      Reflect.getOwnPropertyDescriptor(extInfo, prop),
      undefined,
      `getSelf did not return a ${prop} property`
    );
  }
  for (let prop of ["permissions", "hostPermissions"]) {
    deepEqual(extInfo[prop], [], `getSelf returned the expected ${prop}`);
  }
  await extension.unload();
});

add_task(async function test_management_get_self_permanent() {
  const id = "get_self_test_permanent@tests.mozilla.com";

  let manifest = {
    applications: {
      gecko: {
        id,
      },
    },
    name: "test extension name",
    version: "1.0",
  };

  let extension = ExtensionTestUtils.loadExtension({
    manifest,
    background: backgroundGetSelf,
    useAddonManager: "permanent",
  });
  await extension.startup();
  let extInfo = await extension.awaitMessage("management-getSelf");

  equal(extInfo.id, id, "getSelf returned the expected id");
  equal(
    extInfo.installType,
    "normal",
    "getSelf returned the expected installType"
  );
  await extension.unload();
});
