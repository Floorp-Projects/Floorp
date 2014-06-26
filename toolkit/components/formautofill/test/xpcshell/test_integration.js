/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests overriding the FormAutofillIntegration module functions.
 */

"use strict";

/**
 * Registers and unregisters an integration override function.
 */
add_task(function* test_integration_override() {
  let overrideCalled = false;

  let newIntegrationFn = base => ({
    createRequestAutocompleteUI: Task.async(function* () {
      yield base.createRequestAutocompleteUI.apply(this, arguments);
      overrideCalled = true;
    }),
  });

  FormAutofill.registerIntegration(newIntegrationFn);
  try {
    yield FormAutofill.integration.createRequestAutocompleteUI({});
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
      yield base.createRequestAutocompleteUI.apply(this, arguments);
      overrideCalled = true;
    }),
  });

  FormAutofill.registerIntegration(errorIntegrationFn);
  FormAutofill.registerIntegration(newIntegrationFn);
  try {
    yield FormAutofill.integration.createRequestAutocompleteUI({});
  } finally {
    FormAutofill.unregisterIntegration(errorIntegrationFn);
    FormAutofill.unregisterIntegration(newIntegrationFn);
  }

  Assert.ok(overrideCalled);
});

add_task(terminationTaskFn);
