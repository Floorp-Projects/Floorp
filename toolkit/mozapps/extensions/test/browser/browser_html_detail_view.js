/* eslint max-len: ["error", 80] */

const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm",
  {}
);

const { ExtensionPermissions } = ChromeUtils.import(
  "resource://gre/modules/ExtensionPermissions.jsm",
  {}
);

const SUPPORT_URL = Services.urlFormatter.formatURL(
  Services.prefs.getStringPref("app.support.baseURL")
);
const PB_SUMO_URL = SUPPORT_URL + "extensions-pb";
const DEFAULT_THEME_ID = "default-theme@mozilla.org";
const DARK_THEME_ID = "firefox-compact-dark@mozilla.org";

let gProvider;
let promptService;

AddonTestUtils.initMochitest(this);

function getAddonCard(doc, addonId) {
  return doc.querySelector(`addon-card[addon-id="${addonId}"]`);
}

function getDetailRows(card) {
  return Array.from(
    card.querySelectorAll('[name="details"] .addon-detail-row:not([hidden])')
  );
}

function checkLabel(row, name) {
  let id;
  if (name == "private-browsing") {
    // This id is carried over from the old about:addons.
    id = "detail-private-browsing-label";
  } else {
    id = `addon-detail-${name}-label`;
  }
  is(
    row.ownerDocument.l10n.getAttributes(row.querySelector("label")).id,
    id,
    `The ${name} label is set`
  );
}

function checkLink(link, url, text = url) {
  ok(link, "There is a link");
  is(link.href, url, "The link goes to the URL");
  if (text instanceof Object) {
    // Check the fluent data.
    Assert.deepEqual(
      link.ownerDocument.l10n.getAttributes(link),
      text,
      "The fluent data is set correctly"
    );
  } else {
    // Just check text.
    is(link.textContent, text, "The text is set");
  }
  is(link.getAttribute("target"), "_blank", "The link opens in a new tab");
}

function checkOptions(doc, options, expectedOptions) {
  let numOptions = expectedOptions.length;
  is(options.length, numOptions, `There are ${numOptions} options`);
  for (let i = 0; i < numOptions; i++) {
    let option = options[i];
    is(option.children.length, 2, "There are 2 children for the option");
    let input = option.firstElementChild;
    is(input.tagName, "INPUT", "The input is first");
    let text = option.lastElementChild;
    is(text.tagName, "SPAN", "The label text is second");
    let expected = expectedOptions[i];
    is(input.value, expected.value, "The value is right");
    is(input.checked, expected.checked, "The checked property is correct");
    Assert.deepEqual(
      doc.l10n.getAttributes(text),
      { id: expected.label, args: null },
      "The label has the right text"
    );
  }
}

function assertDeckHeadingHidden(group) {
  ok(group.hidden, "The tab group is hidden");
  for (let button of group.children) {
    ok(button.offsetHeight == 0, `The ${button.name} is hidden`);
  }
}

function assertDeckHeadingButtons(group, visibleButtons) {
  ok(!group.hidden, "The tab group is shown");
  ok(
    group.children.length >= visibleButtons.length,
    `There should be at least ${visibleButtons.length} buttons`
  );
  for (let button of group.children) {
    if (visibleButtons.includes(button.name)) {
      ok(!button.hidden, `The ${button.name} is shown`);
    } else {
      ok(button.hidden, `The ${button.name} is hidden`);
    }
  }
}

async function hasPrivateAllowed(id) {
  let perms = await ExtensionPermissions.get(id);
  return perms.permissions.includes("internal:privateBrowsingAllowed");
}

add_task(async function enableHtmlViews() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.allowPrivateBrowsingByDefault", false]],
  });

  gProvider = new MockProvider();
  gProvider.createAddons([
    {
      id: "addon1@mochi.test",
      name: "Test add-on 1",
      creator: { name: "The creator", url: "http://example.com/me" },
      version: "3.1",
      description: "Short description",
      fullDescription: "Longer description\nWith brs!",
      type: "extension",
      contributionURL: "http://localhost/contribute",
      averageRating: 4.279,
      userPermissions: {
        origins: ["<all_urls>", "file://*/*"],
        permissions: ["alarms", "contextMenus", "tabs", "webNavigation"],
      },
      reviewCount: 5,
      reviewURL: "http://example.com/reviews",
      homepageURL: "http://example.com/addon1",
      updateDate: new Date("2019-03-07T01:00:00"),
      applyBackgroundUpdates: AddonManager.AUTOUPDATE_ENABLE,
    },
    {
      id: "addon2@mochi.test",
      name: "Test add-on 2",
      creator: { name: "I made it" },
      description: "Short description",
      userPermissions: {
        origins: [],
        permissions: ["alarms", "contextMenus"],
      },
      type: "extension",
    },
    {
      id: "theme1@mochi.test",
      name: "Test theme",
      creator: { name: "Artist", url: "http://example.com/artist" },
      description: "A nice tree",
      type: "theme",
      screenshots: [
        {
          url: "http://example.com/preview-wide.png",
          width: 760,
          height: 92,
        },
        {
          url: "http://example.com/preview.png",
          width: 680,
          height: 92,
        },
      ],
    },
  ]);

  promptService = mockPromptService();
});

add_task(async function testOpenDetailView() {
  Services.telemetry.clearEvents();
  let id = "test@mochi.test";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "Test",
      applications: { gecko: { id } },
    },
    useAddonManager: "temporary",
  });
  let id2 = "test2@mochi.test";
  let extension2 = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "Test",
      applications: { gecko: { id: id2 } },
    },
    useAddonManager: "temporary",
  });

  await extension.startup();
  await extension2.startup();

  const goBack = async win => {
    let loaded = waitForViewLoad(win);
    win.managerWindow.document.getElementById("go-back").click();
    await loaded;
  };

  let win = await loadInitialView("extension");
  let doc = win.document;

  // Test click on card to open details.
  let card = getAddonCard(doc, id);
  ok(!card.querySelector("addon-details"), "The card doesn't have details");
  let loaded = waitForViewLoad(win);
  EventUtils.synthesizeMouseAtCenter(card, { clickCount: 1 }, win);
  await loaded;

  card = getAddonCard(doc, id);
  ok(card.querySelector("addon-details"), "The card now has details");

  await goBack(win);

  // Test using more options menu.
  card = getAddonCard(doc, id);
  loaded = waitForViewLoad(win);
  card.querySelector('[action="expand"]').click();
  await loaded;

  card = getAddonCard(doc, id);
  ok(card.querySelector("addon-details"), "The card now has details");

  await goBack(win);

  card = getAddonCard(doc, id2);
  loaded = waitForViewLoad(win);
  card.querySelector('[action="expand"]').click();
  await loaded;

  await closeView(win);
  await extension.unload();
  await extension2.unload();

  assertAboutAddonsTelemetryEvents([
    ["addonsManager", "view", "aboutAddons", "list", { type: "extension" }],
    [
      "addonsManager",
      "view",
      "aboutAddons",
      "detail",
      { type: "extension", addonId: id },
    ],
    ["addonsManager", "view", "aboutAddons", "list", { type: "extension" }],
    [
      "addonsManager",
      "view",
      "aboutAddons",
      "detail",
      { type: "extension", addonId: id },
    ],
    ["addonsManager", "view", "aboutAddons", "list", { type: "extension" }],
    [
      "addonsManager",
      "view",
      "aboutAddons",
      "detail",
      { type: "extension", addonId: id2 },
    ],
  ]);
});

add_task(async function testDetailOperations() {
  Services.telemetry.clearEvents();
  let id = "test@mochi.test";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "Test",
      applications: { gecko: { id } },
    },
    useAddonManager: "temporary",
  });

  await extension.startup();

  let win = await loadInitialView("extension");
  let doc = win.document;

  let card = getAddonCard(doc, id);
  ok(!card.querySelector("addon-details"), "The card doesn't have details");
  let loaded = waitForViewLoad(win);
  EventUtils.synthesizeMouseAtCenter(card, { clickCount: 1 }, win);
  await loaded;

  card = getAddonCard(doc, id);
  let panel = card.querySelector("panel-list");

  // Check button visibility.
  let disableButton = panel.querySelector('[action="toggle-disabled"]');
  ok(!disableButton.hidden, "The disable button is visible");

  let removeButton = panel.querySelector('[action="remove"]');
  ok(!removeButton.hidden, "The remove button is visible");

  let separator = panel.querySelector("panel-item-separator:last-of-type");
  ok(separator.hidden, "The separator is hidden");

  let expandButton = panel.querySelector('[action="expand"]');
  ok(expandButton.hidden, "The expand button is hidden");

  // Check toggling disabled.
  let name = card.querySelector(".addon-name");
  is(name.textContent, "Test", "The name is set when enabled");
  is(doc.l10n.getAttributes(name).id, null, "There is no l10n name");

  // Disable the extension.
  let disableToggled = BrowserTestUtils.waitForEvent(card, "update");
  disableButton.click();
  await disableToggled;

  // The (disabled) text should be shown now.
  Assert.deepEqual(
    doc.l10n.getAttributes(name),
    { id: "addon-name-disabled", args: { name: "Test" } },
    "The name is updated to the disabled text"
  );

  // Enable the add-on.
  let extensionStarted = AddonTestUtils.promiseWebExtensionStartup(id);
  disableToggled = BrowserTestUtils.waitForEvent(card, "update");
  disableButton.click();
  await Promise.all([disableToggled, extensionStarted]);

  // Name is just the add-on name again.
  is(name.textContent, "Test", "The name is reset when enabled");
  is(doc.l10n.getAttributes(name).id, null, "There is no l10n name");

  // Remove but cancel.
  let cancelled = BrowserTestUtils.waitForEvent(card, "remove-cancelled");
  removeButton.click();
  await cancelled;

  // Remove the extension.
  let viewChanged = waitForViewLoad(win);
  // Tell the mock prompt service that the prompt was accepted.
  promptService._response = 0;
  removeButton.click();
  await viewChanged;

  // We're on the list view now and there's no card for this extension.
  const addonList = doc.querySelector("addon-list");
  ok(addonList, "There's an addon-list now");
  ok(!getAddonCard(doc, id), "The extension no longer has a card");
  let addon = await AddonManager.getAddonByID(id);
  ok(
    addon && !!(addon.pendingOperations & AddonManager.PENDING_UNINSTALL),
    "The addon is pending uninstall"
  );

  // Ensure that a pending uninstall bar has been created for the
  // pending uninstall extension, and pressing the undo button will
  // refresh the list and render a card to the re-enabled extension.
  assertHasPendingUninstalls(addonList, 1);
  assertHasPendingUninstallAddon(addonList, addon);

  extensionStarted = AddonTestUtils.promiseWebExtensionStartup(addon.id);
  await testUndoPendingUninstall(addonList, addon);
  info("Wait for the pending uninstall addon complete restart");
  await extensionStarted;

  card = getAddonCard(doc, addon.id);
  ok(card, "Addon card rendered after clicking pending uninstall undo button");

  await closeView(win);
  await extension.unload();

  assertAboutAddonsTelemetryEvents([
    ["addonsManager", "view", "aboutAddons", "list", { type: "extension" }],
    [
      "addonsManager",
      "view",
      "aboutAddons",
      "detail",
      { type: "extension", addonId: id },
    ],
    [
      "addonsManager",
      "action",
      "aboutAddons",
      null,
      { type: "extension", addonId: id, action: "disable", view: "detail" },
    ],
    [
      "addonsManager",
      "action",
      "aboutAddons",
      null,
      { type: "extension", addonId: id, action: "enable" },
    ],
    [
      "addonsManager",
      "action",
      "aboutAddons",
      "cancelled",
      { type: "extension", addonId: id, action: "uninstall", view: "detail" },
    ],
    [
      "addonsManager",
      "action",
      "aboutAddons",
      "accepted",
      { type: "extension", addonId: id, action: "uninstall", view: "detail" },
    ],
    ["addonsManager", "view", "aboutAddons", "list", { type: "extension" }],
    [
      "addonsManager",
      "action",
      "aboutAddons",
      null,
      { type: "extension", addonId: id, action: "undo", view: "list" },
    ],
  ]);
});

add_task(async function testFullDetails() {
  Services.telemetry.clearEvents();
  let id = "addon1@mochi.test";
  let win = await loadInitialView("extension");
  let doc = win.document;

  // The list card.
  let card = getAddonCard(doc, id);
  ok(!card.hasAttribute("expanded"), "The list card is not expanded");

  // Make sure the preview is hidden.
  let preview = card.querySelector(".card-heading-image");
  ok(preview, "There is a preview");
  is(preview.hidden, true, "The preview is hidden");

  let loaded = waitForViewLoad(win);
  card.querySelector('[action="expand"]').click();
  await loaded;

  // This is now the detail card.
  card = getAddonCard(doc, id);
  ok(card.hasAttribute("expanded"), "The detail card is expanded");

  // Make sure the preview is hidden.
  preview = card.querySelector(".card-heading-image");
  ok(preview, "There is a preview");
  is(preview.hidden, true, "The preview is hidden");

  let details = card.querySelector("addon-details");

  // Check all the deck buttons are hidden.
  assertDeckHeadingButtons(details.tabGroup, ["details", "permissions"]);

  let desc = details.querySelector(".addon-detail-description");
  is(
    desc.innerHTML,
    "Longer description<br>With brs!",
    "The full description replaces newlines with <br>"
  );

  let contrib = details.querySelector(".addon-detail-contribute");
  ok(contrib, "The contribution section is visible");

  let waitForTab = BrowserTestUtils.waitForNewTab(
    gBrowser,
    "http://localhost/contribute"
  );
  contrib.querySelector("button").click();
  BrowserTestUtils.removeTab(await waitForTab);

  let rows = getDetailRows(card);

  // Auto updates.
  let row = rows.shift();
  checkLabel(row, "updates");
  let expectedOptions = [
    { value: "1", label: "addon-detail-updates-radio-default", checked: false },
    { value: "2", label: "addon-detail-updates-radio-on", checked: true },
    { value: "0", label: "addon-detail-updates-radio-off", checked: false },
  ];
  let options = row.lastElementChild.querySelectorAll("label");
  checkOptions(doc, options, expectedOptions);

  // Private browsing, functionality checked in another test.
  row = rows.shift();
  checkLabel(row, "private-browsing");

  // Private browsing help text.
  row = rows.shift();
  ok(row.classList.contains("addon-detail-help-row"), "There's a help row");
  ok(!row.hidden, "The help row is shown");
  is(
    doc.l10n.getAttributes(row).id,
    "addon-detail-private-browsing-help",
    "The help row is for private browsing"
  );

  // Author.
  row = rows.shift();
  checkLabel(row, "author");
  let link = row.querySelector("a");
  checkLink(link, "http://example.com/me", "The creator");

  // Version.
  row = rows.shift();
  checkLabel(row, "version");
  let text = row.lastChild;
  is(text.textContent, "3.1", "The version is set");

  // Last updated.
  row = rows.shift();
  checkLabel(row, "last-updated");
  text = row.lastChild;
  is(text.textContent, "March 7, 2019", "The last updated date is set");

  // Homepage.
  row = rows.shift();
  checkLabel(row, "homepage");
  link = row.querySelector("a");
  checkLink(link, "http://example.com/addon1");

  // Reviews.
  row = rows.shift();
  checkLabel(row, "rating");
  let rating = row.lastElementChild;
  ok(rating.classList.contains("addon-detail-rating"), "Found the rating el");
  let starsElem = rating.querySelector("five-star-rating");
  is(starsElem.rating, 4.279, "Exact rating used for calculations");
  let stars = Array.from(starsElem.shadowRoot.querySelectorAll(".rating-star"));
  let fullAttrs = stars.map(star => star.getAttribute("fill")).join(",");
  is(fullAttrs, "full,full,full,full,half", "Four and a half stars are full");
  link = rating.querySelector("a");
  checkLink(link, "http://example.com/reviews", {
    id: "addon-detail-reviews-link",
    args: { numberOfReviews: 5 },
  });

  // While we are here, let's test edge cases of star ratings.
  async function testRating(rating, ratingRounded, expectation) {
    starsElem.rating = rating;
    await starsElem.ownerDocument.l10n.translateElements([starsElem]);
    is(
      starsElem.ratingBuckets.join(","),
      expectation,
      `Rendering of rating ${rating}`
    );

    is(
      starsElem.title,
      `Rated ${ratingRounded} out of 5`,
      "Rendered title must contain at most one fractional digit"
    );
  }
  await testRating(0.0, "0", "empty,empty,empty,empty,empty");
  await testRating(0.123, "0.1", "empty,empty,empty,empty,empty");
  await testRating(0.249, "0.2", "empty,empty,empty,empty,empty");
  await testRating(0.25, "0.3", "half,empty,empty,empty,empty");
  await testRating(0.749, "0.7", "half,empty,empty,empty,empty");
  await testRating(0.75, "0.8", "full,empty,empty,empty,empty");
  await testRating(1.0, "1", "full,empty,empty,empty,empty");
  await testRating(4.249, "4.2", "full,full,full,full,empty");
  await testRating(4.25, "4.3", "full,full,full,full,half");
  await testRating(4.749, "4.7", "full,full,full,full,half");
  await testRating(5.0, "5", "full,full,full,full,full");

  // That should've been all the rows.
  is(rows.length, 0, "There are no more rows left");

  await closeView(win);

  assertAboutAddonsTelemetryEvents([
    ["addonsManager", "view", "aboutAddons", "list", { type: "extension" }],
    [
      "addonsManager",
      "view",
      "aboutAddons",
      "detail",
      { type: "extension", addonId: id },
    ],
    [
      "addonsManager",
      "action",
      "aboutAddons",
      null,
      { type: "extension", addonId: id, action: "contribute", view: "detail" },
    ],
  ]);
});

add_task(async function testMinimalExtension() {
  let win = await loadInitialView("extension");
  let doc = win.document;

  let card = getAddonCard(doc, "addon2@mochi.test");
  ok(!card.hasAttribute("expanded"), "The list card is not expanded");
  let loaded = waitForViewLoad(win);
  card.querySelector('[action="expand"]').click();
  await loaded;

  card = getAddonCard(doc, "addon2@mochi.test");
  let details = card.querySelector("addon-details");

  // Check all the deck buttons are hidden.
  assertDeckHeadingButtons(details.tabGroup, ["details", "permissions"]);

  let desc = details.querySelector(".addon-detail-description");
  is(desc.textContent, "", "There is no full description");

  let contrib = details.querySelector(".addon-detail-contribute");
  ok(contrib.hidden, "The contribution element is hidden");

  let rows = getDetailRows(card);

  // Automatic updates.
  let row = rows.shift();
  checkLabel(row, "updates");

  // Private browsing settings.
  row = rows.shift();
  checkLabel(row, "private-browsing");

  // Private browsing help text.
  row = rows.shift();
  ok(row.classList.contains("addon-detail-help-row"), "There's a help row");
  ok(!row.hidden, "The help row is shown");
  is(
    doc.l10n.getAttributes(row).id,
    "addon-detail-private-browsing-help",
    "The help row is for private browsing"
  );

  // Author.
  row = rows.shift();
  checkLabel(row, "author");
  let text = row.lastChild;
  is(text.textContent, "I made it", "The author is set");
  ok(text instanceof Text, "The author is a text node");

  is(rows.length, 0, "There are no more rows");

  await closeView(win);
});

add_task(async function testDefaultTheme() {
  let win = await loadInitialView("theme");
  let doc = win.document;

  // The list card.
  let card = getAddonCard(doc, DEFAULT_THEME_ID);
  ok(!card.hasAttribute("expanded"), "The list card is not expanded");

  // Make sure the preview is hidden.
  let preview = card.querySelector(".card-heading-image");
  ok(preview, "There is a preview");
  is(preview.hidden, true, "The preview is hidden");
  let loaded = waitForViewLoad(win);
  card.querySelector('[action="expand"]').click();
  await loaded;

  card = getAddonCard(doc, DEFAULT_THEME_ID);

  // Make sure the preview is hidden.
  preview = card.querySelector(".card-heading-image");
  ok(preview, "There is a preview");
  is(preview.hidden, true, "The preview is hidden");

  // Check all the deck buttons are hidden.
  assertDeckHeadingHidden(card.details.tabGroup);

  let rows = getDetailRows(card);

  // Author.
  let author = rows.shift();
  checkLabel(author, "author");
  let text = author.lastChild;
  is(text.textContent, "Mozilla", "The author is set");

  // Version.
  let version = rows.shift();
  checkLabel(version, "version");
  is(version.lastChild.textContent, "1.0", "It's always version 1.0");

  // Last updated.
  let lastUpdated = rows.shift();
  checkLabel(lastUpdated, "last-updated");
  ok(lastUpdated.lastChild.textContent, "There is a date set");

  is(rows.length, 0, "There are no more rows");

  await closeView(win);
});

add_task(async function testStaticTheme() {
  let win = await loadInitialView("theme");
  let doc = win.document;

  // The list card.
  let card = getAddonCard(doc, "theme1@mochi.test");
  ok(!card.hasAttribute("expanded"), "The list card is not expanded");

  // Make sure the preview is set.
  let preview = card.querySelector(".card-heading-image");
  ok(preview, "There is a preview");
  is(preview.src, "http://example.com/preview.png", "The preview URL is set");
  is(preview.width, "664", "The width is set");
  is(preview.height, "90", "The height is set");
  is(preview.hidden, false, "The preview is visible");

  // Load the detail view.
  let loaded = waitForViewLoad(win);
  card.querySelector('[action="expand"]').click();
  await loaded;

  card = getAddonCard(doc, "theme1@mochi.test");

  // Make sure the preview is still set.
  preview = card.querySelector(".card-heading-image");
  ok(preview, "There is a preview");
  is(preview.src, "http://example.com/preview.png", "The preview URL is set");
  is(preview.width, "664", "The width is set");
  is(preview.height, "90", "The height is set");
  is(preview.hidden, false, "The preview is visible");

  // Check all the deck buttons are hidden.
  assertDeckHeadingHidden(card.details.tabGroup);

  let rows = getDetailRows(card);

  // Automatic updates.
  let row = rows.shift();
  checkLabel(row, "updates");

  // Author.
  let author = rows.shift();
  checkLabel(author, "author");
  let text = author.lastElementChild;
  is(text.textContent, "Artist", "The author is set");

  is(rows.length, 0, "There was only 1 row");

  await closeView(win);
});

add_task(async function testPrivateBrowsingExtension() {
  Services.telemetry.clearEvents();
  let id = "pb@mochi.test";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "My PB extension",
      applications: { gecko: { id } },
    },
    useAddonManager: "permanent",
  });

  await extension.startup();

  let win = await loadInitialView("extension");
  let doc = win.document;

  // The add-on shouldn't show that it's allowed yet.
  let card = getAddonCard(doc, id);
  let badge = card.querySelector(".addon-badge-private-browsing-allowed");
  ok(badge.hidden, "The PB badge is hidden initially");
  ok(!(await hasPrivateAllowed(id)), "PB is not allowed");

  // Load the detail view.
  let loaded = waitForViewLoad(win);
  card.querySelector('[action="expand"]').click();
  await loaded;

  // The badge is still hidden on the detail view.
  card = getAddonCard(doc, id);
  badge = card.querySelector(".addon-badge-private-browsing-allowed");
  ok(badge.hidden, "The PB badge is hidden on the detail view");
  ok(!(await hasPrivateAllowed(id)), "PB is not allowed");

  let pbRow = card.querySelector(".addon-detail-row-private-browsing");
  let name = card.querySelector(".addon-name");

  // Allow private browsing.
  let [allow, disallow] = pbRow.querySelectorAll("input");
  let updated = BrowserTestUtils.waitForEvent(card, "update");

  // Check that the disabled state isn't shown while reloading the add-on.
  let addonDisabled = AddonTestUtils.promiseAddonEvent("onDisabled");
  allow.click();
  await addonDisabled;
  is(
    doc.l10n.getAttributes(name).id,
    null,
    "The disabled message is not shown for the add-on"
  );

  // Check the PB stuff.
  await updated;
  ok(!badge.hidden, "The PB badge is now shown");
  ok(await hasPrivateAllowed(id), "PB is allowed");
  is(
    doc.l10n.getAttributes(name).id,
    null,
    "The disabled message is not shown for the add-on"
  );

  info("Verify the badge links to the support page");
  let tabOpened = BrowserTestUtils.waitForNewTab(gBrowser, PB_SUMO_URL);
  EventUtils.synthesizeMouseAtCenter(badge, {}, win);
  let tab = await tabOpened;
  BrowserTestUtils.removeTab(tab);

  // Disable the add-on and change the value.
  updated = BrowserTestUtils.waitForEvent(card, "update");
  card.querySelector('[action="toggle-disabled"]').click();
  await updated;

  // It's still allowed in PB.
  ok(!badge.hidden, "The PB badge is shown");
  ok(await hasPrivateAllowed(id), "PB is allowed");

  // Disallow PB.
  updated = BrowserTestUtils.waitForEvent(card, "update");
  disallow.click();
  await updated;

  ok(badge.hidden, "The PB badge is hidden");
  ok(!(await hasPrivateAllowed(id)), "PB is disallowed");

  // Allow PB.
  updated = BrowserTestUtils.waitForEvent(card, "update");
  allow.click();
  await updated;

  ok(!badge.hidden, "The PB badge is hidden");
  ok(await hasPrivateAllowed(id), "PB is disallowed");

  await closeView(win);
  await extension.unload();

  assertAboutAddonsTelemetryEvents([
    ["addonsManager", "view", "aboutAddons", "list", { type: "extension" }],
    [
      "addonsManager",
      "view",
      "aboutAddons",
      "detail",
      { type: "extension", addonId: id },
    ],
    [
      "addonsManager",
      "action",
      "aboutAddons",
      "on",
      {
        type: "extension",
        addonId: id,
        action: "privateBrowsingAllowed",
        view: "detail",
      },
    ],
    [
      "addonsManager",
      "action",
      "aboutAddons",
      null,
      { type: "extension", addonId: id, action: "disable" },
    ],
    [
      "addonsManager",
      "action",
      "aboutAddons",
      "off",
      {
        type: "extension",
        addonId: id,
        action: "privateBrowsingAllowed",
        view: "detail",
      },
    ],
    [
      "addonsManager",
      "action",
      "aboutAddons",
      "on",
      {
        type: "extension",
        addonId: id,
        action: "privateBrowsingAllowed",
        view: "detail",
      },
    ],
  ]);
});

add_task(async function testInvalidExtension() {
  let win = await open_manager("addons://detail/foo");
  let categoryUtils = new CategoryUtilities(win);
  is(
    categoryUtils.selectedCategory,
    "discover",
    "Should fall back to the discovery pane"
  );

  ok(!gBrowser.canGoBack, "The view has been replaced");

  await close_manager(win);
});

add_task(async function testInvalidExtensionNoDiscover() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.getAddons.showPane", false]],
  });

  let win = await open_manager("addons://detail/foo");
  let categoryUtils = new CategoryUtilities(win);
  is(
    categoryUtils.selectedCategory,
    "extension",
    "Should fall back to the extension list if discover is disabled"
  );

  ok(!gBrowser.canGoBack, "The view has been replaced");

  await close_manager(win);
  await SpecialPowers.popPrefEnv();
});

add_task(async function testExternalUninstall() {
  let id = "remove@mochi.test";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "Remove me",
      applications: { gecko: { id } },
    },
    useAddonManager: "temporary",
  });
  await extension.startup();
  let addon = await AddonManager.getAddonByID(id);

  let win = await loadInitialView("extension");
  let doc = win.document;

  // Load the detail view.
  let card = doc.querySelector(`addon-card[addon-id="${id}"]`);
  let detailsLoaded = waitForViewLoad(win);
  card.querySelector('[action="expand"]').click();
  await detailsLoaded;

  // Uninstall the add-on with undo. Should go to extension list.
  let listLoaded = waitForViewLoad(win);
  await addon.uninstall(true);
  await listLoaded;

  // Verify the list view was loaded and the card is gone.
  let list = doc.querySelector("addon-list");
  is(list.type, "extension", "We're on the extension list page");
  card = list.querySelector(`addon-card[addon-id="${id}"]`);
  ok(!card, "The card has been removed");

  await extension.unload();
  closeView(win);
});

add_task(async function testExternalThemeUninstall() {
  let id = "remove-theme@mochi.test";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: { gecko: { id } },
      name: "Remove theme",
      theme: {},
    },
    useAddonManager: "temporary",
  });
  await extension.startup();
  let addon = await AddonManager.getAddonByID(id);

  let win = await loadInitialView("theme");
  let doc = win.document;

  // Load the detail view.
  let card = doc.querySelector(`addon-card[addon-id="${id}"]`);
  let detailsLoaded = waitForViewLoad(win);
  card.querySelector('[action="expand"]').click();
  await detailsLoaded;

  // Uninstall the add-on without undo. Should go to theme list.
  let listLoaded = waitForViewLoad(win);
  await addon.uninstall();
  await listLoaded;

  // Verify the list view was loaded and the card is gone.
  let list = doc.querySelector("addon-list");
  is(list.type, "theme", "We're on the theme list page");
  card = list.querySelector(`addon-card[addon-id="${id}"]`);
  ok(!card, "The card has been removed");

  await extension.unload();
  closeView(win);
});

add_task(async function testPrivateBrowsingAllowedListView() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "Allowed PB extension",
      applications: { gecko: { id: "allowed@mochi.test" } },
    },
    useAddonManager: "permanent",
  });

  await extension.startup();
  let perms = { permissions: ["internal:privateBrowsingAllowed"], origins: [] };
  await ExtensionPermissions.add("allowed@mochi.test", perms);
  let addon = await AddonManager.getAddonByID("allowed@mochi.test");
  await addon.reload();

  let win = await loadInitialView("extension");
  let doc = win.document;

  // The allowed extension should have a badge on load.
  let card = getAddonCard(doc, "allowed@mochi.test");
  let badge = card.querySelector(".addon-badge-private-browsing-allowed");
  ok(!badge.hidden, "The PB badge is shown for the allowed add-on");

  await extension.unload();
  await closeView(win);
});

add_task(async function testPermissions() {
  async function runTest(id, permissions) {
    let win = await loadInitialView("extension");
    let doc = win.document;

    let card = getAddonCard(doc, id);
    ok(!card.hasAttribute("expanded"), "The list card is not expanded");
    let loaded = waitForViewLoad(win);
    card.querySelector('[action="expand"]').click();
    await loaded;

    card = getAddonCard(doc, id);
    let { deck, tabGroup } = card.details;

    // Check all the deck buttons are hidden.
    assertDeckHeadingButtons(tabGroup, ["details", "permissions"]);

    let permsBtn = tabGroup.querySelector('[name="permissions"]');
    let permsShown = BrowserTestUtils.waitForEvent(deck, "view-changed");
    permsBtn.click();
    await permsShown;

    let permsSection = card.querySelector("addon-permissions-list");
    let rows = Array.from(permsSection.querySelectorAll(".addon-detail-row"));

    info("Check displayed permissions");
    if (permissions) {
      for (let name in permissions) {
        // Check the permission-info class to make sure it's for a permission.
        let row = rows.shift();
        ok(
          row.classList.contains("permission-info"),
          `There's a row for ${name}`
        );
      }
    } else {
      let row = rows.shift();
      is(
        doc.l10n.getAttributes(row).id,
        "addon-permissions-empty",
        "There's a message when no permissions are shown"
      );
    }

    info("Check learn more link");
    let row = rows.shift();
    is(row.children.length, 1, "There's one child for learn more");
    let link = row.firstElementChild;
    let rootUrl = Services.urlFormatter.formatURLPref("app.support.baseURL");
    let url = rootUrl + "extension-permissions";
    is(link.href, url, "The URL is set");
    is(link.getAttribute("target"), "_blank", "The link opens in a new tab");

    await closeView(win);
  }

  info("Check permissions for add-on with permission message");
  await runTest("addon1@mochi.test", ["<all_urls>", "tabs", "webNavigation"]);

  info("Check permissions for add-on without permission messages");
  await runTest("addon2@mochi.test");
});

// When the back button is used, its disabled state will be updated. If it
// isn't updated when showing a view, then it will be disabled on the next
// use (bug 1551213) if the last use caused it to become disabled.
add_task(async function testGoBackButton() {
  // Make sure the list view is the first loaded view so you cannot go back.
  Services.prefs.setCharPref(PREF_UI_LASTCATEGORY, "addons://list/extension");

  let id = "addon1@mochi.test";
  let win = await loadInitialView("extension");
  let doc = win.document;
  let backButton = win.managerWindow.document.getElementById("go-back");

  let loadDetailView = () => {
    let loaded = waitForViewLoad(win);
    getAddonCard(doc, id)
      .querySelector("[action=expand]")
      .click();
    return loaded;
  };

  let checkBackButtonState = () => {
    is_element_visible(backButton, "Back button is visible on the detail page");
    ok(!backButton.disabled, "Back button is enabled");
  };

  // Load the detail view, first time should be fine.
  await loadDetailView();
  checkBackButtonState();

  // Use the back button directly to pop from history and trigger its disabled
  // state to be updated.
  let loaded = waitForViewLoad(win);
  backButton.click();
  await loaded;

  await loadDetailView();
  checkBackButtonState();

  await closeView(win);
});

add_task(async function testEmptyMoreOptionsMenu() {
  let theme = await AddonManager.getAddonByID(DEFAULT_THEME_ID);
  ok(theme.isActive, "The default theme is enabled");

  let win = await loadInitialView("theme");
  let doc = win.document;

  let card = getAddonCard(doc, DEFAULT_THEME_ID);
  let enabledItems = card.options.visibleItems;
  is(enabledItems.length, 1, "There is one enabled item");
  is(enabledItems[0].getAttribute("action"), "expand", "Expand is enabled");
  let moreOptionsButton = card.querySelector(".more-options-button");
  ok(!moreOptionsButton.hidden, "The more options button is visible");

  let loaded = waitForViewLoad(win);
  enabledItems[0].click();
  await loaded;

  card = getAddonCard(doc, DEFAULT_THEME_ID);
  enabledItems = card.options.visibleItems;
  is(enabledItems.length, 0, "There are no enabled items");
  moreOptionsButton = card.querySelector(".more-options-button");
  ok(moreOptionsButton.hidden, "The more options button is now hidden");

  // Switch themes, this should show the menu again.
  let darkTheme = await AddonManager.getAddonByID(DARK_THEME_ID);
  let updated = BrowserTestUtils.waitForEvent(card, "update");
  await darkTheme.enable();
  await updated;

  enabledItems = card.options.visibleItems;
  is(enabledItems.length, 1, "There is one item visible");
  is(
    enabledItems[0].getAttribute("action"),
    "toggle-disabled",
    "Enable is the item"
  );
  ok(!moreOptionsButton.hidden, "The more options button is now visible");

  updated = BrowserTestUtils.waitForEvent(card, "update");
  await enabledItems[0].click();
  await updated;

  enabledItems = card.options.visibleItems;
  is(enabledItems.length, 0, "There are no items visible");
  ok(moreOptionsButton.hidden, "The more options button is hidden again");

  await closeView(win);
});
