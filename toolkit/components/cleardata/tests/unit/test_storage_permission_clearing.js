/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/***
 * Tests clearing storage access and persistent storage permissions
 */

"use strict";

const baseDomain = "example.net";
const baseDomain2 = "host.org";
const uri = Services.io.newURI(`https://` + baseDomain);
const uri2 = Services.io.newURI(`https://` + baseDomain2);
const uri3 = Services.io.newURI(`https://www.` + baseDomain);
const principal = Services.scriptSecurityManager.createContentPrincipal(
  uri,
  {}
);
const principal2 = Services.scriptSecurityManager.createContentPrincipal(
  uri2,
  {}
);
const principal3 = Services.scriptSecurityManager.createContentPrincipal(
  uri3,
  {}
);

add_task(async function test_clearing_by_principal() {
  Services.perms.addFromPrincipal(
    principal,
    "storage-access",
    Services.perms.ALLOW_ACTION
  );
  Assert.equal(
    Services.perms.testExactPermissionFromPrincipal(
      principal,
      "storage-access"
    ),
    Services.perms.ALLOW_ACTION,
    "There is a storage-access permission set for principal 1"
  );

  Services.perms.addFromPrincipal(
    principal,
    "persistent-storage",
    Services.perms.ALLOW_ACTION
  );
  Assert.equal(
    Services.perms.testExactPermissionFromPrincipal(
      principal,
      "persistent-storage"
    ),
    Services.perms.ALLOW_ACTION,
    "There is a persistent-storage permission set for principal 1"
  );

  // Add a principal that shouldn't get cleared
  Services.perms.addFromPrincipal(
    principal2,
    "storage-access",
    Services.perms.ALLOW_ACTION
  );
  Assert.equal(
    Services.perms.testExactPermissionFromPrincipal(
      principal2,
      "storage-access"
    ),
    Services.perms.ALLOW_ACTION,
    "There is a storage-access permission set for principal 2"
  );

  // Add an unrelated permission which we don't expect to be cleared
  Services.perms.addFromPrincipal(
    principal,
    "desktop-notification",
    Services.perms.ALLOW_ACTION
  );
  Assert.equal(
    Services.perms.testExactPermissionFromPrincipal(
      principal,
      "desktop-notification"
    ),
    Services.perms.ALLOW_ACTION,
    "There is a desktop-notification permission set for principal 1"
  );

  await new Promise(aResolve => {
    Services.clearData.deleteDataFromPrincipal(
      principal,
      true,
      Ci.nsIClearDataService.CLEAR_STORAGE_PERMISSIONS,
      value => {
        Assert.equal(value, 0);
        aResolve();
      }
    );
  });

  Assert.equal(
    Services.perms.testExactPermissionFromPrincipal(
      principal,
      "storage-access"
    ),
    Services.perms.UNKNOWN_ACTION,
    "storage-access permission for principal 1 has been removed"
  );
  Assert.equal(
    Services.perms.testExactPermissionFromPrincipal(
      principal,
      "persistent-storage"
    ),
    Services.perms.UNKNOWN_ACTION,
    "persistent-storage permission for principal 1 should be removed"
  );
  Assert.equal(
    Services.perms.testExactPermissionFromPrincipal(
      principal2,
      "storage-access"
    ),
    Services.perms.ALLOW_ACTION,
    "storage-access permission for principal 2 should not be removed"
  );
  Assert.equal(
    Services.perms.testExactPermissionFromPrincipal(
      principal,
      "desktop-notification"
    ),
    Services.perms.ALLOW_ACTION,
    "desktop-notification permission for principal should not be removed"
  );
});

add_task(async function test_clearing_by_baseDomain() {
  Services.perms.addFromPrincipal(
    principal,
    "storage-access",
    Services.perms.ALLOW_ACTION
  );
  Services.perms.addFromPrincipal(
    principal2,
    "storage-access",
    Services.perms.ALLOW_ACTION
  );
  Services.perms.addFromPrincipal(
    principal3,
    "storage-access",
    Services.perms.ALLOW_ACTION
  );
  Assert.equal(
    Services.perms.testExactPermissionFromPrincipal(
      principal,
      "storage-access"
    ),
    Services.perms.ALLOW_ACTION,
    "There is a storage-access permission set for principal 1"
  );
  Assert.equal(
    Services.perms.testExactPermissionFromPrincipal(
      principal2,
      "storage-access"
    ),
    Services.perms.ALLOW_ACTION,
    "There is a storage-access permission set for principal 2"
  );

  await new Promise(aResolve => {
    Services.clearData.deleteDataFromBaseDomain(
      baseDomain,
      true,
      Ci.nsIClearDataService.CLEAR_STORAGE_PERMISSIONS,
      value => {
        Assert.equal(value, 0);
        aResolve();
      }
    );
  });

  Assert.equal(
    Services.perms.testExactPermissionFromPrincipal(
      principal,
      "storage-access"
    ),
    Services.perms.UNKNOWN_ACTION,
    "storage-access permission for principal 1 has been removed"
  );
  Assert.equal(
    Services.perms.testExactPermissionFromPrincipal(
      principal2,
      "storage-access"
    ),
    Services.perms.ALLOW_ACTION,
    "storage-access permission for principal 2 should not be removed"
  );
  Assert.equal(
    Services.perms.testExactPermissionFromPrincipal(
      principal3,
      "storage-access"
    ),
    Services.perms.UNKNOWN_ACTION,
    "storage-access permission for principal 3 should be removed"
  );
});

add_task(async function test_clearing_by_range() {
  let currTime = Date.now();
  let modificationTimeTwoHoursAgo = new Date(currTime - 2 * 60 * 60 * 1000);
  let modificationTimeThirtyMinsAgo = new Date(currTime - 30 * 60 * 1000);

  Services.perms.testAddFromPrincipalByTime(
    principal,
    "storage-access",
    Services.perms.ALLOW_ACTION,
    modificationTimeTwoHoursAgo
  );
  Assert.equal(
    Services.perms.testExactPermissionFromPrincipal(
      principal,
      "storage-access"
    ),
    Services.perms.ALLOW_ACTION,
    "There is a storage-access permission set for principal 1"
  );

  Services.perms.testAddFromPrincipalByTime(
    principal2,
    "persistent-storage",
    Services.perms.ALLOW_ACTION,
    modificationTimeThirtyMinsAgo
  );
  Assert.equal(
    Services.perms.testExactPermissionFromPrincipal(
      principal2,
      "persistent-storage"
    ),
    Services.perms.ALLOW_ACTION,
    "There is a persistent-storage permission set for principal 2"
  );

  let modificationTimeOneHourAgo = new Date(currTime - 60 * 60 * 1000);
  // We need to pass in microseconds to the clear data service
  // so we multiply the ranges by 1000
  await new Promise(aResolve => {
    Services.clearData.deleteDataInTimeRange(
      modificationTimeOneHourAgo * 1000,
      Date.now() * 1000,
      true,
      Ci.nsIClearDataService.CLEAR_STORAGE_PERMISSIONS,
      value => {
        Assert.equal(value, 0);
        aResolve();
      }
    );
  });

  Assert.equal(
    Services.perms.testExactPermissionFromPrincipal(
      principal,
      "storage-access"
    ),
    Services.perms.ALLOW_ACTION,
    "storage-access permission for principal 1 should not be removed"
  );
  Assert.equal(
    Services.perms.testExactPermissionFromPrincipal(
      principal2,
      "persistent-storage"
    ),
    Services.perms.UNKNOWN_ACTION,
    "persistent-storage permission for principal 2 should be removed"
  );
});
