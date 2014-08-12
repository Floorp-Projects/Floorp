/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Testing search suggestions from SearchSuggestionController.jsm.
 */

"use strict";

Cu.import("resource://gre/modules/FormHistory.jsm");
Cu.import("resource://gre/modules/SearchSuggestionController.jsm");
Cu.import("resource://gre/modules/Timer.jsm");

let httpServer = new HttpServer();
let getEngine, postEngine, unresolvableEngine;

function run_test() {
  removeMetadata();
  updateAppInfo();

  let httpServer = useHttpServer();
  httpServer.registerContentType("sjs", "sjs");

  do_register_cleanup(() => Task.spawn(function* cleanup() {
    // Remove added form history entries
    yield updateSearchHistory("remove", null);
    FormHistory.shutdown();
  }));

  run_next_test();
}

add_task(function* add_test_engines() {
  let getEngineData = {
    baseURL: gDataUrl,
    engineType: Ci.nsISearchEngine.TYPE_OPENSEARCH,
    name: "GET suggestion engine",
    method: "GET",
  };

  let postEngineData = {
    baseURL: gDataUrl,
    engineType: Ci.nsISearchEngine.TYPE_OPENSEARCH,
    name: "POST suggestion engine",
    method: "POST",
  };

  let unresolvableEngineData = {
    baseURL: "http://example.invalid/",
    engineType: Ci.nsISearchEngine.TYPE_OPENSEARCH,
    name: "Offline suggestion engine",
    method: "GET",
  };

  [getEngine, postEngine, unresolvableEngine] = yield addTestEngines([
    {
      name: getEngineData.name,
      xmlFileName: "engineMaker.sjs?" + JSON.stringify(getEngineData),
    },
    {
      name: postEngineData.name,
      xmlFileName: "engineMaker.sjs?" + JSON.stringify(postEngineData),
    },
    {
      name: unresolvableEngineData.name,
      xmlFileName: "engineMaker.sjs?" + JSON.stringify(unresolvableEngineData),
    },
  ]);
});


// Begin tests

add_task(function* simple_no_result_callback() {
  let deferred = Promise.defer();
  let controller = new SearchSuggestionController((result) => {
    do_check_eq(result.term, "no remote");
    do_check_eq(result.local.length, 0);
    do_check_eq(result.remote.length, 0);
    deferred.resolve();
  });

  controller.fetch("no remote", false, getEngine);
  yield deferred.promise;
});

add_task(function* simple_no_result_callback_and_promise() {
  // Make sure both the callback and promise get results
  let deferred = Promise.defer();
  let controller = new SearchSuggestionController((result) => {
    do_check_eq(result.term, "no results");
    do_check_eq(result.local.length, 0);
    do_check_eq(result.remote.length, 0);
    deferred.resolve();
  });

  let result = yield controller.fetch("no results", false, getEngine);
  do_check_eq(result.term, "no results");
  do_check_eq(result.local.length, 0);
  do_check_eq(result.remote.length, 0);

  yield deferred.promise;
});

add_task(function* simple_no_result_promise() {
  let controller = new SearchSuggestionController();
  let result = yield controller.fetch("no remote", false, getEngine);
  do_check_eq(result.term, "no remote");
  do_check_eq(result.local.length, 0);
  do_check_eq(result.remote.length, 0);
});

add_task(function* simple_remote_no_local_result() {
  let controller = new SearchSuggestionController();
  let result = yield controller.fetch("mo", false, getEngine);
  do_check_eq(result.term, "mo");
  do_check_eq(result.local.length, 0);
  do_check_eq(result.remote.length, 3);
  do_check_eq(result.remote[0], "Mozilla");
  do_check_eq(result.remote[1], "modern");
  do_check_eq(result.remote[2], "mom");
});

add_task(function* simple_local_no_remote_result() {
  yield updateSearchHistory("bump", "no remote entries");

  let controller = new SearchSuggestionController();
  let result = yield controller.fetch("no remote", false, getEngine);
  do_check_eq(result.term, "no remote");
  do_check_eq(result.local.length, 1);
  do_check_eq(result.local[0], "no remote entries");
  do_check_eq(result.remote.length, 0);

  yield updateSearchHistory("remove", "no remote entries");
});

add_task(function* simple_non_ascii() {
  yield updateSearchHistory("bump", "I ❤️ XUL");

  let controller = new SearchSuggestionController();
  let result = yield controller.fetch("I ❤️", false, getEngine);
  do_check_eq(result.term, "I ❤️");
  do_check_eq(result.local.length, 1);
  do_check_eq(result.local[0], "I ❤️ XUL");
  do_check_eq(result.remote.length, 1);
  do_check_eq(result.remote[0], "I ❤️ Mozilla");
});


add_task(function* both_local_remote_result_dedupe() {
  yield updateSearchHistory("bump", "Mozilla");

  let controller = new SearchSuggestionController();
  let result = yield controller.fetch("mo", false, getEngine);
  do_check_eq(result.term, "mo");
  do_check_eq(result.local.length, 1);
  do_check_eq(result.local[0], "Mozilla");
  do_check_eq(result.remote.length, 2);
  do_check_eq(result.remote[0], "modern");
  do_check_eq(result.remote[1], "mom");
});

add_task(function* POST_both_local_remote_result_dedupe() {
  let controller = new SearchSuggestionController();
  let result = yield controller.fetch("mo", false, postEngine);
  do_check_eq(result.term, "mo");
  do_check_eq(result.local.length, 1);
  do_check_eq(result.local[0], "Mozilla");
  do_check_eq(result.remote.length, 2);
  do_check_eq(result.remote[0], "modern");
  do_check_eq(result.remote[1], "mom");
});

add_task(function* both_local_remote_result_dedupe2() {
  yield updateSearchHistory("bump", "mom");

  let controller = new SearchSuggestionController();
  let result = yield controller.fetch("mo", false, getEngine);
  do_check_eq(result.term, "mo");
  do_check_eq(result.local.length, 2);
  do_check_eq(result.local[0], "mom");
  do_check_eq(result.local[1], "Mozilla");
  do_check_eq(result.remote.length, 1);
  do_check_eq(result.remote[0], "modern");
});

add_task(function* both_local_remote_result_dedupe3() {
  // All of the server entries also exist locally
  yield updateSearchHistory("bump", "modern");

  let controller = new SearchSuggestionController();
  let result = yield controller.fetch("mo", false, getEngine);
  do_check_eq(result.term, "mo");
  do_check_eq(result.local.length, 3);
  do_check_eq(result.local[0], "modern");
  do_check_eq(result.local[1], "mom");
  do_check_eq(result.local[2], "Mozilla");
  do_check_eq(result.remote.length, 0);
});

add_task(function* fetch_twice_in_a_row() {
  // Two entries since the first will match the first fetch but not the second.
  yield updateSearchHistory("bump", "delay local");
  yield updateSearchHistory("bump", "delayed local");

  let controller = new SearchSuggestionController();
  let resultPromise1 = controller.fetch("delay", false, getEngine);

  // A second fetch while the server is still waiting to return results leads to an abort.
  let resultPromise2 = controller.fetch("delayed ", false, getEngine);
  yield resultPromise1.then((results) => do_check_null(results));

  let result = yield resultPromise2;
  do_check_eq(result.term, "delayed ");
  do_check_eq(result.local.length, 1);
  do_check_eq(result.local[0], "delayed local");
  do_check_eq(result.remote.length, 1);
  do_check_eq(result.remote[0], "delayed ");
});

add_task(function* fetch_twice_subset_reuse_formHistoryResult() {
  // This tests if we mess up re-using the cached form history result.
  // Two entries since the first will match the first fetch but not the second.
  yield updateSearchHistory("bump", "delay local");
  yield updateSearchHistory("bump", "delayed local");

  let controller = new SearchSuggestionController();
  let result = yield controller.fetch("delay", false, getEngine);
  do_check_eq(result.term, "delay");
  do_check_eq(result.local.length, 2);
  do_check_eq(result.local[0], "delay local");
  do_check_eq(result.local[1], "delayed local");
  do_check_eq(result.remote.length, 1);
  do_check_eq(result.remote[0], "delay");

  // Remove the entry from the DB but it should remain in the cached formHistoryResult.
  yield updateSearchHistory("remove", "delayed local");

  let result2 = yield controller.fetch("delayed ", false, getEngine);
  do_check_eq(result2.term, "delayed ");
  do_check_eq(result2.local.length, 1);
  do_check_eq(result2.local[0], "delayed local");
  do_check_eq(result2.remote.length, 1);
  do_check_eq(result2.remote[0], "delayed ");
});

add_task(function* both_identical_with_more_than_max_results() {
  // Add letters A through Z to form history which will match the server
  for (let charCode = "A".charCodeAt(); charCode <= "Z".charCodeAt(); charCode++) {
    yield updateSearchHistory("bump", "letter " + String.fromCharCode(charCode));
  }

  let controller = new SearchSuggestionController();
  controller.maxLocalResults = 7;
  controller.maxRemoteResults = 10;
  let result = yield controller.fetch("letter ", false, getEngine);
  do_check_eq(result.term, "letter ");
  do_check_eq(result.local.length, 7);
  for (let i = 0; i < controller.maxLocalResults; i++) {
    do_check_eq(result.local[i], "letter " + String.fromCharCode("A".charCodeAt() + i));
  }
  do_check_eq(result.remote.length, 10);
  for (let i = 0; i < controller.maxRemoteResults; i++) {
    do_check_eq(result.remote[i],
                "letter " + String.fromCharCode("A".charCodeAt() + controller.maxLocalResults + i));
  }
});

add_task(function* one_of_each() {
  let controller = new SearchSuggestionController();
  controller.maxLocalResults = 1;
  controller.maxRemoteResults = 1;
  let result = yield controller.fetch("letter ", false, getEngine);
  do_check_eq(result.term, "letter ");
  do_check_eq(result.local.length, 1);
  do_check_eq(result.local[0], "letter A");
  do_check_eq(result.remote.length, 1);
  do_check_eq(result.remote[0], "letter B");
});

add_task(function* local_result_returned_remote_result_disabled() {
  Services.prefs.setBoolPref("browser.search.suggest.enabled", false);
  let controller = new SearchSuggestionController();
  controller.maxLocalResults = 1;
  controller.maxRemoteResults = 1;
  let result = yield controller.fetch("letter ", false, getEngine);
  do_check_eq(result.term, "letter ");
  do_check_eq(result.local.length, 1);
  do_check_eq(result.local[0], "letter A");
  do_check_eq(result.remote.length, 0);
  Services.prefs.clearUserPref("browser.search.suggest.enabled");
});

add_task(function* local_result_returned_remote_result_disabled_after_creation_of_controller() {
  let controller = new SearchSuggestionController();
  controller.maxLocalResults = 1;
  controller.maxRemoteResults = 1;
  Services.prefs.setBoolPref("browser.search.suggest.enabled", false);
  let result = yield controller.fetch("letter ", false, getEngine);
  do_check_eq(result.term, "letter ");
  do_check_eq(result.local.length, 1);
  do_check_eq(result.local[0], "letter A");
  do_check_eq(result.remote.length, 0);
});

add_task(function* one_of_each_disabled_before_creation_enabled_after_creation_of_controller() {
  Services.prefs.setBoolPref("browser.search.suggest.enabled", false);
  let controller = new SearchSuggestionController();
  controller.maxLocalResults = 1;
  controller.maxRemoteResults = 1;
  Services.prefs.setBoolPref("browser.search.suggest.enabled", true);
  let result = yield controller.fetch("letter ", false, getEngine);
  do_check_eq(result.term, "letter ");
  do_check_eq(result.local.length, 1);
  do_check_eq(result.local[0], "letter A");
  do_check_eq(result.remote.length, 1);
  do_check_eq(result.remote[0], "letter B");
});

add_task(function* clear_suggestions_pref() {
  Services.prefs.clearUserPref("browser.search.suggest.enabled");
});

add_task(function* one_local_zero_remote() {
  let controller = new SearchSuggestionController();
  controller.maxLocalResults = 1;
  controller.maxRemoteResults = 0;
  let result = yield controller.fetch("letter ", false, getEngine);
  do_check_eq(result.term, "letter ");
  do_check_eq(result.local.length, 1);
  do_check_eq(result.local[0], "letter A");
  do_check_eq(result.remote.length, 0);
});

add_task(function* zero_local_one_remote() {
  let controller = new SearchSuggestionController();
  controller.maxLocalResults = 0;
  controller.maxRemoteResults = 1;
  let result = yield controller.fetch("letter ", false, getEngine);
  do_check_eq(result.term, "letter ");
  do_check_eq(result.local.length, 0);
  do_check_eq(result.remote.length, 1);
  do_check_eq(result.remote[0], "letter A");
});

add_task(function* stop_search() {
  let controller = new SearchSuggestionController((result) => {
    do_throw("The callback shouldn't be called after stop()");
  });
  let resultPromise = controller.fetch("mo", false, getEngine);
  controller.stop();
  yield resultPromise.then((result) => {
    do_check_null(result);
  });
});

add_task(function* empty_searchTerm() {
  // Empty searches don't go to the server but still get form history.
  let controller = new SearchSuggestionController();
  let result = yield controller.fetch("", false, getEngine);
  do_check_eq(result.term, "");
  do_check_true(result.local.length > 0);
  do_check_eq(result.remote.length, 0);
});

add_task(function* slow_timeout() {
  let d = Promise.defer();
  function check_result(result) {
    do_check_eq(result.term, "slow ");
    do_check_eq(result.local.length, 1);
    do_check_eq(result.local[0], "slow local result");
    do_check_eq(result.remote.length, 0);
  }
  yield updateSearchHistory("bump", "slow local result");

  let controller = new SearchSuggestionController();
  setTimeout(function check_timeout() {
    // The HTTP response takes 10 seconds so check that we already have results after 2 seconds.
    check_result(result);
    d.resolve();
  }, 2000);
  let result = yield controller.fetch("slow ", false, getEngine);
  check_result(result);
  yield d.promise;
});

add_task(function* slow_stop() {
  let d = Promise.defer();
  let controller = new SearchSuggestionController();
  let resultPromise = controller.fetch("slow ", false, getEngine);
  setTimeout(function check_timeout() {
    // The HTTP response takes 10 seconds but we timeout in less than a second so just use 0.
    controller.stop();
    d.resolve();
  }, 0);
  yield resultPromise.then((result) => {
    do_check_null(result);
  });

  yield d.promise;
});


// Error handling

add_task(function* remote_term_mismatch() {
  yield updateSearchHistory("bump", "Query Mismatch Entry");

  let controller = new SearchSuggestionController();
  let result = yield controller.fetch("Query Mismatch", false, getEngine);
  do_check_eq(result.term, "Query Mismatch");
  do_check_eq(result.local.length, 1);
  do_check_eq(result.local[0], "Query Mismatch Entry");
  do_check_eq(result.remote.length, 0);
});

add_task(function* http_404() {
  yield updateSearchHistory("bump", "HTTP 404 Entry");

  let controller = new SearchSuggestionController();
  let result = yield controller.fetch("HTTP 404", false, getEngine);
  do_check_eq(result.term, "HTTP 404");
  do_check_eq(result.local.length, 1);
  do_check_eq(result.local[0], "HTTP 404 Entry");
  do_check_eq(result.remote.length, 0);
});

add_task(function* http_500() {
  yield updateSearchHistory("bump", "HTTP 500 Entry");

  let controller = new SearchSuggestionController();
  let result = yield controller.fetch("HTTP 500", false, getEngine);
  do_check_eq(result.term, "HTTP 500");
  do_check_eq(result.local.length, 1);
  do_check_eq(result.local[0], "HTTP 500 Entry");
  do_check_eq(result.remote.length, 0);
});

add_task(function* unresolvable_server() {
  yield updateSearchHistory("bump", "Unresolvable Server Entry");

  let controller = new SearchSuggestionController();
  let result = yield controller.fetch("Unresolvable Server", false, unresolvableEngine);
  do_check_eq(result.term, "Unresolvable Server");
  do_check_eq(result.local.length, 1);
  do_check_eq(result.local[0], "Unresolvable Server Entry");
  do_check_eq(result.remote.length, 0);
});


// Exception handling

add_task(function* missing_pb() {
  Assert.throws(() => {
    let controller = new SearchSuggestionController();
    controller.fetch("No privacy");
  }, /priva/i);
});

add_task(function* missing_engine() {
  Assert.throws(() => {
    let controller = new SearchSuggestionController();
    controller.fetch("No engine", false);
  }, /engine/i);
});

add_task(function* invalid_engine() {
  Assert.throws(() => {
    let controller = new SearchSuggestionController();
    controller.fetch("invalid engine", false, {});
  }, /engine/i);
});

add_task(function* no_results_requested() {
  Assert.throws(() => {
    let controller = new SearchSuggestionController();
    controller.maxLocalResults = 0;
    controller.maxRemoteResults = 0;
    controller.fetch("No results requested", false, getEngine);
  }, /result/i);
});

add_task(function* minus_one_results_requested() {
  Assert.throws(() => {
    let controller = new SearchSuggestionController();
    controller.maxLocalResults = -1;
    controller.fetch("-1 results requested", false, getEngine);
  }, /result/i);
});


// Helpers

function updateSearchHistory(operation, value) {
  let deferred = Promise.defer();
  FormHistory.update({
                       op: operation,
                       fieldname: "searchbar-history",
                       value: value,
                     },
                     {
                       handleError: function (error) {
                         do_throw("Error occurred updating form history: " + error);
                         deferred.reject(error);
                       },
                       handleCompletion: function (reason) {
                         if (!reason)
                           deferred.resolve();
                       }
                     });
  return deferred.promise;
}
