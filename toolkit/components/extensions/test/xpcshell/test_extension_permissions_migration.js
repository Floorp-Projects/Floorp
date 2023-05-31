"use strict";

const {
  ExtensionPermissions,
  OLD_JSON_FILENAME,
  OLD_RKV_DIRNAME,
  RKV_DIRNAME,
} = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionPermissions.sys.mjs"
);

const GOOD_JSON_FILE = {
  "wikipedia@search.mozilla.org": {
    permissions: ["internal:privateBrowsingAllowed"],
    origins: [],
  },
  "amazon@search.mozilla.org": {
    permissions: ["internal:privateBrowsingAllowed"],
    origins: [],
  },
  "doh-rollout@mozilla.org": {
    permissions: ["internal:privateBrowsingAllowed"],
    origins: [],
  },
};

const BAD_JSON_FILE = {
  "test@example.org": "what",
};

const BAD_FILE = "what is this { } {";

const gOldJSONPath = FileUtils.getDir("ProfD", [OLD_JSON_FILENAME]).path;
const gOldRkvPath = FileUtils.getDir("ProfD", [OLD_RKV_DIRNAME]).path;
const gNewRkvPath = FileUtils.getDir("ProfD", [RKV_DIRNAME]).path;

async function test_file(json, extensionIds, expected, fileDeleted) {
  await ExtensionPermissions._resetVersion();
  await ExtensionPermissions._uninit();

  await IOUtils.writeUTF8(gOldJSONPath, json);

  for (let extensionId of extensionIds) {
    let permissions = await ExtensionPermissions.get(extensionId);
    Assert.deepEqual(permissions, expected, "permissions match");
  }

  Assert.equal(
    await IOUtils.exists(gOldJSONPath),
    !fileDeleted,
    "old file was deleted"
  );

  Assert.ok(
    await IOUtils.exists(gNewRkvPath),
    "found the store at the new rkv path"
  );

  Assert.ok(
    !(await IOUtils.exists(gOldRkvPath)),
    "expect old rkv path to not exist"
  );
}

add_setup(async () => {
  // Bug 1646182: Force ExtensionPermissions to run in rkv mode, because this
  // test does not make sense with the legacy method (which will be removed in
  // the above bug).
  await ExtensionPermissions._uninit();
});

add_task(async function test_migrate_good_json() {
  let expected = {
    permissions: ["internal:privateBrowsingAllowed"],
    origins: [],
  };

  await test_file(
    JSON.stringify(GOOD_JSON_FILE),
    [
      "wikipedia@search.mozilla.org",
      "amazon@search.mozilla.org",
      "doh-rollout@mozilla.org",
    ],
    expected,
    /* fileDeleted */ true
  );
});

add_task(async function test_migrate_bad_json() {
  let expected = { permissions: [], origins: [] };

  await test_file(
    BAD_FILE,
    ["test@example.org"],
    expected,
    /* fileDeleted */ false
  );
  await IOUtils.remove(gOldJSONPath);
});

add_task(async function test_migrate_bad_file() {
  let expected = { permissions: [], origins: [] };

  await test_file(
    JSON.stringify(BAD_JSON_FILE),
    ["test2@example.org"],
    expected,
    /* fileDeleted */ false
  );
  await IOUtils.remove(gOldJSONPath);
});
