/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  do_test_pending();

  do_load_manifest("data/chrome.manifest");

  configureToLoadJarEngines();

  // Copy an engine in [profile]/searchplugins/ and ensure it's not
  // overriding the same file from a jar.
  // The description in the file we are copying is 'profile'.
  let dir = gProfD.clone();
  dir.append("searchplugins");
  if (!dir.exists())
    dir.create(dir.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
  do_get_file("data/engine-override.xml").copyTo(dir, "bug645970.xml");

  Assert.ok(!Services.search.isInitialized);

  Services.search.init(function search_initialized(aStatus) {
    Assert.ok(Components.isSuccessCode(aStatus));
    Assert.ok(Services.search.isInitialized);

    // test engines from dir are not loaded.
    let engines = Services.search.getEngines();
    Assert.equal(engines.length, 1);

    // test jar engine is loaded ok.
    let engine = Services.search.getEngineByName("bug645970");
    Assert.notEqual(engine, null);

    Assert.equal(engine.description, "bug645970");

    do_test_finished();
  });
}
