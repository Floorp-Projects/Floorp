/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test that defaultEngine property can be set and yields the proper events and\
 * behavior (search results)
 */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
});

let getVariableStub;

let defaultGetVariable = name => {
  if (name == "seperatePrivateDefaultUIEnabled") {
    return true;
  }
  if (name == "seperatePrivateDefaultUrlbarResultEnabled") {
    return false;
  }
  return undefined;
};

add_setup(async () => {
  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault.ui.enabled",
    true
  );
  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault",
    true
  );

  sinon.spy(NimbusFeatures.searchConfiguration, "onUpdate");
  sinon.stub(NimbusFeatures.searchConfiguration, "ready").resolves();
  getVariableStub = sinon.stub(
    NimbusFeatures.searchConfiguration,
    "getVariable"
  );
  getVariableStub.callsFake(defaultGetVariable);

  do_get_profile();
  Services.fog.initializeFOG();

  await SearchTestUtils.useTestEngines("data1");

  await AddonTestUtils.promiseStartupManager();

  let promiseSaved = promiseSaveSettingsData();
  await Services.search.init();
  await promiseSaved;
});

async function switchExperiment(newExperiment) {
  let promiseReloaded =
    SearchTestUtils.promiseSearchNotification("engines-reloaded");
  let promiseSaved = promiseSaveSettingsData();

  // Stub getVariable to populate the cache with our expected data
  getVariableStub.callsFake(name => {
    if (name == "experiment") {
      return newExperiment;
    }
    return defaultGetVariable(name);
  });
  for (let call of NimbusFeatures.searchConfiguration.onUpdate.getCalls()) {
    call.args[0]();
  }

  await promiseReloaded;
  await promiseSaved;
}

function getSettingsAttribute(setting, isAppProvided) {
  return Services.search.wrappedJSObject._settings.getVerifiedMetaDataAttribute(
    setting,
    isAppProvided
  );
}

add_task(async function test_experiment_setting() {
  Assert.equal(
    Services.search.defaultEngine.name,
    "engine1",
    "Should have the application default engine as default"
  );

  // Start the experiment.
  await switchExperiment("exp1");

  Assert.equal(
    Services.search.defaultEngine.name,
    "engine2",
    "Should have set the experiment engine as default"
  );

  // End the experiment.
  await switchExperiment("");

  Assert.equal(
    Services.search.defaultEngine.name,
    "engine1",
    "Should have reset the default engine to the application default"
  );
  Assert.equal(
    getSettingsAttribute(
      "defaultEngineId",
      Services.search.defaultEngine.isAppProvided
    ),
    "",
    "Should have kept the saved attribute as empty"
  );
});

add_task(async function test_experiment_setting_to_same_as_user() {
  Services.search.defaultEngine = Services.search.getEngineByName("engine2");

  Assert.equal(
    Services.search.defaultEngine.name,
    "engine2",
    "Should have the user selected engine as default"
  );
  Assert.equal(
    getSettingsAttribute(
      "defaultEngineId",
      Services.search.defaultEngine.isAppProvided
    ),
    "engine2@search.mozilla.orgdefault"
  );

  // Start the experiment, ensure user default is maintained.
  await switchExperiment("exp1");
  Assert.equal(
    getSettingsAttribute(
      "defaultEngineId",
      Services.search.defaultEngine.isAppProvided
    ),
    "engine2@search.mozilla.orgdefault"
  );

  Assert.equal(
    Services.search.appDefaultEngine.name,
    "engine2",
    "Should have set the experiment engine as default"
  );
  Assert.equal(
    Services.search.defaultEngine.name,
    "engine2",
    "Should have set the experiment engine as default"
  );

  // End the experiment.
  await switchExperiment("");

  Assert.equal(
    Services.search.appDefaultEngine.name,
    "engine1",
    "Should have set the app default engine correctly"
  );
  Assert.equal(
    Services.search.defaultEngine.name,
    "engine2",
    "Should have kept the engine the same "
  );
  Assert.equal(
    getSettingsAttribute(
      "defaultEngineId",
      Services.search.defaultEngine.isAppProvided
    ),
    "engine2@search.mozilla.orgdefault",
    "Should have kept the saved attribute as the user's preference"
  );
});

add_task(async function test_experiment_setting_user_changed_back_during() {
  Services.search.defaultEngine = Services.search.getEngineByName("engine1");

  Assert.equal(
    Services.search.defaultEngine.name,
    "engine1",
    "Should have the application default engine as default"
  );
  Assert.equal(
    getSettingsAttribute(
      "defaultEngineId",
      Services.search.defaultEngine.isAppProvided
    ),
    "",
    "Should have an empty settings attribute"
  );

  // Start the experiment.
  await switchExperiment("exp1");

  Assert.equal(
    Services.search.defaultEngine.name,
    "engine2",
    "Should have set the experiment engine as default"
  );
  Assert.equal(
    getSettingsAttribute(
      "defaultEngineId",
      Services.search.defaultEngine.isAppProvided
    ),
    "",
    "Should still have an empty settings attribute"
  );

  // User resets to the original default engine.
  Services.search.defaultEngine = Services.search.getEngineByName("engine1");
  Assert.equal(
    Services.search.defaultEngine.name,
    "engine1",
    "Should have the user selected engine as default"
  );
  Assert.equal(
    getSettingsAttribute(
      "defaultEngineId",
      Services.search.defaultEngine.isAppProvided
    ),
    "engine1@search.mozilla.orgdefault"
  );

  // Ending the experiment should keep the original default and reset the
  // saved attribute.
  await switchExperiment("");

  Assert.equal(
    Services.search.appDefaultEngine.name,
    "engine1",
    "Should have set the app default engine correctly"
  );
  Assert.equal(
    Services.search.defaultEngine.name,
    "engine1",
    "Should have kept the engine the same"
  );
  Assert.equal(
    getSettingsAttribute(
      "defaultEngineId",
      Services.search.defaultEngine.isAppProvided
    ),
    "",
    "Should have reset the saved attribute to empty after the experiment ended"
  );
});

add_task(async function test_experiment_setting_user_changed_back_private() {
  Services.search.defaultPrivateEngine =
    Services.search.getEngineByName("engine1");

  Assert.equal(
    Services.search.defaultPrivateEngine.name,
    "engine1",
    "Should have the user selected engine as default"
  );
  Assert.equal(
    getSettingsAttribute(
      "privateDefaultEngineId",
      Services.search.defaultPrivateEngine.isAppProvided
    ),
    "",
    "Should have an empty settings attribute"
  );

  // Start the experiment.
  await switchExperiment("exp2");

  Assert.equal(
    Services.search.defaultPrivateEngine.name,
    "exp2",
    "Should have set the experiment engine as default"
  );
  Assert.equal(
    getSettingsAttribute(
      "defaultEngineId",
      Services.search.defaultEngine.isAppProvided
    ),
    "",
    "Should still have an empty settings attribute"
  );

  // User resets to the original default engine.
  Services.search.defaultPrivateEngine =
    Services.search.getEngineByName("engine1");
  Assert.equal(
    Services.search.defaultPrivateEngine.name,
    "engine1",
    "Should have the user selected engine as default"
  );
  Assert.equal(
    getSettingsAttribute(
      "privateDefaultEngineId",
      Services.search.defaultPrivateEngine.isAppProvided
    ),
    "engine1@search.mozilla.orgdefault"
  );

  // Ending the experiment should keep the original default and reset the
  // saved attribute.
  await switchExperiment("");

  Assert.equal(Services.search.appPrivateDefaultEngine.name, "engine1");
  Assert.equal(
    Services.search.defaultEngine.name,
    "engine1",
    "Should have kept the engine the same "
  );
  Assert.equal(
    getSettingsAttribute(
      "privateDefaultEngineId",
      Services.search.defaultPrivateEngine.isAppProvided
    ),
    "",
    "Should have reset the saved attribute to empty after the experiment ended"
  );
});

add_task(async function test_experiment_setting_user_changed_to_other_during() {
  Services.search.defaultEngine = Services.search.getEngineByName("engine1");

  Assert.equal(
    Services.search.defaultEngine.name,
    "engine1",
    "Should have the application default engine as default"
  );
  Assert.equal(
    getSettingsAttribute(
      "defaultEngineId",
      Services.search.defaultEngine.isAppProvided
    ),
    "",
    "Should have an empty settings attribute"
  );

  // Start the experiment.
  await switchExperiment("exp3");

  Assert.equal(
    Services.search.defaultEngine.name,
    "exp3",
    "Should have set the experiment engine as default"
  );
  Assert.equal(
    getSettingsAttribute(
      "defaultEngineId",
      Services.search.defaultEngine.isAppProvided
    ),
    "",
    "Should still have an empty settings attribute"
  );

  // User changes to a different default engine
  Services.search.defaultEngine = Services.search.getEngineByName("engine2");
  Assert.equal(
    Services.search.defaultEngine.name,
    "engine2",
    "Should have the user selected engine as default"
  );
  Assert.equal(
    getSettingsAttribute(
      "defaultEngineId",
      Services.search.defaultEngine.isAppProvided
    ),
    "engine2@search.mozilla.orgdefault",
    "Should have correctly set the user's default in settings"
  );

  // Ending the experiment should keep the original default and reset the
  // saved attribute.
  await switchExperiment("");

  Assert.equal(
    Services.search.appDefaultEngine.name,
    "engine1",
    "Should have set the app default engine correctly"
  );
  Assert.equal(
    Services.search.defaultEngine.name,
    "engine2",
    "Should have kept the user's choice of engine"
  );
  Assert.equal(
    getSettingsAttribute(
      "defaultEngineId",
      Services.search.defaultEngine.isAppProvided
    ),
    "engine2@search.mozilla.orgdefault",
    "Should have kept the user's choice in settings"
  );
});

add_task(async function test_experiment_setting_user_hid_app_default_during() {
  // Add all the test engines to be general search engines. This is important
  // for the test, as the removed experiment engine needs to be a general search
  // engine, and the first in the list (aided by the orderHint in
  // data1/engines.json).
  SearchUtils.GENERAL_SEARCH_ENGINE_IDS.add("engine1@search.mozilla.org");
  SearchUtils.GENERAL_SEARCH_ENGINE_IDS.add("engine2@search.mozilla.org");
  SearchUtils.GENERAL_SEARCH_ENGINE_IDS.add("exp2@search.mozilla.org");
  SearchUtils.GENERAL_SEARCH_ENGINE_IDS.add("exp3@search.mozilla.org");

  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault",
    false
  );
  Services.search.defaultEngine = Services.search.getEngineByName("engine1");

  Assert.equal(
    Services.search.defaultEngine.name,
    "engine1",
    "Should have the application default engine as default"
  );
  Assert.equal(
    getSettingsAttribute(
      "defaultEngineId",
      Services.search.defaultEngine.isAppProvided
    ),
    "",
    "Should have an empty settings attribute"
  );

  // Start the experiment.
  await switchExperiment("exp3");

  Assert.equal(
    Services.search.defaultEngine.name,
    "exp3",
    "Should have set the experiment engine as default"
  );
  Assert.equal(
    getSettingsAttribute(
      "defaultEngineId",
      Services.search.defaultEngine.isAppProvided
    ),
    "",
    "Should still have an empty settings attribute"
  );

  // User hides the original application engine
  await Services.search.removeEngine(
    Services.search.getEngineByName("engine1")
  );
  Assert.equal(
    Services.search.getEngineByName("engine1").hidden,
    true,
    "Should have hid the selected engine"
  );

  // Ending the experiment should keep the original default and reset the
  // saved attribute.
  await switchExperiment("");

  Assert.equal(
    Services.search.appDefaultEngine.name,
    "engine1",
    "Should have set the app default engine correctly"
  );
  Assert.equal(
    Services.search.defaultEngine.hidden,
    false,
    "Should not have set default engine to an engine that is hidden"
  );
  Assert.equal(
    Services.search.defaultEngine.name,
    "engine2",
    "Should have reset the user's engine to the next available engine"
  );
  Assert.equal(
    getSettingsAttribute(
      "defaultEngineId",
      Services.search.defaultEngine.isAppProvided
    ),
    "engine2@search.mozilla.orgdefault",
    "Should have saved the choice in settings"
  );
});
