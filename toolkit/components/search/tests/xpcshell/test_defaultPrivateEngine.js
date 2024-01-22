/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test that defaultEngine property can be set and yields the proper events and\
 * behavior (search results)
 */

"use strict";

let engine1;
let engine2;
let appDefault;
let appPrivateDefault;

add_setup(async () => {
  do_get_profile();
  Services.fog.initializeFOG();

  await SearchTestUtils.useTestEngines();

  Services.prefs.setCharPref(SearchUtils.BROWSER_SEARCH_PREF + "region", "US");
  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault.ui.enabled",
    true
  );
  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault",
    true
  );

  useHttpServer("opensearch");
  await AddonTestUtils.promiseStartupManager();

  await Services.search.init();

  appDefault = Services.search.appDefaultEngine;
  appPrivateDefault = Services.search.appPrivateDefaultEngine;
  engine1 = Services.search.getEngineByName("engine-rel-searchform-purpose");
  engine2 = Services.search.getEngineByName("engine-chromeicon");
});

add_task(async function test_defaultPrivateEngine() {
  Assert.equal(
    Services.search.defaultPrivateEngine,
    appPrivateDefault,
    "Should have the app private default as the default private engine"
  );
  Assert.equal(
    Services.search.defaultEngine,
    appDefault,
    "Should have the app default as the default engine"
  );

  await assertGleanDefaultEngine({
    normal: {
      engineId: "engine",
      displayName: "Test search engine",
      loadPath: SearchUtils.newSearchConfigEnabled
        ? "[app]engine@search.mozilla.org"
        : "[addon]engine@search.mozilla.org",
      submissionUrl: "https://www.google.com/search?q=",
      verified: "default",
    },
    private: {
      engineId: "engine-pref",
      displayName: "engine-pref",
      loadPath: SearchUtils.newSearchConfigEnabled
        ? "[app]engine-pref@search.mozilla.org"
        : "[addon]engine-pref@search.mozilla.org",
      submissionUrl: "https://www.google.com/search?q=",
      verified: "default",
    },
  });

  let promise = promiseDefaultNotification("private");
  Services.search.defaultPrivateEngine = engine1;
  Assert.equal(
    await promise,
    engine1,
    "Should have notified setting the private engine to the new one"
  );

  Assert.equal(
    Services.search.defaultPrivateEngine,
    engine1,
    "Should have set the private engine to the new one"
  );
  Assert.equal(
    Services.search.defaultEngine,
    appDefault,
    "Should not have changed the default engine"
  );

  await assertGleanDefaultEngine({
    normal: {
      engineId: "engine",
      displayName: "Test search engine",
      loadPath: SearchUtils.newSearchConfigEnabled
        ? "[app]engine@search.mozilla.org"
        : "[addon]engine@search.mozilla.org",
      submissionUrl: "https://www.google.com/search?q=",
      verified: "default",
    },
    private: {
      engineId: "engine-rel-searchform-purpose",
      displayName: "engine-rel-searchform-purpose",
      loadPath: SearchUtils.newSearchConfigEnabled
        ? "[app]engine-rel-searchform-purpose@search.mozilla.org"
        : "[addon]engine-rel-searchform-purpose@search.mozilla.org",
      submissionUrl: "https://www.google.com/search?q=&channel=sb",
      verified: "default",
    },
  });

  promise = promiseDefaultNotification("private");
  await Services.search.setDefaultPrivate(
    engine2,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );
  Assert.equal(
    await promise,
    engine2,
    "Should have notified setting the private engine to the new one using async api"
  );
  Assert.equal(
    Services.search.defaultPrivateEngine,
    engine2,
    "Should have set the private engine to the new one using the async api"
  );

  // We use the names here as for some reason the getDefaultPrivate promise
  // returns something which is an nsISearchEngine but doesn't compare
  // exactly to what engine2 is.
  Assert.equal(
    (await Services.search.getDefaultPrivate()).name,
    engine2.name,
    "Should have got the correct private engine with the async api"
  );
  Assert.equal(
    Services.search.defaultEngine,
    appDefault,
    "Should not have changed the default engine"
  );

  await assertGleanDefaultEngine({
    normal: {
      engineId: "engine",
      displayName: "Test search engine",
      loadPath: SearchUtils.newSearchConfigEnabled
        ? "[app]engine@search.mozilla.org"
        : "[addon]engine@search.mozilla.org",
      submissionUrl: "https://www.google.com/search?q=",
      verified: "default",
    },
    private: {
      engineId: "engine-chromeicon",
      displayName: "engine-chromeicon",
      loadPath: SearchUtils.newSearchConfigEnabled
        ? "[app]engine-chromeicon@search.mozilla.org"
        : "[addon]engine-chromeicon@search.mozilla.org",
      submissionUrl: "https://www.google.com/search?q=",
      verified: "default",
    },
  });

  promise = promiseDefaultNotification("private");
  await Services.search.setDefaultPrivate(
    engine1,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );
  Assert.equal(
    await promise,
    engine1,
    "Should have notified reverting the private engine to the selected one using async api"
  );
  Assert.equal(
    Services.search.defaultPrivateEngine,
    engine1,
    "Should have reverted the private engine to the selected one using the async api"
  );

  await assertGleanDefaultEngine({
    normal: {
      engineId: "engine",
    },
    private: {
      engineId: "engine-rel-searchform-purpose",
    },
  });

  engine1.hidden = true;
  Assert.equal(
    Services.search.defaultPrivateEngine,
    appPrivateDefault,
    "Should reset to the app default private engine when hiding the default"
  );
  Assert.equal(
    Services.search.defaultEngine,
    appDefault,
    "Should not have changed the default engine"
  );

  await assertGleanDefaultEngine({
    normal: {
      engineId: "engine",
    },
    private: {
      engineId: "engine-pref",
    },
  });

  engine1.hidden = false;
  Services.search.defaultEngine = engine1;
  Assert.equal(
    Services.search.defaultPrivateEngine,
    appPrivateDefault,
    "Setting the default engine should not affect the private default"
  );

  await assertGleanDefaultEngine({
    normal: {
      engineId: "engine-rel-searchform-purpose",
    },
    private: {
      engineId: "engine-pref",
    },
  });

  Services.search.defaultEngine = appDefault;
});

add_task(async function test_telemetry_private_empty_submission_url() {
  await SearchTestUtils.promiseNewSearchEngine({
    url: `${gDataUrl}simple.xml`,
    setAsDefaultPrivate: true,
  });

  await assertGleanDefaultEngine({
    normal: {
      engineId: appDefault.telemetryId,
    },
    private: {
      engineId: "other-simple",
      displayName: "simple",
      loadPath: "[http]localhost/simple.xml",
      submissionUrl: "blank:",
      verified: "verified",
    },
  });

  Services.search.defaultEngine = appDefault;
});

add_task(async function test_defaultPrivateEngine_turned_off() {
  Services.search.defaultEngine = appDefault;
  Services.search.defaultPrivateEngine = engine1;

  await assertGleanDefaultEngine({
    normal: {
      engineId: "engine",
    },
    private: {
      engineId: "engine-rel-searchform-purpose",
    },
  });

  let promise = promiseDefaultNotification("private");
  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault",
    false
  );
  Assert.equal(
    await promise,
    appDefault,
    "Should have notified setting the first engine correctly."
  );

  await assertGleanDefaultEngine({
    normal: {
      engineId: "engine",
    },
    private: {
      engineId: "",
    },
  });

  promise = promiseDefaultNotification("normal");
  let privatePromise = promiseDefaultNotification("private");
  Services.search.defaultEngine = engine1;
  Assert.equal(
    await promise,
    engine1,
    "Should have notified setting the first engine correctly."
  );
  Assert.equal(
    await privatePromise,
    engine1,
    "Should have notified setting of the private engine as well."
  );
  Assert.equal(
    Services.search.defaultPrivateEngine,
    engine1,
    "Should be set to the first engine correctly"
  );
  Assert.equal(
    Services.search.defaultEngine,
    engine1,
    "Should keep the default engine in sync with the pref off"
  );

  await assertGleanDefaultEngine({
    normal: {
      engineId: "engine-rel-searchform-purpose",
    },
    private: {
      engineId: "",
    },
  });

  promise = promiseDefaultNotification("private");
  Services.search.defaultPrivateEngine = engine2;
  Assert.equal(
    await promise,
    engine2,
    "Should have notified setting the second engine correctly."
  );
  Assert.equal(
    Services.search.defaultPrivateEngine,
    engine2,
    "Should be set to the second engine correctly"
  );
  Assert.equal(
    Services.search.defaultEngine,
    engine1,
    "Should not change the normal mode default engine"
  );
  Assert.equal(
    Services.prefs.getBoolPref(
      SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault",
      false
    ),
    true,
    "Should have set the separate private default pref to true"
  );

  await assertGleanDefaultEngine({
    normal: {
      engineId: "engine-rel-searchform-purpose",
    },
    private: {
      engineId: "engine-chromeicon",
    },
  });

  promise = promiseDefaultNotification("private");
  Services.search.defaultPrivateEngine = engine1;
  Assert.equal(
    await promise,
    engine1,
    "Should have notified resetting to the first engine again"
  );
  Assert.equal(
    Services.search.defaultPrivateEngine,
    engine1,
    "Should be reset to the first engine again"
  );
  Assert.equal(
    Services.search.defaultEngine,
    engine1,
    "Should keep the default engine in sync with the pref off"
  );

  await assertGleanDefaultEngine({
    normal: {
      engineId: "engine-rel-searchform-purpose",
    },
    private: {
      engineId: "engine-rel-searchform-purpose",
    },
  });
});

add_task(async function test_defaultPrivateEngine_ui_turned_off() {
  engine1.hidden = false;
  engine2.hidden = false;
  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault",
    true
  );

  Services.search.defaultEngine = engine2;
  Services.search.defaultPrivateEngine = engine1;

  await assertGleanDefaultEngine({
    normal: {
      engineId: "engine-chromeicon",
    },
    private: {
      engineId: "engine-rel-searchform-purpose",
    },
  });

  let promise = promiseDefaultNotification("private");
  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault.ui.enabled",
    false
  );
  Assert.equal(
    await promise,
    engine2,
    "Should have notified for resetting of the private pref."
  );

  await assertGleanDefaultEngine({
    normal: {
      engineId: "engine-chromeicon",
    },
    private: {
      engineId: "",
    },
  });

  promise = promiseDefaultNotification("normal");
  Services.search.defaultPrivateEngine = engine1;
  Assert.equal(
    await promise,
    engine1,
    "Should have notified setting the first engine correctly."
  );
  Assert.equal(
    Services.search.defaultPrivateEngine,
    engine1,
    "Should be set to the first engine correctly"
  );

  await assertGleanDefaultEngine({
    normal: {
      engineId: "engine-rel-searchform-purpose",
    },
    private: {
      engineId: "",
    },
  });
});

add_task(async function test_defaultPrivateEngine_same_engine_toggle_pref() {
  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault",
    true
  );
  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault.ui.enabled",
    true
  );

  // Set the normal and private engines to be the same
  Services.search.defaultEngine = engine2;
  Services.search.defaultPrivateEngine = engine2;

  await assertGleanDefaultEngine({
    normal: {
      engineId: "engine-chromeicon",
    },
    private: {
      engineId: "engine-chromeicon",
    },
  });

  // Disable pref
  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault",
    false
  );
  Assert.equal(
    Services.search.defaultPrivateEngine,
    engine2,
    "Should not change the default private engine"
  );
  Assert.equal(
    Services.search.defaultEngine,
    engine2,
    "Should not change the default engine"
  );

  await assertGleanDefaultEngine({
    normal: {
      engineId: "engine-chromeicon",
    },
    private: {
      engineId: "",
    },
  });

  // Re-enable pref
  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault",
    true
  );
  Assert.equal(
    Services.search.defaultPrivateEngine,
    engine2,
    "Should not change the default private engine"
  );
  Assert.equal(
    Services.search.defaultEngine,
    engine2,
    "Should not change the default engine"
  );

  await assertGleanDefaultEngine({
    normal: {
      engineId: "engine-chromeicon",
    },
    private: {
      engineId: "engine-chromeicon",
    },
  });
});

add_task(async function test_defaultPrivateEngine_same_engine_toggle_ui_pref() {
  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault",
    true
  );
  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault.ui.enabled",
    true
  );

  // Set the normal and private engines to be the same
  Services.search.defaultEngine = engine2;
  Services.search.defaultPrivateEngine = engine2;

  await assertGleanDefaultEngine({
    normal: {
      engineId: "engine-chromeicon",
    },
    private: {
      engineId: "engine-chromeicon",
    },
  });

  // Disable UI pref
  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault.ui.enabled",
    false
  );
  Assert.equal(
    Services.search.defaultPrivateEngine,
    engine2,
    "Should not change the default private engine"
  );
  Assert.equal(
    Services.search.defaultEngine,
    engine2,
    "Should not change the default engine"
  );

  await assertGleanDefaultEngine({
    normal: {
      engineId: "engine-chromeicon",
    },
    private: {
      engineId: "",
    },
  });

  // Re-enable UI pref
  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault.ui.enabled",
    true
  );
  Assert.equal(
    Services.search.defaultPrivateEngine,
    engine2,
    "Should not change the default private engine"
  );
  Assert.equal(
    Services.search.defaultEngine,
    engine2,
    "Should not change the default engine"
  );

  await assertGleanDefaultEngine({
    normal: {
      engineId: "engine-chromeicon",
    },
    private: {
      engineId: "engine-chromeicon",
    },
  });
});
