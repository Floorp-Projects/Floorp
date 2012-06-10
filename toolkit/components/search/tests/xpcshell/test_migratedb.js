/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * test_migratedb: Start search engine
 * - without search-metadata.json
 * - with search.sqlite
 *
 * Ensure that nothing explodes.
 */

function run_test()
{
  removeMetadata();

  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "2");

  let search_sqlite = do_get_file("data/search.sqlite");
  search_sqlite.copyTo(gProfD, "search.sqlite");

  let search = Services.search;

  do_test_pending();
  afterCommit(
    function()
    {
      //Check that search-metadata.json has been created
      let metadata = gProfD.clone();
      metadata.append("search-metadata.json");
      do_check_true(metadata.exists());

      removeMetadata();
      do_test_finished();
    }
  );

  search.getEngines();
}
