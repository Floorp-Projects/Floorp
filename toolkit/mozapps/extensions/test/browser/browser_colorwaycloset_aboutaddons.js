/* Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from ../../../../../browser/components/colorways/tests/browser/head.js */
loadTestSubscript(
  "../../../../../browser/components/colorways/tests/browser/head.js"
);

AddonTestUtils.initMochitest(this);

add_setup(async function() {
  registerMockCollectionL10nIds();
});

/**
 * Verifies that BuiltInThemes.findActiveColorwayCollection has proper l10n data if an active collection is found.
 * This function is for catching possible missing l10n data for an active collection - which was originally loaded in Nightly only
 * (also see Bug 1774422). A collection is unavailable on all channels once past its expiry date. Testing functionality for both
 * active and expired collections should be done using stubs.
 */
add_task(async function verifyActiveCollectionL10nStrings() {
  info(
    "Verify data returned by BuiltInThemes.findActiveColorwayCollection is in the expected format"
  );
  let collection = BuiltInThemes.findActiveColorwayCollection();

  if (collection) {
    info("Found an active collection");
    ok(collection.l10nId, "Collection l10n data");
    ok(collection.l10nId?.title, "Collection has valid l10n title");
    ok(collection.l10nId?.description, "Collection has valid l10n description");
  } else {
    info("Couldn't find an active collection");
  }
});

/**
 * Tests that the colorway closet section only appears if a colorway collection
 * is available and the colorway closet pref is enabled.
 */
add_task(async function testColorwayClosetPrefEnabled() {
  const clearBuiltInThemesStubs = initBuiltInThemesStubs();
  await SpecialPowers.pushPrefEnv({
    set: [["browser.theme.colorway-closet", true]],
  });

  const { addon } = await installTestTheme(NO_INTENSITY_COLORWAY_THEME_ID);

  let win = await loadInitialView("theme");
  let doc = win.document;

  // Add mocked fluent resources
  doc.l10n.addResourceIds(["mock-colorways.ftl"]);

  let colorwaySection = getSection(doc, "colorways-section");
  ok(colorwaySection, "colorway section was found");

  // Make sure fluent strings have all been translated before
  // asserting the expected element to not have empty textContent.
  await doc.l10n.translateFragment(colorwaySection);

  info("Verifying colorway closet list contents");
  ok(
    colorwaySection.querySelector(".list-section-heading"),
    "colorway closet heading was found"
  );
  ok(
    colorwaySection.querySelector(".list-section-subheading"),
    "colorway closet subheading was found"
  );

  let card = colorwaySection.querySelector("colorways-card");
  ok(card, "colorway closet card was found");

  info("Verifying colorway closet card contents");
  ok(
    card.querySelector("#colorways-preview-text-container"),
    "Preview text container found"
  );

  const cardImage = card.querySelector(".card-heading-image");
  ok(cardImage, "Preview image container found");
  is(
    cardImage.src,
    "mockCollectionPreview.avif",
    "Preview image has correct source"
  );

  const previewTextHeader = card.querySelector(
    "#colorways-preview-text-container > h3"
  );
  ok(previewTextHeader.textContent, "Preview text header should not be empty");

  const previewTextSubheader = card.querySelector(
    "#colorways-preview-text-container > p"
  );
  ok(
    previewTextSubheader.textContent,
    "Preview text subheader should not be empty"
  );
  ok(
    card.querySelector("#colorways-expiry-date-container"),
    "colorway collection expiry date container found"
  );
  const expiryDateSpan = card.querySelector("#colorways-expiry-date > span");
  ok(
    expiryDateSpan.textContent,
    "colorway collection expiry date should not be empty"
  );

  ok(
    card.querySelector("#colorways-button"),
    "colorway collection button found"
  );

  await closeView(win);
  await addon.uninstall(true);
  await SpecialPowers.popPrefEnv();
  clearBuiltInThemesStubs();
});

/**
 * Tests that the colorway closet section does not appear if a colorway collection
 * is available but the colorway closet pref is disabled.
 */
add_task(async function testColorwayClosetSectionPrefDisabled() {
  const clearBuiltInThemesStubs = initBuiltInThemesStubs();
  await SpecialPowers.pushPrefEnv({
    set: [["browser.theme.colorway-closet", false]],
  });

  const { addon } = await installTestTheme(NO_INTENSITY_COLORWAY_THEME_ID);

  let win = await loadInitialView("theme");
  let doc = win.document;
  let colorwaySection = getSection(doc, "colorways-section");

  ok(!colorwaySection, "colorway section should not be found");

  await closeView(win);
  await addon.uninstall(true);
  await SpecialPowers.popPrefEnv();
  clearBuiltInThemesStubs();
});

/**
 * Tests that the colorway closet opener is called when the Try Colorways button
 * is selected.
 */
add_task(async function testButtonOpenModal() {
  const clearBuiltInThemesStubs = initBuiltInThemesStubs();
  const clearColorwayClosetOpenerStubs = initColorwayClosetOpenerStubs();

  await SpecialPowers.pushPrefEnv({
    set: [["browser.theme.colorway-closet", true]],
  });

  const { addon } = await installTestTheme(NO_INTENSITY_COLORWAY_THEME_ID);

  let win = await loadInitialView("theme");
  let doc = win.document;
  let colorwaySection = getSection(doc, "colorways-section");

  ok(colorwaySection, "colorway section was found");

  let card = colorwaySection.querySelector("colorways-card");
  ok(card, "colorway closet card was found");

  let colorwaysButton = card.querySelector("#colorways-button");
  ok(colorwaysButton, "colorway collection button found");

  colorwaysButton.click();

  ok(ColorwayClosetOpener.openModal.calledOnce, "Got expected openModal call");

  await closeView(win);
  await addon.uninstall(true);
  await SpecialPowers.popPrefEnv();
  clearBuiltInThemesStubs();
  clearColorwayClosetOpenerStubs();
});

/**
 * Tests that disabled retained expired colorways appear in the list of retained
 * colorway themes, while disabled unexpired ones do not.
 */
add_task(async function testColorwayClosetSectionOneRetainedOneUnexpired() {
  const clearBuiltInThemesStubs = initBuiltInThemesStubs();
  await SpecialPowers.pushPrefEnv({
    set: [["browser.theme.colorway-closet", true]],
  });

  // Set expired theme as a retained colorway theme
  const retainedThemePrefName = "browser.theme.retainedExpiredThemes";
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        retainedThemePrefName,
        JSON.stringify([NO_INTENSITY_EXPIRED_COLORWAY_THEME_ID]),
      ],
    ],
  });
  const { addon: expiredAddon } = await installTestTheme(
    NO_INTENSITY_EXPIRED_COLORWAY_THEME_ID
  );

  // Set up a valid addon that acts as a colorway theme that is not yet expired
  const { addon: validAddon } = await installTestTheme(
    NO_INTENSITY_COLORWAY_THEME_ID
  );

  await expiredAddon.disable();
  await validAddon.disable();

  let win = await loadInitialView("theme");
  let doc = win.document;
  let colorwaySection = getSection(doc, "colorways-section");

  info("Verifying colorway section order of elements");
  ok(
    colorwaySection.children.length,
    "colorway section should have at least 1 element"
  );
  is(
    colorwaySection.children[0].classList[0],
    "list-section-heading",
    "colorway section header should be first"
  );
  is(
    colorwaySection.children[1].classList[0],
    "list-section-subheading",
    "colorway section subheader should be second"
  );
  is(
    colorwaySection.children[2].tagName.toLowerCase(),
    "colorways-card",
    "colorway closet list should be third"
  );
  is(
    colorwaySection.children[3].tagName.toLowerCase(),
    "addon-card",
    "addon theme card should be fourth"
  );

  info("Verifying cards in list of retained colorway themes");
  let expiredAddonCard = colorwaySection.querySelector(
    `addon-card[addon-id='${NO_INTENSITY_EXPIRED_COLORWAY_THEME_ID}']`
  );
  ok(
    colorwaySection.contains(expiredAddonCard),
    "Colorways section contains the expired theme."
  );

  let disabledSection = getSection(doc, "theme-disabled-section");
  expiredAddonCard = disabledSection.querySelector(
    `addon-card[addon-id='${NO_INTENSITY_EXPIRED_COLORWAY_THEME_ID}']`
  );
  ok(
    !disabledSection.contains(expiredAddonCard),
    "The regular, non-Colorways 'Disabled' section does not contain the expired theme."
  );

  let validAddonCard = colorwaySection.querySelector(
    `addon-card[addon-id='${NO_INTENSITY_COLORWAY_THEME_ID}']`
  );
  ok(
    !colorwaySection.contains(validAddonCard),
    "Colorways section does not contain valid theme."
  );

  await closeView(win);
  await expiredAddon.uninstall(true);
  await validAddon.uninstall(true);
  await SpecialPowers.popPrefEnv();
  await SpecialPowers.popPrefEnv();
  clearBuiltInThemesStubs();
});

/**
 * Tests that the Colorway Closet does not appear when there is no active
 * collection, and that retained themes are still visible.
 */
add_task(async function testColorwayNoActiveCollection() {
  const clearBuiltInThemesStubs = initBuiltInThemesStubs(false);
  await SpecialPowers.pushPrefEnv({
    set: [["browser.theme.colorway-closet", true]],
  });

  // Set expired theme as a retained colorway theme
  const retainedThemePrefName = "browser.theme.retainedExpiredThemes";
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        retainedThemePrefName,
        JSON.stringify([NO_INTENSITY_EXPIRED_COLORWAY_THEME_ID]),
      ],
    ],
  });
  const { addon: expiredAddon } = await installTestTheme(
    NO_INTENSITY_EXPIRED_COLORWAY_THEME_ID
  );
  await expiredAddon.disable();

  let win = await loadInitialView("theme");
  let doc = win.document;
  let colorwaySection = getSection(doc, "colorways-section");

  ok(colorwaySection, "colorway section was found");
  ok(
    !colorwaySection.querySelector("colorways-card"),
    "colorway closet card was not found"
  );

  info("Verifying that header and subheader are still visible");
  is(
    colorwaySection.children[0].classList[0],
    "list-section-heading",
    "colorway section header should be first"
  );
  is(
    colorwaySection.children[1].classList[0],
    "list-section-subheading",
    "colorway section subheader should be second"
  );

  let expiredAddonCard = colorwaySection.querySelector(
    `addon-card[addon-id='${NO_INTENSITY_EXPIRED_COLORWAY_THEME_ID}']`
  );
  ok(
    colorwaySection.contains(expiredAddonCard),
    "Colorways section contains the expired theme."
  );

  await closeView(win);
  await expiredAddon.uninstall(true);
  await SpecialPowers.popPrefEnv();
  await SpecialPowers.popPrefEnv();
  clearBuiltInThemesStubs();
});

/**
 * Tests that the Colorway Closet card's CTA button changes text when there
 * is a colorway enabled.
 */
add_task(async function testColorwayButtonTextWithColorwayEnabled() {
  const clearBuiltInThemesStubs = initBuiltInThemesStubs();
  await SpecialPowers.pushPrefEnv({
    set: [["browser.theme.colorway-closet", true]],
  });

  const { addon: validColorway } = await installTestTheme(
    BALANCED_COLORWAY_THEME_ID
  );
  const { addon: expiredColorway } = await installTestTheme(
    NO_INTENSITY_EXPIRED_COLORWAY_THEME_ID
  );
  let win = await loadInitialView("theme");
  let doc = win.document;

  // Add mocked fluent resources
  doc.l10n.addResourceIds(["mock-colorways.ftl"]);

  await validColorway.disable();
  await expiredColorway.disable();

  let colorwaySection = getSection(doc, "colorways-section");
  ok(colorwaySection, "colorway section was found");

  let card = colorwaySection.querySelector("colorways-card");
  ok(card, "colorway closet card was found");

  let colorwaysButton = card.querySelector("#colorways-button");
  ok(colorwaysButton, "colorway collection button found");

  is(
    colorwaysButton.getAttribute("data-l10n-id"),
    "theme-colorways-button",
    "button has the expected fluent id when no colorway is enabled"
  );

  await expiredColorway.enable();

  is(
    colorwaysButton.getAttribute("data-l10n-id"),
    "theme-colorways-button",
    "button fluent id remains unchanged since enabled colorway is not in active collection"
  );

  await expiredColorway.enable();

  is(
    colorwaysButton.getAttribute("data-l10n-id"),
    "theme-colorways-button",
    "button fluent id remains unchanged since enabled colorway is not in active collection"
  );

  await validColorway.enable();

  is(
    colorwaysButton.getAttribute("data-l10n-id"),
    "theme-colorways-button-colorway-enabled",
    "button fluent id is updated since newly enabled colorway is found in active collection"
  );

  // Make sure the updated fluent id is also defined in the fluent files loaded
  await doc.l10n.translateFragment(colorwaySection);

  await closeView(win);
  await validColorway.uninstall(true);
  await expiredColorway.uninstall(true);
  await SpecialPowers.popPrefEnv();
  clearBuiltInThemesStubs();
});

/**
 * Tests that telemetry is registered when the Try Colorways button is selected.
 */
add_task(async function test_telemetry_trycolorways_button() {
  const clearBuiltInThemesStubs = initBuiltInThemesStubs();
  Services.telemetry.clearEvents();
  await SpecialPowers.pushPrefEnv({
    set: [["browser.theme.colorway-closet", true]],
  });

  const { addon } = await installTestTheme(NO_INTENSITY_COLORWAY_THEME_ID);
  await addon.disable();

  let win = await loadInitialView("theme");
  let doc = win.document;
  let colorwaySection = getSection(doc, "colorways-section");
  let card = colorwaySection.querySelector("colorways-card");

  let colorwaysButton = card.querySelector("#colorways-button");
  ok(colorwaysButton, "colorway collection button found");

  info("Selecting colorways button with all colorways disabled");
  colorwaysButton.click();

  await waitForColorwaysTelemetryPromise();

  TelemetryTestUtils.assertEvents(
    [
      {
        category: "colorways_modal",
        method: "try_colorways",
        object: "aboutaddons",
      },
    ],
    { category: "colorways_modal", object: "aboutaddons" }
  );

  await closeView(win);
  await addon.uninstall(true);
  await SpecialPowers.popPrefEnv();
  clearBuiltInThemesStubs();
  Services.telemetry.clearEvents();
});

/**
 * Tests that telemetry is registered when the Change Colorways button is selected.
 */
add_task(async function test_telemetry_changecolorway_button() {
  const clearBuiltInThemesStubs = initBuiltInThemesStubs();
  Services.telemetry.clearEvents();
  await SpecialPowers.pushPrefEnv({
    set: [["browser.theme.colorway-closet", true]],
  });

  const { addon } = await installTestTheme(NO_INTENSITY_COLORWAY_THEME_ID);
  await addon.enable();

  let win = await loadInitialView("theme");
  let doc = win.document;
  let colorwaySection = getSection(doc, "colorways-section");
  let card = colorwaySection.querySelector("colorways-card");

  let colorwaysButton = card.querySelector("#colorways-button");
  ok(colorwaysButton, "colorway collection button found");

  info("Selecting colorways button with a colorway enabled");
  colorwaysButton.click();

  await waitForColorwaysTelemetryPromise();

  TelemetryTestUtils.assertEvents(
    [
      {
        category: "colorways_modal",
        method: "change_colorway",
        object: "aboutaddons",
      },
    ],
    { category: "colorways_modal", object: "aboutaddons" }
  );

  await closeView(win);
  await addon.uninstall(true);
  await SpecialPowers.popPrefEnv();
  clearBuiltInThemesStubs();
  Services.telemetry.clearEvents();
});
