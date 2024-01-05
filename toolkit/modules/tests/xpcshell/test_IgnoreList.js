/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  IgnoreLists: "resource://gre/modules/IgnoreLists.sys.mjs",
  RemoteSettings: "resource://services-settings/remote-settings.sys.mjs",
  RemoteSettingsClient:
    "resource://services-settings/RemoteSettingsClient.sys.mjs",
  sinon: "resource://testing-common/Sinon.sys.mjs",
});

const IGNORELIST_KEY = "hijack-blocklists";

const IGNORELIST_TEST_DATA = [
  {
    id: "load-paths",
    matches: ["[other]addEngineWithDetails:searchignore@mozilla.com"],
  },
  {
    id: "submission-urls",
    matches: ["ignore=true"],
  },
];

let getStub;

add_task(async function setup() {
  const ignoreListSettings = RemoteSettings(IGNORELIST_KEY);
  getStub = sinon.stub(ignoreListSettings, "get");
});

add_task(async function test_ignoreList_basic_get() {
  getStub.onFirstCall().returns(IGNORELIST_TEST_DATA);

  const settings = await IgnoreLists.getAndSubscribe(() => {});

  Assert.deepEqual(
    settings,
    IGNORELIST_TEST_DATA,
    "Should have obtained the correct data from the database."
  );
});

add_task(async function test_ignoreList_reentry() {
  let promise = Promise.withResolvers();
  getStub.resetHistory();
  getStub.onFirstCall().returns(promise.promise);

  let firstResult;
  let secondResult;

  const firstCallPromise = IgnoreLists.getAndSubscribe(() => {}).then(
    result => (firstResult = result)
  );
  const secondCallPromise = IgnoreLists.getAndSubscribe(() => {}).then(
    result => (secondResult = result)
  );

  Assert.strictEqual(
    firstResult,
    undefined,
    "Should not have returned the first result yet."
  );
  Assert.strictEqual(
    secondResult,
    undefined,
    "Should not have returned the second result yet."
  );

  promise.resolve(IGNORELIST_TEST_DATA);

  await Promise.all([firstCallPromise, secondCallPromise]);

  Assert.deepEqual(
    firstResult,
    IGNORELIST_TEST_DATA,
    "Should have returned the correct data to the first call."
  );
  Assert.deepEqual(
    secondResult,
    IGNORELIST_TEST_DATA,
    "Should have returned the correct data to the second call."
  );
});

add_task(async function test_ignoreList_updates() {
  getStub.onFirstCall().returns([]);

  let updateData;
  let listener = eventData => {
    updateData = eventData.data.current;
  };

  await IgnoreLists.getAndSubscribe(listener);

  Assert.ok(!updateData, "Should not have given an update yet");

  const NEW_DATA = [
    {
      id: "load-paths",
      schema: 1553857697843,
      last_modified: 1553859483588,
      matches: ["[other]addEngineWithDetails:searchignore@mozilla.com"],
    },
    {
      id: "submission-urls",
      schema: 1553857697843,
      last_modified: 1553859435500,
      matches: ["ignore=true"],
    },
  ];

  // Simulate an ignore list update.
  await RemoteSettings("hijack-blocklists").emit("sync", {
    data: {
      current: NEW_DATA,
    },
  });

  Assert.deepEqual(
    updateData,
    NEW_DATA,
    "Should have updated the listener with the correct data"
  );

  IgnoreLists.unsubscribe(listener);

  await RemoteSettings("hijack-blocklists").emit("sync", {
    data: {
      current: [
        {
          id: "load-paths",
          schema: 1553857697843,
          last_modified: 1553859483589,
          matches: [],
        },
        {
          id: "submission-urls",
          schema: 1553857697843,
          last_modified: 1553859435501,
          matches: [],
        },
      ],
    },
  });

  Assert.deepEqual(
    updateData,
    NEW_DATA,
    "Should not have updated the listener"
  );
});

add_task(async function test_ignoreList_db_modification() {
  // Fill the database with some values that we can use to test that it is cleared.
  const db = RemoteSettings(IGNORELIST_KEY).db;
  await db.importChanges({}, Date.now(), IGNORELIST_TEST_DATA, { clear: true });

  // Stub the get() so that the first call simulates a signature error, and
  // the second simulates success reading from the dump.
  getStub.resetHistory();
  getStub
    .onFirstCall()
    .rejects(new RemoteSettingsClient.InvalidSignatureError("abc"));
  getStub.onSecondCall().returns(IGNORELIST_TEST_DATA);

  let result = await IgnoreLists.getAndSubscribe(() => {});

  Assert.ok(
    getStub.calledTwice,
    "Should have called the get() function twice."
  );

  const databaseEntries = await db.list();
  Assert.equal(databaseEntries.length, 0, "Should have cleared the database.");

  Assert.deepEqual(
    result,
    IGNORELIST_TEST_DATA,
    "Should have returned the correct data."
  );
});

add_task(async function test_ignoreList_db_modification_never_succeeds() {
  // Fill the database with some values that we can use to test that it is cleared.
  const db = RemoteSettings(IGNORELIST_KEY).db;
  await db.importChanges({}, Date.now(), IGNORELIST_TEST_DATA, { clear: true });

  // Now simulate the condition where for some reason we never get a
  // valid result.
  getStub.reset();
  getStub.rejects(new RemoteSettingsClient.InvalidSignatureError("abc"));

  let result = await IgnoreLists.getAndSubscribe(() => {});

  Assert.ok(
    getStub.calledTwice,
    "Should have called the get() function twice."
  );

  const databaseEntries = await db.list();
  Assert.equal(databaseEntries.length, 0, "Should have cleared the database.");

  Assert.deepEqual(result, [], "Should have returned an empty result.");
});
