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
  useTestEngineConfig();

  Services.prefs.setCharPref(SearchUtils.BROWSER_SEARCH_PREF + "region", "US");
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

  Services.search.defaultPrivateEngine = engine1;
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
  await Services.search.setDefaultPrivate(engine2);
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

  await Services.search.setDefaultPrivate(engine1);
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
  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault",
    false
  );

  Services.search.defaultPrivateEngine = engine1;
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
  Services.search.defaultPrivateEngine = engine2;
  Assert.equal(
    Services.search.defaultPrivateEngine,
    engine2,
    "Should be set to the second engine correctly"
  );
  Assert.equal(
    Services.search.defaultEngine,
    engine2,
    "Should keep the default engine in sync with the pref off"
  );
  Services.search.defaultPrivateEngine = engine1;
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

  // Test that hiding the currently-default engine affects the defaultEngine getter
  // We fallback first to the original default...
  engine1.hidden = true;
  Assert.equal(
    Services.search.defaultPrivateEngine,
    originalDefault,
    "Should reset to the original engine"
  );
  Assert.equal(
    Services.search.defaultEngine,
    originalDefault,
    "Should also reset the normal default to the original engine"
  );

  // ... and then to the first visible engine in the list, so move our second
  // engine to that position.
  await Services.search.moveEngine(engine2, 0);
  originalDefault.hidden = true;
  Assert.equal(
    Services.search.defaultPrivateEngine,
    engine2,
    "Should correctly set the second engine as private default"
  );
  Assert.equal(
    Services.search.defaultEngine,
    engine2,
    "Should also set the normal default to the second engine"
  );

  // Test that setting defaultPrivateEngine to an already-hidden engine works, but
  // doesn't change the return value of the getter
  Services.search.defaultPrivateEngine = engine1;
  Assert.equal(
    Services.search.defaultPrivateEngine,
    engine2,
    "Should not change anything if attempted to be set to a hidden engine"
  );
  Assert.equal(
    Services.search.defaultEngine,
    engine2,
    "Should also keep the normal default if attempted to be set to a hidden engine"
  );
});
