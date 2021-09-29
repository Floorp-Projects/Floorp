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
  let colorwayList = doc.querySelector("section[section='2']");
  await colorwayList.cardsReady;
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
    "Header string is correct."
  );

  // Check that an enabled theme moves to the enabled section & out of the colorways section.
  let card = colorwayList.querySelector(
    "addon-card[addon-id='test-colorway@mozilla.org']"
  );

  ok(
    colorwayList.contains(card),
    "Colorways section contains monochromatic theme"
  );

  let addon = await AddonManager.getAddonByID("test-colorway@mozilla.org");
  await addon.enable();
  let enabledSection = doc.querySelector("section[section='0']");

  // After enabling the theme the card will not move to the appropriate section until a mousemove.
  let mutationPromise = BrowserTestUtils.waitForMutationCondition(
    enabledSection,
    { childList: true },
    () => enabledSection.contains(card)
  );

  await EventUtils.synthesizeMouseAtCenter(heading, { type: "mousemove" }, win);
  await mutationPromise;

  ok(
    enabledSection.contains(card),
    "Enabled section contains enabled colorway theme"
  );

  ok(
    !colorwayList.contains(card),
    "Colorway section no longer contains enabled theme"
  );

  // // Check that disabling a theme moves it back to the colorways section.
  await addon.disable();

  // After disabling a theme the card will not move to the appropriate section until a mousemove.
  mutationPromise = BrowserTestUtils.waitForMutationCondition(
    colorwayList,
    { childList: true },
    () => colorwayList.contains(card)
  );

  await EventUtils.synthesizeMouseAtCenter(
    subheading,
    { type: "mousemove" },
    win
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
