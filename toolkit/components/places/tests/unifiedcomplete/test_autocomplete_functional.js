/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Functional tests for inline autocomplete

const PREF_AUTOCOMPLETE_ENABLED = "browser.urlbar.autocomplete.enabled";

add_task(async function test_disabling_autocomplete() {
  info("Check disabling autocomplete disables autofill");
  Services.prefs.setBoolPref(PREF_AUTOCOMPLETE_ENABLED, false);
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://visit.mozilla.org"),
  });
  await check_autocomplete({
    search: "vis",
    autofilled: "vis",
    completed: "vis"
  });
  await cleanup();
});

add_task(async function test_urls_order() {
  info("Add urls, check for correct order");
  let places = [{ uri: NetUtil.newURI("http://visit1.mozilla.org") },
                { uri: NetUtil.newURI("http://visit2.mozilla.org") }];
  await PlacesTestUtils.addVisits(places);
  await check_autocomplete({
    search: "vis",
    autofilled: "visit2.mozilla.org/",
    completed: "http://visit2.mozilla.org/"
  });
  await cleanup();
});

add_task(async function test_bookmark_first() {
  info("With a bookmark and history, the query result should be the bookmark");
  await addBookmark({ uri: NetUtil.newURI("http://bookmark1.mozilla.org/") });
  await PlacesTestUtils.addVisits(NetUtil.newURI("http://bookmark1.mozilla.org/foo"));
  await check_autocomplete({
    search: "bookmark",
    autofilled: "bookmark1.mozilla.org/",
    completed: "http://bookmark1.mozilla.org/"
  });
  await cleanup();
});

add_task(async function test_complete_querystring() {
  info("Check to make sure we autocomplete after ?");
  await PlacesTestUtils.addVisits(NetUtil.newURI("http://smokey.mozilla.org/foo?bacon=delicious"));
  await check_autocomplete({
    search: "smokey.mozilla.org/foo?",
    autofilled: "smokey.mozilla.org/foo?bacon=delicious",
    completed: "http://smokey.mozilla.org/foo?bacon=delicious",
  });
  await cleanup();
});

add_task(async function test_complete_fragment() {
  info("Check to make sure we autocomplete after #");
  await PlacesTestUtils.addVisits(NetUtil.newURI("http://smokey.mozilla.org/foo?bacon=delicious#bar"));
  await check_autocomplete({
    search: "smokey.mozilla.org/foo?bacon=delicious#bar",
    autofilled: "smokey.mozilla.org/foo?bacon=delicious#bar",
    completed: "http://smokey.mozilla.org/foo?bacon=delicious#bar",
  });
  await cleanup();
});

add_task(async function test_autocomplete_enabled_pref() {
  Services.prefs.setBoolPref(PREF_AUTOCOMPLETE_ENABLED, false);
  let types = ["history", "bookmark", "openpage"];
  for (let type of types) {
    Assert.equal(Services.prefs.getBoolPref("browser.urlbar.suggest." + type), false,
                 "suggest." + type + "pref should be false");
  }
  Services.prefs.setBoolPref(PREF_AUTOCOMPLETE_ENABLED, true);
  for (let type of types) {
    Assert.equal(Services.prefs.getBoolPref("browser.urlbar.suggest." + type), true,
                 "suggest." + type + "pref should be true");
  }

  // Clear prefs.
  Services.prefs.clearUserPref(PREF_AUTOCOMPLETE_ENABLED);
  for (let type of types) {
    Services.prefs.clearUserPref("browser.urlbar.suggest." + type);
  }
});
