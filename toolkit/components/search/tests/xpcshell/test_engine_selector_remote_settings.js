/* Any copyright is dedicated to the Public Domain.
 *    http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  Promise: "resource://gre/modules/Promise.jsm",
  SearchEngineSelector: "resource://gre/modules/SearchEngineSelector.jsm",
});

const TEST_CONFIG = [
  {
    engineName: "aol",
    orderHint: 500,
    webExtension: {
      locales: ["default"],
    },
    appliesTo: [
      {
        included: { everywhere: true },
      },
      {
        included: { regions: ["us"] },
        webExtension: {
          locales: ["$USER_LOCALE"],
        },
      },
    ],
  },
  {
    engineName: "lycos",
    orderHint: 1000,
    default: "yes",
    appliesTo: [
      {
        included: { everywhere: true },
        excluded: { locales: { matches: ["zh-CN"] } },
      },
    ],
  },
  {
    engineName: "altavista",
    orderHint: 2000,
    defaultPrivate: "yes",
    appliesTo: [
      {
        included: { locales: { matches: ["en-US"] } },
      },
      {
        included: { regions: ["default"] },
      },
    ],
  },
  {
    engineName: "excite",
    default: "yes-if-no-other",
    appliesTo: [
      {
        included: { everywhere: true },
        excluded: { regions: ["us"] },
      },
      {
        included: { everywhere: true },
        cohort: "acohortid",
      },
    ],
  },
  {
    engineName: "askjeeves",
  },
];

let getStub;

add_task(async function setup() {
  const searchConfigSettings = await RemoteSettings(SearchUtils.SETTINGS_KEY);
  getStub = sinon.stub(searchConfigSettings, "get");
});

add_task(async function test_selector_basic_get() {
  const listenerSpy = sinon.spy();
  const engineSelector = new SearchEngineSelector(listenerSpy);
  getStub.onFirstCall().returns(TEST_CONFIG);

  const { engines } = await engineSelector.fetchEngineConfiguration(
    "en-US",
    "default",
    "default"
  );

  Assert.deepEqual(
    engines.map(e => e.engineName),
    ["lycos", "altavista", "aol", "excite"],
    "Should have obtained the correct data from the database."
  );
  Assert.ok(listenerSpy.notCalled, "Should not have called the listener");
});

add_task(async function test_selector_get_reentry() {
  const listenerSpy = sinon.spy();
  const engineSelector = new SearchEngineSelector(listenerSpy);
  let promise = Promise.defer();
  getStub.resetHistory();
  getStub.onFirstCall().returns(promise.promise);
  delete engineSelector._configuration;

  let firstResult;
  let secondResult;

  const firstCallPromise = engineSelector
    .fetchEngineConfiguration("en-US", "default", "default")
    .then(result => (firstResult = result.engines));

  const secondCallPromise = engineSelector
    .fetchEngineConfiguration("en-US", "default", "default")
    .then(result => (secondResult = result.engines));

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

  promise.resolve(TEST_CONFIG);

  await Promise.all([firstCallPromise, secondCallPromise]);
  Assert.deepEqual(
    firstResult.map(e => e.engineName),
    ["lycos", "altavista", "aol", "excite"],
    "Should have returned the correct data to the first call"
  );

  Assert.deepEqual(
    secondResult.map(e => e.engineName),
    ["lycos", "altavista", "aol", "excite"],
    "Should have returned the correct data to the second call"
  );
  Assert.ok(listenerSpy.notCalled, "Should not have called the listener");
});

add_task(async function test_selector_config_update() {
  const listenerSpy = sinon.spy();
  const engineSelector = new SearchEngineSelector(listenerSpy);
  getStub.resetHistory();
  getStub.onFirstCall().returns(TEST_CONFIG);

  const { engines } = await engineSelector.fetchEngineConfiguration(
    "en-US",
    "default",
    "default"
  );

  Assert.deepEqual(
    engines.map(e => e.engineName),
    ["lycos", "altavista", "aol", "excite"],
    "Should have got the correct configuration"
  );

  Assert.ok(listenerSpy.notCalled, "Should not have called the listener yet");

  const NEW_DATA = [
    {
      default: "yes",
      engineName: "askjeeves",
      appliesTo: [{ included: { everywhere: true } }],
      schema: 1553857697843,
      last_modified: 1553859483588,
    },
  ];

  getStub.resetHistory();
  getStub.onFirstCall().returns(NEW_DATA);
  await RemoteSettings(SearchUtils.SETTINGS_KEY).emit("sync", {
    data: {
      current: NEW_DATA,
    },
  });

  Assert.ok(listenerSpy.called, "Should have called the listener");

  const result = await engineSelector.fetchEngineConfiguration(
    "en-US",
    "default",
    "default"
  );

  Assert.deepEqual(
    result.engines.map(e => e.engineName),
    ["askjeeves"],
    "Should have updated the configuration with the new data"
  );
});

add_task(async function test_selector_db_modification() {
  const engineSelector = new SearchEngineSelector();
  // Fill the database with some values that we can use to test that it is cleared.
  const db = await RemoteSettings(SearchUtils.SETTINGS_KEY).db;
  await db.importChanges(
    {},
    42,
    [
      {
        id: "85e1f268-9ca5-4b52-a4ac-922df5c07264",
        default: "yes",
        engineName: "askjeeves",
        appliesTo: [{ included: { everywhere: true } }],
      },
    ],
    { clear: true }
  );

  // Stub the get() so that the first call simulates a signature error, and
  // the second simulates success reading from the dump.
  getStub.resetHistory();
  getStub
    .onFirstCall()
    .rejects(new RemoteSettingsClient.InvalidSignatureError("abc"));
  getStub.onSecondCall().returns(TEST_CONFIG);

  let result = await engineSelector.fetchEngineConfiguration(
    "en-US",
    "default",
    "default"
  );

  Assert.ok(
    getStub.calledTwice,
    "Should have called the get() function twice."
  );

  const databaseEntries = await db.list();
  Assert.equal(databaseEntries.length, 0, "Should have cleared the database.");

  Assert.deepEqual(
    result.engines.map(e => e.engineName),
    ["lycos", "altavista", "aol", "excite"],
    "Should have returned the correct data."
  );
});

add_task(async function test_selector_db_modification_never_succeeds() {
  const engineSelector = new SearchEngineSelector();
  // Fill the database with some values that we can use to test that it is cleared.
  const db = RemoteSettings(SearchUtils.SETTINGS_KEY).db;
  await db.importChanges(
    {},
    42,
    [
      {
        id: "b70edfdd-1c3f-4b7b-ab55-38cb048636c0",
        default: "yes",
        engineName: "askjeeves",
        appliesTo: [{ included: { everywhere: true } }],
      },
    ],
    {
      clear: true,
    }
  );

  // Now simulate the condition where for some reason we never get a
  // valid result.
  getStub.reset();
  getStub.rejects(new RemoteSettingsClient.InvalidSignatureError("abc"));

  await Assert.rejects(
    engineSelector.fetchEngineConfiguration("en-US", "default", "default"),
    ex => ex.result == Cr.NS_ERROR_UNEXPECTED,
    "Should have rejected loading the engine configuration"
  );

  Assert.ok(
    getStub.calledTwice,
    "Should have called the get() function twice."
  );

  const databaseEntries = await db.list();
  Assert.equal(databaseEntries.length, 0, "Should have cleared the database.");
});

add_task(async function test_empty_results() {
  // Check that returning an empty result re-tries.
  const engineSelector = new SearchEngineSelector();
  // Fill the database with some values that we can use to test that it is cleared.
  const db = await RemoteSettings(SearchUtils.SETTINGS_KEY).db;
  await db.importChanges(
    {},
    42,
    [
      {
        id: "df5655ca-e045-4f8c-a7ee-047eeb654722",
        default: "yes",
        engineName: "askjeeves",
        appliesTo: [{ included: { everywhere: true } }],
      },
    ],
    {
      clear: true,
    }
  );

  // Stub the get() so that the first call simulates an empty database, and
  // the second simulates success reading from the dump.
  getStub.resetHistory();
  getStub.onFirstCall().returns([]);
  getStub.onSecondCall().returns(TEST_CONFIG);

  let result = await engineSelector.fetchEngineConfiguration(
    "en-US",
    "default",
    "default"
  );

  Assert.ok(
    getStub.calledTwice,
    "Should have called the get() function twice."
  );

  const databaseEntries = await db.list();
  Assert.equal(databaseEntries.length, 0, "Should have cleared the database.");

  Assert.deepEqual(
    result.engines.map(e => e.engineName),
    ["lycos", "altavista", "aol", "excite"],
    "Should have returned the correct data."
  );
});
