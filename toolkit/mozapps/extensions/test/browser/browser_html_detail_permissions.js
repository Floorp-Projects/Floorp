/* eslint max-len: ["error", 80] */

const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);
const { ExtensionPermissions } = ChromeUtils.import(
  "resource://gre/modules/ExtensionPermissions.jsm"
);

AddonTestUtils.initMochitest(this);

async function background() {
  browser.permissions.onAdded.addListener(perms => {
    browser.test.sendMessage("permission-added", perms);
  });
  browser.permissions.onRemoved.addListener(perms => {
    browser.test.sendMessage("permission-removed", perms);
  });
}

async function getExtensions({ manifest_version = 2 } = {}) {
  let extensions = {
    "addon0@mochi.test": ExtensionTestUtils.loadExtension({
      manifest: {
        manifest_version,
        name: "Test add-on 0",
        applications: { gecko: { id: "addon0@mochi.test" } },
        permissions: ["alarms", "contextMenus"],
      },
      background,
      useAddonManager: "temporary",
    }),
    "addon1@mochi.test": ExtensionTestUtils.loadExtension({
      manifest: {
        manifest_version,
        name: "Test add-on 1",
        applications: { gecko: { id: "addon1@mochi.test" } },
        permissions: ["alarms", "contextMenus", "tabs", "webNavigation"],
        // Note: for easier testing, we merge host_permissions into permissions
        // when loading mv2 extensions, see ExtensionTestCommon.generateFiles.
        host_permissions: ["<all_urls>", "file://*/*"],
      },
      background,
      useAddonManager: "temporary",
    }),
    "addon2@mochi.test": ExtensionTestUtils.loadExtension({
      manifest: {
        manifest_version,
        name: "Test add-on 2",
        applications: { gecko: { id: "addon2@mochi.test" } },
        permissions: ["alarms", "contextMenus"],
        optional_permissions: ["http://mochi.test/*"],
      },
      background,
      useAddonManager: "temporary",
    }),
    "addon3@mochi.test": ExtensionTestUtils.loadExtension({
      manifest: {
        manifest_version,
        name: "Test add-on 3",
        version: "1.0",
        applications: { gecko: { id: "addon3@mochi.test" } },
        permissions: ["tabs"],
        optional_permissions: ["webNavigation", "<all_urls>"],
      },
      background,
      useAddonManager: "temporary",
    }),
    "addon4@mochi.test": ExtensionTestUtils.loadExtension({
      manifest: {
        manifest_version,
        name: "Test add-on 4",
        applications: { gecko: { id: "addon4@mochi.test" } },
        optional_permissions: ["tabs", "webNavigation"],
      },
      background,
      useAddonManager: "temporary",
    }),
    "addon5@mochi.test": ExtensionTestUtils.loadExtension({
      manifest: {
        manifest_version,
        name: "Test add-on 5",
        applications: { gecko: { id: "addon5@mochi.test" } },
        optional_permissions: ["*://*/*"],
      },
      background,
      useAddonManager: "temporary",
    }),
    "priv6@mochi.test": ExtensionTestUtils.loadExtension({
      isPrivileged: true,
      manifest: {
        manifest_version,
        name: "Privileged add-on 6",
        applications: { gecko: { id: "priv6@mochi.test" } },
        optional_permissions: [
          "file://*/*",
          "about:reader*",
          "resource://pdf.js/*",
          "*://*.mozilla.com/*",
          "*://*/*",
          "<all_urls>",
        ],
      },
      background,
      useAddonManager: "temporary",
    }),
    "addon7@mochi.test": ExtensionTestUtils.loadExtension({
      manifest: {
        manifest_version,
        name: "Test add-on 7",
        applications: { gecko: { id: "addon7@mochi.test" } },
        optional_permissions: ["<all_urls>", "*://*/*", "file://*/*"],
      },
      background,
      useAddonManager: "temporary",
    }),
    "addon8@mochi.test": ExtensionTestUtils.loadExtension({
      manifest: {
        manifest_version,
        name: "Test add-on 8",
        applications: { gecko: { id: "addon8@mochi.test" } },
        optional_permissions: ["*://*/*", "file://*/*", "<all_urls>"],
      },
      background,
      useAddonManager: "temporary",
    }),
    "other@mochi.test": ExtensionTestUtils.loadExtension({
      manifest: {
        manifest_version,
        name: "Test add-on 6",
        applications: { gecko: { id: "other@mochi.test" } },
        optional_permissions: [
          "tabs",
          "webNavigation",
          "<all_urls>",
          "*://*/*",
        ],
      },
      useAddonManager: "temporary",
    }),
  };
  for (let ext of Object.values(extensions)) {
    await ext.startup();
  }
  return extensions;
}

async function runTest(options) {
  let {
    extension,
    addonId,
    permissions = [],
    optional_permissions = [],
    optional_enabled = [],
    // Map<permission->string> to check optional_permissions against, if set.
    optional_strings = {},
    view,
  } = options;
  if (extension) {
    addonId = extension.id;
  }

  let win = view || (await loadInitialView("extension"));

  let card = getAddonCard(win, addonId);
  let permsSection = card.querySelector("addon-permissions-list");
  if (!permsSection) {
    ok(!card.hasAttribute("expanded"), "The list card is not expanded");
    let loaded = waitForViewLoad(win);
    card.querySelector('[action="expand"]').click();
    await loaded;
  }

  card = getAddonCard(win, addonId);
  let { deck, tabGroup } = card.details;

  let permsBtn = tabGroup.querySelector('[name="permissions"]');
  let permsShown = BrowserTestUtils.waitForEvent(deck, "view-changed");
  permsBtn.click();
  await permsShown;

  permsSection = card.querySelector("addon-permissions-list");

  let rows = Array.from(permsSection.querySelectorAll(".addon-detail-row"));
  let permission_rows = Array.from(
    permsSection.querySelectorAll(".permission-info")
  );

  // Last row is the learn more link.
  info("Check learn more link");
  let link = rows[rows.length - 1].firstElementChild;
  let rootUrl = Services.urlFormatter.formatURLPref("app.support.baseURL");
  let url = rootUrl + "extension-permissions";
  is(link.href, url, "The URL is set");
  is(link.getAttribute("target"), "_blank", "The link opens in a new tab");

  // We should have one more row (learn more) that the combined permissions,
  // or if no permissions, 2 rows.
  let num_permissions = permissions.length + optional_permissions.length;
  is(
    permission_rows.length,
    num_permissions,
    "correct number of details rows are present"
  );

  info("Check displayed permissions");
  if (!num_permissions) {
    is(
      win.document.l10n.getAttributes(rows[0]).id,
      "addon-permissions-empty",
      "There's a message when no permissions are shown"
    );
  }
  if (permissions.length) {
    for (let name of permissions) {
      // Check the permission-info class to make sure it's for a permission.
      let row = permission_rows.shift();
      ok(
        row.classList.contains("permission-info"),
        `required permission row for ${name}`
      );
    }
  }

  let addon = await AddonManager.getAddonByID(addonId);
  info(`addon ${addon.id} is ${addon.userDisabled ? "disabled" : "enabled"}`);

  function waitForPermissionChange(id) {
    return new Promise(resolve => {
      info(`listening for change on ${id}`);
      let listener = (type, data) => {
        info(`change permissions ${JSON.stringify(data)}`);
        if (data.extensionId !== id) {
          return;
        }
        ExtensionPermissions.removeListener(listener);
        resolve(data);
      };
      ExtensionPermissions.addListener(listener);
    });
  }

  // This tests the permission change and button state when the user
  // changes the state in about:addons.
  async function testTogglePermissionButton(
    permission,
    button,
    excpectDisabled = false
  ) {
    let enabled = optional_enabled.includes(permission);
    if (excpectDisabled) {
      enabled = !enabled;
    }
    is(
      button.checked,
      enabled,
      `permission is set correctly for ${permission}: ${button.checked}`
    );
    let change;
    if (addon.userDisabled || !extension) {
      change = waitForPermissionChange(addonId);
    } else if (!enabled) {
      change = extension.awaitMessage("permission-added");
    } else {
      change = extension.awaitMessage("permission-removed");
    }

    button.click();

    let perms = await change;
    if (addon.userDisabled || !extension) {
      perms = enabled ? perms.removed : perms.added;
    }
    ok(
      perms.permissions.includes(permission) ||
        perms.origins.includes(permission),
      "permission was toggled"
    );

    await BrowserTestUtils.waitForCondition(async () => {
      return button.checked == !enabled;
    }, "button changed state");
  }

  // This tests that the button changes state if the permission is
  // changed outside of about:addons
  async function testExternalPermissionChange(permission, button) {
    let enabled = button.checked;
    let type = button.getAttribute("permission-type");
    let change;
    if (addon.userDisabled || !extension) {
      change = waitForPermissionChange(addonId);
    } else if (!enabled) {
      change = extension.awaitMessage("permission-added");
    } else {
      change = extension.awaitMessage("permission-removed");
    }

    let permissions = { permissions: [], origins: [] };
    if (type == "origin") {
      permissions.origins = [permission];
    } else {
      permissions.permissions = [permission];
    }

    if (enabled) {
      await ExtensionPermissions.remove(addonId, permissions);
    } else {
      await ExtensionPermissions.add(addonId, permissions);
    }

    let perms = await change;
    if (addon.userDisabled || !extension) {
      perms = enabled ? perms.removed : perms.added;
    }
    ok(
      perms.permissions.includes(permission) ||
        perms.origins.includes(permission),
      "permission was toggled"
    );

    await BrowserTestUtils.waitForCondition(async () => {
      return button.checked == !enabled;
    }, "button changed state");
  }

  // This tests that changing the permission on another addon does
  // not change the UI for the addon we're testing.
  async function testOtherPermissionChange(permission, toggle) {
    let type = toggle.getAttribute("permission-type");
    let otherId = "other@mochi.test";
    let change = waitForPermissionChange(otherId);
    let perms = await ExtensionPermissions.get(otherId);
    let existing = type == "origin" ? perms.origins : perms.permissions;
    let permissions = { permissions: [], origins: [] };
    if (type == "origin") {
      permissions.origins = [permission];
    } else {
      permissions.permissions = [permission];
    }

    if (existing.includes(permission)) {
      await ExtensionPermissions.remove(otherId, permissions);
    } else {
      await ExtensionPermissions.add(otherId, permissions);
    }
    await change;
  }

  if (optional_permissions.length) {
    for (let name of optional_permissions) {
      // Check the row is a permission row with the correct key on the toggle
      // control.
      let row = permission_rows.shift();
      let label = row.firstElementChild;

      let str = optional_strings[name];
      if (str) {
        is(label.textContent, str, `Expected permission string ${str}`);
      }

      let toggle = label.lastElementChild;
      ok(
        row.classList.contains("permission-info"),
        `optional permission row for ${name}`
      );
      is(
        toggle.getAttribute("permission-key"),
        name,
        `optional permission toggle exists for ${name}`
      );
      await testTogglePermissionButton(name, toggle);
      await testTogglePermissionButton(name, toggle, true);

      // make a change "outside" the UI and check the values.
      // toggle twice to test both add/remove.
      await testExternalPermissionChange(name, toggle);
      // change another addon to mess around with optional permission
      // values to see if it effects the addon we're testing here.  The
      // next check would fail if anythign bleeds onto other addons.
      await testOtherPermissionChange(name, toggle);
      // repeate the "outside" test.
      await testExternalPermissionChange(name, toggle);
    }
  }

  if (!view) {
    await closeView(win);
  }
}

async function testPermissionsView({ manifestV3enabled, manifest_version }) {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.manifestV3.enabled", manifestV3enabled]],
  });

  // pre-set a permission prior to starting extensions.
  await ExtensionPermissions.add("addon4@mochi.test", {
    permissions: ["tabs"],
    origins: [],
  });

  let extensions = await getExtensions({ manifest_version });

  info("Check add-on with required permissions");
  if (manifest_version < 3) {
    await runTest({
      extension: extensions["addon1@mochi.test"],
      permissions: ["<all_urls>", "tabs", "webNavigation"],
    });
  } else {
    await runTest({
      extension: extensions["addon1@mochi.test"],
      permissions: ["tabs", "webNavigation"],
      optional_permissions: ["<all_urls>"],
    });
  }

  info("Check add-on without any displayable permissions");
  await runTest({ extension: extensions["addon0@mochi.test"] });

  info("Check add-on with only one optional origin.");
  await runTest({
    extension: extensions["addon2@mochi.test"],
    optional_permissions: manifestV3enabled ? ["http://mochi.test/*"] : [],
    optional_strings: {
      "http://mochi.test/*": "Access your data for http://mochi.test",
    },
  });

  info("Check add-on with both required and optional permissions");
  await runTest({
    extension: extensions["addon3@mochi.test"],
    permissions: ["tabs"],
    optional_permissions: ["webNavigation", "<all_urls>"],
  });

  // Grant a specific optional host permission not listed in the manifest.
  await ExtensionPermissions.add("addon3@mochi.test", {
    permissions: [],
    origins: ["https://example.com/*"],
  });
  extensions["addon3@mochi.test"].awaitMessage("permission-added");

  info("Check addon3 again and expect the new optional host permission");
  await runTest({
    extension: extensions["addon3@mochi.test"],
    permissions: ["tabs"],
    optional_permissions: [
      "webNavigation",
      "<all_urls>",
      ...(manifestV3enabled ? ["https://example.com/*"] : []),
    ],
    optional_enabled: ["https://example.com/*"],
    optional_strings: {
      "https://example.com/*": "Access your data for https://example.com",
    },
  });

  info("Check add-on with only optional permissions, tabs is pre-enabled");
  await runTest({
    extension: extensions["addon4@mochi.test"],
    optional_permissions: ["tabs", "webNavigation"],
    optional_enabled: ["tabs"],
  });

  info("Check add-on with a global match pattern in place of all urls");
  await runTest({
    extension: extensions["addon5@mochi.test"],
    optional_permissions: ["*://*/*"],
  });

  info("Check privileged add-on with non-web origin permissions");
  await runTest({
    extension: extensions["priv6@mochi.test"],
    optional_permissions: [
      "<all_urls>",
      ...(manifestV3enabled ? ["*://*.mozilla.com/*"] : []),
    ],
    optional_strings: {
      "*://*.mozilla.com/*":
        "Access your data for sites in the *://mozilla.com domain",
    },
  });

  info(`Check that <all_urls> is used over other "all websites" permissions`);
  await runTest({
    extension: extensions["addon7@mochi.test"],
    optional_permissions: ["<all_urls>"],
  });

  info(`And again with permissions in the opposite order in the manifest`);
  await runTest({
    extension: extensions["addon8@mochi.test"],
    optional_permissions: ["<all_urls>"],
  });

  for (let ext of Object.values(extensions)) {
    await ext.unload();
  }

  await SpecialPowers.popPrefEnv();
}

add_task(async function testPermissionsView_MV2_manifestV3disabled() {
  await testPermissionsView({ manifestV3enabled: false, manifest_version: 2 });
});

add_task(async function testPermissionsView_MV2_manifestV3enabled() {
  await testPermissionsView({ manifestV3enabled: true, manifest_version: 2 });
});

add_task(async function testPermissionsView_MV3() {
  await testPermissionsView({ manifestV3enabled: true, manifest_version: 3 });
});

add_task(async function testPermissionsViewStates() {
  let ID = "addon@mochi.test";

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "Test add-on 3",
      version: "1.0",
      applications: { gecko: { id: ID } },
      permissions: ["tabs"],
      optional_permissions: ["webNavigation", "<all_urls>"],
    },
    useAddonManager: "permanent",
  });
  await extension.startup();

  info(
    "Check toggling permissions on a disabled addon with addon3@mochi.test."
  );
  let view = await loadInitialView("extension");
  let addon = await AddonManager.getAddonByID(ID);
  await addon.disable();
  ok(addon.userDisabled, "addon is disabled");
  await runTest({
    extension,
    permissions: ["tabs"],
    optional_permissions: ["webNavigation", "<all_urls>"],
    view,
  });
  await addon.enable();
  ok(!addon.userDisabled, "addon is enabled");

  async function install_addon(extensionData) {
    let xpi = await AddonTestUtils.createTempWebExtensionFile(extensionData);
    let { addon } = await AddonTestUtils.promiseInstallFile(xpi);
    return addon;
  }

  function wait_for_addon_item_updated(addonId) {
    return BrowserTestUtils.waitForEvent(getAddonCard(view, addonId), "update");
  }

  let promiseItemUpdated = wait_for_addon_item_updated(ID);
  addon = await install_addon({
    manifest: {
      name: "Test add-on 3",
      version: "2.0",
      applications: { gecko: { id: ID } },
      optional_permissions: ["webNavigation"],
    },
    useAddonManager: "permanent",
  });
  is(addon.version, "2.0", "addon upgraded");
  await promiseItemUpdated;

  await runTest({
    addonId: addon.id,
    optional_permissions: ["webNavigation"],
    view,
  });

  // While the view is still available, test setting a permission
  // that is not in the manifest of the addon.
  let card = getAddonCard(view, addon.id);
  await Assert.rejects(
    card.setAddonPermission("webRequest", "permission", "add"),
    /permission missing from manifest/,
    "unable to set the addon permission"
  );

  await closeView(view);
  await extension.unload();
});

add_task(async function testAllUrlsNotGrantedUnconditionally_MV3() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.manifestV3.enabled", true]],
  });

  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      manifest_version: 3,
      host_permissions: ["<all_urls>"],
    },
    async background() {
      const perms = await browser.permissions.getAll();
      browser.test.sendMessage("granted-permissions", perms);
    },
  });

  await extension.startup();
  const perms = await extension.awaitMessage("granted-permissions");
  ok(
    !perms.origins.includes("<all_urls>"),
    "Optional <all_urls> should not be granted as host permission yet"
  );
  ok(
    !perms.permissions.includes("<all_urls>"),
    "Optional <all_urls> should not be granted as an API permission neither"
  );

  await extension.unload();
  await SpecialPowers.popPrefEnv();
});
