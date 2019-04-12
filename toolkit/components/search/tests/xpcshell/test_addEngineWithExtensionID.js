/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const kSearchEngineID = "addEngineWithDetails_test_engine";
const kSearchEngineURL = "https://example.com/?search={searchTerms}";
const kSearchTerm = "foo";
const kExtensionID1 = "extension1@mozilla.com";
const kExtensionID2 = "extension2@mozilla.com";
const kExtension2LoadPath = "[http]localhost/test-search-engine.xml:extension2@mozilla.com";

add_task(async function setup() {
  await AddonTestUtils.promiseStartupManager();
});

add_task(async function test_addEngineWithDetailsWithExtensionID() {
  Assert.ok(!Services.search.isInitialized);

  await Services.search.addEngineWithDetails(kSearchEngineID, "", "", "", "get",
                                             kSearchEngineURL, kExtensionID1);

  let engine = Services.search.getEngineByName(kSearchEngineID);
  Assert.notEqual(engine, null);

  let engines = await Services.search.getEnginesByExtensionID(kExtensionID1);
  Assert.equal(engines.length, 1);
  Assert.equal(engines[0].name, engine.name);
});

add_task(async function test_addEngineWithExtensionID() {
  let engine = await Services.search.addEngine(gDataUrl + "engine.xml", null,
                                               false, kExtensionID2);
  let engines = await Services.search.getEnginesByExtensionID(kExtensionID2);
  Assert.equal(engines.length, 1);
  Assert.deepEqual(engines[0].name, engine.name);
  Assert.equal(engine.wrappedJSObject._loadPath, kExtension2LoadPath);
});

function run_test() {
  useHttpServer();

  run_next_test();
}
