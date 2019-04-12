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
  await AddonTestUtils.promiseStartupManager();
});

async function addAndMakeDefault(name, searchURL) {
  await Services.search.addEngineWithDetails(name, null, null, null, "GET", searchURL);
  let engine = Services.search.getEngineByName(name);
  await Services.search.setDefault(engine);
  return engine;
}

add_task(async function test() {
  Assert.ok(!Services.search.isInitialized);
  let engineInfo;
  let engine;

  for (let [name, searchURL] of SUBMISSION_YES) {
    engine = await addAndMakeDefault(name, searchURL);
    engineInfo = await Services.search.getDefaultEngineInfo();
    Assert.equal(engineInfo.submissionURL, searchURL.replace("{searchTerms}", ""));
    await Services.search.removeEngine(engine);
  }

 for (let [name, searchURL] of SUBMISSION_NO) {
   engine = await addAndMakeDefault(name, searchURL);
   engineInfo = await Services.search.getDefaultEngineInfo();
   Assert.equal(engineInfo.submissionURL, null);
   await Services.search.removeEngine(engine);
 }
});
