/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint max-len: ["error", 80] */
"use strict";

const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);

AddonTestUtils.initMochitest(this);

function makeResult({ guid, type }) {
  return {
    addon: {
      authors: [{ name: "Some author" }],
      current_version: {
        files: [{ platform: "all", url: "data:," }],
      },
      url: "data:,",
      guid,
      type,
    },
  };
}

function mockResults() {
  let types = ["extension", "theme", "extension", "extension", "theme"];
  return {
    results: types.map((type, i) =>
      makeResult({
        guid: `${type}${i}@mochi.test`,
        type,
      })
    ),
  };
}

add_task(async function setup() {
  let results = btoa(JSON.stringify(mockResults()));
  await SpecialPowers.pushPrefEnv({
    set: [
      // Disable personalized recommendations, they will break the data URI.
      ["browser.discovery.enabled", false],
      ["extensions.getAddons.discovery.api_url", `data:;base64,${results}`],
      [
        "extensions.recommendations.themeRecommendationUrl",
        "https://example.com/theme",
      ],
    ],
  });
});

function checkExtraContents(doc, type, opts = {}) {
  let { showThemeRecommendationFooter = type === "theme" } = opts;
  let footer = doc.querySelector("footer");
  let amoButton = footer.querySelector('[action="open-amo"]');
  let privacyPolicyLink = footer.querySelector(".privacy-policy-link");
  let themeRecommendationFooter = footer.querySelector(".theme-recommendation");
  let themeRecommendationLink =
    themeRecommendationFooter && themeRecommendationFooter.querySelector("a");
  let taarNotice = doc.querySelector("taar-notice");

  is_element_visible(footer, "The footer is visible");

  if (type == "extension") {
    ok(taarNotice, "There is a TAAR notice");
    is_element_visible(amoButton, "The AMO button is shown");
    is_element_visible(privacyPolicyLink, "The privacy policy is visible");
  } else if (type == "theme") {
    ok(!taarNotice, "There is no TAAR notice");
    ok(amoButton, "AMO button is shown");
    ok(!privacyPolicyLink, "There is no privacy policy");
  } else {
    throw new Error(`Unknown type ${type}`);
  }

  if (showThemeRecommendationFooter) {
    is_element_visible(
      themeRecommendationFooter,
      "There's a theme recommendation footer"
    );
    is_element_visible(themeRecommendationLink, "There's a link to the theme");
    is(themeRecommendationLink.target, "_blank", "The link opens in a new tab");
    is(
      themeRecommendationLink.href,
      "https://example.com/theme",
      "The link goes to the pref's URL"
    );
    is(
      doc.l10n.getAttributes(themeRecommendationFooter).id,
      "recommended-theme-1",
      "The recommendation has the right l10n-id"
    );
  } else {
    ok(
      !themeRecommendationFooter || themeRecommendationFooter.hidden,
      "There's no theme recommendation"
    );
  }
}

async function installAddon({ card, recommendedList, manifestExtra = {} }) {
  // Install an add-on to hide the card.
  let hidden = BrowserTestUtils.waitForEvent(
    recommendedList,
    "card-hidden",
    false,
    e => e.detail.card == card
  );
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: { gecko: { id: card.addonId } },
      ...manifestExtra,
    },
    useAddonManager: "temporary",
  });
  await extension.startup();
  await hidden;
  return extension;
}

async function testListRecommendations({ type, manifestExtra = {} }) {
  Services.telemetry.clearEvents();

  let win = await loadInitialView(type);
  let doc = win.document;

  // Wait for the list to render, rendering is tested with the discovery pane.
  let recommendedList = doc.querySelector("recommended-addon-list");
  await recommendedList.cardsReady;

  checkExtraContents(doc, type);

  // Check that the cards are all for the right type.
  let cards = doc.querySelectorAll("recommended-addon-card");
  ok(!!cards.length, "There were some cards found");
  for (let card of cards) {
    is(card.discoAddon.type, type, `The card is for a ${type}`);
    is_element_visible(card, "The card is visible");
  }

  // Install an add-on for the first card, verify it is hidden.
  let { addonId } = cards[0];
  ok(addonId, "The card has an addonId");

  // Installing the add-on will fail since the URL doesn't point to a valid
  // XPI. This will trigger the telemetry though.
  let installButton = cards[0].querySelector('[action="install-addon"]');
  let { panel } = PopupNotifications;
  let popupId = "addon-install-failed-notification";
  let failPromise = TestUtils.topicObserved("addon-install-failed");
  installButton.click();
  await failPromise;
  // Wait for the installing popup to be hidden and leave just the error popup.
  await BrowserTestUtils.waitForCondition(() => {
    return panel.children.length == 1 && panel.firstElementChild.id == popupId;
  });

  // Dismiss the popup.
  panel.firstElementChild.button.click();
  await BrowserTestUtils.waitForPopupEvent(panel, "hidden");

  let extension = await installAddon({ card: cards[0], recommendedList });
  is_element_hidden(cards[0], "The card is now hidden");

  // Switch away and back, there should still be a hidden card.
  await closeView(win);
  win = await loadInitialView(type);
  doc = win.document;
  recommendedList = doc.querySelector("recommended-addon-list");
  await recommendedList.cardsReady;

  cards = Array.from(doc.querySelectorAll("recommended-addon-card"));

  let hiddenCard = cards.pop();
  is(hiddenCard.addonId, addonId, "The expected card was found");
  is_element_hidden(hiddenCard, "The card is still hidden");

  ok(!!cards.length, "There are still some visible cards");
  for (let card of cards) {
    is(card.discoAddon.type, type, `The card is for a ${type}`);
    is_element_visible(card, "The card is visible");
  }

  // Uninstall the add-on, verify the card is shown again.
  let shown = BrowserTestUtils.waitForEvent(recommendedList, "card-shown");
  await extension.unload();
  await shown;

  is_element_visible(hiddenCard, "The card is now shown");

  await closeView(win);

  assertAboutAddonsTelemetryEvents(
    [
      [
        "addonsManager",
        "action",
        "aboutAddons",
        null,
        { action: "installFromRecommendation", view: "list", addonId, type },
      ],
    ],
    { methods: ["action"] }
  );
}

add_task(async function testExtensionList() {
  await testListRecommendations({ type: "extension" });
});

add_task(async function testThemeList() {
  await testListRecommendations({
    type: "theme",
    manifestExtra: { theme: {} },
  });
});

add_task(async function testInstallAllExtensions() {
  let type = "extension";
  let win = await loadInitialView(type);
  let doc = win.document;

  // Wait for the list to render, rendering is tested with the discovery pane.
  let recommendedList = doc.querySelector("recommended-addon-list");
  await recommendedList.cardsReady;

  // Find more button is shown.
  checkExtraContents(doc, type);

  let cards = Array.from(doc.querySelectorAll("recommended-addon-card"));
  is(cards.length, 3, "We found some cards");

  let extensions = await Promise.all(
    cards.map(card => installAddon({ card, recommendedList }))
  );

  // The find more on AMO button is shown.
  checkExtraContents(doc, type);

  // Uninstall one of the extensions, the button should still be shown.
  let extension = extensions.pop();
  let shown = BrowserTestUtils.waitForEvent(recommendedList, "card-shown");
  await extension.unload();
  await shown;

  // The find more on AMO button is shown.
  checkExtraContents(doc, type);

  await Promise.all(extensions.map(extension => extension.unload()));
  await closeView(win);
});

add_task(async function testError() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.getAddons.discovery.api_url", "data:,"]],
  });

  let win = await loadInitialView("extension");
  let doc = win.document;

  // Wait for the list to render, rendering is tested with the discovery pane.
  let recommendedList = doc.querySelector("recommended-addon-list");
  await recommendedList.cardsReady;

  checkExtraContents(doc, "extension");

  await closeView(win);
  await SpecialPowers.popPrefEnv();
});

add_task(async function testThemesNoRecommendationUrl() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.recommendations.themeRecommendationUrl", ""]],
  });

  let win = await loadInitialView("theme");
  let doc = win.document;

  // Wait for the list to render, rendering is tested with the discovery pane.
  let recommendedList = doc.querySelector("recommended-addon-list");
  await recommendedList.cardsReady;

  checkExtraContents(doc, "theme", { showThemeRecommendationFooter: false });

  await closeView(win);
  await SpecialPowers.popPrefEnv();
});

add_task(async function testRecommendationsDisabled() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.htmlaboutaddons.recommendations.enabled", false]],
  });

  let types = ["extension", "theme"];

  for (let type of types) {
    let win = await loadInitialView(type);
    let doc = win.document;

    let recommendedList = doc.querySelector("recommended-addon-list");
    ok(!recommendedList, `There are no recommendations on the ${type} page`);

    await closeView(win);
  }

  await SpecialPowers.popPrefEnv();
});
