/* eslint max-len: ["error", 80] */

const {AddonTestUtils} =
  ChromeUtils.import("resource://testing-common/AddonTestUtils.jsm", {});

const {ExtensionPermissions} =
  ChromeUtils.import("resource://gre/modules/ExtensionPermissions.jsm", {});

let gProvider;
let promptService;

AddonTestUtils.initMochitest(this);

function getAddonCard(doc, addonId) {
  return doc.querySelector(`addon-card[addon-id="${addonId}"]`);
}

function checkLabel(row, name) {
  let id;
  if (name == "private-browsing") {
    // This id is carried over from the old about:addons.
    id = "detail-private-browsing-label";
  } else {
    id = `addon-detail-${name}-label`;
  }
  is(row.ownerDocument.l10n.getAttributes(row.querySelector("label")).id,
    id, `The ${name} label is set`);
}

function checkLink(link, url, text = url) {
  ok(link, "There is a link");
  is(link.href, url, "The link goes to the URL");
  if (text instanceof Object) {
    // Check the fluent data.
    Assert.deepEqual(
      link.ownerDocument.l10n.getAttributes(link),
      text, "The fluent data is set correctly");
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
      doc.l10n.getAttributes(text), {id: expected.label},
      "The label has the right text");
  }
}

async function hasPrivateAllowed(id) {
  let perms = await ExtensionPermissions.get(id);
  return perms.permissions.includes("internal:privateBrowsingAllowed");
}

add_task(async function enableHtmlViews() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.htmlaboutaddons.enabled", true],
          ["extensions.allowPrivateBrowsingByDefault", false]],
  });

  gProvider = new MockProvider();
  gProvider.createAddons([{
    id: "addon1@mochi.test",
    name: "Test add-on 1",
    creator: {name: "The creator", url: "http://example.com/me"},
    version: "3.1",
    description: "Short description",
    fullDescription: "Longer description\nWith brs!",
    type: "extension",
    contributionURL: "http://foo.com",
    averageRating: 4.279,
    reviewCount: 5,
    reviewURL: "http://example.com/reviews",
    homepageURL: "http://example.com/addon1",
    updateDate: new Date("2019-03-07T01:00:00"),
    applyBackgroundUpdates: AddonManager.AUTOUPDATE_ENABLE,
  }, {
    id: "addon2@mochi.test",
    name: "Test add-on 2",
    creator: {name: "I made it"},
    description: "Short description",
    type: "extension",
  }, {
    id: "theme1@mochi.test",
    name: "Test theme",
    creator: {name: "Artist", url: "http://example.com/artist"},
    description: "A nice tree",
    type: "theme",
    screenshots: [{
      url: "http://example.com/preview-wide.png",
      width: 760,
      height: 92,
    }, {
      url: "http://example.com/preview.png",
      width: 680,
      height: 92,
    }],
  }]);

  promptService = mockPromptService();
});

add_task(async function testOpenDetailView() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "Test",
      applications: {gecko: {id: "test@mochi.test"}},
    },
    useAddonManager: "temporary",
  });

  await extension.startup();

  let win = await loadInitialView("extension");
  let doc = win.document;

  // Test double click on card to open details.
  let card = getAddonCard(doc, "test@mochi.test");
  ok(!card.querySelector("addon-details"), "The card doesn't have details");
  let loaded = waitForViewLoad(win);
  EventUtils.synthesizeMouseAtCenter(card, {clickCount: 1}, win);
  EventUtils.synthesizeMouseAtCenter(card, {clickCount: 2}, win);
  await loaded;

  card = getAddonCard(doc, "test@mochi.test");
  ok(card.querySelector("addon-details"), "The card now has details");

  loaded = waitForViewLoad(win);
  win.managerWindow.document.getElementById("go-back").click();
  await loaded;

  // Test using more options menu.
  card = getAddonCard(doc, "test@mochi.test");
  loaded = waitForViewLoad(win);
  card.querySelector('[action="expand"]').click();
  await loaded;

  card = getAddonCard(doc, "test@mochi.test");
  ok(card.querySelector("addon-details"), "The card now has details");

  await closeView(win);
  await extension.unload();
});

add_task(async function testDetailOperations() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "Test",
      applications: {gecko: {id: "test@mochi.test"}},
    },
    useAddonManager: "temporary",
  });

  await extension.startup();

  let win = await loadInitialView("extension");
  let doc = win.document;

  let card = getAddonCard(doc, "test@mochi.test");
  ok(!card.querySelector("addon-details"), "The card doesn't have details");
  let loaded = waitForViewLoad(win);
  EventUtils.synthesizeMouseAtCenter(card, {clickCount: 1}, win);
  EventUtils.synthesizeMouseAtCenter(card, {clickCount: 2}, win);
  await loaded;

  card = getAddonCard(doc, "test@mochi.test");
  let panel = card.querySelector("panel-list");

  // Check button visibility.
  let disableButton = panel.querySelector('[action="toggle-disabled"]');
  ok(!disableButton.hidden, "The disable button is visible");

  let removeButton = panel.querySelector('[action="remove"]');
  ok(!removeButton.hidden, "The remove button is visible");

  let separator = panel.querySelector("panel-item-separator");
  ok(separator.hidden, "The separator is hidden");

  let expandButton = panel.querySelector('[action="expand"]');
  ok(expandButton.hidden, "The expand button is hidden");

  // Check toggling disabled.
  let name = card.querySelector(".addon-name");
  is(name.textContent, "Test", "The name is set when enabled");
  is(doc.l10n.getAttributes(name).id, "", "There is no l10n name");

  // Disable the extension.
  let disableToggled = BrowserTestUtils.waitForEvent(card, "update");
  disableButton.click();
  await disableToggled;

  // The (disabled) text should be shown now.
  Assert.deepEqual(
    doc.l10n.getAttributes(name),
    {id: "addon-name-disabled", args: {name: "Test"}},
    "The name is updated to the disabled text");

  // Enable the add-on.
  let extensionStarted = AddonTestUtils.promiseWebExtensionStartup(
    "test@mochi.test");
  disableToggled = BrowserTestUtils.waitForEvent(card, "update");
  disableButton.click();
  await Promise.all([disableToggled, extensionStarted]);

  // Name is just the add-on name again.
  is(name.textContent, "Test", "The name is reset when enabled");
  is(doc.l10n.getAttributes(name).id, "", "There is no l10n name");

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
  ok(!getAddonCard(doc, "test@mochi.test"),
     "The extension no longer has a card");
  let addon = await AddonManager.getAddonByID("test@mochi.test");
  ok(addon && !!(addon.pendingOperations & AddonManager.PENDING_UNINSTALL),
     "The addon is pending uninstall");

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
});

add_task(async function testFullDetails() {
  let win = await loadInitialView("extension");
  let doc = win.document;

  // The list card.
  let card = getAddonCard(doc, "addon1@mochi.test");
  ok(!card.hasAttribute("expanded"), "The list card is not expanded");

  // Make sure the preview is hidden.
  let preview = card.querySelector(".card-heading-image");
  ok(preview, "There is a preview");
  is(preview.hidden, true, "The preview is hidden");

  let loaded = waitForViewLoad(win);
  card.querySelector('[action="expand"]').click();
  await loaded;

  // This is now the detail card.
  card = getAddonCard(doc, "addon1@mochi.test");
  ok(card.hasAttribute("expanded"), "The detail card is expanded");

  // Make sure the preview is hidden.
  preview = card.querySelector(".card-heading-image");
  ok(preview, "There is a preview");
  is(preview.hidden, true, "The preview is hidden");

  let details = card.querySelector("addon-details");
  let desc = details.querySelector(".addon-detail-description");
  is(desc.innerHTML, "Longer description<br>With brs!",
     "The full description replaces newlines with <br>");

  let contrib = details.querySelector(".addon-detail-contribute");
  ok(contrib, "The contribution section is visible");

  let rows = Array.from(details.querySelectorAll(".addon-detail-row"));

  // Auto updates.
  let row = rows.shift();
  checkLabel(row, "updates");
  let expectedOptions = [
    {value: "1", label: "addon-detail-updates-radio-default", checked: false},
    {value: "2", label: "addon-detail-updates-radio-on", checked: true},
    {value: "0", label: "addon-detail-updates-radio-off", checked: false},
  ];
  let options = row.lastElementChild.querySelectorAll("label");
  checkOptions(doc, options, expectedOptions);

  // Private browsing, functionality checked in another test.
  row = rows.shift();
  checkLabel(row, "private-browsing");

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
    args: {numberOfReviews: 5},
  });

  // While we are here, let's test edge cases of star ratings.
  async function testRating(rating, ratingRounded, expectation) {
    starsElem.rating = rating;
    await starsElem.ownerDocument.l10n.translateElements([starsElem]);
    is(starsElem.ratingBuckets.join(","), expectation,
       `Rendering of rating ${rating}`);

    is(starsElem.title, `Rated ${ratingRounded} out of 5`,
       "Rendered title must contain at most one fractional digit");
  }
  await testRating(0.000, "0", "empty,empty,empty,empty,empty");
  await testRating(0.123, "0.1", "empty,empty,empty,empty,empty");
  await testRating(0.249, "0.2", "empty,empty,empty,empty,empty");
  await testRating(0.250, "0.3", "half,empty,empty,empty,empty");
  await testRating(0.749, "0.7", "half,empty,empty,empty,empty");
  await testRating(0.750, "0.8", "full,empty,empty,empty,empty");
  await testRating(1.000, "1", "full,empty,empty,empty,empty");
  await testRating(4.249, "4.2", "full,full,full,full,empty");
  await testRating(4.250, "4.3", "full,full,full,full,half");
  await testRating(4.749, "4.7", "full,full,full,full,half");
  await testRating(5.000, "5", "full,full,full,full,full");

  // That should've been all the rows.
  is(rows.length, 0, "There are no more rows left");

  await closeView(win);
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

  let desc = details.querySelector(".addon-detail-description");
  is(desc.textContent, "", "There is no full description");

  let contrib = details.querySelector(".addon-detail-contribute");
  ok(!contrib, "There is no contribution element");

  let rows = Array.from(details.querySelectorAll(".addon-detail-row"));

  // Automatic updates.
  let row = rows.shift();
  checkLabel(row, "updates");

  row = rows.shift();
  checkLabel(row, "private-browsing");

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
  let card = getAddonCard(doc, "default-theme@mozilla.org");
  ok(!card.hasAttribute("expanded"), "The list card is not expanded");

  // Make sure the preview is hidden.
  let preview = card.querySelector(".card-heading-image");
  ok(preview, "There is a preview");
  is(preview.hidden, true, "The preview is hidden");
  let loaded = waitForViewLoad(win);
  card.querySelector('[action="expand"]').click();
  await loaded;

  card = getAddonCard(doc, "default-theme@mozilla.org");

  // Make sure the preview is hidden.
  preview = card.querySelector(".card-heading-image");
  ok(preview, "There is a preview");
  is(preview.hidden, true, "The preview is hidden");

  let rows = Array.from(card.querySelectorAll(".addon-detail-row"));

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

  let rows = Array.from(card.querySelectorAll(".addon-detail-row"));

  // Automatic updates.
  let row = rows.shift();
  checkLabel(row, "updates");

  // Author.
  let author = rows.shift();
  checkLabel(author, "author");
  let text = author.lastChild;
  is(text.textContent, "Artist", "The author is set");

  is(rows.length, 0, "There was only 1 row");

  await closeView(win);
});

add_task(async function testPrivateBrowsingExtension() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "My PB extension",
      applications: {gecko: {id: "pb@mochi.test"}},
    },
    useAddonManager: "permanent",
  });

  await extension.startup();

  let win = await loadInitialView("extension");
  let doc = win.document;

  // The add-on shouldn't show that it's allowed yet.
  let card = getAddonCard(doc, "pb@mochi.test");
  let badge = card.querySelector(".addon-badge-private-browsing-allowed");
  ok(badge.hidden, "The PB badge is hidden initially");
  ok(!await hasPrivateAllowed("pb@mochi.test"), "PB is not allowed");

  // Load the detail view.
  let loaded = waitForViewLoad(win);
  card.querySelector('[action="expand"]').click();
  await loaded;

  // The badge is still hidden on the detail view.
  card = getAddonCard(doc, "pb@mochi.test");
  badge = card.querySelector(".addon-badge-private-browsing-allowed");
  ok(badge.hidden, "The PB badge is hidden on the detail view");
  ok(!await hasPrivateAllowed("pb@mochi.test"), "PB is not allowed");

  let pbRow = card.querySelector(".addon-detail-row-private-browsing");

  // Allow private browsing.
  let [allow, disallow] = pbRow.querySelectorAll("input");
  let updated = BrowserTestUtils.waitForEvent(card, "update");
  allow.click();
  await updated;
  ok(!badge.hidden, "The PB badge is now shown");
  ok(await hasPrivateAllowed("pb@mochi.test"), "PB is allowed");

  // Disable the add-on and change the value.
  updated = BrowserTestUtils.waitForEvent(card, "update");
  card.querySelector('[action="toggle-disabled"]').click();
  await updated;

  // It's still allowed in PB.
  ok(!badge.hidden, "The PB badge is shown");
  ok(await hasPrivateAllowed("pb@mochi.test"), "PB is allowed");

  // Disallow PB.
  updated = BrowserTestUtils.waitForEvent(card, "update");
  disallow.click();
  await updated;

  ok(badge.hidden, "The PB badge is hidden");
  ok(!await hasPrivateAllowed("pb@mochi.test"), "PB is disallowed");

  // Allow PB.
  updated = BrowserTestUtils.waitForEvent(card, "update");
  allow.click();
  await updated;

  ok(!badge.hidden, "The PB badge is hidden");
  ok(await hasPrivateAllowed("pb@mochi.test"), "PB is disallowed");

  await extension.unload();
  await closeView(win);
});

add_task(async function testPrivateBrowsingAllowedListView() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "Allowed PB extension",
      applications: {gecko: {id: "allowed@mochi.test"}},
    },
    useAddonManager: "permanent",
  });

  await extension.startup();
  let perms = {permissions: ["internal:privateBrowsingAllowed"], origins: []};
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
