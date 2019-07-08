/* import-globals-from ../../../common/tests/unit/head_helpers.js */

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
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
    '{"data":[{"id":"1","title":"title 1"},{"id":"2","title":"title b"}],"last_modified":"42"}',
    serialized
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
  Assert.equal("TypeError: localRecords is null", error.message);
});
