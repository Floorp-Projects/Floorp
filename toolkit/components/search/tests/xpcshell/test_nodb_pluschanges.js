/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * test_nodb: Start search service without existing settings file.
 *
 * Ensure that :
 * - nothing explodes;
 * - if we change the order, search.json.mozlz4 is updated;
 * - this search.json.mozlz4 can be parsed;
 * - the order stored in search.json.mozlz4 is consistent.
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

add_task(async function test_nodb_pluschanges() {
  let engine1 = await SearchTestUtils.promiseNewSearchEngine({
    url: `${gDataUrl}engine.xml`,
  });
  let engine2 = await SearchTestUtils.promiseNewSearchEngine({
    url: `${gDataUrl}engine2.xml`,
  });
  await promiseAfterSettings();

  let search = Services.search;

  await search.moveEngine(engine1, 0);
  await search.moveEngine(engine2, 1);

  // This is needed to avoid some reentrency issues in nsSearchService.
  info("Next step is forcing flush");
  await new Promise(resolve => executeSoon(resolve));

  info("Forcing flush");
  let promiseCommit = promiseAfterSettings();
  search.QueryInterface(Ci.nsIObserver).observe(null, "quit-application", "");
  await promiseCommit;
  info("Commit complete");

  // Check that the entries are placed as specified correctly
  let metadata = await promiseEngineMetadata();
  Assert.equal(metadata["Test search engine"].order, 1);
  Assert.equal(metadata["A second test engine"].order, 2);
});
