"use strict";

const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");
const { ExtensionPermissions } = ChromeUtils.import(
  "resource://gre/modules/ExtensionPermissions.jsm"
);

add_task(async function setup() {
  // Bug 1646182: Force ExtensionPermissions to run in rkv mode, because this
  // test does not make sense with the legacy method (which will be removed in
  // the above bug).
  await ExtensionPermissions._uninit();
});

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

const gOldSettingsJSON = do_get_profile().clone();
gOldSettingsJSON.append("extension-preferences.json");

async function test_file(json, extensionIds, expected, fileDeleted) {
  await ExtensionPermissions._resetVersion();
  await ExtensionPermissions._uninit();

  await OS.File.writeAtomic(gOldSettingsJSON.path, json, {
    encoding: "utf-8",
  });

  for (let extensionId of extensionIds) {
    let permissions = await ExtensionPermissions.get(extensionId);
    Assert.deepEqual(permissions, expected, "permissions match");
  }

  Assert.equal(
    await OS.File.exists(gOldSettingsJSON.path),
    !fileDeleted,
    "old file was deleted"
  );
}

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
  await OS.File.remove(gOldSettingsJSON.path);
});

add_task(async function test_migrate_bad_file() {
  let expected = { permissions: [], origins: [] };

  await test_file(
    JSON.stringify(BAD_JSON_FILE),
    ["test2@example.org"],
    expected,
    /* fileDeleted */ false
  );
  await OS.File.remove(gOldSettingsJSON.path);
});
