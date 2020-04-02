/* Any copyright is dedicated to the Public Domain.
 *    http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests covering nsISearchService::addEngine's optional callback.
 */

const { MockRegistrar } = ChromeUtils.import(
  "resource://testing-common/MockRegistrar.jsm"
);

("use strict");

// Only need to stub the methods actually called by nsSearchService
var promptService = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIPromptService]),
  confirmEx() {},
};
var prompt = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIPrompt]),
  alert() {},
};
// Override the prompt service and nsIPrompt, since the search service currently
// prompts in response to certain installation failures we test here
// XXX this should disappear once bug 863474 is fixed
MockRegistrar.register(
  "@mozilla.org/embedcomp/prompt-service;1",
  promptService
);
MockRegistrar.register("@mozilla.org/prompter;1", prompt);

// First test inits the search service
add_task(async function init_search_service() {
  useHttpServer();
  await AddonTestUtils.promiseStartupManager();
});

// Simple test of the search callback
add_task(async function simple_callback_test() {
  let engine = await Services.search.addEngine(
    gDataUrl + "engine.xml",
    null,
    false
  );
  Assert.ok(!!engine);
  Assert.notEqual(engine.name, (await Services.search.getDefault()).name);
  Assert.equal(
    engine.wrappedJSObject._loadPath,
    "[http]localhost/test-search-engine.xml"
  );
});

// Test of the search callback on duplicate engine failures
add_task(async function duplicate_failure_test() {
  // Re-add the same engine added in the previous test
  let engine;
  try {
    engine = await Services.search.addEngine(
      gDataUrl + "engine.xml",
      null,
      false
    );
  } catch (ex) {
    let errorCode = ex.result;
    Assert.ok(!!errorCode);
    Assert.equal(errorCode, Ci.nsISearchService.ERROR_DUPLICATE_ENGINE);
  } finally {
    Assert.ok(!engine);
  }
});

// Test of the search callback on failure to load the engine failures
add_task(async function load_failure_test() {
  // Try adding an engine that doesn't exist.
  let engine;
  try {
    engine = await Services.search.addEngine(
      "http://invalid/data/engine.xml",
      null,
      false
    );
  } catch (ex) {
    let errorCode = ex.result;
    Assert.ok(!!errorCode);
    Assert.equal(errorCode, Ci.nsISearchService.ERROR_UNKNOWN_FAILURE);
  } finally {
    Assert.ok(!engine);
  }
});
