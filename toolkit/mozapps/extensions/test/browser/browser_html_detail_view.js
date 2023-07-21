/* eslint max-len: ["error", 80] */

"use strict";

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);

const { ExtensionPermissions } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionPermissions.sys.mjs"
);

const { QuarantinedDomains } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionPermissions.sys.mjs"
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

function getDetailRows(card) {
  return Array.from(
    card.querySelectorAll('[name="details"] .addon-detail-row:not([hidden])')
  );
}

async function checkLabel(row, name) {
  let id;
  if (name == "private-browsing") {
    // This id is carried over from the old about:addons.
    id = "detail-private-browsing-label";
  } else {
    id = `addon-detail-${name}-label`;
  }
  const doc = row.ownerDocument;
  await doc.l10n.translateElements([row]);
  const rowHeaderEl = row.firstElementChild;
  is(doc.l10n.getAttributes(rowHeaderEl).id, id, `The ${name} label is set`);
  if (row.getAttribute("role") === "group") {
    // For the rows on which the role="group" attribute is set,
    // let's make sure that the element itself includes an aria-label
    // which provides to the screen reader a label similar to the one
    // rendered as the visual section header.
    //
    // NOTE: more screen reader accessibility assertions are being
    // covered by the checkRowScreenReaderAccessibility test helper.
    is(
      row.getAttribute("aria-label"),
      rowHeaderEl.textContent,
      "expect an aria-label from role=group row to match row header el text"
    );
    // For these rows we expect rowHeaderEl to be a span.
    is(rowHeaderEl.tagName, "SPAN", "row header element should be a span");
  } else {
    // For the other rows which we have not set a role="group" attribute
    // on, we expect the rowHeaderEl to still be a label.
    is(
      rowHeaderEl.tagName,
      "LABEL",
      "row header element expected to be a label"
    );
  }
}

async function checkRowScreenReaderAccessibility(
  row,
  { groupName, expectedFluentId }
) {
  const doc = row.ownerDocument;
  // Make sure the row isn't missing any strings expected to be associated
  // to the fluent ids (which would make translateElements to reject
  // and the test to fail explicitly).
  await doc.l10n.translateElements([row]);
  is(
    row.getAttribute("role"),
    "group",
    `Expect ${groupName} row to have role group`
  );
  is(
    doc.l10n.getAttributes(row).id,
    expectedFluentId,
    `Got expected fluent id associated to the ${groupName} row`
  );
  // Make sure that screen readers will be able to announce to the
  // user what is the group of controls being entered.
  ok(
    !!row.getAttribute("aria-label")?.length,
    `Expect non empty aria-label on the ${groupName} row`
  );
}

async function checkQuarantinedDomainsUserAllowedRows(card, rows) {
  // Account for the rows related to per-addon quarantineIgnoredByUser UI,
  // underling functionality of the UI is checked in its own test task.
  const doc = card.ownerDocument;
  if (card.addon.canChangeQuarantineIgnored) {
    let row = rows.shift();
    await checkLabel(row, "quarantined-domains");
    await checkRowScreenReaderAccessibility(row, {
      groupName: "quarantined domains exempt controls",
      expectedFluentId: "addon-detail-group-label-quarantined-domains",
    });

    // quarantineIgnoredByUser UI help text.
    row = rows.shift();
    ok(row.classList.contains("addon-detail-help-row"), "There's a help row");
    ok(!row.hidden, "The help row is shown");
    is(
      doc.l10n.getAttributes(row.firstElementChild).id,
      "addon-detail-quarantined-domains-help",
      "The help row is for quarantined domains"
    );
  }
}

function formatUrl(contentAttribute, url) {
  let parsedUrl = new URL(url);
  parsedUrl.searchParams.set("utm_source", "firefox-browser");
  parsedUrl.searchParams.set("utm_medium", "firefox-browser");
  parsedUrl.searchParams.set("utm_content", contentAttribute);
  return parsedUrl.href;
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
  let buttons = group.querySelectorAll(".tab-button");
  for (let button of buttons) {
    ok(button.offsetHeight == 0, `The ${button.name} is hidden`);
  }
}

function assertDeckHeadingButtons(group, visibleButtons) {
  ok(!group.hidden, "The tab group is shown");
  let buttons = group.querySelectorAll(".tab-button");
  ok(
    buttons.length >= visibleButtons.length,
    `There should be at least ${visibleButtons.length} buttons`
  );
  for (let button of buttons) {
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

async function assertBackButtonIsDisabled(win) {
  let backButton = await BrowserTestUtils.waitForCondition(async () => {
    let backButton = win.document.querySelector(".back-button");

    // Wait until the button is visible in the page.
    return backButton && !backButton.hidden ? backButton : false;
  });

  ok(backButton, "back button is rendered");
  ok(backButton.disabled, "back button is disabled");
}

add_setup(async function enableHtmlViews() {
  gProvider = new MockProvider(["extension", "sitepermission"]);
  gProvider.createAddons([
    {
      id: "addon1@mochi.test",
      name: "Test add-on 1",
      creator: { name: "The creator", url: "http://addons.mozilla.org/me" },
      version: "3.1",
      description: "Short description",
      fullDescription: "Longer description\nWith brs!",
      type: "extension",
      contributionURL: "http://example.com/contribute",
      averageRating: 4.279,
      userPermissions: {
        origins: ["<all_urls>", "file://*/*"],
        permissions: ["alarms", "contextMenus", "tabs", "webNavigation"],
      },
      reviewCount: 5,
      reviewURL: "http://addons.mozilla.org/reviews",
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
      id: "addon3@mochi.test",
      name: "Test add-on 3",
      creator: { name: "Look a super long description" },
      description: "Short description",
      fullDescription: "Mozilla\n".repeat(100),
      userPermissions: {
        origins: [],
        permissions: ["alarms", "contextMenus"],
      },
      type: "extension",
      contributionURL: "http://example.com/contribute",
      updateDate: new Date("2022-03-07T01:00:00"),
    },
    {
      id: "addon4@mochi.test",
      name: "Test add-on 4",
      creator: { name: "Some name" },
      description: "Short description",
      userPermissions: {
        origins: [],
        permissions: ["alarms", "contextMenus"],
      },
      type: "extension",
      reviewCount: 0,
      reviewURL: "http://addons.mozilla.org/reviews",
      averageRating: 0,
    },
    {
      // NOTE: Keep the mock properties in sync with the one that
      // SitePermsAddonWrapper would be providing in real synthetic
      // addon entries managed by the SitePermsAddonProvider.
      id: "sitepermission@mochi.test",
      version: "2.0",
      name: "Test site permission add-on",
      description: "permission description",
      fullDescription: "detailed description",
      siteOrigin: "http://mochi.test",
      sitePermissions: ["midi"],
      type: "sitepermission",
      permissions: AddonManager.PERM_CAN_UNINSTALL,
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
  let id = "test@mochi.test";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "Test",
      browser_specific_settings: { gecko: { id } },
    },
    useAddonManager: "temporary",
  });
  let id2 = "test2@mochi.test";
  let extension2 = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "Test",
      browser_specific_settings: { gecko: { id: id2 } },
    },
    useAddonManager: "temporary",
  });

  await extension.startup();
  await extension2.startup();

  const goBack = async win => {
    let loaded = waitForViewLoad(win);
    let backButton = win.document.querySelector(".back-button");
    ok(!backButton.disabled, "back button is enabled");
    backButton.click();
    await loaded;
  };

  let win = await loadInitialView("extension");

  // Test click on card to open details.
  let card = getAddonCard(win, id);
  ok(!card.querySelector("addon-details"), "The card doesn't have details");
  let loaded = waitForViewLoad(win);
  EventUtils.synthesizeMouseAtCenter(card, { clickCount: 1 }, win);
  await loaded;

  card = getAddonCard(win, id);
  ok(card.querySelector("addon-details"), "The card now has details");

  await goBack(win);

  // Test using more options menu.
  card = getAddonCard(win, id);
  loaded = waitForViewLoad(win);
  card.querySelector('[action="expand"]').click();
  await loaded;

  card = getAddonCard(win, id);
  ok(card.querySelector("addon-details"), "The card now has details");

  await goBack(win);

  card = getAddonCard(win, id2);
  loaded = waitForViewLoad(win);
  card.querySelector('[action="expand"]').click();
  await loaded;

  await goBack(win);

  // Test click on add-on name.
  card = getAddonCard(win, id2);
  ok(!card.querySelector("addon-details"), "The card isn't expanded");
  let addonName = card.querySelector(".addon-name");
  loaded = waitForViewLoad(win);
  EventUtils.synthesizeMouseAtCenter(addonName, {}, win);
  await loaded;
  card = getAddonCard(win, id2);
  ok(card.querySelector("addon-details"), "The card is expanded");

  await closeView(win);
  await extension.unload();
  await extension2.unload();
});

add_task(async function testDetailOperations() {
  let id = "test@mochi.test";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "Test",
      browser_specific_settings: { gecko: { id } },
    },
    useAddonManager: "temporary",
  });

  await extension.startup();

  let win = await loadInitialView("extension");
  let doc = win.document;

  let card = getAddonCard(win, id);
  ok(!card.querySelector("addon-details"), "The card doesn't have details");
  let loaded = waitForViewLoad(win);
  EventUtils.synthesizeMouseAtCenter(card, { clickCount: 1 }, win);
  await loaded;

  card = getAddonCard(win, id);
  let panel = card.querySelector("panel-list");

  // Check button visibility.
  let disableButton = card.querySelector('[action="toggle-disabled"]');
  ok(!disableButton.hidden, "The disable button is visible");

  let removeButton = panel.querySelector('[action="remove"]');
  ok(!removeButton.hidden, "The remove button is visible");

  let separator = panel.querySelector("hr:last-of-type");
  ok(separator.hidden, "The separator is hidden");

  let expandButton = panel.querySelector('[action="expand"]');
  ok(expandButton.hidden, "The expand button is hidden");

  // Check toggling disabled.
  let name = card.addonNameEl;
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
  ok(!getAddonCard(win, id), "The extension no longer has a card");
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

  card = getAddonCard(win, addon.id);
  ok(card, "Addon card rendered after clicking pending uninstall undo button");

  await closeView(win);
  await extension.unload();
});

add_task(async function testFullDetails() {
  let id = "addon1@mochi.test";
  let headingId = "addon1_mochi_test-heading";
  let win = await loadInitialView("extension");
  let doc = win.document;

  // The list card.
  let card = getAddonCard(win, id);
  ok(!card.hasAttribute("expanded"), "The list card is not expanded");

  // Make sure the preview is hidden.
  let preview = card.querySelector(".card-heading-image");
  ok(preview, "There is a preview");
  is(preview.hidden, true, "The preview is hidden");

  let loaded = waitForViewLoad(win);
  card.querySelector('[action="expand"]').click();
  await loaded;

  // This is now the detail card.
  card = getAddonCard(win, id);
  ok(card.hasAttribute("expanded"), "The detail card is expanded");

  let cardHeading = card.querySelector("h1");
  is(cardHeading.textContent, "Test add-on 1", "Card heading is set");
  is(cardHeading.id, headingId, "Heading has correct id");
  is(
    card.querySelector(".card").getAttribute("aria-labelledby"),
    headingId,
    "Card is labelled by the heading"
  );

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

  let sitepermissionsRow = details.querySelector(
    ".addon-detail-sitepermissions"
  );
  is(
    sitepermissionsRow.hidden,
    true,
    "AddonSitePermissionsList should be hidden for this addon type"
  );

  // Check the show more button is not there
  const showMoreBtn = card.querySelector(".addon-detail-description-toggle");
  ok(showMoreBtn.hidden, "The show more button is not visible");

  let contrib = details.querySelector(".addon-detail-contribute");
  ok(contrib, "The contribution section is visible");

  let waitForTab = BrowserTestUtils.waitForNewTab(
    gBrowser,
    "http://example.com/contribute"
  );
  contrib.querySelector("button").click();
  BrowserTestUtils.removeTab(await waitForTab);

  let rows = getDetailRows(card);

  // Auto updates.
  let row = rows.shift();

  await checkLabel(row, "updates");
  await checkRowScreenReaderAccessibility(row, {
    groupName: "updates controls",
    expectedFluentId: "addon-detail-group-label-updates",
  });

  let expectedOptions = [
    { value: "1", label: "addon-detail-updates-radio-default", checked: false },
    { value: "2", label: "addon-detail-updates-radio-on", checked: true },
    { value: "0", label: "addon-detail-updates-radio-off", checked: false },
  ];
  let options = row.lastElementChild.querySelectorAll("label");
  checkOptions(doc, options, expectedOptions);

  // Private browsing, functionality checked in another test.
  row = rows.shift();
  await checkLabel(row, "private-browsing");
  await checkRowScreenReaderAccessibility(row, {
    groupName: "private browsing controls",
    expectedFluentId: "addon-detail-group-label-private-browsing",
  });

  // Private browsing help text.
  row = rows.shift();
  ok(row.classList.contains("addon-detail-help-row"), "There's a help row");
  ok(!row.hidden, "The help row is shown");
  is(
    doc.l10n.getAttributes(row).id,
    "addon-detail-private-browsing-help",
    "The help row is for private browsing"
  );

  await checkQuarantinedDomainsUserAllowedRows(card, rows);

  // Author.
  row = rows.shift();
  await checkLabel(row, "author");
  let link = row.querySelector("a");
  let authorLink = formatUrl(
    "addons-manager-user-profile-link",
    "http://addons.mozilla.org/me"
  );
  checkLink(link, authorLink, "The creator");

  // Version.
  row = rows.shift();
  await checkLabel(row, "version");
  let text = row.lastChild;
  is(text.textContent, "3.1", "The version is set");

  // Last updated.
  row = rows.shift();
  await checkLabel(row, "last-updated");
  text = row.lastChild;
  is(text.textContent, "March 7, 2019", "The last updated date is set");

  // Homepage.
  row = rows.shift();
  await checkLabel(row, "homepage");
  link = row.querySelector("a");
  checkLink(link, "http://example.com/addon1");

  // Reviews.
  row = rows.shift();
  await checkLabel(row, "rating");
  let rating = row.lastElementChild;
  ok(rating.classList.contains("addon-detail-rating"), "Found the rating el");
  let mozFiveStar = rating.querySelector("moz-five-star");
  is(mozFiveStar.rating, 4.279, "Exact rating used for calculations");
  let stars = Array.from(mozFiveStar.starEls);
  let fullAttrs = stars.map(star => star.getAttribute("fill")).join(",");
  is(fullAttrs, "full,full,full,full,half", "Four and a half stars are full");
  link = rating.querySelector("a");
  let reviewsLink = formatUrl(
    "addons-manager-reviews-link",
    "http://addons.mozilla.org/reviews"
  );
  checkLink(link, reviewsLink, {
    id: "addon-detail-reviews-link",
    args: { numberOfReviews: 5 },
  });

  // While we are here, let's test edge cases of star ratings.
  async function testRating(rating, ratingRounded, expectation) {
    mozFiveStar.rating = rating;
    await mozFiveStar.updateComplete;
    if (mozFiveStar.ownerDocument.hasPendingL10nMutations) {
      await BrowserTestUtils.waitForEvent(
        mozFiveStar.ownerDocument,
        "L10nMutationsFinished"
      );
    }
    let starsString = Array.from(mozFiveStar.starEls)
      .map(star => star.getAttribute("fill"))
      .join(",");
    is(starsString, expectation, `Rendering of rating ${rating}`);

    is(
      mozFiveStar.starsWrapperEl.title,
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
});

add_task(async function testFullDetailsShowMoreButton() {
  const id = "addon3@mochi.test";
  const win = await loadInitialView("extension");

  // The list card.
  let card = getAddonCard(win, id);
  const loaded = waitForViewLoad(win);
  card.querySelector('[action="expand"]').click();
  await loaded;

  // This is now the detail card.
  card = getAddonCard(win, id);

  // Check the show more button is there
  const showMoreBtn = card.querySelector(".addon-detail-description-toggle");
  ok(!showMoreBtn.hidden, "The show more button is visible");

  const descriptionWrapper = card.querySelector(
    ".addon-detail-description-wrapper"
  );
  ok(
    descriptionWrapper.classList.contains("addon-detail-description-collapse"),
    "The long description is collapsed"
  );

  // After click the description should be expanded
  showMoreBtn.click();
  ok(
    !descriptionWrapper.classList.contains("addon-detail-description-collapse"),
    "The long description is expanded"
  );

  await closeView(win);
});

add_task(async function testMinimalExtension() {
  let win = await loadInitialView("extension");
  let doc = win.document;

  let card = getAddonCard(win, "addon2@mochi.test");
  ok(!card.hasAttribute("expanded"), "The list card is not expanded");
  let loaded = waitForViewLoad(win);
  card.querySelector('[action="expand"]').click();
  await loaded;

  card = getAddonCard(win, "addon2@mochi.test");
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
  await checkLabel(row, "updates");

  // Private browsing settings.
  row = rows.shift();
  await checkLabel(row, "private-browsing");

  // Private browsing help text.
  row = rows.shift();
  ok(row.classList.contains("addon-detail-help-row"), "There's a help row");
  ok(!row.hidden, "The help row is shown");
  is(
    doc.l10n.getAttributes(row).id,
    "addon-detail-private-browsing-help",
    "The help row is for private browsing"
  );

  await checkQuarantinedDomainsUserAllowedRows(card, rows);

  // Author.
  row = rows.shift();
  await checkLabel(row, "author");
  let text = row.lastChild;
  is(text.textContent, "I made it", "The author is set");
  ok(Text.isInstance(text), "The author is a text node");

  is(rows.length, 0, "There are no more rows");

  await closeView(win);
});

add_task(async function testDefaultTheme() {
  let win = await loadInitialView("theme");

  // The list card.
  let card = getAddonCard(win, DEFAULT_THEME_ID);
  ok(!card.hasAttribute("expanded"), "The list card is not expanded");

  let preview = card.querySelector(".card-heading-image");
  ok(preview, "There is a preview");
  ok(!preview.hidden, "The preview is visible");

  let loaded = waitForViewLoad(win);
  card.querySelector('[action="expand"]').click();
  await loaded;

  card = getAddonCard(win, DEFAULT_THEME_ID);

  preview = card.querySelector(".card-heading-image");
  ok(preview, "There is a preview");
  ok(!preview.hidden, "The preview is visible");

  // Check all the deck buttons are hidden.
  assertDeckHeadingHidden(card.details.tabGroup);

  let rows = getDetailRows(card);

  // Author.
  let author = rows.shift();
  await checkLabel(author, "author");
  let text = author.lastChild;
  is(text.textContent, "Mozilla", "The author is set");

  // Version.
  let version = rows.shift();
  await checkLabel(version, "version");
  is(version.lastChild.textContent, "1.3", "It's always version 1.3");

  // Last updated.
  let lastUpdated = rows.shift();
  await checkLabel(lastUpdated, "last-updated");
  let dateText = lastUpdated.lastChild.textContent;
  ok(dateText, "There is a date set");
  ok(!dateText.includes("Invalid Date"), `"${dateText}" should be a date`);

  is(rows.length, 0, "There are no more rows");

  await closeView(win);
});

add_task(async function testStaticTheme() {
  let win = await loadInitialView("theme");

  // The list card.
  let card = getAddonCard(win, "theme1@mochi.test");
  ok(!card.hasAttribute("expanded"), "The list card is not expanded");

  // Make sure the preview is set.
  let preview = card.querySelector(".card-heading-image");
  ok(preview, "There is a preview");
  is(preview.src, "http://example.com/preview.png", "The preview URL is set");
  is(preview.width, 664, "The width is set");
  is(preview.height, 90, "The height is set");
  is(preview.hidden, false, "The preview is visible");

  // Load the detail view.
  let loaded = waitForViewLoad(win);
  card.querySelector('[action="expand"]').click();
  await loaded;

  card = getAddonCard(win, "theme1@mochi.test");

  // Make sure the preview is still set.
  preview = card.querySelector(".card-heading-image");
  ok(preview, "There is a preview");
  is(preview.src, "http://example.com/preview.png", "The preview URL is set");
  is(preview.width, 664, "The width is set");
  is(preview.height, 90, "The height is set");
  is(preview.hidden, false, "The preview is visible");

  // Check all the deck buttons are hidden.
  assertDeckHeadingHidden(card.details.tabGroup);

  let rows = getDetailRows(card);

  // Automatic updates.
  let row = rows.shift();
  await checkLabel(row, "updates");

  // Author.
  let author = rows.shift();
  await checkLabel(author, "author");
  let text = author.lastElementChild;
  is(text.textContent, "Artist", "The author is set");

  is(rows.length, 0, "There was only 1 row");

  await closeView(win);
});

add_task(async function testSitePermission() {
  let win = await loadInitialView("sitepermission");

  // The list card.
  let card = getAddonCard(win, "sitepermission@mochi.test");
  ok(!card.hasAttribute("expanded"), "The list card is not expanded");

  // Load the detail view.
  let loaded = waitForViewLoad(win);
  card.querySelector('[action="expand"]').click();
  await loaded;

  card = getAddonCard(win, "sitepermission@mochi.test");

  // Check all the deck buttons are hidden.
  assertDeckHeadingHidden(card.details.tabGroup);

  let sitepermissionsRow = card.querySelector(".addon-detail-sitepermissions");
  is(
    BrowserTestUtils.is_visible(sitepermissionsRow),
    true,
    "AddonSitePermissionsList should be visible for this addon type"
  );

  let [versionRow, ...restRows] = getDetailRows(card);
  await checkLabel(versionRow, "version");

  Assert.deepEqual(
    restRows.map(row => row.getAttribute("class")),
    [],
    "All other details row are hidden as expected"
  );

  let permissions = Array.from(
    card.querySelectorAll(".addon-permissions-list .permission-info")
  );
  is(permissions.length, 1, "a permission is listed");
  is(permissions[0].textContent, "Access MIDI devices", "got midi permission");

  await closeView(win);
});

add_task(async function testPrivateBrowsingExtension() {
  let id = "pb@mochi.test";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "My PB extension",
      browser_specific_settings: { gecko: { id } },
    },
    useAddonManager: "permanent",
  });

  await extension.startup();

  let win = await loadInitialView("extension");
  let doc = win.document;

  // The add-on shouldn't show that it's allowed yet.
  let card = getAddonCard(win, id);
  let badge = card.querySelector(".addon-badge-private-browsing-allowed");
  ok(badge.hidden, "The PB badge is hidden initially");
  ok(!(await hasPrivateAllowed(id)), "PB is not allowed");

  // Load the detail view.
  let loaded = waitForViewLoad(win);
  card.querySelector('[action="expand"]').click();
  await loaded;

  // The badge is still hidden on the detail view.
  card = getAddonCard(win, id);
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

  // Not sure what better to await here.
  await TestUtils.waitForCondition(() => !badge.hidden);

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
  ok(await hasPrivateAllowed(id), "PB is allowed");
  ok(!badge.hidden, "The PB badge is shown");

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
      browser_specific_settings: { gecko: { id } },
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
  ok(list, "Moved to a list page");
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
      browser_specific_settings: { gecko: { id } },
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
  ok(list, "Moved to a list page");
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
      browser_specific_settings: { gecko: { id: "allowed@mochi.test" } },
    },
    useAddonManager: "permanent",
  });

  await extension.startup();
  let perms = { permissions: ["internal:privateBrowsingAllowed"], origins: [] };
  await ExtensionPermissions.add("allowed@mochi.test", perms);
  let addon = await AddonManager.getAddonByID("allowed@mochi.test");
  await addon.reload();

  let win = await loadInitialView("extension");

  // The allowed extension should have a badge on load.
  let card = getAddonCard(win, "allowed@mochi.test");
  let badge = card.querySelector(".addon-badge-private-browsing-allowed");
  ok(!badge.hidden, "The PB badge is shown for the allowed add-on");

  await extension.unload();
  await closeView(win);
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
  let backButton = doc.querySelector(".back-button");

  let loadDetailView = () => {
    let loaded = waitForViewLoad(win);
    getAddonCard(win, id).querySelector("[action=expand]").click();
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

  let card = getAddonCard(win, DEFAULT_THEME_ID);
  let enabledItems = card.options.visibleItems;
  is(enabledItems.length, 1, "There is one enabled item");
  is(enabledItems[0].getAttribute("action"), "expand", "Expand is enabled");
  let moreOptionsButton = card.querySelector(".more-options-button");
  ok(!moreOptionsButton.hidden, "The more options button is visible");

  let loaded = waitForViewLoad(win);
  enabledItems[0].click();
  await loaded;

  card = getAddonCard(win, DEFAULT_THEME_ID);
  let toggleDisabledButton = card.querySelector('[action="toggle-disabled"]');
  enabledItems = card.options.visibleItems;
  is(enabledItems.length, 0, "There are no enabled items");
  moreOptionsButton = card.querySelector(".more-options-button");
  ok(moreOptionsButton.hidden, "The more options button is now hidden");
  ok(toggleDisabledButton.hidden, "The disable button is hidden");

  // Switch themes, the menu should be hidden, but enable button should appear.
  let darkTheme = await AddonManager.getAddonByID(DARK_THEME_ID);
  let updated = BrowserTestUtils.waitForEvent(card, "update");
  await darkTheme.enable();
  await updated;

  ok(moreOptionsButton.hidden, "The more options button is still hidden");
  ok(!toggleDisabledButton.hidden, "The enable button is visible");

  updated = BrowserTestUtils.waitForEvent(card, "update");
  await toggleDisabledButton.click();
  await updated;

  ok(moreOptionsButton.hidden, "The more options button is hidden");
  ok(toggleDisabledButton.hidden, "The disable button is hidden");

  await closeView(win);
});

add_task(async function testGoBackButtonIsDisabledWhenHistoryIsEmpty() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: { name: "Test Go Back Button" },
    useAddonManager: "temporary",
  });
  await extension.startup();

  let viewID = `addons://detail/${encodeURIComponent(extension.id)}`;

  // When we have a fresh new tab, `about:addons` is opened in it.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, null);
  // Simulate a click on "Manage extension" from a context menu.
  let win = await BrowserOpenAddonsMgr(viewID);
  await assertBackButtonIsDisabled(win);

  BrowserTestUtils.removeTab(tab);
  await extension.unload();
});

add_task(async function testGoBackButtonIsDisabledWhenHistoryIsEmptyInNewTab() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: { name: "Test Go Back Button" },
    useAddonManager: "temporary",
  });
  await extension.startup();

  let viewID = `addons://detail/${encodeURIComponent(extension.id)}`;

  // When we have a tab with a page loaded, `about:addons` will be opened in a
  // new tab.
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.org"
  );
  let addonsTabLoaded = BrowserTestUtils.waitForNewTab(
    gBrowser,
    "about:addons",
    true
  );
  // Simulate a click on "Manage extension" from a context menu.
  let win = await BrowserOpenAddonsMgr(viewID);
  let addonsTab = await addonsTabLoaded;
  await assertBackButtonIsDisabled(win);

  BrowserTestUtils.removeTab(addonsTab);
  BrowserTestUtils.removeTab(tab);
  await extension.unload();
});

add_task(async function testGoBackButtonIsDisabledAfterBrowserBackButton() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: { name: "Test Go Back Button" },
    useAddonManager: "temporary",
  });
  await extension.startup();

  let viewID = `addons://detail/${encodeURIComponent(extension.id)}`;

  // When we have a fresh new tab, `about:addons` is opened in it.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, null);
  // Simulate a click on "Manage extension" from a context menu.
  let win = await BrowserOpenAddonsMgr(viewID);
  await assertBackButtonIsDisabled(win);

  // Navigate to the extensions list.
  await new CategoryUtilities(win).openType("extension");

  // Click on the browser back button.
  gBrowser.goBack();
  await assertBackButtonIsDisabled(win);

  BrowserTestUtils.removeTab(tab);
  await extension.unload();
});

add_task(async function testQuarantinedDomainsUserAllowedUI() {
  let regularExtId = "regular@mochi.test";
  let privilegedExtId = "privileged@mochi.test";
  let recommendedExtId = "recommended@mochi.test";
  let themeId = "theme@mochi.test";
  let provider = new MockProvider();
  provider.createAddons([
    {
      id: privilegedExtId,
      isPrivileged: true,
      name: "A privileged extension",
      type: "extension",
      quarantineIgnoredByApp: true,
      quarantineIgnoredByUser: false,
      canChangeQuarantineIgnored: false,
    },
    {
      id: recommendedExtId,
      isRecommended: true,
      recommendationStates: ["recommended"],
      name: "A Recommended extension",
      type: "extension",
      quarantineIgnoredByApp: true,
      quarantineIgnoredByUser: false,
      canChangeQuarantineIgnored: false,
    },
    {
      id: themeId,
      name: "A fake regular theme",
      type: "theme",
      canChangeQuarantineIgnored: false,
    },
  ]);

  let regularExtension = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "Some regular extension",
      browser_specific_settings: { gecko: { id: regularExtId } },
    },
    useAddonManager: "permanent",
  });

  async function testQuarantinedUserAllowedUIRows(id, { expectVisible }) {
    const perAddonPref = QuarantinedDomains.getUserAllowedAddonIdPrefName(id);
    Services.prefs.clearUserPref(perAddonPref);

    let card = getAddonCard(win, id);

    const cardDetails = card.querySelector("addon-details");
    ok(cardDetails, "Card details found");
    const quarantinedUserAllowedControlsRow = cardDetails.querySelector(
      ".addon-detail-row-quarantined-domains"
    );

    ok(
      quarantinedUserAllowedControlsRow,
      "Found quarantine domains controls row element"
    );

    is(
      BrowserTestUtils.is_visible(quarantinedUserAllowedControlsRow),
      expectVisible,
      `Expect quarantineIgnoreByUser UI to ${
        expectVisible ? "be" : "NOT be"
      } visible`
    );
    const helpRow = quarantinedUserAllowedControlsRow.nextElementSibling;
    is(
      helpRow.classList.contains("addon-detail-help-row"),
      true,
      "Expect next sibling to be an addon-detail-help-row"
    );
    is(
      BrowserTestUtils.is_visible(helpRow),
      expectVisible,
      `Expect quarantineIgnoredByUser UI help to ${
        expectVisible ? "be" : "NOT be"
      } visible`
    );

    if (!expectVisible) {
      // The assertion that follows are going to be executed when the
      // test helper function is called for an addon card detail view
      // for which the quarantined domains rows are expected to be
      // visible.
      return;
    }

    is(
      doc.l10n.getAttributes(helpRow.firstElementChild).id,
      "addon-detail-quarantined-domains-help",
      "Expect addon-detail-help-row to be localized"
    );
    const helpSupportLink = helpRow.querySelector("[is=moz-support-link]");
    ok(helpSupportLink, "Expect a moz-support-link");
    is(
      helpSupportLink?.getAttribute("support-page"),
      "quarantined-domains",
      "Expect support link to point to SUMO quarantined-domains page"
    );
    // Make sure none of the elements in the help row are missing
    // the expected strings associated to the fluent ids being set
    // (if any is missing, l10n.translateElements will reject and
    // trigger an explicit test failure);
    await doc.l10n.translateElements([helpRow]);

    const radioInputs = Array.from(
      quarantinedUserAllowedControlsRow.querySelectorAll(
        "input[name=quarantined-domains-user-allowed]"
      )
    );

    Assert.deepEqual(
      radioInputs.map(el => el.value),
      ["1", "0"],
      "Got the expected radio inputs values"
    );

    Assert.deepEqual(
      radioInputs.map(el => doc.l10n.getAttributes(el.nextElementSibling).id),
      ["allow", "disallow"].map(
        txt => `addon-detail-quarantined-domains-${txt}`
      ),
      "Got the expected fluent ids on the radio input text"
    );

    const checkRadioInputsState = ({ expectUserAllowed }) => {
      is(
        card.addon.quarantineIgnoredByUser,
        expectUserAllowed,
        `Expect the test extension to ${
          expectUserAllowed ? "be" : "NOT be"
        } quarantineIgnoredByUser`
      );
      is(
        radioInputs[0].checked,
        expectUserAllowed,
        `Expect 'allow' radio button to ${
          expectUserAllowed ? "be" : "NOT be"
        } checked`
      );
      is(
        radioInputs[1].checked,
        !expectUserAllowed,
        `Expect 'disallow' radio button ${
          expectUserAllowed ? "NOT be" : "be"
        } checked`
      );
    };

    info("Verify initially NOT allowed to access quarantine domains");
    checkRadioInputsState({ expectUserAllowed: false });

    info("Click 'allow' radio input");
    radioInputs[0].click();
    checkRadioInputsState({ expectUserAllowed: true });

    info("Click 'disallow' radio input");
    radioInputs[1].click();
    checkRadioInputsState({ expectUserAllowed: false });

    info("Verify quarantineIgnoredByUser changes reflected in about:addons UI");

    info("Allow test extension on quarantined domains");
    let promisePropertyChanged =
      AddonTestUtils.promiseAddonEvent("onPropertyChanged");
    card.addon.quarantineIgnoredByUser = true;
    await promisePropertyChanged;
    checkRadioInputsState({ expectUserAllowed: true });

    info("Disallow test extension on quarantined domains");
    promisePropertyChanged =
      AddonTestUtils.promiseAddonEvent("onPropertyChanged");
    card.addon.quarantineIgnoredByUser = false;
    await promisePropertyChanged;
    checkRadioInputsState({ expectUserAllowed: false });
  }

  await SpecialPowers.pushPrefEnv({
    set: [
      // Make sure the quarantined domains feature is initially enabled
      // otherwise the "quarantineIgnoredByUser UI" rows are
      // going to be hidden.
      ["extensions.quarantinedDomains.enabled", true],
      // Make sure this test is always running with the
      // "per-addon quarantineIgnoredByUser UI" feature enabled.
      ["extensions.quarantinedDomains.uiDisabled", false],
    ],
  });

  // Clear any per-addon pref once this test file is exiting.
  registerCleanupFunction(() => {
    const prefBranch = Services.prefs.getBranch(
      QuarantinedDomains.PREF_ADDONS_BRANCH_NAME
    );
    for (const leafName of prefBranch.getChildList("")) {
      const prefName = QuarantinedDomains.PREF_ADDONS_BRANCH_NAME + leafName;
      info(`Clearing user pref ${prefName}`);
      Services.prefs.clearUserPref(prefName);
    }
  });

  await regularExtension.startup();

  let win = await loadInitialView("extension");
  let doc = win.document;

  info("Test quarantineIgnoredByUser UI on a regular extension");
  let loaded = waitForViewLoad(win);
  getAddonCard(win, regularExtId).querySelector('[action="expand"]').click();
  await loaded;

  await testQuarantinedUserAllowedUIRows(regularExtId, { expectVisible: true });

  info("Go back to extensions list view");
  loaded = waitForViewLoad(win);
  win.history.back();
  await loaded;

  info("Test quarantineIgnoredByUser UI on a privileged extension");
  loaded = waitForViewLoad(win);
  getAddonCard(win, privilegedExtId).querySelector('[action="expand"]').click();
  await loaded;

  await testQuarantinedUserAllowedUIRows(privilegedExtId, {
    expectVisible: false,
  });

  info("Go back to extensions list view");
  loaded = waitForViewLoad(win);
  win.history.back();
  await loaded;

  info("Test quarantineIgnoredByUser UI on a recommended extension");
  loaded = waitForViewLoad(win);
  getAddonCard(win, recommendedExtId)
    .querySelector('[action="expand"]')
    .click();
  await loaded;

  await testQuarantinedUserAllowedUIRows(recommendedExtId, {
    expectVisible: false,
  });

  info("Switch to theme list view");
  loaded = waitForViewLoad(win);
  doc.querySelector("#categories > [name=theme]").click();
  await loaded;

  info("Test quarantineIgnoredByUser UI on a non extension addon type (theme)");
  loaded = waitForViewLoad(win);
  getAddonCard(win, themeId).querySelector('[action="expand"]').click();
  await loaded;

  await testQuarantinedUserAllowedUIRows(themeId, { expectVisible: false });

  info("Verify regular extension card on quarantined domains feature disabled");
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.quarantinedDomains.enabled", false]],
  });

  info("Switch to extension list view");
  loaded = waitForViewLoad(win);
  doc.querySelector("#categories > [name=extension]").click();
  await loaded;

  loaded = waitForViewLoad(win);
  getAddonCard(win, regularExtId).querySelector('[action="expand"]').click();
  await loaded;

  await testQuarantinedUserAllowedUIRows(regularExtId, {
    expectVisible: false,
  });

  await SpecialPowers.popPrefEnv();

  info("Verify regular extenson card uiDisabled pref set to true");
  await SpecialPowers.pushPrefEnv({
    set: [
      // Make sure the quarantineIgnoredByUser UI is also hidden
      // when the quarantine domains feature is enabled but the
      // "per-addon quarantineIgnoredByUser UI" feature is disabled.
      ["extensions.quarantinedDomains.uiDisabled", true],
    ],
  });

  info("Switch to extension list view");
  loaded = waitForViewLoad(win);
  doc.querySelector("#categories > [name=extension]").click();
  await loaded;

  loaded = waitForViewLoad(win);
  getAddonCard(win, regularExtId).querySelector('[action="expand"]').click();
  await loaded;

  await testQuarantinedUserAllowedUIRows(regularExtId, {
    expectVisible: false,
  });

  await closeView(win);
  await regularExtension.unload();
  await SpecialPowers.popPrefEnv();
  await SpecialPowers.popPrefEnv();
});

add_task(async function testRatingsElementVisibleIfReviewURLExists() {
  let win = await loadInitialView("extension");
  let id = "addon4@mochi.test";
  let card = getAddonCard(win, id);

  let loaded = waitForViewLoad(win);
  card.querySelector('[action="expand"]').click();
  await loaded;

  card = getAddonCard(win, id);

  let rows = getDetailRows(card);

  let expectedRowCount = 5;
  if (card.addon.canChangeQuarantineIgnored) {
    expectedRowCount += 2;
  }
  is(rows.length, expectedRowCount, "Expected row count");

  // Reviews.
  // addon4@mochi.test is similar to addon1@mochi.test whose rows have already
  // been checked in testFullDetails. Here we only check the last row
  // which is unique to this test case due to the presence of "reviewURL".
  let row = rows.pop();
  await checkLabel(row, "rating");
  let rating = row.lastElementChild;
  ok(rating.classList.contains("addon-detail-rating"), "Found the rating el");
  ok(!row.hidden, "The rating row is shown");
  let mozFiveStar = rating.querySelector("moz-five-star");
  is(mozFiveStar.rating, 0, "0 rating when there are no reviews");
  let stars = Array.from(mozFiveStar.starEls);
  let fullAttrs = stars.map(star => star.getAttribute("fill")).join(",");
  is(fullAttrs, "empty,empty,empty,empty,empty", "All stars are empty");
  let link = rating.querySelector("a");
  let reviewsLink = formatUrl(
    "addons-manager-reviews-link",
    "http://addons.mozilla.org/reviews"
  );
  checkLink(link, reviewsLink, {
    id: "addon-detail-reviews-link",
    args: { numberOfReviews: 0 },
  });

  await closeView(win);
});
