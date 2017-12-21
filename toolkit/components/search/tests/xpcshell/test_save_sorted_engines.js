/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Ensure that metadata are stored correctly on disk after:
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
  useHttpServer();

  run_next_test();
}

add_task(async function test_save_sorted_engines() {
  let [engine1, engine2] = await addTestEngines([
    { name: "Test search engine", xmlFileName: "engine.xml" },
    { name: "A second test engine", xmlFileName: "engine2.xml"},
  ]);
  await promiseAfterCache();

  let search = Services.search;

  // Test moving the engines
  search.moveEngine(engine1, 0);
  search.moveEngine(engine2, 1);

  // Changes should be commited immediately
  await promiseAfterCache();
  info("Commit complete after moveEngine");

  // Check that the entries are placed as specified correctly
  let metadata = await promiseEngineMetadata();
  Assert.equal(metadata["test-search-engine"].order, 1);
  Assert.equal(metadata["a-second-test-engine"].order, 2);

  // Test removing an engine
  search.removeEngine(engine1);
  await promiseAfterCache();
  info("Commit complete after removeEngine");

  // Check that the order of the remaining engine was updated correctly
  metadata = await promiseEngineMetadata();
  Assert.equal(metadata["a-second-test-engine"].order, 1);

  // Test adding a new engine
  search.addEngineWithDetails("foo", "", "foo", "", "GET",
                              "http://searchget/?search={searchTerms}");
  await promiseAfterCache();
  info("Commit complete after addEngineWithDetails");

  metadata = await promiseEngineMetadata();
  Assert.equal(metadata.foo.alias, "foo");
  Assert.ok(metadata.foo.order > 0);
});
