/* Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from ../../../../../browser/components/colorways/tests/browser/head.js */
loadTestSubscript(
  "../../../../../browser/components/colorways/tests/browser/head.js"
);

AddonTestUtils.initMochitest(this);

/**
 * This function behaves similarily to testInColorwayClosetModal, except it is customized for
 * thoroughly testing about:addons callbacks passed to the Colorway Closet modal.
 * @param {Boolean} isCancel true if modal is to be closed using the Cancel button, or false if setting a colorway
 */
async function testOpenModal(isCancel) {
  // Set up mock themes
  const clearBuiltInThemesStubs = initBuiltInThemesStubs();
  const mockThemes = [
    SOFT_COLORWAY_THEME_ID,
    BALANCED_COLORWAY_THEME_ID,
    BOLD_COLORWAY_THEME_ID,
  ];
  const mockThemesAddons = [];
  const mockThemesUninstall = async () => {
    for (const addon of mockThemesAddons) {
      await addon.uninstall();
    }
  };
  registerCleanupFunction(mockThemesUninstall);
  for (const theme of mockThemes) {
    const { addon } = await installTestTheme(theme);
    await addon.disable();
    mockThemesAddons.push(addon);
  }

  const sinonSandbox = sinon.createSandbox();
  let win = await loadInitialView("theme");
  let pageDocument = win.document;
  let ColorwayClosetCard = win.customElements.get("colorways-card");

  const colorwayOpenModalSpy = sinonSandbox.spy(
    ColorwayClosetOpener,
    "openModal"
  );

  // Verify that active theme is not tested colorway and not listed as enabled in about:addons.
  const activeThemeId = Services.prefs.getStringPref(
    "extensions.activeThemeID"
  );
  let enabledSection = getSection(pageDocument, "theme-enabled-section");
  isnot(
    activeThemeId,
    BALANCED_COLORWAY_THEME_ID,
    "Current Active theme should not be a colorway test theme"
  );
  ok(
    !enabledSection.querySelector(
      `addon-card[addon-id='${BALANCED_COLORWAY_THEME_ID}']`
    ),
    "Soon to be tested colorway should not be in enabled section in about:addons"
  );

  // Set up Colorway Closet Card spies
  const callOnModalClosedSpy = sinonSandbox.spy(
    ColorwayClosetCard,
    "callOnModalClosed"
  );
  const runPendingCallbacksSpy = sinonSandbox.spy(
    ColorwayClosetCard,
    "runPendingModalClosedCallbacks"
  );

  const waitForAddonDisabled = addonId =>
    AddonTestUtils.promiseAddonEvent("onDisabled", addon => {
      // Log the onDisabled events to make it easier to investigate unexpected test failures.
      info(
        `Seen addon onDisabled addon event for ${addon.id} (waiting for ${addonId})`
      );
      return addon.id === addonId;
    });

  // onDisabled is triggered as a side-effect of enabling a different theme and should
  // always be called after onEnabled for that theme.
  const promiseDisabledAfterOpenModal = waitForAddonDisabled(activeThemeId);

  // Open modal
  info("Open the colorway closet dialog by clicking colorways CTA button");
  is(
    ColorwayClosetCard.hasModalOpen,
    false,
    "ColorwayClosetCard.hasModalOpen should be false"
  );
  const colorwaysButton = pageDocument.querySelector(
    "[action='open-colorways']"
  );
  colorwaysButton.click();

  // Verify callbacks after opening modal
  is(
    colorwayOpenModalSpy.callCount,
    1,
    "Expect 1 ColorwayClosetOpener.openModal call"
  );
  is(
    ColorwayClosetCard.hasModalOpen,
    true,
    "ColorwayClosetCard.hasModalOpen has been updated"
  );

  // Wait for modal to load
  const { dialog, closedPromise } = colorwayOpenModalSpy.getCall(0).returnValue;
  ok(closedPromise instanceof Promise, "Got closedPromise as expected");
  await dialog._dialogReady;
  let modalDocument = dialog._frame.contentDocument;
  let modalWindow = dialog._frame.contentWindow;

  const {
    cancelButton,
    colorwayIntensities,
    setColorwayButton,
  } = getColorwayClosetTestElements(modalDocument);

  info("Wait for theme update");
  await promiseDisabledAfterOpenModal;

  const activeColorwayThemeId = Services.prefs.getStringPref(
    "extensions.activeThemeID"
  );

  ok(
    BuiltInThemes.isColorwayFromCurrentCollection(activeColorwayThemeId),
    `Expect ${activeColorwayThemeId} to be in the active colorway collection`
  );
  isnot(
    activeColorwayThemeId,
    SOFT_COLORWAY_THEME_ID,
    "Automatically selected colorway theme should not be the same as the one to be manually selected"
  );

  // Ensure that about:addons UI is not updated after loading a colorway from the modal.
  enabledSection = getSection(pageDocument, "theme-enabled-section");
  ok(
    !enabledSection.querySelector(
      `addon-card[addon-id='${activeColorwayThemeId}']`
    ),
    "Selected colorway in modal should not be visible in enabled section in about:addons"
  );

  is(
    callOnModalClosedSpy.callCount,
    2,
    "Expected callOnModalClosed to be called twice"
  );
  // Expected number of closedModalCallbacks:
  // 1. Enabled balanced colorway
  // 2. Disabled default theme
  is(
    ColorwayClosetCard.closedModalCallbacks.size,
    2,
    "There should be 2 closedModalCallbacks in total"
  );

  const promiseDisabledAfterChange = waitForAddonDisabled(
    activeColorwayThemeId
  );

  // Wait for intensity button to be initialized
  await BrowserTestUtils.waitForMutationCondition(
    colorwayIntensities,
    { subtree: true, attributeFilter: ["value"] },
    () =>
      colorwayIntensities.querySelector(
        `input[value="${SOFT_COLORWAY_THEME_ID}"]`
      ),
    "Waiting for intensity button to be available"
  );

  info("Changing intensity");
  colorwayIntensities
    .querySelector(`input[value="${SOFT_COLORWAY_THEME_ID}"]`)
    .click();
  await promiseDisabledAfterChange;

  // Expected number of closedModalCallbacks:
  // 1. Enabled colorway (automatically selected on modal opened)
  // 2. Disabled default theme
  // 3. Enabled soft colorway (manually selected in the modal)
  // 4. Disabled colorway (automatically selected on modal opened)
  is(
    ColorwayClosetCard.closedModalCallbacks.size,
    4,
    "There should be 4 closedModalCallbacks in total"
  );

  // Add a sentinelCallbackSpy to the closedModalCallbacks set to
  // verify that callbacks are tracked after setting a colorway and
  // closing the modal.
  let sentinelCallbackSpy = sinonSandbox.spy();
  ColorwayClosetCard.closedModalCallbacks.add(sentinelCallbackSpy);

  let modalClosedPromise = BrowserTestUtils.waitForEvent(
    modalWindow,
    "unload",
    "Waiting for modal to close"
  );

  if (isCancel) {
    info("Selecting cancel button");
    cancelButton.click();
  } else {
    info("Selecting set colorway button");
    setColorwayButton.click();
  }
  info("Closing modal");
  await modalClosedPromise;
  is(
    ColorwayClosetCard.hasModalOpen,
    false,
    "ColorwayClosetCard.hasModalOpen should be false"
  );

  // ColorwayClosetCard should track remaining pending callbacks.
  ok(
    runPendingCallbacksSpy.calledOnce,
    "Expect ColorwayClosetCard.runPendingModalClosedCallbacks to have been called once"
  );
  if (isCancel) {
    ok(
      sentinelCallbackSpy.notCalled,
      "Expect sentinel callback to not be called"
    );
  } else {
    ok(sentinelCallbackSpy.called, "Expect sentinel callback to be called");
  }

  is(
    ColorwayClosetCard.closedModalCallbacks.size,
    0,
    "There should be 0 closedModalCallbacks in total"
  );

  // Verify UI again after closing the modal
  enabledSection = getSection(pageDocument, "theme-enabled-section");
  if (isCancel) {
    ok(
      !enabledSection.querySelector(
        `addon-card[addon-id='${SOFT_COLORWAY_THEME_ID}']`
      ),
      "Selected colorway should not be in enabled section in about:addons"
    );
  } else {
    ok(
      enabledSection.querySelector(
        `addon-card[addon-id='${SOFT_COLORWAY_THEME_ID}']`
      ),
      "Selected colorway should be in enabled section in about:addons"
    );
  }

  await closeView(win);
  await clearBuiltInThemesStubs();
  await mockThemesUninstall();
  sinonSandbox.restore();
  Services.telemetry.clearEvents();
}

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
  await addon.uninstall();
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
  await addon.uninstall();
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
  await addon.uninstall();
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
  await expiredAddon.uninstall();
  await validAddon.uninstall();
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
  let disabledSection = getSection(doc, "theme-disabled-section");

  ok(!colorwaySection, "colorway section should not exist");

  let expiredAddonCard = disabledSection.querySelector(
    `addon-card[addon-id='${NO_INTENSITY_EXPIRED_COLORWAY_THEME_ID}']`
  );
  ok(
    disabledSection.contains(expiredAddonCard),
    "Disabled themes section contains the expired theme."
  );

  await closeView(win);
  await expiredAddon.uninstall();
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
  await validColorway.uninstall();
  await expiredColorway.uninstall();
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
  await addon.uninstall();
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
  await addon.uninstall();
  await SpecialPowers.popPrefEnv();
  clearBuiltInThemesStubs();
  Services.telemetry.clearEvents();
});

/**
 * Tests that:
 * - the about:addons page does not visually update when a colorway selection is made in the modal
 * - the about:addons page does not visually update if the Cancel button is selected and the modal closes
 */
add_task(async function test_aboutaddons_modal_callbacks_onclosed_cancel() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.theme.colorway-closet", true]],
  });
  await testOpenModal(true);
  await SpecialPowers.popPrefEnv();
});

/**
 * Tests that:
 * - the about:addons page does not visually update when a colorway selection is made in the modal
 * - the about:addons page is visually updated if the Set Colorway button is selected and the modal closes
 */
add_task(
  async function test_aboutaddons_modal_callbacks_onclosed_set_colorway() {
    await SpecialPowers.pushPrefEnv({
      set: [["browser.theme.colorway-closet", true]],
    });
    await testOpenModal(false);
    await SpecialPowers.popPrefEnv();
  }
);
