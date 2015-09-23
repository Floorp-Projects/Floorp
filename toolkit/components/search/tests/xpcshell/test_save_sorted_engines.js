/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * test_save_sorted_engines: Start search engine
 * - without search-metadata.json
 * - without search.sqlite
 *
 * Ensure that search-metadata.json is correct after:
 * - moving an engine
 * - removing an engine
 * - adding a new engine
 *
 * Notes:
 * - we install the search engines of test "test_downloadAndAddEngines.js"
 * to ensure that this test is independent from locale, commercial agreements
 * and configuration of Firefox.
 */

function run_test() {
  removeMetadata();
  updateAppInfo();
  useHttpServer();

  run_next_test();
}

add_task(function* test_save_sorted_engines() {
  let [engine1, engine2] = yield addTestEngines([
    { name: "Test search engine", xmlFileName: "engine.xml" },
    { name: "A second test engine", xmlFileName: "engine2.xml"},
  ]);

  let search = Services.search;

  // Test moving the engines
  search.moveEngine(engine1, 0);
  search.moveEngine(engine2, 1);

  // Changes should be commited immediately
  yield promiseAfterCommit();
  do_print("Commit complete after moveEngine");

  // Check that the entries are placed as specified correctly
  let json = getSearchMetadata();
  do_check_eq(json["[app]/test-search-engine.xml"].order, 1);
  do_check_eq(json["[profile]/a-second-test-engine.xml"].order, 2);

  // Test removing an engine
  search.removeEngine(engine1);
  yield promiseAfterCommit();
  do_print("Commit complete after removeEngine");

  // Check that the order of the remaining engine was updated correctly
  json = getSearchMetadata();
  do_check_eq(json["[profile]/a-second-test-engine.xml"].order, 1);

  // Test adding a new engine
  search.addEngineWithDetails("foo", "", "foo", "", "GET",
                              "http://searchget/?search={searchTerms}");
  yield promiseAfterCommit();
  do_print("Commit complete after addEngineWithDetails");

  json = getSearchMetadata();
  do_check_eq(json["[profile]/foo.xml"].alias, "foo");
  do_check_true(json["[profile]/foo.xml"].order > 0);

  do_print("Cleaning up");
  removeMetadata();
});
