
add_task(async function() {
  const INFO_URL = Services.urlFormatter.formatURLPref("app.support.baseURL") + "webextensions";

  // The mochitest framework installs a bunch of legacy extensions.
  // Fortunately, the extensions.legacy.exceptions preference exists to
  // avoid treating some extensions as legacy for the purposes of the UI.
  const IGNORE = [
    "special-powers@mozilla.org",
    "mochikit@mozilla.org",
    "mozscreenshots@mozilla.org",
  ];

  let exceptions = Services.prefs.getCharPref("extensions.legacy.exceptions");
  exceptions = [ exceptions, ...IGNORE ].join(",");

  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.legacy.enabled", false],
      ["extensions.legacy.exceptions", exceptions],

      ["xpinstall.signatures.required", false],
    ],
  });

  let goodAddons = [
    {
      id: "webextension@tests.mozilla.org",
      name: "WebExtension",
      type: "extension",
      isWebExtension: true,
    },
    {
      id: "mozilla@tests.mozilla.org",
      name: "Mozilla signed extension",
      type: "extension",
      isWebExtension: false,
      signedState: AddonManager.SIGNEDSTATE_PRIVILEGED,
    },
  ];

  let disabledAddon = [
    {
      id: "legacy@tests.mozilla.org",
      name: "Legacy extension",
      type: "extension",
      isWebExtension: false,
      appDisabled: true,
    },
  ];

  let unsignedAddons = [
    {
      id: "unsigned_webext@tests.mozilla.org",
      name: "Unsigned WebExtension",
      type: "extension",
      isWebExtension: true,
      appDisabled: true,
      signedState: AddonManager.SIGNEDSTATE_MISSING,
    },
    {
      id: "unsigned_legacy@tests.mozilla.org",
      name: "Unsigned legacy extension",
      type: "extension",
      isWebExtension: false,
      appDisabled: true,
      signedState: AddonManager.SIGNEDSTATE_MISSING,
    },
  ];

  let provider = new MockProvider();
  provider.createAddons(goodAddons);

  let mgrWin = await open_manager(null);
  let catUtils = new CategoryUtilities(mgrWin);

  // Check that the test addons in the given list are exactly those
  // in the expected list.
  async function checkList(listId, expectIds) {
    let ids = new Set(expectIds);
    for (let item of mgrWin.document.getElementById(listId).children) {
      if (!item.mAddon.id.endsWith("@tests.mozilla.org")) {
        continue;
      }

      ok(ids.has(item.mAddon.id), `Found ${item.mAddon.id} in addons list`);
      ids.delete(item.mAddon.id);
    }

    for (let id of ids) {
      ok(false, `Did not find ${id} in addons list`);
    }
  }

  // Initially, we have two good extensions (a webextension and a
  // "Mozilla Extensions"-signed extension).
  await catUtils.openType("extension");
  checkList("addon-list",
            ["webextension@tests.mozilla.org", "mozilla@tests.mozilla.org"]);

  let banner = mgrWin.document.getElementById("legacy-extensions-notice");
  is_element_hidden(banner, "Warning about legacy extensions should be hidden");
  is(mgrWin.gLegacyView._categoryItem.disabled, true, "Legacy category is hidden");

  // Now add a legacy extension
  provider.createAddons(disabledAddon);

  // The legacy category does not watch for new installs since new
  // legacy extensions cannot be installed while legacy extensions
  // are disabled, so manually refresh it here.
  await mgrWin.gLegacyView.refreshVisibility();

  // Make sure we re-render the extensions list, after that we should
  // still just have the original two entries.
  await catUtils.openType("plugin");
  await catUtils.openType("extension");

  checkList("addon-list",
            ["webextension@tests.mozilla.org", "mozilla@tests.mozilla.org"]);

  // But now the legacy banner and category should be visible
  banner = mgrWin.document.getElementById("legacy-extensions-notice");
  is_element_visible(banner, "Warning about legacy extensions should be visible");

  let catItem = mgrWin.gLegacyView._categoryItem;
  is(catItem.disabled, false, "Legacy category is visible");
  is(catItem.getAttribute("name"), get_string("type.legacy.name"),
     "Category label with no unsigned extensions is correct");

  // Follow the link to the legacy extensions page
  let legacyLink = mgrWin.document.getElementById("legacy-extensions-learnmore-link");
  is_element_visible(legacyLink, "Link to legacy extension is visible");

  let loadPromise = new Promise(resolve => wait_for_view_load(mgrWin, resolve, true));
  legacyLink.click();
  await loadPromise;

  is(mgrWin.gViewController.currentViewId, "addons://legacy/",
     "Legacy extensions link leads to the correct view");

  let link = mgrWin.document.getElementById("legacy-learnmore");
  is(link.href, INFO_URL, "Learn more link points to the right place");

  // The only extension in the list should be the one we just added.
  checkList("legacy-list", ["legacy@tests.mozilla.org"]);

  // Now add some unsigned addons and flip the signing preference
  provider.createAddons(unsignedAddons);
  SpecialPowers.pushPrefEnv({
    set: [
      ["xpinstall.signatures.required", true],
    ],
  });

  // The entry on the left side should now read "Unsupported"
  await mgrWin.gLegacyView.refreshVisibility();
  is(catItem.disabled, false, "Legacy category is visible");
  is(catItem.getAttribute("name"), get_string("type.unsupported.name"),
     "Category label with unsigned extensions is correct");

  // The main extensions list should still have the original two
  // good extensions and the legacy banner.
  await catUtils.openType("extension");
  checkList("addon-list",
            ["webextension@tests.mozilla.org", "mozilla@tests.mozilla.org"]);

  banner = mgrWin.document.getElementById("legacy-extensions-notice");
  is_element_visible(banner, "Warning about legacy extensions should be visible");

  // And the legacy pane should show both legacy and unsigned extensions
  await catUtils.openType("legacy");
  checkList("legacy-list", [
    "legacy@tests.mozilla.org",
    "unsigned_webext@tests.mozilla.org",
    "unsigned_legacy@tests.mozilla.org",
  ]);

  // Disable unsigned extensions
  SpecialPowers.pushPrefEnv({
    set: [
      ["xpinstall.signatures.required", false],
    ],
  });

  await new Promise(executeSoon);

  // The name of the pane should go back to "Legacy Extensions"
  await mgrWin.gLegacyView.refreshVisibility();
  is(catItem.disabled, false, "Legacy category is visible");
  is(catItem.getAttribute("name"), get_string("type.legacy.name"),
     "Category label with no unsigned extensions is correct");

  // The unsigned extension should be present in the main extensions pane
  await catUtils.openType("extension");
  checkList("addon-list", [
    "webextension@tests.mozilla.org",
    "mozilla@tests.mozilla.org",
    "unsigned_webext@tests.mozilla.org",
  ]);

  // And it should not be present in the legacy pane
  await catUtils.openType("legacy");
  checkList("legacy-list", [
    "legacy@tests.mozilla.org",
    "unsigned_legacy@tests.mozilla.org",
  ]);

  await close_manager(mgrWin);

  // Now enable legacy extensions and open a new addons manager tab.
  // The remembered last view will be the list of legacy extensions but
  // now that legacy extensions are enabled, we should jump to the
  // regular Extensions list.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.legacy.enabled", true],
    ],
  });

  mgrWin = await open_manager(null);
  is(mgrWin.gViewController.currentViewId, "addons://list/extension",
     "addons manager switched to extensions list");
  await close_manager(mgrWin);
});
