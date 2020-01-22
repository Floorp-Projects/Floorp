/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test that defaultEngine property can be set and yields the proper events and\
 * behavior (search results)
 */

"use strict";

let engine1;
let engine2;
let originalDefault;
let originalPrivateDefault;

add_task(async function setup() {
  await useTestEngines();

  Services.prefs.setCharPref(SearchUtils.BROWSER_SEARCH_PREF + "region", "US");
  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault.ui.enabled",
    true
  );
  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault",
    true
  );

  useHttpServer();
  await AddonTestUtils.promiseStartupManager();

  await Services.search.init();

  originalDefault = Services.search.originalDefaultEngine;
  originalPrivateDefault = Services.search.originalPrivateDefaultEngine;
  engine1 = Services.search.getEngineByName("engine-rel-searchform-purpose");
  engine2 = Services.search.getEngineByName("engine-chromeicon");
});

add_task(async function test_defaultPrivateEngine() {
  Assert.equal(
    Services.search.defaultPrivateEngine,
    originalPrivateDefault,
    "Should have the original private default as the default private engine"
  );
  Assert.equal(
    Services.search.defaultEngine,
    originalDefault,
    "Should have the original default as the default engine"
  );

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
    originalDefault,
    "Should not have changed the original default engine"
  );
  promise = promiseDefaultNotification("private");
  await Services.search.setDefaultPrivate(engine2);
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
    originalDefault,
    "Should not have changed the original default engine"
  );

  promise = promiseDefaultNotification("private");
  await Services.search.setDefaultPrivate(engine1);
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

  engine1.hidden = true;
  Assert.equal(
    Services.search.defaultPrivateEngine,
    originalPrivateDefault,
    "Should reset to the original default private engine when hiding the default"
  );
  Assert.equal(
    Services.search.defaultEngine,
    originalDefault,
    "Should not have changed the original default engine"
  );

  engine1.hidden = false;
  Services.search.defaultEngine = engine1;
  Assert.equal(
    Services.search.defaultPrivateEngine,
    originalPrivateDefault,
    "Setting the default engine should not affect the private default"
  );

  Services.search.defaultEngine = originalDefault;
});

add_task(async function test_defaultPrivateEngine_turned_off() {
  Services.search.defaultEngine = originalDefault;
  Services.search.defaultPrivateEngine = engine1;

  let promise = promiseDefaultNotification("private");
  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault",
    false
  );
  Assert.equal(
    await promise,
    originalDefault,
    "Should have notified setting the first engine correctly."
  );

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
});
