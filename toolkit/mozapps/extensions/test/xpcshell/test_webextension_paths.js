function run_test() {
  run_next_test();
}

let profileDir;
add_task(function* setup() {
  profileDir = gProfD.clone();
  profileDir.append("extensions");

  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
  startupManager();
});

// When installing an unpacked addon we derive the ID from the
// directory name.  Make sure that if the directoy name is not a valid
// addon ID that we reject it.
add_task(function* test_bad_unpacked_path() {
  let MANIFEST_ID = "webext_bad_path@tests.mozilla.org";

  let manifest = {
    name: "path test",
    description: "test of a bad directory name",
    manifest_version: 2,
    version: "1.0",

    browser_specific_settings: {
      gecko: {
        id: MANIFEST_ID
      }
    }
  };

  const directories = [
    "not a valid ID",
    '"quotes"@tests.mozilla.org',
  ];

  for (let dir of directories) {
    try {
      yield promiseWriteWebManifestForExtension(manifest, profileDir, dir);
    } catch (ex) {
      // This can fail if the underlying filesystem (looking at you windows)
      // doesn't handle some of the characters in the ID.  In that case,
      // just ignore this test on this platform.
      continue;
    }
    yield promiseRestartManager();

    let addon = yield promiseAddonByID(dir);
    do_check_eq(addon, null);
    addon = yield promiseAddonByID(MANIFEST_ID);
    do_check_eq(addon, null);
  }
});

