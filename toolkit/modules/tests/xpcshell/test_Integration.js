/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests the Integration.jsm module.
 */

"use strict";
ChromeUtils.import("resource://gre/modules/Integration.jsm", this);

const TestIntegration = {
  value: "value",

  get valueFromThis() {
    return this.value;
  },

  get property() {
    return this._property;
  },

  set property(value) {
    this._property = value;
  },

  method(argument) {
    this.methodArgument = argument;
    return "method" + argument;
  },

  async asyncMethod(argument) {
    this.asyncMethodArgument = argument;
    return "asyncMethod" + argument;
  },
};

let overrideFn = base => ({
  value: "overridden-value",

  get property() {
    return "overridden-" + base.__lookupGetter__("property").call(this);
  },

  set property(value) {
    base.__lookupSetter__("property").call(this, "overridden-" + value);
  },

  method() {
    return "overridden-" + base.method.apply(this, arguments);
  },

  async asyncMethod() {
    return "overridden-" + (await base.asyncMethod.apply(this, arguments));
  },
});

let superOverrideFn = base => ({
  __proto__: base,

  value: "overridden-value",

  get property() {
    return "overridden-" + super.property;
  },

  set property(value) {
    super.property = "overridden-" + value;
  },

  method() {
    return "overridden-" + super.method(...arguments);
  },

  async asyncMethod() {
    // We cannot use the "super" keyword in methods defined using "Task.async".
    return "overridden-" + (await base.asyncMethod.apply(this, arguments));
  },
});

/**
 * Fails the test if the results of method invocations on the combined object
 * don't match the expected results based on how many overrides are registered.
 *
 * @param combined
 *        The combined object based on the TestIntegration root.
 * @param overridesCount
 *        Zero if the root object is not overridden, or a higher value to test
 *        the presence of one or more integration overrides.
 */
async function assertCombinedResults(combined, overridesCount) {
  let expectedValue = overridesCount > 0 ? "overridden-value" : "value";
  let prefix = "overridden-".repeat(overridesCount);

  Assert.equal(combined.value, expectedValue);
  Assert.equal(combined.valueFromThis, expectedValue);

  combined.property = "property";
  Assert.equal(combined.property, prefix.repeat(2) + "property");

  combined.methodArgument = "";
  Assert.equal(combined.method("-argument"), prefix + "method-argument");
  Assert.equal(combined.methodArgument, "-argument");

  combined.asyncMethodArgument = "";
  Assert.equal(
    await combined.asyncMethod("-argument"),
    prefix + "asyncMethod-argument"
  );
  Assert.equal(combined.asyncMethodArgument, "-argument");
}

/**
 * Fails the test if the results of method invocations on the combined object
 * for the "testModule" integration point don't match the expected results based
 * on how many overrides are registered.
 *
 * @param overridesCount
 *        Zero if the root object is not overridden, or a higher value to test
 *        the presence of one or more integration overrides.
 */
async function assertCurrentCombinedResults(overridesCount) {
  let combined = Integration.testModule.getCombined(TestIntegration);
  await assertCombinedResults(combined, overridesCount);
}

/**
 * Checks the initial state with no integration override functions registered.
 */
add_task(async function test_base() {
  await assertCurrentCombinedResults(0);
});

/**
 * Registers and unregisters an integration override function.
 */
add_task(async function test_override() {
  Integration.testModule.register(overrideFn);
  await assertCurrentCombinedResults(1);

  // Registering the same function more than once has no effect.
  Integration.testModule.register(overrideFn);
  await assertCurrentCombinedResults(1);

  Integration.testModule.unregister(overrideFn);
  await assertCurrentCombinedResults(0);
});

/**
 * Registers and unregisters more than one integration override function, of
 * which one uses the prototype and the "super" keyword to access the base.
 */
add_task(async function test_override_super_multiple() {
  Integration.testModule.register(overrideFn);
  Integration.testModule.register(superOverrideFn);
  await assertCurrentCombinedResults(2);

  Integration.testModule.unregister(overrideFn);
  await assertCurrentCombinedResults(1);

  Integration.testModule.unregister(superOverrideFn);
  await assertCurrentCombinedResults(0);
});

/**
 * Registers an integration override function that throws an exception, and
 * ensures that this does not block other functions from being registered.
 */
add_task(async function test_override_error() {
  let errorOverrideFn = base => {
    throw new Error("Expected error.");
  };

  Integration.testModule.register(errorOverrideFn);
  Integration.testModule.register(overrideFn);
  await assertCurrentCombinedResults(1);

  Integration.testModule.unregister(errorOverrideFn);
  Integration.testModule.unregister(overrideFn);
  await assertCurrentCombinedResults(0);
});

/**
 * Checks that state saved using the "this" reference is preserved as a shallow
 * copy when registering new integration override functions.
 */
add_task(async function test_state_preserved() {
  let valueObject = { toString: () => "toString" };

  let combined = Integration.testModule.getCombined(TestIntegration);
  combined.property = valueObject;
  Assert.ok(combined.property === valueObject);

  Integration.testModule.register(overrideFn);
  combined = Integration.testModule.getCombined(TestIntegration);
  Assert.equal(combined.property, "overridden-toString");

  Integration.testModule.unregister(overrideFn);
  combined = Integration.testModule.getCombined(TestIntegration);
  Assert.ok(combined.property === valueObject);
});

/**
 * Checks that the combined integration objects cannot be used with XPCOM.
 *
 * This is limited by the fact that interfaces with the "[function]" annotation,
 * for example nsIObserver, do not call the QueryInterface implementation.
 */
add_task(async function test_xpcom_throws() {
  let combined = Integration.testModule.getCombined(TestIntegration);

  // This calls QueryInterface because it looks for nsISupportsWeakReference.
  Assert.throws(
    () => Services.obs.addObserver(combined, "test-topic", true),
    /NS_NOINTERFACE/
  );
});

/**
 * Checks that getters defined by defineModuleGetter are able to retrieve the
 * latest version of the combined integration object.
 */
add_task(async function test_defineModuleGetter() {
  let objectForGetters = {};

  // Test with and without the optional "symbol" parameter.
  Integration.testModule.defineModuleGetter(
    objectForGetters,
    "TestIntegration",
    "resource://testing-common/TestIntegration.jsm"
  );
  Integration.testModule.defineModuleGetter(
    objectForGetters,
    "integration",
    "resource://testing-common/TestIntegration.jsm",
    "TestIntegration"
  );

  Integration.testModule.register(overrideFn);
  await assertCombinedResults(objectForGetters.integration, 1);
  await assertCombinedResults(objectForGetters.TestIntegration, 1);

  Integration.testModule.unregister(overrideFn);
  await assertCombinedResults(objectForGetters.integration, 0);
  await assertCombinedResults(objectForGetters.TestIntegration, 0);
});
