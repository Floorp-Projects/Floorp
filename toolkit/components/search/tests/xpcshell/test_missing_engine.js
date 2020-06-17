/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// This test is designed to check the search service keeps working if there's
// a built-in engine missing from the configuration.

"use strict";

const { MockRegistrar } = ChromeUtils.import(
  "resource://testing-common/MockRegistrar.jsm"
);

const SEARCH_SERVICE_TOPIC = "browser-search-service";
const SEARCH_ENGINE_TOPIC = "browser-search-engine-modified";

const GOOD_CONFIG = [
  {
    webExtension: {
      id: "engine@search.mozilla.org",
    },
    appliesTo: [
      {
        included: { everywhere: true },
      },
    ],
  },
];

const BAD_CONFIG = [
  ...GOOD_CONFIG,
  {
    webExtension: {
      id: "engine-missing@search.mozilla.org",
    },
    appliesTo: [
      {
        included: { everywhere: true },
      },
    ],
  },
];

function listenFor(name, key) {
  let notifyObserved = false;
  let obs = (subject, topic, data) => {
    if (data == key) {
      notifyObserved = true;
    }
  };
  Services.obs.addObserver(obs, name);

  return () => {
    Services.obs.removeObserver(obs, name);
    return notifyObserved;
  };
}

let configurationStub;

add_task(async function setup() {
  SearchTestUtils.useMockIdleService(registerCleanupFunction);
  await AddonTestUtils.promiseStartupManager();
});

add_task(async function test_startup_with_missing() {
  configurationStub = await useTestEngines("data", null, BAD_CONFIG);

  const result = await Services.search.init();
  Assert.ok(
    Components.isSuccessCode(result),
    "Should have started the search service successfully."
  );

  const engines = await Services.search.getEngines();

  Assert.deepEqual(
    engines.map(e => e.name),
    ["Test search engine"],
    "Should have listed just the good engine"
  );
});

add_task(async function test_update_with_missing() {
  configurationStub.returns(GOOD_CONFIG);

  await Services.search.reInit();

  const engines = await Services.search.getEngines();

  Assert.deepEqual(
    engines.map(e => e.name),
    ["Test search engine"],
    "Should have just the good engine"
  );

  // TODO: Bug 1542269: When remote settings is enabled, remove the reInit
  // and uncomment the code below.
  await Services.search.reInit();

  // const reloadObserved = SearchTestUtils.promiseSearchNotification(
  //   "engines-reloaded"
  // );
  //
  // await RemoteSettings(SearchUtils.SETTINGS_KEY).emit("sync", {
  //   data: {
  //     current: BAD_CONFIG,
  //   },
  // });
  //
  // idleService._fireObservers("idle");
  //
  // await reloadObserved;

  Assert.deepEqual(
    engines.map(e => e.name),
    ["Test search engine"],
    "Should still have just the good engine"
  );
});
