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

add_setup(async function () {
  useHttpServer();
  await AddonTestUtils.promiseStartupManager();
});

add_task(async function test_save_sorted_engines() {
  let engine1 = await SearchTestUtils.promiseNewSearchEngine({
    url: `${gDataUrl}engine.xml`,
  });
  let engine2 = await SearchTestUtils.promiseNewSearchEngine({
    url: `${gDataUrl}engine2.xml`,
  });
  await promiseAfterSettings();

  let search = Services.search;

  // Test moving the engines
  await search.moveEngine(engine1, 0);
  await search.moveEngine(engine2, 1);

  // Changes should be commited immediately
  await promiseAfterSettings();
  info("Commit complete after moveEngine");

  // Check that the entries are placed as specified correctly
  let metadata = await promiseEngineMetadata();
  Assert.equal(metadata["Test search engine"].order, 1);
  Assert.equal(metadata["A second test engine"].order, 2);

  // Test removing an engine
  search.removeEngine(engine1);
  await promiseAfterSettings();
  info("Commit complete after removeEngine");

  // Check that the order of the remaining engine was updated correctly
  metadata = await promiseEngineMetadata();
  Assert.equal(metadata["A second test engine"].order, 1);

  // Test adding a new engine
  await SearchTestUtils.installSearchExtension({
    name: "foo",
    keyword: "foo",
  });

  let engine = Services.search.getEngineByName("foo");
  await promiseAfterSettings();
  info("Commit complete after addEngineWithDetails");

  metadata = await promiseEngineMetadata();
  Assert.ok(engine.aliases.includes("foo"));
  Assert.ok(metadata.foo.order > 0);
});
