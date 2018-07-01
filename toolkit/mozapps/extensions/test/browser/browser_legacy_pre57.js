
add_task(async function() {
  const INFO_URL = Services.urlFormatter.formatURLPref("app.support.baseURL") + "webextensions";

  const NAMES = {
    newTheme: "New LWT",
    legacy: "Legacy Extension",
    webextension: "WebExtension",
    dictionary: "Dictionary",
    langpack: "Language Pack",
  };
  let addons = [
    {
      id: "new-theme@tests.mozilla.org",
      name: NAMES.newTheme,
      type: "theme",
      isWebExtension: true,
    },
    {
      id: "legacy@tests.mozilla.org",
      name: NAMES.legacy,
      type: "extension",
      isWebExtension: false,
    },
    {
      id: "webextension@tests.mozilla.org",
      name: NAMES.webextension,
      type: "extension",
      isWebExtension: true,
    },
    {
      id: "dictionary@tests.mozilla.org",
      name: NAMES.dictionary,
      type: "dictionary",
    },
  ];

  let provider = new MockProvider();
  provider.createAddons(addons);

  let mgrWin = await open_manager(null);
  let catUtils = new CategoryUtilities(mgrWin);

  async function check(category, name, isLegacy) {
    await catUtils.openType(category);

    let document = mgrWin.document;
    // First find the  entry in the list.
    let item = Array.from(document.getElementById("addon-list").childNodes)
                    .find(i => i.getAttribute("name") == name);

    ok(item, `Found ${name} in list`);
    item.parentNode.ensureElementIsVisible(item);

    // Check the badge
    let badge = document.getAnonymousElementByAttribute(item, "anonid", "legacy");

    if (isLegacy) {
      is_element_visible(badge, `Legacy badge is visible for ${name}`);
      is(badge.href, INFO_URL, "Legacy badge link is correct");
    } else {
      is_element_hidden(badge, `Legacy badge is hidden for ${name}`);
    }

    // Click down to the details page.
    let detailsButton = document.getAnonymousElementByAttribute(item, "anonid", "details-btn");
    EventUtils.synthesizeMouseAtCenter(detailsButton, {}, mgrWin);
    await new Promise(resolve => wait_for_view_load(mgrWin, resolve));

    // And check the badge
    let elements = document.getElementsByClassName("legacy-warning");
    is(elements.length, 1, "Found the legacy-warning element");
    badge = elements[0];

    if (isLegacy) {
      is_element_visible(badge, `Legacy badge is visible for ${name}`);
      is(badge.href, INFO_URL, "Legacy badge link is correct");
    } else {
      is_element_hidden(badge, `Legacy badge is hidden for ${name}`);
    }
  }

  await check("theme", NAMES.newTheme, false);
  await check("extension", NAMES.legacy, true);
  await check("extension", NAMES.webextension, false);
  await check("dictionary", NAMES.dictionary, false);

  await close_manager(mgrWin);
});
