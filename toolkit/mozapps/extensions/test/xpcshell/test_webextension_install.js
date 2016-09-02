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

const IMPLICIT_ID_XPI = "data/webext-implicit-id.xpi";
const IMPLICIT_ID_ID = "webext_implicit_id@tests.mozilla.org";

// webext-implicit-id.xpi has a minimal manifest with no
// applications or browser_specific_settings, so its id comes
// from its signature, which should be the ID constant defined below.
add_task(function* test_implicit_id() {
  let addon = yield promiseAddonByID(IMPLICIT_ID_ID);
  do_check_eq(addon, null);

  let xpifile = do_get_file(IMPLICIT_ID_XPI);
  yield promiseInstallAllFiles([xpifile]);

  addon = yield promiseAddonByID(IMPLICIT_ID_ID);
  do_check_neq(addon, null);

  addon.uninstall();
});

// We should also be able to install webext-implicit-id.xpi temporarily
// and it should look just like the regular install (ie, the ID should
// come from the signature)
add_task(function* test_implicit_id_temp() {
  let addon = yield promiseAddonByID(IMPLICIT_ID_ID);
  do_check_eq(addon, null);

  let xpifile = do_get_file(IMPLICIT_ID_XPI);
  yield AddonManager.installTemporaryAddon(xpifile);

  addon = yield promiseAddonByID(IMPLICIT_ID_ID);
  do_check_neq(addon, null);

  addon.uninstall();
});

// We should be able to temporarily install an unsigned web extension
// that does not have an ID in its manifest.
add_task(function* test_unsigned_no_id_temp_install() {
  if (!TEST_UNPACKED) {
    do_print("This test does not apply when using packed extensions");
    return;
  }
  const manifest = {
    name: "no ID",
    description: "extension without an ID",
    manifest_version: 2,
    version: "1.0"
  };

  const addonDir = writeWebManifestForExtension(manifest, gTmpD,
                                                "the-addon-sub-dir");
  const addon = yield AddonManager.installTemporaryAddon(addonDir);
  ok(addon.id, "ID should have been auto-generated");

  // Install the same directory again, as if re-installing or reloading.
  const secondAddon = yield AddonManager.installTemporaryAddon(addonDir);
  // The IDs should be the same.
  equal(secondAddon.id, addon.id);

  secondAddon.uninstall();
  addonDir.remove(true);
});

// We should be able to install two extensions from manifests without IDs
// at different locations and get two unique extensions.
add_task(function* test_multiple_no_id_extensions() {
  if (!TEST_UNPACKED) {
    do_print("This test does not apply when using packed extensions");
    return;
  }
  const manifest = {
    name: "no ID",
    description: "extension without an ID",
    manifest_version: 2,
    version: "1.0"
  };

  const firstAddonDir = writeWebManifestForExtension(manifest, gTmpD,
                                                     "addon-sub-dir-one");
  const secondAddonDir = writeWebManifestForExtension(manifest, gTmpD,
                                                      "addon-sub-dir-two");
  const [firstAddon, secondAddon] = yield Promise.all([
    AddonManager.installTemporaryAddon(firstAddonDir),
    AddonManager.installTemporaryAddon(secondAddonDir)
  ]);

  const allAddons = yield new Promise(resolve => {
    AddonManager.getAllAddons(addons => resolve(addons));
  });
  do_print(`Found these add-ons: ${allAddons.map(a => a.name).join(", ")}`);
  const filtered = allAddons.filter(addon => addon.name === manifest.name);
  // Make sure we have two add-ons by the same name.
  equal(filtered.length, 2);

  firstAddon.uninstall();
  firstAddonDir.remove(true);
  secondAddon.uninstall();
  secondAddonDir.remove(true);
});

// Test that we can get the ID from browser_specific_settings
add_task(function* test_bss_id() {
  const ID = "webext_bss_id@tests.mozilla.org";

  let manifest = {
    name: "bss test",
    description: "test that ID may be in browser_specific_settings",
    manifest_version: 2,
    version: "1.0",

    browser_specific_settings: {
      gecko: {
        id: ID
      }
    }
  };

  let addon = yield promiseAddonByID(ID);
  do_check_eq(addon, null);

  writeWebManifestForExtension(manifest, profileDir, ID);
  yield promiseRestartManager();

  addon = yield promiseAddonByID(ID);
  do_check_neq(addon, null);

  addon.uninstall();
});

// Test that if we have IDs in both browser_specific_settings and applications,
// that we prefer the ID in browser_specific_settings.
add_task(function* test_two_ids() {
  const GOOD_ID = "two_ids@tests.mozilla.org";
  const BAD_ID = "i_am_obsolete@tests.mozilla.org";

  let manifest = {
    name: "two id test",
    description: "test a web extension with ids in both applications and browser_specific_settings",
    manifest_version: 2,
    version: "1.0",

    applications: {
      gecko: {
        id: BAD_ID
      }
    },

    browser_specific_settings: {
      gecko: {
        id: GOOD_ID
      }
    }
  }

  writeWebManifestForExtension(manifest, profileDir, GOOD_ID);
  yield promiseRestartManager();

  let addon = yield promiseAddonByID(BAD_ID);
  do_check_eq(addon, null);
  addon = yield promiseAddonByID(GOOD_ID);
  do_check_neq(addon, null);

  addon.uninstall();
});

add_task(function* strict_min_star() {
  const addonId = "strict_min_max@tests.mozilla.org";
  const MANIFEST = {
    name: "strict min star test",
    description: "test strict min with star",
    manifest_version: 2,
    version: "1.0",
  };

  function flushAndRemove(file) {
    // flush JAR cache and remove the file
    Services.obs.notifyObservers(file, "flush-cache-entry", null);
    file.remove(true);
  }

  // * in min will generate a warning
  for (let version of ["0.*", "0.*.0"]) {
    let apps = {
      applications: {
        gecko: {
          id: addonId,
          strict_min_version: version,
        },
      },
    }
    let testManifest = Object.assign(apps, MANIFEST);

    let addonDir = writeWebManifestForExtension(testManifest, gTmpD,
                                                "strict_min_star");
    let { messages } = yield promiseConsoleOutput(function* () {
      yield AddonManager.installTemporaryAddon(addonDir);
    });
    ok(messages.some(msg => msg.message.includes("The use of '*' in strict_min_version is deprecated")),
       "Deprecation warning for strict_min_version with '*' was generated");

    let addon = yield promiseAddonByID(addonId);

    notEqual(addon, null, "Add-on is installed");
    equal(addon.id, addonId, "Add-on has the expected id");

    addon.uninstall();
    flushAndRemove(addonDir);
  }
});
