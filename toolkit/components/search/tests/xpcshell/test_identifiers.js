/* Any copyright is dedicated to the Public Domain.
 *    http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test of a search engine's identifier.
 */

"use strict";

add_setup(async function () {
  await SearchTestUtils.useTestEngines("simple-engines");
  await AddonTestUtils.promiseStartupManager();

  const result = await Services.search.init();
  Assert.ok(
    Components.isSuccessCode(result),
    "Should have initialized the service"
  );

  useHttpServer();
  await SearchTestUtils.promiseNewSearchEngine({
    url: `${gDataUrl}engine.xml`,
  });
});

function checkIdentifier(engineName, expectedIdentifier, expectedTelemetryId) {
  const engine = Services.search.getEngineByName(engineName);
  Assert.ok(
    engine instanceof Ci.nsISearchEngine,
    "Should be derived from nsISearchEngine"
  );

  Assert.equal(
    engine.identifier,
    expectedIdentifier,
    "Should have the correct identifier"
  );

  Assert.equal(
    engine.telemetryId,
    expectedTelemetryId,
    "Should have the correct telemetry Id"
  );
}

add_task(async function test_from_profile() {
  // An engine loaded from the profile directory won't have an identifier,
  // because it's not built-in.
  checkIdentifier(kTestEngineName, null, `other-${kTestEngineName}`);
});

add_task(async function test_from_telemetry_id() {
  checkIdentifier("basic", "telemetry", "telemetry");
});

add_task(async function test_from_webextension_id() {
  // If not specified, the telemetry Id is derived from the WebExtension prefix,
  // it should not use the WebExtension display name.
  checkIdentifier("Simple Engine", "simple", "simple");
});
