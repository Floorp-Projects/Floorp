/* Any copyright is dedicated to the Public Domain.
 *    http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests covering sending submission URLs for major engines
 */

const SUBMISSION_YES = [
  ["Google1 Test", "https://www.google.com/search", "q={searchTerms}"],
  ["Google2 Test", "https://www.google.co.uk/search", "q={searchTerms}"],
  ["Yahoo1 Test", "https://search.yahoo.com/search", "p={searchTerms}"],
  ["Yahoo2 Test", "https://uk.search.yahoo.com/search", "p={searchTerms}"],
  ["AOL1 Test", "https://search.aol.com/aol/search", "q={searchTerms}"],
  ["AOL2 Test", "https://search.aol.co.uk/aol/search", "q={searchTerms}"],
  ["Yandex1 Test", "https://yandex.ru/search/", "text={searchTerms}"],
  ["Yandex2 Test", "https://yandex.com/search/", "text={searchTerms}"],
  ["Ask1 Test", "https://www.ask.com/web", "q={searchTerms}"],
  ["Ask2 Test", "https://fr.ask.com/web", "q={searchTerms}"],
  ["Bing Test", "https://www.bing.com/search", "q={searchTerms}"],
  [
    "Startpage Test",
    "https://www.startpage.com/do/search",
    "query={searchTerms}",
  ],
  ["DuckDuckGo Test", "https://duckduckgo.com/", "q={searchTerms}"],
  ["Baidu Test", "https://www.baidu.com/s", "wd={searchTerms}"],
];

const SUBMISSION_NO = [
  ["Other1 Test", "https://example.com", "q={searchTerms}"],
  ["Other2 Test", "https://googlebutnotgoogle.com", "q={searchTerms}"],
];

add_setup(async function () {
  await SearchTestUtils.useTestEngines("data1");
  await AddonTestUtils.promiseStartupManager();
});

async function addAndMakeDefault(name, search_url, search_url_get_params) {
  await SearchTestUtils.installSearchExtension({
    name,
    search_url,
    search_url_get_params,
  });

  let engine = Services.search.getEngineByName(name);
  await Services.search.setDefault(
    engine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );
  return engine;
}

add_task(async function test_submission_url_matching() {
  Assert.ok(!Services.search.isInitialized);
  let engineInfo;
  let engine;

  for (let [name, searchURL, searchParams] of SUBMISSION_YES) {
    engine = await addAndMakeDefault(name, searchURL, searchParams);
    engineInfo = Services.search.getDefaultEngineInfo();
    Assert.equal(
      engineInfo.defaultSearchEngineData.submissionURL,
      (searchURL + "?" + searchParams).replace("{searchTerms}", "")
    );
    await Services.search.removeEngine(engine);
  }

  for (let [name, searchURL, searchParams] of SUBMISSION_NO) {
    engine = await addAndMakeDefault(name, searchURL, searchParams);
    engineInfo = Services.search.getDefaultEngineInfo();
    Assert.equal(engineInfo.defaultSearchEngineData.submissionURL, null);
    await Services.search.removeEngine(engine);
  }
});

add_task(async function test_submission_url_built_in() {
  const engine = await Services.search.getEngineByName("engine1");
  await Services.search.setDefault(
    engine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );

  const engineInfo = Services.search.getDefaultEngineInfo();
  Assert.equal(
    engineInfo.defaultSearchEngineData.submissionURL,
    "https://1.example.com/search?q=",
    "Should have given the submission url for a built-in engine."
  );
});
