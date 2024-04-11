/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that ensure application provided engines have all base fields set up
 * correctly from the search configuration.
 */

"use strict";

let CONFIG = [
  {
    identifier: "testEngine",
    recordType: "engine",
    base: {
      aliases: ["testEngine1", "testEngine2"],
      charset: "EUC-JP",
      classification: "general",
      name: "testEngine name",
      partnerCode: "pc",
      urls: {
        search: {
          base: "https://example.com/1",
          // Method defaults to GET
          params: [
            { name: "partnerCode", value: "{partnerCode}" },
            { name: "starbase", value: "Regula I" },
            { name: "experiment", value: "Genesis" },
            {
              name: "accessPoint",
              searchAccessPoint: {
                addressbar: "addressbar",
                contextmenu: "contextmenu",
                homepage: "homepage",
                newtab: "newtab",
                searchbar: "searchbar",
              },
            },
          ],
          searchTermParamName: "search",
        },
        suggestions: {
          base: "https://example.com/2",
          method: "POST",
          searchTermParamName: "suggestions",
        },
        trending: {
          base: "https://example.com/3",
          searchTermParamName: "trending",
        },
      },
    },
    variants: [{ environment: { allRegionsAndLocales: true } }],
  },
  {
    identifier: "testOtherValuesEngine",
    recordType: "engine",
    base: {
      classification: "unknown",
      name: "testOtherValuesEngine name",
      urls: {
        search: {
          base: "https://example.com/1",
          searchTermParamName: "search",
        },
      },
    },
    variants: [{ environment: { allRegionsAndLocales: true } }],
  },
  {
    recordType: "defaultEngines",
    globalDefault: "engine_no_initial_icon",
    specificDefaults: [],
  },
  {
    recordType: "engineOrders",
    orders: [],
  },
];

add_setup(async function () {
  await SearchTestUtils.useTestEngines("simple-engines", null, CONFIG);
  await Services.search.init();
});

add_task(async function test_engine_with_all_params_set() {
  let engine = Services.search.getEngineById(
    "testEngine@search.mozilla.orgdefault"
  );
  Assert.ok(engine, "Should have found the engine");

  Assert.equal(
    engine.name,
    "testEngine name",
    "Should have the correct engine name"
  );
  Assert.deepEqual(
    engine.aliases,
    ["@testEngine1", "@testEngine2"],
    "Should have the correct aliases"
  );
  Assert.ok(
    engine.isGeneralPurposeEngine,
    "Should be a general purpose engine"
  );
  Assert.equal(
    engine.wrappedJSObject.queryCharset,
    "EUC-JP",
    "Should have the correct encoding"
  );

  let submission = engine.getSubmission("test");
  Assert.equal(
    submission.uri.spec,
    "https://example.com/1?partnerCode=pc&starbase=Regula%20I&experiment=Genesis&accessPoint=searchbar&search=test",
    "Should have the correct search URL"
  );
  Assert.ok(!submission.postData, "Should not have postData for a GET url");

  let suggestSubmission = engine.getSubmission(
    "test",
    SearchUtils.URL_TYPE.SUGGEST_JSON
  );
  Assert.equal(
    suggestSubmission.uri.spec,
    "https://example.com/2",
    "Should have the correct suggestion URL"
  );
  Assert.equal(
    suggestSubmission.postData.data.data,
    "suggestions=test",
    "Should have the correct postData for a POST URL"
  );

  let trendingSubmission = engine.getSubmission(
    "test",
    SearchUtils.URL_TYPE.TRENDING_JSON
  );
  Assert.equal(
    trendingSubmission.uri.spec,
    "https://example.com/3?trending=test"
  );
  Assert.ok(!submission.postData, "Should not have postData for a GET url");
});

add_task(async function test_engine_with_some_params_set() {
  let engine = Services.search.getEngineById(
    "testOtherValuesEngine@search.mozilla.orgdefault"
  );
  Assert.ok(engine, "Should have found the engine");

  Assert.equal(
    engine.name,
    "testOtherValuesEngine name",
    "Should have the correct engine name"
  );
  Assert.deepEqual(engine.aliases, [], "Should have no aliases");
  Assert.ok(
    !engine.isGeneralPurposeEngine,
    "Should not be a general purpose engine"
  );
  Assert.equal(
    engine.wrappedJSObject.queryCharset,
    "UTF-8",
    "Should default to UTF-8 charset"
  );
  Assert.equal(
    engine.getSubmission("test").uri.spec,
    "https://example.com/1?search=test",
    "Should have the correct search URL"
  );
  Assert.equal(
    engine.getSubmission("test", SearchUtils.URL_TYPE.SUGGEST_JSON),
    null,
    "Should not have a suggestions URL"
  );
  Assert.equal(
    engine.getSubmission("test", SearchUtils.URL_TYPE.TRENDING_JSON),
    null,
    "Should not have a trending URL"
  );
});
