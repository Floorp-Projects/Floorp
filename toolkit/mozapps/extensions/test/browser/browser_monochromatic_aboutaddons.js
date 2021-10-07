/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);

AddonTestUtils.initMochitest(this);

add_task(async function testMonochromaticList() {
  // Install test theme before loading view.
  const themeXpi = AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      name: "Monochromatic Theme",
      applications: { gecko: { id: "test-colorway@mozilla.org" } },
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

  // Check that the test theme is in the colorways section.
  let card = colorwayList.querySelector(
    "addon-card[addon-id='test-colorway@mozilla.org']"
  );

  ok(
    colorwayList.contains(card),
    "Colorways section contains monochromatic theme"
  );

  // Check that the test theme is in the enabled section.
  let addon = await AddonManager.getAddonByID("test-colorway@mozilla.org");
  let enabledSection = doc.querySelector("section[section='0']");
  let mutationPromise = BrowserTestUtils.waitForMutationCondition(
    enabledSection,
    { childList: true },
    () =>
      enabledSection.children.length > 1 &&
      enabledSection.children[1].getAttribute("addon-id") ==
        "test-colorway@mozilla.org"
  );
  await addon.enable();
  await mutationPromise;
  let enabledCard = enabledSection.querySelector(
    "addon-card[addon-id='test-colorway@mozilla.org']"
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
