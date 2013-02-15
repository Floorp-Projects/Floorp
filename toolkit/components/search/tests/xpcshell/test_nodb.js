/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * test_nodb: Start search engine
 * - without search-metadata.json
 * - without search.sqlite
 *
 * Ensure that :
 * - nothing explodes;
 * - no search-metadata.json is created.
 */


function run_test()
{
  removeMetadata();
  updateAppInfo();

  let search = Services.search;

  do_test_pending();
  search.init(function ss_initialized(rv) {
    do_check_true(Components.isSuccessCode(rv));
    do_timeout(500, function() {
      // Check that search-metadata.json has not been
      // created. Note that we cannot do much better
      // than a timeout for checking a non-event.
      let metadata = gProfD.clone();
      metadata.append("search-metadata.json");
      do_check_true(!metadata.exists());
      removeMetadata();

      do_test_finished();
    });
  });
}
