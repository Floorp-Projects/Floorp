/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { PermissionTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PermissionTestUtils.sys.mjs"
);

const DOMAIN_A = "example.com";
const SUB_DOMAIN_A = "sub." + DOMAIN_A;
const DOMAIN_B = "example.org";
const DOMAIN_C = "example.net";

const ORIGIN_A = "https://" + DOMAIN_A;
const ORIGIN_SUB_A = "https://" + SUB_DOMAIN_A;
const ORIGIN_B = "https://" + DOMAIN_B;
const ORIGIN_C = "https://" + DOMAIN_C;
const ORIGIN_NON_HTTP = "file:///foo/bar.html";

const OA_PBM = { privateBrowsingId: 1 };
const PRINCIPAL_C_PBM = Services.scriptSecurityManager.createContentPrincipal(
  Services.io.newURI(ORIGIN_C),
  OA_PBM
);

let btp;
let userActivationLifetimeSec = Services.prefs.getIntPref(
  "privacy.bounceTrackingProtection.bounceTrackingActivationLifetimeSec"
);

function cleanup() {
  btp.clearAll();
  Services.perms.removeAll();
  Services.prefs.setBoolPref(
    "privacy.bounceTrackingProtection.hasMigratedUserActivationData",
    false
  );
}

add_setup(function () {
  // Need a profile to data clearing calls.
  do_get_profile();

  btp = Cc["@mozilla.org/bounce-tracking-protection;1"].getService(
    Ci.nsIBounceTrackingProtection
  );

  // Clean initial state.
  cleanup();
});

add_task(async function test_user_activation_perm_migration() {
  // Assert initial test state.
  Assert.deepEqual(
    btp.testGetUserActivationHosts({}),
    [],
    "No user activation hosts initially."
  );
  Assert.equal(
    Services.perms.getAllByTypes(["storageAccessAPI"]).length,
    0,
    "No user activation permissions initially."
  );

  info("Add test user activation permissions.");

  let now = Date.now();

  // Non-expired permissions.
  PermissionTestUtils.addWithModificationTime(
    ORIGIN_A,
    "storageAccessAPI",
    Services.perms.ALLOW_ACTION,
    now
  );
  PermissionTestUtils.addWithModificationTime(
    ORIGIN_C,
    "storageAccessAPI",
    Services.perms.ALLOW_ACTION,
    now - 1000
  );

  // A non expired permission for a subdomain of DOMAIN_A that has an older modification time.
  PermissionTestUtils.addWithModificationTime(
    ORIGIN_SUB_A,
    "storageAccessAPI",
    Services.perms.ALLOW_ACTION,
    now - 500
  );

  // An expired permission.
  PermissionTestUtils.addWithModificationTime(
    ORIGIN_B,
    "storageAccessAPI",
    Services.perms.ALLOW_ACTION,
    now - userActivationLifetimeSec * 1.2 * 1000
  );

  // A non-HTTP permission.
  PermissionTestUtils.addWithModificationTime(
    ORIGIN_NON_HTTP,
    "storageAccessAPI",
    Services.perms.ALLOW_ACTION,
    now
  );

  // A permission for PBM. Ideally we'd test a more persistent permission type
  // here with custom oa, but permission seperation by userContextId isn't
  // enabled yet (Bug 1641584).
  PermissionTestUtils.addWithModificationTime(
    PRINCIPAL_C_PBM,
    "storageAccessAPI",
    Services.perms.ALLOW_ACTION,
    now
  );

  info("Trigger migration.");
  btp.testMaybeMigrateUserInteractionPermissions();

  Assert.deepEqual(
    btp
      .testGetUserActivationHosts({})
      .map(entry => entry.siteHost)
      .sort(),
    [DOMAIN_A, DOMAIN_C].sort(),
    "Should have imported the correct user activation flags."
  );
  Assert.deepEqual(
    btp.testGetUserActivationHosts(OA_PBM).map(entry => entry.siteHost),
    [DOMAIN_C],
    "Should have imported the correct user activation flags for PBM."
  );

  info("Reset the BTP user activation store");
  btp.clearAll();

  info("Trigger migration again.");
  btp.testMaybeMigrateUserInteractionPermissions();

  Assert.deepEqual(
    btp.testGetUserActivationHosts({}),
    [],
    "Should not have imported the user activation flags again."
  );
  Assert.deepEqual(
    btp.testGetUserActivationHosts(OA_PBM),
    [],
    "Should not have imported the user activation flags again for PBM."
  );

  cleanup();
});
