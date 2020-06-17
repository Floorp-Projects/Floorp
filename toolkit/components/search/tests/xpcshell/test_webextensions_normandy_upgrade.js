/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

SearchTestUtils.initXPCShellAddonManager(this, "system");

async function restart() {
  Services.search.wrappedJSObject.reset();
  await AddonTestUtils.promiseRestartManager();
  await Services.search.init(false);
}

const CONFIG_DEFAULT = [
  {
    webExtension: { id: "plainengine@search.mozilla.org" },
    appliesTo: [{ included: { everywhere: true } }],
  },
];

const CONFIG_UPDATED = [
  {
    webExtension: { id: "plainengine@search.mozilla.org" },
    appliesTo: [{ included: { everywhere: true } }],
  },
  {
    webExtension: { id: "example@search.mozilla.org" },
    appliesTo: [{ included: { everywhere: true } }],
  },
];

async function getEngineNames() {
  let engines = await Services.search.getDefaultEngines();
  return engines.map(engine => engine._name);
}

add_task(async function setup() {
  await useTestEngines("test-extensions", null, CONFIG_DEFAULT);
  await AddonTestUtils.promiseStartupManager();
  registerCleanupFunction(AddonTestUtils.promiseShutdownManager);
  SearchTestUtils.useMockIdleService(registerCleanupFunction);
  await Services.search.init();
});

// Test the situation where we receive an updated configuration
// that references an engine that doesnt exist locally as it
// will be installed by Normandy.
add_task(async function test_config_before_normandy() {
  // Ensure initial default setup.
  await SearchTestUtils.updateRemoteSettingsConfig(CONFIG_DEFAULT);
  await restart();
  Assert.deepEqual(await getEngineNames(), ["Plain"]);
  // Updated configuration references nonexistant engine.
  await SearchTestUtils.updateRemoteSettingsConfig(CONFIG_UPDATED);
  Assert.deepEqual(
    await getEngineNames(),
    ["Plain"],
    "Updated engine hasnt been installed yet"
  );
  // Normandy then installs the engine.
  let addon = await SearchTestUtils.installSystemSearchExtension();
  Assert.deepEqual(
    await getEngineNames(),
    ["Plain", "Example"],
    "Both engines are now enabled"
  );
  await addon.unload();
});

// Test the situation where we receive a newly installed
// engine from Normandy followed by the update to the
// configuration that uses that engine.
add_task(async function test_normandy_before_config() {
  // Ensure initial default setup.
  await SearchTestUtils.updateRemoteSettingsConfig(CONFIG_DEFAULT);
  await restart();
  Assert.deepEqual(await getEngineNames(), ["Plain"]);
  // Normandy installs the enigne.
  let addon = await SearchTestUtils.installSystemSearchExtension();
  Assert.deepEqual(
    await getEngineNames(),
    ["Plain"],
    "Normandy engine ignored as not in config yet"
  );
  // Configuration is updated to use the engine.
  await SearchTestUtils.updateRemoteSettingsConfig(CONFIG_UPDATED);
  Assert.deepEqual(
    await getEngineNames(),
    ["Plain", "Example"],
    "Both engines are now enabled"
  );
  await addon.unload();
});
