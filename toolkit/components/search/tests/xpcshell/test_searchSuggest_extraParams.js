/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_CONFIG = [
  {
    webExtension: {
      id: "get@search.mozilla.org",
      name: "Get Engine",
      search_url: "https://example.com",
      search_url_get_params: "webExtension=1&search={searchTerms}",
      suggest_url: "https://example.com",
      suggest_url_get_params: "webExtension=1&suggest={searchTerms}",
    },
    appliesTo: [{ included: { everywhere: true } }],
    suggestExtraParams: [
      {
        name: "custom_param",
        pref: "test_pref_param",
        condition: "pref",
      },
    ],
  },
];

const TEST_CONFIG_V2 = [
  {
    recordType: "engine",
    identifier: "get",
    base: {
      name: "Get Engine",
      urls: {
        search: {
          base: "https://example.com",
          params: [
            {
              name: "webExtension",
              value: "1",
            },
          ],
          searchTermParamName: "search",
        },
        suggestions: {
          base: "https://example.com",
          params: [
            {
              name: "custom_param",
              experimentConfig: "test_pref_param",
            },
            {
              name: "webExtension",
              value: "1",
            },
          ],
          searchTermParamName: "suggest",
        },
      },
    },
    variants: [
      {
        environment: { allRegionsAndLocales: true },
      },
    ],
  },
  {
    recordType: "defaultEngines",
    globalDefault: "get",
    specificDefaults: [],
  },
  {
    recordType: "engineOrders",
    orders: [],
  },
];

add_setup(async function () {
  await SearchTestUtils.useTestEngines(
    "method-extensions",
    null,
    SearchUtils.newSearchConfigEnabled ? TEST_CONFIG_V2 : TEST_CONFIG
  );
  await AddonTestUtils.promiseStartupManager();
  await Services.search.init();
});

add_task(async function test_custom_suggest_param() {
  let engine = Services.search.getEngineByName("Get Engine");
  Assert.notEqual(engine, null, "Should have found an engine");

  let submissionSuggest = engine.getSubmission(
    "bar",
    SearchUtils.URL_TYPE.SUGGEST_JSON
  );
  Assert.equal(
    submissionSuggest.uri.spec,
    "https://example.com/?webExtension=1&suggest=bar",
    "Suggest URLs should match"
  );

  let defaultBranch = Services.prefs.getDefaultBranch("browser.search.");
  defaultBranch.setCharPref("param.test_pref_param", "good");

  let nextSubmissionSuggest = engine.getSubmission(
    "bar",
    SearchUtils.URL_TYPE.SUGGEST_JSON
  );
  Assert.equal(
    nextSubmissionSuggest.uri.spec,
    "https://example.com/?custom_param=good&webExtension=1&suggest=bar",
    "Suggest URLs should include custom param"
  );
});
