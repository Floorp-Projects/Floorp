/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests overriding the FormAutofillIntegration module functions.
 */

"use strict";

/**
 * The requestAutocomplete UI will not be displayed during these tests.
 */
add_task(function* test_initialize() {
  FormAutofillTest.requestAutocompleteResponse = { canceled: true };
});

/**
 * Registers and unregisters an integration override function.
 */
add_task(function* test_integration_override() {
  let overrideCalled = false;

  let newIntegrationFn = base => ({
    createRequestAutocompleteUI: Task.async(function* () {
      overrideCalled = true;
      return yield base.createRequestAutocompleteUI.apply(this, arguments);
    }),
  });

  FormAutofill.registerIntegration(newIntegrationFn);
  try {
    let ui = yield FormAutofill.integration.createRequestAutocompleteUI({});
    let result = yield ui.show();
    Assert.ok(result.canceled);
  } finally {
    FormAutofill.unregisterIntegration(newIntegrationFn);
  }

  Assert.ok(overrideCalled);
});

/**
 * Registers an integration override function that throws an exception, and
 * ensures that this does not block other functions from being registered.
 */
add_task(function* test_integration_override_error() {
  let overrideCalled = false;

  let errorIntegrationFn = base => { throw "Expected error." };

  let newIntegrationFn = base => ({
    createRequestAutocompleteUI: Task.async(function* () {
      overrideCalled = true;
      return yield base.createRequestAutocompleteUI.apply(this, arguments);
    }),
  });

  FormAutofill.registerIntegration(errorIntegrationFn);
  FormAutofill.registerIntegration(newIntegrationFn);
  try {
    let ui = yield FormAutofill.integration.createRequestAutocompleteUI({});
    let result = yield ui.show();
    Assert.ok(result.canceled);
  } finally {
    FormAutofill.unregisterIntegration(errorIntegrationFn);
    FormAutofill.unregisterIntegration(newIntegrationFn);
  }

  Assert.ok(overrideCalled);
});

add_task(terminationTaskFn);
