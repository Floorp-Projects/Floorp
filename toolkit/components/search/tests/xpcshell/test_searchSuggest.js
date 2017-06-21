/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Testing search suggestions from SearchSuggestionController.jsm.
 */

"use strict";

Cu.import("resource://gre/modules/FormHistory.jsm");
Cu.import("resource://gre/modules/SearchSuggestionController.jsm");
Cu.import("resource://gre/modules/Timer.jsm");

// We must make sure the FormHistoryStartup component is
// initialized in order for it to respond to FormHistory
// requests from nsFormAutoComplete.js.
var formHistoryStartup = Cc["@mozilla.org/satchel/form-history-startup;1"].
                         getService(Ci.nsIObserver);
formHistoryStartup.observe(null, "profile-after-change", null);

var httpServer = new HttpServer();
var getEngine, postEngine, unresolvableEngine;

function run_test() {
  Services.prefs.setBoolPref("browser.search.suggest.enabled", true);

  removeMetadata();

  let server = useHttpServer();
  server.registerContentType("sjs", "sjs");

  do_register_cleanup(() => (async function cleanup() {
    // Remove added form history entries
    await updateSearchHistory("remove", null);
    Services.prefs.clearUserPref("browser.search.suggest.enabled");
  })());

  run_next_test();
}

add_task(async function add_test_engines() {
  let getEngineData = {
    baseURL: gDataUrl,
    name: "GET suggestion engine",
    method: "GET",
  };

  let postEngineData = {
    baseURL: gDataUrl,
    name: "POST suggestion engine",
    method: "POST",
  };

  let unresolvableEngineData = {
    baseURL: "http://example.invalid/",
    name: "Offline suggestion engine",
    method: "GET",
  };

  [getEngine, postEngine, unresolvableEngine] = await addTestEngines([
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

add_task(async function simple_no_result_callback() {
  await new Promise(resolve => {
    let controller = new SearchSuggestionController((result) => {
      do_check_eq(result.term, "no remote");
      do_check_eq(result.local.length, 0);
      do_check_eq(result.remote.length, 0);
      resolve();
    });

    controller.fetch("no remote", false, getEngine);
  });
});

add_task(async function simple_no_result_callback_and_promise() {
  // Make sure both the callback and promise get results
  let deferred = Promise.defer();
  let controller = new SearchSuggestionController((result) => {
    do_check_eq(result.term, "no results");
    do_check_eq(result.local.length, 0);
    do_check_eq(result.remote.length, 0);
    deferred.resolve();
  });

  let result = await controller.fetch("no results", false, getEngine);
  do_check_eq(result.term, "no results");
  do_check_eq(result.local.length, 0);
  do_check_eq(result.remote.length, 0);

  await deferred.promise;
});

add_task(async function simple_no_result_promise() {
  let controller = new SearchSuggestionController();
  let result = await controller.fetch("no remote", false, getEngine);
  do_check_eq(result.term, "no remote");
  do_check_eq(result.local.length, 0);
  do_check_eq(result.remote.length, 0);
});

add_task(async function simple_remote_no_local_result() {
  let controller = new SearchSuggestionController();
  let result = await controller.fetch("mo", false, getEngine);
  do_check_eq(result.term, "mo");
  do_check_eq(result.local.length, 0);
  do_check_eq(result.remote.length, 3);
  do_check_eq(result.remote[0], "Mozilla");
  do_check_eq(result.remote[1], "modern");
  do_check_eq(result.remote[2], "mom");
});

add_task(async function remote_term_case_mismatch() {
  let controller = new SearchSuggestionController();
  let result = await controller.fetch("Query Case Mismatch", false, getEngine);
  do_check_eq(result.term, "Query Case Mismatch");
  do_check_eq(result.remote.length, 1);
  do_check_eq(result.remote[0], "Query Case Mismatch");
});

add_task(async function simple_local_no_remote_result() {
  await updateSearchHistory("bump", "no remote entries");

  let controller = new SearchSuggestionController();
  let result = await controller.fetch("no remote", false, getEngine);
  do_check_eq(result.term, "no remote");
  do_check_eq(result.local.length, 1);
  do_check_eq(result.local[0], "no remote entries");
  do_check_eq(result.remote.length, 0);

  await updateSearchHistory("remove", "no remote entries");
});

add_task(async function simple_non_ascii() {
  await updateSearchHistory("bump", "I ❤️ XUL");

  let controller = new SearchSuggestionController();
  let result = await controller.fetch("I ❤️", false, getEngine);
  do_check_eq(result.term, "I ❤️");
  do_check_eq(result.local.length, 1);
  do_check_eq(result.local[0], "I ❤️ XUL");
  do_check_eq(result.remote.length, 1);
  do_check_eq(result.remote[0], "I ❤️ Mozilla");
});

add_task(async function both_local_remote_result_dedupe() {
  await updateSearchHistory("bump", "Mozilla");

  let controller = new SearchSuggestionController();
  let result = await controller.fetch("mo", false, getEngine);
  do_check_eq(result.term, "mo");
  do_check_eq(result.local.length, 1);
  do_check_eq(result.local[0], "Mozilla");
  do_check_eq(result.remote.length, 2);
  do_check_eq(result.remote[0], "modern");
  do_check_eq(result.remote[1], "mom");
});

add_task(async function POST_both_local_remote_result_dedupe() {
  let controller = new SearchSuggestionController();
  let result = await controller.fetch("mo", false, postEngine);
  do_check_eq(result.term, "mo");
  do_check_eq(result.local.length, 1);
  do_check_eq(result.local[0], "Mozilla");
  do_check_eq(result.remote.length, 2);
  do_check_eq(result.remote[0], "modern");
  do_check_eq(result.remote[1], "mom");
});

add_task(async function both_local_remote_result_dedupe2() {
  await updateSearchHistory("bump", "mom");

  let controller = new SearchSuggestionController();
  let result = await controller.fetch("mo", false, getEngine);
  do_check_eq(result.term, "mo");
  do_check_eq(result.local.length, 2);
  do_check_eq(result.local[0], "mom");
  do_check_eq(result.local[1], "Mozilla");
  do_check_eq(result.remote.length, 1);
  do_check_eq(result.remote[0], "modern");
});

add_task(async function both_local_remote_result_dedupe3() {
  // All of the server entries also exist locally
  await updateSearchHistory("bump", "modern");

  let controller = new SearchSuggestionController();
  let result = await controller.fetch("mo", false, getEngine);
  do_check_eq(result.term, "mo");
  do_check_eq(result.local.length, 3);
  do_check_eq(result.local[0], "modern");
  do_check_eq(result.local[1], "mom");
  do_check_eq(result.local[2], "Mozilla");
  do_check_eq(result.remote.length, 0);
});

add_task(async function fetch_twice_in_a_row() {
  // Two entries since the first will match the first fetch but not the second.
  await updateSearchHistory("bump", "delay local");
  await updateSearchHistory("bump", "delayed local");

  let controller = new SearchSuggestionController();
  let resultPromise1 = controller.fetch("delay", false, getEngine);

  // A second fetch while the server is still waiting to return results leads to an abort.
  let resultPromise2 = controller.fetch("delayed ", false, getEngine);
  await resultPromise1.then((results) => do_check_null(results));

  let result = await resultPromise2;
  do_check_eq(result.term, "delayed ");
  do_check_eq(result.local.length, 1);
  do_check_eq(result.local[0], "delayed local");
  do_check_eq(result.remote.length, 1);
  do_check_eq(result.remote[0], "delayed ");
});

add_task(async function fetch_twice_subset_reuse_formHistoryResult() {
  // This tests if we mess up re-using the cached form history result.
  // Two entries since the first will match the first fetch but not the second.
  await updateSearchHistory("bump", "delay local");
  await updateSearchHistory("bump", "delayed local");

  let controller = new SearchSuggestionController();
  let result = await controller.fetch("delay", false, getEngine);
  do_check_eq(result.term, "delay");
  do_check_eq(result.local.length, 2);
  do_check_eq(result.local[0], "delay local");
  do_check_eq(result.local[1], "delayed local");
  do_check_eq(result.remote.length, 1);
  do_check_eq(result.remote[0], "delay");

  // Remove the entry from the DB but it should remain in the cached formHistoryResult.
  await updateSearchHistory("remove", "delayed local");

  let result2 = await controller.fetch("delayed ", false, getEngine);
  do_check_eq(result2.term, "delayed ");
  do_check_eq(result2.local.length, 1);
  do_check_eq(result2.local[0], "delayed local");
  do_check_eq(result2.remote.length, 1);
  do_check_eq(result2.remote[0], "delayed ");
});

add_task(async function both_identical_with_more_than_max_results() {
  // Add letters A through Z to form history which will match the server
  for (let charCode = "A".charCodeAt(); charCode <= "Z".charCodeAt(); charCode++) {
    await updateSearchHistory("bump", "letter " + String.fromCharCode(charCode));
  }

  let controller = new SearchSuggestionController();
  controller.maxLocalResults = 7;
  controller.maxRemoteResults = 10;
  let result = await controller.fetch("letter ", false, getEngine);
  do_check_eq(result.term, "letter ");
  do_check_eq(result.local.length, 7);
  for (let i = 0; i < controller.maxLocalResults; i++) {
    do_check_eq(result.local[i], "letter " + String.fromCharCode("A".charCodeAt() + i));
  }
  do_check_eq(result.local.length + result.remote.length, 10);
  for (let i = 0; i < result.remote.length; i++) {
    do_check_eq(result.remote[i],
                "letter " + String.fromCharCode("A".charCodeAt() + controller.maxLocalResults + i));
  }
});

add_task(async function noremote_maxLocal() {
  let controller = new SearchSuggestionController();
  controller.maxLocalResults = 2; // (should be ignored because no remote results)
  controller.maxRemoteResults = 0;
  let result = await controller.fetch("letter ", false, getEngine);
  do_check_eq(result.term, "letter ");
  do_check_eq(result.local.length, 26);
  for (let i = 0; i < result.local.length; i++) {
    do_check_eq(result.local[i], "letter " + String.fromCharCode("A".charCodeAt() + i));
  }
  do_check_eq(result.remote.length, 0);
});

add_task(async function someremote_maxLocal() {
  let controller = new SearchSuggestionController();
  controller.maxLocalResults = 2;
  controller.maxRemoteResults = 4;
  let result = await controller.fetch("letter ", false, getEngine);
  do_check_eq(result.term, "letter ");
  do_check_eq(result.local.length, 2);
  for (let i = 0; i < result.local.length; i++) {
    do_check_eq(result.local[i], "letter " + String.fromCharCode("A".charCodeAt() + i));
  }
  do_check_eq(result.remote.length, 2);
  // "A" and "B" will have been de-duped, start at C for remote results
  for (let i = 0; i < result.remote.length; i++) {
    do_check_eq(result.remote[i], "letter " + String.fromCharCode("C".charCodeAt() + i));
  }
});

add_task(async function one_of_each() {
  let controller = new SearchSuggestionController();
  controller.maxLocalResults = 1;
  controller.maxRemoteResults = 2;
  let result = await controller.fetch("letter ", false, getEngine);
  do_check_eq(result.term, "letter ");
  do_check_eq(result.local.length, 1);
  do_check_eq(result.local[0], "letter A");
  do_check_eq(result.remote.length, 1);
  do_check_eq(result.remote[0], "letter B");
});

add_task(async function local_result_returned_remote_result_disabled() {
  Services.prefs.setBoolPref("browser.search.suggest.enabled", false);
  let controller = new SearchSuggestionController();
  controller.maxLocalResults = 1;
  controller.maxRemoteResults = 1;
  let result = await controller.fetch("letter ", false, getEngine);
  do_check_eq(result.term, "letter ");
  do_check_eq(result.local.length, 26);
  for (let i = 0; i < 26; i++) {
    do_check_eq(result.local[i], "letter " + String.fromCharCode("A".charCodeAt() + i));
  }
  do_check_eq(result.remote.length, 0);
  Services.prefs.setBoolPref("browser.search.suggest.enabled", true);
});

add_task(async function local_result_returned_remote_result_disabled_after_creation_of_controller() {
  let controller = new SearchSuggestionController();
  controller.maxLocalResults = 1;
  controller.maxRemoteResults = 1;
  Services.prefs.setBoolPref("browser.search.suggest.enabled", false);
  let result = await controller.fetch("letter ", false, getEngine);
  do_check_eq(result.term, "letter ");
  do_check_eq(result.local.length, 26);
  for (let i = 0; i < 26; i++) {
    do_check_eq(result.local[i], "letter " + String.fromCharCode("A".charCodeAt() + i));
  }
  do_check_eq(result.remote.length, 0);
  Services.prefs.setBoolPref("browser.search.suggest.enabled", true);
});

add_task(async function one_of_each_disabled_before_creation_enabled_after_creation_of_controller() {
  Services.prefs.setBoolPref("browser.search.suggest.enabled", false);
  let controller = new SearchSuggestionController();
  controller.maxLocalResults = 1;
  controller.maxRemoteResults = 2;
  Services.prefs.setBoolPref("browser.search.suggest.enabled", true);
  let result = await controller.fetch("letter ", false, getEngine);
  do_check_eq(result.term, "letter ");
  do_check_eq(result.local.length, 1);
  do_check_eq(result.local[0], "letter A");
  do_check_eq(result.remote.length, 1);
  do_check_eq(result.remote[0], "letter B");
});

add_task(async function reset_suggestions_pref() {
  Services.prefs.setBoolPref("browser.search.suggest.enabled", true);
});

add_task(async function one_local_zero_remote() {
  let controller = new SearchSuggestionController();
  controller.maxLocalResults = 1;
  controller.maxRemoteResults = 0;
  let result = await controller.fetch("letter ", false, getEngine);
  do_check_eq(result.term, "letter ");
  do_check_eq(result.local.length, 26);
  for (let i = 0; i < 26; i++) {
    do_check_eq(result.local[i], "letter " + String.fromCharCode("A".charCodeAt() + i));
  }
  do_check_eq(result.remote.length, 0);
});

add_task(async function zero_local_one_remote() {
  let controller = new SearchSuggestionController();
  controller.maxLocalResults = 0;
  controller.maxRemoteResults = 1;
  let result = await controller.fetch("letter ", false, getEngine);
  do_check_eq(result.term, "letter ");
  do_check_eq(result.local.length, 0);
  do_check_eq(result.remote.length, 1);
  do_check_eq(result.remote[0], "letter A");
});

add_task(async function stop_search() {
  let controller = new SearchSuggestionController((result) => {
    do_throw("The callback shouldn't be called after stop()");
  });
  let resultPromise = controller.fetch("mo", false, getEngine);
  controller.stop();
  await resultPromise.then((result) => {
    do_check_null(result);
  });
});

add_task(async function empty_searchTerm() {
  // Empty searches don't go to the server but still get form history.
  let controller = new SearchSuggestionController();
  let result = await controller.fetch("", false, getEngine);
  do_check_eq(result.term, "");
  do_check_true(result.local.length > 0);
  do_check_eq(result.remote.length, 0);
});

add_task(async function slow_timeout() {
  let d = Promise.defer();
  function check_result(result) {
    do_check_eq(result.term, "slow ");
    do_check_eq(result.local.length, 1);
    do_check_eq(result.local[0], "slow local result");
    do_check_eq(result.remote.length, 0);
  }
  await updateSearchHistory("bump", "slow local result");

  let controller = new SearchSuggestionController();
  setTimeout(function check_timeout() {
    // The HTTP response takes 10 seconds so check that we already have results after 2 seconds.
    check_result(result);
    d.resolve();
  }, 2000);
  let result = await controller.fetch("slow ", false, getEngine);
  check_result(result);
  await d.promise;
});

add_task(async function slow_stop() {
  let d = Promise.defer();
  let controller = new SearchSuggestionController();
  let resultPromise = controller.fetch("slow ", false, getEngine);
  setTimeout(function check_timeout() {
    // The HTTP response takes 10 seconds but we timeout in less than a second so just use 0.
    controller.stop();
    d.resolve();
  }, 0);
  await resultPromise.then((result) => {
    do_check_null(result);
  });

  await d.promise;
});


// Error handling

add_task(async function remote_term_mismatch() {
  await updateSearchHistory("bump", "Query Mismatch Entry");

  let controller = new SearchSuggestionController();
  let result = await controller.fetch("Query Mismatch", false, getEngine);
  do_check_eq(result.term, "Query Mismatch");
  do_check_eq(result.local.length, 1);
  do_check_eq(result.local[0], "Query Mismatch Entry");
  do_check_eq(result.remote.length, 0);
});

add_task(async function http_404() {
  await updateSearchHistory("bump", "HTTP 404 Entry");

  let controller = new SearchSuggestionController();
  let result = await controller.fetch("HTTP 404", false, getEngine);
  do_check_eq(result.term, "HTTP 404");
  do_check_eq(result.local.length, 1);
  do_check_eq(result.local[0], "HTTP 404 Entry");
  do_check_eq(result.remote.length, 0);
});

add_task(async function http_500() {
  await updateSearchHistory("bump", "HTTP 500 Entry");

  let controller = new SearchSuggestionController();
  let result = await controller.fetch("HTTP 500", false, getEngine);
  do_check_eq(result.term, "HTTP 500");
  do_check_eq(result.local.length, 1);
  do_check_eq(result.local[0], "HTTP 500 Entry");
  do_check_eq(result.remote.length, 0);
});

add_task(async function unresolvable_server() {
  await updateSearchHistory("bump", "Unresolvable Server Entry");

  let controller = new SearchSuggestionController();
  let result = await controller.fetch("Unresolvable Server", false, unresolvableEngine);
  do_check_eq(result.term, "Unresolvable Server");
  do_check_eq(result.local.length, 1);
  do_check_eq(result.local[0], "Unresolvable Server Entry");
  do_check_eq(result.remote.length, 0);
});


// Exception handling

add_task(async function missing_pb() {
  Assert.throws(() => {
    let controller = new SearchSuggestionController();
    controller.fetch("No privacy");
  }, /priva/i);
});

add_task(async function missing_engine() {
  Assert.throws(() => {
    let controller = new SearchSuggestionController();
    controller.fetch("No engine", false);
  }, /engine/i);
});

add_task(async function invalid_engine() {
  Assert.throws(() => {
    let controller = new SearchSuggestionController();
    controller.fetch("invalid engine", false, {});
  }, /engine/i);
});

add_task(async function no_results_requested() {
  Assert.throws(() => {
    let controller = new SearchSuggestionController();
    controller.maxLocalResults = 0;
    controller.maxRemoteResults = 0;
    controller.fetch("No results requested", false, getEngine);
  }, /result/i);
});

add_task(async function minus_one_results_requested() {
  Assert.throws(() => {
    let controller = new SearchSuggestionController();
    controller.maxLocalResults = -1;
    controller.fetch("-1 results requested", false, getEngine);
  }, /result/i);
});

add_task(async function test_userContextId() {
  let controller = new SearchSuggestionController();
  controller._fetchRemote = function(searchTerm, engine, privateMode, userContextId) {
    Assert.equal(userContextId, 1);
    return Promise.defer();
  };

  controller.fetch("test", false, getEngine, 1);
});

// Helpers

function updateSearchHistory(operation, value) {
  return new Promise((resolve, reject) => {
    FormHistory.update({
                         op: operation,
                         fieldname: "searchbar-history",
                         value,
                       },
                       {
                         handleError(error) {
                           do_throw("Error occurred updating form history: " + error);
                           reject(error);
                         },
                         handleCompletion(reason) {
                           if (!reason)
                             resolve();
                         }
                       });
  });
}
