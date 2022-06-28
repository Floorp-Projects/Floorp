/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);
const { BuiltInThemeConfig } = ChromeUtils.import(
  "resource:///modules/BuiltInThemeConfig.jsm"
);

const { ColorwayClosetOpener } = ChromeUtils.import(
  "resource:///modules/ColorwayClosetOpener.jsm"
);
const { BuiltInThemes } = ChromeUtils.import(
  "resource:///modules/BuiltInThemes.jsm"
);

AddonTestUtils.initMochitest(this);

const kTestThemeId = "test-colorway@mozilla.org";
const kTestExpiredThemeId = `expired-${kTestThemeId}`;
// Mock collection l10n is part of the mocked fluent resources.
const mockL10nId = "colorway-collection-test-mock";

// Return a mock expiry date set 1 year ahead from the current date.
function getMockExpiry() {
  const expireDate = new Date();
  expireDate.setFullYear(expireDate.getFullYear() + 1);
  return expireDate;
}

function getMockThemeXpi(id) {
  return AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      name: "Monochromatic Theme",
      applications: { gecko: { id } },
      theme: {},
    },
  });
}

function setMockThemeToExpired(id) {
  let yesterday = new Date();
  yesterday.setDate(yesterday.getDate() - 1);
  yesterday = yesterday.toISOString().split("T")[0];
  // Add the test theme to our list of built-in themes so that aboutaddons.js
  // will think this theme is expired.
  BuiltInThemes.builtInThemeMap.set(id, {
    version: "1.0",
    expiry: yesterday,
    // We use the manifest from Light theme since we know it will be in-tree
    // indefinitely.
    path: "resource://builtin-themes/light/",
  });
}

function setBuiltInThemeConfigMock(...args) {
  info("Mocking BuiltInThemeConfig.findActiveColorwaysCollection");
  BuiltInThemeConfig.findActiveColorwayCollection = () => {
    // Return no active collection
    if (!args || !args.length) {
      info("Return no active collection");
      return null;
    }

    const { mockExpiry, mockL10nId } = args[0];
    info(
      `Return mock active colorway collection with expiry set to: ${
        mockExpiry.toUTCString().split("T")[0]
      }`
    );
    return {
      id: "colorway-test-collection",
      expiry: mockExpiry,
      l10nId: {
        title: mockL10nId,
      },
      cardImagePath: "mockCollectionPreview.avif",
    };
  };
}

function clearBuiltInThemeConfigMock(originalFindActiveCollection) {
  info("Cleaning up BuiltInThemeConfigMock");
  if (
    BuiltInThemeConfig.findActiveColorwayCollection !==
    originalFindActiveCollection
  ) {
    BuiltInThemeConfig.findActiveColorwayCollection = originalFindActiveCollection;
  }
}

add_setup(async function() {
  info("Register mock fluent locale strings");

  let tmpDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
  tmpDir.append("l10n-colorwaycloset-mocks");

  await IOUtils.makeDirectory(tmpDir.path, { ignoreExisting: true });
  await IOUtils.writeUTF8(
    PathUtils.join(tmpDir.path, "mock-colorwaycloset.ftl"),
    [
      "colorway-collection-test-mock = Mock collection title",
      "colorway-collection-test-mock-subheading = Mock collection subheading",
    ].join("\n")
  );
  let resProto = Services.io
    .getProtocolHandler("resource")
    .QueryInterface(Ci.nsIResProtocolHandler);

  resProto.setSubstitution(
    "l10n-colorwaycloset-mocks",
    Services.io.newFileURI(tmpDir)
  );

  let mockSource = new L10nFileSource(
    "colorwayscloset-mocks",
    "app",
    ["en-US"],
    "resource://l10n-colorwaycloset-mocks/"
  );

  let l10nReg = L10nRegistry.getInstance();
  l10nReg.registerSources([mockSource]);

  registerCleanupFunction(async () => {
    l10nReg.removeSources([mockSource]);
    resProto.setSubstitution("l10n-colorwaycloset-mocks", null);
    info(`Clearing temporary directory ${tmpDir.path}`);
    await IOUtils.remove(tmpDir.path, { recursive: true, ignoreAbsent: true });
  });

  // Confirm that the mock fluent resources are available as expected.
  let bundles = l10nReg.generateBundles(["en-US"], ["mock-colorwaycloset.ftl"]);
  let bundle0 = (await bundles.next()).value;
  is(
    bundle0.locales[0],
    "en-US",
    "Got the expected locale in the mock L10nFileSource"
  );
  ok(
    bundle0.hasMessage("colorway-collection-test-mock"),
    "Got the expected l10n id in the mock L10nFileSource"
  );
});

/**
 * Tests that the colorway closet section only appears if a colorway collection
 * is available and the colorway closet pref is enabled.
 */
add_task(async function testColorwayClosetPrefEnabled() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.theme.colorway-closet", true]],
  });

  // Mock BuiltInThemeConfig.findActiveColorwaysCollection with test colorways.
  const originalFindActiveCollection =
    BuiltInThemeConfig.findActiveColorwayCollection;
  registerCleanupFunction(() => {
    clearBuiltInThemeConfigMock(originalFindActiveCollection);
  });

  // Mock expiry date string and BuiltInThemeConfig.findActiveColorwayCollection()
  const mockExpiry = getMockExpiry();

  info(
    "Verify first if any data from BuiltInThemeConfig.findActiveColorwayCollection is valid"
  );
  let collection = BuiltInThemeConfig.findActiveColorwayCollection();
  if (collection) {
    info("Found a collection");
    ok(collection.l10nId, "Collection in BuiltInThemeConfig has l10n data");
    ok(
      collection.l10nId.title,
      "Collection in BuiltInThemeConfig has valid l10n title"
    );
  }

  setBuiltInThemeConfigMock({ mockExpiry, mockL10nId });

  const themeXpi = getMockThemeXpi(kTestThemeId);
  const { addon } = await AddonTestUtils.promiseInstallFile(themeXpi);

  let win = await loadInitialView("theme");
  let doc = win.document;
  ok(
    doc.head.querySelector("link[href='preview/colorwaycloset.ftl']"),
    "Expect a link to the colorwaycloset fluent resources to be found"
  );

  // Add mocked fluent resources for the mocked active colorway collection.
  doc.l10n.addResourceIds(["mock-colorwaycloset.ftl"]);

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
  clearBuiltInThemeConfigMock(originalFindActiveCollection);
});

/**
 * Tests that the colorway closet section does not appear if a colorway collection
 * is available but the colorway closet pref is disabled.
 */
add_task(async function testColorwayClosetSectionPrefDisabled() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.theme.colorway-closet", false]],
  });
  const themeXpi = getMockThemeXpi(kTestThemeId);
  const { addon } = await AddonTestUtils.promiseInstallFile(themeXpi);

  let win = await loadInitialView("theme");
  let doc = win.document;
  let colorwaySection = getSection(doc, "colorways-section");
  ok(!colorwaySection, "colorway section should not be found");

  await closeView(win);
  await addon.uninstall(true);
  await SpecialPowers.popPrefEnv();
});

/**
 * Tests that the colorway closet opener is called when the Try Colorways button
 * is selected.
 */
add_task(async function testButtonOpenModal() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.theme.colorway-closet", true]],
  });

  // Mock BuiltInThemeConfig.findActiveColorwaysCollection with test colorways.
  const originalFindActiveCollection =
    BuiltInThemeConfig.findActiveColorwayCollection;
  registerCleanupFunction(() => {
    clearBuiltInThemeConfigMock(originalFindActiveCollection);
  });

  // Mock expiry date string and BuiltInThemeConfig.findActiveColorwayCollection()
  const mockExpiry = getMockExpiry();
  setBuiltInThemeConfigMock({ mockExpiry, mockL10nId });

  let originalOpenModal = ColorwayClosetOpener.openModal;
  const clearOpenModalMock = () => {
    if (originalOpenModal) {
      ColorwayClosetOpener.openModal = originalOpenModal;
      originalOpenModal = null;
    }
  };
  registerCleanupFunction(clearOpenModalMock);

  const themeXpi = getMockThemeXpi(kTestThemeId);
  const { addon } = await AddonTestUtils.promiseInstallFile(themeXpi);

  let win = await loadInitialView("theme");
  let doc = win.document;
  let colorwaySection = getSection(doc, "colorways-section");

  ok(colorwaySection, "colorway section was found");

  let card = colorwaySection.querySelector("colorways-card");
  ok(card, "colorway closet card was found");

  let colorwaysButton = card.querySelector("#colorways-button");
  ok(colorwaysButton, "colorway collection button found");
  let colorwayOpenerPromise = new Promise(resolve => {
    ColorwayClosetOpener.openModal = () => {
      ok(true, "openModal was called");
      resolve();
    };
  });
  colorwaysButton.click();
  info("Waiting for openModal promise to resolve");
  await colorwayOpenerPromise;

  await closeView(win);
  await addon.uninstall(true);
  clearOpenModalMock();
  await SpecialPowers.popPrefEnv();
  clearBuiltInThemeConfigMock(originalFindActiveCollection);
});

/**
 * Tests that disabled retained expired colorways appear in the list of retained
 * colorway themes, while disabled unexpired ones do not.
 */
add_task(async function testColorwayClosetSectionOneRetainedOneUnexpired() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.theme.colorway-closet", true]],
  });

  // Mock BuiltInThemeConfig.findActiveColorwaysCollection with test colorways.
  const originalFindActiveCollection =
    BuiltInThemeConfig.findActiveColorwayCollection;
  registerCleanupFunction(() => {
    clearBuiltInThemeConfigMock(originalFindActiveCollection);
  });

  // Mock expiry date string and BuiltInThemeConfig.findActiveColorwayCollection()
  const mockExpiry = getMockExpiry();
  setBuiltInThemeConfigMock({ mockExpiry, mockL10nId });

  // Set expired theme as a retained colorway theme
  const retainedThemePrefName = "browser.theme.retainedExpiredThemes";
  await SpecialPowers.pushPrefEnv({
    set: [[retainedThemePrefName, JSON.stringify([kTestExpiredThemeId])]],
  });
  const themeXpiExpiredAddon = getMockThemeXpi(kTestExpiredThemeId);
  const expiredAddon = (
    await AddonTestUtils.promiseInstallFile(themeXpiExpiredAddon)
  ).addon;

  // Set up a valid addon that acts as a colorway theme that is not yet expired
  const validThemeId = `valid-${kTestThemeId}`;
  const themeXpiValidAddon = getMockThemeXpi(validThemeId);
  const validAddon = (
    await AddonTestUtils.promiseInstallFile(themeXpiValidAddon)
  ).addon;

  await expiredAddon.disable();
  await validAddon.disable();

  // Make the test theme appear expired.
  setMockThemeToExpired(kTestExpiredThemeId);
  registerCleanupFunction(() => {
    BuiltInThemes.builtInThemeMap.delete(kTestExpiredThemeId);
  });

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
    `addon-card[addon-id='${kTestExpiredThemeId}']`
  );
  ok(
    colorwaySection.contains(expiredAddonCard),
    "Colorways section contains the expired theme."
  );
  let disabledSection = getSection(doc, "theme-disabled-section");
  expiredAddonCard = disabledSection.querySelector(
    `addon-card[addon-id='${kTestExpiredThemeId}']`
  );
  ok(
    !disabledSection.contains(expiredAddonCard),
    "The regular, non-Colorways 'Disabled' section does not contain the expired theme."
  );

  let validAddonCard = colorwaySection.querySelector(
    `addon-card[addon-id='${validThemeId}']`
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
  clearBuiltInThemeConfigMock(originalFindActiveCollection);
});

/**
 * Tests that the Colorway Closet does not appear when there is no active
 * collection, and that retained themes are still visible.
 */
add_task(async function testColorwayNoActiveCollection() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.theme.colorway-closet", true]],
  });

  // Mock BuiltInThemeConfig.findActiveColorwaysCollection with test colorways.
  const originalFindActiveCollection =
    BuiltInThemeConfig.findActiveColorwayCollection;
  registerCleanupFunction(() => {
    clearBuiltInThemeConfigMock(originalFindActiveCollection);
  });

  setBuiltInThemeConfigMock();

  // Set expired theme as a retained colorway theme
  const retainedThemePrefName = "browser.theme.retainedExpiredThemes";
  await SpecialPowers.pushPrefEnv({
    set: [[retainedThemePrefName, JSON.stringify([kTestExpiredThemeId])]],
  });
  const themeXpiExpiredAddon = getMockThemeXpi(kTestExpiredThemeId);
  const expiredAddon = (
    await AddonTestUtils.promiseInstallFile(themeXpiExpiredAddon)
  ).addon;

  await expiredAddon.disable();

  // Make the test theme appear expired.
  setMockThemeToExpired(kTestExpiredThemeId);
  registerCleanupFunction(() => {
    BuiltInThemes.builtInThemeMap.delete(kTestExpiredThemeId);
  });

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
    `addon-card[addon-id='${kTestExpiredThemeId}']`
  );
  ok(
    colorwaySection.contains(expiredAddonCard),
    "Colorways section contains the expired theme."
  );

  await closeView(win);
  await expiredAddon.uninstall(true);
  await SpecialPowers.popPrefEnv();
  await SpecialPowers.popPrefEnv();
  clearBuiltInThemeConfigMock(originalFindActiveCollection);
});
