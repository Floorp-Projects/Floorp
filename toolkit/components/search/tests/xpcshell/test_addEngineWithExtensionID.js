/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const kSearchEngineID = "addEngineWithDetails_test_engine";
const kSearchEngineURL = "http://example.com/?search={searchTerms}";
const kSearchTerm = "foo";
const kExtensionID1 = "extension1@mozilla.com";
const kExtensionID2 = "extension2@mozilla.com";
const kExtension2LoadPath = "[http]localhost/test-search-engine.xml:extension2@mozilla.com";

add_task(async function test_addEngineWithDetailsWithExtensionID() {
  Assert.ok(!Services.search.isInitialized);

  await asyncInit();

  Services.search.addEngineWithDetails(kSearchEngineID, "", "", "", "get",
                                       kSearchEngineURL, kExtensionID1);

  let engine = Services.search.getEngineByName(kSearchEngineID);
  Assert.notEqual(engine, null);

  let engines = Services.search.getEnginesByExtensionID(kExtensionID1);
  Assert.equal(engines.length, 1);
  Assert.equal(engines[0], engine);
});

add_test(function test_addEngineWithExtensionID() {
  let searchCallback = {
    onSuccess(engine) {
      let engines = Services.search.getEnginesByExtensionID(kExtensionID2);
      Assert.equal(engines.length, 1);
      Assert.equal(engines[0], engine);
      Assert.equal(engine.wrappedJSObject._loadPath, kExtension2LoadPath);
      run_next_test();
    },
    onError(errorCode) {
      do_throw("search callback returned error: " + errorCode);
    }
  };
  Services.search.addEngine(gDataUrl + "engine.xml", null,
                            null, false, searchCallback, kExtensionID2);
});

function run_test() {
  useHttpServer();

  run_next_test();
}
