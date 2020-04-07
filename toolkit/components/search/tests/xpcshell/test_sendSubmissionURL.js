/* Any copyright is dedicated to the Public Domain.
 *    http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests covering sending submission URLs for major engines
 */

const SUBMISSION_YES = new Map([
  ["Google1 Test", "https://www.google.com/search?q={searchTerms}"],
  ["Google2 Test", "https://www.google.co.uk/search?q={searchTerms}"],
  ["Yahoo1 Test", "https://search.yahoo.com/search?p={searchTerms}"],
  ["Yahoo2 Test", "https://uk.search.yahoo.com/search?p={searchTerms}"],
  ["AOL1 Test", "https://search.aol.com/aol/search?q={searchTerms}"],
  ["AOL2 Test", "https://search.aol.co.uk/aol/search?q={searchTerms}"],
  ["Yandex1 Test", "https://yandex.ru/search/?text={searchTerms}"],
  ["Yandex2 Test", "https://yandex.com/search/?text{searchTerms}"],
  ["Ask1 Test", "https://www.ask.com/web?q={searchTerms}"],
  ["Ask2 Test", "https://fr.ask.com/web?q={searchTerms}"],
  ["Bing Test", "https://www.bing.com/search?q={searchTerms}"],
  ["Startpage Test", "https://www.startpage.com/do/search?query={searchTerms}"],
  ["DuckDuckGo Test", "https://duckduckgo.com/?q={searchTerms}"],
  ["Baidu Test", "https://www.baidu.com/s?wd={searchTerms}"],
]);

const SUBMISSION_NO = new Map([
  ["Other1 Test", "https://example.com?q={searchTerms}"],
  ["Other2 Test", "https://googlebutnotgoogle.com?q={searchTerms}"],
]);

add_task(async function setup() {
  await useTestEngines("data1");
  installDistributionEngine();

  await AddonTestUtils.promiseStartupManager();
});

async function addAndMakeDefault(name, searchURL) {
  await Services.search.addEngineWithDetails(name, {
    method: "GET",
    template: searchURL,
  });
  let engine = Services.search.getEngineByName(name);
  await Services.search.setDefault(engine);
  return engine;
}

add_task(async function test_submission_url_matching() {
  Assert.ok(!Services.search.isInitialized);
  let engineInfo;
  let engine;

  for (let [name, searchURL] of SUBMISSION_YES) {
    engine = await addAndMakeDefault(name, searchURL);
    engineInfo = await Services.search.getDefaultEngineInfo();
    Assert.equal(
      engineInfo.defaultSearchEngineData.submissionURL,
      searchURL.replace("{searchTerms}", "")
    );
    await Services.search.removeEngine(engine);
  }

  for (let [name, searchURL] of SUBMISSION_NO) {
    engine = await addAndMakeDefault(name, searchURL);
    engineInfo = await Services.search.getDefaultEngineInfo();
    Assert.equal(engineInfo.defaultSearchEngineData.submissionURL, null);
    await Services.search.removeEngine(engine);
  }
});

add_task(async function test_submission_url_built_in() {
  const engine = await Services.search.getEngineByName("engine1");
  await Services.search.setDefault(engine);

  const engineInfo = await Services.search.getDefaultEngineInfo();
  Assert.equal(
    engineInfo.defaultSearchEngineData.submissionURL,
    "https://1.example.com/search?q=",
    "Should have given the submission url for a built-in engine."
  );
});

add_task(async function test_submission_url_distribution() {
  const engine = Services.search.getEngineByName("basic");
  await Services.search.setDefault(engine);

  const engineInfo = await Services.search.getDefaultEngineInfo();
  Assert.equal(
    engineInfo.defaultSearchEngineData.submissionURL,
    "http://searchtest.local/?search=",
    "Should have given the submission url for a distribution engine."
  );
});

add_task(async function test_submission_url_promoted_by_preference() {
  const engine = await Services.search.addEngineWithDetails("fakeEngine", {
    method: "GET",
    template: "https://fake.example.com/?q={searchTerms}",
  });

  await Services.search.setDefault(engine);

  let engineInfo = await Services.search.getDefaultEngineInfo();
  Assert.equal(
    engineInfo.defaultSearchEngineData.submissionURL,
    undefined,
    "Should not have a submission url for a user-based engine"
  );

  Services.prefs.setCharPref(
    SearchUtils.BROWSER_SEARCH_PREF + "order.extra.1",
    "fakeEngine"
  );

  engineInfo = await Services.search.getDefaultEngineInfo();
  Assert.equal(
    engineInfo.defaultSearchEngineData.submissionURL,
    "https://fake.example.com/?q=",
    "Should have a submission URL for a pref-promoted user-based engine (extra)"
  );

  Services.prefs.clearUserPref(
    SearchUtils.BROWSER_SEARCH_PREF + "order.extra.1"
  );
  let localizedStr = Cc["@mozilla.org/pref-localizedstring;1"].createInstance(
    Ci.nsIPrefLocalizedString
  );
  localizedStr.data = "fakeEngine";
  Services.prefs.setComplexValue(
    SearchUtils.BROWSER_SEARCH_PREF + "order.1",
    Ci.nsIPrefLocalizedString,
    localizedStr
  );
  Assert.equal(
    engineInfo.defaultSearchEngineData.submissionURL,
    "https://fake.example.com/?q=",
    "Should have a submission URL for a pref-promoted user-based engine"
  );
});
