
add_task(async function() {
  // The mochitest framework installs a bunch of legacy extensions.
  // Fortunately, the extensions.legacy.exceptions preference exists to
  // avoid treating some extensions as legacy for the purposes of the UI.
  const IGNORE = [
    "special-powers@mozilla.org",
    "mochikit@mozilla.org",
  ];

  let exceptions = Services.prefs.getCharPref("extensions.legacy.exceptions");
  exceptions = [ exceptions, ...IGNORE ].join(",");

  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.legacy.enabled", false],
      ["extensions.legacy.exceptions", exceptions],
    ],
  });

  const ID = "theme@tests.mozilla.org";

  let provider = new MockProvider();
  provider.createAddons([{
    id: ID,
    name: "Complete Theme",
    type: "theme",
    appDisabled: true,
  }]);

  // Open about:addons and go to the themes list
  let mgrWin = await open_manager(null);
  let catUtils = new CategoryUtilities(mgrWin);
  await catUtils.openType("theme");

  // Our complete theme should not be displayed
  let list = mgrWin.document.getElementById("addon-list");
  let item = list.children.find(item => item.mAddon.id == ID);
  is(item, undefined, `Theme ${ID} should not be in the list of active themes`);

  // The warning banner and the legacy category should both be visible
  let banner = mgrWin.document.getElementById("legacy-extensions-notice");
  is_element_visible(banner, "Warning about legacy themes should be visible");
  is(mgrWin.gLegacyView._categoryItem.disabled, false, "Legacy category should be visible ");

  // Follow the link to the legacy extensions page
  let legacyLink = mgrWin.document.getElementById("legacy-extensions-learnmore-link");
  is_element_visible(legacyLink, "Link to legacy extensions is visible");

  let loadPromise = new Promise(resolve => wait_for_view_load(mgrWin, resolve, true));
  legacyLink.click();
  await loadPromise;

  is(mgrWin.gViewController.currentViewId, "addons://legacy/",
     "Legacy extensions link leads to the correct view");

  list = mgrWin.document.getElementById("legacy-list");
  is(list.children.length, 1, "Should have 1 item in the legacy list");
  item = list.children[0];
  is(item.mAddon.id, ID, "Complete theme should be in the list");

  // Click the find a replacement button
  let button = document.getAnonymousElementByAttribute(item, "anonid", "replacement-btn");
  is_element_visible(button, "Find a replacement butotn is visible");

  // In automation, app.support.baseURL points to a page on localhost.
  // The actual page is 404 in the test but that doesn't matter here,
  // just the fact that we load the right URL.
  let url = Services.prefs.getStringPref("app.support.baseURL") + "complete-themes";
  let tabPromise = BrowserTestUtils.waitForNewTab(gBrowser, url);
  button.click();
  let tab = await tabPromise;
  ok(true, "Find a replacement button opened SUMO page");
  BrowserTestUtils.removeTab(tab);

  await close_manager(mgrWin);
});
