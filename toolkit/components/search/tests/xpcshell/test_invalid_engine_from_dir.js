/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test that invalid engine files with xml extensions will not break
 * initialization. See Bug 940446.
 */
add_task(async function setup() {
  await AddonTestUtils.promiseStartupManager();
});

add_task(async function test_invalid_engine_from_dir() {
  Assert.ok(!Services.search.isInitialized);

  let engineFile = do_get_profile().clone();
  engineFile.append("searchplugins");
  engineFile.append("test-search-engine.xml");
  engineFile.parent.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

  // Copy the invalid engine to the test profile.
  let engineTemplateFile = do_get_file("data/invalid-engine.xml");
  engineTemplateFile.copyTo(engineFile.parent, "test-search-engine.xml");

  await Services.search.init();
});
