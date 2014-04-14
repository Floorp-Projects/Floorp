/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Cu.import("resource://gre/modules/PriorityUrlProvider.jsm");

function run_test() {
  run_next_test();
}

add_task(function* search_engine_match() {
  let engine = yield promiseDefaultSearchEngine();
  let token = engine.getResultDomain();
  let match = yield PriorityUrlProvider.getMatch(token.substr(0, 1));
  do_check_eq(match.url, engine.searchForm);
  do_check_eq(match.title, engine.name);
  do_check_eq(match.iconUrl, engine.iconURI ? engine.iconURI.spec : null);
  do_check_eq(match.reason, "search");
});

add_task(function* no_match() {
  do_check_eq(null, yield PriorityUrlProvider.getMatch("test"));
});

add_task(function* hide_search_engine_nomatch() {
  let engine = yield promiseDefaultSearchEngine();
  let token = engine.getResultDomain();
  let promiseTopic = promiseSearchTopic("engine-changed");
  Services.search.removeEngine(engine);
  yield promiseTopic;
  do_check_true(engine.hidden);
  do_check_eq(null, yield PriorityUrlProvider.getMatch(token.substr(0, 1)));
});

add_task(function* add_search_engine_match() {
  let promiseTopic = promiseSearchTopic("engine-added");
  do_check_eq(null, yield PriorityUrlProvider.getMatch("bacon"));
  Services.search.addEngineWithDetails("bacon", "", "bacon", "Search Bacon",
                                       "GET", "http://www.bacon.moz/?search={searchTerms}");
  yield promiseSearchTopic;
  let match = yield PriorityUrlProvider.getMatch("bacon");
  do_check_eq(match.url, "http://www.bacon.moz");
  do_check_eq(match.title, "bacon");
  do_check_eq(match.iconUrl, null);
  do_check_eq(match.reason, "search");
});

add_task(function* remove_search_engine_nomatch() {
  let engine = Services.search.getEngineByName("bacon");
  let promiseTopic = promiseSearchTopic("engine-removed");
  Services.search.removeEngine(engine);
  yield promiseTopic;
  do_check_eq(null, yield PriorityUrlProvider.getMatch("bacon"));
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
    do_log_info("browser-search-engine-modified: " + verb);
    if (verb == expectedVerb) {
      Services.obs.removeObserver(observe, "browser-search-engine-modified");
      deferred.resolve();
    }
  }, "browser-search-engine-modified", false);
  return deferred.promise;
}
