/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Import the rust-based and kinto-based implementations. Not great to grab
// these as they're somewhat private, but we want to run the pings through our
// validation machinery which is here in the sync test code.
const { extensionStorageSync: rustImpl } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionStorageSync.sys.mjs"
);
const { extensionStorageSyncKinto: kintoImpl } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionStorageSyncKinto.sys.mjs"
);

const { Service } = ChromeUtils.importESModule(
  "resource://services-sync/service.sys.mjs"
);
const { ExtensionStorageEngineBridge } = ChromeUtils.importESModule(
  "resource://services-sync/engines/extension-storage.sys.mjs"
);

Services.prefs.setBoolPref("webextensions.storage.sync.kinto", false);
Services.prefs.setStringPref("webextensions.storage.sync.log.level", "debug");

// It's tricky to force error cases here (the databases are opened with
// exclusive locks) and that part of the code has coverage in the vendored
// application-services webext-storage crate. So this just tests that the
// migration data ends up in the ping, and exactly once.
add_task(async function test_sync_migration_telem() {
  // Set some stuff using the kinto-based impl prior to fully setting up sync.
  let e1 = { id: "test@mozilla.com" };
  let c1 = { extension: e1, callOnClose() {} };

  let e2 = { id: "test-2@mozilla.com" };
  let c2 = { extension: e2, callOnClose() {} };
  await kintoImpl.set(e1, { foo: "bar" }, c1);
  await kintoImpl.set(e1, { baz: "quux" }, c1);
  await kintoImpl.set(e2, { second: "2nd" }, c2);

  Assert.deepEqual(await rustImpl.get(e1, "foo", c1), { foo: "bar" });
  Assert.deepEqual(await rustImpl.get(e1, "baz", c1), { baz: "quux" });
  Assert.deepEqual(await rustImpl.get(e2, null, c2), { second: "2nd" });

  // Explicitly unregister first. It's very possible this isn't needed for this
  // case, however it's fairly harmless, we hope to uplift this patch to beta,
  // and earlier today we had beta-only problems caused by this (bug 1629116)
  await Service.engineManager.unregister("extension-storage");
  await Service.engineManager.register(ExtensionStorageEngineBridge);
  let engine = Service.engineManager.get("extension-storage");
  let server = await serverForFoo(engine, undefined);
  try {
    await SyncTestingInfrastructure(server);
    await Service.engineManager.switchAlternatives();

    _("First sync");
    let ping = await sync_engine_and_validate_telem(engine, false, null, true);
    Assert.deepEqual(ping.migrations, [
      {
        type: "webext-storage",
        entries: 3,
        entriesSuccessful: 3,
        extensions: 2,
        extensionsSuccessful: 2,
        openFailure: false,
      },
    ]);

    // force another sync
    await engine.setLastSync(0);
    _("Second sync");

    ping = await sync_engine_and_validate_telem(engine, false, null, true);
    Assert.deepEqual(ping.migrations, undefined);
  } finally {
    await kintoImpl.clear(e1, c1);
    await kintoImpl.clear(e2, c2);
    await rustImpl.clear(e1, c1);
    await rustImpl.clear(e2, c2);
    await promiseStopServer(server);
    await engine.finalize();
  }
});
