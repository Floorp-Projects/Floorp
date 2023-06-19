/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* Test that MozParam condition="pref" values used in search URLs can be set
   by Nimbus, and that their special characters are URL encoded. */

"use strict";

const { NimbusFeatures } = ChromeUtils.importESModule(
  "resource://nimbus/ExperimentAPI.sys.mjs"
);

const baseURL = "https://www.google.com/search?q=foo";

let getVariableStub;
let updateStub;

add_setup(async function () {
  updateStub = sinon.stub(NimbusFeatures.search, "onUpdate");
  getVariableStub = sinon.stub(NimbusFeatures.search, "getVariable");
  sinon.stub(NimbusFeatures.search, "ready").resolves();

  // The test engines used in this test need to be recognized as 'default'
  // engines, or their MozParams will be ignored.
  await SearchTestUtils.useTestEngines();
});

add_task(async function test_pref_initial_value() {
  // These values should match the nimbusParams below and the data/test/manifest.json
  // search engine configuration
  getVariableStub.withArgs("extraParams").returns([
    {
      key: "code",
      // The & and = in this parameter are to check that they are correctly
      // encoded, and not treated as a separate parameter.
      value: "good&id=unique",
    },
  ]);

  await AddonTestUtils.promiseStartupManager();
  await Services.search.init();

  Assert.ok(
    updateStub.called,
    "Should have called onUpdate to listen for future updates"
  );

  const engine = Services.search.getEngineByName("engine-pref");
  Assert.equal(
    engine.getSubmission("foo").uri.spec,
    baseURL + "&code=good%26id%3Dunique",
    "Should have got the submission URL with the correct code"
  );
});

add_task(async function test_pref_updated() {
  getVariableStub.withArgs("extraParams").returns([
    {
      key: "code",
      // The & and = in this parameter are to check that they are correctly
      // encoded, and not treated as a separate parameter.
      value: "supergood&id=unique123456",
    },
  ]);
  // Update the pref without re-init nor restart.
  updateStub.firstCall.args[0]();

  const engine = Services.search.getEngineByName("engine-pref");
  Assert.equal(
    engine.getSubmission("foo").uri.spec,
    baseURL + "&code=supergood%26id%3Dunique123456",
    "Should have got the submission URL with the updated code"
  );
});

add_task(async function test_multiple_params() {
  getVariableStub.withArgs("extraParams").returns([
    {
      key: "code",
      value: "sng",
    },
    {
      key: "test",
      value: "sup",
    },
  ]);
  // Update the pref without re-init nor restart.
  updateStub.firstCall.args[0]();

  let engine = Services.search.getEngineByName("engine-pref");
  Assert.equal(
    engine.getSubmission("foo").uri.spec,
    baseURL + "&code=sng&test=sup",
    "Should have got the submission URL with both parameters"
  );

  // Test removing just one of the parameters.
  getVariableStub.withArgs("extraParams").returns([
    {
      key: "code",
      value: "sng",
    },
  ]);
  // Update the pref without re-init nor restart.
  updateStub.firstCall.args[0]();

  engine = Services.search.getEngineByName("engine-pref");
  Assert.equal(
    engine.getSubmission("foo").uri.spec,
    baseURL + "&code=sng",
    "Should have got the submission URL with one parameter"
  );
});

add_task(async function test_pref_cleared() {
  // Update the pref without re-init nor restart.
  // Note you can't delete a preference from the default branch.
  getVariableStub.withArgs("extraParams").returns([]);
  updateStub.firstCall.args[0]();

  let engine = Services.search.getEngineByName("engine-pref");
  Assert.equal(
    engine.getSubmission("foo").uri.spec,
    baseURL,
    "Should have just the base URL after the pref was cleared"
  );
});
