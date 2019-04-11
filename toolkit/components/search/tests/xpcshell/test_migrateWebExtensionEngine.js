/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const kSearchEngineID = "addEngineWithDetails_test_engine";
const kExtensionID = "test@example.com";

const kSearchEngineDetails = {
  template: "http://example.com/?search={searchTerms}",
  description: "Test Description",
  iconURL: "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNkYPhfDwAChwGA60e6kgAAAABJRU5ErkJggg==",
  suggestURL: "http://example.com/?suggest={searchTerms}",
  alias: "alias_foo",
  extensionID: kExtensionID,
};

add_task(async function setup() {
  await AddonTestUtils.promiseStartupManager();
});

add_task(async function test_migrateLegacyEngine() {
  Assert.ok(!Services.search.isInitialized);

  await Services.search.addEngineWithDetails(kSearchEngineID, kSearchEngineDetails);

  // Modify the loadpath so it looks like an legacy plugin loadpath
  let engine = Services.search.getEngineByName(kSearchEngineID);
  engine.wrappedJSObject._loadPath = `jar:[profile]/extensions/${kExtensionID}.xpi!/engine.xml`;
  engine.wrappedJSObject._extensionID = null;

  // This should replace the existing engine
  await Services.search.addEngineWithDetails(kSearchEngineID, kSearchEngineDetails);

  engine = Services.search.getEngineByName(kSearchEngineID);
  Assert.equal(engine.wrappedJSObject._loadPath, "[other]addEngineWithDetails:" + kExtensionID);
  Assert.equal(engine.wrappedJSObject._extensionID, kExtensionID);
});
