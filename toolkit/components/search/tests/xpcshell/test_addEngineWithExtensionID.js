/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const kSearchEngineID = "addEngineWithDetails_test_engine";
const kSearchEngineURL = "https://example.com/?search={searchTerms}";
const kSearchTerm = "foo";
const kExtensionID1 = "extension1@mozilla.com";

add_task(async function setup() {
  useHttpServer();
  await AddonTestUtils.promiseStartupManager();
});

add_task(async function test_addEngineWithDetailsWithExtensionID() {
  Assert.ok(!Services.search.isInitialized);

  await Services.search.addEngineWithDetails(kSearchEngineID, {
    method: "get",
    template: kSearchEngineURL,
    extensionID: kExtensionID1,
  });

  let engine = Services.search.getEngineByName(kSearchEngineID);
  Assert.notEqual(engine, null);
  Assert.ok(
    !engine.isAppProvided,
    "Should not be shown as an app-provided engine"
  );

  let engines = await Services.search.getEnginesByExtensionID(kExtensionID1);
  Assert.equal(engines.length, 1);
  Assert.equal(engines[0].name, engine.name);
});
