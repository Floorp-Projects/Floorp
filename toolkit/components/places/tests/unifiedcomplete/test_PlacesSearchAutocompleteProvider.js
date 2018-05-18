/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/PlacesSearchAutocompleteProvider.jsm");

add_task(async function() {
    // Tell the search service we are running in the US.  This also has the
    // desired side-effect of preventing our geoip lookup.
   Services.prefs.setCharPref("browser.search.countryCode", "US");
   Services.prefs.setBoolPref("browser.search.geoSpecificDefaults", false);

   Services.search.restoreDefaultEngines();
   Services.search.resetToOriginalDefaultEngine();
});

add_task(async function search_engine_match() {
  let engine = await promiseDefaultSearchEngine();
  let token = engine.getResultDomain();
  let match = await PlacesSearchAutocompleteProvider.findMatchByToken(token.substr(0, 1));
  Assert.equal(match.url, engine.searchForm);
  Assert.equal(match.engineName, engine.name);
  Assert.equal(match.iconUrl, engine.iconURI ? engine.iconURI.spec : null);
});

add_task(async function no_match() {
  Assert.equal(null, await PlacesSearchAutocompleteProvider.findMatchByToken("test"));
});

add_task(async function hide_search_engine_nomatch() {
  let engine = await promiseDefaultSearchEngine();
  let token = engine.getResultDomain();
  let promiseTopic = promiseSearchTopic("engine-changed");
  Services.search.removeEngine(engine);
  await promiseTopic;
  Assert.ok(engine.hidden);
  let match = await PlacesSearchAutocompleteProvider.findMatchByToken(token.substr(0, 1));
  Assert.ok(!match || match.token != token);
});

add_task(async function add_search_engine_match() {
  let promiseTopic = promiseSearchTopic("engine-added");
  Assert.equal(null, await PlacesSearchAutocompleteProvider.findMatchByToken("bacon"));
  Services.search.addEngineWithDetails("bacon", "", "pork", "Search Bacon",
                                       "GET", "http://www.bacon.moz/?search={searchTerms}");
  await promiseTopic;
  let match = await PlacesSearchAutocompleteProvider.findMatchByToken("bacon");
  Assert.equal(match.url, "http://www.bacon.moz");
  Assert.equal(match.engineName, "bacon");
  Assert.equal(match.iconUrl, null);
});

add_task(async function test_aliased_search_engine_match() {
  Assert.equal(null, await PlacesSearchAutocompleteProvider.findMatchByAlias("sober"));
  // Lower case
  let match = await PlacesSearchAutocompleteProvider.findMatchByAlias("pork");
  Assert.equal(match.engineName, "bacon");
  Assert.equal(match.alias, "pork");
  Assert.equal(match.iconUrl, null);
  // Upper case
  let match1 = await PlacesSearchAutocompleteProvider.findMatchByAlias("PORK");
  Assert.equal(match1.engineName, "bacon");
  Assert.equal(match1.alias, "pork");
  Assert.equal(match1.iconUrl, null);
  // Cap case
  let match2 = await PlacesSearchAutocompleteProvider.findMatchByAlias("Pork");
  Assert.equal(match2.engineName, "bacon");
  Assert.equal(match2.alias, "pork");
  Assert.equal(match2.iconUrl, null);
});

add_task(async function test_aliased_search_engine_match_upper_case_alias() {
  let promiseTopic = promiseSearchTopic("engine-added");
  Assert.equal(null, await PlacesSearchAutocompleteProvider.findMatchByToken("patch"));
  Services.search.addEngineWithDetails("patch", "", "PR", "Search Patch",
                                       "GET", "http://www.patch.moz/?search={searchTerms}");
  await promiseTopic;
  // lower case
  let match = await PlacesSearchAutocompleteProvider.findMatchByAlias("pr");
  Assert.equal(match.engineName, "patch");
  Assert.equal(match.alias, "PR");
  Assert.equal(match.iconUrl, null);
  // Upper case
  let match1 = await PlacesSearchAutocompleteProvider.findMatchByAlias("PR");
  Assert.equal(match1.engineName, "patch");
  Assert.equal(match1.alias, "PR");
  Assert.equal(match1.iconUrl, null);
  // Cap case
  let match2 = await PlacesSearchAutocompleteProvider.findMatchByAlias("Pr");
  Assert.equal(match2.engineName, "patch");
  Assert.equal(match2.alias, "PR");
  Assert.equal(match2.iconUrl, null);
});

add_task(async function remove_search_engine_nomatch() {
  let engine = Services.search.getEngineByName("bacon");
  let promiseTopic = promiseSearchTopic("engine-removed");
  Services.search.removeEngine(engine);
  await promiseTopic;
  Assert.equal(null, await PlacesSearchAutocompleteProvider.findMatchByToken("bacon"));
});

add_task(async function test_parseSubmissionURL_basic() {
  // Most of the logic of parseSubmissionURL is tested in the search service
  // itself, thus we only do a sanity check of the wrapper here.
  let engine = await promiseDefaultSearchEngine();
  let submissionURL = engine.getSubmission("terms").uri.spec;

  let result = PlacesSearchAutocompleteProvider.parseSubmissionURL(submissionURL);
  Assert.equal(result.engineName, engine.name);
  Assert.equal(result.terms, "terms");

  result = PlacesSearchAutocompleteProvider.parseSubmissionURL("http://example.org/");
  Assert.equal(result, null);
});

function promiseDefaultSearchEngine() {
  return new Promise(resolve => {
    Services.search.init( () => {
      resolve(Services.search.defaultEngine);
    });
  });
}

function promiseSearchTopic(expectedVerb) {
  return new Promise(resolve => {
    Services.obs.addObserver( function observe(subject, topic, verb) {
      info("browser-search-engine-modified: " + verb);
      if (verb == expectedVerb) {
        Services.obs.removeObserver(observe, "browser-search-engine-modified");
        resolve();
      }
    }, "browser-search-engine-modified");
  });
}
