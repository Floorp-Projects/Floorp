/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const SEARCH_SERVICE_TOPIC = "browser-search-service";
const SEARCH_ENGINE_TOPIC = "browser-search-engine-modified";

const CONFIG = {
  data: [
    {
      webExtension: {
        id: "engine@search.mozilla.org",
      },
      orderHint: 30,
      appliesTo: [
        {
          included: { everywhere: true },
          excluded: { regions: ["FR"] },
          default: "yes",
          defaultPrivate: "yes",
        },
      ],
    },
    {
      webExtension: {
        id: "engine-pref@search.mozilla.org",
      },
      orderHint: 20,
      appliesTo: [
        {
          included: { regions: ["FR"] },
          default: "yes",
          defaultPrivate: "yes",
        },
      ],
    },
  ],
};

// Default engine with no region defined.
const DEFAULT = "Test search engine";
// Default engine with region set to FR.
const FR_DEFAULT = "engine-pref";

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

add_task(async function setup() {
  Services.prefs.setBoolPref("browser.search.separatePrivateDefault", true);

  await useTestEngines();
  await AddonTestUtils.promiseStartupManager();

  let confUrl = `data:application/json,${JSON.stringify(CONFIG)}`;
  Services.prefs.setStringPref("search.config.url", confUrl);
});

add_task(async function test_regular_init() {
  let reloadObserved = listenFor(SEARCH_SERVICE_TOPIC, "engines-reloaded");
  let geoUrl = `data:application/json,{"country_code": "FR"}`;
  Services.prefs.setCharPref("browser.search.geoip.url", geoUrl);

  await Promise.all([
    Services.search.init(true),
    SearchTestUtils.promiseSearchNotification("ensure-known-region-done"),
    promiseAfterCache(),
  ]);

  Assert.equal(
    Services.search.defaultEngine.name,
    FR_DEFAULT,
    "Geo defined default should be set"
  );

  Assert.ok(
    !reloadObserved(),
    "Engines don't reload with immediate region fetch"
  );
});

add_task(async function test_init_with_slow_region_lookup() {
  let reloadObserved = listenFor(SEARCH_SERVICE_TOPIC, "engines-reloaded");
  let initPromise;

  // Ensure the region lookup completes after init so the
  // engines are reloaded
  Services.prefs.setCharPref("browser.search.region", "");
  let srv = useHttpServer();
  srv.registerPathHandler("/fetch_region", async (req, res) => {
    res.processAsync();
    await initPromise;
    res.setStatusLine("1.1", 200, "OK");
    res.write(JSON.stringify({ country_code: "FR" }));
    res.finish();
  });

  Services.prefs.setCharPref(
    "browser.search.geoip.url",
    `http://localhost:${srv.identity.primaryPort}/fetch_region`
  );

  // Kick off a re-init.
  initPromise = asyncReInit();
  await initPromise;

  let otherPromises = [
    SearchTestUtils.promiseSearchNotification("ensure-known-region-done"),
    promiseAfterCache(),
    SearchTestUtils.promiseSearchNotification(
      "engine-default",
      SEARCH_ENGINE_TOPIC
    ),
  ];

  Assert.equal(
    Services.search.defaultEngine.name,
    DEFAULT,
    "Test engine shouldn't be the default anymore"
  );

  await Promise.all(otherPromises);

  // Ensure that correct engine is being reported as the default.
  Assert.equal(
    Services.search.defaultEngine.name,
    FR_DEFAULT,
    "engine-pref should be the default in FR"
  );
  Assert.equal(
    (await Services.search.getDefaultPrivate()).name,
    FR_DEFAULT,
    "engine-pref should be the private default in FR"
  );

  Assert.ok(reloadObserved(), "Engines do reload with delayed region fetch");
});
