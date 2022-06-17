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

AddonTestUtils.initMochitest(this);

const kTestThemeId = "test-colorway@mozilla.org";

// Return a mock expiry date set 1 year ahead from the current date.
function getMockExpiry() {
  const expireDate = new Date();
  expireDate.setFullYear(expireDate.getFullYear() + 1);
  return expireDate;
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
  const clearBuiltInThemeConfigMock = () => {
    if (
      BuiltInThemeConfig.findActiveColorwayCollection !==
      originalFindActiveCollection
    ) {
      BuiltInThemeConfig.findActiveColorwayCollection = originalFindActiveCollection;
    }
  };
  registerCleanupFunction(clearBuiltInThemeConfigMock);

  // Mock collection l10n part of the mocked fluent resources.
  const mockL10nId = "colorway-collection-test-mock";

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

  info("Now mocking BuiltInThemeConfig.findActiveColorwayCollection");
  BuiltInThemeConfig.findActiveColorwayCollection = () => {
    info(
      `Return mock active colorway collection with expiry set to: ${
        mockExpiry.toUTCString().split("T")[0]
      }`
    );
    return {
      id: "colorway-test-collection",
      expiry: mockExpiry,
      l10nId: { title: mockL10nId },
    };
  };

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
  ok(
    doc.head.querySelector("link[href='preview/colorwaycloset.ftl']"),
    "Expect a link to the colorwaycloset fluent resources to be found"
  );

  // Add mocked fluent resources for the mocked active colorway collection.
  doc.l10n.addResourceIds(["mock-colorwaycloset.ftl"]);

  let colorwayClosetList = doc.querySelector("colorways-list");

  // Make sure fluent strings have all been translated before
  // asserting the expected element to not have empty textContent.
  await doc.l10n.translateFragment(colorwayClosetList);

  info("Verifying colorway closet list contents");
  ok(colorwayClosetList, "colorway closet list was found");
  ok(
    colorwayClosetList.querySelector("#colorways-section-heading"),
    "colorway closet heading was found"
  );
  ok(
    colorwayClosetList.querySelector("#colorways-section-subheading"),
    "colorway closet subheading was found"
  );

  let cards = colorwayClosetList.querySelectorAll("colorways-card");
  ok(cards.length, "At least one colorway closet card was found");

  info("Verifying colorway closet card contents");
  let card = cards[0];
  ok(
    card.querySelector("#colorways-preview-text-container"),
    "Preview text container found"
  );
  ok(
    card.querySelector(".card-heading-image"),
    "Preview image container found"
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
  clearBuiltInThemeConfigMock();
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

/**
 * Tests that the colorway closet opener is called when the Try Colorways button
 * is selected.
 */
add_task(async function testButtonOpenModal() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.theme.colorway-closet", true]],
  });

  let originalOpenModal = ColorwayClosetOpener.openModal;
  const clearOpenModalMock = () => {
    if (originalOpenModal) {
      ColorwayClosetOpener.openModal = originalOpenModal;
      originalOpenModal = null;
    }
  };
  registerCleanupFunction(clearOpenModalMock);

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

  ok(colorwayClosetList, "colorway closet list was found");

  let cards = colorwayClosetList.querySelectorAll("colorways-card");
  ok(cards.length, "At least one colorway closet card was found");

  let colorwaysButton = cards[0].querySelector("#colorways-button");
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
});
