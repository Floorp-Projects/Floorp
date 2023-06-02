/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint max-len: ["error", 80] */

"use strict";

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);

AddonTestUtils.initMochitest(this);

const SUPPORT_URL = "http://support.allizom.org/support-dummy/";
const SUMO_URL = SUPPORT_URL + "add-on-badges";
const SUPPORTED_BADGES = ["recommended", "line", "verified"];

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["app.support.baseURL", SUPPORT_URL]],
  });
});

const server = AddonTestUtils.createHttpServer({
  hosts: ["support.allizom.org"],
});
server.registerPathHandler("/support-dummy", (request, response) => {
  response.write("Dummy");
});

async function checkRecommendedBadge(id, badges = []) {
  async function checkBadge() {
    let card = win.document.querySelector(`addon-card[addon-id="${id}"]`);
    for (let badgeName of SUPPORTED_BADGES) {
      let badge = card.querySelector(`.addon-badge-${badgeName}`);
      let hidden = !badges.includes(badgeName);
      is(
        badge.hidden,
        hidden,
        `badge ${badgeName} is ${hidden ? "hidden" : "shown"}`
      );
      // Verify the utm params.
      ok(
        badge.href.startsWith(SUMO_URL),
        "links to sumo correctly " + badge.href
      );
      if (!hidden) {
        info(`Verify the ${badgeName} badge links to the support page`);
        let tabLoaded = BrowserTestUtils.waitForNewTab(gBrowser, badge.href);
        EventUtils.synthesizeMouseAtCenter(badge, {}, win);
        BrowserTestUtils.removeTab(await tabLoaded);
      }
      let url = new URL(badge.href);
      is(
        url.searchParams.get("utm_content"),
        "promoted-addon-badge",
        "content param correct"
      );
      is(
        url.searchParams.get("utm_source"),
        "firefox-browser",
        "source param correct"
      );
      is(
        url.searchParams.get("utm_medium"),
        "firefox-browser",
        "medium param correct"
      );
    }
    for (let badgeName of badges) {
      if (!SUPPORTED_BADGES.includes(badgeName)) {
        ok(
          !card.querySelector(`.addon-badge-${badgeName}`),
          `no badge element for ${badgeName}`
        );
      }
    }
    return card;
  }

  let win = await loadInitialView("extension");

  // Check list view.
  let card = await checkBadge();

  // Load detail view.
  let loaded = waitForViewLoad(win);
  card.querySelector('[action="expand"]').click();
  await loaded;

  // Check detail view.
  await checkBadge();

  await closeView(win);
}

add_task(async function testNotRecommended() {
  let id = "not-recommended@mochi.test";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: { browser_specific_settings: { gecko: { id } } },
    useAddonManager: "temporary",
  });
  await extension.startup();

  await checkRecommendedBadge(id);

  await extension.unload();
});

async function test_badged_addon(addon) {
  let provider = new MockProvider();
  provider.createAddons([addon]);
  await checkRecommendedBadge(addon.id, addon.recommendationStates);

  provider.unregister();
}

add_task(async function testRecommended() {
  await test_badged_addon({
    id: "recommended@mochi.test",
    isRecommended: true,
    recommendationStates: ["recommended"],
    name: "Recommended",
    type: "extension",
  });
});

add_task(async function testLine() {
  await test_badged_addon({
    id: "line@mochi.test",
    isRecommended: false,
    recommendationStates: ["line"],
    name: "Line",
    type: "extension",
  });
});

add_task(async function testVerified() {
  await test_badged_addon({
    id: "verified@mochi.test",
    isRecommended: false,
    recommendationStates: ["verified"],
    name: "Verified",
    type: "extension",
  });
});

add_task(async function testOther() {
  await test_badged_addon({
    id: "other@mochi.test",
    isRecommended: false,
    recommendationStates: ["other"],
    name: "No Badge",
    type: "extension",
  });
});

add_task(async function testMultiple() {
  await test_badged_addon({
    id: "multiple@mochi.test",
    isRecommended: false,
    recommendationStates: ["verified", "recommended", "other"],
    name: "Multiple",
    type: "extension",
  });
});
