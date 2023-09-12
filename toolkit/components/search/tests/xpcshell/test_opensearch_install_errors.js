/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test that various install failures are handled correctly.
 */

add_setup(async function () {
  useHttpServer("opensearch");
  await AddonTestUtils.promiseStartupManager();
  await Services.search.init();

  // This test purposely attempts to load an invalid engine.
  consoleAllowList.push("_onLoad: Failed to init engine!");
  consoleAllowList.push("Invalid search plugin due to namespace not matching");
});

add_task(async function test_invalid_path_fails() {
  await Assert.rejects(
    Services.search.addOpenSearchEngine("http://invalid/data/engine.xml", null),
    error => {
      Assert.equal(
        error.result,
        Ci.nsISearchService.ERROR_DOWNLOAD_FAILURE,
        "Should have returned download failure."
      );
      return true;
    },
    "Should fail to install an engine with an invalid path."
  );
});

add_task(async function test_install_duplicate_fails() {
  let engine = await Services.search.addOpenSearchEngine(
    gDataUrl + "simple.xml",
    null
  );
  Assert.equal(engine.name, "simple", "Should have installed the engine.");

  await Assert.rejects(
    Services.search.addOpenSearchEngine(gDataUrl + "simple.xml", null),
    error => {
      Assert.equal(
        error.result,
        Ci.nsISearchService.ERROR_DUPLICATE_ENGINE,
        "Should have returned duplicate failure."
      );
      return true;
    },
    "Should fail to install a duplicate engine."
  );
});

add_task(async function test_invalid_engine_from_dir() {
  await Assert.rejects(
    Services.search.addOpenSearchEngine(gDataUrl + "invalid.xml", null),
    error => {
      Assert.equal(
        error.result,
        Ci.nsISearchService.ERROR_ENGINE_CORRUPTED,
        "Should have returned corruption failure."
      );
      return true;
    },
    "Should fail to install an invalid engine."
  );
});
