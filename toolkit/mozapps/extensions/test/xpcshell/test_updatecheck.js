/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that AddonUpdateChecker works correctly

Components.utils.import("resource://gre/modules/addons/AddonUpdateChecker.jsm");

Components.utils.import("resource://testing-common/httpd.js");

var testserver = createHttpServer(4444);
testserver.registerDirectory("/data/", do_get_file("data"));

function checkUpdates(aId, aUpdateKey, aUpdateFile) {
  return new Promise((resolve, reject) => {
    AddonUpdateChecker.checkForUpdates(aId, aUpdateKey, `http://localhost:4444/data/${aUpdateFile}`, {
      onUpdateCheckComplete: resolve,

      onUpdateCheckError(status) {
        let error = new Error("Update check failed with status " + status);
        error.status = status;
        reject(error);
      }
    });
  });
}

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");

  run_next_test();
}

// Test that a basic update check returns the expected available updates
add_task(async function() {
  for (let file of ["test_updatecheck.rdf", "test_updatecheck.json"]) {
    let updates = await checkUpdates("updatecheck1@tests.mozilla.org", null, file);

    equal(updates.length, 5);
    let update = AddonUpdateChecker.getNewestCompatibleUpdate(updates);
    notEqual(update, null);
    equal(update.version, "3.0");
    update = AddonUpdateChecker.getCompatibilityUpdate(updates, "2");
    notEqual(update, null);
    equal(update.version, "2.0");
    equal(update.targetApplications[0].minVersion, "1");
    equal(update.targetApplications[0].maxVersion, "2");
  }
});

/*
 * Tests that the security checks are applied correctly
 *
 * Test     signature   updateHash  updateLink   expected
 *--------------------------------------------------------
 * 2        absent      absent      http         fail
 * 3        broken      absent      http         fail
 * 4        correct     absent      http         no update
 * 5        correct     sha1        http         update
 * 6        corrent     absent      https        update
 * 7        corrent     sha1        https        update
 * 8        corrent     md2         http         no update
 * 9        corrent     md2         https        update
 */

var updateKey = "MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQDK426erD/H3XtsjvaB5+PJqbhj" +
                "Zc9EDI5OCJS8R3FIObJ9ZHJK1TXeaE7JWqt9WUmBWTEFvwS+FI9vWu8058N9CHhD" +
                "NyeP6i4LuUYjTURnn7Yw/IgzyIJ2oKsYa32RuxAyteqAWqPT/J63wBixIeCxmysf" +
                "awB/zH4KaPiY3vnrzQIDAQAB";

add_task(async function() {
  for (let file of ["test_updatecheck.rdf", "test_updatecheck.json"]) {
    try {
      await checkUpdates("test_bug378216_5@tests.mozilla.org",
                         updateKey, file);
      throw "Expected the update check to fail";
    } catch (e) {}
  }
});

add_task(async function() {
  for (let file of ["test_updatecheck.rdf", "test_updatecheck.json"]) {
    try {
      await checkUpdates("test_bug378216_7@tests.mozilla.org",
                         updateKey, file);

      throw "Expected the update check to fail";
    } catch (e) {}
  }
});

add_task(async function() {
  // Make sure that the JSON manifest is rejected when an update key is
  // required, but perform the remaining tests which aren't expected to fail
  // because of the update key, without requiring one for the JSON variant.

  try {
    await checkUpdates("test_bug378216_8@tests.mozilla.org",
                       updateKey, "test_updatecheck.json");

    throw "Expected the update check to fail";
  } catch (e) {}

  for (let [file, key] of [["test_updatecheck.rdf", updateKey],
                           ["test_updatecheck.json", null]]) {
    let updates = await checkUpdates("test_bug378216_8@tests.mozilla.org",
                                     key, file);
    equal(updates.length, 1);
    ok(!("updateURL" in updates[0]));
  }
});

add_task(async function() {
  for (let [file, key] of [["test_updatecheck.rdf", updateKey],
                           ["test_updatecheck.json", null]]) {
    let updates = await checkUpdates("test_bug378216_9@tests.mozilla.org",
                                     key, file);
    equal(updates.length, 1);
    equal(updates[0].version, "2.0");
    ok("updateURL" in updates[0]);
  }
});

add_task(async function() {
  for (let [file, key] of [["test_updatecheck.rdf", updateKey],
                           ["test_updatecheck.json", null]]) {
    let updates = await checkUpdates("test_bug378216_10@tests.mozilla.org",
                                     key, file);
    equal(updates.length, 1);
    equal(updates[0].version, "2.0");
    ok("updateURL" in updates[0]);
  }
});

add_task(async function() {
  for (let [file, key] of [["test_updatecheck.rdf", updateKey],
                           ["test_updatecheck.json", null]]) {
    let updates = await checkUpdates("test_bug378216_11@tests.mozilla.org",
                                     key, file);
    equal(updates.length, 1);
    equal(updates[0].version, "2.0");
    ok("updateURL" in updates[0]);
  }
});

add_task(async function() {
  for (let [file, key] of [["test_updatecheck.rdf", updateKey],
                           ["test_updatecheck.json", null]]) {
    let updates = await checkUpdates("test_bug378216_12@tests.mozilla.org",
                                     key, file);
    equal(updates.length, 1);
    Assert.equal(false, "updateURL" in updates[0]);
  }
});

add_task(async function() {
  for (let [file, key] of [["test_updatecheck.rdf", updateKey],
                           ["test_updatecheck.json", null]]) {
    let updates = await checkUpdates("test_bug378216_13@tests.mozilla.org",
                                     key, file);
    equal(updates.length, 1);
    equal(updates[0].version, "2.0");
    ok("updateURL" in updates[0]);
  }
});

add_task(async function() {
  for (let file of ["test_updatecheck.rdf", "test_updatecheck.json"]) {
    let updates = await checkUpdates("test_bug378216_14@tests.mozilla.org",
                                     null, file);
    equal(updates.length, 0);
  }
});

add_task(async function() {
  for (let file of ["test_updatecheck.rdf", "test_updatecheck.json"]) {
    try {
      await checkUpdates("test_bug378216_15@tests.mozilla.org",
                         null, file);

      throw "Update check should have failed";
    } catch (e) {
      equal(e.status, AddonManager.ERROR_PARSE_ERROR);
    }
  }
});

add_task(async function() {
  for (let file of ["test_updatecheck.rdf", "test_updatecheck.json"]) {
    let updates = await checkUpdates("ignore-compat@tests.mozilla.org",
                                     null, file);
    equal(updates.length, 3);
    let update = AddonUpdateChecker.getNewestCompatibleUpdate(
      updates, null, null, true);
    notEqual(update, null);
    equal(update.version, 2);
  }
});

add_task(async function() {
  for (let file of ["test_updatecheck.rdf", "test_updatecheck.json"]) {
    let updates = await checkUpdates("compat-override@tests.mozilla.org",
                                     null, file);
    equal(updates.length, 3);
    let overrides = [{
      type: "incompatible",
      minVersion: 1,
      maxVersion: 2,
      appID: "xpcshell@tests.mozilla.org",
      appMinVersion: 0.1,
      appMaxVersion: 0.2
    }, {
      type: "incompatible",
      minVersion: 2,
      maxVersion: 2,
      appID: "xpcshell@tests.mozilla.org",
      appMinVersion: 1,
      appMaxVersion: 2
    }];
    let update = AddonUpdateChecker.getNewestCompatibleUpdate(
      updates, null, null, true, false, overrides);
    notEqual(update, null);
    equal(update.version, 1);
  }
});

add_task(async function() {
  for (let file of ["test_updatecheck.rdf", "test_updatecheck.json"]) {
    let updates = await checkUpdates("compat-strict-optin@tests.mozilla.org",
                                     null, file);
    equal(updates.length, 1);
    let update = AddonUpdateChecker.getNewestCompatibleUpdate(
      updates, null, null, true, false);
    equal(update, null);
  }
});
