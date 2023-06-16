"use strict";

const {
  ExtensionPermissions,
  OLD_RKV_DIRNAME,
  RKV_DIRNAME,
  VERSION_KEY,
  VERSION_VALUE,
} = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionPermissions.sys.mjs"
);
const { FileTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/FileTestUtils.sys.mjs"
);
const { FileUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/FileUtils.sys.mjs"
);
const { KeyValueService } = ChromeUtils.importESModule(
  "resource://gre/modules/kvstore.sys.mjs"
);

add_setup(async () => {
  // Bug 1646182: Force ExtensionPermissions to run in rkv mode, because this
  // test does not make sense with the legacy method (which will be removed in
  // the above bug).
  ExtensionPermissions._useLegacyStorageBackend = false;
  await ExtensionPermissions._uninit();
});

// NOTE: this test lives in its own test file to make sure it is isolated
// from other tests that would be creating the kvstore instance and
// would prevent this test to properly simulate the kvstore path migration.
add_task(async function test_migrate_to_separate_kvstore_store_path() {
  const ADDON_ID_01 = "test-addon-01@test-extension";
  const ADDON_ID_02 = "test-addon-02@test-extension";
  // This third test extension is only used as the one that should
  // have some content scripts stored in ExtensionScriptingStore.
  const ADDON_ID_03 = "test-addon-03@test-extension";

  const oldStorePath = FileUtils.getDir("ProfD", [OLD_RKV_DIRNAME]).path;
  const newStorePath = FileUtils.getDir("ProfD", [RKV_DIRNAME]).path;

  // Verify that we are going to be using the expected backend, and that
  // the rkv path migration is only enabled by default in Nightly builds.
  info("Verify test environment match the expected pre-conditions");

  const permsStore = ExtensionPermissions._getStore();
  equal(
    permsStore.constructor.name,
    "PermissionStore",
    "active ExtensionPermissions store should be an instance of PermissionStore"
  );

  equal(
    permsStore._shouldMigrateFromOldKVStorePath,
    AppConstants.NIGHTLY_BUILD,
    "ExtensionPermissions rkv migration expected to be enabled by default only in Nightly"
  );

  info(
    "Uninitialize ExtensionPermissions and make sure no existing kvstore dir"
  );
  await ExtensionPermissions._uninit({ recreateStore: false });
  equal(
    ExtensionPermissions._getStore(),
    null,
    "PermissionStore has been nullified"
  );
  await IOUtils.remove(oldStorePath, { ignoreAbsent: true, recursive: true });
  await IOUtils.remove(newStorePath, { ignoreAbsent: true, recursive: true });

  info("Create an existing kvstore dir on the old path");

  // Populated the kvstore with some expected permissions.
  const expectedPermsAddon01 = {
    permissions: ["tabs"],
    origins: ["http://*/*"],
  };
  const expectedPermsAddon02 = {
    permissions: ["proxy"],
    origins: ["https://*/*"],
  };

  const expectedScriptAddon01 = {
    id: "script-addon-01",
    allFrames: false,
    matches: ["<all_urls>"],
    js: ["/test-script-addon-01.js"],
    persistAcrossSessions: true,
    runAt: "document_end",
  };

  const expectedScriptAddon02 = {
    id: "script-addon-02",
    allFrames: false,
    matches: ["<all_urls"],
    css: ["/test-script-addon-02.css"],
    persistAcrossSessions: true,
    runAt: "document_start",
  };

  {
    // Make sure the folder exists
    await IOUtils.makeDirectory(oldStorePath, { ignoreExisting: true });
    // Create a permission kvstore dir on the old file path.
    const kvstore = await KeyValueService.getOrCreate(
      oldStorePath,
      "permissions"
    );
    await kvstore.writeMany([
      ["_version", 1],
      [`id-${ADDON_ID_01}`, JSON.stringify(expectedPermsAddon01)],
      [`id-${ADDON_ID_02}`, JSON.stringify(expectedPermsAddon02)],
    ]);
  }

  {
    // Add also scripting kvstore data into the same temp dir path.
    const kvstore = await KeyValueService.getOrCreate(
      oldStorePath,
      "scripting-contentScripts"
    );
    await kvstore.writeMany([
      [
        `${ADDON_ID_03}/${expectedScriptAddon01.id}`,
        JSON.stringify(expectedScriptAddon01),
      ],
      [
        `${ADDON_ID_03}/${expectedScriptAddon02.id}`,
        JSON.stringify(expectedScriptAddon02),
      ],
    ]);
  }

  ok(
    await IOUtils.exists(oldStorePath),
    "Found kvstore dir for the old store path"
  );
  ok(
    !(await IOUtils.exists(newStorePath)),
    "Expect kvstore dir for the new store path to don't exist yet"
  );

  info("Re-initialize the ExtensionPermission store and assert migrated data");
  await ExtensionPermissions._uninit({ recreateStore: true });

  // Explicitly enable migration (needed to make sure we hit the migration code
  // that is only enabled by default on Nightly).
  if (!AppConstants.NIGHTLY_BUILD) {
    info("Enable ExtensionPermissions rkv migration on non-nightly channel");
    const newStoreInstance = ExtensionPermissions._getStore();
    newStoreInstance._shouldMigrateFromOldKVStorePath = true;
  }

  const permsAddon01 = await ExtensionPermissions._get(ADDON_ID_01);
  const permsAddon02 = await ExtensionPermissions._get(ADDON_ID_02);

  Assert.deepEqual(
    { permsAddon01, permsAddon02 },
    {
      permsAddon01: expectedPermsAddon01,
      permsAddon02: expectedPermsAddon02,
    },
    "Got the expected permissions migrated to the new store file path"
  );

  await ExtensionPermissions._uninit({ recreateStore: false });

  ok(
    await IOUtils.exists(newStorePath),
    "Found kvstore dir for the new store path"
  );

  {
    const newKVStore = await KeyValueService.getOrCreate(
      newStorePath,
      "permissions"
    );
    Assert.equal(
      await newKVStore.get(VERSION_KEY),
      VERSION_VALUE,
      "Got the expected value set on the kvstore _version key"
    );
  }

  // kvstore internally caching behavior doesn't make it easy to make sure
  // we would be hitting a failure if the ExtensionPermissions kvstore migration
  // would be mistakenly removing the old kvstore dir as part of that migration,
  // and so the test case is explicitly verifying that the directory does still
  // exist and then it copies it into a new path to confirm that the expected
  // data have been kept in the old kvstore dir.
  ok(
    await IOUtils.exists(oldStorePath),
    "Found kvstore dir for the old store path"
  );
  const oldStoreCopiedPath = FileTestUtils.getTempFile("kvstore-dir").path;
  await IOUtils.copy(oldStorePath, oldStoreCopiedPath, { recursive: true });

  // Confirm that the content scripts have not been copied into
  // the new kvstore path.
  async function assertStoredContentScripts(storePath, expectedKeys) {
    const kvstore = await KeyValueService.getOrCreate(
      storePath,
      "scripting-contentScripts"
    );
    const enumerator = await kvstore.enumerate();
    const keys = [];
    while (enumerator.hasMoreElements()) {
      keys.push(enumerator.getNext().key);
    }
    Assert.deepEqual(
      keys,
      expectedKeys,
      `Got the expected scripts in the kvstore path ${storePath}`
    );
  }

  info(
    "Verify that no content scripts are stored in the new kvstore dir reserved for permissions"
  );
  await assertStoredContentScripts(newStorePath, []);
  info(
    "Verify that existing content scripts have been not been removed old kvstore dir"
  );
  await assertStoredContentScripts(oldStoreCopiedPath, [
    `${ADDON_ID_03}/${expectedScriptAddon01.id}`,
    `${ADDON_ID_03}/${expectedScriptAddon02.id}`,
  ]);

  await ExtensionPermissions._uninit({ recreateStore: true });

  await IOUtils.remove(newStorePath, { recursive: true });
  await IOUtils.remove(oldStorePath, { recursive: true });
});
