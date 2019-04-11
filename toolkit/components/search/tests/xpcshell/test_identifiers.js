/* Any copyright is dedicated to the Public Domain.
 *    http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test that a search engine's identifier can be extracted from the filename.
 */

"use strict";

const SEARCH_APP_DIR = 1;

add_task(async function setup() {
  configureToLoadJarEngines();
  await AddonTestUtils.promiseStartupManager();
});

add_test(function test_identifier() {
  Services.search.init().then(async function initComplete(aResult) {
    info("init'd search service");
    Assert.ok(Components.isSuccessCode(aResult));

    await installTestEngine();
    let profileEngine = Services.search.getEngineByName(kTestEngineName);
    let jarEngine = Services.search.getEngineByName("bug645970");

    Assert.ok(profileEngine instanceof Ci.nsISearchEngine);
    Assert.ok(jarEngine instanceof Ci.nsISearchEngine);

    // An engine loaded from the profile directory won't have an identifier,
    // because it's not built-in.
    Assert.equal(profileEngine.identifier, null);

    // An engine loaded from a JAR will have an identifier corresponding to
    // the filename inside the JAR. (In this case it's the same as the name.)
    Assert.equal(jarEngine.identifier, "bug645970");

    run_next_test();
  });
});
