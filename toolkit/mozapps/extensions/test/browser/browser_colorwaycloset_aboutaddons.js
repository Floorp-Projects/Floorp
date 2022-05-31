/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);

AddonTestUtils.initMochitest(this);

const kTestThemeId = "test-colorway@mozilla.org";

/**
 * Tests that the colorway closet section only appears if a colorway collection
 * is available and the colorway closet pref is enabled.
 */
add_task(async function testColorwayClosetPrefEnabled() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.theme.colorway-closet", true]],
  });
  const themeXpi = AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      name: "Monochromatic Theme",
      applications: { gecko: { id: kTestThemeId } },
      theme: {},
    },
  });
  const { addon } = await AddonTestUtils.promiseInstallFile(themeXpi);

  let win = await loadInitialView("theme");
  let doc = win.document;
  let colorwayClosetList = doc.querySelector("colorways-list");

  info("Verifying colorway closet list contents");
  ok(colorwayClosetList, "colorway closet list was found");
  ok(
    colorwayClosetList.querySelector("#colorways-section-heading"),
    "colorway closet heading was found"
  );
  ok(
    colorwayClosetList.querySelector("#colorways-section-heading"),
    "colorway closet subheading was found"
  );

  let cards = colorwayClosetList.querySelectorAll("colorways-card");
  ok(cards.length, "At least one colorway closet card was found");

  info("Verifying colorway closet card contents");
  let card = cards[0];
  ok(
    card.querySelector("#colorways-preview-text-container")?.textContent,
    "Preview text found"
  );
  ok(card.querySelector(".card-heading-image"), "Preview found");
  ok(
    card.querySelector("#colorways-expiry-date-container"),
    "colorway collection expiry date found"
  );
  ok(
    card.querySelector("#colorways-button"),
    "colorway collection button found"
  );

  await closeView(win);
  await addon.uninstall(true);
});

/**
 * Tests that the colorway closet section does not appear if a colorway collection
 * is available but the colorway closet pref is disabled.
 */
add_task(async function testColorwayClosetSectionPrefDisabled() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.theme.colorway-closet", false]],
  });
  const themeXpi = AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      name: "Monochromatic Theme",
      applications: { gecko: { id: kTestThemeId } },
      theme: {},
    },
  });
  const { addon } = await AddonTestUtils.promiseInstallFile(themeXpi);

  let win = await loadInitialView("theme");
  let doc = win.document;
  let colorwayClosetList = doc.querySelector("colorways-list");

  ok(!colorwayClosetList, "colorway closet list should not be found");

  await closeView(win);
  await addon.uninstall(true);
});
