/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test that invalid engine files with xml extensions will not break
 * initialization. See Bug 940446.
 */
function run_test() {
  do_test_pending();

  Assert.ok(!Services.search.isInitialized);

  let engineFile = gProfD.clone();
  engineFile.append("searchplugins");
  engineFile.append("test-search-engine.xml");
  engineFile.parent.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

  // Copy the invalid engine to the test profile.
  let engineTemplateFile = do_get_file("data/invalid-engine.xml");
  engineTemplateFile.copyTo(engineFile.parent, "test-search-engine.xml");

  Services.search.init(function search_initialized(aStatus) {
    // The invalid engine should have been skipped and should not
    // have caused an exception.
    Assert.ok(Components.isSuccessCode(aStatus));
    Assert.ok(Services.search.isInitialized);

    do_test_finished();
  });
}
