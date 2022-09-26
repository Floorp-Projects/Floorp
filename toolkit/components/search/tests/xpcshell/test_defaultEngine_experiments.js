/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test that defaultEngine property can be set and yields the proper events and\
 * behavior (search results)
 */

"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  NimbusFeatures: "resource://nimbus/ExperimentAPI.jsm",
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

  sinon.spy(NimbusFeatures.search, "onUpdate");
  sinon.stub(NimbusFeatures.search, "ready").resolves();
  getVariableStub = sinon.stub(NimbusFeatures.search, "getVariable");
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
  let promiseReloaded = SearchTestUtils.promiseSearchNotification(
    "engines-reloaded"
  );
  let promiseSaved = promiseSaveSettingsData();

  // Stub getVariable to populate the cache with our expected data
  getVariableStub.callsFake(name => {
    if (name == "experiment") {
      return newExperiment;
    }
    return defaultGetVariable(name);
  });
  for (let call of NimbusFeatures.search.onUpdate.getCalls()) {
    call.args[0]();
  }

  await promiseReloaded;
  await promiseSaved;
}

function getSettingsAttribute(setting) {
  return Services.search.wrappedJSObject._settings.getVerifiedMetaDataAttribute(
    setting
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
    getSettingsAttribute("current"),
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
  Assert.equal(getSettingsAttribute("current"), "engine2");

  // Start the experiment, ensure user default is maintained.
  await switchExperiment("exp1");
  Assert.equal(getSettingsAttribute("current"), "engine2");

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

  Assert.equal(Services.search.appDefaultEngine.name, "engine1");
  Assert.equal(
    Services.search.defaultEngine.name,
    "engine2",
    "Should have kept the engine the same "
  );
  Assert.equal(
    getSettingsAttribute("current"),
    "engine2",
    "Should have kept the saved attribute as the user's preference"
  );
});

add_task(async function test_experiment_setting_user_changed_back_during() {
  Services.search.defaultEngine = Services.search.getEngineByName("engine1");

  Assert.equal(
    Services.search.defaultEngine.name,
    "engine1",
    "Should have the user selected engine as default"
  );
  Assert.equal(
    getSettingsAttribute("current"),
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
    getSettingsAttribute("current"),
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
  Assert.equal(getSettingsAttribute("current"), "engine1");

  // Ending the experiment should keep the original default and reset the
  // saved attribute.
  await switchExperiment("");

  Assert.equal(Services.search.appDefaultEngine.name, "engine1");
  Assert.equal(
    Services.search.defaultEngine.name,
    "engine1",
    "Should have kept the engine the same "
  );
  Assert.equal(
    getSettingsAttribute("current"),
    "",
    "Should have reset the saved attribute to empty after the experiment ended"
  );
});

add_task(async function test_experiment_setting_user_changed_to_other_during() {
  // Set the engine to non-application default.
  // Apply the experiment setting & check
  // Set the engine to a different engine.
  // Check what happens when reverting to non-experiment.
});

add_task(async function test_experiment_setting_user_changed_back_private() {
  Services.search.defaultEngine = Services.search.getEngineByName("engine1");

  Assert.equal(
    Services.search.defaultPrivateEngine.name,
    "engine1",
    "Should have the user selected engine as default"
  );
  Assert.equal(
    getSettingsAttribute("private"),
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
    getSettingsAttribute("current"),
    "",
    "Should still have an empty settings attribute"
  );

  // User resets to the original default engine.
  Services.search.defaultPrivateEngine = Services.search.getEngineByName(
    "engine1"
  );
  Assert.equal(
    Services.search.defaultPrivateEngine.name,
    "engine1",
    "Should have the user selected engine as default"
  );
  Assert.equal(getSettingsAttribute("private"), "engine1");

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
    getSettingsAttribute("private"),
    "",
    "Should have reset the saved attribute to empty after the experiment ended"
  );
});
