/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Cu.import("resource://gre/modules/PlacesSearchAutocompleteProvider.jsm");

function run_test() {
  // Tell the search service we are running in the US.  This also has the
  // desired side-effect of preventing our geoip lookup.
  Services.prefs.setBoolPref("browser.search.isUS", true);
  Services.prefs.setCharPref("browser.search.countryCode", "US");
  Services.prefs.setBoolPref("browser.search.geoSpecificDefaults", false);
  run_next_test();
}

add_task(function* search_engine_match() {
  let engine = yield promiseDefaultSearchEngine();
  let token = engine.getResultDomain();
  let match = yield PlacesSearchAutocompleteProvider.findMatchByToken(token.substr(0, 1));
  do_check_eq(match.url, engine.searchForm);
  do_check_eq(match.engineName, engine.name);
  do_check_eq(match.iconUrl, engine.iconURI ? engine.iconURI.spec : null);
});

add_task(function* no_match() {
  do_check_eq(null, yield PlacesSearchAutocompleteProvider.findMatchByToken("test"));
});

add_task(function* hide_search_engine_nomatch() {
  let engine = yield promiseDefaultSearchEngine();
  let token = engine.getResultDomain();
  let promiseTopic = promiseSearchTopic("engine-changed");
  Services.search.removeEngine(engine);
  yield promiseTopic;
  do_check_true(engine.hidden);
  do_check_eq(null, yield PlacesSearchAutocompleteProvider.findMatchByToken(token.substr(0, 1)));
});

add_task(function* add_search_engine_match() {
  let promiseTopic = promiseSearchTopic("engine-added");
  do_check_eq(null, yield PlacesSearchAutocompleteProvider.findMatchByToken("bacon"));
  Services.search.addEngineWithDetails("bacon", "", "pork", "Search Bacon",
                                       "GET", "http://www.bacon.moz/?search={searchTerms}");
  yield promiseTopic;
  let match = yield PlacesSearchAutocompleteProvider.findMatchByToken("bacon");
  do_check_eq(match.url, "http://www.bacon.moz");
  do_check_eq(match.engineName, "bacon");
  do_check_eq(match.iconUrl, null);
});

add_task(function* test_aliased_search_engine_match() {
  do_check_eq(null, yield PlacesSearchAutocompleteProvider.findMatchByAlias("sober"));
  // Lower case
  let match = yield PlacesSearchAutocompleteProvider.findMatchByAlias("pork");
  do_check_eq(match.engineName, "bacon");
  do_check_eq(match.alias, "pork");
  do_check_eq(match.iconUrl, null);
  // Upper case
  let match1 = yield PlacesSearchAutocompleteProvider.findMatchByAlias("PORK");
  do_check_eq(match1.engineName, "bacon");
  do_check_eq(match1.alias, "pork");
  do_check_eq(match1.iconUrl, null);
  // Cap case
  let match2 = yield PlacesSearchAutocompleteProvider.findMatchByAlias("Pork");
  do_check_eq(match2.engineName, "bacon");
  do_check_eq(match2.alias, "pork");
  do_check_eq(match2.iconUrl, null);
});

add_task(function* test_aliased_search_engine_match_upper_case_alias() {
  let promiseTopic = promiseSearchTopic("engine-added");
  do_check_eq(null, yield PlacesSearchAutocompleteProvider.findMatchByToken("patch"));
  Services.search.addEngineWithDetails("patch", "", "PR", "Search Patch",
                                       "GET", "http://www.patch.moz/?search={searchTerms}");
  yield promiseTopic;
  // lower case
  let match = yield PlacesSearchAutocompleteProvider.findMatchByAlias("pr");
  do_check_eq(match.engineName, "patch");
  do_check_eq(match.alias, "PR");
  do_check_eq(match.iconUrl, null);
  // Upper case
  let match1 = yield PlacesSearchAutocompleteProvider.findMatchByAlias("PR");
  do_check_eq(match1.engineName, "patch");
  do_check_eq(match1.alias, "PR");
  do_check_eq(match1.iconUrl, null);
  // Cap case
  let match2 = yield PlacesSearchAutocompleteProvider.findMatchByAlias("Pr");
  do_check_eq(match2.engineName, "patch");
  do_check_eq(match2.alias, "PR");
  do_check_eq(match2.iconUrl, null);
});

add_task(function* remove_search_engine_nomatch() {
  let engine = Services.search.getEngineByName("bacon");
  let promiseTopic = promiseSearchTopic("engine-removed");
  Services.search.removeEngine(engine);
  yield promiseTopic;
  do_check_eq(null, yield PlacesSearchAutocompleteProvider.findMatchByToken("bacon"));
});

add_task(function* test_parseSubmissionURL_basic() {
  // Most of the logic of parseSubmissionURL is tested in the search service
  // itself, thus we only do a sanity check of the wrapper here.
  let engine = yield promiseDefaultSearchEngine();
  let submissionURL = engine.getSubmission("terms").uri.spec;

  let result = PlacesSearchAutocompleteProvider.parseSubmissionURL(submissionURL);
  do_check_eq(result.engineName, engine.name);
  do_check_eq(result.terms, "terms");

  result = PlacesSearchAutocompleteProvider.parseSubmissionURL("http://example.org/");
  do_check_eq(result, null);
});

function promiseDefaultSearchEngine() {
  let deferred = Promise.defer();
  Services.search.init( () => {
    deferred.resolve(Services.search.defaultEngine);
  });
  return deferred.promise;
}

function promiseSearchTopic(expectedVerb) {
  let deferred = Promise.defer();
  Services.obs.addObserver( function observe(subject, topic, verb) {
    do_print("browser-search-engine-modified: " + verb);
    if (verb == expectedVerb) {
      Services.obs.removeObserver(observe, "browser-search-engine-modified");
      deferred.resolve();
    }
  }, "browser-search-engine-modified", false);
  return deferred.promise;
}
