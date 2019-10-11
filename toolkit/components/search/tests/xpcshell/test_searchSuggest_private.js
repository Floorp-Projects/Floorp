/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that search suggestions from SearchSuggestionController.jsm operate
 * correctly in private mode.
 */

"use strict";

const { SearchSuggestionController } = ChromeUtils.import(
  "resource://gre/modules/SearchSuggestionController.jsm"
);

let engine;

add_task(async function setup() {
  Services.prefs.setBoolPref("browser.search.suggest.enabled", true);

  let server = useHttpServer();
  server.registerContentType("sjs", "sjs");

  await AddonTestUtils.promiseStartupManager();

  const engineData = {
    baseURL: gDataUrl,
    name: "GET suggestion engine",
    method: "GET",
  };

  [engine] = await addTestEngines([
    {
      name: engineData.name,
      xmlFileName: "engineMaker.sjs?" + JSON.stringify(engineData),
    },
  ]);
});

add_task(async function test_suggestions_in_private_mode_enabled() {
  Services.prefs.setBoolPref("browser.search.suggest.enabled.private", true);

  let controller = new SearchSuggestionController();
  controller.maxLocalResults = 0;
  controller.maxRemoteResults = 1;
  let result = await controller.fetch("mo", true, engine);
  Assert.equal(result.remote.length, 1);
});

add_task(async function test_suggestions_in_private_mode_disabled() {
  Services.prefs.setBoolPref("browser.search.suggest.enabled.private", false);

  let controller = new SearchSuggestionController();
  controller.maxLocalResults = 0;
  controller.maxRemoteResults = 1;
  let result = await controller.fetch("mo", true, engine);
  Assert.equal(result.remote.length, 0);
});
