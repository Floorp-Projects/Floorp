/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);
const { BuiltInThemes } = ChromeUtils.import(
  "resource:///modules/BuiltInThemes.jsm"
);

const kTestThemeId = "test-colorway@mozilla.org";

add_setup(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.theme.colorway-closet", false]],
  });

  AddonTestUtils.initMochitest(this);
});

add_task(async function testMonochromaticList() {
  // Install test theme before loading view.
  const themeXpi = AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      name: "Monochromatic Theme",
      applications: { gecko: { id: kTestThemeId } },
      theme: {},
    },
  });

  const themeAddon = await AddonManager.installTemporaryAddon(themeXpi);

  let win = await loadInitialView("theme");
  let doc = win.document;

  // Wait for the colorway themes list to render.
  let colorwayList = doc.querySelector(".monochromatic-addon-list");
  await themeAddon.disable();

  // Check that all the cards are visible.
  let cards = doc.querySelectorAll("addon-card");
  ok(!!cards.length, "There were some cards found");
  for (let card of cards) {
    is_element_visible(card, "The card is visible");
  }

  // Check for the colorways section heading & subheading.
  let heading = colorwayList.querySelector(".header-name");
  let subheading = colorwayList.querySelector(".list-section-subheading");

  is(
    heading.getAttribute("data-l10n-id"),
    "theme-monochromatic-heading",
    "Header string is correct."
  );

  is(
    subheading.getAttribute("data-l10n-id"),
    "theme-monochromatic-subheading",
    "Subheader string is correct."
  );

  // Check that the test theme is in the colorways section. It should be there
  // because it hasn't yet expired.
  let card = colorwayList.querySelector(
    "addon-card[addon-id='test-colorway@mozilla.org']"
  );

  ok(
    colorwayList.contains(card),
    "Colorways section contains monochromatic theme"
  );

  // Check that themes are sorted according to their position in the config.

  // Collect all builtin colorway themes that are not expired.
  const isColorwayTheme = id => id.endsWith("-colorway@mozilla.org");

  let builtInThemeIds = Array.from(BuiltInThemes.builtInThemeMap.keys()).filter(
    id => isColorwayTheme(id) && !BuiltInThemes.themeIsExpired(id)
  );

  // Collect all about:addons colorway card ids.
  let colorwayIds = Array.from(
    colorwayList.querySelectorAll("addon-card"),
    card => card.getAttribute("addon-id")
  );

  // Check that last item is the test theme we installed. If so, we remove it
  // so we can properly compare arrays.
  Assert.equal(colorwayIds.pop(), "test-colorway@mozilla.org");

  // Compare colorway themes in config to colorway themes rendered on page.
  Assert.deepEqual(
    colorwayIds,
    builtInThemeIds,
    "Both arrays should be the same"
  );

  // Check that the test theme is in the enabled section.
  let addon = await AddonManager.getAddonByID("test-colorway@mozilla.org");
  let enabledSection = getSection(doc, "theme-enabled-section");
  let mutationPromise = BrowserTestUtils.waitForMutationCondition(
    enabledSection,
    { childList: true },
    () =>
      enabledSection.children.length > 1 &&
      enabledSection.children[1].getAttribute("addon-id") == kTestThemeId
  );
  await addon.enable();
  await mutationPromise;
  let enabledCard = enabledSection.querySelector(
    `addon-card[addon-id='${kTestThemeId}']`
  );
  ok(
    enabledSection.contains(enabledCard),
    "Enabled section contains enabled colorway theme"
  );

  // Check that disabling a theme removes it from the enabled section.
  await addon.disable();

  mutationPromise = BrowserTestUtils.waitForMutationCondition(
    enabledSection,
    { childList: true },
    () => !enabledSection.contains(enabledCard)
  );

  await mutationPromise;

  ok(
    !enabledSection.contains(card),
    "Enabled section no longer contains disabled colorway theme"
  );

  ok(
    colorwayList.contains(card),
    "Colorway section contains recently disabled theme"
  );

  await closeView(win);
  await themeAddon.uninstall(true);
});

add_task(async function testExpiredThemes() {
  const themeXpi = AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      name: "Monochromatic Theme",
      applications: { gecko: { id: kTestThemeId } },
      theme: {},
    },
  });

  // Make the test theme appear expired.
  let yesterday = new Date();
  yesterday.setDate(yesterday.getDate() - 1);
  yesterday = yesterday.toISOString().split("T")[0];
  // Add the test theme to our list of built-in themes so that aboutaddons.js
  // will think this theme is expired.
  BuiltInThemes.builtInThemeMap.set(kTestThemeId, {
    version: "1.0",
    expiry: yesterday,
    // We use the manifest from Light theme since we know it will be in-tree
    // indefinitely.
    path: "resource://builtin-themes/light/",
  });
  registerCleanupFunction(() => {
    BuiltInThemes.builtInThemeMap.delete(kTestThemeId);
  });

  // Make the test theme appear retained.
  const retainedThemePrefName = "browser.theme.retainedExpiredThemes";
  Services.prefs.setStringPref(
    retainedThemePrefName,
    JSON.stringify([kTestThemeId])
  );
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref(retainedThemePrefName);
  });

  const expiredAddon = await AddonManager.installTemporaryAddon(themeXpi);
  let addon = await AddonManager.getAddonByID(kTestThemeId);
  await addon.disable();

  let win = await loadInitialView("theme");
  let doc = win.document;

  let colorwayList = doc.querySelector(".monochromatic-addon-list");

  // We need branching logic here since the outcome depends on whether there
  // are active non-test Colorway themes when the test runs.
  if (colorwayList) {
    let card = colorwayList.querySelector(
      `addon-card[addon-id='${kTestThemeId}']`
    );
    ok(
      !colorwayList.contains(card),
      "Colorways section does not contain expired theme."
    );
  } else {
    ok(
      true,
      "The Colorways section is not in the DOM because all Colorway themes are expired."
    );
  }

  let disabledSection = getSection(doc, "theme-disabled-section");
  let card = disabledSection.querySelector(
    `addon-card[addon-id='${kTestThemeId}']`
  );
  ok(
    disabledSection.contains(card),
    "The regular, non-Colorways 'Disabled' section contains the expired theme."
  );

  await closeView(win);
  await expiredAddon.uninstall(true);
});
