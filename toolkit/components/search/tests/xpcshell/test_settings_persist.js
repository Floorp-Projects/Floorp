/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const CONFIG_DEFAULT = [
  {
    webExtension: {
      id: "plainengine@search.mozilla.org",
      name: "Plain",
      search_url: "https://duckduckgo.com/",
      params: [
        {
          name: "q",
          value: "{searchTerms}",
        },
      ],
    },
    appliesTo: [{ included: { everywhere: true } }],
  },
  {
    webExtension: {
      id: "special-engine@search.mozilla.org",
      name: "Special",
      search_url: "https://www.google.com/search",
      params: [
        {
          name: "q",
          value: "{searchTerms}",
        },
      ],
    },
    appliesTo: [{ included: { everywhere: true } }],
  },
];

const CONFIG_UPDATED = CONFIG_DEFAULT.filter(r =>
  r.webExtension.id.startsWith("plainengine")
);

async function startup() {
  let settingsFileWritten = promiseAfterSettings();
  let ss = new SearchService();
  await AddonTestUtils.promiseRestartManager();
  await ss.init(false);
  await settingsFileWritten;
  return ss;
}

async function updateConfig(config) {
  const settings = await RemoteSettings(SearchUtils.SETTINGS_KEY);
  settings.get.restore();
  sinon.stub(settings, "get").returns(config);
}

async function visibleEngines(ss) {
  return (await ss.getVisibleEngines()).map(e => e._name);
}

add_setup(async function () {
  await SearchTestUtils.useTestEngines("test-extensions", null, CONFIG_DEFAULT);
  registerCleanupFunction(AddonTestUtils.promiseShutdownManager);
  await AddonTestUtils.promiseStartupManager();
  // This is only needed as otherwise events will not be properly notified
  // due to https://searchfox.org/mozilla-central/source/toolkit/components/search/SearchUtils.jsm#186
  let settingsFileWritten = promiseAfterSettings();
  await Services.search.init(false);
  Services.search.wrappedJSObject._removeObservers();
  await settingsFileWritten;
});

add_task(async function () {
  let ss = await startup();
  Assert.ok(
    (await visibleEngines(ss)).includes("Special"),
    "Should have both engines on first startup"
  );

  let settingsFileWritten = promiseAfterSettings();
  let engine = await ss.getEngineByName("Special");
  await ss.removeEngine(engine);
  await settingsFileWritten;

  Assert.ok(
    !(await visibleEngines(ss)).includes("Special"),
    "Special has been remove, only Plain should remain"
  );

  ss._removeObservers();
  updateConfig(CONFIG_UPDATED);
  ss = await startup();

  Assert.ok(
    !(await visibleEngines(ss)).includes("Special"),
    "Updated to new configuration that doesnt have Special"
  );

  ss._removeObservers();
  updateConfig(CONFIG_DEFAULT);
  ss = await startup();

  Assert.ok(
    !(await visibleEngines(ss)).includes("Special"),
    "Configuration now includes Special but we should remember its removal"
  );
});
