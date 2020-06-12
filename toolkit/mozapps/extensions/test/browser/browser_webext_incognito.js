/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);
const { ExtensionPermissions } = ChromeUtils.import(
  "resource://gre/modules/ExtensionPermissions.jsm"
);
const { Management } = ChromeUtils.import(
  "resource://gre/modules/Extension.jsm"
);

var gManagerWindow;

AddonTestUtils.initMochitest(this);

function get_test_items() {
  var items = {};

  for (let item of gManagerWindow
    .getHtmlBrowser()
    .contentDocument.querySelectorAll("addon-card")) {
    items[item.getAttribute("addon-id")] = item;
  }

  return items;
}

function getHtmlElem(selector) {
  return gManagerWindow
    .getHtmlBrowser()
    .contentDocument.querySelector(selector);
}

function getPrivateBrowsingBadge(card) {
  return card.querySelector(".addon-badge-private-browsing-allowed");
}

function getPreferencesButtonAtListView(card) {
  return card.querySelector("panel-item[action='preferences']");
}

function getPreferencesButtonAtDetailsView() {
  return getHtmlElem("panel-item[action='preferences']");
}

function isInlineOptionsVisible() {
  // The following button is used to open the inline options browser.
  return !getHtmlElem(".tab-button[name='preferences']").hidden;
}

function getPrivateBrowsingValue() {
  return getHtmlElem("input[type='radio'][name='private-browsing']:checked")
    .value;
}

async function setPrivateBrowsingValue(value, id) {
  let changePromise = new Promise(resolve => {
    const listener = (type, { extensionId, added, removed }) => {
      if (extensionId == id) {
        // Let's make sure we received the right message
        let { permissions } = value == "0" ? removed : added;
        ok(permissions.includes("internal:privateBrowsingAllowed"));
        resolve();
      }
    };
    Management.once("change-permissions", listener);
  });
  let radio = getHtmlElem(
    `input[type="radio"][name="private-browsing"][value="${value}"]`
  );
  EventUtils.synthesizeMouseAtCenter(
    radio,
    { clickCount: 1 },
    radio.ownerGlobal
  );
  // Let's make sure we wait until the change has peristed in the database
  return changePromise;
}

// Check whether the private browsing inputs are visible in the details view.
function checkIsModifiable(expected) {
  if (expected) {
    is_element_visible(
      getHtmlElem(".addon-detail-row-private-browsing"),
      "Private browsing should be visible"
    );
  } else {
    is_element_hidden(
      getHtmlElem(".addon-detail-row-private-browsing"),
      "Private browsing should be hidden"
    );
  }
  checkHelpRow(".addon-detail-row-private-browsing", expected);
}

// Check whether the details view shows that private browsing is forcibly disallowed.
function checkIsDisallowed(expected) {
  if (expected) {
    is_element_visible(
      getHtmlElem(".addon-detail-row-private-browsing-disallowed"),
      "Private browsing should be disallowed"
    );
  } else {
    is_element_hidden(
      getHtmlElem(".addon-detail-row-private-browsing-disallowed"),
      "Private browsing should not be disallowed"
    );
  }
  checkHelpRow(".addon-detail-row-private-browsing-disallowed", expected);
}

// Check whether the details view shows that private browsing is forcibly allowed.
function checkIsRequired(expected) {
  if (expected) {
    is_element_visible(
      getHtmlElem(".addon-detail-row-private-browsing-required"),
      "Private browsing should be required"
    );
  } else {
    is_element_hidden(
      getHtmlElem(".addon-detail-row-private-browsing-required"),
      "Private browsing should not be required"
    );
  }
  checkHelpRow(".addon-detail-row-private-browsing-required", expected);
}

function checkHelpRow(selector, expected) {
  let helpRow = getHtmlElem(`${selector} + .addon-detail-help-row`);
  if (expected) {
    is_element_visible(helpRow, `Help row should be shown: ${selector}`);
    is_element_visible(helpRow.querySelector("a"), "Expected learn more link");
  } else {
    is_element_hidden(helpRow, `Help row should be hidden: ${selector}`);
  }
}

async function hasPrivateAllowed(id) {
  let perms = await ExtensionPermissions.get(id);
  return (
    perms.permissions.length == 1 &&
    perms.permissions[0] == "internal:privateBrowsingAllowed"
  );
}

add_task(function clearInitialTelemetry() {
  // Clear out any telemetry data that existed before this file is run.
  Services.telemetry.clearEvents();
});

add_task(async function test_badge_and_toggle_incognito() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.allowPrivateBrowsingByDefault", false]],
  });

  let addons = new Map([
    [
      "@test-default",
      {
        useAddonManager: "temporary",
        manifest: {
          applications: {
            gecko: { id: "@test-default" },
          },
        },
      },
    ],
    [
      "@test-override",
      {
        useAddonManager: "temporary",
        manifest: {
          applications: {
            gecko: { id: "@test-override" },
          },
        },
        incognitoOverride: "spanning",
      },
    ],
    [
      "@test-override-permanent",
      {
        useAddonManager: "permanent",
        manifest: {
          applications: {
            gecko: { id: "@test-override-permanent" },
          },
        },
        incognitoOverride: "spanning",
      },
    ],
    [
      "@test-not-allowed",
      {
        useAddonManager: "temporary",
        manifest: {
          applications: {
            gecko: { id: "@test-not-allowed" },
          },
          incognito: "not_allowed",
        },
      },
    ],
  ]);
  let extensions = [];
  for (let definition of addons.values()) {
    let extension = ExtensionTestUtils.loadExtension(definition);
    extensions.push(extension);
    await extension.startup();
  }

  gManagerWindow = await open_manager("addons://list/extension");
  let items = get_test_items();
  for (let [id, definition] of addons.entries()) {
    ok(items[id], `${id} listed`);
    let badge = getPrivateBrowsingBadge(items[id]);
    if (definition.incognitoOverride == "spanning") {
      is_element_visible(badge, `private browsing badge is visible`);
    } else {
      is_element_hidden(badge, `private browsing badge is hidden`);
    }
  }
  await close_manager(gManagerWindow);

  for (let [id, definition] of addons.entries()) {
    gManagerWindow = await open_manager(
      "addons://detail/" + encodeURIComponent(id)
    );
    ok(true, `==== ${id} detail opened`);
    if (definition.manifest.incognito == "not_allowed") {
      checkIsModifiable(false);
      ok(!(await hasPrivateAllowed(id)), "Private browsing permission not set");
      checkIsDisallowed(true);
    } else {
      // This assumes PERM_CAN_CHANGE_PRIVATEBROWSING_ACCESS, we test other options in a later test in this file.
      checkIsModifiable(true);
      if (definition.incognitoOverride == "spanning") {
        is(getPrivateBrowsingValue(), "1", "Private browsing should be on");
        ok(await hasPrivateAllowed(id), "Private browsing permission set");
        await setPrivateBrowsingValue("0", id);
        is(getPrivateBrowsingValue(), "0", "Private browsing should be off");
        ok(
          !(await hasPrivateAllowed(id)),
          "Private browsing permission removed"
        );
      } else {
        is(getPrivateBrowsingValue(), "0", "Private browsing should be off");
        ok(
          !(await hasPrivateAllowed(id)),
          "Private browsing permission not set"
        );
        await setPrivateBrowsingValue("1", id);
        is(getPrivateBrowsingValue(), "1", "Private browsing should be on");
        ok(await hasPrivateAllowed(id), "Private browsing permission set");
      }
    }
    await close_manager(gManagerWindow);
  }

  for (let extension of extensions) {
    await extension.unload();
  }

  const expectedExtras = {
    action: "privateBrowsingAllowed",
    view: "detail",
    type: "extension",
  };

  assertAboutAddonsTelemetryEvents(
    [
      [
        "addonsManager",
        "action",
        "aboutAddons",
        "on",
        { ...expectedExtras, addonId: "@test-default" },
      ],
      [
        "addonsManager",
        "action",
        "aboutAddons",
        "off",
        { ...expectedExtras, addonId: "@test-override" },
      ],
      [
        "addonsManager",
        "action",
        "aboutAddons",
        "off",
        { ...expectedExtras, addonId: "@test-override-permanent" },
      ],
    ],
    { methods: ["action"] }
  );
});

add_task(async function test_addon_preferences_button() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.allowPrivateBrowsingByDefault", false]],
  });

  let addons = new Map([
    [
      "test-inline-options@mozilla.com",
      {
        useAddonManager: "temporary",
        manifest: {
          name: "Extension with inline options",
          applications: { gecko: { id: "test-inline-options@mozilla.com" } },
          options_ui: { page: "options.html", open_in_tab: false },
        },
      },
    ],
    [
      "test-newtab-options@mozilla.com",
      {
        useAddonManager: "temporary",
        manifest: {
          name: "Extension with options page in a new tab",
          applications: { gecko: { id: "test-newtab-options@mozilla.com" } },
          options_ui: { page: "options.html", open_in_tab: true },
        },
      },
    ],
    [
      "test-not-allowed@mozilla.com",
      {
        useAddonManager: "temporary",
        manifest: {
          name: "Extension not allowed in PB windows",
          incognito: "not_allowed",
          applications: { gecko: { id: "test-not-allowed@mozilla.com" } },
          options_ui: { page: "options.html", open_in_tab: true },
        },
      },
    ],
  ]);

  async function runTest(openInPrivateWin) {
    const win = await BrowserTestUtils.openNewBrowserWindow({
      private: openInPrivateWin,
    });

    gManagerWindow = await open_manager(
      "addons://list/extension",
      undefined,
      undefined,
      undefined,
      win
    );

    const checkPrefsVisibility = (id, hasInlinePrefs, expectVisible) => {
      if (!hasInlinePrefs) {
        const detailsPrefBtn = getPreferencesButtonAtDetailsView();
        is(
          !detailsPrefBtn.hidden,
          expectVisible,
          `The ${id} prefs button in the addon details has the expected visibility`
        );
      } else {
        is(
          isInlineOptionsVisible(),
          expectVisible,
          `The ${id} inline prefs in the addon details has the expected visibility`
        );
      }
    };

    const setAddonPrivateBrowsingAccess = async (id, allowPrivateBrowsing) => {
      const cardUpdatedPromise = BrowserTestUtils.waitForEvent(
        getHtmlElem("addon-card"),
        "update"
      );
      is(
        getPrivateBrowsingValue(),
        allowPrivateBrowsing ? "0" : "1",
        `Private browsing should be initially ${
          allowPrivateBrowsing ? "off" : "on"
        }`
      );

      // Get the DOM element we want to click on (to allow or disallow the
      // addon on private browsing windows).
      await setPrivateBrowsingValue(allowPrivateBrowsing ? "1" : "0", id);

      info(`Waiting for details view of ${id} to be reloaded`);
      await cardUpdatedPromise;

      is(
        getPrivateBrowsingValue(),
        allowPrivateBrowsing ? "1" : "0",
        `Private browsing should be initially ${
          allowPrivateBrowsing ? "on" : "off"
        }`
      );

      is(
        await hasPrivateAllowed(id),
        allowPrivateBrowsing,
        `Private browsing permission ${
          allowPrivateBrowsing ? "added" : "removed"
        }`
      );
      let badge = getPrivateBrowsingBadge(getHtmlElem("addon-card"));
      is(
        !badge.hidden,
        allowPrivateBrowsing,
        `Expected private browsing badge at ${id}`
      );
    };

    const extensions = [];
    for (const definition of addons.values()) {
      const extension = ExtensionTestUtils.loadExtension(definition);
      extensions.push(extension);
      await extension.startup();
    }

    const items = get_test_items();

    for (const id of addons.keys()) {
      // Check the preferences button in the addon list page.
      is(
        getPreferencesButtonAtListView(items[id]).hidden,
        openInPrivateWin,
        `The ${id} prefs button in the addon list has the expected visibility`
      );
    }

    for (const [id, definition] of addons.entries()) {
      // Check the preferences button or inline frame in the addon
      // details page.
      info(`Opening addon details for ${id}`);
      const hasInlinePrefs = !definition.manifest.options_ui.open_in_tab;
      const onceViewChanged = BrowserTestUtils.waitForEvent(
        gManagerWindow,
        "ViewChanged"
      );
      gManagerWindow.loadView(`addons://detail/${encodeURIComponent(id)}`);
      await onceViewChanged;

      checkPrefsVisibility(id, hasInlinePrefs, !openInPrivateWin);

      // While testing in a private window, also check that the preferences
      // are going to be visible when we toggle the PB access for the addon.
      if (openInPrivateWin && definition.manifest.incognito !== "not_allowed") {
        await setAddonPrivateBrowsingAccess(id, true);
        checkPrefsVisibility(id, hasInlinePrefs, true);

        await setAddonPrivateBrowsingAccess(id, false);
        checkPrefsVisibility(id, hasInlinePrefs, false);
      }
    }

    for (const extension of extensions) {
      await extension.unload();
    }

    await close_manager(gManagerWindow);
    await BrowserTestUtils.closeWindow(win);
  }

  // run tests in private and non-private windows.
  await runTest(true);
  await runTest(false);
});

add_task(async function test_addon_postinstall_incognito_hidden_checkbox() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.allowPrivateBrowsingByDefault", false],
      ["extensions.langpacks.signatures.required", false],
    ],
  });

  const TEST_ADDONS = [
    {
      manifest: {
        name: "Extension incognito default opt-in",
        applications: {
          gecko: { id: "ext-incognito-default-opt-in@mozilla.com" },
        },
      },
    },
    {
      manifest: {
        name: "Extension incognito not_allowed",
        applications: {
          gecko: { id: "ext-incognito-not-allowed@mozilla.com" },
        },
        incognito: "not_allowed",
      },
    },
    {
      manifest: {
        name: "Static Theme",
        applications: { gecko: { id: "static-theme@mozilla.com" } },
        theme: {
          colors: {
            frame: "#FFFFFF",
            tab_background_text: "#000",
          },
        },
      },
    },
    {
      manifest: {
        name: "Dictionary",
        applications: { gecko: { id: "dictionary@mozilla.com" } },
        dictionaries: {
          und: "dictionaries/und.dic",
        },
      },
      files: {
        "dictionaries/und.dic": "",
        "dictionaries/und.aff": "",
      },
    },
    {
      manifest: {
        name: "Langpack",
        applications: { gecko: { id: "langpack@mozilla.com" } },
        langpack_id: "und",
        languages: {
          und: {
            chrome_resources: {
              global: "chrome/und/locale/und/global",
            },
            version: "20190326174300",
          },
        },
      },
    },
  ];

  for (let definition of TEST_ADDONS) {
    let { id } = definition.manifest.applications.gecko;
    info(
      `Testing incognito checkbox visibility on ${id} post install notification`
    );

    const xpi = AddonTestUtils.createTempWebExtensionFile(definition);
    let install = await AddonManager.getInstallForFile(xpi);

    await Promise.all([
      waitAppMenuNotificationShown("addon-installed", id, true),
      install.install().then(() => {
        Services.obs.notifyObservers(
          {
            addon: install.addon,
            target: gBrowser.selectedBrowser,
          },
          "webextension-install-notify"
        );
      }),
    ]);

    const { addon } = install;
    const { permissions } = addon;
    const canChangePBAccess = Boolean(
      permissions & AddonManager.PERM_CAN_CHANGE_PRIVATEBROWSING_ACCESS
    );

    if (id === "ext-incognito-default-opt-in@mozilla.com") {
      ok(
        canChangePBAccess,
        `${id} should have the PERM_CAN_CHANGE_PRIVATEBROWSING_ACCESS permission`
      );
    } else {
      ok(
        !canChangePBAccess,
        `${id} should not have the PERM_CAN_CHANGE_PRIVATEBROWSING_ACCESS permission`
      );
    }

    // This tests the visibility of various private detail rows.
    gManagerWindow = await open_manager(
      "addons://detail/" + encodeURIComponent(id)
    );
    info(`addon ${id} detail opened`);
    if (addon.type === "extension") {
      checkIsModifiable(canChangePBAccess);
      let required = addon.incognito === "spanning";
      checkIsRequired(!canChangePBAccess && required);
      checkIsDisallowed(!canChangePBAccess && !required);
    } else {
      checkIsModifiable(false);
      checkIsRequired(false);
      checkIsDisallowed(false);
    }
    await close_manager(gManagerWindow);

    await addon.uninstall();
  }

  // It is not possible to create a privileged add-on and install it, so just
  // simulate an installed privileged add-on and check the UI.
  await test_incognito_of_privileged_addons();
});

// Checks that the private browsing flag of privileged add-ons cannot be modified.
async function test_incognito_of_privileged_addons() {
  // In mochitests it is not possible to create and install a privileged add-on
  // or a system add-on, so create a mock provider that simulates privileged
  // add-ons (which lack the PERM_CAN_CHANGE_PRIVATEBROWSING_ACCESS permission).
  let provider = new MockProvider();
  provider.createAddons([
    {
      name: "default incognito",
      id: "default-incognito@mock",
      incognito: "spanning", // This is the default.
      // Anything without the PERM_CAN_CHANGE_PRIVATEBROWSING_ACCESS permission.
      permissions: 0,
    },
    {
      name: "not_allowed incognito",
      id: "not-allowed-incognito@mock",
      incognito: "not_allowed",
      // Anything without the PERM_CAN_CHANGE_PRIVATEBROWSING_ACCESS permission.
      permissions: 0,
    },
  ]);

  gManagerWindow = await open_manager(
    "addons://detail/default-incognito%40mock"
  );
  checkIsModifiable(false);
  checkIsRequired(true);
  checkIsDisallowed(false);
  await close_manager(gManagerWindow);

  gManagerWindow = await open_manager(
    "addons://detail/not-allowed-incognito%40mock"
  );
  checkIsModifiable(false);
  checkIsRequired(false);
  checkIsDisallowed(true);
  await close_manager(gManagerWindow);

  provider.unregister();
}
