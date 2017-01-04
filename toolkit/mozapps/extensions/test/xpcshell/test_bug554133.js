/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that if the AMO response provides total_results,
// searchSucceeded is called with the correct number of total results

Components.utils.import("resource://gre/modules/addons/AddonRepository.jsm");

const PREF_GETADDONS_GETSEARCHRESULTS = "extensions.getAddons.search.url";

Components.utils.import("resource://testing-common/httpd.js");
var server;

var TESTS = [
{
  query:      "bug554133",
  maxResults: 2,
  length:     2,
  total:      100
},
{
  query:      "bug554133",
  maxResults: 10,
  length:     10,
  total:      100
},
{
  query:      "bug554133",
  maxResults: 100,
  length:     10,
  total:      100
}
];

var gCurrentTest = 0;
var SearchCallback = {
  searchSucceeded(addons, length, total) {
    do_check_false(AddonRepository.isSearching);
    do_check_eq(addons.length, length);
    do_check_eq(length, TESTS[gCurrentTest].length);
    do_check_eq(total, TESTS[gCurrentTest].total);

    gCurrentTest++;
    run_current_test();
  },

  searchFailed() {
    server.stop(do_test_finished);
    do_throw("Search results failed");
  }
};

function run_current_test() {
  if (gCurrentTest < TESTS.length) {
    var query = TESTS[gCurrentTest].query;
    var maxResults = TESTS[gCurrentTest].maxResults;
    AddonRepository.searchAddons(query, maxResults, SearchCallback);
  } else
    server.stop(do_test_finished);
}

function run_test() {
  // Setup for test
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9");

  startupManager();

  server = new HttpServer();
  server.registerDirectory("/", do_get_file("data"));
  mapFile("/data/test_bug554133.xml", server);
  server.start(-1);
  gPort = server.identity.primaryPort;

  // Point search to the test server
  Services.prefs.setCharPref(PREF_GETADDONS_GETSEARCHRESULTS,
                             "http://localhost:" + gPort + "/data/test_%TERMS%.xml");

  do_check_neq(AddonRepository, null);
  gCurrentTest = 0;
  run_current_test();
}

