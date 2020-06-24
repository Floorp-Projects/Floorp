/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that AddonUpdateChecker works correctly

const { AddonUpdateChecker } = ChromeUtils.import(
  "resource://gre/modules/addons/AddonUpdateChecker.jsm"
);

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");

var testserver = AddonTestUtils.createHttpServer({ hosts: ["example.com"] });

testserver.registerDirectory("/data/", do_get_file("data"));

function checkUpdates(aId) {
  return new Promise((resolve, reject) => {
    AddonUpdateChecker.checkForUpdates(
      aId,
      `http://example.com/data/test_updatecheck.json`,
      {
        onUpdateCheckComplete: resolve,

        onUpdateCheckError(status) {
          let error = new Error("Update check failed with status " + status);
          error.status = status;
          reject(error);
        },
      }
    );
  });
}

// Test that a basic update check returns the expected available updates
add_task(async function test_basic_update() {
  let updates = await checkUpdates("updatecheck1@tests.mozilla.org");

  equal(updates.length, 5);
  let update = await AddonUpdateChecker.getNewestCompatibleUpdate(updates);
  notEqual(update, null);
  equal(update.version, "3.0");
  update = AddonUpdateChecker.getCompatibilityUpdate(updates, "2");
  notEqual(update, null);
  equal(update.version, "2.0");
  equal(update.targetApplications[0].minVersion, "1");
  equal(update.targetApplications[0].maxVersion, "2");
});

/*
 * Tests that the security checks are applied correctly
 *
 * Test     updateHash  updateLink   expected
 *--------------------------------------------
 * 4        absent      http         no update
 * 5        sha1        http         update
 * 6        absent      https        update
 * 7        sha1        https        update
 * 8        md2         http         no update
 * 9        md2         https        update
 */

add_task(async function test_4() {
  let updates = await checkUpdates("test_bug378216_8@tests.mozilla.org");
  equal(updates.length, 1);
  ok(!("updateURL" in updates[0]));
});

add_task(async function test_5() {
  let updates = await checkUpdates("test_bug378216_9@tests.mozilla.org");
  equal(updates.length, 1);
  equal(updates[0].version, "2.0");
  ok("updateURL" in updates[0]);
});

add_task(async function test_6() {
  let updates = await checkUpdates("test_bug378216_10@tests.mozilla.org");
  equal(updates.length, 1);
  equal(updates[0].version, "2.0");
  ok("updateURL" in updates[0]);
});

add_task(async function test_7() {
  let updates = await checkUpdates("test_bug378216_11@tests.mozilla.org");
  equal(updates.length, 1);
  equal(updates[0].version, "2.0");
  ok("updateURL" in updates[0]);
});

add_task(async function test_8() {
  let updates = await checkUpdates("test_bug378216_12@tests.mozilla.org");
  equal(updates.length, 1);
  ok(!("updateURL" in updates[0]));
});

add_task(async function test_9() {
  let updates = await checkUpdates("test_bug378216_13@tests.mozilla.org");
  equal(updates.length, 1);
  equal(updates[0].version, "2.0");
  ok("updateURL" in updates[0]);
});

add_task(async function test_no_update_data() {
  let updates = await checkUpdates("test_bug378216_14@tests.mozilla.org");
  equal(updates.length, 0);
});

add_task(async function test_invalid_json() {
  await checkUpdates("test_bug378216_15@tests.mozilla.org")
    .then(() => {
      ok(false, "Expected the update check to fail");
    })
    .catch(e => {
      equal(
        e.status,
        AddonManager.ERROR_PARSE_ERROR,
        "expected AddonManager.ERROR_PARSE_ERROR"
      );
    });
});

add_task(async function test_ignore_compat() {
  let updates = await checkUpdates("ignore-compat@tests.mozilla.org");
  equal(updates.length, 3);
  let update = await AddonUpdateChecker.getNewestCompatibleUpdate(
    updates,
    null,
    null,
    true
  );
  notEqual(update, null);
  equal(update.version, 2);
});

add_task(async function test_strict_compat() {
  let updates = await checkUpdates("compat-strict-optin@tests.mozilla.org");
  equal(updates.length, 1);
  let update = await AddonUpdateChecker.getNewestCompatibleUpdate(
    updates,
    null,
    null,
    true,
    false
  );
  equal(update, null);
});
