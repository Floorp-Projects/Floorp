/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm", this);
ChromeUtils.import("resource://featuregates/FeatureGate.jsm", this);
ChromeUtils.import(
  "resource://featuregates/FeatureGateImplementation.jsm",
  this
);
ChromeUtils.import("resource://testing-common/httpd.js", this);

const kDefinitionDefaults = {
  id: "test-feature",
  title: "Test Feature",
  description: "A feature for testing",
  restartRequired: false,
  type: "boolean",
  preference: "test.feature",
  defaultValue: false,
  isPublic: false,
};

function definitionFactory(override = {}) {
  return Object.assign({}, kDefinitionDefaults, override);
}

class DefinitionServer {
  constructor(definitionOverrides = []) {
    this.server = new HttpServer();
    this.server.registerPathHandler("/definitions.json", this);
    this.definitions = {};

    for (const override of definitionOverrides) {
      this.addDefinition(override);
    }

    this.server.start();
    registerCleanupFunction(
      () => new Promise(resolve => this.server.stop(resolve))
    );
  }

  // for nsIHttpRequestHandler
  handle(request, response) {
    // response.setHeader("Content-Type", "application/json");
    response.write(JSON.stringify(this.definitions));
  }

  get definitionsUrl() {
    const { primaryScheme, primaryHost, primaryPort } = this.server.identity;
    return `${primaryScheme}://${primaryHost}:${primaryPort}/definitions.json`;
  }

  addDefinition(overrides = {}) {
    const definition = definitionFactory(overrides);
    // convert targeted values, used by fromId
    definition.isPublic = { default: definition.isPublic };
    definition.defaultValue = { default: definition.defaultValue };
    this.definitions[definition.id] = definition;
    return definition;
  }
}

// getValue should work
add_task(async function testGetValue() {
  const preference = "test.pref";
  equal(
    Services.prefs.getPrefType(preference),
    Services.prefs.PREF_INVALID,
    "Before creating the feature gate, the preference should not exist"
  );
  const feature = new FeatureGateImplementation(
    definitionFactory({ preference, defaultValue: false })
  );
  equal(
    Services.prefs.getPrefType(preference),
    Services.prefs.PREF_INVALID,
    "Instantiating a feature gate should not set its default value"
  );
  equal(
    await feature.getValue(),
    false,
    "getValue() should return the feature gate's default"
  );

  Services.prefs.setBoolPref(preference, true);
  equal(
    await feature.getValue(),
    true,
    "getValue() should return the new value"
  );

  Services.prefs.setBoolPref(preference, false);
  equal(
    await feature.getValue(),
    false,
    "getValue() should return the third value"
  );

  // cleanup
  Services.prefs.getDefaultBranch("").deleteBranch(preference);
});

// event observers should work
add_task(async function testGetValue() {
  const preference = "test.pref";
  const feature = new FeatureGateImplementation(
    definitionFactory({ preference, defaultValue: false })
  );
  const observer = {
    onChange: sinon.stub(),
    onEnable: sinon.stub(),
    onDisable: sinon.stub(),
  };

  let rv = await feature.addObserver(observer);
  equal(rv, false, "addObserver returns the current value");

  Assert.deepEqual(observer.onChange.args, [], "onChange should not be called");
  Assert.deepEqual(observer.onEnable.args, [], "onEnable should not be called");
  Assert.deepEqual(
    observer.onDisable.args,
    [],
    "onDisable should not be called"
  );

  Services.prefs.setBoolPref(preference, true);
  await Promise.resolve(); // Allow events to be called async
  Assert.deepEqual(
    observer.onChange.args,
    [[true]],
    "onChange should be called with the new value"
  );
  Assert.deepEqual(observer.onEnable.args, [[]], "onEnable should be called");
  Assert.deepEqual(
    observer.onDisable.args,
    [],
    "onDisable should not be called"
  );

  Services.prefs.setBoolPref(preference, false);
  await Promise.resolve(); // Allow events to be called async
  Assert.deepEqual(
    observer.onChange.args,
    [[true], [false]],
    "onChange should be called again with the new value"
  );
  Assert.deepEqual(
    observer.onEnable.args,
    [[]],
    "onEnable should not be called a second time"
  );
  Assert.deepEqual(
    observer.onDisable.args,
    [[]],
    "onDisable should be called for the first time"
  );

  Services.prefs.setBoolPref(preference, false);
  await Promise.resolve(); // Allow events to be called async
  Assert.deepEqual(
    observer.onChange.args,
    [[true], [false]],
    "onChange should not be called if the value did not change"
  );
  Assert.deepEqual(
    observer.onEnable.args,
    [[]],
    "onEnable should not be called again if the value did not change"
  );
  Assert.deepEqual(
    observer.onDisable.args,
    [[]],
    "onDisable should not be called if the value did not change"
  );

  // cleanup
  feature.removeAllObservers();
  Services.prefs.getDefaultBranch("").deleteBranch(preference);
});
