/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const kSearchEngineID1 = "ignorelist_test_engine1";
const kSearchEngineURL1 =
  "http://example.com/?search={searchTerms}&ignore=true";
const kExtensionID = "searchignore@mozilla.com";

add_task(async function setup() {
  await AddonTestUtils.promiseStartupManager();
});

add_task(async function test_ignoreList_db_modification() {
  Assert.ok(
    !Services.search.isInitialized,
    "Search service should not be initialized to begin with."
  );

  // Fill the database so we can check it was cleared afterwards
  const collection = await RemoteSettings("hijack-blocklists").openCollection();
  await collection.clear();
  await collection.create(
    {
      id: "submission-urls",
      matches: ["ignore=true"],
    },
    { synced: true }
  );
  await collection.create(
    {
      id: "load-paths",
      matches: ["[other]addEngineWithDetails:searchignore@mozilla.com"],
    },
    { synced: true }
  );
  await collection.db.saveLastModified(42);

  const ignoreListSettings = RemoteSettings(
    SearchUtils.SETTINGS_IGNORELIST_KEY
  );
  const getStub = sinon.stub(ignoreListSettings, "get");

  // Stub the get() so that the first call simulates a signature error, and
  // the second simulates success reading from the dump.
  getStub
    .onFirstCall()
    .throws(new RemoteSettingsClient.InvalidSignatureError("abc"));
  getStub.onSecondCall().returns([
    {
      id: "load-paths",
      matches: ["[other]addEngineWithDetails:searchignore@mozilla.com"],
      _status: "synced",
    },
    {
      id: "submission-urls",
      matches: ["ignore=true"],
      _status: "synced",
    },
  ]);

  const updatePromise = SearchTestUtils.promiseSearchNotification(
    "settings-update-complete"
  );

  await Services.search.addEngineWithDetails(kSearchEngineID1, {
    method: "get",
    template: kSearchEngineURL1,
  });

  await updatePromise;

  const engine = Services.search.getEngineByName(kSearchEngineID1);
  Assert.equal(
    engine,
    null,
    "Engine with ignored search params should not exist"
  );

  const databaseEntries = await collection.list();
  Assert.equal(
    databaseEntries.data.length,
    0,
    "Should have cleared the database."
  );
});
