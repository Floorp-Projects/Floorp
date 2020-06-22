/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.defineModuleGetter(
  this,
  "LoginStore",
  "resource://gre/modules/LoginStore.jsm"
);
const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

/**
 * Tests collecting telemetry when login store is missing.
 */
add_task(async function test_login_store_missing_telemetry() {
  // Clear events recorded in the jsonfile category during startup.
  Services.telemetry.clearEvents();

  // Check that logins.json does not exist.
  let loginsStorePath = OS.Path.join(
    OS.Constants.Path.profileDir,
    "logins.json"
  );
  Assert.equal(false, await OS.File.exists(loginsStorePath));

  // Create a LoginStore.jsm object and try to load logins.json.
  let store = new LoginStore(loginsStorePath);
  await store.load();

  TelemetryTestUtils.assertEvents(
    [["jsonfile", "load", "logins"]],
    {},
    { clear: true }
  );
});

/**
 * Tests collecting telemetry when the login store is corrupt.
 */
add_task(async function test_login_store_corrupt_telemetry() {
  let loginsStorePath = OS.Path.join(
    OS.Constants.Path.profileDir,
    "logins.json"
  );
  let store = new LoginStore(loginsStorePath);

  let string = '{"logins":[{"hostname":"http://www.example.com","id":1,';
  await OS.File.writeAtomic(store.path, new TextEncoder().encode(string));
  await store.load();

  // A .corrupt file should have been created.
  Assert.ok(await OS.File.exists(store.path + ".corrupt"));

  TelemetryTestUtils.assertEvents(
    [
      ["jsonfile", "load", "logins", ""],
      ["jsonfile", "load", "logins", "invalid_json"],
    ],
    {},
    { clear: true }
  );

  // Clean up.
  await OS.File.remove(store.path + ".corrupt");
  await OS.File.remove(store.path);
});
