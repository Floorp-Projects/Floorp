/* import-globals-from ../../../common/tests/unit/head_helpers.js */

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { TestUtils } = ChromeUtils.import(
  "resource://testing-common/TestUtils.jsm"
);

const { RemoteSettingsWorker } = ChromeUtils.import(
  "resource://services-settings/RemoteSettingsWorker.jsm"
);
const { Kinto } = ChromeUtils.import(
  "resource://services-common/kinto-offline-client.js"
);

const IS_ANDROID = AppConstants.platform == "android";

add_task(async function test_canonicaljson_merges_remote_into_local() {
  const localRecords = [
    { id: "1", title: "title 1" },
    { id: "2", title: "title 2" },
    { id: "3", title: "title 3" },
  ];
  const remoteRecords = [
    { id: "2", title: "title b" },
    { id: "3", deleted: true },
  ];
  const timestamp = 42;

  const serialized = await RemoteSettingsWorker.canonicalStringify(
    localRecords,
    remoteRecords,
    timestamp
  );

  Assert.equal(
    serialized,
    '{"data":[{"id":"1","title":"title 1"},{"id":"2","title":"title b"}],"last_modified":"42"}'
  );
});

add_task(async function test_import_json_dump_into_idb() {
  if (IS_ANDROID) {
    // Skip test: we don't ship remote settings dumps on Android (see package-manifest).
    return;
  }
  const kintoCollection = new Kinto({
    bucket: "main",
    adapter: Kinto.adapters.IDB,
    adapterOptions: { dbName: "remote-settings" },
  }).collection("language-dictionaries");
  const { data: before } = await kintoCollection.list();
  Assert.equal(before.length, 0);

  await RemoteSettingsWorker.importJSONDump("main", "language-dictionaries");

  const { data: after } = await kintoCollection.list();
  Assert.ok(after.length > 0);
});

add_task(async function test_throws_error_if_worker_fails() {
  let error;
  try {
    await RemoteSettingsWorker.canonicalStringify(null, [], 42);
  } catch (e) {
    error = e;
  }
  Assert.equal(error.message.endsWith("localRecords is null"), true);
});

add_task(async function test_stops_worker_after_timeout() {
  // Change the idle time.
  Services.prefs.setIntPref(
    "services.settings.worker_idle_max_milliseconds",
    1
  );
  // Run a task:
  let serialized = await RemoteSettingsWorker.canonicalStringify([], [], 42);
  Assert.equal(serialized, '{"data":[],"last_modified":"42"}', "API works.");
  // Check that the worker gets stopped now the task is done:
  await TestUtils.waitForCondition(() => !RemoteSettingsWorker.worker);
  // Ensure the worker stays alive for 10 minutes instead:
  Services.prefs.setIntPref(
    "services.settings.worker_idle_max_milliseconds",
    600000
  );
  // Run another task:
  serialized = await RemoteSettingsWorker.canonicalStringify([], [], 42);
  Assert.equal(
    serialized,
    '{"data":[],"last_modified":"42"}',
    "API still works."
  );
  Assert.ok(RemoteSettingsWorker.worker, "Worker should stay alive a bit.");

  // Clear the pref.
  Services.prefs.clearUserPref(
    "services.settings.worker_idle_max_milliseconds"
  );
});
