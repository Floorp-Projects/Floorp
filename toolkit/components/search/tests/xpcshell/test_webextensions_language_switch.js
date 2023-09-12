/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { promiseShutdownManager, promiseStartupManager } = AddonTestUtils;

add_setup(async function () {
  Services.locale.availableLocales = [
    ...Services.locale.availableLocales,
    "en",
    "de",
    "fr",
  ];
  Services.locale.requestedLocales = ["en"];

  await SearchTestUtils.useTestEngines("data1");
  await promiseStartupManager();
  await Services.search.init();
  await promiseAfterSettings();

  registerCleanupFunction(promiseShutdownManager);
});

add_task(async function test_language_switch_changes_name() {
  await SearchTestUtils.installSearchExtension(
    {
      name: "__MSG_engineName__",
      id: "engine@tests.mozilla.org",
      search_url_get_params: `q={searchTerms}&version=1.0`,
      default_locale: "en",
      version: "1.0",
    },
    { skipUnload: false },
    {
      "_locales/en/messages.json": {
        engineName: {
          message: "English Name",
          description: "The Name",
        },
      },
      "_locales/fr/messages.json": {
        engineName: {
          message: "French Name",
          description: "The Name",
        },
      },
    }
  );

  let engine = Services.search.getEngineById("engine@tests.mozilla.orgdefault");
  Assert.ok(!!engine, "Should have loaded the engine");
  Assert.equal(
    engine.name,
    "English Name",
    "Should have loaded the English version of the name"
  );

  await Services.search.setDefault(
    engine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );

  let promiseChanged = TestUtils.topicObserved(
    "browser-search-engine-modified",
    (eng, verb) => verb == "engine-changed"
  );

  await promiseSetLocale("fr");

  await promiseChanged;

  engine = Services.search.getEngineById("engine@tests.mozilla.orgdefault");
  Assert.ok(!!engine, "Should still be available");
  Assert.equal(
    engine.name,
    "French Name",
    "Should have updated to the French version of the name"
  );

  Assert.equal(
    (await Services.search.getDefault()).id,
    engine.id,
    "Should have kept the default engine the same"
  );

  promiseChanged = TestUtils.topicObserved(
    "browser-search-engine-modified",
    (eng, verb) => verb == "engine-changed"
  );

  // Check for changing to a locale the add-on doesn't have.
  await promiseSetLocale("de");

  await promiseChanged;

  engine = Services.search.getEngineById("engine@tests.mozilla.orgdefault");
  Assert.ok(!!engine, "Should still be available");
  Assert.equal(
    engine.name,
    "English Name",
    "Should have fallen back to the default locale (English) version of the name"
  );

  Assert.equal(
    (await Services.search.getDefault()).id,
    engine.id,
    "Should have kept the default engine the same"
  );
});
