/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test that invalid engine files correctly fail to install.
 */
const mockPrompter = {
  promptCount: 0,
  alert() {
    this.promptCount++;
  },
  QueryInterface: ChromeUtils.generateQI(["nsIPrompt"]),
};

add_task(async function setup() {
  useHttpServer();
  await AddonTestUtils.promiseStartupManager();
  await Services.search.init();

  // Mock the modal error dialog
  let windowWatcher = {
    getNewPrompter: () => mockPrompter,
    QueryInterface: ChromeUtils.generateQI(["nsIWindowWatcher"]),
  };
  let origWindowWatcher = Services.ww;
  Services.ww = windowWatcher;
  registerCleanupFunction(() => {
    Services.ww = origWindowWatcher;
  });
});

add_task(async function test_invalid_engine_from_dir() {
  console.log(gDataUrl + "data/invalid-engine.xml");
  await Assert.rejects(
    Services.search.addOpenSearchEngine(
      gDataUrl + "data/invalid-engine.xml",
      null
    ),
    error => {
      Assert.ok(error.result == Ci.nsISearchService.ERROR_UNKNOWN_FAILURE);
      return true;
    },
    "Should fail to install an invalid engine."
  );
  // The prompt happens after addEngine rejects, so wait here just in case.
  await TestUtils.waitForCondition(
    () => mockPrompter.promptCount == 1,
    "Should have prompted the user"
  );
});
