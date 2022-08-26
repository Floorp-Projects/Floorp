/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { PromiseUtils } = ChromeUtils.import(
  "resource://gre/modules/PromiseUtils.jsm"
);

const HOMEPAGE_PREF = "browser.startup.homepage";
const NEWTAB_PREF = "browser.newtabpage.enabled";
const HIGHLIGHTS_PREF =
  "browser.newtabpage.activity-stream.feeds.section.highlights";
const HIGHLIGHTS_ROWS_PREF =
  "browser.newtabpage.activity-stream.section.highlights.rows";
const SEARCH_PREF = "browser.newtabpage.activity-stream.showSearch";
const TOPSITES_PREF = "browser.newtabpage.activity-stream.feeds.topsites";
const SNIPPETS_PREF = "browser.newtabpage.activity-stream.feeds.snippets";
const TOPSTORIES_PREF =
  "browser.newtabpage.activity-stream.feeds.system.topstories";

add_setup(async function() {
  await SpecialPowers.pushPrefEnv({
    // Highlights are preffed off by default.
    set: [[HIGHLIGHTS_PREF, true]],
  });

  registerCleanupFunction(async () => {
    await SpecialPowers.popPrefEnv();
    [
      HOMEPAGE_PREF,
      NEWTAB_PREF,
      HIGHLIGHTS_PREF,
      HIGHLIGHTS_ROWS_PREF,
      SEARCH_PREF,
      TOPSITES_PREF,
      SNIPPETS_PREF,
    ].forEach(prefName => Services.prefs.clearUserPref(prefName));
  });
});

add_task(async function test_CONFIGURE_HOMEPAGE_newtab_home_prefs() {
  const action = {
    type: "CONFIGURE_HOMEPAGE",
    data: { homePage: "default", newtab: "default" },
  };
  await SpecialPowers.pushPrefEnv({
    set: [
      [HOMEPAGE_PREF, "about:blank"],
      [NEWTAB_PREF, false],
    ],
  });

  Assert.ok(Services.prefs.prefHasUserValue(HOMEPAGE_PREF), "Test setup ok");
  Assert.ok(Services.prefs.prefHasUserValue(NEWTAB_PREF), "Test setup ok");

  await SMATestUtils.executeAndValidateAction(action);

  Assert.ok(
    !Services.prefs.prefHasUserValue(HOMEPAGE_PREF),
    "Homepage pref should be back to default"
  );
  Assert.ok(
    !Services.prefs.prefHasUserValue(NEWTAB_PREF),
    "Newtab pref should be back to default"
  );
});

add_task(async function test_CONFIGURE_HOMEPAGE_layout_prefs() {
  const action = {
    type: "CONFIGURE_HOMEPAGE",
    data: {
      layout: {
        search: true,
        topsites: false,
        highlights: false,
        snippets: false,
        topstories: false,
      },
    },
  };
  await SpecialPowers.pushPrefEnv({
    set: [
      [HIGHLIGHTS_ROWS_PREF, 3],
      [SEARCH_PREF, false],
    ],
  });

  await SMATestUtils.executeAndValidateAction(action);

  Assert.ok(Services.prefs.getBoolPref(SEARCH_PREF), "Search is turned on");
  Assert.ok(
    !Services.prefs.getBoolPref(TOPSITES_PREF),
    "Topsites are turned off"
  );
  Assert.ok(
    Services.prefs.getBoolPref(HIGHLIGHTS_PREF),
    "HIGHLIGHTS_PREF are on because they have been customized"
  );
  Assert.ok(
    !Services.prefs.getBoolPref(TOPSTORIES_PREF),
    "Topstories are turned off"
  );
  Assert.ok(
    !Services.prefs.getBoolPref(SNIPPETS_PREF),
    "Snippets are turned off"
  );
});
