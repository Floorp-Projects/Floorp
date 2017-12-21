/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  do_load_manifest("data/chrome.manifest");

  configureToLoadJarEngines();
  installDistributionEngine();

  Assert.ok(!Services.search.isInitialized);

  // test that the engine from the distribution overrides our jar engine
  let engines = Services.search.getEngines();
  Assert.equal(engines.length, 1);

  Assert.ok(Services.search.isInitialized);

  let engine = Services.search.getEngineByName("bug645970");
  Assert.notEqual(engine, null);

  // check the engine we have is actually the one from the distribution
  Assert.equal(engine.description, "override");
}
