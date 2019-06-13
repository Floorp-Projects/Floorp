/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const {AddonTestUtils} = ChromeUtils.import("resource://testing-common/AddonTestUtils.jsm");
const {ExtensionPermissions} = ChromeUtils.import("resource://gre/modules/ExtensionPermissions.jsm");

var gManagerWindow;

AddonTestUtils.initMochitest(this);

function get_test_items() {
  var items = {};

  if (gManagerWindow.useHtmlViews) {
    for (let item of gManagerWindow.getHtmlBrowser().contentDocument.querySelectorAll("addon-card")) {
      items[item.getAttribute("addon-id")] = item;
    }
  } else {
    for (let item of gManagerWindow.document.getElementById("addon-list").childNodes) {
      items[item.mAddon.id] = item;
    }
  }

  return items;
}

function get(aId) {
  return gManagerWindow.document.getElementById(aId);
}

function getHtmlElem(selector) {
  return gManagerWindow.getHtmlBrowser().contentDocument.querySelector(selector);
}

function getPrivateBrowsingBadge(card) {
  if (gManagerWindow.useHtmlViews) {
    return card.querySelector(".addon-badge-private-browsing-allowed");
  }
  return card.ownerDocument.getAnonymousElementByAttribute(card, "anonid", "privateBrowsing");
}

function getPreferencesButtonAtListView(card) {
  if (gManagerWindow.useHtmlViews) {
    return card.querySelector("panel-item[action='preferences']");
  }
  return card._preferencesBtn;
}

function getPreferencesButtonAtDetailsView() {
  if (gManagerWindow.useHtmlViews) {
    return getHtmlElem("panel-item[action='preferences']");
  }
  return gManagerWindow.document.getElementById("detail-prefs-btn");
}

function isInlineOptionsVisible() {
  if (gManagerWindow.useHtmlViews) {
    // The following button is used to open the inline options browser.
    return !getHtmlElem("named-deck-button[name='preferences']").hidden;
  }
  return !!gManagerWindow.document.getElementById("addon-options");
}

function getPrivateBrowsingValue() {
  if (gManagerWindow.useHtmlViews) {
    return getHtmlElem("input[type='radio'][name='private-browsing']:checked").value;
  }
  return gManagerWindow.document.getElementById("detail-privateBrowsing").value;
}

async function setPrivateBrowsingValue(value) {
  if (gManagerWindow.useHtmlViews) {
    let radio = getHtmlElem(`input[type="radio"][name="private-browsing"][value="${value}"]`);
    EventUtils.synthesizeMouseAtCenter(radio, { clickCount: 1 }, radio.ownerGlobal);
    return TestUtils.waitForCondition(() => radio.checked,
                                      `Waiting for privateBrowsing=${value}`);
  }
  let privateBrowsing = gManagerWindow.document.getElementById("detail-privateBrowsing");
  let radio = privateBrowsing.querySelector(`radio[value="${value}"]`);
  EventUtils.synthesizeMouseAtCenter(radio, { clickCount: 1 }, gManagerWindow);
  return TestUtils.waitForCondition(() => privateBrowsing.value == value,
                                    `Waiting for privateBrowsing=${value}`);
}

// Check whether the private browsing inputs are visible in the details view.
function checkIsModifiable(expected) {
  if (gManagerWindow.useHtmlViews) {
    if (expected) {
      is_element_visible(getHtmlElem(".addon-detail-row-private-browsing"), "Private browsing should be visible");
    } else {
      is_element_hidden(getHtmlElem(".addon-detail-row-private-browsing"), "Private browsing should be hidden");
    }
    checkHelpRow(".addon-detail-row-private-browsing", expected);
    return;
  }
  if (expected) {
    is_element_visible(get("detail-privateBrowsing-row"), "Private browsing should be visible");
    is_element_visible(get("detail-privateBrowsing-row-footer"), "Private browsing footer should be visible");
  } else {
    is_element_hidden(get("detail-privateBrowsing-row"), "Private browsing should be hidden");
    is_element_hidden(get("detail-privateBrowsing-row-footer"), "Private browsing footer should be hidden");
  }
}

// Check whether the details view shows that private browsing is forcibly disallowed.
function checkIsDisallowed(expected) {
  if (gManagerWindow.useHtmlViews) {
    if (expected) {
      is_element_visible(getHtmlElem(".addon-detail-row-private-browsing-disallowed"), "Private browsing should be disallowed");
    } else {
      is_element_hidden(getHtmlElem(".addon-detail-row-private-browsing-disallowed"), "Private browsing should not be disallowed");
    }
    checkHelpRow(".addon-detail-row-private-browsing-disallowed", expected);
    return;
  }
  if (expected) {
    is_element_visible(get("detail-privateBrowsing-disallowed"), "Private browsing should be disallowed");
    is_element_visible(get("detail-privateBrowsing-disallowed-footer"), "Private browsing footer should be disallowed");
  } else {
    is_element_hidden(get("detail-privateBrowsing-disallowed"), "Private browsing should not be disallowed");
    is_element_hidden(get("detail-privateBrowsing-disallowed-footer"), "Private browsing footer should not be disallowed");
  }
}

// Check whether the details view shows that private browsing is forcibly allowed.
function checkIsRequired(expected) {
  if (gManagerWindow.useHtmlViews) {
    if (expected) {
      is_element_visible(getHtmlElem(".addon-detail-row-private-browsing-required"), "Private browsing should be required");
    } else {
      is_element_hidden(getHtmlElem(".addon-detail-row-private-browsing-required"), "Private browsing should not be required");
    }
    checkHelpRow(".addon-detail-row-private-browsing-required", expected);
    return;
  }
  if (expected) {
    is_element_visible(get("detail-privateBrowsing-required"), "Private required should be visible");
    is_element_visible(get("detail-privateBrowsing-required-footer"), "Private required footer should be visible");
  } else {
    is_element_hidden(get("detail-privateBrowsing-required"), "Private required should be hidden");
    is_element_hidden(get("detail-privateBrowsing-required-footer"), "Private required footer should be hidden");
  }
}

function checkHelpRow(selector, expected) {
  let helpRow = getHtmlElem(`${selector} + .addon-detail-help-row`);
  if (expected) {
    is_element_visible(helpRow, `Help row should be shown: ${selector}`);
    is_element_visible(helpRow.querySelector("a, [action='pb-learn-more']"), "Expected learn more link");
  } else {
    is_element_hidden(helpRow, `Help row should be hidden: ${selector}`);
  }
}

async function hasPrivateAllowed(id) {
  let perms = await ExtensionPermissions.get(id);
  return perms.permissions.length == 1 &&
         perms.permissions[0] == "internal:privateBrowsingAllowed";
}

add_task(function clearInitialTelemetry() {
  // Clear out any telemetry data that existed before this file is run.
  Services.telemetry.clearEvents();
});

async function test_badge_and_toggle_incognito() {
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
    gManagerWindow = await open_manager("addons://detail/" + encodeURIComponent(id));
    ok(true, `==== ${id} detail opened`);
    if (definition.manifest.incognito == "not_allowed") {
      checkIsModifiable(false);
      ok(!await hasPrivateAllowed(id), "Private browsing permission not set");
      checkIsDisallowed(true);
    } else {
      // This assumes PERM_CAN_CHANGE_PRIVATEBROWSING_ACCESS, we test other options in a later test in this file.
      checkIsModifiable(true);
      if (definition.incognitoOverride == "spanning") {
        is(getPrivateBrowsingValue(), "1", "Private browsing should be on");
        ok(await hasPrivateAllowed(id), "Private browsing permission set");
        await setPrivateBrowsingValue("0");
        is(getPrivateBrowsingValue(), "0", "Private browsing should be off");
        ok(!await hasPrivateAllowed(id), "Private browsing permission removed");
      } else {
        is(getPrivateBrowsingValue(), "0", "Private browsing should be off");
        ok(!await hasPrivateAllowed(id), "Private browsing permission not set");
        await setPrivateBrowsingValue("1");
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

  assertTelemetryMatches([
    ["action", "aboutAddons", "on", {...expectedExtras, addonId: "@test-default"}],
    ["action", "aboutAddons", "off", {...expectedExtras, addonId: "@test-override"}],
    ["action", "aboutAddons", "off", {...expectedExtras, addonId: "@test-override-permanent"}],
  ], {filterMethods: ["action"]});

  Services.prefs.clearUserPref("extensions.allowPrivateBrowsingByDefault");
}

async function test_addon_preferences_button() {
  await SpecialPowers.pushPrefEnv({set: [["extensions.allowPrivateBrowsingByDefault", false]]});

  let addons = new Map([
    ["test-inline-options@mozilla.com", {
      useAddonManager: "temporary",
      manifest: {
        name: "Extension with inline options",
        applications: {gecko: {id: "test-inline-options@mozilla.com"}},
        options_ui: {page: "options.html", open_in_tab: false},
      },
    }],
    ["test-newtab-options@mozilla.com", {
      useAddonManager: "temporary",
      manifest: {
        name: "Extension with options page in a new tab",
        applications: {gecko: {id: "test-newtab-options@mozilla.com"}},
        options_ui: {page: "options.html", open_in_tab: true},
      },
    }],
    ["test-not-allowed@mozilla.com", {
      useAddonManager: "temporary",
      manifest: {
        name: "Extension not allowed in PB windows",
        incognito: "not_allowed",
        applications: {gecko: {id: "test-not-allowed@mozilla.com"}},
        options_ui: {page: "options.html", open_in_tab: true},
      },
    }],
  ]);

  async function runTest(openInPrivateWin) {
    const win = await BrowserTestUtils.openNewBrowserWindow({
      private: openInPrivateWin,
    });

    gManagerWindow = await open_manager(
      "addons://list/extension", undefined, undefined, undefined, win);

    const checkPrefsVisibility = (id, hasInlinePrefs, expectVisible) => {
      if (!hasInlinePrefs) {
        const detailsPrefBtn = getPreferencesButtonAtDetailsView();
        is(!detailsPrefBtn.hidden, expectVisible,
           `The ${id} prefs button in the addon details has the expected visibility`);
      } else {
        is(isInlineOptionsVisible(), expectVisible,
           `The ${id} inline prefs in the addon details has the expected visibility`);
      }
    };

    const setAddonPrivateBrowsingAccess = async (id, allowPrivateBrowsing) => {
      let cardUpdatedPromise;
      if (gManagerWindow.useHtmlViews) {
        cardUpdatedPromise = BrowserTestUtils.waitForEvent(getHtmlElem("addon-card"), "update");
      } else {
        cardUpdatedPromise = BrowserTestUtils.waitForEvent(gManagerWindow, "ViewChanged");
      }
      is(getPrivateBrowsingValue(),
         allowPrivateBrowsing ? "0" : "1",
         `Private browsing should be initially ${allowPrivateBrowsing ? "off" : "on"}`);

      // Get the DOM element we want to click on (to allow or disallow the
      // addon on private browsing windows).
      await setPrivateBrowsingValue(allowPrivateBrowsing ? "1" : "0");

      info(`Waiting for details view of ${id} to be reloaded`);
      await cardUpdatedPromise;

      is(getPrivateBrowsingValue(),
         allowPrivateBrowsing ? "1" : "0",
         `Private browsing should be initially ${allowPrivateBrowsing ? "on" : "off"}`);

      is(await hasPrivateAllowed(id), allowPrivateBrowsing,
         `Private browsing permission ${allowPrivateBrowsing ? "added" : "removed"}`);
      if (gManagerWindow.useHtmlViews) {
        let badge = getPrivateBrowsingBadge(getHtmlElem("addon-card"));
        is(!badge.hidden, allowPrivateBrowsing, `Expected private browsing badge at ${id}`);
      }
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
      is(getPreferencesButtonAtListView(items[id]).hidden, openInPrivateWin,
        `The ${id} prefs button in the addon list has the expected visibility`);
    }

    for (const [id, definition] of addons.entries()) {
      // Check the preferences button or inline frame in the addon
      // details page.
      info(`Opening addon details for ${id}`);
      const hasInlinePrefs = !definition.manifest.options_ui.open_in_tab;
      const onceViewChanged = BrowserTestUtils.waitForEvent(gManagerWindow, "ViewChanged");
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
}

async function test_addon_postinstall_incognito_hidden_checkbox(withHtmlViews) {
  await SpecialPowers.pushPrefEnv({set: [
    ["extensions.allowPrivateBrowsingByDefault", false],
    ["extensions.langpacks.signatures.required", false],
    ["extensions.htmlaboutaddons.enabled", withHtmlViews],
  ]});

  const TEST_ADDONS = [
    {
      manifest: {
        name: "Extension incognito default opt-in",
        applications: {gecko: {id: "ext-incognito-default-opt-in@mozilla.com"}},
      },
    },
    {
      manifest: {
        name: "Extension incognito not_allowed",
        applications: {gecko: {id: "ext-incognito-not-allowed@mozilla.com"}},
        incognito: "not_allowed",
      },
    },
    {
      manifest: {
        name: "Static Theme",
        applications: {gecko: {id: "static-theme@mozilla.com"}},
        theme: {
          colors: {
            accentcolor: "#FFFFFF",
            textcolor: "#000",
          },
        },
      },
    },
    {
      manifest: {
        name: "Dictionary",
        applications: {gecko: {id: "dictionary@mozilla.com"}},
        dictionaries: {
          "und": "dictionaries/und.dic",
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
        applications: {gecko: {id: "langpack@mozilla.com"}},
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
    let {id} = definition.manifest.applications.gecko;
    info(`Testing incognito checkbox visibility on ${id} post install notification`);

    const xpi = AddonTestUtils.createTempWebExtensionFile(definition);
    let install = await AddonManager.getInstallForFile(xpi);

    await Promise.all([
      waitAppMenuNotificationShown("addon-installed", id, true),
      install.install().then(() => {
        Services.obs.notifyObservers({
          addon: install.addon, target: gBrowser.selectedBrowser,
        }, "webextension-install-notify");
      }),
    ]);

    const {addon} = install;
    const {permissions} = addon;
    const canChangePBAccess = Boolean(permissions & AddonManager.PERM_CAN_CHANGE_PRIVATEBROWSING_ACCESS);

    if (id === "ext-incognito-default-opt-in@mozilla.com") {
      ok(canChangePBAccess, `${id} should have the PERM_CAN_CHANGE_PRIVATEBROWSING_ACCESS permission`);
    } else {
      ok(!canChangePBAccess, `${id} should not have the PERM_CAN_CHANGE_PRIVATEBROWSING_ACCESS permission`);
    }

    // This tests the visibility of various private detail rows.
    gManagerWindow = await open_manager("addons://detail/" + encodeURIComponent(id));
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

  // No popPrefEnv because of bug 1557397.
}

// Checks that the private browsing flag of privileged add-ons cannot be modified.
async function test_incognito_of_privileged_addons() {
  // In mochitests it is not possible to create and install a privileged add-on
  // or a system add-on, so create a mock provider that simulates privileged
  // add-ons (which lack the PERM_CAN_CHANGE_PRIVATEBROWSING_ACCESS permission).
  let provider = new MockProvider();
  provider.createAddons([{
    name: "default incognito",
    id: "default-incognito@mock",
    incognito: "spanning", // This is the default.
    // Anything without the PERM_CAN_CHANGE_PRIVATEBROWSING_ACCESS permission.
    permissions: 0,
  }, {
    name: "not_allowed incognito",
    id: "not-allowed-incognito@mock",
    incognito: "not_allowed",
    // Anything without the PERM_CAN_CHANGE_PRIVATEBROWSING_ACCESS permission.
    permissions: 0,
  }]);

  gManagerWindow = await open_manager("addons://detail/default-incognito%40mock");
  checkIsModifiable(false);
  checkIsRequired(true);
  checkIsDisallowed(false);
  await close_manager(gManagerWindow);

  gManagerWindow = await open_manager("addons://detail/not-allowed-incognito%40mock");
  checkIsModifiable(false);
  checkIsRequired(false);
  checkIsDisallowed(true);
  await close_manager(gManagerWindow);

  provider.unregister();
}

add_task(async function test_badge_and_toggle_incognito_on_XUL_aboutaddons() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.htmlaboutaddons.enabled", false]],
  });
  await test_badge_and_toggle_incognito();
  // No popPrefEnv because of bug 1557397.
});

add_task(async function test_badge_and_toggle_incognito_on_HTML_aboutaddons() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.htmlaboutaddons.enabled", true]],
  });
  await test_badge_and_toggle_incognito();
  // No popPrefEnv because of bug 1557397.
});

add_task(async function test_addon_preferences_button_on_XUL_aboutaddons() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.htmlaboutaddons.enabled", false],
      ["extensions.htmlaboutaddons.inline-options.enabled", false],
    ],
  });
  await test_addon_preferences_button();
  // No popPrefEnv because of bug 1557397.
});

add_task(async function test_addon_preferences_button_on_HTML_aboutaddons() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.htmlaboutaddons.enabled", true],
      ["extensions.htmlaboutaddons.inline-options.enabled", true],
    ],
  });
  await test_addon_preferences_button();
  // No popPrefEnv because of bug 1557397.
});

add_task(async function test_addon_postinstall_incognito_hidden_checkbox_on_XUL_aboutaddons() {
  await test_addon_postinstall_incognito_hidden_checkbox(false);
});

add_task(async function test_addon_postinstall_incognito_hidden_checkbox_on_HTML_aboutaddons() {
  await test_addon_postinstall_incognito_hidden_checkbox(true);
});
